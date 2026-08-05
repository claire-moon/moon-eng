#include "moon/hitl_cockpit_ui.h"

#include <stdio.h>
#include <string.h>

static const char ui_help_title[] = "TEST COCKPIT HELP";
static const char ui_help_body[] =
    "UP/DOWN selects a case or action. ENTER/SPACE opens it. ESC returns or "
    "requests commit and exit. F1 opens this help. AUTO, MANUAL, and RESULT "
    "are separate lanes. A MANUAL PASS, FAIL, or BLOCKED result requires two "
    "distinct fresh key edges in a LIVE DOS session. The confirmation defaults "
    "to NO. Smoke and replay input may navigate and render, but can never write "
    "MANUAL evidence. Stale identity disables MANUAL PASS.";
static const char ui_help_close[] = "CLOSE";
static const char ui_pass_label[] = "RECORD MANUAL PASS";
static const char ui_fail_label[] = "RECORD MANUAL FAIL";
static const char ui_blocked_label[] = "RECORD MANUAL BLOCKED";
static const char ui_back_label[] = "BACK TO CASE LIST";
static const char ui_confirm_yes_label[] = "RECORD";
static const char ui_confirm_no_label[] = "CANCEL";
static const char ui_exit_title[] = "COMMIT TEST EVIDENCE?";
static const char ui_exit_message[] =
    "Write and verify the transactional summary, then exit TEST COCKPIT?";
static const char ui_exit_yes_label[] = "COMMIT + EXIT";
static const char ui_exit_no_label[] = "CANCEL";

static CguiText ui_text(const char *text)
{
    CguiText result;

    result.data = text;
    result.length = text != NULL ? strlen(text) : 0u;
    return result;
}

static CguiRect ui_rect(int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1)
{
    CguiRect result;

    result.x0 = x0;
    result.y0 = y0;
    result.x1 = x1;
    result.y1 = y1;
    return result;
}

static int ui_is_valid(const HitlCockpitUi *ui)
{
    size_t count;

    if (ui == NULL || ui->initialized == 0u || ui->cgui == NULL ||
        ui->cockpit == NULL || ui->journal == NULL ||
        ui->cockpit->initialized == 0u || ui->cockpit->plan == NULL ||
        ui->cockpit->auto_evidence == NULL ||
        ui->mode < HITL_COCKPIT_UI_LIST ||
        ui->mode > HITL_COCKPIT_UI_DONE) {
        return 0;
    }
    count = hitl_cockpit_case_count(ui->cockpit);
    return count != 0u && count <= HITL_MAX_CASES &&
           ui->selected_index < count;
}

static void ui_event_clear(HitlCockpitUiEvent *event)
{
    if (event != NULL) {
        event->type = HITL_COCKPIT_UI_EVENT_NONE;
    }
}

static void ui_notice_clear(HitlCockpitUi *ui)
{
    if (ui != NULL) {
        ui->notice[0] = '\0';
    }
}

static void ui_notice_result(HitlCockpitUi *ui,
                             const char *prefix,
                             HitlCockpitResult result)
{
    if (ui == NULL) {
        return;
    }
    (void)snprintf(ui->notice, sizeof(ui->notice), "%s: %s",
                   prefix, hitl_cockpit_result_string(result));
}

static const char *ui_banner(const HitlCockpitUi *ui)
{
    if (ui->notice[0] != '\0') {
        return ui->notice;
    }
    if (ui->cockpit->session_authority != HITL_INPUT_LIVE_DOS) {
        return "READ ONLY - SMOKE/REPLAY CANNOT WRITE MANUAL";
    }
    if (!hitl_cockpit_auto_identity_current(ui->cockpit)) {
        return "STALE EVIDENCE - MANUAL PASS DISABLED";
    }
    return "LIVE DOS - MANUAL RESULTS REQUIRE TWO FRESH KEY EDGES";
}

static size_t ui_page_count_for(size_t count)
{
    return (count + (size_t)HITL_COCKPIT_UI_VISIBLE_CASES - 1u) /
           (size_t)HITL_COCKPIT_UI_VISIBLE_CASES;
}

