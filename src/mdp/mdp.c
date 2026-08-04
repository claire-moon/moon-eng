#include "moon/mdp.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MDP_DIRECTORY_OFFSET MDP_HEADER_SIZE

typedef struct MdpSortedChunk {
    const MdpWriteChunk *chunk;
    size_t original_index;
} MdpSortedChunk;

uint16_t mdp_read_u16le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

uint32_t mdp_read_u32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

void mdp_write_u16le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
}

void mdp_write_u32le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)((value >> 8) & 0xffu);
    bytes[2] = (uint8_t)((value >> 16) & 0xffu);
    bytes[3] = (uint8_t)((value >> 24) & 0xffu);
}

uint32_t mdp_crc32(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xffffffff);
    size_t i;
    unsigned int bit;

    for (i = 0u; i < size; ++i) {
        crc ^= bytes[i];
        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }

    return crc ^ UINT32_C(0xffffffff);
}

const char *mdp_result_string(MdpResult result)
{
    switch (result) {
    case MDP_OK: return "ok";
    case MDP_ERR_ARGUMENT: return "invalid argument";
    case MDP_ERR_MEMORY: return "out of memory";
    case MDP_ERR_FORMAT: return "invalid MDP format";
    case MDP_ERR_VERSION: return "unsupported MDP version";
    case MDP_ERR_BOUNDS: return "out-of-bounds MDP data";
    case MDP_ERR_OVERFLOW: return "integer overflow or file too large";
    case MDP_ERR_CRC: return "CRC mismatch";
    case MDP_ERR_UNSUPPORTED: return "unsupported MDP feature";
    case MDP_ERR_DUPLICATE: return "duplicate MDP asset key";
    default: return "unknown MDP error";
    }
}

static int mdp_add_size(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) {
        return 0;
    }
    *result = a + b;
    return 1;
}

static int mdp_mul_size(size_t a, size_t b, size_t *result)
{
    if (a != 0u && b > SIZE_MAX / a) {
        return 0;
    }
    *result = a * b;
    return 1;
}

static int mdp_align_size(size_t value, size_t *result)
{
    size_t remainder = value & (MDP_ALIGNMENT - 1u);
    size_t padding = remainder == 0u ? 0u : MDP_ALIGNMENT - remainder;
    return mdp_add_size(value, padding, result);
}

