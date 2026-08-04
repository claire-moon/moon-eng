#ifndef MOON_MDP_H
#define MOON_MDP_H

/*
 * MOON Data Package (MDP) v1 portable codec.
 *
 * All integers in an MDP file are encoded explicitly in little-endian order.
 * The declarations in this header intentionally do not expose packed C
 * structures: an MDP file must never depend on compiler padding or host types.
 *
 * Version 1 archives are canonical: the directory starts immediately after
 * the 32-byte header; schema versions start at one; records are sorted by the
 * four FourCC bytes and asset ID; and (type, asset ID) is the unique asset
 * key. Payloads follow in directory order at dense four-byte
 * boundaries. Alignment bytes are zero and trailing bytes are forbidden.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDP_VERSION_MAJOR 1u
#define MDP_VERSION_MINOR 0u

#define MDP_HEADER_SIZE 32u
#define MDP_DIRECTORY_RECORD_SIZE 28u
#define MDP_ALIGNMENT 4u

#define MDP_FOURCC(a, b, c, d) \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

enum {
    MDP_CHUNK_COMPRESSION_RAW = 0u,
    MDP_CHUNK_COMPRESSION_RLE = 1u,
    MDP_CHUNK_COMPRESSION_LZSS = 2u,
    MDP_CHUNK_COMPRESSION_MASK = 3u,
    MDP_CHUNK_KNOWN_FLAGS = MDP_CHUNK_COMPRESSION_MASK
};

typedef enum MdpResult {
    MDP_OK = 0,
    MDP_ERR_ARGUMENT,
    MDP_ERR_MEMORY,
    MDP_ERR_FORMAT,
    MDP_ERR_VERSION,
    MDP_ERR_BOUNDS,
    MDP_ERR_OVERFLOW,
    MDP_ERR_CRC,
    MDP_ERR_UNSUPPORTED,
    MDP_ERR_DUPLICATE
} MdpResult;

typedef struct MdpDirectoryEntry {
    uint32_t type;
    uint16_t schema_version;
    uint16_t flags;
    uint32_t asset_id;
    uint32_t offset;
    uint32_t packed_size;
    uint32_t unpacked_size;
    uint32_t crc32;
} MdpDirectoryEntry;

typedef struct MdpWriteChunk {
    uint32_t type;
    uint16_t schema_version;
    uint16_t flags;
    uint32_t asset_id;
    const void *data;
    size_t size;
} MdpWriteChunk;

/*
 * An open archive borrows the input byte buffer. The caller must keep it alive
 * and unchanged until mdp_archive_close() is called. Do not call
 * mdp_archive_open() on an archive that is already open; close it first.
 */
typedef struct MdpArchive {
    const uint8_t *data;
    size_t size;
    MdpDirectoryEntry *entries;
    uint32_t entry_count;
    uint16_t version_major;
    uint16_t version_minor;
} MdpArchive;

uint16_t mdp_read_u16le(const uint8_t *bytes);
uint32_t mdp_read_u32le(const uint8_t *bytes);
void mdp_write_u16le(uint8_t *bytes, uint16_t value);
void mdp_write_u32le(uint8_t *bytes, uint32_t value);

uint32_t mdp_crc32(const void *data, size_t size);
const char *mdp_result_string(MdpResult result);

MdpResult mdp_archive_open(MdpArchive *archive,
                           const void *data,
                           size_t size);
void mdp_archive_close(MdpArchive *archive);

const MdpDirectoryEntry *mdp_archive_entry(const MdpArchive *archive,
                                           uint32_t index);
const MdpDirectoryEntry *mdp_archive_find(const MdpArchive *archive,
                                          uint32_t type,
                                          uint32_t asset_id);
/* Returns the stored bytes and packed size. Compression-aware decoding uses a
 * separate bounded caller-buffer API when compressed chunks are enabled. */
MdpResult mdp_archive_stored_chunk(const MdpArchive *archive,
                                   const MdpDirectoryEntry *entry,
                                   const void **data,
                                   size_t *packed_size);

/*
 * The v1 writer currently emits raw chunks only. On success, *output is owned
 * by the caller and must be released with mdp_buffer_free().
 */
MdpResult mdp_write_archive(const MdpWriteChunk *chunks,
                            size_t chunk_count,
                            void **output,
                            size_t *output_size);
void mdp_buffer_free(void *buffer);

#ifdef __cplusplus
}
#endif

#endif
