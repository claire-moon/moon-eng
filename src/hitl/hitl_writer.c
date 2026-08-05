#include "moon/hitl_cockpit.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define HITL_CRC_INITIAL UINT32_C(0xffffffff)
#define HITL_CRC_XOR_OUT UINT32_C(0xffffffff)
#define HITL_CAPTURE_LENGTH 10u

typedef struct HitlJournalScan {
    uint32_t crc_state;
    uint32_t event_count;
    uint32_t next_sequence;
    uint32_t last_tick;
    HitlStatus manual[HITL_MAX_CASES];
    char capture[HITL_MAX_CASES][HITL_CAPTURE_LENGTH + 1u];
} HitlJournalScan;

static uint32_t hitl_crc32_update(uint32_t state,
                                  const void *data,
                                  size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    unsigned int bit;

    for (index = 0u; index < size; ++index) {
        state ^= (uint32_t)bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(state & UINT32_C(1));
            state = (state >> 1) ^
                    (UINT32_C(0xedb88320) & mask);
        }
    }
    return state;
}

static int hitl_writer_status_valid(HitlStatus status)
{
    return status == HITL_STATUS_UNRUN || status == HITL_STATUS_PASS ||
           status == HITL_STATUS_FAIL || status == HITL_STATUS_BLOCKED;
}

static int hitl_writer_manual_status_valid(HitlStatus status)
{
    return status == HITL_STATUS_PASS || status == HITL_STATUS_FAIL ||
           status == HITL_STATUS_BLOCKED;
}

static int hitl_writer_field_valid(const char *text, int allow_empty)
{
    size_t index;

    if (text == NULL) {
        return 0;
    }
    for (index = 0u; index <= HITL_RECORD_CONTENT_MAX; ++index) {
        unsigned char character = (unsigned char)text[index];
        if (character == 0u) {
            return allow_empty || index != 0u;
        }
        if (character < 0x20u || character > 0xfeu ||
            character == (unsigned char)'\t' ||
            character == (unsigned char)'\r' ||
            character == (unsigned char)'\n') {
            return 0;
        }
    }
    return 0;
}

static int hitl_writer_plan_case_index(const HitlPlan *plan,
                                       const char *case_id,
                                       size_t *case_index)
{
    size_t index;

    if (plan == NULL || case_id == NULL) {
        return 0;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        if (strcmp(plan->cases[index].id, case_id) == 0) {
            if (case_index != NULL) {
                *case_index = index;
            }
            return 1;
        }
    }
    return 0;
}

static int hitl_writer_plan_valid(const HitlPlan *plan)
{
    size_t index;

    if (plan == NULL || plan->format_major != HITL_FORMAT_MAJOR ||
        plan->case_count == 0u || plan->case_count > HITL_MAX_CASES ||
        plan->identity.run[0] == '\0' ||
        plan->identity.build_hash[0] == '\0' ||
        plan->identity.plan_hash[0] == '\0' ||
        plan->identity.profile_hash[0] == '\0' ||
        plan->profile.id[0] == '\0') {
        return 0;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        if (plan->cases[index].id[0] == '\0' ||
            plan->cases[index].auto_required > 1u ||
            plan->cases[index].manual_required > 1u ||
            (plan->cases[index].auto_required == 0u &&
             plan->cases[index].manual_required == 0u)) {
            return 0;
        }
    }
    return 1;
}

static int hitl_writer_auto_shape_valid(const HitlPlan *plan,
                                        const HitlAutoEvidence *evidence)
{
    size_t index;

    if (!hitl_writer_plan_valid(plan) || evidence == NULL ||
        evidence->format_major != HITL_FORMAT_MAJOR ||
        evidence->record_count != plan->case_count) {
        return 0;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        if (!hitl_writer_status_valid(evidence->records[index].status) ||
            strcmp(plan->cases[index].id,
                   evidence->records[index].case_id) != 0) {
            return 0;
        }
    }
    return 1;
}

static const char *hitl_journal_event_name(HitlJournalEventType type)
{
    switch (type) {
        case HITL_JOURNAL_OPEN: return "OPEN";
        case HITL_JOURNAL_STEP: return "STEP";
        case HITL_JOURNAL_CAPTURE: return "CAPTURE";
        case HITL_JOURNAL_MANUAL: return "MANUAL";
        case HITL_JOURNAL_NOTE: return "NOTE";
        case HITL_JOURNAL_WARNING: return "WARNING";
        case HITL_JOURNAL_COMMIT: return "COMMIT";
        case HITL_JOURNAL_COMMIT_FAIL: return "COMMIT_FAIL";
        default: return NULL;
    }
}

