#include "moon/mdp.h"

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

/* Independently inspected canonical MDP1 fixture; SHA-256:
 * 0b71339bf43584fbc0fcebb7e868fd20a09adf2e8c5b5e3aef56ad94b75268c1
 */
static const uint8_t golden_fixture[] = {
    0x4d, 0x44, 0x50, 0x31, 0x01, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x00, 0x00, 0x02, 0x51, 0x12, 0x5f,
    0x4d, 0x41, 0x50, 0x20, 0x01, 0x00, 0x00, 0x00,
    0x2a, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0xf4, 0x99, 0x0b, 0x47, 0x50, 0x41, 0x4c, 0x20,
    0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x36, 0xfd, 0x2d, 0xa6,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00,
    0x09, 0x08, 0x07, 0x00
};

static int test_endian_and_crc(void)
{
    uint8_t bytes[4];
    static const char check_text[] = "123456789";

    mdp_write_u16le(bytes, UINT16_C(0xa1b2));
    CHECK(bytes[0] == 0xb2u && bytes[1] == 0xa1u);
    CHECK(mdp_read_u16le(bytes) == UINT16_C(0xa1b2));

    mdp_write_u32le(bytes, UINT32_C(0x1234abcd));
    CHECK(bytes[0] == 0xcdu && bytes[1] == 0xabu &&
          bytes[2] == 0x34u && bytes[3] == 0x12u);
    CHECK(mdp_read_u32le(bytes) == UINT32_C(0x1234abcd));
    CHECK(mdp_crc32(check_text, 9u) == UINT32_C(0xcbf43926));
    CHECK(mdp_crc32(NULL, 0u) == 0u);
    return 1;
}

static int build_fixture(void **package, size_t *size)
{
    static const uint8_t map_data[] = { 1u, 2u, 3u, 4u, 5u };
    static const uint8_t palette_data[] = { 9u, 8u, 7u };
    MdpWriteChunk chunks[2];

    memset(chunks, 0, sizeof(chunks));
    chunks[0].type = MDP_FOURCC('P', 'A', 'L', ' ');
    chunks[0].schema_version = 2u;
    chunks[0].asset_id = 7u;
    chunks[0].data = palette_data;
    chunks[0].size = sizeof(palette_data);
    chunks[1].type = MDP_FOURCC('M', 'A', 'P', ' ');
    chunks[1].schema_version = 1u;
    chunks[1].asset_id = 42u;
    chunks[1].data = map_data;
    chunks[1].size = sizeof(map_data);
    return mdp_write_archive(chunks, 2u, package, size) == MDP_OK;
}

static int test_round_trip(void)
{
    static const uint8_t map_data[] = { 1u, 2u, 3u, 4u, 5u };
    static const uint8_t palette_data[] = { 9u, 8u, 7u };
    MdpWriteChunk reversed[2];
    MdpArchive archive;
    MdpDirectoryEntry outsider;
    const MdpDirectoryEntry *entry;
    const void *chunk_data;
    size_t chunk_size;
    void *first = NULL;
    void *second = NULL;
    size_t first_size = 0u;
    size_t second_size = 0u;

    CHECK(build_fixture(&first, &first_size));
    CHECK(first_size == sizeof(golden_fixture));
    CHECK(memcmp(first, golden_fixture, sizeof(golden_fixture)) == 0);
    CHECK(memcmp(first, "MDP1", 4u) == 0);
    CHECK(mdp_read_u32le((const uint8_t *)first + 12u) == 32u);
    CHECK(mdp_read_u32le((const uint8_t *)first + 16u) == 2u);
    CHECK(mdp_read_u32le((const uint8_t *)first + 20u) == 28u);
    CHECK(mdp_read_u32le((const uint8_t *)first + 24u) == first_size);

    CHECK(mdp_archive_open(&archive, golden_fixture,
                           sizeof(golden_fixture)) == MDP_OK);
    CHECK(archive.entry_count == 2u);
    CHECK(mdp_archive_entry(&archive, 2u) == NULL);

    entry = mdp_archive_find(&archive, MDP_FOURCC('M', 'A', 'P', ' '), 42u);
    CHECK(entry != NULL);
    CHECK(entry == mdp_archive_entry(&archive, 0u));
    CHECK(entry->offset % MDP_ALIGNMENT == 0u);
    CHECK(mdp_archive_stored_chunk(&archive, entry,
                                   &chunk_data, &chunk_size) == MDP_OK);
    CHECK(chunk_size == sizeof(map_data));
    CHECK(memcmp(chunk_data, map_data, sizeof(map_data)) == 0);

    entry = mdp_archive_find(&archive, MDP_FOURCC('P', 'A', 'L', ' '), 7u);
    CHECK(entry != NULL);
    CHECK(entry == mdp_archive_entry(&archive, 1u));
    CHECK(mdp_archive_stored_chunk(&archive, entry,
                                   &chunk_data, &chunk_size) == MDP_OK);
    CHECK(chunk_size == sizeof(palette_data));
    CHECK(memcmp(chunk_data, palette_data, sizeof(palette_data)) == 0);
    CHECK(mdp_archive_find(&archive,
                           MDP_FOURCC('M', 'A', 'P', ' '), 43u) == NULL);

    outsider = *entry;
    chunk_data = &archive;
    chunk_size = 99u;
    CHECK(mdp_archive_stored_chunk(&archive, &outsider,
                                   &chunk_data, &chunk_size) ==
          MDP_ERR_ARGUMENT);
    CHECK(chunk_data == NULL && chunk_size == 0u);
    mdp_archive_close(&archive);

    memset(reversed, 0, sizeof(reversed));
    reversed[0].type = MDP_FOURCC('M', 'A', 'P', ' ');
    reversed[0].schema_version = 1u;
    reversed[0].asset_id = 42u;
    reversed[0].data = map_data;
    reversed[0].size = sizeof(map_data);
    reversed[1].type = MDP_FOURCC('P', 'A', 'L', ' ');
    reversed[1].schema_version = 2u;
    reversed[1].asset_id = 7u;
    reversed[1].data = palette_data;
    reversed[1].size = sizeof(palette_data);
    CHECK(mdp_write_archive(reversed, 2u, &second, &second_size) == MDP_OK);
    CHECK(first_size == second_size);
    CHECK(memcmp(first, second, first_size) == 0);

    mdp_buffer_free(second);
    mdp_buffer_free(first);
    return 1;
}

