#include "moon_hitl.h"

#include "moon/cgui.h"
#include "moon/dos_runtime.h"
#include "moon/hitl.h"
#include "moon/hitl_cockpit.h"
#include "moon/hitl_cockpit_ui.h"
#include "moon/runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/movedata.h>

#define MOON_HITL_PATH_CAPACITY 260u
#define MOON_HITL_VGA_MEMORY UINT32_C(0x000a0000)
#define MOON_HITL_SMOKE_STEPS 6u
#define MOON_HITL_SMOKE_TIMEOUT_SECONDS 4u

typedef enum MoonHitlAction {
    MOON_HITL_ACTION_UP = 0,
    MOON_HITL_ACTION_DOWN,
    MOON_HITL_ACTION_ENTER,
    MOON_HITL_ACTION_SPACE,
    MOON_HITL_ACTION_ESCAPE,
    MOON_HITL_ACTION_F1,
    MOON_HITL_ACTION_PASS,
    MOON_HITL_ACTION_FAIL,
    MOON_HITL_ACTION_BLOCKED,
    MOON_HITL_ACTION_CLEAR,
    MOON_HITL_ACTION_CAPTURE,
    MOON_HITL_ACTION_BACK,
    MOON_HITL_ACTION_YES,
    MOON_HITL_ACTION_NO,
    MOON_HITL_ACTION_COUNT
} MoonHitlAction;

typedef struct MoonHitlPaths {
    char auto_path[MOON_HITL_PATH_CAPACITY];
    char journal_path[MOON_HITL_PATH_CAPACITY];
    char new_path[MOON_HITL_PATH_CAPACITY];
    char out_path[MOON_HITL_PATH_CAPACITY];
    char old_path[MOON_HITL_PATH_CAPACITY];
    char smoke_path[MOON_HITL_PATH_CAPACITY];
} MoonHitlPaths;

typedef struct MoonHitlFiles {
    FILE *journal_stream;
} MoonHitlFiles;

typedef struct MoonHitlSmokeState {
    uint32_t frames;
    uint32_t events;
    uint32_t manual_count;
    uint32_t frame_hash;
    uint32_t readback_hash;
    uint32_t synthetic_step;
    uint8_t manual_attempted;
    uint8_t committed;
} MoonHitlSmokeState;

typedef struct MoonHitlSession {
    HitlPlan plan;
    HitlAutoEvidence auto_evidence;
    HitlCockpit cockpit;
    HitlJournal journal;
    HitlCockpitUi ui;
    MoonContext moon;
    MoonDosRuntime dos;
    CguiContext cgui;
    MoonHitlPaths paths;
    MoonHitlFiles files;
    uint32_t logical_tick;
    uint32_t edge_serial;
    uint8_t dos_initialized;
    uint8_t cgui_initialized;
} MoonHitlSession;

static MoonHitlSession moon_hitl_session;
static uint8_t moon_hitl_framebuffer[MOON_DOS_FRAMEBUFFER_BYTES];
static uint8_t moon_hitl_readback[MOON_DOS_FRAMEBUFFER_BYTES];

static MoonHitlResult moon_hitl_run_internal(
    const char *plan_path,
    HitlInputAuthority authority,
    int synthetic_smoke);
static MoonHitlResult moon_hitl_commit_summary(
    MoonHitlSession *session,
    HitlInputAuthority authority);
static uint32_t moon_hitl_hash_bytes(const uint8_t *bytes,
                                     size_t byte_count);

static int moon_hitl_bind(MoonInput *input,
                          uint16_t action,
                          MoonKey key)
{
    return moon_input_bind(input, action, 0u, key) == MOON_OK;
}

