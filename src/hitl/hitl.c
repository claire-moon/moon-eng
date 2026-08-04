#include "moon/hitl.h"

#include <limits.h>
#include <string.h>

static int hitl_is_extension_record(const HitlRecord *record);

static int hitl_is_upper_alnum(unsigned char value)
{
    return (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
           (value >= (unsigned char)'0' && value <= (unsigned char)'9');
}

static int hitl_is_identifier_char(unsigned char value)
{
    return hitl_is_upper_alnum(value) || value == (unsigned char)'.' ||
           value == (unsigned char)'_' || value == (unsigned char)'-';
}

static int hitl_is_printable_byte(unsigned char value)
{
    return value >= 0x20u && value <= 0xfeu;
}

static int hitl_status_valid(HitlStatus status)
{
    return status == HITL_STATUS_PASS || status == HITL_STATUS_FAIL ||
           status == HITL_STATUS_BLOCKED || status == HITL_STATUS_UNRUN;
}

static int hitl_copy_checked(char *destination,
                             size_t destination_size,
                             const char *source,
                             size_t minimum_length,
                             size_t maximum_length,
                             int (*character_check)(unsigned char))
{
    size_t length;
    size_t index;

    if (destination == NULL || destination_size == 0u || source == NULL) {
        return 0;
    }
    length = strlen(source);
    if (length < minimum_length || length > maximum_length ||
        length + 1u > destination_size) {
        return 0;
    }
    for (index = 0u; index < length; ++index) {
        if (!character_check((unsigned char)source[index])) {
            return 0;
        }
    }
    memcpy(destination, source, length + 1u);
    return 1;
}

static int hitl_is_lower_hex(unsigned char value)
{
    return (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
           (value >= (unsigned char)'a' && value <= (unsigned char)'f');
}

static int hitl_parse_u32(const char *text, uint32_t *value)
{
    uint32_t parsed = 0u;
    size_t index;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    for (index = 0u; text[index] != '\0'; ++index) {
        uint32_t digit;

        if (text[index] < '0' || text[index] > '9') {
            return 0;
        }
        digit = (uint32_t)(text[index] - '0');
        if (parsed > (UINT32_MAX - digit) / UINT32_C(10)) {
            return 0;
        }
        parsed = parsed * UINT32_C(10) + digit;
    }
    *value = parsed;
    return 1;
}

static int hitl_parse_small_unsigned(const char *text,
                                     unsigned int maximum,
                                     unsigned int *value)
{
    uint32_t parsed;

    if (!hitl_parse_u32(text, &parsed) || parsed > maximum) {
        return 0;
    }
    *value = (unsigned int)parsed;
    return 1;
}

const char *hitl_status_string(HitlStatus status)
{
    switch (status) {
    case HITL_STATUS_PASS: return "PASS";
    case HITL_STATUS_FAIL: return "FAIL";
    case HITL_STATUS_BLOCKED: return "BLOCKED";
    case HITL_STATUS_UNRUN: return "UNRUN";
    case HITL_STATUS_INVALID: return "INVALID";
    default: return "INVALID";
    }
}

int hitl_status_from_string(const char *text, HitlStatus *status)
{
    if (text == NULL || status == NULL) {
        return 0;
    }
    if (strcmp(text, "PASS") == 0) {
        *status = HITL_STATUS_PASS;
    } else if (strcmp(text, "FAIL") == 0) {
        *status = HITL_STATUS_FAIL;
    } else if (strcmp(text, "BLOCKED") == 0) {
        *status = HITL_STATUS_BLOCKED;
    } else if (strcmp(text, "UNRUN") == 0) {
        *status = HITL_STATUS_UNRUN;
    } else {
        *status = HITL_STATUS_INVALID;
        return 0;
    }
    return 1;
}

const char *hitl_result_string(HitlResult result)
{
    switch (result) {
    case HITL_OK: return "ok";
    case HITL_END: return "end of file";
    case HITL_ERR_ARGUMENT: return "invalid argument";
    case HITL_ERR_IO: return "I/O error";
    case HITL_ERR_RECORD_TOO_LONG: return "HITL record too long";
    case HITL_ERR_FIELD_COUNT: return "invalid HITL field count";
    case HITL_ERR_FORMAT: return "invalid HITL format";
    case HITL_ERR_VERSION: return "unsupported HITL version";
    case HITL_ERR_UNKNOWN_RECORD: return "unknown HITL record";
    case HITL_ERR_UNKNOWN_CASE: return "unknown HITL case";
    case HITL_ERR_DUPLICATE: return "duplicate HITL record or case";
    case HITL_ERR_MISSING: return "missing HITL record or case";
    case HITL_ERR_LIMIT: return "HITL implementation limit exceeded";
    case HITL_ERR_STALE: return "stale HITL evidence";
    default: return "unknown HITL error";
    }
}

void hitl_reader_init(HitlReader *reader, FILE *stream)
{
    if (reader == NULL) {
        return;
    }
    reader->stream = stream;
    reader->line_number = 0u;
}

HitlResult hitl_reader_next(HitlReader *reader, HitlRecord *record)
{
    if (reader == NULL || record == NULL || reader->stream == NULL) {
        return HITL_ERR_ARGUMENT;
    }

    for (;;) {
        size_t length = 0u;
        int saw_byte = 0;

        memset(record, 0, sizeof(*record));
        record->line_number = reader->line_number + 1u;

        for (;;) {
            int input = fgetc(reader->stream);

            if (input == EOF) {
                if (ferror(reader->stream)) {
                    return HITL_ERR_IO;
                }
                if (!saw_byte) {
                    return HITL_END;
                }
                ++reader->line_number;
                break;
            }
            saw_byte = 1;

            if (input == '\n') {
                ++reader->line_number;
                break;
            }
            if (input == '\r') {
                int following = fgetc(reader->stream);
                if (following != '\n') {
                    return ferror(reader->stream) ? HITL_ERR_IO :
                                                   HITL_ERR_FORMAT;
                }
                ++reader->line_number;
                break;
            }
            if ((unsigned int)input < 0x20u && input != '\t') {
                return HITL_ERR_FORMAT;
            }
            if ((unsigned int)input > 0xfeu) {
                return HITL_ERR_FORMAT;
            }
            if (length >= HITL_RECORD_CONTENT_MAX) {
                return HITL_ERR_RECORD_TOO_LONG;
            }
            record->storage[length++] = (char)(unsigned char)input;
        }

        record->storage[length] = '\0';
        record->line_number = reader->line_number;
        if (length == 0u || record->storage[0] == '#') {
            continue;
        }

        record->field_count = 1u;
        record->fields[0] = record->storage;
        {
            size_t index;
            for (index = 0u; index < length; ++index) {
                if (record->storage[index] == '\t') {
                    if (record->field_count >= HITL_MAX_FIELDS) {
                        /* Optional extensions are opaque to older readers. */
                        if (hitl_is_extension_record(record)) {
                            return HITL_OK;
                        }
                        return HITL_ERR_FIELD_COUNT;
                    }
                    record->storage[index] = '\0';
                    record->fields[record->field_count++] =
                        record->storage + index + 1u;
                }
            }
        }
        return HITL_OK;
    }
}

static int hitl_is_extension_record(const HitlRecord *record)
{
    return record->field_count != 0u && record->fields[0][0] == 'X' &&
           record->fields[0][1] == '-' && record->fields[0][2] != '\0';
}

static HitlResult hitl_read_header(HitlReader *reader,
                                   unsigned int *major,
                                   unsigned int *minor,
                                   unsigned long *error_line)
{
    HitlRecord record;
    HitlResult result;

    memset(&record, 0, sizeof(record));
    result = hitl_reader_next(reader, &record);

    if (result != HITL_OK) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return result == HITL_END ? HITL_ERR_MISSING : result;
    }
    if (record.field_count != 3u || strcmp(record.fields[0], "HITL") != 0) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return record.field_count != 3u ? HITL_ERR_FIELD_COUNT :
                                         HITL_ERR_FORMAT;
    }
    if (!hitl_parse_small_unsigned(record.fields[1], UINT_MAX, major) ||
        !hitl_parse_small_unsigned(record.fields[2], UINT_MAX, minor)) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return HITL_ERR_FORMAT;
    }
    if (*major != HITL_FORMAT_MAJOR) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return HITL_ERR_VERSION;
    }
    return HITL_OK;
}