static CguiResult ui_build_list(HitlCockpitUi *ui)
{
    size_t count;
    size_t page_count;
    size_t index;
    CguiResult result;

    count = hitl_cockpit_case_count(ui->cockpit);
    ui->page_start =
        (ui->selected_index / (size_t)HITL_COCKPIT_UI_VISIBLE_CASES) *
        (size_t)HITL_COCKPIT_UI_VISIBLE_CASES;
    page_count = count - ui->page_start;
    if (page_count > (size_t)HITL_COCKPIT_UI_VISIBLE_CASES) {
        page_count = (size_t)HITL_COCKPIT_UI_VISIBLE_CASES;
    }

    for (index = 0u; index < page_count; ++index) {
        size_t case_index;
        const HitlCase *test_case;
        const HitlCaseState *state;
        HitlCombinedResult combined;

        case_index = ui->page_start + index;
        test_case = &ui->cockpit->plan->cases[case_index];
        state = hitl_cockpit_case_state(ui->cockpit, case_index);
        if (state == NULL) {
            return CGUI_ERR_STATE;
        }
        combined = hitl_cockpit_combined(ui->cockpit, case_index);
        (void)snprintf(ui->row_text[index], sizeof(ui->row_text[index]),
                       "%-24.24s %-7.7s %-7.7s %-7.7s",
                       test_case->id,
                       hitl_status_string(state->auto_status),
                       hitl_status_string(state->manual_status),
                       hitl_status_string(combined.status));
        ui->menu_items[index].id =
            HITL_COCKPIT_UI_CASE_BASE + (CguiId)case_index + UINT32_C(1);
        ui->menu_items[index].label = ui_text(ui->row_text[index]);
        ui->menu_items[index].enabled = 1u;
    }

    result = cgui_menu_set_items(ui->cgui,
                                 HITL_COCKPIT_UI_OWNER_LIST,
                                 ui->menu_items,
                                 page_count);
    if (result != CGUI_OK) {
        return result;
    }
    return cgui_menu_set_focus(
        ui->cgui,
        HITL_COCKPIT_UI_CASE_BASE + (CguiId)ui->selected_index + UINT32_C(1));
}

static CguiResult ui_build_detail(HitlCockpitUi *ui)
{
    static const CguiId ids[] = {
        HITL_COCKPIT_UI_ACT_PASS,
        HITL_COCKPIT_UI_ACT_FAIL,
        HITL_COCKPIT_UI_ACT_BLOCKED,
        HITL_COCKPIT_UI_ACT_BACK
    };
    static const char *const labels[] = {
        ui_pass_label,
        ui_fail_label,
        ui_blocked_label,
        ui_back_label
    };
    size_t index;

    for (index = 0u; index < sizeof(ids) / sizeof(ids[0]); ++index) {
        ui->menu_items[index].id = ids[index];
        ui->menu_items[index].label = ui_text(labels[index]);
        /* Keep actions reachable so denied synthetic/stale attempts warn. */
        ui->menu_items[index].enabled = 1u;
    }
    return cgui_menu_set_items(ui->cgui,
                               HITL_COCKPIT_UI_OWNER_DETAIL,
                               ui->menu_items,
                               sizeof(ids) / sizeof(ids[0]));
}

static CguiResult ui_enter_list(HitlCockpitUi *ui)
{
    CguiResult result;

    result = ui_build_list(ui);
    if (result == CGUI_OK) {
        ui->mode = HITL_COCKPIT_UI_LIST;
    }
    return result;
}

static CguiResult ui_enter_detail(HitlCockpitUi *ui)
{
    CguiResult result;

    result = ui_build_detail(ui);
    if (result == CGUI_OK) {
        ui->mode = HITL_COCKPIT_UI_DETAIL;
    }
    return result;
}

static CguiResult ui_open_help(HitlCockpitUi *ui)
{
    CguiHelpSpec spec;
    CguiResult result;

    spec.owner_id = HITL_COCKPIT_UI_OWNER_HELP;
    spec.title = ui_text(ui_help_title);
    spec.body = ui_text(ui_help_body);
    spec.close_label = ui_text(ui_help_close);
    result = cgui_help_open(ui->cgui, &spec);
    if (result == CGUI_OK) {
        ui->return_mode = ui->mode;
        ui->mode = HITL_COCKPIT_UI_HELP;
    }
    return result;
}

static CguiResult ui_open_exit_confirm(HitlCockpitUi *ui)
{
    CguiConfirmSpec spec;
    CguiResult result;

    spec.owner_id = HITL_COCKPIT_UI_OWNER_CONFIRM_EXIT;
    spec.yes_id = HITL_COCKPIT_UI_EXIT_YES;
    spec.no_id = HITL_COCKPIT_UI_EXIT_NO;
    spec.title = ui_text(ui_exit_title);
    spec.message = ui_text(ui_exit_message);
    spec.yes_label = ui_text(ui_exit_yes_label);
    spec.no_label = ui_text(ui_exit_no_label);
    result = cgui_confirm_open(ui->cgui, &spec);
    if (result == CGUI_OK) {
        ui->mode = HITL_COCKPIT_UI_CONFIRM_EXIT;
    }
    return result;
}