static int moon_hitl_bind_input(MoonInput *input)
{
    return
        moon_hitl_bind(input, MOON_HITL_ACTION_UP,
                       MOON_KEY_E0(0x48u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_DOWN,
                       MOON_KEY_E0(0x50u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_ENTER,
                       MOON_KEY_NORMAL(0x1cu)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_SPACE,
                       MOON_KEY_NORMAL(0x39u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_ESCAPE,
                       MOON_KEY_NORMAL(0x01u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_F1,
                       MOON_KEY_NORMAL(0x3bu)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_PASS,
                       MOON_KEY_NORMAL(0x19u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_FAIL,
                       MOON_KEY_NORMAL(0x21u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_BLOCKED,
                       MOON_KEY_NORMAL(0x30u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_CLEAR,
                       MOON_KEY_NORMAL(0x2eu)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_CAPTURE,
                       MOON_KEY_NORMAL(0x1fu)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_BACK,
                       MOON_KEY_NORMAL(0x0eu)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_YES,
                       MOON_KEY_NORMAL(0x15u)) &&
        moon_hitl_bind(input, MOON_HITL_ACTION_NO,
                       MOON_KEY_NORMAL(0x31u));
}

static int moon_hitl_make_sibling_path(char *destination,
                                       size_t destination_size,
                                       const char *plan_path,
                                       const char *leaf)
{
    size_t plan_length;
    size_t leaf_length;
    size_t prefix_length = 0u;
    size_t index;

    if (destination == NULL || destination_size == 0u || plan_path == NULL ||
        leaf == NULL || plan_path[0] == '\0' || leaf[0] == '\0') {
        return 0;
    }
    plan_length = strlen(plan_path);
    leaf_length = strlen(leaf);
    for (index = 0u; index < plan_length; ++index) {
        if (plan_path[index] == '/' || plan_path[index] == '\\' ||
            plan_path[index] == ':') {
            prefix_length = index + 1u;
        }
    }
    if (prefix_length + leaf_length + 1u > destination_size) {
        return 0;
    }
    if (prefix_length != 0u) {
        memcpy(destination, plan_path, prefix_length);
    }
    memcpy(destination + prefix_length, leaf, leaf_length + 1u);
    return 1;
}

static int moon_hitl_paths_init(MoonHitlPaths *paths, const char *plan_path)
{
    if (paths == NULL) {
        return 0;
    }
    memset(paths, 0, sizeof(*paths));
    return
        moon_hitl_make_sibling_path(paths->auto_path,
                                    sizeof(paths->auto_path),
                                    plan_path, "AUTO.OUT") &&
        moon_hitl_make_sibling_path(paths->journal_path,
                                    sizeof(paths->journal_path),
                                    plan_path, "HITL.JRN") &&
        moon_hitl_make_sibling_path(paths->new_path,
                                    sizeof(paths->new_path),
                                    plan_path, "HITL.NEW") &&
        moon_hitl_make_sibling_path(paths->out_path,
                                    sizeof(paths->out_path),
                                    plan_path, "HITL.OUT") &&
        moon_hitl_make_sibling_path(paths->old_path,
                                    sizeof(paths->old_path),
                                    plan_path, "HITL.OLD") &&
        moon_hitl_make_sibling_path(paths->smoke_path,
                                    sizeof(paths->smoke_path),
                                    plan_path, "HITLSMOK.OUT");
}

static MoonHitlResult moon_hitl_load_plan(const char *plan_path,
                                          HitlPlan *plan)
{
    FILE *stream;
    HitlResult result;
    unsigned long error_line = 0u;
    int close_result;

    stream = fopen(plan_path, "rb");
    if (stream == NULL) {
        return MOON_HITL_ERR_PLAN;
    }
    result = hitl_plan_read(stream, plan, &error_line);
    close_result = fclose(stream);
    if (result != HITL_OK || close_result != 0) {
        return MOON_HITL_ERR_PLAN;
    }
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_load_auto(const char *auto_path,
                                          const HitlPlan *plan,
                                          HitlAutoEvidence *evidence)
{
    FILE *stream;
    HitlResult result;
    unsigned long error_line = 0u;
    int close_result;

    stream = fopen(auto_path, "rb");
    if (stream == NULL) {
        return MOON_HITL_ERR_AUTO;
    }
    result = hitl_auto_read(stream, plan, evidence, &error_line);
    close_result = fclose(stream);
    if (result == HITL_ERR_IO || result == HITL_ERR_ARGUMENT ||
        close_result != 0) {
        return MOON_HITL_ERR_AUTO;
    }
    return MOON_HITL_OK;
}

static void moon_hitl_copy_action(CguiActionState *destination,
                                  const MoonInput *input,
                                  uint16_t action)
{
    const MoonButtonState *source = moon_input_action(input, action);

    destination->pressed = source != NULL ? source->pressed : 0u;
    destination->repeat = source != NULL ? source->repeat : 0u;
}

static int moon_hitl_action_pressed(const MoonInput *input, uint16_t action)
{
    const MoonButtonState *button = moon_input_action(input, action);

    return button != NULL && button->pressed != 0u;
}

static CguiId moon_hitl_resolve_accelerator(const MoonHitlSession *session)
{
    HitlCockpitUiMode mode = hitl_cockpit_ui_mode(&session->ui);
    const MoonInput *input = &session->moon.input;

    if (mode == HITL_COCKPIT_UI_CONFIRM_MANUAL) {
        if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_NO)) {
            return HITL_COCKPIT_UI_MAN_NO;
        }
        if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_YES)) {
            return HITL_COCKPIT_UI_MAN_YES;
        }
        return CGUI_ID_NONE;
    }
    if (mode == HITL_COCKPIT_UI_CONFIRM_EXIT) {
        if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_NO)) {
            return HITL_COCKPIT_UI_EXIT_NO;
        }
        if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_YES)) {
            return HITL_COCKPIT_UI_EXIT_YES;
        }
        return CGUI_ID_NONE;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_PASS)) {
        return HITL_COCKPIT_UI_ACT_PASS;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_FAIL)) {
        return HITL_COCKPIT_UI_ACT_FAIL;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_BLOCKED)) {
        return HITL_COCKPIT_UI_ACT_BLOCKED;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_CLEAR)) {
        return HITL_COCKPIT_UI_ACT_CLEAR;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_CAPTURE)) {
        return HITL_COCKPIT_UI_ACT_CAPTURE;
    }
    if (moon_hitl_action_pressed(input, MOON_HITL_ACTION_BACK)) {
        return HITL_COCKPIT_UI_ACT_BACK;
    }
    return CGUI_ID_NONE;
}