static int hitl_journal_event_from_name(const char *name,
                                        HitlJournalEventType *type)
{
    HitlJournalEventType candidate;

    if (name == NULL || type == NULL) {
        return 0;
    }
    for (candidate = HITL_JOURNAL_OPEN;
         candidate <= HITL_JOURNAL_COMMIT_FAIL;
         candidate = (HitlJournalEventType)((int)candidate + 1)) {
        const char *candidate_name = hitl_journal_event_name(candidate);
        if (candidate_name != NULL && strcmp(name, candidate_name) == 0) {
            *type = candidate;
            return 1;
        }
    }
    return 0;
}

static int hitl_writer_capture_valid(const char *name)
{
    return name != NULL && strlen(name) == HITL_CAPTURE_LENGTH &&
           name[0] == 'C' && name[1] == 'A' && name[2] == 'P' &&
           name[3] >= '0' && name[3] <= '9' &&
           name[4] >= '0' && name[4] <= '9' &&
           name[5] >= '0' && name[5] <= '9' &&
           strcmp(name + 6, ".PCX") == 0;
}

static int hitl_writer_parse_u32(const char *text, uint32_t *value)
{
    uint32_t parsed = 0u;
    const unsigned char *cursor = (const unsigned char *)text;

    if (text == NULL || value == NULL || *cursor == 0u) {
        return 0;
    }
    while (*cursor != 0u) {
        uint32_t digit;
        if (*cursor < (unsigned char)'0' ||
            *cursor > (unsigned char)'9') {
            return 0;
        }
        digit = (uint32_t)(*cursor - (unsigned char)'0');
        if (parsed > (UINT32_MAX - digit) / UINT32_C(10)) {
            return 0;
        }
        parsed = parsed * UINT32_C(10) + digit;
        ++cursor;
    }
    *value = parsed;
    return 1;
}

static int hitl_writer_parse_crc(const char *text, uint32_t *value)
{
    uint32_t parsed = 0u;
    size_t index;

    if (text == NULL || value == NULL || strlen(text) != 8u) {
        return 0;
    }
    for (index = 0u; index < 8u; ++index) {
        unsigned char character = (unsigned char)text[index];
        uint32_t digit;
        if (character >= (unsigned char)'0' &&
            character <= (unsigned char)'9') {
            digit = (uint32_t)(character - (unsigned char)'0');
        } else if (character >= (unsigned char)'a' &&
                   character <= (unsigned char)'f') {
            digit = (uint32_t)(character - (unsigned char)'a' + 10u);
        } else {
            return 0;
        }
        parsed = (parsed << 4) | digit;
    }
    *value = parsed;
    return 1;
}

static size_t hitl_writer_split_fields(char *line,
                                       char **fields,
                                       size_t capacity)
{
    size_t count = 0u;
    char *cursor;

    if (line == NULL || fields == NULL || capacity == 0u) {
        return 0u;
    }
    fields[count++] = line;
    for (cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (count >= capacity) {
                return capacity + 1u;
            }
            *cursor = '\0';
            fields[count++] = cursor + 1;
        }
    }
    return count;
}

/* Returns 1 for a record, 0 for clean EOF, and -1 for malformed/I/O input. */
static int hitl_writer_read_line(FILE *stream,
                                 char line[HITL_RECORD_CONTENT_MAX + 1u],
                                 size_t *length,
                                 uint32_t *crc_state)
{
    size_t used = 0u;
    int character;

    if (stream == NULL || line == NULL || length == NULL) {
        return -1;
    }
    character = fgetc(stream);
    if (character == EOF) {
        return ferror(stream) != 0 ? -1 : 0;
    }
    for (;;) {
        unsigned char byte = (unsigned char)character;
        if (crc_state != NULL) {
            *crc_state = hitl_crc32_update(*crc_state, &byte, 1u);
        }
        if (character == '\r') {
            unsigned char lf;
            character = fgetc(stream);
            if (character != '\n') {
                return -1;
            }
            lf = (unsigned char)'\n';
            if (crc_state != NULL) {
                *crc_state = hitl_crc32_update(*crc_state, &lf, 1u);
            }
            line[used] = '\0';
            *length = used;
            return used != 0u ? 1 : -1;
        }
        if (character == '\n' || character == 0 ||
            used >= HITL_RECORD_CONTENT_MAX) {
            return -1;
        }
        line[used++] = (char)character;
        character = fgetc(stream);
        if (character == EOF) {
            return -1;
        }
    }
}

