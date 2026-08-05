#ifndef MOON_HITL_COCKPIT_UI_H
#define MOON_HITL_COCKPIT_UI_H

/*
 * Portable CGUI presentation/controller bridge for MOON TEST COCKPIT.
 *
 * The UI owns no files and grants no evidence authority.  The caller supplies
 * a controller, attached journal, CGUI context, normalized actions, and the
 * proof associated with the winning action edge.  A confirmed exit emits a
 * commit request which the platform application must service transactionally.
 */

#include "moon/cgui.h"
#include "moon/hitl_cockpit.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HITL_COCKPIT_UI_VISIBLE_CASES 12u
#define HITL_COCKPIT_UI_ROW_TEXT_CAPACITY 96u
#define HITL_COCKPIT_UI_NOTICE_CAPACITY 192u
#define HITL_COCKPIT_UI_MODAL_TEXT_CAPACITY 320u

/* Frozen, non-overlapping stable-ID namespaces. */
#define HITL_COCKPIT_UI_OWNER_LIST          UINT32_C(0x48490001)
#define HITL_COCKPIT_UI_OWNER_DETAIL        UINT32_C(0x48490002)
#define HITL_COCKPIT_UI_OWNER_HELP          UINT32_C(0x48490003)
#define HITL_COCKPIT_UI_OWNER_CONFIRM_MAN   UINT32_C(0x48490004)
#define HITL_COCKPIT_UI_OWNER_CONFIRM_EXIT  UINT32_C(0x48490005)
#define HITL_COCKPIT_UI_CASE_BASE           UINT32_C(0x48491000)
#define HITL_COCKPIT_UI_ACT_PASS            UINT32_C(0x48492001)
#define HITL_COCKPIT_UI_ACT_FAIL            UINT32_C(0x48492002)
#define HITL_COCKPIT_UI_ACT_BLOCKED         UINT32_C(0x48492003)
#define HITL_COCKPIT_UI_ACT_CLEAR           UINT32_C(0x48492004)
#define HITL_COCKPIT_UI_ACT_CAPTURE         UINT32_C(0x48492005)
#define HITL_COCKPIT_UI_ACT_BACK            UINT32_C(0x48492006)
#define HITL_COCKPIT_UI_MAN_YES             UINT32_C(0x48493001)
#define HITL_COCKPIT_UI_MAN_NO              UINT32_C(0x48493002)
#define HITL_COCKPIT_UI_EXIT_YES            UINT32_C(0x48494001)
#define HITL_COCKPIT_UI_EXIT_NO             UINT32_C(0x48494002)

typedef enum HitlCockpitUiMode {
    HITL_COCKPIT_UI_LIST = 0,
    HITL_COCKPIT_UI_DETAIL,
    HITL_COCKPIT_UI_HELP,
    HITL_COCKPIT_UI_CONFIRM_MANUAL,
    HITL_COCKPIT_UI_CONFIRM_EXIT,
    HITL_COCKPIT_UI_COMMITTING,
    HITL_COCKPIT_UI_ERROR,
    HITL_COCKPIT_UI_DONE
} HitlCockpitUiMode;

typedef enum HitlCockpitUiEventType {
    HITL_COCKPIT_UI_EVENT_NONE = 0,
    HITL_COCKPIT_UI_EVENT_COMMIT_REQUEST
} HitlCockpitUiEventType;

typedef struct HitlCockpitUiEvent {
    HitlCockpitUiEventType type;
} HitlCockpitUiEvent;

/* proof describes the action edge which wins CGUI's documented priority. */
typedef struct HitlCockpitUiInput {
    CguiInputFrame gui;
    HitlInputProof proof;
} HitlCockpitUiInput;

/* Public only to preserve caller-owned, allocation-free DOS storage. */
typedef struct HitlCockpitUi {
    CguiContext *cgui;
    HitlCockpit *cockpit;
    HitlJournal *journal;
    CguiMenuItem menu_items[HITL_COCKPIT_UI_VISIBLE_CASES];
    char row_text[HITL_COCKPIT_UI_VISIBLE_CASES]
                 [HITL_COCKPIT_UI_ROW_TEXT_CAPACITY];
    char notice[HITL_COCKPIT_UI_NOTICE_CAPACITY];
    char modal_title[HITL_COCKPIT_UI_MODAL_TEXT_CAPACITY];
    char modal_message[HITL_COCKPIT_UI_MODAL_TEXT_CAPACITY];
    size_t selected_index;
    size_t page_start;
    HitlCockpitResult last_cockpit_result;
    HitlCockpitUiMode mode;
    HitlCockpitUiMode return_mode;
    uint8_t initialized;
} HitlCockpitUi;

CguiResult hitl_cockpit_ui_init(HitlCockpitUi *ui,
                                CguiContext *cgui,
                                HitlCockpit *cockpit,
                                HitlJournal *journal);
void hitl_cockpit_ui_reset(HitlCockpitUi *ui);

/* Clears event first and emits at most one platform request per UI tick. */
CguiResult hitl_cockpit_ui_update(HitlCockpitUi *ui,
                                  const HitlCockpitUiInput *input,
                                  HitlCockpitUiEvent *event);
CguiResult hitl_cockpit_ui_draw(HitlCockpitUi *ui);

/* Complete a previously emitted commit request without performing path I/O. */
CguiResult hitl_cockpit_ui_commit_finished(
    HitlCockpitUi *ui,
    HitlCockpitResult result);

HitlCockpitUiMode hitl_cockpit_ui_mode(const HitlCockpitUi *ui);
size_t hitl_cockpit_ui_selected_index(const HitlCockpitUi *ui);
size_t hitl_cockpit_ui_page_start(const HitlCockpitUi *ui);
size_t hitl_cockpit_ui_page_count(const HitlCockpitUi *ui);
HitlCockpitResult hitl_cockpit_ui_last_result(const HitlCockpitUi *ui);

#ifdef __cplusplus
}
#endif

#endif