static int moon_hitl_winning_edge_is_fresh(const CguiInputFrame *gui)
{
    if (gui->actions[CGUI_ACTION_ESCAPE].pressed != 0u) {
        return 1;
    }
    if (gui->actions[CGUI_ACTION_F1].pressed != 0u) {
        return 1;
    }
    if (gui->accelerator_id != CGUI_ID_NONE) {
        return 1;
    }
    if (gui->actions[CGUI_ACTION_UP].pressed != 0u ||
        gui->actions[CGUI_ACTION_UP].repeat != 0u ||
        gui->actions[CGUI_ACTION_DOWN].pressed != 0u ||
        gui->actions[CGUI_ACTION_DOWN].repeat != 0u) {
        return gui->actions[CGUI_ACTION_UP].pressed != 0u ||
               gui->actions[CGUI_ACTION_DOWN].pressed != 0u;
    }
    return gui->actions[CGUI_ACTION_ENTER].pressed != 0u ||
           gui->actions[CGUI_ACTION_SPACE].pressed != 0u;
}

static int moon_hitl_next_edge_serial(MoonHitlSession *session)
{
    if (session->edge_serial == UINT32_MAX) {
        return 0;
    }
    ++session->edge_serial;
    return 1;
}

static MoonHitlResult moon_hitl_make_live_input(MoonHitlSession *session,
                                                HitlCockpitUiInput *output)
{
    int fresh;

    memset(output, 0, sizeof(*output));
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_UP],
                          &session->moon.input, MOON_HITL_ACTION_UP);
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_DOWN],
                          &session->moon.input, MOON_HITL_ACTION_DOWN);
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_ENTER],
                          &session->moon.input, MOON_HITL_ACTION_ENTER);
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_SPACE],
                          &session->moon.input, MOON_HITL_ACTION_SPACE);
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_ESCAPE],
                          &session->moon.input, MOON_HITL_ACTION_ESCAPE);
    moon_hitl_copy_action(&output->gui.actions[CGUI_ACTION_F1],
                          &session->moon.input, MOON_HITL_ACTION_F1);
    output->gui.accelerator_id = moon_hitl_resolve_accelerator(session);

    fresh = moon_hitl_winning_edge_is_fresh(&output->gui);
    if (fresh != 0 && !moon_hitl_next_edge_serial(session)) {
        return MOON_HITL_ERR_INPUT;
    }
    output->proof.authority = HITL_INPUT_LIVE_DOS;
    output->proof.fresh_pressed = (uint8_t)(fresh != 0);
    output->proof.edge_serial = session->edge_serial;
    output->proof.tick = session->logical_tick;
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_make_synthetic_input(
    MoonHitlSession *session,
    MoonHitlSmokeState *smoke,
    HitlCockpitUiInput *output)
{
    memset(output, 0, sizeof(*output));
    switch (smoke->synthetic_step) {
        case 0u:
            output->gui.actions[CGUI_ACTION_DOWN].pressed = 1u;
            break;
        case 1u:
            output->gui.actions[CGUI_ACTION_UP].pressed = 1u;
            break;
        case 2u:
            output->gui.actions[CGUI_ACTION_ENTER].pressed = 1u;
            break;
        case 3u:
            output->gui.actions[CGUI_ACTION_ESCAPE].pressed = 1u;
            break;
        case 4u:
            output->gui.actions[CGUI_ACTION_ESCAPE].pressed = 1u;
            break;
        case 5u:
            output->gui.accelerator_id = HITL_COCKPIT_UI_EXIT_YES;
            break;
        default:
            return MOON_HITL_ERR_CGUI;
    }
    if (!moon_hitl_next_edge_serial(session)) {
        return MOON_HITL_ERR_INPUT;
    }
    output->proof.authority = HITL_INPUT_SYNTHETIC;
    output->proof.fresh_pressed = 1u;
    output->proof.edge_serial = session->edge_serial;
    output->proof.tick = session->logical_tick;
    ++smoke->synthetic_step;
    return MOON_HITL_OK;
}

