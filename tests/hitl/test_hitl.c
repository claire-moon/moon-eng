#include "moon/hitl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while (0)

#define HASH_A \
    "aaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaa" \
    "aaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaa"
#define HASH_B \
    "bbbbbbbbbbbbbbbb" "bbbbbbbbbbbbbbbb" \
    "bbbbbbbbbbbbbbbb" "bbbbbbbbbbbbbbbb"
#define HASH_C \
    "cccccccccccccccc" "cccccccccccccccc" \
    "cccccccccccccccc" "cccccccccccccccc"
#define HASH_D \
    "dddddddddddddddd" "dddddddddddddddd" \
    "dddddddddddddddd" "dddddddddddddddd"

#define PLAN_PREAMBLE \
    "HITL\t1\t0\r\n" \
    "RUN\tR260730A\r\n" \
    "BUILD_HASH\t" HASH_A "\r\n" \
    "PLAN_HASH\t" HASH_B "\r\n" \
    "PROFILE_HASH\t" HASH_C "\r\n" \
    "PROFILE\tW98P90\tWindows 98 SE\tPentium 90\t16 MB\tA220 I7 D1 H5\r\n"

#define VALID_PLAN \
    "# selected acceptance cases\r\n" \
    PLAN_PREAMBLE \
    "CASE\tVID.START\t1\t1\tZEUS starts and displays the title screen\r\n" \
    "CASE\tMENU.KEYS\t0\t1\tEvery main menu command is keyboard reachable\n"

#define AUTO_PREAMBLE \
    "HITL\t1\t0\r\n" \
    "RUN\tR260730A\r\n" \
    "BUILD_HASH\t" HASH_A "\r\n" \
    "PLAN_HASH\t" HASH_B "\r\n" \
    "PROFILE_HASH\t" HASH_C "\r\n"

#define VALID_AUTO \
    AUTO_PREAMBLE \
    "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\trequires operator\r\n" \
    "AUTO\tVID.START\tPASS\tOK\t184\ttitle state reached\r\n"

static FILE *open_bytes(const void *data, size_t size)
{
    FILE *stream = tmpfile();

    if (stream == NULL) {
        return NULL;
    }
    if (size != 0u && fwrite(data, 1u, size, stream) != size) {
        fclose(stream);
        return NULL;
    }
    if (fflush(stream) != 0 || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    return stream;
}

static FILE *open_text(const char *text)
{
    return open_bytes(text, strlen(text));
}

static int memory_is_zero(const void *memory, size_t size)
{
    const unsigned char *bytes = (const unsigned char *)memory;
    size_t index;

    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) {
            return 0;
        }
    }
    return 1;
}

static HitlResult read_plan_text(const char *text,
                                 HitlPlan *plan,
                                 unsigned long *line)
{
    FILE *stream = open_text(text);
    HitlResult result;

    if (stream == NULL) {
        return HITL_ERR_IO;
    }
    result = hitl_plan_read(stream, plan, line);
    fclose(stream);
    return result;
}

static HitlResult read_auto_text(const char *text,
                                 const HitlPlan *plan,
                                 HitlAutoEvidence *evidence,
                                 unsigned long *line)
{
    FILE *stream = open_text(text);
    HitlResult result;

    if (stream == NULL) {
        return HITL_ERR_IO;
    }
    result = hitl_auto_read(stream, plan, evidence, line);
    fclose(stream);
    return result;
}

static int expect_plan_error_zero(const char *text, HitlResult expected)
{
    static HitlPlan plan;
    unsigned long line = 0u;

    memset(&plan, 0xa5, sizeof(plan));
    CHECK(read_plan_text(text, &plan, &line) == expected);
    CHECK(memory_is_zero(&plan, sizeof(plan)));
    return 1;
}

static int expect_auto_error_zero(const char *text,
                                  const HitlPlan *plan,
                                  HitlResult expected)
{
    static HitlAutoEvidence evidence;
    unsigned long line = 0u;

    memset(&evidence, 0xa5, sizeof(evidence));
    CHECK(read_auto_text(text, plan, &evidence, &line) == expected);
    CHECK(memory_is_zero(&evidence, sizeof(evidence)));
    return 1;
}