static HitlCockpitResult hitl_writer_expect_record(
    FILE *stream,
    uint32_t *crc_state,
    unsigned long *line_number,
    const char *field0,
    const char *field1,
    const char *field2)
{
    char line[HITL_RECORD_CONTENT_MAX + 1u];
    char *fields[HITL_MAX_FIELDS];
    size_t length;
    size_t count;
    int read_result;
    size_t expected_count = field2 != NULL ? 3u : 2u;

    read_result = hitl_writer_read_line(stream, line, &length, crc_state);
    (void)length;
    ++*line_number;
    if (read_result != 1) {
        return read_result == 0 ? HITL_COCKPIT_ERR_FORMAT
                                : HITL_COCKPIT_ERR_IO;
    }
    count = hitl_writer_split_fields(line, fields, HITL_MAX_FIELDS);
    if (count != expected_count || strcmp(fields[0], field0) != 0 ||
        strcmp(fields[1], field1) != 0 ||
        (field2 != NULL && strcmp(fields[2], field2) != 0)) {
        return HITL_COCKPIT_ERR_FORMAT;
    }
    return HITL_COCKPIT_OK;
}

static HitlCockpitResult hitl_writer_scan_preamble(
    FILE *stream,
    const HitlIdentity *identity,
    uint32_t *crc_state,
    unsigned long *line_number)
{
    HitlCockpitResult result;

    result = hitl_writer_expect_record(stream, crc_state, line_number,
                                       "HITL", "1", "0");
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_expect_record(stream, crc_state, line_number,
                                           "RUN", identity->run, NULL);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_expect_record(stream, crc_state, line_number,
                                           "BUILD_HASH",
                                           identity->build_hash, NULL);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_expect_record(stream, crc_state, line_number,
                                           "PLAN_HASH",
                                           identity->plan_hash, NULL);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_expect_record(stream, crc_state, line_number,
                                           "PROFILE_HASH",
                                           identity->profile_hash, NULL);
    }
    return result;
}

static HitlCockpitResult hitl_writer_scan_journal(
    FILE *stream,
    const HitlPlan *plan,
    HitlJournalScan *scan,
    unsigned long *error_line)
{
    char line[HITL_RECORD_CONTENT_MAX + 1u];
    char *fields[HITL_MAX_FIELDS];
    unsigned long line_number = 0u;
    size_t length;
    size_t field_count;
    int read_result;
    HitlCockpitResult result;

    if (stream == NULL || !hitl_writer_plan_valid(plan) || scan == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (error_line != NULL) {
        *error_line = 0u;
    }
    memset(scan, 0, sizeof(*scan));
    scan->crc_state = HITL_CRC_INITIAL;
    scan->next_sequence = 1u;
    clearerr(stream);
    if (fseek(stream, 0L, SEEK_SET) != 0) {
        return HITL_COCKPIT_ERR_IO;
    }
    result = hitl_writer_scan_preamble(stream, &plan->identity,
                                       &scan->crc_state, &line_number);
    if (result != HITL_COCKPIT_OK) {
        if (error_line != NULL) {
            *error_line = line_number;
        }
        return result;
    }

    for (;;) {
        uint32_t sequence;
        uint32_t tick;
        HitlJournalEventType type;
        size_t case_index = 0u;

        read_result = hitl_writer_read_line(stream, line, &length,
                                            &scan->crc_state);
        (void)length;
        if (read_result == 0) {
            break;
        }
        ++line_number;
        if (read_result < 0) {
            if (error_line != NULL) {
                *error_line = line_number;
            }
            return ferror(stream) != 0 ? HITL_COCKPIT_ERR_IO
                                       : HITL_COCKPIT_ERR_FORMAT;
        }
        field_count = hitl_writer_split_fields(line, fields,
                                               HITL_MAX_FIELDS);
        if (field_count != 6u || strcmp(fields[0], "EVENT") != 0 ||
            !hitl_writer_parse_u32(fields[1], &sequence) ||
            !hitl_writer_parse_u32(fields[2], &tick) ||
            !hitl_journal_event_from_name(fields[3], &type) ||
            sequence != scan->next_sequence ||
            (scan->event_count != 0u && tick < scan->last_tick) ||
            !hitl_writer_field_valid(fields[4], 0) ||
            !hitl_writer_field_valid(fields[5], 1) ||
            (strcmp(fields[4], "-") != 0 &&
             !hitl_writer_plan_case_index(plan, fields[4], &case_index))) {
            if (error_line != NULL) {
                *error_line = line_number;
            }
            return HITL_COCKPIT_ERR_FORMAT;
        }
        if (type == HITL_JOURNAL_MANUAL) {
            HitlStatus status;
            if (strcmp(fields[4], "-") == 0 ||
                !hitl_status_from_string(fields[5], &status) ||
                !hitl_writer_manual_status_valid(status)) {
                if (error_line != NULL) {
                    *error_line = line_number;
                }
                return HITL_COCKPIT_ERR_FORMAT;
            }
            scan->manual[case_index] = status;
        } else if (type == HITL_JOURNAL_CAPTURE) {
            if (strcmp(fields[4], "-") == 0 ||
                !hitl_writer_capture_valid(fields[5])) {
                if (error_line != NULL) {
                    *error_line = line_number;
                }
                return HITL_COCKPIT_ERR_FORMAT;
            }
            memcpy(scan->capture[case_index], fields[5],
                   HITL_CAPTURE_LENGTH + 1u);
        }
        scan->event_count += 1u;
        scan->last_tick = tick;
        if (sequence == UINT32_MAX) {
            scan->next_sequence = 0u;
        } else {
            scan->next_sequence = sequence + 1u;
        }
    }
    return HITL_COCKPIT_OK;
}

static HitlCockpitResult hitl_writer_emit(FILE *stream,
                                          uint32_t *crc_state,
                                          const char *format,
                                          ...)
{
    char line[HITL_RECORD_MAX_BYTES + 1u];
    va_list arguments;
    int content_length;
    size_t total_length;

    if (stream == NULL || format == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    va_start(arguments, format);
    content_length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (content_length < 0) {
        return HITL_COCKPIT_ERR_FORMAT;
    }
    if ((size_t)content_length > HITL_RECORD_CONTENT_MAX) {
        return HITL_COCKPIT_ERR_LIMIT;
    }
    line[content_length] = '\r';
    line[content_length + 1] = '\n';
    total_length = (size_t)content_length + 2u;
    if (fwrite(line, 1u, total_length, stream) != total_length) {
        return HITL_COCKPIT_ERR_IO;
    }
    if (crc_state != NULL) {
        *crc_state = hitl_crc32_update(*crc_state, line, total_length);
    }
    return HITL_COCKPIT_OK;
}

static HitlCockpitResult hitl_writer_emit_preamble(
    FILE *stream,
    const HitlIdentity *identity,
    uint32_t *crc_state)
{
    HitlCockpitResult result;

    result = hitl_writer_emit(stream, crc_state, "HITL\t1\t0");
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, crc_state, "RUN\t%s",
                                  identity->run);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, crc_state, "BUILD_HASH\t%s",
                                  identity->build_hash);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, crc_state, "PLAN_HASH\t%s",
                                  identity->plan_hash);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, crc_state, "PROFILE_HASH\t%s",
                                  identity->profile_hash);
    }
    return result;
}