static void refresh_directory_crc(uint8_t *bytes)
{
    uint32_t offset = mdp_read_u32le(bytes + 12u);
    uint32_t count = mdp_read_u32le(bytes + 16u);
    uint32_t record_size = mdp_read_u32le(bytes + 20u);
    mdp_write_u32le(bytes + 28u,
                    mdp_crc32(bytes + offset,
                              (size_t)count * record_size));
}

static int expect_mutation_result(size_t byte_offset,
                                  uint8_t xor_value,
                                  MdpResult expected)
{
    void *original = NULL;
    size_t size = 0u;
    uint8_t *copy;
    MdpArchive archive;
    MdpResult result;

    CHECK(build_fixture(&original, &size));
    CHECK(byte_offset < size);
    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    copy[byte_offset] ^= xor_value;
    result = mdp_archive_open(&archive, copy, size);
    CHECK(result == expected);
    mdp_archive_close(&archive);
    free(copy);
    mdp_buffer_free(original);
    return 1;
}

static int test_invalid_packages(void)
{
    void *original = NULL;
    size_t size = 0u;
    uint8_t *copy;
    MdpArchive archive;
    uint32_t first_offset;
    MdpResult result;

    CHECK(expect_mutation_result(0u, 1u, MDP_ERR_FORMAT));
    CHECK(expect_mutation_result(4u, 1u, MDP_ERR_VERSION));
    CHECK(expect_mutation_result(6u, 1u, MDP_ERR_VERSION));
    CHECK(expect_mutation_result(8u, 1u, MDP_ERR_FORMAT));
    CHECK(expect_mutation_result(12u, 1u, MDP_ERR_FORMAT));
    CHECK(expect_mutation_result(20u, 1u, MDP_ERR_FORMAT));
    CHECK(expect_mutation_result(24u, 1u, MDP_ERR_BOUNDS));
    CHECK(expect_mutation_result(28u, 1u, MDP_ERR_CRC));
    CHECK(expect_mutation_result(32u, 1u, MDP_ERR_CRC));

    CHECK(build_fixture(&original, &size));
    first_offset = mdp_read_u32le((const uint8_t *)original + 32u + 12u);

    copy = (uint8_t *)malloc(size + 1u);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    copy[size] = 0u;
    CHECK(mdp_archive_open(&archive, copy, size + 1u) == MDP_ERR_BOUNDS);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    copy[first_offset] ^= 0x80u;
    memset(&archive, 0xa5, sizeof(archive));
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_CRC);
    CHECK(archive.data == NULL && archive.size == 0u &&
          archive.entries == NULL && archive.entry_count == 0u);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 16u, UINT32_MAX);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_BOUNDS);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u16le(copy + 32u + 4u, 0u);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 0u, MDP_FOURCC('Z', 'Z', 'Z', 'Z'));
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    copy[first_offset + 5u] = 1u;
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 12u, first_offset + 1u);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)calloc(size + MDP_ALIGNMENT, 1u);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 24u, (uint32_t)(size + MDP_ALIGNMENT));
    CHECK(mdp_archive_open(&archive, copy, size + MDP_ALIGNMENT) ==
          MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u16le(copy + 32u + 6u, MDP_CHUNK_COMPRESSION_RLE);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_UNSUPPORTED);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u16le(copy + 32u + 6u, MDP_CHUNK_COMPRESSION_LZSS);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_UNSUPPORTED);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u16le(copy + 32u + 6u, UINT16_C(0x8000));
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_UNSUPPORTED);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 20u, 6u);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 16u, (uint32_t)size);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_BOUNDS);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 28u + 0u,
                    mdp_read_u32le(copy + 32u + 0u));
    mdp_write_u32le(copy + 32u + 28u + 8u, 41u);
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 28u + 12u, first_offset);
    refresh_directory_crc(copy);
    result = mdp_archive_open(&archive, copy, size);
    CHECK(result == MDP_ERR_FORMAT);
    free(copy);

    copy = (uint8_t *)malloc(size);
    CHECK(copy != NULL);
    memcpy(copy, original, size);
    mdp_write_u32le(copy + 32u + 28u + 0u,
                    mdp_read_u32le(copy + 32u + 0u));
    mdp_write_u32le(copy + 32u + 28u + 8u,
                    mdp_read_u32le(copy + 32u + 8u));
    refresh_directory_crc(copy);
    CHECK(mdp_archive_open(&archive, copy, size) == MDP_ERR_DUPLICATE);
    free(copy);

    CHECK(mdp_archive_open(&archive, original, size - 1u) == MDP_ERR_BOUNDS);
    mdp_buffer_free(original);
    return 1;
}

