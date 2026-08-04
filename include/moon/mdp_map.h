#ifndef MOON_MDP_MAP_H
#define MOON_MDP_MAP_H

/*
 * Typed MDP cell-map payload, schema version 1.
 *
 * Wire data is encoded explicitly and never by copying these native structs.
 * Map and cell storage remains caller-owned; the codec performs no allocation.
 */

#include "moon/mdp.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDP_MAP_SCHEMA_VERSION 1u
#define MDP_MAP_TYPE MDP_FOURCC('M', 'A', 'P', ' ')
#define MDP_MAP_HEADER_SIZE 24u
#define MDP_MAP_CELL_RECORD_SIZE 16u

/* Header flags are reserved in schema v1 and must be zero. */
#define MDP_MAP_KNOWN_FLAGS 0u

enum {
    MDP_MAP_CELL_SOLID   = 0x0001u,
    MDP_MAP_CELL_SKY     = 0x0002u,
    MDP_MAP_CELL_COLLIDE = 0x0004u,
    MDP_MAP_CELL_DOOR    = 0x0008u,
    MDP_MAP_CELL_TRIGGER = 0x0010u,
    MDP_MAP_CELL_SECRET  = 0x0020u,
    MDP_MAP_CELL_DAMAGE  = 0x0040u,
    MDP_MAP_CELL_TRANS   = 0x0080u,
    MDP_MAP_CELL_KNOWN_FLAGS = 0x00ffu
};

typedef struct MdpMapCell {
    int16_t floor_height;
    int16_t ceiling_height;
    uint16_t wall_height;
    uint8_t floor_material;
    uint8_t ceiling_material;
    uint8_t wall_material;
    uint8_t light;
    uint16_t tag;
    uint16_t flags;
} MdpMapCell;

typedef struct MdpMap {
    uint32_t width;
    uint32_t height;
    uint32_t cell_size;
    uint32_t skybox_id;
    uint32_t cell_count;
    uint16_t flags;
    MdpMapCell *cells;
} MdpMap;

/* Computes width * height without overflowing the 32-bit wire model. */
MdpResult mdp_map_cell_count(uint32_t width,
                             uint32_t height,
                             uint32_t *cell_count);

/* Computes the exact canonical payload size, including the 24-byte header. */
MdpResult mdp_map_encoded_size(uint32_t width,
                               uint32_t height,
                               size_t *encoded_size);

/* Validates one complete MAP payload without allocating or publishing it. */
MdpResult mdp_map_validate(const void *data, size_t size);

/*
 * Encodes one canonical MAP payload into a bounded caller-owned buffer.
 * output, map, map->cells, and output_size must not overlap. Invalid aliasing
 * returns MDP_ERR_ARGUMENT without modifying any region. On every other
 * failure, *output_size is zero and output, map, and map->cells are unchanged.
 */
MdpResult mdp_map_encode(const MdpMap *map,
                         void *output,
                         size_t output_capacity,
                         size_t *output_size);

/*
 * Decodes only after the complete payload has validated. map and cells remain
 * unchanged on failure. The input, map, and cells regions must not overlap;
 * overlap returns MDP_ERR_ARGUMENT without modifying any region.
 */
MdpResult mdp_map_decode(const void *data,
                         size_t size,
                         MdpMapCell *cells,
                         size_t cell_capacity,
                         MdpMap *map);

#ifdef __cplusplus
}
#endif

#endif
