#ifndef MOON_HITL_H
#define MOON_HITL_H

/*
 * Portable parser and evaluator for MOON TEST COCKPIT evidence.
 *
 * The core deliberately has no function that assigns a MANUAL status.  An
 * authorized TEST COCKPIT input path may populate HitlCaseState.manual_status
 * and mark it current for the already-bound identity later, but importing
 * AUTO.OUT can only change the AUTO lane.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HITL_FORMAT_MAJOR 1u
#define HITL_FORMAT_MINOR 0u

#define HITL_RECORD_MAX_BYTES 240u
#define HITL_RECORD_CONTENT_MAX 238u
#define HITL_MAX_FIELDS 8u
#define HITL_MAX_CASES 128u

#define HITL_RUN_MAX 8u
#define HITL_SHA256_HEX_LENGTH 64u
#define HITL_CASE_ID_MAX 24u

/* Unspecified field lengths are bounded by the enclosing record limit. */
#define HITL_PROFILE_ID_MAX HITL_RECORD_CONTENT_MAX
#define HITL_PROFILE_TEXT_MAX HITL_RECORD_CONTENT_MAX
#define HITL_CASE_TITLE_MAX HITL_RECORD_CONTENT_MAX
#define HITL_RESULT_CODE_MAX HITL_RECORD_CONTENT_MAX
#define HITL_DETAIL_MAX HITL_RECORD_CONTENT_MAX

typedef enum HitlStatus {
    /* Zero-initialized evidence is deliberately never successful. */
    HITL_STATUS_UNRUN = 0,
    HITL_STATUS_PASS,
    HITL_STATUS_FAIL,
    HITL_STATUS_BLOCKED,
    HITL_STATUS_INVALID
} HitlStatus;

typedef enum HitlResult {
    HITL_OK = 0,
    HITL_END,
    HITL_ERR_ARGUMENT,
    HITL_ERR_IO,
    HITL_ERR_RECORD_TOO_LONG,
    HITL_ERR_FIELD_COUNT,
    HITL_ERR_FORMAT,
    HITL_ERR_VERSION,
    HITL_ERR_UNKNOWN_RECORD,
    HITL_ERR_UNKNOWN_CASE,
    HITL_ERR_DUPLICATE,
    HITL_ERR_MISSING,
    HITL_ERR_LIMIT,
    HITL_ERR_STALE
} HitlResult;

typedef struct HitlRecord {
    char storage[HITL_RECORD_CONTENT_MAX + 1u];
    char *fields[HITL_MAX_FIELDS];
    size_t field_count;
    unsigned long line_number;
} HitlRecord;

typedef struct HitlReader {
    FILE *stream;
    unsigned long line_number;
} HitlReader;

typedef struct HitlIdentity {
    char run[HITL_RUN_MAX + 1u];
    char build_hash[HITL_SHA256_HEX_LENGTH + 1u];
    char plan_hash[HITL_SHA256_HEX_LENGTH + 1u];
    char profile_hash[HITL_SHA256_HEX_LENGTH + 1u];
} HitlIdentity;

typedef struct HitlProfile {
    char id[HITL_PROFILE_ID_MAX + 1u];
    char operating_system[HITL_PROFILE_TEXT_MAX + 1u];
    char cpu[HITL_PROFILE_TEXT_MAX + 1u];
    char memory[HITL_PROFILE_TEXT_MAX + 1u];
    char blaster[HITL_PROFILE_TEXT_MAX + 1u];
} HitlProfile;

typedef struct HitlCase {
    char id[HITL_CASE_ID_MAX + 1u];
    unsigned int auto_required;
    unsigned int manual_required;
    char title[HITL_CASE_TITLE_MAX + 1u];
} HitlCase;

typedef struct HitlPlan {
    unsigned int format_major;
    unsigned int format_minor;
    HitlIdentity identity;
    HitlProfile profile;
    HitlCase cases[HITL_MAX_CASES];
    size_t case_count;
} HitlPlan;

typedef struct HitlAutoRecord {
    char case_id[HITL_CASE_ID_MAX + 1u];
    HitlStatus status;
    char result_code[HITL_RESULT_CODE_MAX + 1u];
    uint32_t elapsed_ms;
    char detail[HITL_DETAIL_MAX + 1u];
} HitlAutoRecord;

typedef struct HitlAutoEvidence {
    unsigned int format_major;
    unsigned int format_minor;
    HitlIdentity identity;
    HitlAutoRecord records[HITL_MAX_CASES];
    size_t record_count;
    int identity_current;
} HitlAutoEvidence;

typedef struct HitlCaseState {
    HitlStatus auto_status;
    HitlStatus manual_status;
    HitlIdentity identity;
    char case_id[HITL_CASE_ID_MAX + 1u];
    int auto_evidence_current;
    int manual_evidence_current;
} HitlCaseState;

typedef struct HitlCombinedResult {
    HitlStatus status;
    const char *code;
} HitlCombinedResult;

const char *hitl_status_string(HitlStatus status);
int hitl_status_from_string(const char *text, HitlStatus *status);
const char *hitl_result_string(HitlResult result);

void hitl_reader_init(HitlReader *reader, FILE *stream);

/*
 * Reads the next non-blank, non-comment record.  LF and CRLF are accepted;
 * the normalized record content may contain at most 238 bytes so a writer can
 * always append CRLF within the 240-byte format limit.
 */
HitlResult hitl_reader_next(HitlReader *reader, HitlRecord *record);

int hitl_identity_equal(const HitlIdentity *left,
                        const HitlIdentity *right);

/*
 * error_line is optional and receives zero on success or the failing line.
 * plan is published only on HITL_OK and is otherwise cleared to all zeroes.
 */
HitlResult hitl_plan_read(FILE *stream,
                          HitlPlan *plan,
                          unsigned long *error_line);

/*
 * AUTO records are stored in the same order as plan->cases, irrespective of
 * their file order.  A fully parsed identity mismatch returns HITL_ERR_STALE;
 * evidence.identity_current remains false and none of its statuses are
 * eligible for a current combined result.  HITL_ERR_STALE deliberately
 * publishes that fully parsed evidence for diagnosis.  Every other error
 * clears evidence to all zeroes.
 */
HitlResult hitl_auto_read(FILE *stream,
                          const HitlPlan *plan,
                          HitlAutoEvidence *evidence,
                          unsigned long *error_line);

void hitl_case_state_init(HitlCaseState *state);

/*
 * Applies only the AUTO lane and never changes manual_status.  The complete
 * evidence set and selected plan index are required so a status cannot be
 * detached from its run identity or applied to a different case.  A MANUAL
 * result is preserved only when the state was already bound to this exact
 * identity and case; rebinding resets MANUAL to UNRUN.  On failure state is
 * unchanged.
 */
HitlResult hitl_case_state_apply_auto(HitlCaseState *state,
                                      const HitlPlan *plan,
                                      const HitlAutoEvidence *evidence,
                                      size_t case_index);

/*
 * The live plan, evidence set, and case index are revalidated on every call.
 * A state without matching current AUTO evidence maps explicitly to
 * BLOCKED / STALE_EVIDENCE.  A non-UNRUN MANUAL status is eligible only when
 * the trusted cockpit has marked it current for the state's bound identity.
 */
HitlCombinedResult hitl_case_combined(const HitlPlan *plan,
                                      const HitlAutoEvidence *evidence,
                                      size_t case_index,
                                      const HitlCaseState *state);

#ifdef __cplusplus
}
#endif

#endif