static int test_status_conversion(void)
{
    HitlStatus status = HITL_STATUS_INVALID;
    HitlCaseState zero_state;

    memset(&zero_state, 0, sizeof(zero_state));
    CHECK(zero_state.auto_status == HITL_STATUS_UNRUN);
    CHECK(zero_state.manual_status == HITL_STATUS_UNRUN);
    CHECK(zero_state.case_id[0] == '\0');
    CHECK(!zero_state.auto_evidence_current);

    CHECK(strcmp(hitl_status_string(HITL_STATUS_PASS), "PASS") == 0);
    CHECK(strcmp(hitl_status_string(HITL_STATUS_FAIL), "FAIL") == 0);
    CHECK(strcmp(hitl_status_string(HITL_STATUS_BLOCKED), "BLOCKED") == 0);
    CHECK(strcmp(hitl_status_string(HITL_STATUS_UNRUN), "UNRUN") == 0);
    CHECK(strcmp(hitl_status_string((HitlStatus)99), "INVALID") == 0);

    CHECK(hitl_status_from_string("PASS", &status));
    CHECK(status == HITL_STATUS_PASS);
    CHECK(hitl_status_from_string("FAIL", &status));
    CHECK(status == HITL_STATUS_FAIL);
    CHECK(hitl_status_from_string("BLOCKED", &status));
    CHECK(status == HITL_STATUS_BLOCKED);
    CHECK(hitl_status_from_string("UNRUN", &status));
    CHECK(status == HITL_STATUS_UNRUN);
    CHECK(!hitl_status_from_string("pass", &status));
    CHECK(status == HITL_STATUS_INVALID);
    CHECK(!hitl_status_from_string(NULL, &status));
    CHECK(!hitl_status_from_string("PASS", NULL));
    return 1;
}

static int test_record_reader(void)
{
    static const char mixed[] =
        "# ignored comment\r\n"
        "\n"
        "RUN\tR260730A\n"
        "TAIL\tvalue";
    HitlReader reader;
    HitlRecord record;
    HitlResult result;
    FILE *stream = open_bytes(mixed, sizeof(mixed) - 1u);
    char boundary[HITL_RECORD_CONTENT_MAX + 2u];
    unsigned char invalid[] = { 'R', 'U', 'N', '\t', 'A', 0u, 'B', '\n' };
    static const char empty_fields[] = "EMPTY\t\nTRAIL\tA\t\r\n";
    static const char too_many_fields[] =
        "A\tB\tC\tD\tE\tF\tG\tH\tI\n";
    static const char lone_cr[] = "RUN\tR260730A\rTAIL\tvalue\n";
    size_t index;

    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_OK);
    CHECK(record.line_number == 3u);
    CHECK(record.field_count == 2u);
    CHECK(strcmp(record.fields[0], "RUN") == 0);
    CHECK(strcmp(record.fields[1], "R260730A") == 0);
    CHECK(hitl_reader_next(&reader, &record) == HITL_OK);
    CHECK(record.line_number == 4u);
    CHECK(strcmp(record.fields[0], "TAIL") == 0);
    CHECK(strcmp(record.fields[1], "value") == 0);
    CHECK(hitl_reader_next(&reader, &record) == HITL_END);
    fclose(stream);

    for (index = 0u; index < HITL_RECORD_CONTENT_MAX; ++index) {
        boundary[index] = 'A';
    }
    boundary[HITL_RECORD_CONTENT_MAX] = '\n';
    boundary[HITL_RECORD_CONTENT_MAX + 1u] = '\0';
    stream = open_bytes(boundary, HITL_RECORD_CONTENT_MAX + 1u);
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_OK);
    CHECK(strlen(record.fields[0]) == HITL_RECORD_CONTENT_MAX);
    fclose(stream);

    for (index = 0u; index < HITL_RECORD_CONTENT_MAX + 1u; ++index) {
        boundary[index] = 'A';
    }
    boundary[HITL_RECORD_CONTENT_MAX + 1u] = '\n';
    stream = open_bytes(boundary, sizeof(boundary));
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_ERR_RECORD_TOO_LONG);
    fclose(stream);

    stream = open_bytes(invalid, sizeof(invalid));
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_ERR_FORMAT);
    fclose(stream);

    stream = open_text(empty_fields);
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_OK);
    CHECK(record.field_count == 2u);
    CHECK(record.fields[1][0] == '\0');
    CHECK(hitl_reader_next(&reader, &record) == HITL_OK);
    CHECK(record.field_count == 3u);
    CHECK(record.fields[2][0] == '\0');
    fclose(stream);

    stream = open_text(too_many_fields);
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_ERR_FIELD_COUNT);
    CHECK(record.line_number == 1u);
    fclose(stream);

    stream = open_text(lone_cr);
    CHECK(stream != NULL);
    hitl_reader_init(&reader, stream);
    CHECK(hitl_reader_next(&reader, &record) == HITL_ERR_FORMAT);
    CHECK(record.line_number == 1u);
    fclose(stream);

    result = hitl_reader_next(NULL, &record);
    CHECK(result == HITL_ERR_ARGUMENT);
    hitl_reader_init(&reader, NULL);
    CHECK(hitl_reader_next(&reader, &record) == HITL_ERR_ARGUMENT);
    hitl_reader_init(NULL, NULL);
    return 1;
}