static CguiResult ui_open_manual_confirm(HitlCockpitUi *ui,
                                         HitlStatus status)
{
    const HitlCase *test_case;
    CguiConfirmSpec spec;
    CguiResult result;

    test_case = &ui->cockpit->plan->cases[ui->selected_index];
    (void)snprintf(ui->modal_title, sizeof(ui->modal_title),
                   "RECORD MANUAL %s?", hitl_status_string(status));
    (void)snprintf(ui->modal_message, sizeof(ui->modal_message),
                   "CASE %s: %s. Press a second distinct fresh LIVE DOS key "
                   "edge to record this result.",
                   test_case->id, test_case->title);
    spec.owner_id = HITL_COCKPIT_UI_OWNER_CONFIRM_MAN;
    spec.yes_id = HITL_COCKPIT_UI_MAN_YES;
    spec.no_id = HITL_COCKPIT_UI_MAN_NO;
    spec.title = ui_text(ui->modal_title);
    spec.message = ui_text(ui->modal_message);
    spec.yes_label = ui_text(ui_confirm_yes_label);
    spec.no_label = ui_text(ui_confirm_no_label);
    result = cgui_confirm_open(ui->cgui, &spec);
    if (result == CGUI_OK) {
        ui->mode = HITL_COCKPIT_UI_CONFIRM_MANUAL;
    }
    return result;
}

static int ui_list_navigation(const CguiInputFrame *input, int *direction)
{
    int up;
    int down;

    if (input->actions[CGUI_ACTION_ESCAPE].pressed != 0u ||
        input->actions[CGUI_ACTION_F1].pressed != 0u ||
        input->accelerator_id != CGUI_ID_NONE) {
        return 0;
    }
    up = input->actions[CGUI_ACTION_UP].pressed != 0u ||
         input->actions[CGUI_ACTION_UP].repeat != 0u;
    down = input->actions[CGUI_ACTION_DOWN].pressed != 0u ||
           input->actions[CGUI_ACTION_DOWN].repeat != 0u;
    if (up == 0 && down == 0) {
        return 0;
    }
    if (up != 0 && down == 0) {
        *direction = -1;
    } else if (down != 0 && up == 0) {
        *direction = 1;
    } else {
        *direction = 0;
    }
    return 1;
}

static CguiResult ui_move_global(HitlCockpitUi *ui, int direction)
{
    size_t count;

    count = hitl_cockpit_case_count(ui->cockpit);
    if (direction < 0) {
        ui->selected_index = ui->selected_index == 0u
                                 ? count - 1u
                                 : ui->selected_index - 1u;
    } else if (direction > 0) {
        ui->selected_index = (ui->selected_index + 1u) % count;
    }
    ui_notice_clear(ui);
    return ui_build_list(ui);
}

static int ui_case_id_to_index(const HitlCockpitUi *ui,
                               CguiId id,
                               size_t *case_index)
{
    CguiId encoded;
    size_t decoded;

    if (id <= HITL_COCKPIT_UI_CASE_BASE) {
        return 0;
    }
    encoded = id - HITL_COCKPIT_UI_CASE_BASE;
    decoded = (size_t)(encoded - UINT32_C(1));
    if (decoded >= hitl_cockpit_case_count(ui->cockpit)) {
        return 0;
    }
    *case_index = decoded;
    return 1;
}

static int ui_action_status(CguiId id, HitlStatus *status)
{
    if (id == HITL_COCKPIT_UI_ACT_PASS) {
        *status = HITL_STATUS_PASS;
        return 1;
    }
    if (id == HITL_COCKPIT_UI_ACT_FAIL) {
        *status = HITL_STATUS_FAIL;
        return 1;
    }
    if (id == HITL_COCKPIT_UI_ACT_BLOCKED) {
        *status = HITL_STATUS_BLOCKED;
        return 1;
    }
    return 0;
}

static CguiResult ui_begin_manual(HitlCockpitUi *ui,
                                  HitlStatus status,
                                  const HitlInputProof *proof)
{
    HitlCockpitResult cockpit_result;
    CguiResult cgui_result;

    if (!hitl_cockpit_manual_eligible(ui->cockpit,
                                     ui->selected_index,
                                     status)) {
        cockpit_result = ui->cockpit->session_authority ==
                                 HITL_INPUT_LIVE_DOS
                             ? HITL_COCKPIT_ERR_IDENTITY
                             : HITL_COCKPIT_ERR_AUTHORITY;
        ui->last_cockpit_result = cockpit_result;
        ui_notice_result(ui, "MANUAL RESULT DENIED", cockpit_result);
        return CGUI_OK;
    }

    cockpit_result = hitl_cockpit_manual_begin(ui->cockpit,
                                                ui->selected_index,
                                                status,
                                                proof);
    ui->last_cockpit_result = cockpit_result;
    if (cockpit_result != HITL_COCKPIT_OK) {
        ui_notice_result(ui, "MANUAL RESULT DENIED", cockpit_result);
        return CGUI_OK;
    }

    cgui_result = ui_open_manual_confirm(ui, status);
    if (cgui_result != CGUI_OK) {
        hitl_cockpit_manual_cancel(ui->cockpit);
        return cgui_result;
    }
    ui_notice_clear(ui);
    return CGUI_OK;
}