static const char *moon_hitl_selected_case_id(const MoonHitlSession *session)
{
    size_t index = hitl_cockpit_ui_selected_index(&session->ui);

    if (index >= session->plan.case_count) {
        return "-";
    }
    return session->plan.cases[index].id;
}

static MoonHitlResult moon_hitl_record_input_edge(
    MoonHitlSession *session,
    const HitlCockpitUiInput *input)
{
    HitlCockpitResult result;

    if (input->proof.fresh_pressed == 0u) {
        return MOON_HITL_OK;
    }
    result = hitl_journal_append(&session->journal,
                                 HITL_JOURNAL_STEP,
                                 session->logical_tick,
                                 moon_hitl_selected_case_id(session),
                                 input->proof.authority == HITL_INPUT_LIVE_DOS
                                     ? "LIVE_DOS input edge"
                                     : "SYNTHETIC input edge",
                                 input->proof.authority);
    return result == HITL_COCKPIT_OK ? MOON_HITL_OK
                                     : MOON_HITL_ERR_JOURNAL;
}

static MoonHitlResult moon_hitl_attach_journal(MoonHitlSession *session)
{
    unsigned long error_line = 0u;
    HitlCockpitResult result;

    session->files.journal_stream = fopen(session->paths.journal_path, "a+b");
    if (session->files.journal_stream == NULL) {
        return MOON_HITL_ERR_JOURNAL;
    }
    result = hitl_journal_attach(&session->journal,
                                 session->files.journal_stream,
                                 &session->plan,
                                 &error_line);
    if (result != HITL_COCKPIT_OK) {
        (void)fclose(session->files.journal_stream);
        session->files.journal_stream = NULL;
        return MOON_HITL_ERR_JOURNAL;
    }
    session->logical_tick = session->journal.last_tick;
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_close_journal(MoonHitlSession *session)
{
    HitlCockpitResult flush_result = HITL_COCKPIT_OK;
    int close_result = 0;

    if (session->files.journal_stream != NULL) {
        flush_result = hitl_journal_flush(&session->journal);
        hitl_journal_detach(&session->journal);
        close_result = fclose(session->files.journal_stream);
        session->files.journal_stream = NULL;
    }
    return flush_result == HITL_COCKPIT_OK && close_result == 0
               ? MOON_HITL_OK : MOON_HITL_ERR_JOURNAL;
}

static MoonHitlResult moon_hitl_initialize_dos_ui(MoonHitlSession *session)
{
    MoonRuntimeConfig config;
    CguiSurface surface;
    CguiPalette palette;

    if (moon_runtime_config_defaults(&config,
                                     moon_dos_clock_frequency()) != MOON_OK) {
        return MOON_HITL_ERR_RUNTIME;
    }
    config.present_mode = MOON_PRESENT_60;
    config.action_count = (uint16_t)MOON_HITL_ACTION_COUNT;
    if (moon_runtime_init(&session->moon, &config,
                          moon_dos_clock_now()) != MOON_OK) {
        return MOON_HITL_ERR_RUNTIME;
    }
    if (!moon_hitl_bind_input(&session->moon.input)) {
        return MOON_HITL_ERR_INPUT;
    }
    if (moon_dos_runtime_init(&session->dos,
                              &session->moon.input) != MOON_DOS_OK) {
        return MOON_HITL_ERR_RUNTIME;
    }
    session->dos_initialized = 1u;

    memset(moon_hitl_framebuffer, 0, sizeof(moon_hitl_framebuffer));
    surface.pixels = moon_hitl_framebuffer;
    surface.width = MOON_DOS_FRAMEBUFFER_WIDTH;
    surface.height = MOON_DOS_FRAMEBUFFER_HEIGHT;
    surface.stride = (size_t)MOON_DOS_FRAMEBUFFER_WIDTH;
    surface.byte_count = sizeof(moon_hitl_framebuffer);
    cgui_palette_classic(&palette);
    if (cgui_context_init(&session->cgui, &surface,
                          cgui_font_builtin_3x5(), &palette) != CGUI_OK) {
        return MOON_HITL_ERR_CGUI;
    }
    session->cgui_initialized = 1u;
    if (hitl_cockpit_ui_init(&session->ui,
                             &session->cgui,
                             &session->cockpit,
                             &session->journal) != CGUI_OK) {
        return MOON_HITL_ERR_CGUI;
    }
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_probe_synthetic_authority(
    MoonHitlSession *session,
    MoonHitlSmokeState *smoke)
{
    HitlInputProof proof;
    HitlCockpitResult result;
    size_t case_index;

    for (case_index = 0u; case_index < session->plan.case_count;
         ++case_index) {
        if (session->plan.cases[case_index].manual_required != 0u) {
            break;
        }
    }
    if (case_index == session->plan.case_count ||
        !moon_hitl_next_edge_serial(session)) {
        return MOON_HITL_ERR_CGUI;
    }
    memset(&proof, 0, sizeof(proof));
    proof.authority = HITL_INPUT_SYNTHETIC;
    proof.fresh_pressed = 1u;
    proof.edge_serial = session->edge_serial;
    proof.tick = session->logical_tick;
    result = hitl_cockpit_manual_begin(&session->cockpit,
                                       case_index,
                                       HITL_STATUS_PASS,
                                       &proof);
    if (result != HITL_COCKPIT_ERR_AUTHORITY) {
        return MOON_HITL_ERR_CGUI;
    }
    if (hitl_journal_append(&session->journal,
                            HITL_JOURNAL_WARNING,
                            session->logical_tick,
                            session->plan.cases[case_index].id,
                            "SYNTHETIC manual authority rejected",
                            HITL_INPUT_SYNTHETIC) != HITL_COCKPIT_OK) {
        return MOON_HITL_ERR_JOURNAL;
    }
    smoke->manual_attempted = 1u;
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_present(MoonHitlSession *session,
                                        MoonHitlSmokeState *smoke)
{
    if (hitl_cockpit_ui_draw(&session->ui) != CGUI_OK) {
        return MOON_HITL_ERR_CGUI;
    }
    if (smoke != NULL &&
        cgui_surface_hash(&session->cgui,
                          &smoke->frame_hash) != CGUI_OK) {
        return MOON_HITL_ERR_CGUI;
    }
    if (moon_dos_runtime_present(&session->dos,
                                 moon_hitl_framebuffer,
                                 sizeof(moon_hitl_framebuffer)) != MOON_DOS_OK) {
        return MOON_HITL_ERR_PRESENT;
    }
    if (smoke != NULL) {
        if (smoke->frames == UINT32_MAX) {
            return MOON_HITL_ERR_PRESENT;
        }
        ++smoke->frames;
        dosmemget(MOON_HITL_VGA_MEMORY,
                  sizeof(moon_hitl_readback),
                  moon_hitl_readback);
        smoke->readback_hash = moon_hitl_hash_bytes(
            moon_hitl_readback, sizeof(moon_hitl_readback));
    }
    return MOON_HITL_OK;
}

static int moon_hitl_synthetic_mode_valid(const MoonHitlSession *session,
                                          uint32_t completed_steps,
                                          int commit_requested)
{
    HitlCockpitUiMode mode = hitl_cockpit_ui_mode(&session->ui);

    switch (completed_steps) {
        case 1u:
        case 2u:
            return mode == HITL_COCKPIT_UI_LIST;
        case 3u:
            return mode == HITL_COCKPIT_UI_DETAIL;
        case 4u:
            return mode == HITL_COCKPIT_UI_LIST;
        case 5u:
            return mode == HITL_COCKPIT_UI_CONFIRM_EXIT;
        case 6u:
            return commit_requested != 0;
        default:
            return 0;
    }
}

static MoonHitlResult moon_hitl_process_ui_tick(
    MoonHitlSession *session,
    HitlInputAuthority authority,
    MoonHitlSmokeState *smoke,
    int *finished)
{
    HitlCockpitUiInput input;
    HitlCockpitUiEvent event;
    MoonHitlResult result;
    uint32_t completed_steps = 0u;
    int commit_requested;

    if (session->logical_tick == UINT32_MAX) {
        return MOON_HITL_ERR_RUNTIME;
    }
    ++session->logical_tick;
    moon_input_tick(&session->moon.input);
    if (authority == HITL_INPUT_LIVE_DOS) {
        result = moon_hitl_make_live_input(session, &input);
    } else {
        if (smoke == NULL ||
            smoke->synthetic_step >= MOON_HITL_SMOKE_STEPS) {
            return MOON_HITL_ERR_CGUI;
        }
        result = moon_hitl_make_synthetic_input(session, smoke, &input);
        completed_steps = smoke->synthetic_step;
    }
    if (result != MOON_HITL_OK) {
        return result;
    }
    result = moon_hitl_record_input_edge(session, &input);
    if (result != MOON_HITL_OK) {
        return result;
    }
    if (hitl_cockpit_ui_update(&session->ui, &input, &event) != CGUI_OK) {
        return MOON_HITL_ERR_CGUI;
    }
    commit_requested = event.type == HITL_COCKPIT_UI_EVENT_COMMIT_REQUEST;
    if (authority == HITL_INPUT_SYNTHETIC &&
        !moon_hitl_synthetic_mode_valid(session, completed_steps,
                                        commit_requested)) {
        return MOON_HITL_ERR_CGUI;
    }
    if (commit_requested != 0) {
        result = moon_hitl_commit_summary(session, authority);
        if (result != MOON_HITL_OK) {
            (void)hitl_cockpit_ui_commit_finished(
                &session->ui, HITL_COCKPIT_ERR_IO);
            return result;
        }
        if (hitl_cockpit_ui_commit_finished(
                &session->ui, HITL_COCKPIT_OK) != CGUI_OK) {
            return MOON_HITL_ERR_CGUI;
        }
        if (smoke != NULL) {
            smoke->committed = 1u;
        }
    }
    if (hitl_cockpit_ui_mode(&session->ui) == HITL_COCKPIT_UI_DONE) {
        *finished = 1;
    }
    return MOON_HITL_OK;
}

static MoonHitlResult moon_hitl_run_loop(MoonHitlSession *session,
                                         HitlInputAuthority authority,
                                         MoonHitlSmokeState *smoke)
{
    MoonFramePlan plan;
    uint32_t fixed_tick;
    uint32_t start = moon_dos_clock_now();
    uint32_t timeout_ticks = 0u;
    int finished = 0;
    MoonHitlResult result;

    if (smoke != NULL) {
        timeout_ticks = moon_dos_clock_frequency() *
                        MOON_HITL_SMOKE_TIMEOUT_SECONDS;
        if (timeout_ticks == 0u) {
            timeout_ticks = 1u;
        }
    }
    result = moon_hitl_present(session, smoke);
    if (result != MOON_HITL_OK) {
        return result;
    }

    while (finished == 0) {
        uint32_t now;

        if (moon_dos_runtime_drain_input(&session->dos, NULL) != MOON_DOS_OK) {
            return MOON_HITL_ERR_INPUT;
        }
        now = moon_dos_clock_now();
        if (moon_runtime_advance(&session->moon, now, &plan) != MOON_OK) {
            return MOON_HITL_ERR_RUNTIME;
        }
        for (fixed_tick = 0u; fixed_tick < plan.fixed_ticks && finished == 0;
             ++fixed_tick) {
            result = moon_hitl_process_ui_tick(session, authority, smoke,
                                               &finished);
            if (result != MOON_HITL_OK) {
                return result;
            }
        }
        if (plan.present != 0u || finished != 0) {
            result = moon_hitl_present(session, smoke);
            if (result != MOON_HITL_OK) {
                return result;
            }
        }
        if (smoke != NULL &&
            (uint32_t)(now - start) >= timeout_ticks) {
            return MOON_HITL_ERR_RUNTIME;
        }
    }
    return MOON_HITL_OK;
}

static void moon_hitl_record_commit_failure(MoonHitlSession *session,
                                            const char *payload,
                                            HitlInputAuthority authority)
{
    (void)hitl_journal_append(&session->journal,
                              HITL_JOURNAL_COMMIT_FAIL,
                              session->logical_tick,
                              "-",
                              payload,
                              authority);
    (void)hitl_journal_flush(&session->journal);
}

static MoonHitlResult moon_hitl_commit_summary(
    MoonHitlSession *session,
    HitlInputAuthority authority)
{
    FILE *candidate = NULL;
    FILE *verification = NULL;
    HitlCockpitResult result;
    HitlReplaceOutcome outcome;
    unsigned long error_line = 0u;
    int close_result;

    result = hitl_journal_append(&session->journal,
                                 HITL_JOURNAL_COMMIT,
                                 session->logical_tick,
                                 "-",
                                 "summary staged",
                                 authority);
    if (result != HITL_COCKPIT_OK ||
        hitl_journal_flush(&session->journal) != HITL_COCKPIT_OK) {
        moon_hitl_record_commit_failure(session, "journal flush failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }

    candidate = fopen(session->paths.new_path, "w+b");
    if (candidate == NULL) {
        moon_hitl_record_commit_failure(session, "candidate open failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }
    result = hitl_summary_write(candidate, &session->cockpit,
                                &session->journal);
    close_result = fclose(candidate);
    candidate = NULL;
    if (result != HITL_COCKPIT_OK || close_result != 0) {
        (void)remove(session->paths.new_path);
        moon_hitl_record_commit_failure(session, "candidate write failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }

    verification = fopen(session->paths.new_path, "rb");
    if (verification == NULL) {
        (void)remove(session->paths.new_path);
        moon_hitl_record_commit_failure(session, "candidate reopen failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }
    result = hitl_summary_verify(verification,
                                 &session->plan,
                                 &session->auto_evidence,
                                 session->files.journal_stream,
                                 &error_line);
    close_result = fclose(verification);
    verification = NULL;
    if (result != HITL_COCKPIT_OK || close_result != 0) {
        (void)remove(session->paths.new_path);
        moon_hitl_record_commit_failure(session, "candidate verify failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }

    memset(&outcome, 0, sizeof(outcome));
    result = hitl_summary_replace(session->paths.new_path,
                                  session->paths.out_path,
                                  session->paths.old_path,
                                  NULL,
                                  &outcome);
    if (result != HITL_COCKPIT_OK || outcome.output_committed == 0u) {
        moon_hitl_record_commit_failure(session, "summary replace failed",
                                        authority);
        return MOON_HITL_ERR_COMMIT;
    }
    return MOON_HITL_OK;
}

static uint32_t moon_hitl_hash_bytes(const uint8_t *bytes, size_t byte_count)
{
    uint32_t hash = UINT32_C(2166136261);
    size_t index;

    for (index = 0u; index < byte_count; ++index) {
        hash ^= (uint32_t)bytes[index];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

static uint32_t moon_hitl_manual_count(const HitlCockpit *cockpit)
{
    uint32_t count = 0u;
    size_t index;

    for (index = 0u; index < hitl_cockpit_case_count(cockpit); ++index) {
        const HitlCaseState *state =
            hitl_cockpit_case_state(cockpit, index);
        if (state != NULL && state->manual_evidence_current != 0 &&
            state->manual_status != HITL_STATUS_UNRUN) {
            ++count;
        }
    }
    return count;
}

static int moon_hitl_write_smoke_evidence(const char *path,
                                          const MoonHitlSmokeState *smoke,
                                          int passed)
{
    FILE *stream;
    int write_result;
    int close_result;

    stream = fopen(path, "wb");
    if (stream == NULL) {
        return 0;
    }
    write_result = fprintf(
        stream,
        "HITL_COCKPIT_SMOKE\t%s\tPROVENANCE=SYNTHETIC\tFRAMES=%lu\t"
        "EVENTS=%lu\tMANUAL=%lu\tHASH=%08lX\tREADBACK=%08lX\tCOMMIT=%s\r\n",
        passed != 0 ? "PASS" : "FAIL",
        (unsigned long)smoke->frames,
        (unsigned long)smoke->events,
        (unsigned long)smoke->manual_count,
        (unsigned long)smoke->frame_hash,
        (unsigned long)smoke->readback_hash,
        smoke->committed != 0u ? "PASS" : "FAIL");
    close_result = fclose(stream);
    return write_result >= 0 && close_result == 0;
}

static int moon_hitl_prepare_smoke_evidence(const char *path)
{
    FILE *stream = fopen(path, "wb");

    if (stream == NULL) {
        return 0;
    }
    return fclose(stream) == 0;
}

static MoonHitlResult moon_hitl_run_internal(
    const char *plan_path,
    HitlInputAuthority authority,
    int synthetic_smoke)
{
    MoonHitlSession *session = &moon_hitl_session;
    MoonHitlSmokeState smoke;
    MoonHitlSmokeState *smoke_pointer = synthetic_smoke != 0 ? &smoke : NULL;
    MoonHitlResult result = MOON_HITL_OK;
    MoonHitlResult cleanup_result;
    HitlCockpitResult cockpit_result;
    const char *open_case_id = "-";
    int smoke_passed;

    if (plan_path == NULL || plan_path[0] == '\0' ||
        (authority != HITL_INPUT_LIVE_DOS &&
         authority != HITL_INPUT_SYNTHETIC) ||
        (synthetic_smoke != 0) != (authority == HITL_INPUT_SYNTHETIC)) {
        return MOON_HITL_ERR_ARGUMENT;
    }
    memset(session, 0, sizeof(*session));
    memset(&smoke, 0, sizeof(smoke));
    memset(moon_hitl_framebuffer, 0, sizeof(moon_hitl_framebuffer));
    memset(moon_hitl_readback, 0, sizeof(moon_hitl_readback));

    if (!moon_hitl_paths_init(&session->paths, plan_path)) {
        return MOON_HITL_ERR_PATH;
    }
    if (synthetic_smoke != 0 &&
        !moon_hitl_prepare_smoke_evidence(session->paths.smoke_path)) {
        return MOON_HITL_ERR_EVIDENCE;
    }

    result = moon_hitl_load_plan(plan_path, &session->plan);
    if (result != MOON_HITL_OK) {
        goto cleanup;
    }
    result = moon_hitl_load_auto(session->paths.auto_path,
                                 &session->plan,
                                 &session->auto_evidence);
    if (result != MOON_HITL_OK) {
        goto cleanup;
    }
    cockpit_result = hitl_cockpit_init(&session->cockpit,
                                       &session->plan,
                                       &session->auto_evidence,
                                       authority);
    if (cockpit_result != HITL_COCKPIT_OK) {
        result = cockpit_result == HITL_COCKPIT_ERR_IDENTITY
                     ? MOON_HITL_ERR_AUTO : MOON_HITL_ERR_CGUI;
        goto cleanup;
    }
    result = moon_hitl_attach_journal(session);
    if (result != MOON_HITL_OK) {
        goto cleanup;
    }
    if (session->plan.case_count != 0u) {
        open_case_id = session->plan.cases[0].id;
    }
    if (hitl_journal_append(&session->journal,
                            HITL_JOURNAL_OPEN,
                            session->logical_tick,
                            open_case_id,
                            authority == HITL_INPUT_LIVE_DOS
                                ? "TEST COCKPIT LIVE_DOS"
                                : "TEST COCKPIT SYNTHETIC",
                            authority) != HITL_COCKPIT_OK) {
        result = MOON_HITL_ERR_JOURNAL;
        goto cleanup;
    }
    if (!hitl_cockpit_auto_identity_current(&session->cockpit) &&
        hitl_journal_append(&session->journal,
                            HITL_JOURNAL_WARNING,
                            session->logical_tick,
                            "-",
                            "AUTO evidence stale or malformed",
                            authority) != HITL_COCKPIT_OK) {
        result = MOON_HITL_ERR_JOURNAL;
        goto cleanup;
    }
    result = moon_hitl_initialize_dos_ui(session);
    if (result != MOON_HITL_OK) {
        goto cleanup;
    }
    if (synthetic_smoke != 0) {
        result = moon_hitl_probe_synthetic_authority(session, &smoke);
        if (result != MOON_HITL_OK) {
            goto cleanup;
        }
    }
    result = moon_hitl_run_loop(session, authority, smoke_pointer);

cleanup:
    if (synthetic_smoke != 0 && session->cockpit.initialized != 0u) {
        smoke.events = hitl_journal_event_count(&session->journal);
        smoke.manual_count = moon_hitl_manual_count(&session->cockpit);
    }
    if (session->ui.initialized != 0u) {
        hitl_cockpit_ui_reset(&session->ui);
    }
    if (session->cgui_initialized != 0u) {
        cgui_context_reset(&session->cgui);
        session->cgui_initialized = 0u;
    }
    if (session->dos_initialized != 0u) {
        if (moon_dos_runtime_shutdown(&session->dos) != MOON_DOS_OK) {
            result = MOON_HITL_ERR_SHUTDOWN;
        }
        session->dos_initialized = 0u;
    }
    cleanup_result = moon_hitl_close_journal(session);
    if (cleanup_result != MOON_HITL_OK && result == MOON_HITL_OK) {
        result = cleanup_result;
    }

    smoke_passed = result == MOON_HITL_OK &&
                   smoke.frames != 0u &&
                   smoke.events != 0u &&
                   smoke.manual_attempted != 0u &&
                   smoke.manual_count == 0u &&
                   smoke.frame_hash == smoke.readback_hash &&
                   smoke.committed != 0u;
    if (synthetic_smoke != 0) {
        if (!moon_hitl_write_smoke_evidence(session->paths.smoke_path,
                                            &smoke,
                                            smoke_passed)) {
            result = MOON_HITL_ERR_EVIDENCE;
        } else if (!smoke_passed && result == MOON_HITL_OK) {
            result = MOON_HITL_ERR_EVIDENCE;
        }
    }
    if (session->cockpit.initialized != 0u) {
        hitl_cockpit_reset(&session->cockpit);
    }
    return result;
}

const char *moon_hitl_result_name(MoonHitlResult result)
{
    switch (result) {
        case MOON_HITL_OK: return "ok";
        case MOON_HITL_ERR_ARGUMENT: return "invalid argument";
        case MOON_HITL_ERR_PATH: return "DOS-safe evidence path";
        case MOON_HITL_ERR_PLAN: return "HITL.IN load";
        case MOON_HITL_ERR_AUTO: return "AUTO.OUT load";
        case MOON_HITL_ERR_JOURNAL: return "HITL.JRN";
        case MOON_HITL_ERR_RUNTIME: return "runtime initialization/advance";
        case MOON_HITL_ERR_INPUT: return "input binding/drain";
        case MOON_HITL_ERR_CGUI: return "CGUI cockpit";
        case MOON_HITL_ERR_PRESENT: return "VGA presentation";
        case MOON_HITL_ERR_COMMIT: return "transactional summary commit";
        case MOON_HITL_ERR_SHUTDOWN: return "DOS runtime shutdown";
        case MOON_HITL_ERR_EVIDENCE: return "HITLSMOK.OUT";
        default: return "unknown HITL integration error";
    }
}

MoonHitlResult moon_hitl_run_live_dos(const char *plan_path)
{
    return moon_hitl_run_internal(plan_path, HITL_INPUT_LIVE_DOS, 0);
}

MoonHitlResult moon_hitl_run_synthetic_smoke(const char *plan_path)
{
    return moon_hitl_run_internal(plan_path, HITL_INPUT_SYNTHETIC, 1);
}
