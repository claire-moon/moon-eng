#ifndef MOON_HITL_COCKPIT_H
#define MOON_HITL_COCKPIT_H

/*
 * Portable authority, journal, and summary core for MOON TEST COCKPIT.
 *
 * CGUI events are deliberately not evidence authority.  A MANUAL result can
 * be published only after two distinct fresh edges from a session permanently
 * initialized as LIVE_DOS, with an append-only journal write between the
 * second edge and the in-memory state change.
 */

#include "moon/hitl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum HitlCockpitResult {
    HITL_COCKPIT_OK = 0,
    HITL_COCKPIT_ERR_ARGUMENT,
    HITL_COCKPIT_ERR_CONFIG,
    HITL_COCKPIT_ERR_STATE,
    HITL_COCKPIT_ERR_IDENTITY,
    HITL_COCKPIT_ERR_AUTHORITY,
    HITL_COCKPIT_ERR_CONFIRMATION,
    HITL_COCKPIT_ERR_IO,
    HITL_COCKPIT_ERR_FORMAT,
    HITL_COCKPIT_ERR_LIMIT,
    HITL_COCKPIT_ERR_CRC,
    HITL_COCKPIT_ERR_RECOVERY
} HitlCockpitResult;

typedef enum HitlInputAuthority {
    /* Zero-initialized and every smoke/replay session are read-only. */
    HITL_INPUT_SYNTHETIC = 0,
    HITL_INPUT_LIVE_DOS
} HitlInputAuthority;

typedef struct HitlInputProof {
    HitlInputAuthority authority;
    uint8_t fresh_pressed;
    uint32_t edge_serial;
    uint32_t tick;
} HitlInputProof;

typedef enum HitlJournalEventType {
    HITL_JOURNAL_OPEN = 0,
    HITL_JOURNAL_STEP,
    HITL_JOURNAL_CAPTURE,
    HITL_JOURNAL_MANUAL,
    HITL_JOURNAL_NOTE,
    HITL_JOURNAL_WARNING,
    HITL_JOURNAL_COMMIT,
    HITL_JOURNAL_COMMIT_FAIL
} HitlJournalEventType;

/*
 * The stream is borrowed, must be opened in binary read/write mode, and must
 * outlive this object.  Internal fields are public only for caller-owned DOS
 * storage; applications must use the accessors below.
 */
typedef struct HitlJournal {
    FILE *stream;
    const HitlPlan *plan;
    HitlIdentity identity;
    uint32_t crc_state;
    uint32_t event_count;
    uint32_t next_sequence;
    uint32_t last_tick;
    uint8_t initialized;
} HitlJournal;

typedef struct HitlManualPending {
    HitlIdentity identity;
    size_t case_index;
    HitlStatus status;
    uint32_t first_edge_serial;
    uint32_t first_tick;
    uint8_t active;
} HitlManualPending;

/* Plan and AUTO evidence remain borrowed and must outlive the cockpit. */
typedef struct HitlCockpit {
    const HitlPlan *plan;
    const HitlAutoEvidence *auto_evidence;
    HitlCaseState case_states[HITL_MAX_CASES];
    HitlManualPending pending;
    size_t selected_index;
    HitlInputAuthority session_authority;
    uint8_t initialized;
} HitlCockpit;

typedef enum HitlPathResult {
    HITL_PATH_OK = 0,
    HITL_PATH_NOT_FOUND,
    HITL_PATH_ERROR
} HitlPathResult;

typedef HitlPathResult (*HitlRemovePathFn)(void *user,
                                           const char *path);
typedef HitlPathResult (*HitlRenamePathFn)(void *user,
                                           const char *old_path,
                                           const char *new_path);

typedef struct HitlPathOps {
    HitlRemovePathFn remove_path;
    HitlRenamePathFn rename_path;
    void *user;
} HitlPathOps;

typedef struct HitlReplaceOutcome {
    uint8_t output_committed;
    uint8_t prior_output_moved;
    uint8_t prior_output_recoverable;
    uint8_t old_file_retained;
} HitlReplaceOutcome;

