#include "moon/cgui.h"
#include "moon/dos_runtime.h"
#include "moon/runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/movedata.h>

#define CGUI_PRESENT_EVIDENCE_FILE "CGUIPRS.OUT"
#define CGUI_PRESENT_EXPECTED_HASH UINT32_C(0xec5f78fb)
#define CGUI_VGA_MEMORY UINT32_C(0x000a0000)

#define DEMO_TEXT(literal) { (literal), sizeof(literal) - 1u }

enum DemoAction {
    DEMO_ACTION_UP = 0,
    DEMO_ACTION_DOWN,
    DEMO_ACTION_ENTER,
    DEMO_ACTION_SPACE,
    DEMO_ACTION_ESCAPE,
    DEMO_ACTION_F1,
    DEMO_ACTION_FIRST,
    DEMO_ACTION_CONFIRM,
    DEMO_ACTION_QUIT,
    DEMO_ACTION_YES,
    DEMO_ACTION_NO,
    DEMO_ACTION_COUNT
};

enum DemoId {
    DEMO_MENU_OWNER = 77,
    DEMO_ITEM_FIRST = 101,
    DEMO_ITEM_CONFIRM = 400,
    DEMO_ITEM_DISABLED = 9001,
    DEMO_ITEM_QUIT = 65521,
    DEMO_HELP_OWNER = 70001,
    DEMO_CONFIRM_OWNER = 80001,
    DEMO_CONFIRM_YES = 80011,
    DEMO_CONFIRM_NO = 80029
};

static uint8_t demo_framebuffer[MOON_DOS_FRAMEBUFFER_BYTES];
static uint8_t demo_readback[MOON_DOS_FRAMEBUFFER_BYTES];

static const CguiMenuItem demo_menu_items[] = {
    { DEMO_ITEM_FIRST, DEMO_TEXT("RUN CGUI TEST"), 1u },
    { DEMO_ITEM_CONFIRM, DEMO_TEXT("OPEN CONFIRMATION"), 1u },
    { DEMO_ITEM_DISABLED, DEMO_TEXT("DISABLED PROTOTYPE"), 0u },
    { DEMO_ITEM_QUIT, DEMO_TEXT("RETURN TO DOS"), 1u }
};

static const CguiHelpSpec demo_help = {
    DEMO_HELP_OWNER,
    DEMO_TEXT("HELP ME!"),
    DEMO_TEXT("UP/DOWN  MOVE\nENTER/SPACE  SELECT\nF1/ESC  CLOSE HELP\n1/2/4  ACCELERATORS"),
    DEMO_TEXT("PRESS F1, ENTER, SPACE, OR ESC")
};

static const CguiConfirmSpec demo_confirm = {
    DEMO_CONFIRM_OWNER,
    DEMO_CONFIRM_YES,
    DEMO_CONFIRM_NO,
    DEMO_TEXT("DELIBERATE CONFIRMATION"),
    DEMO_TEXT("DEFAULT FOCUS IS NO.\nMOVE, THEN ACCEPT YES. Y/N ANSWER."),
    DEMO_TEXT("YES (Y)"),
    DEMO_TEXT("NO (N)")
};

static const CguiText demo_status_ready =
    DEMO_TEXT("READY - F1 OPENS CALLER-OWNED HELP");

typedef struct DemoUiState {
    char status_bytes[96];
    CguiText status;
    uint32_t activation_count;
} DemoUiState;

