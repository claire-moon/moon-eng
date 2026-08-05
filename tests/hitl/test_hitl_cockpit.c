#include "moon/hitl_cockpit.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "FAIL %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #condition);                       \
            return 0;                                                      \
        }                                                                  \
    } while (0)

#define HASH_A                                                             \
    "aaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaa"                                  \
    "aaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaa"

#define HASH_B                                                             \
    "bbbbbbbbbbbbbbbb" "bbbbbbbbbbbbbbbb"                                  \
    "bbbbbbbbbbbbbbbb" "bbbbbbbbbbbbbbbb"

#define HASH_C                                                             \
    "cccccccccccccccc" "cccccccccccccccc"                                  \
    "cccccccccccccccc" "cccccccccccccccc"

static void make_plan(HitlPlan *plan)
{
    memset(plan, 0, sizeof(*plan));

    plan->format_major = HITL_FORMAT_MAJOR;
    plan->format_minor = HITL_FORMAT_MINOR;

    strcpy(plan->identity.run, "R260804A");
    strcpy(plan->identity.build_hash, HASH_A);
    strcpy(plan->identity.plan_hash, HASH_B);
    strcpy(plan->identity.profile_hash, HASH_C);

    strcpy(plan->profile.id, "DOSBOX");
    strcpy(plan->profile.operating_system, "DOSBox 0.74-3");
    strcpy(plan->profile.cpu, "cycles auto");
    strcpy(plan->profile.memory, "16 MB");
    strcpy(plan->profile.blaster, "NONE");

    plan->case_count = 2u;

    strcpy(plan->cases[0].id, "COCKPIT.NAV");
    plan->cases[0].auto_required = 1u;
    plan->cases[0].manual_required = 1u;
    strcpy(plan->cases[0].title, "Cockpit navigation");

    strcpy(plan->cases[1].id, "COCKPIT.AUTH");
    plan->cases[1].auto_required = 0u;
    plan->cases[1].manual_required = 1u;
    strcpy(plan->cases[1].title, "Live DOS authority");
}

static void make_auto(const HitlPlan *plan, HitlAutoEvidence *evidence)
{
    memset(evidence, 0, sizeof(*evidence));

    evidence->format_major = HITL_FORMAT_MAJOR;
    evidence->format_minor = HITL_FORMAT_MINOR;
    evidence->identity = plan->identity;
    evidence->record_count = plan->case_count;
    evidence->identity_current = 1;

    strcpy(evidence->records[0].case_id, plan->cases[0].id);
    evidence->records[0].status = HITL_STATUS_PASS;
    strcpy(evidence->records[0].result_code, "OK");
    evidence->records[0].elapsed_ms = UINT32_C(1);
    strcpy(evidence->records[0].detail, "portable precheck passed");

    strcpy(evidence->records[1].case_id, plan->cases[1].id);
    evidence->records[1].status = HITL_STATUS_UNRUN;
    strcpy(evidence->records[1].result_code, "MANUAL_ONLY");
    evidence->records[1].elapsed_ms = UINT32_C(0);
    strcpy(evidence->records[1].detail, "requires physical user");
}

static HitlInputProof make_proof(uint32_t serial, uint32_t tick)
{
    HitlInputProof proof;

    memset(&proof, 0, sizeof(proof));
    proof.authority = HITL_INPUT_LIVE_DOS;
    proof.fresh_pressed = 1u;
    proof.edge_serial = serial;
    proof.tick = tick;

    return proof;
}

static FILE *attach_empty_journal(const HitlPlan *plan,
                                  HitlJournal *journal)
{
    FILE *stream = tmpfile();
    unsigned long error_line = 0u;

    if (stream == NULL) {
        return NULL;
    }

    if (hitl_journal_attach(journal, stream, plan, &error_line) !=
        HITL_COCKPIT_OK) {
        fclose(stream);
        return NULL;
    }

    return stream;
}

static int test_initialization_and_authority(void)
{
    HitlPlan plan;
    HitlAutoEvidence evidence;
    HitlCockpit cockpit;
    HitlInputProof proof;

    make_plan(&plan);
    make_auto(&plan, &evidence);
    proof = make_proof(UINT32_C(1), UINT32_C(10));

    CHECK(hitl_cockpit_init(&cockpit, &plan, &evidence,
                            HITL_INPUT_SYNTHETIC) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_cockpit_case_count(&cockpit) == 2u);
    CHECK(hitl_cockpit_auto_identity_current(&cockpit));
    CHECK(!hitl_cockpit_manual_eligible(&cockpit, 0u,
                                        HITL_STATUS_PASS));
    CHECK(hitl_cockpit_manual_begin(&cockpit, 0u,
                                    HITL_STATUS_PASS, &proof) ==
          HITL_COCKPIT_ERR_AUTHORITY);
    hitl_cockpit_reset(&cockpit);

    CHECK(hitl_cockpit_init(&cockpit, &plan, &evidence,
                            HITL_INPUT_LIVE_DOS) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_cockpit_manual_eligible(&cockpit, 0u,
                                       HITL_STATUS_PASS));
    CHECK(hitl_cockpit_manual_eligible(&cockpit, 1u,
                                       HITL_STATUS_FAIL));

    hitl_cockpit_reset(&cockpit);
    return 1;
}