static int test_valid_plan(void)
{
    static HitlPlan plan;
    HitlIdentity copy;
    unsigned long line = 99u;

    CHECK(read_plan_text(VALID_PLAN, &plan, &line) == HITL_OK);
    CHECK(line == 0u);
    CHECK(plan.format_major == 1u && plan.format_minor == 0u);
    CHECK(strcmp(plan.identity.run, "R260730A") == 0);
    CHECK(strcmp(plan.identity.build_hash, HASH_A) == 0);
    CHECK(strcmp(plan.identity.plan_hash, HASH_B) == 0);
    CHECK(strcmp(plan.identity.profile_hash, HASH_C) == 0);
    CHECK(strcmp(plan.profile.id, "W98P90") == 0);
    CHECK(strcmp(plan.profile.operating_system, "Windows 98 SE") == 0);
    CHECK(strcmp(plan.profile.cpu, "Pentium 90") == 0);
    CHECK(strcmp(plan.profile.memory, "16 MB") == 0);
    CHECK(strcmp(plan.profile.blaster, "A220 I7 D1 H5") == 0);
    CHECK(plan.case_count == 2u);
    CHECK(strcmp(plan.cases[0].id, "VID.START") == 0);
    CHECK(plan.cases[0].auto_required == 1u);
    CHECK(plan.cases[0].manual_required == 1u);
    CHECK(strcmp(plan.cases[1].id, "MENU.KEYS") == 0);
    CHECK(plan.cases[1].auto_required == 0u);
    CHECK(plan.cases[1].manual_required == 1u);

    copy = plan.identity;
    CHECK(hitl_identity_equal(&plan.identity, &copy));
    copy.run[0] = 'X';
    CHECK(!hitl_identity_equal(&plan.identity, &copy));
    CHECK(!hitl_identity_equal(NULL, &copy));
    return 1;
}