static int test_writer_rejections(void)
{
    static const uint8_t value = 1u;
    MdpWriteChunk chunks[2];
    void *package = NULL;
    size_t size = 0u;
    MdpArchive archive;

    package = &archive;
    size = 99u;
    CHECK(mdp_write_archive(NULL, 1u, &package, &size) ==
          MDP_ERR_ARGUMENT);
    CHECK(package == NULL && size == 0u);

    memset(chunks, 0, sizeof(chunks));
    chunks[0].type = MDP_FOURCC('T', 'E', 'S', 'T');
    chunks[0].asset_id = 1u;
    chunks[0].schema_version = 1u;
    chunks[0].data = &value;
    chunks[0].size = 1u;
    chunks[1] = chunks[0];
    CHECK(mdp_write_archive(chunks, 2u, &package, &size) ==
          MDP_ERR_DUPLICATE);

    chunks[1].asset_id = 2u;
    chunks[1].flags = MDP_CHUNK_COMPRESSION_LZSS;
    CHECK(mdp_write_archive(chunks, 2u, &package, &size) ==
          MDP_ERR_UNSUPPORTED);

    chunks[1].flags = 0u;
    chunks[1].data = NULL;
    CHECK(mdp_write_archive(chunks, 2u, &package, &size) ==
          MDP_ERR_ARGUMENT);

    chunks[0].schema_version = 0u;
    CHECK(mdp_write_archive(chunks, 1u, &package, &size) ==
          MDP_ERR_FORMAT);
    chunks[0].schema_version = 1u;

#if SIZE_MAX > UINT32_MAX
    chunks[0].size = (size_t)UINT32_MAX + 1u;
    CHECK(mdp_write_archive(chunks, 1u, &package, &size) ==
          MDP_ERR_OVERFLOW);
#endif

    CHECK(mdp_write_archive(NULL, 0u, &package, &size) == MDP_OK);
    CHECK(size == MDP_HEADER_SIZE);
    CHECK(mdp_archive_open(&archive, package, size) == MDP_OK);
    CHECK(archive.entry_count == 0u);
    mdp_archive_close(&archive);
    mdp_buffer_free(package);
    return 1;
}

static int test_canonical_scale(void)
{
    enum { CHUNK_COUNT = 4096 };
    MdpWriteChunk *chunks;
    MdpArchive archive;
    const MdpDirectoryEntry *entry;
    void *package = NULL;
    size_t package_size = 0u;
    size_t i;

    chunks = (MdpWriteChunk *)calloc(CHUNK_COUNT, sizeof(*chunks));
    CHECK(chunks != NULL);
    for (i = 0u; i < CHUNK_COUNT; ++i) {
        chunks[i].type = MDP_FOURCC('Z', 'E', 'R', 'O');
        chunks[i].schema_version = 1u;
        chunks[i].asset_id = (uint32_t)i;
    }

    CHECK(mdp_write_archive(chunks, CHUNK_COUNT,
                            &package, &package_size) == MDP_OK);
    CHECK(package_size == MDP_HEADER_SIZE +
                          CHUNK_COUNT * MDP_DIRECTORY_RECORD_SIZE);
    CHECK(mdp_archive_open(&archive, package, package_size) == MDP_OK);
    CHECK(archive.entry_count == CHUNK_COUNT);

    entry = mdp_archive_find(&archive, MDP_FOURCC('Z', 'E', 'R', 'O'),
                             CHUNK_COUNT / 2u);
    CHECK(entry != NULL && entry->asset_id == CHUNK_COUNT / 2u);
    CHECK(mdp_archive_find(&archive, MDP_FOURCC('Z', 'E', 'R', 'O'),
                           CHUNK_COUNT) == NULL);

    mdp_archive_close(&archive);
    mdp_buffer_free(package);
    free(chunks);
    return 1;
}

int main(void)
{
    if (!test_endian_and_crc() ||
        !test_round_trip() ||
        !test_invalid_packages() ||
        !test_writer_rejections() ||
        !test_canonical_scale()) {
        return 1;
    }
    puts("PASS: MDP v1 codec tests");
    return 0;
}