static CguiResult ui_update_list(HitlCockpitUi *ui,
                                 const HitlCockpitUiInput *input)
{
    CguiEvent cgui_event;
    CguiResult result;
    int direction;

    direction = 0;
    if (ui_list_navigation(&input->gui, &direction)) {
        return ui_move_global(ui, direction);
    }

    result = cgui_update(ui->cgui, &input->gui, &cgui_event);
    if (result != CGUI_OK) {
        return result;
    }
    if (cgui_event.type == CGUI_EVENT_MENU_CANCEL &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_LIST) {
        return ui_open_exit_confirm(ui);
    }
    if (cgui_event.type == CGUI_EVENT_HELP_REQUEST &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_LIST) {
        return ui_open_help(ui);
    }
    if (cgui_event.type == CGUI_EVENT_MENU_ACTIVATE &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_LIST) {
        size_t case_index;

        if (!ui_case_id_to_index(ui, cgui_event.item_id, &case_index)) {
            return CGUI_ERR_STATE;
        }
        ui->selected_index = case_index;
        ui_notice_clear(ui);
        return ui_enter_detail(ui);
    }
    return CGUI_OK;
}

static CguiResult ui_update_detail(HitlCockpitUi *ui,
                                   const HitlCockpitUiInput *input)
{
    CguiEvent cgui_event;
    CguiResult result;

    result = cgui_update(ui->cgui, &input->gui, &cgui_event);
    if (result != CGUI_OK) {
        return result;
    }
    if (cgui_event.type == CGUI_EVENT_MENU_CANCEL &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_DETAIL) {
        ui_notice_clear(ui);
        return ui_enter_list(ui);
    }
    if (cgui_event.type == CGUI_EVENT_HELP_REQUEST &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_DETAIL) {
        return ui_open_help(ui);
    }
    if (cgui_event.type == CGUI_EVENT_MENU_ACTIVATE &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_DETAIL) {
        HitlStatus status;

        if (cgui_event.item_id == HITL_COCKPIT_UI_ACT_BACK) {
            ui_notice_clear(ui);
            return ui_enter_list(ui);
        }
        if (ui_action_status(cgui_event.item_id, &status)) {
            return ui_begin_manual(ui, status, &input->proof);
        }
    }
    return CGUI_OK;
}

static CguiResult ui_update_help(HitlCockpitUi *ui,
                                 const HitlCockpitUiInput *input)
{
    CguiEvent cgui_event;
    CguiResult result;

    result = cgui_update(ui->cgui, &input->gui, &cgui_event);
    if (result != CGUI_OK) {
        return result;
    }
    if (cgui_event.type == CGUI_EVENT_HELP_CLOSE &&
        cgui_event.owner_id == HITL_COCKPIT_UI_OWNER_HELP) {
        ui->mode = ui->return_mode;
    }
    return CGUI_OK;
}

static CguiResult ui_update_manual_confirm(HitlCockpitUi *ui,
                                           const HitlCockpitUiInput *input)
{
    CguiEvent cgui_event;
    CguiResult result;

    result = cgui_update(ui->cgui, &input->gui, &cgui_event);
    if (result != CGUI_OK) {
        return result;
    }
    if (cgui_event.owner_id != HITL_COCKPIT_UI_OWNER_CONFIRM_MAN) {
        return CGUI_OK;
    }
    if (cgui_event.type == CGUI_EVENT_CONFIRM_NO ||
        cgui_event.type == CGUI_EVENT_CONFIRM_CANCEL) {
        hitl_cockpit_manual_cancel(ui->cockpit);
        ui_notice_clear(ui);
        ui->mode = HITL_COCKPIT_UI_DETAIL;
        return CGUI_OK;
    }
    if (cgui_event.type == CGUI_EVENT_CONFIRM_YES) {
        HitlCockpitResult cockpit_result;

        cockpit_result = hitl_cockpit_manual_confirm(ui->cockpit,
                                                      &input->proof,
                                                      ui->journal);
        ui->last_cockpit_result = cockpit_result;
        ui->mode = HITL_COCKPIT_UI_DETAIL;
        if (cockpit_result == HITL_COCKPIT_OK) {
            const HitlCaseState *state;

            state = hitl_cockpit_case_state(ui->cockpit,
                                            ui->selected_index);
            (void)snprintf(ui->notice, sizeof(ui->notice),
                           "MANUAL %s RECORDED",
                           state != NULL
                               ? hitl_status_string(state->manual_status)
                               : "RESULT");
        } else {
            ui_notice_result(ui, "MANUAL CONFIRMATION DENIED",
                             cockpit_result);
        }
    }
    return CGUI_OK;
}