static int mdp_range_inside(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static int mdp_compare_type(uint32_t left, uint32_t right)
{
    unsigned int shift;

    for (shift = 0u; shift < 32u; shift += 8u) {
        uint32_t left_byte = (left >> shift) & 0xffu;
        uint32_t right_byte = (right >> shift) & 0xffu;

        if (left_byte < right_byte) return -1;
        if (left_byte > right_byte) return 1;
    }
    return 0;
}

static int mdp_compare_entry_key(const MdpDirectoryEntry *left,
                                 const MdpDirectoryEntry *right)
{
    int type_order = mdp_compare_type(left->type, right->type);

    if (type_order != 0) return type_order;
    if (left->asset_id < right->asset_id) return -1;
    if (left->asset_id > right->asset_id) return 1;
    return 0;
}

static int mdp_bytes_are_zero(const uint8_t *bytes,
                              size_t begin,
                              size_t end)
{
    size_t i;

    for (i = begin; i < end; ++i) {
        if (bytes[i] != 0u) return 0;
    }
    return 1;
}

static MdpResult mdp_validate_flags(uint16_t flags)
{
    if ((flags & (uint16_t)~MDP_CHUNK_KNOWN_FLAGS) != 0u) {
        return MDP_ERR_UNSUPPORTED;
    }
    if ((flags & MDP_CHUNK_COMPRESSION_MASK) != MDP_CHUNK_COMPRESSION_RAW) {
        return MDP_ERR_UNSUPPORTED;
    }
    return MDP_OK;
}

static void mdp_read_directory_entry(MdpDirectoryEntry *entry,
                                     const uint8_t *record)
{
    entry->type = mdp_read_u32le(record + 0u);
    entry->schema_version = mdp_read_u16le(record + 4u);
    entry->flags = mdp_read_u16le(record + 6u);
    entry->asset_id = mdp_read_u32le(record + 8u);
    entry->offset = mdp_read_u32le(record + 12u);
    entry->packed_size = mdp_read_u32le(record + 16u);
    entry->unpacked_size = mdp_read_u32le(record + 20u);
    entry->crc32 = mdp_read_u32le(record + 24u);
}

static void mdp_write_directory_entry(uint8_t *record,
                                      const MdpDirectoryEntry *entry)
{
    mdp_write_u32le(record + 0u, entry->type);
    mdp_write_u16le(record + 4u, entry->schema_version);
    mdp_write_u16le(record + 6u, entry->flags);
    mdp_write_u32le(record + 8u, entry->asset_id);
    mdp_write_u32le(record + 12u, entry->offset);
    mdp_write_u32le(record + 16u, entry->packed_size);
    mdp_write_u32le(record + 20u, entry->unpacked_size);
    mdp_write_u32le(record + 24u, entry->crc32);
}

void mdp_archive_close(MdpArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    free(archive->entries);
    memset(archive, 0, sizeof(*archive));
}

MdpResult mdp_archive_open(MdpArchive *archive,
                           const void *data,
                           size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t header_size;
    uint32_t directory_offset;
    uint32_t directory_count;
    uint32_t directory_record_size;
    uint32_t file_size;
    uint32_t expected_directory_crc;
    size_t directory_size;
    size_t directory_end;
    size_t payload_minimum;
    size_t entries_size;
    MdpDirectoryEntry *entries = NULL;
    uint32_t i;
    size_t expected_payload_offset;

    if (archive == NULL || (data == NULL && size != 0u)) {
        return MDP_ERR_ARGUMENT;
    }
    memset(archive, 0, sizeof(*archive));

    if (size < MDP_HEADER_SIZE) {
        return MDP_ERR_BOUNDS;
    }
    if (memcmp(bytes, "MDP1", 4u) != 0) {
        return MDP_ERR_FORMAT;
    }

    version_major = mdp_read_u16le(bytes + 4u);
    version_minor = mdp_read_u16le(bytes + 6u);
    header_size = mdp_read_u32le(bytes + 8u);
    directory_offset = mdp_read_u32le(bytes + 12u);
    directory_count = mdp_read_u32le(bytes + 16u);
    directory_record_size = mdp_read_u32le(bytes + 20u);
    file_size = mdp_read_u32le(bytes + 24u);
    expected_directory_crc = mdp_read_u32le(bytes + 28u);

    if (version_major != MDP_VERSION_MAJOR ||
        version_minor > MDP_VERSION_MINOR) {
        return MDP_ERR_VERSION;
    }
    if (header_size != MDP_HEADER_SIZE ||
        directory_record_size != MDP_DIRECTORY_RECORD_SIZE) {
        return MDP_ERR_FORMAT;
    }
    if (directory_offset != MDP_DIRECTORY_OFFSET) {
        return MDP_ERR_FORMAT;
    }
    if ((size_t)file_size != size) {
        return MDP_ERR_BOUNDS;
    }
    if (directory_count >
        (file_size - directory_offset) / directory_record_size) {
        return MDP_ERR_BOUNDS;
    }
    if (!mdp_mul_size((size_t)directory_count,
                      (size_t)directory_record_size,
                      &directory_size) ||
        !mdp_add_size((size_t)directory_offset,
                      directory_size,
                      &directory_end)) {
        return MDP_ERR_OVERFLOW;
    }
    if (!mdp_range_inside((size_t)directory_offset,
                          directory_size,
                          size)) {
        return MDP_ERR_BOUNDS;
    }
    if (mdp_crc32(bytes + directory_offset, directory_size) !=
        expected_directory_crc) {
        return MDP_ERR_CRC;
    }
    if (!mdp_align_size(directory_end, &payload_minimum)) {
        return MDP_ERR_OVERFLOW;
    }
    if (payload_minimum > size ||
        !mdp_bytes_are_zero(bytes, directory_end, payload_minimum)) {
        return MDP_ERR_FORMAT;
    }

    if (directory_count != 0u) {
        if (!mdp_mul_size((size_t)directory_count,
                          sizeof(*entries),
                          &entries_size)) {
            return MDP_ERR_OVERFLOW;
        }
        entries = (MdpDirectoryEntry *)malloc(entries_size);
        if (entries == NULL) {
            return MDP_ERR_MEMORY;
        }
        memset(entries, 0, entries_size);
    }

    expected_payload_offset = payload_minimum;
    for (i = 0u; i < directory_count; ++i) {
        const uint8_t *record = bytes + directory_offset +
                                (size_t)i * directory_record_size;
        MdpResult flag_result;
        size_t payload_end;
        size_t aligned_payload_end;

        mdp_read_directory_entry(&entries[i], record);
        flag_result = mdp_validate_flags(entries[i].flags);
        if (flag_result != MDP_OK) {
            free(entries);
            return flag_result;
        }
        if (entries[i].schema_version == 0u) {
            free(entries);
            return MDP_ERR_FORMAT;
        }
        if (i != 0u) {
            const MdpDirectoryEntry *previous = &entries[i - 1u];

            if (previous->type == entries[i].type &&
                previous->asset_id == entries[i].asset_id) {
                free(entries);
                return MDP_ERR_DUPLICATE;
            }
            if (mdp_compare_entry_key(previous, &entries[i]) >= 0) {
                free(entries);
                return MDP_ERR_FORMAT;
            }
        }
        if ((entries[i].offset & (MDP_ALIGNMENT - 1u)) != 0u) {
            free(entries);
            return MDP_ERR_FORMAT;
        }
        if (!mdp_range_inside((size_t)entries[i].offset,
                              (size_t)entries[i].packed_size,
                              size)) {
            free(entries);
            return MDP_ERR_BOUNDS;
        }
        if ((size_t)entries[i].offset != expected_payload_offset) {
            free(entries);
            return MDP_ERR_FORMAT;
        }
        if (entries[i].packed_size != entries[i].unpacked_size) {
            free(entries);
            return MDP_ERR_FORMAT;
        }
        if (mdp_crc32(bytes + entries[i].offset,
                      (size_t)entries[i].packed_size) != entries[i].crc32) {
            free(entries);
            return MDP_ERR_CRC;
        }
        if (!mdp_add_size((size_t)entries[i].offset,
                          (size_t)entries[i].packed_size,
                          &payload_end) ||
            !mdp_align_size(payload_end, &aligned_payload_end) ||
            aligned_payload_end > size ||
            !mdp_bytes_are_zero(bytes, payload_end, aligned_payload_end)) {
            free(entries);
            return MDP_ERR_FORMAT;
        }
        expected_payload_offset = aligned_payload_end;
    }

    if (expected_payload_offset != size) {
        free(entries);
        return MDP_ERR_FORMAT;
    }

    archive->data = bytes;
    archive->size = size;
    archive->entries = entries;
    archive->entry_count = directory_count;
    archive->version_major = version_major;
    archive->version_minor = version_minor;
    return MDP_OK;
}

const MdpDirectoryEntry *mdp_archive_entry(const MdpArchive *archive,
                                           uint32_t index)
{
    if (archive == NULL || index >= archive->entry_count) {
        return NULL;
    }
    return &archive->entries[index];
}

const MdpDirectoryEntry *mdp_archive_find(const MdpArchive *archive,
                                          uint32_t type,
                                          uint32_t asset_id)
{
    uint32_t begin;
    uint32_t end;

    if (archive == NULL) {
        return NULL;
    }

    begin = 0u;
    end = archive->entry_count;
    while (begin < end) {
        uint32_t middle = begin + (end - begin) / 2u;
        const MdpDirectoryEntry *entry = &archive->entries[middle];
        int type_order = mdp_compare_type(entry->type, type);

        if (type_order < 0 ||
            (type_order == 0 && entry->asset_id < asset_id)) {
            begin = middle + 1u;
        } else {
            end = middle;
        }
    }
    if (begin < archive->entry_count &&
        archive->entries[begin].type == type &&
        archive->entries[begin].asset_id == asset_id) {
        return &archive->entries[begin];
    }
    return NULL;
}

MdpResult mdp_archive_stored_chunk(const MdpArchive *archive,
                                   const MdpDirectoryEntry *entry,
                                   const void **data,
                                   size_t *packed_size)
{
    uintptr_t entry_address;
    uintptr_t entries_begin;
    uintptr_t entries_end;
    size_t entries_size;

    if (data == NULL || packed_size == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    *data = NULL;
    *packed_size = 0u;
    if (archive == NULL || entry == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    if (archive->entry_count == 0u || archive->entries == NULL) {
        return MDP_ERR_ARGUMENT;
    }

    if (!mdp_mul_size((size_t)archive->entry_count,
                      sizeof(*archive->entries),
                      &entries_size)) {
        return MDP_ERR_ARGUMENT;
    }

    entry_address = (uintptr_t)(const void *)entry;
    entries_begin = (uintptr_t)(const void *)archive->entries;
    if (entries_begin > UINTPTR_MAX - entries_size) {
        return MDP_ERR_ARGUMENT;
    }
    entries_end = entries_begin + entries_size;
    if (entry_address < entries_begin || entry_address >= entries_end ||
        (entry_address - entries_begin) % sizeof(*archive->entries) != 0u) {
        return MDP_ERR_ARGUMENT;
    }
    if (!mdp_range_inside((size_t)entry->offset,
                          (size_t)entry->packed_size,
                          archive->size)) {
        return MDP_ERR_BOUNDS;
    }

    *data = archive->data + entry->offset;
    *packed_size = (size_t)entry->packed_size;
    return MDP_OK;
}

static int mdp_compare_sorted_chunks(const void *left, const void *right)
{
    const MdpSortedChunk *a = (const MdpSortedChunk *)left;
    const MdpSortedChunk *b = (const MdpSortedChunk *)right;
    int type_order = mdp_compare_type(a->chunk->type, b->chunk->type);

    if (type_order != 0) return type_order;
    if (a->chunk->asset_id < b->chunk->asset_id) return -1;
    if (a->chunk->asset_id > b->chunk->asset_id) return 1;
    if (a->original_index < b->original_index) return -1;
    if (a->original_index > b->original_index) return 1;
    return 0;
}

MdpResult mdp_write_archive(const MdpWriteChunk *chunks,
                            size_t chunk_count,
                            void **output,
                            size_t *output_size)
{
    MdpSortedChunk *sorted = NULL;
    MdpDirectoryEntry *entries = NULL;
    uint8_t *bytes = NULL;
    size_t directory_size;
    size_t data_offset;
    size_t file_size;
    size_t i;

    if (output == NULL || output_size == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    *output = NULL;
    *output_size = 0u;
    if (chunks == NULL && chunk_count != 0u) {
        return MDP_ERR_ARGUMENT;
    }

    if (chunk_count > UINT32_MAX ||
        !mdp_mul_size(chunk_count,
                      (size_t)MDP_DIRECTORY_RECORD_SIZE,
                      &directory_size) ||
        !mdp_add_size((size_t)MDP_DIRECTORY_OFFSET,
                      directory_size,
                      &data_offset) ||
        !mdp_align_size(data_offset, &data_offset)) {
        return MDP_ERR_OVERFLOW;
    }

    if (chunk_count != 0u) {
        if (chunk_count > SIZE_MAX / sizeof(*sorted) ||
            chunk_count > SIZE_MAX / sizeof(*entries)) {
            return MDP_ERR_OVERFLOW;
        }
        sorted = (MdpSortedChunk *)malloc(chunk_count * sizeof(*sorted));
        entries = (MdpDirectoryEntry *)calloc(chunk_count, sizeof(*entries));
        if (sorted == NULL || entries == NULL) {
            free(sorted);
            free(entries);
            return MDP_ERR_MEMORY;
        }
    }

    for (i = 0u; i < chunk_count; ++i) {
        MdpResult flag_result = mdp_validate_flags(chunks[i].flags);
        if (flag_result != MDP_OK) {
            free(sorted);
            free(entries);
            return flag_result;
        }
        if (chunks[i].schema_version == 0u) {
            free(sorted);
            free(entries);
            return MDP_ERR_FORMAT;
        }
        if ((chunks[i].data == NULL && chunks[i].size != 0u)) {
            free(sorted);
            free(entries);
            return MDP_ERR_ARGUMENT;
        }
        if (chunks[i].size > UINT32_MAX) {
            free(sorted);
            free(entries);
            return MDP_ERR_OVERFLOW;
        }
        sorted[i].chunk = &chunks[i];
        sorted[i].original_index = i;
    }

    if (chunk_count > 1u) {
        qsort(sorted, chunk_count, sizeof(*sorted),
              mdp_compare_sorted_chunks);
    }
    for (i = 1u; i < chunk_count; ++i) {
        if (sorted[i - 1u].chunk->type == sorted[i].chunk->type &&
            sorted[i - 1u].chunk->asset_id == sorted[i].chunk->asset_id) {
            free(sorted);
            free(entries);
            return MDP_ERR_DUPLICATE;
        }
    }

    file_size = data_offset;
    for (i = 0u; i < chunk_count; ++i) {
        if (!mdp_add_size(file_size, sorted[i].chunk->size, &file_size) ||
            !mdp_align_size(file_size, &file_size) ||
            file_size > UINT32_MAX) {
            free(sorted);
            free(entries);
            return MDP_ERR_OVERFLOW;
        }
    }

    bytes = (uint8_t *)calloc(file_size == 0u ? 1u : file_size, 1u);
    if (bytes == NULL) {
        free(sorted);
        free(entries);
        return MDP_ERR_MEMORY;
    }

    memcpy(bytes, "MDP1", 4u);
    mdp_write_u16le(bytes + 4u, MDP_VERSION_MAJOR);
    mdp_write_u16le(bytes + 6u, MDP_VERSION_MINOR);
    mdp_write_u32le(bytes + 8u, MDP_HEADER_SIZE);
    mdp_write_u32le(bytes + 12u, MDP_DIRECTORY_OFFSET);
    mdp_write_u32le(bytes + 16u, (uint32_t)chunk_count);
    mdp_write_u32le(bytes + 20u, MDP_DIRECTORY_RECORD_SIZE);
    mdp_write_u32le(bytes + 24u, (uint32_t)file_size);

    data_offset = (size_t)MDP_DIRECTORY_OFFSET + directory_size;
    if (!mdp_align_size(data_offset, &data_offset)) {
        free(bytes);
        free(sorted);
        free(entries);
        return MDP_ERR_OVERFLOW;
    }

    for (i = 0u; i < chunk_count; ++i) {
        const MdpWriteChunk *chunk = sorted[i].chunk;
        uint8_t *record = bytes + MDP_DIRECTORY_OFFSET +
                          i * MDP_DIRECTORY_RECORD_SIZE;

        entries[i].type = chunk->type;
        entries[i].schema_version = chunk->schema_version;
        entries[i].flags = chunk->flags;
        entries[i].asset_id = chunk->asset_id;
        entries[i].offset = (uint32_t)data_offset;
        entries[i].packed_size = (uint32_t)chunk->size;
        entries[i].unpacked_size = (uint32_t)chunk->size;
        entries[i].crc32 = mdp_crc32(chunk->data, chunk->size);
        mdp_write_directory_entry(record, &entries[i]);

        if (chunk->size != 0u) {
            memcpy(bytes + data_offset, chunk->data, chunk->size);
        }
        data_offset += chunk->size;
        (void)mdp_align_size(data_offset, &data_offset);
    }

    mdp_write_u32le(bytes + 28u,
                    mdp_crc32(bytes + MDP_DIRECTORY_OFFSET,
                              directory_size));

    free(sorted);
    free(entries);
    *output = bytes;
    *output_size = file_size;
    return MDP_OK;
}

void mdp_buffer_free(void *buffer)
{
    free(buffer);
}