static int test_plan_rejections_and_extensions(void)
{
    static HitlPlan plan;
    unsigned long line;
    static const char duplicate_case[] =
        PLAN_PREAMBLE
        "CASE\tVID.START\t1\t0\tfirst\r\n"
        "CASE\tVID.START\t0\t1\tsecond\r\n";
    static const char bad_case_id[] =
        PLAN_PREAMBLE
        "CASE\tvid.START\t1\t0\tbad lowercase ID\r\n";
    static const char no_required_lane[] =
        PLAN_PREAMBLE
        "CASE\tVID.START\t0\t0\tno required lane\r\n";
    static const char missing_case[] = PLAN_PREAMBLE;
    static const char unknown_record[] =
        PLAN_PREAMBLE
        "BOGUS\tvalue\r\n"
        "CASE\tVID.START\t1\t0\tvalid case\r\n";
    static const char duplicate_run[] =
        "HITL\t1\t0\r\n"
        "RUN\tR260730A\r\n"
        "RUN\tR260730B\r\n";
    static const char bad_major[] = "HITL\t2\t0\r\n";
    static const char bad_minor_text[] = "HITL\t1\tminor\r\n";
    static const char bad_minor_signed[] = "HITL\t1\t-1\r\n";
    static const char bad_minor_overflow[] =
        "HITL\t1\t4294967296\r\n";
    static const char header_trailing_tab[] = "HITL\t1\t0\t\r\n";
    static const char empty_run[] = "HITL\t1\t0\r\nRUN\t\r\n";
    static const char run_trailing_tab[] =
        "HITL\t1\t0\r\nRUN\tR260730A\t\r\n";
    static const char empty_profile_field[] =
        "HITL\t1\t0\r\n"
        "RUN\tR260730A\r\n"
        "BUILD_HASH\t" HASH_A "\r\n"
        "PLAN_HASH\t" HASH_B "\r\n"
        "PROFILE_HASH\t" HASH_C "\r\n"
        "PROFILE\tW98P90\t\tPentium 90\t16 MB\tA220 I7 D1 H5\r\n"
        "CASE\tVID.START\t1\t0\ttitle visible\r\n";
    static const char malformed_extension[] =
        "HITL\t1\t0\r\n"
        "X-\tignored\r\n";
    static const char extension[] =
        "HITL\t1\t1\r\n"
        "X-FUTURE\t1\t2\t3\t4\t5\t6\t7\t8\t9\r\n"
        "RUN\tR260730A\r\n"
        "BUILD_HASH\t" HASH_A "\r\n"
        "PLAN_HASH\t" HASH_B "\r\n"
        "PROFILE_HASH\t" HASH_C "\r\n"
        "PROFILE\tDOSBOX\tDOSBox\tcycles auto\t16 MB\tA220 I7 D1 H5\r\n"
        "X-CASE-NOTE\tignored too\r\n"
        "CASE\tVID.START\t1\t0\ttitle visible\r\n";

    memset(&plan, 0xa5, sizeof(plan));
    CHECK(read_plan_text(duplicate_case, &plan, &line) == HITL_ERR_DUPLICATE);
    CHECK(line == 8u);
    CHECK(memory_is_zero(&plan, sizeof(plan)));
    CHECK(expect_plan_error_zero(bad_case_id, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(no_required_lane, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(missing_case, HITL_ERR_MISSING));
    CHECK(expect_plan_error_zero(unknown_record, HITL_ERR_UNKNOWN_RECORD));
    CHECK(expect_plan_error_zero(duplicate_run, HITL_ERR_DUPLICATE));
    CHECK(expect_plan_error_zero(bad_major, HITL_ERR_VERSION));
    CHECK(expect_plan_error_zero(bad_minor_text, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(bad_minor_signed, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(bad_minor_overflow, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(header_trailing_tab,
                                 HITL_ERR_FIELD_COUNT));
    CHECK(expect_plan_error_zero(empty_run, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(run_trailing_tab, HITL_ERR_FIELD_COUNT));
    CHECK(expect_plan_error_zero(empty_profile_field, HITL_ERR_FORMAT));
    CHECK(expect_plan_error_zero(malformed_extension,
                                 HITL_ERR_UNKNOWN_RECORD));
    CHECK(read_plan_text(extension, &plan, &line) == HITL_OK);
    CHECK(plan.format_minor == 1u && plan.case_count == 1u);
    return 1;
}

static int test_valid_auto_and_lane_separation(void)
{
    static HitlPlan plan;
    static HitlAutoEvidence evidence;
    HitlCaseState state;
    HitlCombinedResult combined;
    unsigned long line;
    static const char empty_details[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\tOK\t184\t\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\t\r\n";

    CHECK(read_plan_text(VALID_PLAN, &plan, &line) == HITL_OK);
    CHECK(read_auto_text(VALID_AUTO, &plan, &evidence, &line) == HITL_OK);
    CHECK(line == 0u);
    CHECK(evidence.identity_current);
    CHECK(evidence.record_count == plan.case_count);

    /* AUTO.OUT file order was reversed; memory order follows HITL.IN. */
    CHECK(strcmp(evidence.records[0].case_id, "VID.START") == 0);
    CHECK(evidence.records[0].status == HITL_STATUS_PASS);
    CHECK(evidence.records[0].elapsed_ms == UINT32_C(184));
    CHECK(strcmp(evidence.records[0].detail, "title state reached") == 0);
    CHECK(strcmp(evidence.records[1].case_id, "MENU.KEYS") == 0);
    CHECK(evidence.records[1].status == HITL_STATUS_UNRUN);

    /* The required final detail field may be present and empty. */
    CHECK(read_auto_text(empty_details, &plan, &evidence, &line) == HITL_OK);
    CHECK(evidence.records[0].detail[0] == '\0');
    CHECK(evidence.records[1].detail[0] == '\0');

    hitl_case_state_init(&state);
    CHECK(state.auto_status == HITL_STATUS_UNRUN);
    CHECK(state.manual_status == HITL_STATUS_UNRUN);
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_OK);
    CHECK(state.auto_status == HITL_STATUS_PASS);
    CHECK(state.manual_status == HITL_STATUS_UNRUN);
    CHECK(strcmp(state.case_id, "VID.START") == 0);
    CHECK(state.auto_evidence_current);
    combined = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(combined.status == HITL_STATUS_UNRUN);
    CHECK(strcmp(combined.code, "MANUAL_REQUIRED") == 0);
    return 1;
}

static int test_auto_rejections_and_stale_identity(void)
{
    static HitlPlan plan;
    static HitlAutoEvidence evidence;
    HitlCaseState state;
    HitlCombinedResult combined;
    unsigned long line;
    static const char duplicate[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\tOK\t1\tone\r\n"
        "AUTO\tVID.START\tPASS\tOK\t2\ttwo\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char missing[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\tOK\t1\tone\r\n";
    static const char unknown_case[] =
        AUTO_PREAMBLE
        "AUTO\tOTHER\tPASS\tOK\t1\tunknown\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char invalid_status[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tpass\tOK\t1\tbad status\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char invalid_elapsed[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\tOK\t4294967296\toverflow\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char empty_code[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\t\t1\tempty code\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char trailing_tab[] =
        AUTO_PREAMBLE
        "AUTO\tVID.START\tPASS\tOK\t1\tdetail\t\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n";
    static const char stale[] =
        "HITL\t1\t0\r\n"
        "RUN\tR260730A\r\n"
        "BUILD_HASH\t" HASH_D "\r\n"
        "PLAN_HASH\t" HASH_B "\r\n"
        "PROFILE_HASH\t" HASH_C "\r\n"
        "AUTO\tVID.START\tPASS\tOK\t184\ttitle state reached\r\n"
        "AUTO\tMENU.KEYS\tUNRUN\tMANUAL_ONLY\t0\trequires operator\r\n";

    CHECK(read_plan_text(VALID_PLAN, &plan, &line) == HITL_OK);
    CHECK(expect_auto_error_zero(duplicate, &plan, HITL_ERR_DUPLICATE));
    CHECK(expect_auto_error_zero(missing, &plan, HITL_ERR_MISSING));
    CHECK(expect_auto_error_zero(unknown_case, &plan,
                                 HITL_ERR_UNKNOWN_CASE));
    CHECK(expect_auto_error_zero(invalid_status, &plan, HITL_ERR_FORMAT));
    CHECK(expect_auto_error_zero(invalid_elapsed, &plan, HITL_ERR_FORMAT));
    CHECK(expect_auto_error_zero(empty_code, &plan, HITL_ERR_FORMAT));
    CHECK(expect_auto_error_zero(trailing_tab, &plan,
                                 HITL_ERR_FIELD_COUNT));

    memset(&evidence, 0xa5, sizeof(evidence));
    CHECK(read_auto_text(stale, &plan, &evidence, &line) == HITL_ERR_STALE);
    CHECK(!evidence.identity_current);
    CHECK(evidence.record_count == 2u);
    hitl_case_state_init(&state);
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_ERR_STALE);
    CHECK(state.auto_status == HITL_STATUS_UNRUN);
    CHECK(!state.auto_evidence_current);
    combined = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(combined.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(combined.code, "STALE_EVIDENCE") == 0);
    return 1;
}

static int test_argument_failures(void)
{
    static HitlPlan plan;
    static HitlPlan invalid_plan;
    static HitlAutoEvidence evidence;
    static HitlAutoEvidence malformed_evidence;
    HitlCaseState state;
    HitlCaseState original_state;
    HitlCombinedResult combined;
    FILE *stream;
    unsigned long line = 99u;

    memset(&plan, 0xa5, sizeof(plan));
    CHECK(hitl_plan_read(NULL, &plan, &line) == HITL_ERR_ARGUMENT);
    CHECK(line == 0u);
    CHECK(memory_is_zero(&plan, sizeof(plan)));

    stream = open_text(VALID_PLAN);
    CHECK(stream != NULL);
    line = 99u;
    CHECK(hitl_plan_read(stream, NULL, &line) == HITL_ERR_ARGUMENT);
    CHECK(line == 0u);
    fclose(stream);

    CHECK(read_plan_text(VALID_PLAN, &plan, &line) == HITL_OK);
    memset(&evidence, 0xa5, sizeof(evidence));
    CHECK(hitl_auto_read(NULL, &plan, &evidence, &line) ==
          HITL_ERR_ARGUMENT);
    CHECK(memory_is_zero(&evidence, sizeof(evidence)));

    stream = open_text(VALID_AUTO);
    CHECK(stream != NULL);
    memset(&evidence, 0xa5, sizeof(evidence));
    CHECK(hitl_auto_read(stream, NULL, &evidence, &line) ==
          HITL_ERR_ARGUMENT);
    CHECK(memory_is_zero(&evidence, sizeof(evidence)));
    fclose(stream);

    memset(&invalid_plan, 0, sizeof(invalid_plan));
    stream = open_text(VALID_AUTO);
    CHECK(stream != NULL);
    memset(&evidence, 0xa5, sizeof(evidence));
    CHECK(hitl_auto_read(stream, &invalid_plan, &evidence, &line) ==
          HITL_ERR_ARGUMENT);
    CHECK(memory_is_zero(&evidence, sizeof(evidence)));
    fclose(stream);

    stream = open_text(VALID_AUTO);
    CHECK(stream != NULL);
    CHECK(hitl_auto_read(stream, &plan, NULL, &line) == HITL_ERR_ARGUMENT);
    fclose(stream);

    hitl_case_state_init(NULL);
    hitl_case_state_init(&state);
    state.manual_status = HITL_STATUS_FAIL;
    original_state = state;
    CHECK(read_auto_text(VALID_AUTO, &plan, &evidence, &line) == HITL_OK);
    CHECK(hitl_case_state_apply_auto(NULL, &plan, &evidence, 0u) ==
          HITL_ERR_ARGUMENT);
    CHECK(hitl_case_state_apply_auto(&state, NULL, &evidence, 0u) ==
          HITL_ERR_ARGUMENT);
    CHECK(hitl_case_state_apply_auto(&state, &plan, NULL, 0u) ==
          HITL_ERR_ARGUMENT);
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence,
                                     plan.case_count) ==
          HITL_ERR_ARGUMENT);

    malformed_evidence = evidence;
    malformed_evidence.records[0].status = HITL_STATUS_INVALID;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &malformed_evidence,
                                     0u) == HITL_ERR_FORMAT);
    CHECK(memcmp(&state, &original_state, sizeof(state)) == 0);

    malformed_evidence = evidence;
    strcpy(malformed_evidence.records[0].case_id, "MENU.KEYS");
    CHECK(hitl_case_state_apply_auto(&state, &plan, &malformed_evidence,
                                     0u) == HITL_ERR_FORMAT);
    CHECK(memcmp(&state, &original_state, sizeof(state)) == 0);

    malformed_evidence = evidence;
    malformed_evidence.identity_current = 0;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &malformed_evidence,
                                     0u) == HITL_ERR_STALE);
    CHECK(memcmp(&state, &original_state, sizeof(state)) == 0);
    CHECK(state.auto_status == HITL_STATUS_UNRUN);
    CHECK(state.manual_status == HITL_STATUS_FAIL);

    combined = hitl_case_combined(NULL, &evidence, 0u, &state);
    CHECK(combined.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(combined.code, "STALE_EVIDENCE") == 0);
    CHECK(strcmp(hitl_result_string((HitlResult)999),
                 "unknown HITL error") == 0);
    return 1;
}