static CguiResult ui_update_exit_confirm(HitlCockpitUi *ui,
                                         const HitlCockpitUiInput *input,
                                         HitlCockpitUiEvent *event)
{
    CguiEvent cgui_event;
    CguiResult result;

    result = cgui_update(ui->cgui, &input->gui, &cgui_event);
    if (result != CGUI_OK) {
        return result;
    }
    if (cgui_event.owner_id != HITL_COCKPIT_UI_OWNER_CONFIRM_EXIT) {
        return CGUI_OK;
    }
    if (cgui_event.type == CGUI_EVENT_CONFIRM_NO ||
        cgui_event.type == CGUI_EVENT_CONFIRM_CANCEL) {
        ui->mode = HITL_COCKPIT_UI_LIST;
    } else if (cgui_event.type == CGUI_EVENT_CONFIRM_YES) {
        ui->mode = HITL_COCKPIT_UI_COMMITTING;
        event->type = HITL_COCKPIT_UI_EVENT_COMMIT_REQUEST;
    }
    return CGUI_OK;
}

static CguiResult ui_update_error(HitlCockpitUi *ui,
                                  const HitlCockpitUiInput *input)
{
    if (input->gui.actions[CGUI_ACTION_F1].pressed != 0u) {
        return ui_open_help(ui);
    }
    if (input->gui.actions[CGUI_ACTION_ESCAPE].pressed != 0u ||
        input->gui.actions[CGUI_ACTION_ENTER].pressed != 0u ||
        input->gui.actions[CGUI_ACTION_SPACE].pressed != 0u) {
        ui_notice_clear(ui);
        return ui_enter_list(ui);
    }
    return CGUI_OK;
}

CguiResult hitl_cockpit_ui_init(HitlCockpitUi *ui,
                                CguiContext *cgui,
                                HitlCockpit *cockpit,
                                HitlJournal *journal)
{
    CguiResult result;

    if (ui == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    memset(ui, 0, sizeof(*ui));
    if (cgui == NULL || cockpit == NULL || journal == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (cockpit->initialized == 0u || cockpit->plan == NULL ||
        cockpit->auto_evidence == NULL ||
        hitl_cockpit_case_count(cockpit) == 0u ||
        hitl_cockpit_case_count(cockpit) > HITL_MAX_CASES ||
        cgui_modal_kind(cgui) != CGUI_MODAL_NONE) {
        return CGUI_ERR_STATE;
    }

    ui->cgui = cgui;
    ui->cockpit = cockpit;
    ui->journal = journal;
    ui->selected_index = 0u;
    ui->page_start = 0u;
    ui->last_cockpit_result = HITL_COCKPIT_OK;
    ui->mode = HITL_COCKPIT_UI_LIST;
    ui->return_mode = HITL_COCKPIT_UI_LIST;
    ui->initialized = 1u;
    result = ui_build_list(ui);
    if (result != CGUI_OK) {
        memset(ui, 0, sizeof(*ui));
    }
    return result;
}

void hitl_cockpit_ui_reset(HitlCockpitUi *ui)
{
    if (ui == NULL) {
        return;
    }
    if (ui->initialized != 0u && ui->cgui != NULL) {
        if (cgui_help_is_open(ui->cgui)) {
            (void)cgui_help_close(ui->cgui);
        } else if (cgui_confirm_is_open(ui->cgui)) {
            (void)cgui_confirm_close(ui->cgui);
        }
        cgui_menu_clear(ui->cgui);
    }
    if (ui->cockpit != NULL) {
        hitl_cockpit_manual_cancel(ui->cockpit);
    }
    memset(ui, 0, sizeof(*ui));
}

CguiResult hitl_cockpit_ui_update(HitlCockpitUi *ui,
                                  const HitlCockpitUiInput *input,
                                  HitlCockpitUiEvent *event)
{
    ui_event_clear(event);
    if (ui == NULL || input == NULL || event == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!ui_is_valid(ui)) {
        return CGUI_ERR_STATE;
    }

    switch (ui->mode) {
    case HITL_COCKPIT_UI_LIST:
        return ui_update_list(ui, input);
    case HITL_COCKPIT_UI_DETAIL:
        return ui_update_detail(ui, input);
    case HITL_COCKPIT_UI_HELP:
        return ui_update_help(ui, input);
    case HITL_COCKPIT_UI_CONFIRM_MANUAL:
        return ui_update_manual_confirm(ui, input);
    case HITL_COCKPIT_UI_CONFIRM_EXIT:
        return ui_update_exit_confirm(ui, input, event);
    case HITL_COCKPIT_UI_ERROR:
        return ui_update_error(ui, input);
    case HITL_COCKPIT_UI_COMMITTING:
    case HITL_COCKPIT_UI_DONE:
        return CGUI_OK;
    default:
        return CGUI_ERR_STATE;
    }
}

static CguiResult ui_draw_line(HitlCockpitUi *ui,
                               int32_t x,
                               int32_t y,
                               const char *text,
                               CguiPaletteRole role)
{
    return cgui_draw_text(ui->cgui, x, y, ui_text(text), role);
}

static CguiResult ui_draw_header(HitlCockpitUi *ui)
{
    const HitlPlan *plan;
    CguiRect screen;
    CguiResult result;
    char line[HITL_RECORD_CONTENT_MAX + HITL_SHA256_HEX_LENGTH + 32u];

    plan = ui->cockpit->plan;
    screen = ui_rect(0, 0,
                     (int32_t)ui->cgui->surface.width,
                     (int32_t)ui->cgui->surface.height);
    result = cgui_fill_rect(ui->cgui, screen, CGUI_ROLE_DESKTOP);
    if (result == CGUI_OK) {
        result = cgui_draw_border(ui->cgui, screen,
                                  CGUI_ROLE_BORDER_LIGHT,
                                  CGUI_ROLE_BORDER_DARK);
    }
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 4, 3, "MOON TEST COCKPIT",
                              CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "RUN:%s PROFILE:%s MODE:%s",
                       plan->identity.run,
                       plan->profile.id,
                       ui->cockpit->session_authority == HITL_INPUT_LIVE_DOS
                           ? "LIVE PHYSICAL"
                           : "SYNTHETIC READ ONLY");
        result = ui_draw_line(ui, 4, 10, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "BUILD:%s",
                       plan->identity.build_hash);
        result = ui_draw_line(ui, 4, 17, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "PLAN :%s",
                       plan->identity.plan_hash);
        result = ui_draw_line(ui, 4, 24, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "PROF :%s",
                       plan->identity.profile_hash);
        result = ui_draw_line(ui, 4, 31, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 4, 38, ui_banner(ui),
                              CGUI_ROLE_WARNING);
    }
    return result;
}