static int demo_ascii_equal(const char *left, const char *right)
{
    unsigned char left_character;
    unsigned char right_character;

    if (left == NULL || right == NULL) {
        return 0;
    }
    while (*left != '\0' && *right != '\0') {
        left_character = (unsigned char)*left;
        right_character = (unsigned char)*right;
        if (left_character >= (unsigned char)'a' &&
            left_character <= (unsigned char)'z') {
            left_character = (unsigned char)(left_character -
                (unsigned char)'a' + (unsigned char)'A');
        }
        if (right_character >= (unsigned char)'a' &&
            right_character <= (unsigned char)'z') {
            right_character = (unsigned char)(right_character -
                (unsigned char)'a' + (unsigned char)'A');
        }
        if (left_character != right_character) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static uint32_t demo_hash_bytes(const uint8_t *bytes, size_t byte_count)
{
    uint32_t hash = UINT32_C(2166136261);
    size_t index;

    for (index = 0u; index < byte_count; ++index) {
        hash ^= (uint32_t)bytes[index];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

static int demo_set_status(DemoUiState *state, const char *message)
{
    int written;

    if (state == NULL || message == NULL) {
        return 0;
    }
    written = snprintf(state->status_bytes, sizeof(state->status_bytes),
                       "%s  ACT=%lu", message,
                       (unsigned long)state->activation_count);
    if (written < 0 || (size_t)written >= sizeof(state->status_bytes)) {
        return 0;
    }
    state->status.data = state->status_bytes;
    state->status.length = (size_t)written;
    return 1;
}

static int demo_set_activation_status(DemoUiState *state,
                                      const char *source,
                                      CguiId item_id)
{
    int written;

    if (state == NULL || source == NULL) {
        return 0;
    }
    ++state->activation_count;
    written = snprintf(state->status_bytes, sizeof(state->status_bytes),
                       "ACT=%lu VIA %s ITEM=%lu",
                       (unsigned long)state->activation_count,
                       source,
                       (unsigned long)item_id);
    if (written < 0 || (size_t)written >= sizeof(state->status_bytes)) {
        return 0;
    }
    state->status.data = state->status_bytes;
    state->status.length = (size_t)written;
    return 1;
}

static int demo_bind(MoonInput *input,
                     uint16_t action,
                     uint16_t slot,
                     MoonKey key)
{
    return moon_input_bind(input, action, slot, key) == MOON_OK;
}

static int demo_bind_input(MoonInput *input)
{
    return
        demo_bind(input, DEMO_ACTION_UP, 0u, MOON_KEY_E0(0x48u)) &&
        demo_bind(input, DEMO_ACTION_DOWN, 0u, MOON_KEY_E0(0x50u)) &&
        demo_bind(input, DEMO_ACTION_ENTER, 0u, MOON_KEY_NORMAL(0x1cu)) &&
        demo_bind(input, DEMO_ACTION_SPACE, 0u, MOON_KEY_NORMAL(0x39u)) &&
        demo_bind(input, DEMO_ACTION_ESCAPE, 0u, MOON_KEY_NORMAL(0x01u)) &&
        demo_bind(input, DEMO_ACTION_F1, 0u, MOON_KEY_NORMAL(0x3bu)) &&
        demo_bind(input, DEMO_ACTION_FIRST, 0u, MOON_KEY_NORMAL(0x02u)) &&
        demo_bind(input, DEMO_ACTION_CONFIRM, 0u, MOON_KEY_NORMAL(0x03u)) &&
        demo_bind(input, DEMO_ACTION_QUIT, 0u, MOON_KEY_NORMAL(0x05u)) &&
        demo_bind(input, DEMO_ACTION_YES, 0u, MOON_KEY_NORMAL(0x15u)) &&
        demo_bind(input, DEMO_ACTION_NO, 0u, MOON_KEY_NORMAL(0x31u));
}

static int demo_initialize(MoonContext *moon,
                           MoonDosRuntime *dos,
                           CguiContext *cgui)
{
    MoonRuntimeConfig runtime_config;
    CguiSurface surface;
    CguiPalette palette;

    if (moon_runtime_config_defaults(&runtime_config,
                                     moon_dos_clock_frequency()) != MOON_OK) {
        return 0;
    }
    runtime_config.action_count = (uint16_t)DEMO_ACTION_COUNT;
    if (moon_runtime_init(moon, &runtime_config,
                          moon_dos_clock_now()) != MOON_OK ||
        !demo_bind_input(&moon->input)) {
        return 0;
    }
    if (moon_dos_runtime_init(dos, &moon->input) != MOON_DOS_OK) {
        return 0;
    }

    memset(demo_framebuffer, 0, sizeof(demo_framebuffer));
    surface.pixels = demo_framebuffer;
    surface.width = MOON_DOS_FRAMEBUFFER_WIDTH;
    surface.height = MOON_DOS_FRAMEBUFFER_HEIGHT;
    surface.stride = (size_t)MOON_DOS_FRAMEBUFFER_WIDTH;
    surface.byte_count = sizeof(demo_framebuffer);
    cgui_palette_classic(&palette);
    if (cgui_context_init(cgui, &surface, cgui_font_builtin_3x5(),
                          &palette) != CGUI_OK ||
        cgui_menu_set_items(cgui, DEMO_MENU_OWNER, demo_menu_items,
            sizeof(demo_menu_items) / sizeof(demo_menu_items[0])) != CGUI_OK) {
        (void)moon_dos_runtime_shutdown(dos);
        return 0;
    }
    return 1;
}

static int demo_shutdown(CguiContext *cgui, MoonDosRuntime *dos)
{
    MoonDosResult result;

    cgui_context_reset(cgui);
    result = moon_dos_runtime_shutdown(dos);
    return result == MOON_DOS_OK;
}

static CguiResult demo_draw_scene(CguiContext *cgui, CguiText status)
{
    static const CguiRect stars[] = {
        { 13, 9, 14, 10 }, { 44, 27, 46, 29 },
        { 279, 18, 280, 19 }, { 301, 62, 303, 64 },
        { 20, 132, 21, 133 }, { 290, 155, 291, 156 },
        { 56, 184, 58, 186 }, { 262, 190, 263, 191 }
    };
    CguiRect screen = { 0, 0, 320, 200 };
    CguiRect menu = { 73, 42, 247, 151 };
    CguiRect help = { 28, 19, 292, 181 };
    CguiRect confirm = { 55, 54, 265, 148 };
    CguiText title = DEMO_TEXT("CGUI CONTEXT CORE");
    CguiText footer = DEMO_TEXT("ARROWS ENTER SPACE ESC F1");
    size_t index;
    CguiResult result;

    result = cgui_fill_rect(cgui, screen, CGUI_ROLE_DESKTOP);
    for (index = 0u; result == CGUI_OK &&
         index < sizeof(stars) / sizeof(stars[0]); ++index) {
        result = cgui_fill_rect(cgui, stars[index], CGUI_ROLE_BORDER_LIGHT);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_text(cgui, 126, 17, title, CGUI_ROLE_SELECTION_TEXT);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_menu(cgui, menu);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_text(cgui, 14, 173, status, CGUI_ROLE_FOCUS);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_text(cgui, 106, 190, footer,
                                CGUI_ROLE_TEXT_DISABLED);
    }
    if (result == CGUI_OK && cgui_help_is_open(cgui)) {
        result = cgui_draw_help(cgui, help);
    }
    if (result == CGUI_OK && cgui_confirm_is_open(cgui)) {
        result = cgui_draw_confirm(cgui, confirm);
    }
    return result;
}

static void demo_copy_action(CguiActionState *destination,
                             const MoonInput *input,
                             uint16_t action)
{
    const MoonButtonState *source = moon_input_action(input, action);

    destination->pressed = source != NULL ? source->pressed : 0u;
    destination->repeat = source != NULL ? source->repeat : 0u;
}

static void demo_make_input(const MoonInput *moon_input,
                            CguiModalKind modal_kind,
                            CguiInputFrame *input)
{
    const MoonButtonState *button;

    memset(input, 0, sizeof(*input));
    demo_copy_action(&input->actions[CGUI_ACTION_UP], moon_input,
                     DEMO_ACTION_UP);
    demo_copy_action(&input->actions[CGUI_ACTION_DOWN], moon_input,
                     DEMO_ACTION_DOWN);
    demo_copy_action(&input->actions[CGUI_ACTION_ENTER], moon_input,
                     DEMO_ACTION_ENTER);
    demo_copy_action(&input->actions[CGUI_ACTION_SPACE], moon_input,
                     DEMO_ACTION_SPACE);
    demo_copy_action(&input->actions[CGUI_ACTION_ESCAPE], moon_input,
                     DEMO_ACTION_ESCAPE);
    demo_copy_action(&input->actions[CGUI_ACTION_F1], moon_input,
                     DEMO_ACTION_F1);

    if (modal_kind == CGUI_MODAL_CONFIRM) {
        button = moon_input_action(moon_input, DEMO_ACTION_NO);
        if (button != NULL && button->pressed != 0u) {
            input->accelerator_id = DEMO_CONFIRM_NO;
            return;
        }
        button = moon_input_action(moon_input, DEMO_ACTION_YES);
        if (button != NULL && button->pressed != 0u) {
            input->accelerator_id = DEMO_CONFIRM_YES;
        }
        return;
    }

    button = moon_input_action(moon_input, DEMO_ACTION_FIRST);
    if (button != NULL && button->pressed != 0u) {
        input->accelerator_id = DEMO_ITEM_FIRST;
        return;
    }
    button = moon_input_action(moon_input, DEMO_ACTION_CONFIRM);
    if (button != NULL && button->pressed != 0u) {
        input->accelerator_id = DEMO_ITEM_CONFIRM;
        return;
    }
    button = moon_input_action(moon_input, DEMO_ACTION_QUIT);
    if (button != NULL && button->pressed != 0u) {
        input->accelerator_id = DEMO_ITEM_QUIT;
    }
}

static CguiResult demo_handle_event(CguiContext *cgui,
                                    const CguiEvent *event,
                                    const char *input_source,
                                    DemoUiState *state,
                                    int *running)
{
    switch (event->type) {
        case CGUI_EVENT_NONE:
            return CGUI_OK;
        case CGUI_EVENT_MENU_ACTIVATE:
            if (!demo_set_activation_status(state, input_source,
                                            event->item_id)) {
                return CGUI_ERR_STATE;
            }
            if (event->item_id == DEMO_ITEM_FIRST) {
                return CGUI_OK;
            } else if (event->item_id == DEMO_ITEM_CONFIRM) {
                return cgui_confirm_open(cgui, &demo_confirm);
            } else if (event->item_id == DEMO_ITEM_QUIT) {
                *running = 0;
            }
            return CGUI_OK;
        case CGUI_EVENT_MENU_CANCEL:
            *running = 0;
            return CGUI_OK;
        case CGUI_EVENT_HELP_REQUEST:
            return cgui_help_open(cgui, &demo_help);
        case CGUI_EVENT_HELP_CLOSE:
            return CGUI_OK;
        case CGUI_EVENT_CONFIRM_YES:
            return demo_set_status(state, "CONFIRM YES")
                       ? CGUI_OK : CGUI_ERR_STATE;
        case CGUI_EVENT_CONFIRM_NO:
            return demo_set_status(state, "CONFIRM NO")
                       ? CGUI_OK : CGUI_ERR_STATE;
        case CGUI_EVENT_CONFIRM_CANCEL:
            return demo_set_status(state, "CONFIRM CANCEL")
                       ? CGUI_OK : CGUI_ERR_STATE;
        default:
            return CGUI_ERR_STATE;
    }
}

static const char *demo_input_source(const CguiInputFrame *input)
{
    if (input->accelerator_id != CGUI_ID_NONE) {
        return "ACCEL";
    }
    if (input->actions[CGUI_ACTION_ENTER].pressed != 0u &&
        input->actions[CGUI_ACTION_SPACE].pressed != 0u) {
        return "ENTER+SPACE";
    }
    if (input->actions[CGUI_ACTION_ENTER].pressed != 0u) {
        return "ENTER";
    }
    if (input->actions[CGUI_ACTION_SPACE].pressed != 0u) {
        return "SPACE";
    }
    return "ACTION";
}

static int demo_run_interactive(MoonContext *moon,
                                MoonDosRuntime *dos,
                                CguiContext *cgui)
{
    MoonFramePlan plan;
    CguiInputFrame input;
    CguiEvent event;
    DemoUiState state;
    uint32_t tick;
    int running = 1;

    memset(&state, 0, sizeof(state));
    if (!demo_set_status(&state, "READY - F1 OPENS HELP")) {
        return 0;
    }
    if (demo_draw_scene(cgui, state.status) != CGUI_OK ||
        moon_dos_runtime_present(dos, demo_framebuffer,
                                 sizeof(demo_framebuffer)) != MOON_DOS_OK) {
        return 0;
    }

    while (running != 0) {
        if (moon_dos_runtime_drain_input(dos, NULL) != MOON_DOS_OK ||
            moon_runtime_advance(moon, moon_dos_clock_now(), &plan) != MOON_OK) {
            return 0;
        }
        for (tick = 0u; tick < plan.fixed_ticks; ++tick) {
            moon_input_tick(&moon->input);
            demo_make_input(&moon->input, cgui_modal_kind(cgui), &input);
            if (cgui_update(cgui, &input, &event) != CGUI_OK ||
                demo_handle_event(cgui, &event, demo_input_source(&input),
                                  &state, &running) != CGUI_OK) {
                return 0;
            }
        }
        if (plan.present != 0u && running != 0) {
            if (demo_draw_scene(cgui, state.status) != CGUI_OK ||
                moon_dos_runtime_present(dos, demo_framebuffer,
                    sizeof(demo_framebuffer)) != MOON_DOS_OK) {
                return 0;
            }
        }
    }
    return 1;
}

static int demo_write_smoke_evidence(int passed,
                                     uint32_t frame_hash,
                                     uint32_t readback_hash)
{
    FILE *output = fopen(CGUI_PRESENT_EVIDENCE_FILE, "wb");

    if (output == NULL) {
        return 0;
    }
    if (fprintf(output,
                "CGUI_PRESENT_SMOKE\t%s\tHASH=%08lX\tREADBACK=%08lX\n",
                passed != 0 ? "PASS" : "FAIL",
                (unsigned long)frame_hash,
                (unsigned long)readback_hash) < 0 ||
        fclose(output) != 0) {
        return 0;
    }
    return 1;
}

static int demo_run_smoke(MoonDosRuntime *dos, CguiContext *cgui)
{
    uint32_t frame_hash = 0u;
    uint32_t readback_hash = 0u;
    int passed = 0;
    int shutdown_ok;

    if (cgui_confirm_open(cgui, &demo_confirm) == CGUI_OK &&
        demo_draw_scene(cgui, demo_status_ready) == CGUI_OK &&
        cgui_surface_hash(cgui, &frame_hash) == CGUI_OK &&
        moon_dos_runtime_present(dos, demo_framebuffer,
                                 sizeof(demo_framebuffer)) == MOON_DOS_OK) {
        dosmemget(CGUI_VGA_MEMORY, sizeof(demo_readback), demo_readback);
        readback_hash = demo_hash_bytes(demo_readback,
                                        sizeof(demo_readback));
        passed = frame_hash == readback_hash &&
            (CGUI_PRESENT_EXPECTED_HASH == 0u ||
             frame_hash == CGUI_PRESENT_EXPECTED_HASH);
    }

    shutdown_ok = demo_shutdown(cgui, dos);
    passed = passed != 0 && shutdown_ok != 0;
    if (!demo_write_smoke_evidence(passed, frame_hash, readback_hash)) {
        return 0;
    }
    return passed;
}

int main(int argc, char **argv)
{
    MoonContext moon;
    MoonDosRuntime dos;
    CguiContext cgui;
    int smoke;
    int success;

    memset(&moon, 0, sizeof(moon));
    memset(&dos, 0, sizeof(dos));
    memset(&cgui, 0, sizeof(cgui));

    smoke = argc == 2 && demo_ascii_equal(argv[1], "/SMOKE");
    if (!smoke && !(argc == 1 ||
                    (argc == 2 && demo_ascii_equal(argv[1],
                                                   "/INTERACTIVE")))) {
        fputs("usage: CGUIPRES [/SMOKE | /INTERACTIVE]\n", stderr);
        return 2;
    }
    if (!demo_initialize(&moon, &dos, &cgui)) {
        (void)moon_dos_runtime_shutdown(&dos);
        if (smoke) {
            (void)demo_write_smoke_evidence(0, 0u, 0u);
        }
        return 1;
    }

    if (smoke) {
        return demo_run_smoke(&dos, &cgui) ? 0 : 1;
    }

    success = demo_run_interactive(&moon, &dos, &cgui);
    if (!demo_shutdown(&cgui, &dos)) {
        success = 0;
    }
    if (!success) {
        fputs("CGUI presentation demo failed.\n", stderr);
        return 1;
    }
    puts("CGUI presentation demo returned safely to DOS.");
    return 0;
}