const char *hitl_cockpit_result_string(HitlCockpitResult result);

HitlCockpitResult hitl_cockpit_init(
    HitlCockpit *cockpit,
    const HitlPlan *plan,
    const HitlAutoEvidence *auto_evidence,
    HitlInputAuthority session_authority);
void hitl_cockpit_reset(HitlCockpit *cockpit);

size_t hitl_cockpit_case_count(const HitlCockpit *cockpit);
const HitlCaseState *hitl_cockpit_case_state(const HitlCockpit *cockpit,
                                             size_t case_index);
HitlCombinedResult hitl_cockpit_combined(const HitlCockpit *cockpit,
                                         size_t case_index);
int hitl_cockpit_auto_identity_current(const HitlCockpit *cockpit);
int hitl_cockpit_manual_eligible(const HitlCockpit *cockpit,
                                 size_t case_index,
                                 HitlStatus status);

/* First edge: arm a proposed PASS, FAIL, or BLOCKED result. */
HitlCockpitResult hitl_cockpit_manual_begin(
    HitlCockpit *cockpit,
    size_t case_index,
    HitlStatus status,
    const HitlInputProof *proof);
void hitl_cockpit_manual_cancel(HitlCockpit *cockpit);
int hitl_cockpit_manual_pending(const HitlCockpit *cockpit,
                                size_t *case_index,
                                HitlStatus *status);

/*
 * Second edge: revalidate identity, append MANUAL, then publish state.
 * Every attempted confirmation consumes the pending token.  A failed append
 * therefore leaves MANUAL unchanged and requires a new two-edge attempt.
 */
HitlCockpitResult hitl_cockpit_manual_confirm(
    HitlCockpit *cockpit,
    const HitlInputProof *proof,
    HitlJournal *journal);

/*
 * Attach to an empty or existing journal. Existing events are validated and
 * retained only as history; they never restore current-process MANUAL state.
 */
HitlCockpitResult hitl_journal_attach(HitlJournal *journal,
                                      FILE *stream,
                                      const HitlPlan *plan,
                                      unsigned long *error_line);
void hitl_journal_detach(HitlJournal *journal);
HitlCockpitResult hitl_journal_append(
    HitlJournal *journal,
    HitlJournalEventType type,
    uint32_t tick,
    const char *case_id,
    const char *payload,
    HitlInputAuthority authority);
HitlCockpitResult hitl_journal_flush(HitlJournal *journal);
uint32_t hitl_journal_crc32(const HitlJournal *journal);
uint32_t hitl_journal_event_count(const HitlJournal *journal);

/*
 * CRC-32/ISO-HDLC covers every raw canonical journal byte, including header,
 * identity preamble, EVENT records, and every CRLF. record_count is the EVENT
 * count, not the number of preamble records.
 */
HitlCockpitResult hitl_journal_verify(FILE *stream,
                                      const HitlPlan *plan,
                                      uint32_t *crc32,
                                      uint32_t *record_count,
                                      unsigned long *error_line);

HitlCockpitResult hitl_summary_write(FILE *stream,
                                     const HitlCockpit *cockpit,
                                     const HitlJournal *journal);
HitlCockpitResult hitl_summary_verify(FILE *stream,
                                      const HitlPlan *plan,
                                      const HitlAutoEvidence *auto_evidence,
                                      FILE *journal_stream,
                                      unsigned long *error_line);

/*
 * Replace OUT through NEW/OLD.  NULL ops use stdio remove/rename.  Failures
 * before OUT->OLD leave OUT untouched; failure of NEW->OUT leaves OLD as the
 * recoverable prior output.  The caller writes and verifies NEW first.
 */
HitlCockpitResult hitl_summary_replace(
    const char *new_path,
    const char *out_path,
    const char *old_path,
    const HitlPathOps *ops,
    HitlReplaceOutcome *outcome);

#ifdef __cplusplus
}
#endif

#endif