static HitlResult hitl_copy_run(char destination[HITL_RUN_MAX + 1u],
                                const HitlRecord *record)
{
    if (record->field_count != 2u) {
        return HITL_ERR_FIELD_COUNT;
    }
    return hitl_copy_checked(destination, HITL_RUN_MAX + 1u,
                             record->fields[1], 1u, HITL_RUN_MAX,
                             hitl_is_upper_alnum) ? HITL_OK : HITL_ERR_FORMAT;
}

static HitlResult hitl_copy_hash(
    char destination[HITL_SHA256_HEX_LENGTH + 1u],
    const HitlRecord *record)
{
    if (record->field_count != 2u) {
        return HITL_ERR_FIELD_COUNT;
    }
    return hitl_copy_checked(destination, HITL_SHA256_HEX_LENGTH + 1u,
                             record->fields[1], HITL_SHA256_HEX_LENGTH,
                             HITL_SHA256_HEX_LENGTH,
                             hitl_is_lower_hex) ? HITL_OK : HITL_ERR_FORMAT;
}

static HitlResult hitl_parse_profile(HitlProfile *profile,
                                     const HitlRecord *record)
{
    if (record->field_count != 6u) {
        return HITL_ERR_FIELD_COUNT;
    }
    if (!hitl_copy_checked(profile->id, sizeof(profile->id),
                           record->fields[1], 1u, HITL_PROFILE_ID_MAX,
                           hitl_is_identifier_char) ||
        !hitl_copy_checked(profile->operating_system,
                           sizeof(profile->operating_system),
                           record->fields[2], 1u, HITL_PROFILE_TEXT_MAX,
                           hitl_is_printable_byte) ||
        !hitl_copy_checked(profile->cpu, sizeof(profile->cpu),
                           record->fields[3], 1u, HITL_PROFILE_TEXT_MAX,
                           hitl_is_printable_byte) ||
        !hitl_copy_checked(profile->memory, sizeof(profile->memory),
                           record->fields[4], 1u, HITL_PROFILE_TEXT_MAX,
                           hitl_is_printable_byte) ||
        !hitl_copy_checked(profile->blaster, sizeof(profile->blaster),
                           record->fields[5], 1u, HITL_PROFILE_TEXT_MAX,
                           hitl_is_printable_byte)) {
        return HITL_ERR_FORMAT;
    }
    return HITL_OK;
}

