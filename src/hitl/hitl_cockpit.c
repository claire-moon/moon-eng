#include "moon/hitl_cockpit.h"

#include <string.h>

static int hitl_cockpit_status_valid(HitlStatus status)
{
    return status == HITL_STATUS_UNRUN || status == HITL_STATUS_PASS ||
           status == HITL_STATUS_FAIL || status == HITL_STATUS_BLOCKED;
}

static int hitl_cockpit_manual_status_valid(HitlStatus status)
{
    return status == HITL_STATUS_PASS || status == HITL_STATUS_FAIL ||
           status == HITL_STATUS_BLOCKED;
}

static int hitl_cockpit_plan_valid(const HitlPlan *plan)
{
    size_t index;
    size_t other;

    if (plan == NULL || plan->format_major != HITL_FORMAT_MAJOR ||
        plan->case_count == 0u || plan->case_count > HITL_MAX_CASES ||
        plan->identity.run[0] == '\0' ||
        plan->identity.build_hash[0] == '\0' ||
        plan->identity.plan_hash[0] == '\0' ||
        plan->identity.profile_hash[0] == '\0') {
        return 0;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        const HitlCase *definition = &plan->cases[index];
        if (definition->id[0] == '\0' ||
            definition->auto_required > 1u ||
            definition->manual_required > 1u ||
            (definition->auto_required == 0u &&
             definition->manual_required == 0u)) {
            return 0;
        }
        for (other = 0u; other < index; ++other) {
            if (strcmp(definition->id, plan->cases[other].id) == 0) {
                return 0;
            }
        }
    }
    return 1;
}

static int hitl_cockpit_auto_shape_valid(const HitlPlan *plan,
                                         const HitlAutoEvidence *evidence)
{
    size_t index;

    if (!hitl_cockpit_plan_valid(plan) || evidence == NULL ||
        evidence->format_major != HITL_FORMAT_MAJOR ||
        evidence->record_count != plan->case_count) {
        return 0;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        if (!hitl_cockpit_status_valid(evidence->records[index].status) ||
            strcmp(plan->cases[index].id,
                   evidence->records[index].case_id) != 0) {
            return 0;
        }
    }
    return 1;
}

static int hitl_cockpit_auto_current(const HitlCockpit *cockpit)
{
    return cockpit != NULL && cockpit->initialized != 0u &&
           hitl_cockpit_auto_shape_valid(cockpit->plan,
                                         cockpit->auto_evidence) &&
           cockpit->auto_evidence->identity_current != 0 &&
           hitl_identity_equal(&cockpit->plan->identity,
                               &cockpit->auto_evidence->identity);
}

static int hitl_cockpit_binding_current(const HitlCockpit *cockpit,
                                        size_t case_index)
{
    const HitlCaseState *state;

    if (cockpit == NULL || cockpit->initialized == 0u ||
        !hitl_cockpit_plan_valid(cockpit->plan) ||
        case_index >= cockpit->plan->case_count) {
        return 0;
    }
    state = &cockpit->case_states[case_index];
    return hitl_identity_equal(&state->identity,
                               &cockpit->plan->identity) &&
           strcmp(state->case_id, cockpit->plan->cases[case_index].id) == 0;
}

static int hitl_cockpit_live_proof(const HitlCockpit *cockpit,
                                   const HitlInputProof *proof)
{
    return cockpit != NULL && proof != NULL &&
           cockpit->session_authority == HITL_INPUT_LIVE_DOS &&
           proof->authority == HITL_INPUT_LIVE_DOS &&
           proof->fresh_pressed != 0u && proof->edge_serial != 0u;
}

const char *hitl_cockpit_result_string(HitlCockpitResult result)
{
    switch (result) {
        case HITL_COCKPIT_OK: return "ok";
        case HITL_COCKPIT_ERR_ARGUMENT: return "invalid argument";
        case HITL_COCKPIT_ERR_CONFIG: return "invalid configuration";
        case HITL_COCKPIT_ERR_STATE: return "invalid cockpit state";
        case HITL_COCKPIT_ERR_IDENTITY: return "stale evidence identity";
        case HITL_COCKPIT_ERR_AUTHORITY: return "manual authority denied";
        case HITL_COCKPIT_ERR_CONFIRMATION: return "confirmation edge denied";
        case HITL_COCKPIT_ERR_IO: return "evidence I/O failure";
        case HITL_COCKPIT_ERR_FORMAT: return "invalid evidence format";
        case HITL_COCKPIT_ERR_LIMIT: return "evidence limit exceeded";
        case HITL_COCKPIT_ERR_CRC: return "evidence CRC mismatch";
        case HITL_COCKPIT_ERR_RECOVERY: return "output recovery required";
        default: return "unknown cockpit result";
    }
}