HitlCockpitResult hitl_journal_attach(HitlJournal *journal,
                                      FILE *stream,
                                      const HitlPlan *plan,
                                      unsigned long *error_line)
{
    HitlJournal candidate;
    HitlJournalScan scan;
    HitlCockpitResult result;
    long size;

    if (journal == NULL || stream == NULL ||
        !hitl_writer_plan_valid(plan)) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (error_line != NULL) {
        *error_line = 0u;
    }
    memset(&candidate, 0, sizeof(candidate));
    clearerr(stream);
    if (fseek(stream, 0L, SEEK_END) != 0 || (size = ftell(stream)) < 0L) {
        return HITL_COCKPIT_ERR_IO;
    }
    if (size == 0L) {
        candidate.crc_state = HITL_CRC_INITIAL;
        if (fseek(stream, 0L, SEEK_SET) != 0) {
            return HITL_COCKPIT_ERR_IO;
        }
        result = hitl_writer_emit_preamble(stream, &plan->identity,
                                            &candidate.crc_state);
        if (result != HITL_COCKPIT_OK || fflush(stream) != 0) {
            return result != HITL_COCKPIT_OK ? result
                                             : HITL_COCKPIT_ERR_IO;
        }
        candidate.next_sequence = 1u;
    } else {
        result = hitl_writer_scan_journal(stream, plan, &scan, error_line);
        if (result != HITL_COCKPIT_OK) {
            return result;
        }
        if (scan.next_sequence == 0u) {
            return HITL_COCKPIT_ERR_LIMIT;
        }
        candidate.crc_state = scan.crc_state;
        candidate.event_count = scan.event_count;
        candidate.next_sequence = scan.next_sequence;
        candidate.last_tick = scan.last_tick;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        return HITL_COCKPIT_ERR_IO;
    }
    candidate.stream = stream;
    candidate.plan = plan;
    candidate.identity = plan->identity;
    candidate.initialized = 1u;
    *journal = candidate;
    return HITL_COCKPIT_OK;
}