static HitlResult hitl_parse_case(HitlCase *definition,
                                  const HitlRecord *record)
{
    unsigned int auto_required;
    unsigned int manual_required;

    if (record->field_count != 5u) {
        return HITL_ERR_FIELD_COUNT;
    }
    if (!hitl_copy_checked(definition->id, sizeof(definition->id),
                           record->fields[1], 1u, HITL_CASE_ID_MAX,
                           hitl_is_identifier_char) ||
        !hitl_parse_small_unsigned(record->fields[2], 1u,
                                   &auto_required) ||
        !hitl_parse_small_unsigned(record->fields[3], 1u,
                                   &manual_required) ||
        (auto_required == 0u && manual_required == 0u) ||
        !hitl_copy_checked(definition->title, sizeof(definition->title),
                           record->fields[4], 1u, HITL_CASE_TITLE_MAX,
                           hitl_is_printable_byte)) {
        return HITL_ERR_FORMAT;
    }
    definition->auto_required = auto_required;
    definition->manual_required = manual_required;
    return HITL_OK;
}

static int hitl_case_index(const HitlPlan *plan, const char *case_id)
{
    size_t index;

    for (index = 0u; index < plan->case_count; ++index) {
        if (strcmp(plan->cases[index].id, case_id) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static int hitl_known_plan_record(const char *name)
{
    return strcmp(name, "RUN") == 0 ||
           strcmp(name, "BUILD_HASH") == 0 ||
           strcmp(name, "PLAN_HASH") == 0 ||
           strcmp(name, "PROFILE_HASH") == 0 ||
           strcmp(name, "PROFILE") == 0 ||
           strcmp(name, "CASE") == 0;
}

static void hitl_set_error_line(unsigned long *error_line,
                                const HitlRecord *record)
{
    if (error_line != NULL) {
        *error_line = record->line_number;
    }
}

static HitlResult hitl_plan_failure(HitlPlan *plan, HitlResult result)
{
    memset(plan, 0, sizeof(*plan));
    return result;
}

HitlResult hitl_plan_read(FILE *stream,
                          HitlPlan *plan,
                          unsigned long *error_line)
{
    HitlReader reader;
    HitlRecord record;
    HitlResult result;
    unsigned int stage = 0u;

    if (error_line != NULL) {
        *error_line = 0u;
    }
    if (plan == NULL) {
        return HITL_ERR_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    if (stream == NULL) {
        return HITL_ERR_ARGUMENT;
    }
    memset(&record, 0, sizeof(record));
    hitl_reader_init(&reader, stream);
    result = hitl_read_header(&reader, &plan->format_major,
                              &plan->format_minor, error_line);
    if (result != HITL_OK) {
        return hitl_plan_failure(plan, result);
    }

    while ((result = hitl_reader_next(&reader, &record)) == HITL_OK) {
        const char *name = record.fields[0];

        if (hitl_is_extension_record(&record)) {
            continue;
        }
        if (strcmp(name, "RUN") == 0) {
            if (stage > 0u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(plan, HITL_ERR_DUPLICATE);
            }
            result = hitl_copy_run(plan->identity.run, &record);
            stage = 1u;
        } else if (strcmp(name, "BUILD_HASH") == 0) {
            if (stage != 1u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(
                    plan, stage > 1u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(plan->identity.build_hash, &record);
            stage = 2u;
        } else if (strcmp(name, "PLAN_HASH") == 0) {
            if (stage != 2u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(
                    plan, stage > 2u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(plan->identity.plan_hash, &record);
            stage = 3u;
        } else if (strcmp(name, "PROFILE_HASH") == 0) {
            if (stage != 3u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(
                    plan, stage > 3u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(plan->identity.profile_hash, &record);
            stage = 4u;
        } else if (strcmp(name, "PROFILE") == 0) {
            if (stage != 4u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(
                    plan, stage > 4u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_parse_profile(&plan->profile, &record);
            stage = 5u;
        } else if (strcmp(name, "CASE") == 0) {
            size_t index;
            HitlCase definition;

            if (stage != 5u) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(plan, HITL_ERR_FORMAT);
            }
            if (plan->case_count >= HITL_MAX_CASES) {
                hitl_set_error_line(error_line, &record);
                return hitl_plan_failure(plan, HITL_ERR_LIMIT);
            }
            memset(&definition, 0, sizeof(definition));
            result = hitl_parse_case(&definition, &record);
            if (result == HITL_OK) {
                for (index = 0u; index < plan->case_count; ++index) {
                    if (strcmp(plan->cases[index].id, definition.id) == 0) {
                        hitl_set_error_line(error_line, &record);
                        return hitl_plan_failure(plan, HITL_ERR_DUPLICATE);
                    }
                }
                plan->cases[plan->case_count++] = definition;
            }
        } else {
            hitl_set_error_line(error_line, &record);
            return hitl_plan_failure(
                plan, hitl_known_plan_record(name) ? HITL_ERR_FORMAT :
                                                     HITL_ERR_UNKNOWN_RECORD);
        }

        if (result != HITL_OK) {
            hitl_set_error_line(error_line, &record);
            return hitl_plan_failure(plan, result);
        }
    }
    if (result != HITL_END) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return hitl_plan_failure(plan, result);
    }
    if (stage != 5u || plan->case_count == 0u) {
        if (error_line != NULL) {
            *error_line = reader.line_number + 1u;
        }
        return hitl_plan_failure(plan, HITL_ERR_MISSING);
    }
    return HITL_OK;
}

int hitl_identity_equal(const HitlIdentity *left,
                        const HitlIdentity *right)
{
    if (left == NULL || right == NULL) {
        return 0;
    }
    return strcmp(left->run, right->run) == 0 &&
           strcmp(left->build_hash, right->build_hash) == 0 &&
           strcmp(left->plan_hash, right->plan_hash) == 0 &&
           strcmp(left->profile_hash, right->profile_hash) == 0;
}

static HitlResult hitl_parse_auto_record(HitlAutoRecord *output,
                                         const HitlRecord *record)
{
    uint32_t elapsed_ms;

    if (record->field_count != 6u) {
        return HITL_ERR_FIELD_COUNT;
    }
    if (!hitl_copy_checked(output->case_id, sizeof(output->case_id),
                           record->fields[1], 1u, HITL_CASE_ID_MAX,
                           hitl_is_identifier_char) ||
        !hitl_status_from_string(record->fields[2], &output->status) ||
        !hitl_copy_checked(output->result_code,
                           sizeof(output->result_code),
                           record->fields[3], 1u, HITL_RESULT_CODE_MAX,
                           hitl_is_identifier_char) ||
        !hitl_parse_u32(record->fields[4], &elapsed_ms) ||
        !hitl_copy_checked(output->detail, sizeof(output->detail),
                           record->fields[5], 0u, HITL_DETAIL_MAX,
                           hitl_is_printable_byte)) {
        return HITL_ERR_FORMAT;
    }
    output->elapsed_ms = elapsed_ms;
    return HITL_OK;
}

static int hitl_known_auto_record(const char *name)
{
    return strcmp(name, "RUN") == 0 ||
           strcmp(name, "BUILD_HASH") == 0 ||
           strcmp(name, "PLAN_HASH") == 0 ||
           strcmp(name, "PROFILE_HASH") == 0 ||
           strcmp(name, "AUTO") == 0;
}

static HitlResult hitl_auto_failure(HitlAutoEvidence *evidence,
                                    HitlResult result)
{
    memset(evidence, 0, sizeof(*evidence));
    return result;
}

HitlResult hitl_auto_read(FILE *stream,
                          const HitlPlan *plan,
                          HitlAutoEvidence *evidence,
                          unsigned long *error_line)
{
    HitlReader reader;
    HitlRecord record;
    HitlResult result;
    unsigned int stage = 0u;
    unsigned char present[HITL_MAX_CASES];

    if (error_line != NULL) {
        *error_line = 0u;
    }
    if (evidence == NULL) {
        return HITL_ERR_ARGUMENT;
    }
    memset(evidence, 0, sizeof(*evidence));
    if (stream == NULL || plan == NULL || plan->case_count == 0u ||
        plan->case_count > HITL_MAX_CASES) {
        return HITL_ERR_ARGUMENT;
    }
    memset(&record, 0, sizeof(record));
    memset(present, 0, sizeof(present));
    hitl_reader_init(&reader, stream);
    result = hitl_read_header(&reader, &evidence->format_major,
                              &evidence->format_minor, error_line);
    if (result != HITL_OK) {
        return hitl_auto_failure(evidence, result);
    }

    while ((result = hitl_reader_next(&reader, &record)) == HITL_OK) {
        const char *name = record.fields[0];

        if (hitl_is_extension_record(&record)) {
            continue;
        }
        if (strcmp(name, "RUN") == 0) {
            if (stage > 0u) {
                hitl_set_error_line(error_line, &record);
                return hitl_auto_failure(evidence, HITL_ERR_DUPLICATE);
            }
            result = hitl_copy_run(evidence->identity.run, &record);
            stage = 1u;
        } else if (strcmp(name, "BUILD_HASH") == 0) {
            if (stage != 1u) {
                hitl_set_error_line(error_line, &record);
                return hitl_auto_failure(
                    evidence,
                    stage > 1u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(evidence->identity.build_hash, &record);
            stage = 2u;
        } else if (strcmp(name, "PLAN_HASH") == 0) {
            if (stage != 2u) {
                hitl_set_error_line(error_line, &record);
                return hitl_auto_failure(
                    evidence,
                    stage > 2u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(evidence->identity.plan_hash, &record);
            stage = 3u;
        } else if (strcmp(name, "PROFILE_HASH") == 0) {
            if (stage != 3u) {
                hitl_set_error_line(error_line, &record);
                return hitl_auto_failure(
                    evidence,
                    stage > 3u ? HITL_ERR_DUPLICATE : HITL_ERR_FORMAT);
            }
            result = hitl_copy_hash(evidence->identity.profile_hash, &record);
            stage = 4u;
        } else if (strcmp(name, "AUTO") == 0) {
            HitlAutoRecord auto_record;
            int index;

            if (stage != 4u) {
                hitl_set_error_line(error_line, &record);
                return hitl_auto_failure(evidence, HITL_ERR_FORMAT);
            }
            memset(&auto_record, 0, sizeof(auto_record));
            result = hitl_parse_auto_record(&auto_record, &record);
            if (result == HITL_OK) {
                index = hitl_case_index(plan, auto_record.case_id);
                if (index < 0) {
                    hitl_set_error_line(error_line, &record);
                    return hitl_auto_failure(evidence,
                                             HITL_ERR_UNKNOWN_CASE);
                }
                if (present[(size_t)index] != 0u) {
                    hitl_set_error_line(error_line, &record);
                    return hitl_auto_failure(evidence,
                                             HITL_ERR_DUPLICATE);
                }
                evidence->records[(size_t)index] = auto_record;
                present[(size_t)index] = 1u;
                ++evidence->record_count;
            }
        } else {
            hitl_set_error_line(error_line, &record);
            return hitl_auto_failure(
                evidence, hitl_known_auto_record(name) ? HITL_ERR_FORMAT :
                                                        HITL_ERR_UNKNOWN_RECORD);
        }

        if (result != HITL_OK) {
            hitl_set_error_line(error_line, &record);
            return hitl_auto_failure(evidence, result);
        }
    }
    if (result != HITL_END) {
        if (error_line != NULL) {
            *error_line = record.line_number;
        }
        return hitl_auto_failure(evidence, result);
    }
    if (stage != 4u || evidence->record_count != plan->case_count) {
        if (error_line != NULL) {
            *error_line = reader.line_number + 1u;
        }
        return hitl_auto_failure(evidence, HITL_ERR_MISSING);
    }
    {
        size_t index;
        for (index = 0u; index < plan->case_count; ++index) {
            if (present[index] == 0u) {
                if (error_line != NULL) {
                    *error_line = reader.line_number + 1u;
                }
                return hitl_auto_failure(evidence, HITL_ERR_MISSING);
            }
        }
    }

    evidence->identity_current =
        hitl_identity_equal(&plan->identity, &evidence->identity);
    if (!evidence->identity_current) {
        return HITL_ERR_STALE;
    }
    return HITL_OK;
}

void hitl_case_state_init(HitlCaseState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

HitlResult hitl_case_state_apply_auto(HitlCaseState *state,
                                      const HitlPlan *plan,
                                      const HitlAutoEvidence *evidence,
                                      size_t case_index)
{
    HitlCaseState candidate;
    size_t index;

    if (state == NULL || plan == NULL || evidence == NULL ||
        plan->case_count == 0u || plan->case_count > HITL_MAX_CASES ||
        evidence->record_count != plan->case_count ||
        case_index >= plan->case_count ||
        plan->format_major != HITL_FORMAT_MAJOR ||
        evidence->format_major != HITL_FORMAT_MAJOR) {
        return HITL_ERR_ARGUMENT;
    }
    if (!evidence->identity_current ||
        !hitl_identity_equal(&plan->identity, &evidence->identity)) {
        return HITL_ERR_STALE;
    }
    for (index = 0u; index < plan->case_count; ++index) {
        if (!hitl_status_valid(evidence->records[index].status) ||
            strcmp(plan->cases[index].id,
                   evidence->records[index].case_id) != 0) {
            return HITL_ERR_FORMAT;
        }
    }

    memset(&candidate, 0, sizeof(candidate));
    if (state->auto_evidence_current &&
        state->manual_evidence_current &&
        hitl_status_valid(state->manual_status) &&
        hitl_identity_equal(&state->identity, &evidence->identity) &&
        strcmp(state->case_id, plan->cases[case_index].id) == 0) {
        candidate.manual_status = state->manual_status;
        candidate.manual_evidence_current = 1;
    } else {
        candidate.manual_status = HITL_STATUS_UNRUN;
    }
    candidate.auto_status = evidence->records[case_index].status;
    candidate.identity = evidence->identity;
    memcpy(candidate.case_id, plan->cases[case_index].id,
           strlen(plan->cases[case_index].id) + 1u);
    candidate.auto_evidence_current = 1;
    *state = candidate;
    return HITL_OK;
}

static HitlCombinedResult hitl_combined_result(HitlStatus status,
                                                const char *code)
{
    HitlCombinedResult result;
    result.status = status;
    result.code = code;
    return result;
}

HitlCombinedResult hitl_case_combined(const HitlPlan *plan,
                                      const HitlAutoEvidence *evidence,
                                      size_t case_index,
                                      const HitlCaseState *state)
{
    const HitlCase *definition;

    if (plan == NULL || evidence == NULL || state == NULL ||
        plan->case_count == 0u || plan->case_count > HITL_MAX_CASES ||
        evidence->record_count != plan->case_count ||
        case_index >= plan->case_count ||
        !evidence->identity_current ||
        !hitl_identity_equal(&plan->identity, &evidence->identity) ||
        !state->auto_evidence_current ||
        !hitl_identity_equal(&state->identity, &evidence->identity) ||
        strcmp(plan->cases[case_index].id,
               evidence->records[case_index].case_id) != 0 ||
        strcmp(plan->cases[case_index].id, state->case_id) != 0 ||
        state->auto_status != evidence->records[case_index].status ||
        !hitl_status_valid(state->auto_status) ||
        !hitl_status_valid(state->manual_status) ||
        (state->manual_status != HITL_STATUS_UNRUN &&
         !state->manual_evidence_current)) {
        return hitl_combined_result(HITL_STATUS_BLOCKED,
                                    "STALE_EVIDENCE");
    }
    definition = &plan->cases[case_index];
    if (definition->auto_required > 1u ||
        definition->manual_required > 1u ||
        (definition->auto_required == 0u &&
         definition->manual_required == 0u)) {
        return hitl_combined_result(HITL_STATUS_BLOCKED,
                                    "STALE_EVIDENCE");
    }
    if (state->auto_status == HITL_STATUS_FAIL) {
        return hitl_combined_result(HITL_STATUS_FAIL, "AUTO_FAIL");
    }
    if (state->manual_status == HITL_STATUS_FAIL) {
        return hitl_combined_result(HITL_STATUS_FAIL, "MANUAL_FAIL");
    }
    if (definition->auto_required != 0u &&
        state->auto_status == HITL_STATUS_BLOCKED) {
        return hitl_combined_result(HITL_STATUS_BLOCKED, "AUTO_BLOCKED");
    }
    if (definition->manual_required != 0u &&
        state->manual_status == HITL_STATUS_BLOCKED) {
        return hitl_combined_result(HITL_STATUS_BLOCKED, "MANUAL_BLOCKED");
    }
    if (definition->auto_required != 0u &&
        state->auto_status != HITL_STATUS_PASS) {
        return hitl_combined_result(HITL_STATUS_UNRUN, "AUTO_REQUIRED");
    }
    if (definition->manual_required != 0u &&
        state->manual_status != HITL_STATUS_PASS) {
        return hitl_combined_result(HITL_STATUS_UNRUN, "MANUAL_REQUIRED");
    }
    return hitl_combined_result(HITL_STATUS_PASS, "OK");
}