HitlCockpitResult hitl_cockpit_init(
    HitlCockpit *cockpit,
    const HitlPlan *plan,
    const HitlAutoEvidence *auto_evidence,
    HitlInputAuthority session_authority)
{
    size_t index;
    int auto_shape_valid;

    if (cockpit == NULL || plan == NULL || auto_evidence == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (!hitl_cockpit_plan_valid(plan) ||
        (session_authority != HITL_INPUT_SYNTHETIC &&
         session_authority != HITL_INPUT_LIVE_DOS)) {
        return HITL_COCKPIT_ERR_CONFIG;
    }

    memset(cockpit, 0, sizeof(*cockpit));
    cockpit->plan = plan;
    cockpit->auto_evidence = auto_evidence;
    cockpit->session_authority = session_authority;
    auto_shape_valid = hitl_cockpit_auto_shape_valid(plan, auto_evidence);
    for (index = 0u; index < plan->case_count; ++index) {
        HitlCaseState *state = &cockpit->case_states[index];
        hitl_case_state_init(state);
        state->identity = plan->identity;
        memcpy(state->case_id, plan->cases[index].id,
               strlen(plan->cases[index].id) + 1u);
        if (auto_shape_valid) {
            state->auto_status = auto_evidence->records[index].status;
        }
        if (auto_shape_valid && auto_evidence->identity_current != 0 &&
            hitl_identity_equal(&plan->identity,
                                &auto_evidence->identity)) {
            if (hitl_case_state_apply_auto(state, plan, auto_evidence,
                                           index) != HITL_OK) {
                hitl_cockpit_reset(cockpit);
                return HITL_COCKPIT_ERR_CONFIG;
            }
        }
    }
    cockpit->initialized = 1u;
    return HITL_COCKPIT_OK;
}

void hitl_cockpit_reset(HitlCockpit *cockpit)
{
    if (cockpit != NULL) {
        memset(cockpit, 0, sizeof(*cockpit));
    }
}

size_t hitl_cockpit_case_count(const HitlCockpit *cockpit)
{
    if (cockpit == NULL || cockpit->initialized == 0u ||
        !hitl_cockpit_plan_valid(cockpit->plan)) {
        return 0u;
    }
    return cockpit->plan->case_count;
}

const HitlCaseState *hitl_cockpit_case_state(const HitlCockpit *cockpit,
                                             size_t case_index)
{
    if (case_index >= hitl_cockpit_case_count(cockpit)) {
        return NULL;
    }
    return &cockpit->case_states[case_index];
}

HitlCombinedResult hitl_cockpit_combined(const HitlCockpit *cockpit,
                                         size_t case_index)
{
    HitlCombinedResult result;

    result.status = HITL_STATUS_BLOCKED;
    result.code = "STALE_EVIDENCE";
    if (cockpit == NULL || cockpit->initialized == 0u ||
        cockpit->plan == NULL || cockpit->auto_evidence == NULL ||
        case_index >= cockpit->plan->case_count) {
        return result;
    }
    return hitl_case_combined(cockpit->plan, cockpit->auto_evidence,
                              case_index,
                              &cockpit->case_states[case_index]);
}

int hitl_cockpit_auto_identity_current(const HitlCockpit *cockpit)
{
    return hitl_cockpit_auto_current(cockpit);
}

int hitl_cockpit_manual_eligible(const HitlCockpit *cockpit,
                                 size_t case_index,
                                 HitlStatus status)
{
    if (!hitl_cockpit_manual_status_valid(status) ||
        !hitl_cockpit_binding_current(cockpit, case_index) ||
        cockpit->session_authority != HITL_INPUT_LIVE_DOS) {
        return 0;
    }
    if (status == HITL_STATUS_PASS && !hitl_cockpit_auto_current(cockpit)) {
        return 0;
    }
    return 1;
}

HitlCockpitResult hitl_cockpit_manual_begin(
    HitlCockpit *cockpit,
    size_t case_index,
    HitlStatus status,
    const HitlInputProof *proof)
{
    if (cockpit == NULL || proof == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (cockpit->initialized == 0u || cockpit->pending.active != 0u) {
        return HITL_COCKPIT_ERR_STATE;
    }
    if (!hitl_cockpit_live_proof(cockpit, proof)) {
        return HITL_COCKPIT_ERR_AUTHORITY;
    }
    if (!hitl_cockpit_manual_status_valid(status) ||
        case_index >= hitl_cockpit_case_count(cockpit)) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (!hitl_cockpit_binding_current(cockpit, case_index)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }
    if (status == HITL_STATUS_PASS && !hitl_cockpit_auto_current(cockpit)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }

    cockpit->pending.identity = cockpit->plan->identity;
    cockpit->pending.case_index = case_index;
    cockpit->pending.status = status;
    cockpit->pending.first_edge_serial = proof->edge_serial;
    cockpit->pending.first_tick = proof->tick;
    cockpit->pending.active = 1u;
    return HITL_COCKPIT_OK;
}

void hitl_cockpit_manual_cancel(HitlCockpit *cockpit)
{
    if (cockpit != NULL) {
        memset(&cockpit->pending, 0, sizeof(cockpit->pending));
    }
}

int hitl_cockpit_manual_pending(const HitlCockpit *cockpit,
                                size_t *case_index,
                                HitlStatus *status)
{
    if (cockpit == NULL || cockpit->initialized == 0u ||
        cockpit->pending.active == 0u) {
        return 0;
    }
    if (case_index != NULL) {
        *case_index = cockpit->pending.case_index;
    }
    if (status != NULL) {
        *status = cockpit->pending.status;
    }
    return 1;
}

HitlCockpitResult hitl_cockpit_manual_confirm(
    HitlCockpit *cockpit,
    const HitlInputProof *proof,
    HitlJournal *journal)
{
    HitlManualPending pending;
    HitlCaseState *state;
    const char *case_id;
    const char *payload;
    HitlCockpitResult result;

    if (cockpit == NULL || proof == NULL || journal == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (cockpit->initialized == 0u || cockpit->pending.active == 0u) {
        return HITL_COCKPIT_ERR_STATE;
    }

    pending = cockpit->pending;
    memset(&cockpit->pending, 0, sizeof(cockpit->pending));

    if (!hitl_cockpit_live_proof(cockpit, proof)) {
        return HITL_COCKPIT_ERR_AUTHORITY;
    }
    if (proof->edge_serial == pending.first_edge_serial ||
        proof->tick <= pending.first_tick) {
        return HITL_COCKPIT_ERR_CONFIRMATION;
    }
    if (!hitl_cockpit_plan_valid(cockpit->plan) ||
        pending.case_index >= cockpit->plan->case_count ||
        !hitl_identity_equal(&pending.identity,
                             &cockpit->plan->identity) ||
        !hitl_cockpit_binding_current(cockpit, pending.case_index)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }
    if (pending.status == HITL_STATUS_PASS &&
        !hitl_cockpit_auto_current(cockpit)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }
    if (journal->initialized == 0u ||
        !hitl_identity_equal(&journal->identity,
                             &cockpit->plan->identity)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }

    case_id = cockpit->plan->cases[pending.case_index].id;
    payload = hitl_status_string(pending.status);
    result = hitl_journal_append(journal, HITL_JOURNAL_MANUAL,
                                 proof->tick, case_id, payload,
                                 proof->authority);
    if (result != HITL_COCKPIT_OK) {
        return result;
    }

    state = &cockpit->case_states[pending.case_index];
    state->identity = cockpit->plan->identity;
    memcpy(state->case_id, case_id, strlen(case_id) + 1u);
    state->manual_status = pending.status;
    state->manual_evidence_current = 1;
    return HITL_COCKPIT_OK;
}