static CguiRect ui_menu_bounds(const HitlCockpitUi *ui,
                               int32_t y0,
                               int32_t y1)
{
    int32_t width;
    int32_t height;

    width = (int32_t)ui->cgui->surface.width;
    height = (int32_t)ui->cgui->surface.height;
    if (width < 6) {
        return ui_rect(0, y0, width, y1 < height ? y1 : height);
    }
    if (y1 > height) {
        y1 = height;
    }
    if (y0 > y1) {
        y0 = y1;
    }
    return ui_rect(3, y0, width - 3, y1);
}

static CguiResult ui_draw_wrapped(HitlCockpitUi *ui,
                                  int32_t x,
                                  int32_t *y,
                                  int32_t x1,
                                  const char *text,
                                  size_t max_lines,
                                  CguiPaletteRole role)
{
    const char *cursor;
    size_t remaining;
    size_t columns;
    size_t line_index;

    if (text == NULL || y == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    columns = x1 > x && ui->cgui->font.advance_x != 0u
                  ? (size_t)(x1 - x) / (size_t)ui->cgui->font.advance_x
                  : 1u;
    if (columns == 0u) {
        columns = 1u;
    }
    cursor = text;
    remaining = strlen(text);
    for (line_index = 0u;
         line_index < max_lines && remaining != 0u;
         ++line_index) {
        size_t take;
        size_t split;
        CguiText span;
        CguiResult result;

        take = remaining < columns ? remaining : columns;
        split = take;
        if (take < remaining) {
            while (split > 0u && cursor[split] != ' ') {
                --split;
            }
            if (split == 0u) {
                split = take;
            }
        }
        span.data = cursor;
        span.length = split;
        result = cgui_draw_text(ui->cgui, x, *y, span, role);
        if (result != CGUI_OK) {
            return result;
        }
        cursor += split;
        remaining -= split;
        while (remaining != 0u && *cursor == ' ') {
            ++cursor;
            --remaining;
        }
        *y += (int32_t)ui->cgui->font.line_advance;
    }
    return CGUI_OK;
}

static CguiResult ui_draw_list(HitlCockpitUi *ui)
{
    const HitlCase *test_case;
    const HitlCaseState *state;
    HitlCombinedResult combined;
    CguiResult result;
    size_t count;
    size_t visible;
    char line[HITL_RECORD_CONTENT_MAX + 96u];

    result = ui_draw_header(ui);
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 7, 46,
                              "CASE                     AUTO    MANUAL  RESULT",
                              CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_menu(ui->cgui,
                                ui_menu_bounds(ui, 52, 152));
    }

    count = hitl_cockpit_case_count(ui->cockpit);
    visible = count - ui->page_start;
    if (visible > (size_t)HITL_COCKPIT_UI_VISIBLE_CASES) {
        visible = (size_t)HITL_COCKPIT_UI_VISIBLE_CASES;
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line),
                       "CASES %lu-%lu/%lu  PAGE %lu/%lu",
                       (unsigned long)(ui->page_start + 1u),
                       (unsigned long)(ui->page_start + visible),
                       (unsigned long)count,
                       (unsigned long)(ui->page_start /
                                           HITL_COCKPIT_UI_VISIBLE_CASES + 1u),
                       (unsigned long)ui_page_count_for(count));
        result = ui_draw_line(ui, 4, 155, line, CGUI_ROLE_TEXT);
    }

    test_case = &ui->cockpit->plan->cases[ui->selected_index];
    state = hitl_cockpit_case_state(ui->cockpit, ui->selected_index);
    combined = hitl_cockpit_combined(ui->cockpit, ui->selected_index);
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "%s - %s",
                       test_case->id, test_case->title);
        result = ui_draw_line(ui, 4, 162, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK && state != NULL) {
        (void)snprintf(line, sizeof(line),
                       "REQUIRED A:%u M:%u  AUTO:%s MANUAL:%s",
                       test_case->auto_required,
                       test_case->manual_required,
                       hitl_status_string(state->auto_status),
                       hitl_status_string(state->manual_status));
        result = ui_draw_line(ui, 4, 169, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "COMBINED:%s / %s",
                       hitl_status_string(combined.status),
                       combined.code != NULL ? combined.code : "INVALID");
        result = ui_draw_line(ui, 4, 176, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 4, 190,
                              "UP/DOWN SELECT  ENTER DETAILS  F1 HELP  ESC COMMIT",
                              CGUI_ROLE_TEXT);
    }
    return result;
}

