#include "moon/mdp.h"
#include "moon/mdp_map.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct InputFile {
    uint8_t *bytes;
    size_t size;
} InputFile;

static void usage(const char *program)
{
    fprintf(stderr,
            "usage:\n"
            "  %s validate PACKAGE.MDP\n"
            "  %s list PACKAGE.MDP\n"
            "  %s pack PACKAGE.MDP FOURCC ID SCHEMA FILE "
            "[FOURCC ID SCHEMA FILE ...]\n",
            program, program, program);
}

static int read_file(const char *path, InputFile *file)
{
    FILE *stream;
    long length;
    uint8_t *bytes;

    memset(file, 0, sizeof(*file));
    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "mdpc: cannot open %s\n", path);
        return 0;
    }
    if (fseek(stream, 0L, SEEK_END) != 0 ||
        (length = ftell(stream)) < 0L ||
        fseek(stream, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "mdpc: cannot measure %s\n", path);
        fclose(stream);
        return 0;
    }
#if SIZE_MAX < ULONG_MAX
    if ((unsigned long)length > (unsigned long)SIZE_MAX) {
        fprintf(stderr, "mdpc: %s is too large for this build\n", path);
        fclose(stream);
        return 0;
    }
#endif

    bytes = (uint8_t *)malloc(length == 0L ? 1u : (size_t)length);
    if (bytes == NULL) {
        fprintf(stderr, "mdpc: out of memory reading %s\n", path);
        fclose(stream);
        return 0;
    }
    if (length != 0L &&
        fread(bytes, 1u, (size_t)length, stream) != (size_t)length) {
        fprintf(stderr, "mdpc: incomplete read from %s\n", path);
        free(bytes);
        fclose(stream);
        return 0;
    }
    if (fclose(stream) != 0) {
        fprintf(stderr, "mdpc: error closing %s\n", path);
        free(bytes);
        return 0;
    }

    file->bytes = bytes;
    file->size = (size_t)length;
    return 1;
}

static int sidecar_path(const char *path,
                        const char extension[4],
                        char result[FILENAME_MAX])
{
    size_t length = strlen(path);
    size_t base_length = length;
    size_t filename_offset = 0u;
    size_t i;

    for (i = 0u; i < length; ++i) {
        if (path[i] == '/' || path[i] == '\\') {
            filename_offset = i + 1u;
            base_length = length;
        } else if (path[i] == '.' && i >= filename_offset) {
            base_length = i;
        }
    }
    if (base_length + 5u > FILENAME_MAX) return 0;

    memcpy(result, path, base_length);
    result[base_length] = '.';
    memcpy(result + base_length + 1u, extension, 3u);
    result[base_length + 4u] = '\0';
    return strcmp(path, result) != 0;
}

/* Returns one for present, zero for absent, and minus one for an I/O error. */
static int path_state(const char *path)
{
    struct stat info;

    if (stat(path, &info) == 0) return 1;
    if (errno == ENOENT) return 0;
    return -1;
}