static int test_combined_results(void)
{
    static HitlPlan plan;
    static HitlPlan rebound_plan;
    static HitlAutoEvidence evidence;
    static HitlAutoEvidence rebound_evidence;
    HitlCaseState state;
    HitlCombinedResult result;
    unsigned long line;
    static const char combined_plan[] =
        PLAN_PREAMBLE
        "CASE\tAUTO.ONLY\t1\t0\tautomation only\r\n"
        "CASE\tMANUAL.ONLY\t0\t1\tmanual only\r\n"
        "CASE\tBOTH\t1\t1\tboth lanes\r\n";
    static const char combined_auto[] =
        AUTO_PREAMBLE
        "AUTO\tAUTO.ONLY\tPASS\tOK\t1\tautomated\r\n"
        "AUTO\tMANUAL.ONLY\tUNRUN\tMANUAL_ONLY\t0\toperator\r\n"
        "AUTO\tBOTH\tPASS\tOK\t2\tprecheck\r\n";

    CHECK(read_plan_text(combined_plan, &plan, &line) == HITL_OK);
    CHECK(read_auto_text(combined_auto, &plan, &evidence, &line) == HITL_OK);
    hitl_case_state_init(&state);

    result = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(result.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(result.code, "STALE_EVIDENCE") == 0);

    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(result.status == HITL_STATUS_PASS);
    CHECK(strcmp(result.code, "OK") == 0);

    evidence.records[0].status = HITL_STATUS_FAIL;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(result.status == HITL_STATUS_FAIL);
    CHECK(strcmp(result.code, "AUTO_FAIL") == 0);

    evidence.records[0].status = HITL_STATUS_BLOCKED;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(result.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(result.code, "AUTO_BLOCKED") == 0);

    evidence.records[0].status = HITL_STATUS_UNRUN;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 0u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 0u, &state);
    CHECK(result.status == HITL_STATUS_UNRUN);
    CHECK(strcmp(result.code, "AUTO_REQUIRED") == 0);

    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 1u) ==
          HITL_OK);
    state.manual_status = HITL_STATUS_PASS; /* trusted cockpit input */
    state.manual_evidence_current = 1;
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_PASS);

    state.manual_status = HITL_STATUS_FAIL;
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_FAIL);
    CHECK(strcmp(result.code, "MANUAL_FAIL") == 0);

    state.manual_status = HITL_STATUS_BLOCKED;
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(result.code, "MANUAL_BLOCKED") == 0);

    state.manual_status = HITL_STATUS_UNRUN;
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_UNRUN);
    CHECK(strcmp(result.code, "MANUAL_REQUIRED") == 0);

    /* An optional lane that actually fails still fails the case. */
    evidence.records[1].status = HITL_STATUS_FAIL;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 1u) ==
          HITL_OK);
    state.manual_status = HITL_STATUS_PASS;
    state.manual_evidence_current = 1;
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_FAIL);
    CHECK(strcmp(result.code, "AUTO_FAIL") == 0);

    /* Optional BLOCKED does not invalidate a passing required lane. */
    evidence.records[1].status = HITL_STATUS_BLOCKED;
    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 1u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 1u, &state);
    CHECK(result.status == HITL_STATUS_PASS);

    /* Reapplying the same binding preserves a trusted manual result. */
    CHECK(state.manual_status == HITL_STATUS_PASS);
    CHECK(state.manual_evidence_current);

    /* A new identity with the same case IDs invalidates the old state. */
    rebound_plan = plan;
    rebound_evidence = evidence;
    strcpy(rebound_plan.identity.build_hash, HASH_D);
    strcpy(rebound_evidence.identity.build_hash, HASH_D);
    rebound_evidence.identity_current = 1;
    result = hitl_case_combined(&rebound_plan, &rebound_evidence, 1u,
                                &state);
    CHECK(result.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(result.code, "STALE_EVIDENCE") == 0);

    CHECK(hitl_case_state_apply_auto(&state, &rebound_plan,
                                     &rebound_evidence, 1u) == HITL_OK);
    CHECK(state.manual_status == HITL_STATUS_UNRUN);
    CHECK(!state.manual_evidence_current);
    result = hitl_case_combined(&rebound_plan, &rebound_evidence, 1u,
                                &state);
    CHECK(result.status == HITL_STATUS_UNRUN);
    CHECK(strcmp(result.code, "MANUAL_REQUIRED") == 0);

    /* A changed AUTO status must be explicitly rebound before evaluation. */
    rebound_evidence.records[1].status = HITL_STATUS_PASS;
    result = hitl_case_combined(&rebound_plan, &rebound_evidence, 1u,
                                &state);
    CHECK(result.status == HITL_STATUS_BLOCKED);
    CHECK(strcmp(result.code, "STALE_EVIDENCE") == 0);

    CHECK(hitl_case_state_apply_auto(&state, &plan, &evidence, 2u) ==
          HITL_OK);
    result = hitl_case_combined(&plan, &evidence, 2u, &state);
    CHECK(result.status == HITL_STATUS_UNRUN);
    CHECK(strcmp(result.code, "MANUAL_REQUIRED") == 0);
    state.manual_status = HITL_STATUS_PASS;
    state.manual_evidence_current = 1;
    result = hitl_case_combined(&plan, &evidence, 2u, &state);
    CHECK(result.status == HITL_STATUS_PASS);
    return 1;
}

int main(void)
{
    if (!test_status_conversion() ||
        !test_record_reader() ||
        !test_valid_plan() ||
        !test_plan_rejections_and_extensions() ||
        !test_valid_auto_and_lane_separation() ||
        !test_auto_rejections_and_stale_identity() ||
        !test_argument_failures() ||
        !test_combined_results()) {
        return 1;
    }
    puts("PASS: HITL evidence core tests");
    return 0;
}