static CguiResult ui_draw_detail(HitlCockpitUi *ui)
{
    const HitlPlan *plan;
    const HitlCase *test_case;
    const HitlCaseState *state;
    const HitlAutoRecord *automatic;
    HitlCombinedResult combined;
    CguiResult result;
    int32_t y;
    char line[HITL_RECORD_CONTENT_MAX + 96u];

    plan = ui->cockpit->plan;
    test_case = &plan->cases[ui->selected_index];
    state = hitl_cockpit_case_state(ui->cockpit, ui->selected_index);
    automatic = &ui->cockpit->auto_evidence->records[ui->selected_index];
    combined = hitl_cockpit_combined(ui->cockpit, ui->selected_index);

    result = ui_draw_header(ui);
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "CASE %lu/%lu: %s",
                       (unsigned long)(ui->selected_index + 1u),
                       (unsigned long)plan->case_count,
                       test_case->id);
        result = ui_draw_line(ui, 4, 46, line, CGUI_ROLE_WARNING);
    }
    y = 53;
    if (result == CGUI_OK) {
        result = ui_draw_wrapped(ui, 4, &y,
                                 (int32_t)ui->cgui->surface.width - 4,
                                 test_case->title, 3u, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "REQUIRED AUTO:%u MANUAL:%u",
                       test_case->auto_required,
                       test_case->manual_required);
        result = ui_draw_line(ui, 4, 72, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = cgui_draw_menu(ui->cgui,
                                ui_menu_bounds(ui, 80, 116));
    }
    if (result == CGUI_OK && state != NULL) {
        (void)snprintf(line, sizeof(line), "AUTO:%s CURRENT:%s",
                       hitl_status_string(state->auto_status),
                       state->auto_evidence_current ? "YES" : "NO");
        result = ui_draw_line(ui, 4, 121, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK && state != NULL) {
        (void)snprintf(line, sizeof(line), "MANUAL:%s CURRENT:%s",
                       hitl_status_string(state->manual_status),
                       state->manual_evidence_current ? "YES" : "NO");
        result = ui_draw_line(ui, 4, 128, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line), "RESULT:%s / %s",
                       hitl_status_string(combined.status),
                       combined.code != NULL ? combined.code : "INVALID");
        result = ui_draw_line(ui, 4, 135, line, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        (void)snprintf(line, sizeof(line),
                       "AUTO CODE:%s  ELAPSED:%lu MS",
                       automatic->result_code,
                       (unsigned long)automatic->elapsed_ms);
        result = ui_draw_line(ui, 4, 142, line, CGUI_ROLE_TEXT);
    }
    y = 149;
    if (result == CGUI_OK) {
        result = ui_draw_wrapped(ui, 4, &y,
                                 (int32_t)ui->cgui->surface.width - 4,
                                 automatic->detail, 3u, CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK && ui->notice[0] != '\0') {
        result = ui_draw_line(ui, 4, 174, ui->notice,
                              CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 4, 190,
                              "UP/DOWN SELECT  ENTER CHOOSE  F1 HELP  ESC BACK",
                              CGUI_ROLE_TEXT);
    }
    return result;
}

static CguiResult ui_draw_status_screen(HitlCockpitUi *ui,
                                        const char *title,
                                        const char *instruction)
{
    CguiResult result;

    result = ui_draw_header(ui);
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 20, 80, title, CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK && ui->notice[0] != '\0') {
        result = ui_draw_line(ui, 20, 94, ui->notice,
                              CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK) {
        result = ui_draw_line(ui, 20, 108, instruction,
                              CGUI_ROLE_TEXT);
    }
    return result;
}

static CguiRect ui_modal_bounds(const HitlCockpitUi *ui)
{
    int32_t width;
    int32_t height;

    width = (int32_t)ui->cgui->surface.width;
    height = (int32_t)ui->cgui->surface.height;
    if (width <= 24 || height <= 32) {
        return ui_rect(0, 0, width, height);
    }
    return ui_rect(12, 16, width - 12, height - 16);
}

CguiResult hitl_cockpit_ui_draw(HitlCockpitUi *ui)
{
    CguiResult result;

    if (!ui_is_valid(ui)) {
        return ui == NULL ? CGUI_ERR_ARGUMENT : CGUI_ERR_STATE;
    }
    switch (ui->mode) {
    case HITL_COCKPIT_UI_LIST:
        return ui_draw_list(ui);
    case HITL_COCKPIT_UI_DETAIL:
        return ui_draw_detail(ui);
    case HITL_COCKPIT_UI_HELP:
        if (ui->return_mode == HITL_COCKPIT_UI_DETAIL) {
            result = ui_draw_detail(ui);
        } else if (ui->return_mode == HITL_COCKPIT_UI_ERROR) {
            result = ui_draw_status_screen(
                ui, "COMMIT FAILED", "F1 CLOSES HELP; ENTER RETURNS TO CASES");
        } else {
            result = ui_draw_list(ui);
        }
        if (result == CGUI_OK) {
            result = cgui_draw_help(ui->cgui, ui_modal_bounds(ui));
        }
        return result;
    case HITL_COCKPIT_UI_CONFIRM_MANUAL:
        result = ui_draw_detail(ui);
        if (result == CGUI_OK) {
            result = cgui_draw_confirm(ui->cgui, ui_modal_bounds(ui));
        }
        return result;
    case HITL_COCKPIT_UI_CONFIRM_EXIT:
        result = ui_draw_list(ui);
        if (result == CGUI_OK) {
            result = cgui_draw_confirm(ui->cgui, ui_modal_bounds(ui));
        }
        return result;
    case HITL_COCKPIT_UI_COMMITTING:
        return ui_draw_status_screen(
            ui, "COMMITTING TEST EVIDENCE", "WAIT FOR TRANSACTIONAL WRITER");
    case HITL_COCKPIT_UI_ERROR:
        return ui_draw_status_screen(
            ui, "COMMIT FAILED", "PRESS ENTER OR ESC TO RETURN TO CASES");
    case HITL_COCKPIT_UI_DONE:
        return ui_draw_status_screen(
            ui, "TEST EVIDENCE COMMITTED", "HITL.OUT IS READY FOR THE HOST");
    default:
        return CGUI_ERR_STATE;
    }
}

CguiResult hitl_cockpit_ui_commit_finished(
    HitlCockpitUi *ui,
    HitlCockpitResult result)
{
    if (!ui_is_valid(ui)) {
        return ui == NULL ? CGUI_ERR_ARGUMENT : CGUI_ERR_STATE;
    }
    if (ui->mode != HITL_COCKPIT_UI_COMMITTING) {
        return CGUI_ERR_STATE;
    }
    ui->last_cockpit_result = result;
    if (result == HITL_COCKPIT_OK) {
        ui_notice_clear(ui);
        ui->mode = HITL_COCKPIT_UI_DONE;
    } else {
        ui_notice_result(ui, "COMMIT FAILED", result);
        ui->mode = HITL_COCKPIT_UI_ERROR;
    }
    return CGUI_OK;
}

HitlCockpitUiMode hitl_cockpit_ui_mode(const HitlCockpitUi *ui)
{
    return ui_is_valid(ui) ? ui->mode : HITL_COCKPIT_UI_ERROR;
}

size_t hitl_cockpit_ui_selected_index(const HitlCockpitUi *ui)
{
    return ui_is_valid(ui) ? ui->selected_index : 0u;
}

size_t hitl_cockpit_ui_page_start(const HitlCockpitUi *ui)
{
    return ui_is_valid(ui) ? ui->page_start : 0u;
}

size_t hitl_cockpit_ui_page_count(const HitlCockpitUi *ui)
{
    return ui_is_valid(ui)
               ? ui_page_count_for(hitl_cockpit_case_count(ui->cockpit))
               : 0u;
}

HitlCockpitResult hitl_cockpit_ui_last_result(const HitlCockpitUi *ui)
{
    return ui_is_valid(ui) ? ui->last_cockpit_result
                           : HITL_COCKPIT_ERR_STATE;
}