static int test_two_edge_manual_commit(void)
{
    HitlPlan plan;
    HitlAutoEvidence evidence;
    HitlCockpit cockpit;
    HitlJournal journal;
    HitlInputProof first;
    HitlInputProof second;
    HitlInputProof third;
    const HitlCaseState *state;
    FILE *stream;
    size_t pending_case = 99u;
    HitlStatus pending_status = HITL_STATUS_INVALID;
    uint32_t crc = 0u;
    uint32_t count = 0u;
    unsigned long error_line = 0u;

    make_plan(&plan);
    make_auto(&plan, &evidence);

    CHECK(hitl_cockpit_init(&cockpit, &plan, &evidence,
                            HITL_INPUT_LIVE_DOS) ==
          HITL_COCKPIT_OK);

    stream = attach_empty_journal(&plan, &journal);
    CHECK(stream != NULL);

    first = make_proof(UINT32_C(1), UINT32_C(10));

    CHECK(hitl_cockpit_manual_begin(&cockpit, 0u,
                                    HITL_STATUS_PASS, &first) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_cockpit_manual_pending(&cockpit, &pending_case,
                                      &pending_status));
    CHECK(pending_case == 0u);
    CHECK(pending_status == HITL_STATUS_PASS);

    /*
     * The same edge cannot confirm. The failed attempt consumes the pending
     * token and leaves the published MANUAL state unchanged.
     */
    CHECK(hitl_cockpit_manual_confirm(&cockpit, &first, &journal) ==
          HITL_COCKPIT_ERR_CONFIRMATION);
    CHECK(!hitl_cockpit_manual_pending(&cockpit, NULL, NULL));

    state = hitl_cockpit_case_state(&cockpit, 0u);
    CHECK(state != NULL);
    CHECK(state->manual_status == HITL_STATUS_UNRUN);
    CHECK(!state->manual_evidence_current);

    second = make_proof(UINT32_C(2), UINT32_C(20));
    third = make_proof(UINT32_C(3), UINT32_C(21));

    CHECK(hitl_cockpit_manual_begin(&cockpit, 0u,
                                    HITL_STATUS_PASS, &second) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_cockpit_manual_confirm(&cockpit, &third, &journal) ==
          HITL_COCKPIT_OK);

    state = hitl_cockpit_case_state(&cockpit, 0u);
    CHECK(state != NULL);
    CHECK(state->manual_status == HITL_STATUS_PASS);
    CHECK(state->manual_evidence_current);
    CHECK(hitl_journal_event_count(&journal) == UINT32_C(1));

    CHECK(hitl_journal_verify(stream, &plan, &crc, &count,
                              &error_line) ==
          HITL_COCKPIT_OK);
    CHECK(error_line == 0u);
    CHECK(count == UINT32_C(1));
    CHECK(crc == hitl_journal_crc32(&journal));

    hitl_journal_detach(&journal);
    hitl_cockpit_reset(&cockpit);
    fclose(stream);
    return 1;
}

static int test_stale_pass_gate(void)
{
    HitlPlan plan;
    HitlAutoEvidence evidence;
    HitlCockpit cockpit;

    make_plan(&plan);
    make_auto(&plan, &evidence);
    evidence.identity_current = 0;

    CHECK(hitl_cockpit_init(&cockpit, &plan, &evidence,
                            HITL_INPUT_LIVE_DOS) ==
          HITL_COCKPIT_OK);

    CHECK(!hitl_cockpit_auto_identity_current(&cockpit));
    CHECK(!hitl_cockpit_manual_eligible(&cockpit, 0u,
                                        HITL_STATUS_PASS));
    CHECK(hitl_cockpit_manual_eligible(&cockpit, 0u,
                                       HITL_STATUS_FAIL));
    CHECK(hitl_cockpit_manual_eligible(&cockpit, 0u,
                                       HITL_STATUS_BLOCKED));

    hitl_cockpit_reset(&cockpit);
    return 1;
}

static int test_imported_journal_is_history_only(void)
{
    HitlPlan plan;
    HitlAutoEvidence evidence;
    HitlCockpit cockpit;
    HitlJournal journal;
    const HitlCaseState *state;
    FILE *stream;
    unsigned long error_line = 0u;

    make_plan(&plan);
    make_auto(&plan, &evidence);

    stream = attach_empty_journal(&plan, &journal);
    CHECK(stream != NULL);

    CHECK(hitl_journal_append(&journal, HITL_JOURNAL_MANUAL,
                              UINT32_C(10), "COCKPIT.NAV", "FAIL",
                              HITL_INPUT_LIVE_DOS) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_journal_event_count(&journal) == UINT32_C(1));
    hitl_journal_detach(&journal);

    CHECK(hitl_cockpit_init(&cockpit, &plan, &evidence,
                            HITL_INPUT_LIVE_DOS) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_journal_attach(&journal, stream, &plan,
                              &error_line) ==
          HITL_COCKPIT_OK);
    CHECK(hitl_journal_event_count(&journal) == UINT32_C(1));

    state = hitl_cockpit_case_state(&cockpit, 0u);
    CHECK(state != NULL);
    CHECK(state->manual_status == HITL_STATUS_UNRUN);
    CHECK(!state->manual_evidence_current);

    hitl_journal_detach(&journal);
    hitl_cockpit_reset(&cockpit);
    fclose(stream);
    return 1;
}

int main(void)
{
    if (!test_initialization_and_authority() ||
        !test_two_edge_manual_commit() ||
        !test_stale_pass_gate() ||
        !test_imported_journal_is_history_only()) {
        return 1;
    }

    puts("PASS: HITL cockpit controller tests");
    return 0;
}