void hitl_journal_detach(HitlJournal *journal)
{
    if (journal != NULL) {
        memset(journal, 0, sizeof(*journal));
    }
}

HitlCockpitResult hitl_journal_append(
    HitlJournal *journal,
    HitlJournalEventType type,
    uint32_t tick,
    const char *case_id,
    const char *payload,
    HitlInputAuthority authority)
{
    const char *type_name = hitl_journal_event_name(type);
    HitlCockpitResult result;

    if (journal == NULL || case_id == NULL || payload == NULL ||
        type_name == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (journal->initialized == 0u || journal->stream == NULL ||
        journal->plan == NULL || journal->next_sequence == 0u) {
        return HITL_COCKPIT_ERR_STATE;
    }
    if (!hitl_writer_plan_valid(journal->plan) ||
        !hitl_identity_equal(&journal->identity,
                             &journal->plan->identity)) {
        return HITL_COCKPIT_ERR_IDENTITY;
    }
    if (!hitl_writer_field_valid(case_id, 0) ||
        !hitl_writer_field_valid(payload, 1)) {
        return HITL_COCKPIT_ERR_FORMAT;
    }
    if (journal->event_count != 0u && tick < journal->last_tick) {
        return HITL_COCKPIT_ERR_FORMAT;
    }
    if (strcmp(case_id, "-") != 0 &&
        !hitl_writer_plan_case_index(journal->plan, case_id, NULL)) {
        return HITL_COCKPIT_ERR_FORMAT;
    }
    if (type == HITL_JOURNAL_MANUAL) {
        HitlStatus status;
        if (authority != HITL_INPUT_LIVE_DOS || strcmp(case_id, "-") == 0 ||
            !hitl_status_from_string(payload, &status) ||
            !hitl_writer_manual_status_valid(status)) {
            return HITL_COCKPIT_ERR_AUTHORITY;
        }
    }
    if (type == HITL_JOURNAL_CAPTURE &&
        (strcmp(case_id, "-") == 0 ||
         !hitl_writer_capture_valid(payload))) {
        return HITL_COCKPIT_ERR_FORMAT;
    }

    clearerr(journal->stream);
    if (fseek(journal->stream, 0L, SEEK_END) != 0) {
        return HITL_COCKPIT_ERR_IO;
    }
    result = hitl_writer_emit(journal->stream, &journal->crc_state,
                              "EVENT\t%lu\t%lu\t%s\t%s\t%s",
                              (unsigned long)journal->next_sequence,
                              (unsigned long)tick, type_name,
                              case_id, payload);
    if (result != HITL_COCKPIT_OK || fflush(journal->stream) != 0) {
        journal->initialized = 0u;
        return result != HITL_COCKPIT_OK ? result : HITL_COCKPIT_ERR_IO;
    }
    journal->event_count += 1u;
    journal->last_tick = tick;
    if (journal->next_sequence == UINT32_MAX) {
        journal->next_sequence = 0u;
    } else {
        journal->next_sequence += 1u;
    }
    return HITL_COCKPIT_OK;
}

HitlCockpitResult hitl_journal_flush(HitlJournal *journal)
{
    if (journal == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (journal->initialized == 0u || journal->stream == NULL) {
        return HITL_COCKPIT_ERR_STATE;
    }
    if (fflush(journal->stream) != 0) {
        journal->initialized = 0u;
        return HITL_COCKPIT_ERR_IO;
    }
    return HITL_COCKPIT_OK;
}

uint32_t hitl_journal_crc32(const HitlJournal *journal)
{
    if (journal == NULL || journal->initialized == 0u) {
        return 0u;
    }
    return journal->crc_state ^ HITL_CRC_XOR_OUT;
}

uint32_t hitl_journal_event_count(const HitlJournal *journal)
{
    return journal != NULL && journal->initialized != 0u
               ? journal->event_count : 0u;
}

HitlCockpitResult hitl_journal_verify(FILE *stream,
                                      const HitlPlan *plan,
                                      uint32_t *crc32,
                                      uint32_t *record_count,
                                      unsigned long *error_line)
{
    HitlJournalScan scan;
    HitlCockpitResult result;

    if (stream == NULL || plan == NULL || crc32 == NULL ||
        record_count == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    *crc32 = 0u;
    *record_count = 0u;
    result = hitl_writer_scan_journal(stream, plan, &scan, error_line);
    if (result != HITL_COCKPIT_OK) {
        return result;
    }
    *crc32 = scan.crc_state ^ HITL_CRC_XOR_OUT;
    *record_count = scan.event_count;
    return HITL_COCKPIT_OK;
}

HitlCockpitResult hitl_summary_write(FILE *stream,
                                     const HitlCockpit *cockpit,
                                     const HitlJournal *journal)
{
    HitlJournalScan journal_scan;
    uint32_t counts[4] = { 0u, 0u, 0u, 0u };
    size_t index;
    HitlCockpitResult result;

    if (stream == NULL || cockpit == NULL || journal == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (cockpit->initialized == 0u || journal->initialized == 0u ||
        !hitl_writer_plan_valid(cockpit->plan) ||
        !hitl_identity_equal(&cockpit->plan->identity,
                             &journal->identity)) {
        return HITL_COCKPIT_ERR_STATE;
    }
    if (fflush(journal->stream) != 0) {
        return HITL_COCKPIT_ERR_IO;
    }
    result = hitl_writer_scan_journal(journal->stream, cockpit->plan,
                                      &journal_scan, NULL);
    if (result != HITL_COCKPIT_OK ||
        journal_scan.crc_state != journal->crc_state ||
        journal_scan.event_count != journal->event_count) {
        return result != HITL_COCKPIT_OK ? result
                                         : HITL_COCKPIT_ERR_CRC;
    }
    result = hitl_writer_emit_preamble(stream,
                                        &cockpit->plan->identity, NULL);
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, NULL, "PROFILE\t%s",
                                  cockpit->plan->profile.id);
    }
    for (index = 0u; result == HITL_COCKPIT_OK &&
         index < cockpit->plan->case_count; ++index) {
        const HitlCaseState *state = &cockpit->case_states[index];
        HitlCombinedResult combined;
        if (!hitl_writer_status_valid(state->auto_status) ||
            !hitl_writer_status_valid(state->manual_status) ||
            !hitl_identity_equal(&state->identity,
                                 &cockpit->plan->identity) ||
            strcmp(state->case_id,
                   cockpit->plan->cases[index].id) != 0 ||
            (state->manual_status != HITL_STATUS_UNRUN &&
             state->manual_evidence_current == 0)) {
            return HITL_COCKPIT_ERR_IDENTITY;
        }
        combined = hitl_cockpit_combined(cockpit, index);
        if (!hitl_writer_status_valid(combined.status)) {
            return HITL_COCKPIT_ERR_FORMAT;
        }
        if (combined.status == HITL_STATUS_PASS) {
            counts[0] += 1u;
        } else if (combined.status == HITL_STATUS_FAIL) {
            counts[1] += 1u;
        } else if (combined.status == HITL_STATUS_BLOCKED) {
            counts[2] += 1u;
        } else {
            counts[3] += 1u;
        }
        result = hitl_writer_emit(stream, NULL,
            "RESULT\t%s\t%s\t%s\t%s\t%s",
            cockpit->plan->cases[index].id,
            hitl_status_string(state->auto_status),
            hitl_status_string(state->manual_status), combined.code,
            journal_scan.capture[index][0] != '\0'
                ? journal_scan.capture[index] : "-");
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, NULL,
            "SUMMARY\t%lu\t%lu\t%lu\t%lu",
            (unsigned long)counts[0], (unsigned long)counts[1],
            (unsigned long)counts[2], (unsigned long)counts[3]);
    }
    if (result == HITL_COCKPIT_OK) {
        result = hitl_writer_emit(stream, NULL, "JOURNAL\t%08lx\t%lu",
            (unsigned long)hitl_journal_crc32(journal),
            (unsigned long)hitl_journal_event_count(journal));
    }
    if (result == HITL_COCKPIT_OK && fflush(stream) != 0) {
        result = HITL_COCKPIT_ERR_IO;
    }
    return result;
}

static void hitl_writer_expected_state(HitlCaseState *state,
                                       const HitlPlan *plan,
                                       const HitlAutoEvidence *evidence,
                                       size_t index)
{
    int auto_shape_valid = hitl_writer_auto_shape_valid(plan, evidence);

    hitl_case_state_init(state);
    state->identity = plan->identity;
    memcpy(state->case_id, plan->cases[index].id,
           strlen(plan->cases[index].id) + 1u);
    if (auto_shape_valid) {
        state->auto_status = evidence->records[index].status;
    }
    if (auto_shape_valid && evidence->identity_current != 0 &&
        hitl_identity_equal(&plan->identity, &evidence->identity)) {
        (void)hitl_case_state_apply_auto(state, plan, evidence, index);
    }
}

HitlCockpitResult hitl_summary_verify(FILE *stream,
                                      const HitlPlan *plan,
                                      const HitlAutoEvidence *auto_evidence,
                                      FILE *journal_stream,
                                      unsigned long *error_line)
{
    HitlJournalScan journal;
    uint32_t ignored_crc = HITL_CRC_INITIAL;
    uint32_t expected_counts[4] = { 0u, 0u, 0u, 0u };
    char line[HITL_RECORD_CONTENT_MAX + 1u];
    char *fields[HITL_MAX_FIELDS];
    unsigned long line_number = 0u;
    size_t length;
    size_t field_count;
    size_t index;
    int read_result;
    HitlCockpitResult result;

    if (stream == NULL || !hitl_writer_plan_valid(plan) ||
        auto_evidence == NULL || journal_stream == NULL) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    if (error_line != NULL) {
        *error_line = 0u;
    }
    result = hitl_writer_scan_journal(journal_stream, plan, &journal,
                                      error_line);
    if (result != HITL_COCKPIT_OK) {
        return result;
    }
    clearerr(stream);
    if (fseek(stream, 0L, SEEK_SET) != 0) {
        return HITL_COCKPIT_ERR_IO;
    }
    result = hitl_writer_scan_preamble(stream, &plan->identity,
                                       &ignored_crc, &line_number);
    if (result != HITL_COCKPIT_OK) {
        if (error_line != NULL) {
            *error_line = line_number;
        }
        return result;
    }
    result = hitl_writer_expect_record(stream, &ignored_crc, &line_number,
                                       "PROFILE", plan->profile.id, NULL);
    if (result != HITL_COCKPIT_OK) {
        if (error_line != NULL) {
            *error_line = line_number;
        }
        return result;
    }

    for (index = 0u; index < plan->case_count; ++index) {
        HitlCaseState state;
        HitlCombinedResult combined;
        const char *capture;
        HitlStatus summary_manual;

        read_result = hitl_writer_read_line(stream, line, &length,
                                            &ignored_crc);
        (void)length;
        ++line_number;
        field_count = read_result == 1
            ? hitl_writer_split_fields(line, fields, HITL_MAX_FIELDS) : 0u;
        hitl_writer_expected_state(&state, plan, auto_evidence, index);
        if (read_result != 1 || field_count != 6u ||
            !hitl_status_from_string(fields[3], &summary_manual) ||
            !hitl_writer_status_valid(summary_manual)) {
            if (error_line != NULL) {
                *error_line = line_number;
            }
            return read_result < 0 && ferror(stream) != 0
                       ? HITL_COCKPIT_ERR_IO : HITL_COCKPIT_ERR_FORMAT;
        }
        state.manual_status = summary_manual;
        state.manual_evidence_current =
            summary_manual != HITL_STATUS_UNRUN ? 1 : 0;
        combined = hitl_case_combined(plan, auto_evidence, index, &state);
        capture = journal.capture[index][0] != '\0'
                      ? journal.capture[index] : "-";
        if (strcmp(fields[0], "RESULT") != 0 ||
            strcmp(fields[1], plan->cases[index].id) != 0 ||
            strcmp(fields[2], hitl_status_string(state.auto_status)) != 0 ||
            strcmp(fields[4], combined.code) != 0 ||
            strcmp(fields[5], capture) != 0) {
            if (error_line != NULL) {
                *error_line = line_number;
            }
            return read_result < 0 && ferror(stream) != 0
                       ? HITL_COCKPIT_ERR_IO : HITL_COCKPIT_ERR_FORMAT;
        }
        if (combined.status == HITL_STATUS_PASS) {
            expected_counts[0] += 1u;
        } else if (combined.status == HITL_STATUS_FAIL) {
            expected_counts[1] += 1u;
        } else if (combined.status == HITL_STATUS_BLOCKED) {
            expected_counts[2] += 1u;
        } else {
            expected_counts[3] += 1u;
        }
    }

    read_result = hitl_writer_read_line(stream, line, &length, &ignored_crc);
    (void)length;
    ++line_number;
    field_count = read_result == 1
        ? hitl_writer_split_fields(line, fields, HITL_MAX_FIELDS) : 0u;
    if (read_result != 1 || field_count != 5u ||
        strcmp(fields[0], "SUMMARY") != 0) {
        result = HITL_COCKPIT_ERR_FORMAT;
        goto fail;
    }
    for (index = 0u; index < 4u; ++index) {
        uint32_t parsed;
        if (!hitl_writer_parse_u32(fields[index + 1u], &parsed) ||
            parsed != expected_counts[index]) {
            result = HITL_COCKPIT_ERR_FORMAT;
            goto fail;
        }
    }

    read_result = hitl_writer_read_line(stream, line, &length, &ignored_crc);
    (void)length;
    ++line_number;
    field_count = read_result == 1
        ? hitl_writer_split_fields(line, fields, HITL_MAX_FIELDS) : 0u;
    if (read_result != 1 || field_count != 3u ||
        strcmp(fields[0], "JOURNAL") != 0) {
        result = HITL_COCKPIT_ERR_FORMAT;
        goto fail;
    }
    {
        uint32_t parsed_crc;
        uint32_t parsed_count;
        if (!hitl_writer_parse_crc(fields[1], &parsed_crc) ||
            !hitl_writer_parse_u32(fields[2], &parsed_count) ||
            parsed_crc != (journal.crc_state ^ HITL_CRC_XOR_OUT) ||
            parsed_count != journal.event_count) {
            result = HITL_COCKPIT_ERR_CRC;
            goto fail;
        }
    }
    read_result = hitl_writer_read_line(stream, line, &length, &ignored_crc);
    if (read_result != 0) {
        ++line_number;
        result = read_result < 0 && ferror(stream) != 0
                     ? HITL_COCKPIT_ERR_IO : HITL_COCKPIT_ERR_FORMAT;
        goto fail;
    }
    return HITL_COCKPIT_OK;

fail:
    if (error_line != NULL) {
        *error_line = line_number;
    }
    return result;
}

static HitlPathResult hitl_default_remove(void *user, const char *path)
{
    (void)user;
    if (remove(path) == 0) {
        return HITL_PATH_OK;
    }
    return errno == ENOENT ? HITL_PATH_NOT_FOUND : HITL_PATH_ERROR;
}

static HitlPathResult hitl_default_rename(void *user,
                                         const char *old_path,
                                         const char *new_path)
{
    (void)user;
    if (rename(old_path, new_path) == 0) {
        return HITL_PATH_OK;
    }
    return errno == ENOENT ? HITL_PATH_NOT_FOUND : HITL_PATH_ERROR;
}

HitlCockpitResult hitl_summary_replace(
    const char *new_path,
    const char *out_path,
    const char *old_path,
    const HitlPathOps *ops,
    HitlReplaceOutcome *outcome)
{
    HitlPathOps selected;
    HitlPathResult path_result;
    HitlReplaceOutcome candidate;

    if (new_path == NULL || out_path == NULL || old_path == NULL ||
        outcome == NULL || *new_path == '\0' || *out_path == '\0' ||
        *old_path == '\0' || strcmp(new_path, out_path) == 0 ||
        strcmp(new_path, old_path) == 0 || strcmp(out_path, old_path) == 0) {
        return HITL_COCKPIT_ERR_ARGUMENT;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (ops == NULL) {
        selected.remove_path = hitl_default_remove;
        selected.rename_path = hitl_default_rename;
        selected.user = NULL;
    } else {
        if (ops->remove_path == NULL || ops->rename_path == NULL) {
            return HITL_COCKPIT_ERR_ARGUMENT;
        }
        selected = *ops;
    }

    path_result = selected.remove_path(selected.user, old_path);
    if (path_result == HITL_PATH_ERROR) {
        *outcome = candidate;
        return HITL_COCKPIT_ERR_IO;
    }

    path_result = selected.rename_path(selected.user, out_path, old_path);
    if (path_result == HITL_PATH_ERROR) {
        *outcome = candidate;
        return HITL_COCKPIT_ERR_IO;
    }
    if (path_result == HITL_PATH_OK) {
        candidate.prior_output_moved = 1u;
        candidate.prior_output_recoverable = 1u;
        candidate.old_file_retained = 1u;
    }

    path_result = selected.rename_path(selected.user, new_path, out_path);
    if (path_result != HITL_PATH_OK) {
        if (candidate.prior_output_moved != 0u) {
            HitlPathResult rollback = selected.rename_path(
                selected.user, old_path, out_path);
            if (rollback == HITL_PATH_OK) {
                candidate.prior_output_moved = 0u;
                candidate.old_file_retained = 0u;
                candidate.prior_output_recoverable = 1u;
                *outcome = candidate;
                return HITL_COCKPIT_ERR_IO;
            }
            *outcome = candidate;
            return HITL_COCKPIT_ERR_RECOVERY;
        }
        *outcome = candidate;
        return HITL_COCKPIT_ERR_IO;
    }
    candidate.output_committed = 1u;

    if (candidate.prior_output_moved != 0u) {
        path_result = selected.remove_path(selected.user, old_path);
        if (path_result == HITL_PATH_OK ||
            path_result == HITL_PATH_NOT_FOUND) {
            candidate.old_file_retained = 0u;
            candidate.prior_output_moved = 0u;
        }
    }
    *outcome = candidate;
    return HITL_COCKPIT_OK;
}