static int write_file(const char *path, const void *data, size_t size)
{
    char temporary[FILENAME_MAX];
    char backup[FILENAME_MAX];
    FILE *stream;
    int output_state;
    int ok = 1;

    if (!sidecar_path(path, "$$$", temporary) ||
        !sidecar_path(path, "$BK", backup)) {
        fprintf(stderr, "mdpc: output path is too long or reserved: %s\n",
                path);
        return 0;
    }
    output_state = path_state(path);
    if (output_state < 0) {
        fprintf(stderr, "mdpc: cannot inspect output %s\n", path);
        return 0;
    }
    if (path_state(temporary) != 0 || path_state(backup) != 0) {
        fprintf(stderr,
                "mdpc: stale transaction file beside %s; recover or remove it\n",
                path);
        return 0;
    }

    stream = fopen(temporary, "wb");
    if (stream == NULL) {
        fprintf(stderr, "mdpc: cannot create temporary output %s\n",
                temporary);
        return 0;
    }
    if (size != 0u && fwrite(data, 1u, size, stream) != size) {
        fprintf(stderr, "mdpc: incomplete write to %s\n", temporary);
        ok = 0;
    }
    if (fclose(stream) != 0) {
        fprintf(stderr, "mdpc: error closing %s\n", temporary);
        ok = 0;
    }
    if (!ok) {
        (void)remove(temporary);
        return 0;
    }

    if (output_state != 0 && rename(path, backup) != 0) {
        fprintf(stderr, "mdpc: cannot preserve prior output %s\n", path);
        (void)remove(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        fprintf(stderr, "mdpc: cannot commit output %s\n", path);
        if (output_state != 0 && rename(backup, path) != 0) {
            fprintf(stderr,
                    "mdpc: prior output remains recoverable as %s\n",
                    backup);
        }
        (void)remove(temporary);
        return 0;
    }

    if (output_state != 0 && remove(backup) != 0) {
        fprintf(stderr, "mdpc: warning: prior output remains as %s\n",
                backup);
    }
    return 1;
}

static int parse_u32(const char *text, uint32_t *value)
{
    uint32_t parsed = 0u;
    const unsigned char *cursor = (const unsigned char *)text;

    if (text == NULL || *text == '\0') return 0;

    while (*cursor != '\0') {
        uint32_t digit;

        if (*cursor < (unsigned char)'0' ||
            *cursor > (unsigned char)'9') {
            return 0;
        }
        digit = (uint32_t)(*cursor - (unsigned char)'0');
        if (parsed > (UINT32_MAX - digit) / 10u) {
            return 0;
        }
        parsed = parsed * 10u + digit;
        cursor++;
    }
    *value = parsed;
    return 1;
}

static int parse_u16(const char *text, uint16_t *value)
{
    uint32_t parsed;
    if (!parse_u32(text, &parsed) || parsed > UINT16_MAX) {
        return 0;
    }
    *value = (uint16_t)parsed;
    return 1;
}

static int parse_fourcc(const char *text, uint32_t *value)
{
    if (strlen(text) != 4u) {
        return 0;
    }
    *value = MDP_FOURCC(text[0], text[1], text[2], text[3]);
    return 1;
}

static void print_fourcc(uint32_t type)
{
    unsigned int shift;
    for (shift = 0u; shift < 32u; shift += 8u) {
        unsigned char c = (unsigned char)((type >> shift) & 0xffu);
        putchar(c >= 32u && c <= 126u ? (int)c : '.');
    }
}

static int validate_typed_chunk(const char *package_path,
                                uint32_t type,
                                uint32_t asset_id,
                                uint16_t schema_version,
                                const void *data,
                                size_t size)
{
    MdpResult result;

    if (type != MDP_MAP_TYPE) {
        return 1;
    }
    if (schema_version != MDP_MAP_SCHEMA_VERSION) {
        fprintf(stderr,
                "mdpc: %s: MAP asset %lu uses unsupported schema %u\n",
                package_path,
                (unsigned long)asset_id,
                (unsigned int)schema_version);
        return 0;
    }
    result = mdp_map_validate(data, size);
    if (result != MDP_OK) {
        fprintf(stderr, "mdpc: %s: MAP asset %lu: %s\n",
                package_path,
                (unsigned long)asset_id,
                mdp_result_string(result));
        return 0;
    }
    return 1;
}

static int open_package(const char *path,
                        InputFile *file,
                        MdpArchive *archive)
{
    MdpResult result;
    if (!read_file(path, file)) {
        return 0;
    }
    result = mdp_archive_open(archive, file->bytes, file->size);
    if (result != MDP_OK) {
        fprintf(stderr, "mdpc: %s: %s\n", path, mdp_result_string(result));
        free(file->bytes);
        memset(file, 0, sizeof(*file));
        return 0;
    }
    return 1;
}

static int command_validate(const char *path)
{
    InputFile file;
    MdpArchive archive;
    uint32_t i;

    if (!open_package(path, &file, &archive)) {
        return 1;
    }
    for (i = 0u; i < archive.entry_count; ++i) {
        const MdpDirectoryEntry *entry = mdp_archive_entry(&archive, i);
        const void *data = NULL;
        size_t size = 0u;
        MdpResult result = mdp_archive_stored_chunk(&archive, entry,
                                                    &data, &size);

        if (result != MDP_OK ||
            !validate_typed_chunk(path, entry->type, entry->asset_id,
                                  entry->schema_version, data, size)) {
            if (result != MDP_OK) {
                fprintf(stderr, "mdpc: %s: asset %lu: %s\n",
                        path,
                        (unsigned long)entry->asset_id,
                        mdp_result_string(result));
            }
            mdp_archive_close(&archive);
            free(file.bytes);
            return 1;
        }
    }
    printf("%s: valid MDP %u.%u, %lu chunks, %lu bytes\n",
           path,
           (unsigned int)archive.version_major,
           (unsigned int)archive.version_minor,
           (unsigned long)archive.entry_count,
           (unsigned long)archive.size);
    mdp_archive_close(&archive);
    free(file.bytes);
    return 0;
}

static int command_list(const char *path)
{
    InputFile file;
    MdpArchive archive;
    uint32_t i;

    if (!open_package(path, &file, &archive)) {
        return 1;
    }
    printf("TYPE ID SCHEMA FLAGS OFFSET PACKED UNPACKED CRC32\n");
    for (i = 0u; i < archive.entry_count; ++i) {
        const MdpDirectoryEntry *entry = mdp_archive_entry(&archive, i);
        print_fourcc(entry->type);
        printf(" %lu %u 0x%04x %lu %lu %lu %08lx\n",
               (unsigned long)entry->asset_id,
               (unsigned int)entry->schema_version,
               (unsigned int)entry->flags,
               (unsigned long)entry->offset,
               (unsigned long)entry->packed_size,
               (unsigned long)entry->unpacked_size,
               (unsigned long)entry->crc32);
    }
    mdp_archive_close(&archive);
    free(file.bytes);
    return 0;
}

static int command_pack(int argc, char **argv)
{
    size_t count = (size_t)(argc - 3) / 4u;
    MdpWriteChunk *chunks;
    InputFile *files;
    void *package = NULL;
    size_t package_size = 0u;
    MdpResult result;
    size_t i;
    int status = 1;

    chunks = (MdpWriteChunk *)calloc(count, sizeof(*chunks));
    files = (InputFile *)calloc(count, sizeof(*files));
    if (chunks == NULL || files == NULL) {
        fprintf(stderr, "mdpc: out of memory\n");
        free(chunks);
        free(files);
        return 1;
    }

    for (i = 0u; i < count; ++i) {
        int base = 3 + (int)i * 4;
        if (!parse_fourcc(argv[base], &chunks[i].type) ||
            !parse_u32(argv[base + 1], &chunks[i].asset_id) ||
            !parse_u16(argv[base + 2], &chunks[i].schema_version)) {
            fprintf(stderr, "mdpc: invalid chunk description near %s\n",
                    argv[base]);
            goto cleanup;
        }
        if (!read_file(argv[base + 3], &files[i])) {
            goto cleanup;
        }
        if (!validate_typed_chunk(argv[2], chunks[i].type,
                                  chunks[i].asset_id,
                                  chunks[i].schema_version,
                                  files[i].bytes, files[i].size)) {
            goto cleanup;
        }
        chunks[i].flags = MDP_CHUNK_COMPRESSION_RAW;
        chunks[i].data = files[i].bytes;
        chunks[i].size = files[i].size;
    }

    result = mdp_write_archive(chunks, count, &package, &package_size);
    if (result != MDP_OK) {
        fprintf(stderr, "mdpc: cannot build package: %s\n",
                mdp_result_string(result));
        goto cleanup;
    }
    if (!write_file(argv[2], package, package_size)) {
        goto cleanup;
    }
    printf("%s: wrote %lu chunks, %lu bytes\n",
           argv[2], (unsigned long)count, (unsigned long)package_size);
    status = 0;

cleanup:
    mdp_buffer_free(package);
    for (i = 0u; i < count; ++i) {
        free(files[i].bytes);
    }
    free(files);
    free(chunks);
    return status;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "validate") == 0) {
        return command_validate(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "list") == 0) {
        return command_list(argv[2]);
    }
    if (argc >= 7 && strcmp(argv[1], "pack") == 0 &&
        (argc - 3) % 4 == 0) {
        return command_pack(argc, argv);
    }
    usage(argv[0]);
    return 2;
}
