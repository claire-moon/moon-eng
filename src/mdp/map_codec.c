#include "moon/mdp_map.h"

#include <limits.h>
#include <string.h>

#define MDP_MAP_CELL_FLOOR_HEIGHT_OFFSET 0u
#define MDP_MAP_CELL_CEILING_HEIGHT_OFFSET 2u
#define MDP_MAP_CELL_WALL_HEIGHT_OFFSET 4u
#define MDP_MAP_CELL_FLOOR_MATERIAL_OFFSET 6u
#define MDP_MAP_CELL_CEILING_MATERIAL_OFFSET 7u
#define MDP_MAP_CELL_WALL_MATERIAL_OFFSET 8u
#define MDP_MAP_CELL_LIGHT_OFFSET 9u
#define MDP_MAP_CELL_TAG_OFFSET 10u
#define MDP_MAP_CELL_FLAGS_OFFSET 12u
#define MDP_MAP_CELL_RESERVED_OFFSET 14u

typedef struct MdpMapWireInfo {
    uint16_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t cell_size;
    uint32_t skybox_id;
    uint32_t cell_count;
} MdpMapWireInfo;

static int mdp_map_mul_size(size_t left, size_t right, size_t *result)
{
    if (left != 0u && right > SIZE_MAX / left) {
        return 0;
    }
    *result = left * right;
    return 1;
}

static int mdp_map_add_size(size_t left, size_t right, size_t *result)
{
    if (left > SIZE_MAX - right) {
        return 0;
    }
    *result = left + right;
    return 1;
}

static int mdp_map_range(const void *pointer,
                         size_t length,
                         uintptr_t *begin,
                         uintptr_t *end)
{
    uintptr_t start = (uintptr_t)pointer;

    if (length > (size_t)UINTPTR_MAX ||
        start > UINTPTR_MAX - (uintptr_t)length) {
        return 0;
    }
    *begin = start;
    *end = start + (uintptr_t)length;
    return 1;
}

static int mdp_map_ranges_overlap(const void *left,
                                  size_t left_size,
                                  const void *right,
                                  size_t right_size)
{
    uintptr_t left_begin;
    uintptr_t left_end;
    uintptr_t right_begin;
    uintptr_t right_end;

    if (left_size == 0u || right_size == 0u) {
        return 0;
    }
    if (!mdp_map_range(left, left_size, &left_begin, &left_end) ||
        !mdp_map_range(right, right_size, &right_begin, &right_end)) {
        return 1;
    }
    return left_begin < right_end && right_begin < left_end;
}

static int16_t mdp_map_read_i16le(const uint8_t *bytes)
{
    uint16_t wire = mdp_read_u16le(bytes);
    int32_t value;

    if ((wire & UINT16_C(0x8000)) == 0u) {
        value = (int32_t)wire;
    } else {
        value = (int32_t)wire - INT32_C(65536);
    }
    return (int16_t)value;
}

static void mdp_map_write_i16le(uint8_t *bytes, int16_t value)
{
    mdp_write_u16le(bytes, (uint16_t)value);
}

static MdpResult mdp_map_validate_cell(const MdpMapCell *cell)
{
    if ((cell->flags &
         (uint16_t)~(uint16_t)MDP_MAP_CELL_KNOWN_FLAGS) != 0u) {
        return MDP_ERR_UNSUPPORTED;
    }
    if (cell->floor_height > cell->ceiling_height) {
        return MDP_ERR_FORMAT;
    }
    return MDP_OK;
}

static void mdp_map_read_cell(MdpMapCell *cell, const uint8_t *record)
{
    cell->floor_height =
        mdp_map_read_i16le(record + MDP_MAP_CELL_FLOOR_HEIGHT_OFFSET);
    cell->ceiling_height =
        mdp_map_read_i16le(record + MDP_MAP_CELL_CEILING_HEIGHT_OFFSET);
    cell->wall_height =
        mdp_read_u16le(record + MDP_MAP_CELL_WALL_HEIGHT_OFFSET);
    cell->floor_material = record[MDP_MAP_CELL_FLOOR_MATERIAL_OFFSET];
    cell->ceiling_material = record[MDP_MAP_CELL_CEILING_MATERIAL_OFFSET];
    cell->wall_material = record[MDP_MAP_CELL_WALL_MATERIAL_OFFSET];
    cell->light = record[MDP_MAP_CELL_LIGHT_OFFSET];
    cell->tag = mdp_read_u16le(record + MDP_MAP_CELL_TAG_OFFSET);
    cell->flags = mdp_read_u16le(record + MDP_MAP_CELL_FLAGS_OFFSET);
}

static void mdp_map_write_cell(uint8_t *record, const MdpMapCell *cell)
{
    mdp_map_write_i16le(record + MDP_MAP_CELL_FLOOR_HEIGHT_OFFSET,
                        cell->floor_height);
    mdp_map_write_i16le(record + MDP_MAP_CELL_CEILING_HEIGHT_OFFSET,
                        cell->ceiling_height);
    mdp_write_u16le(record + MDP_MAP_CELL_WALL_HEIGHT_OFFSET,
                    cell->wall_height);
    record[MDP_MAP_CELL_FLOOR_MATERIAL_OFFSET] = cell->floor_material;
    record[MDP_MAP_CELL_CEILING_MATERIAL_OFFSET] = cell->ceiling_material;
    record[MDP_MAP_CELL_WALL_MATERIAL_OFFSET] = cell->wall_material;
    record[MDP_MAP_CELL_LIGHT_OFFSET] = cell->light;
    mdp_write_u16le(record + MDP_MAP_CELL_TAG_OFFSET, cell->tag);
    mdp_write_u16le(record + MDP_MAP_CELL_FLAGS_OFFSET, cell->flags);
    mdp_write_u16le(record + MDP_MAP_CELL_RESERVED_OFFSET, 0u);
}

MdpResult mdp_map_cell_count(uint32_t width,
                             uint32_t height,
                             uint32_t *cell_count)
{
    if (cell_count == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    *cell_count = 0u;
    if (width == 0u || height == 0u) {
        return MDP_ERR_FORMAT;
    }
    if (width > UINT32_MAX / height) {
        return MDP_ERR_OVERFLOW;
    }
    *cell_count = width * height;
    return MDP_OK;
}

MdpResult mdp_map_encoded_size(uint32_t width,
                               uint32_t height,
                               size_t *encoded_size)
{
    uint32_t cell_count;
    size_t cells_size;
    size_t total_size;
    MdpResult result;

    if (encoded_size == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    *encoded_size = 0u;
    result = mdp_map_cell_count(width, height, &cell_count);
    if (result != MDP_OK) {
        return result;
    }
    if (!mdp_map_mul_size((size_t)cell_count,
                          (size_t)MDP_MAP_CELL_RECORD_SIZE,
                          &cells_size) ||
        !mdp_map_add_size((size_t)MDP_MAP_HEADER_SIZE,
                          cells_size,
                          &total_size) ||
        total_size > UINT32_MAX) {
        return MDP_ERR_OVERFLOW;
    }
    *encoded_size = total_size;
    return MDP_OK;
}

static MdpResult mdp_map_validate_wire(const void *data,
                                       size_t size,
                                       MdpMapWireInfo *info)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t version;
    uint16_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t cell_size;
    uint32_t skybox_id;
    uint32_t cell_count;
    size_t expected_size;
    uint32_t i;
    MdpResult result;

    if (data == NULL || info == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    if (size < MDP_MAP_HEADER_SIZE) {
        return MDP_ERR_BOUNDS;
    }
    if (memcmp(bytes, "MAP1", 4u) != 0) {
        return MDP_ERR_FORMAT;
    }

    version = mdp_read_u16le(bytes + 4u);
    flags = mdp_read_u16le(bytes + 6u);
    width = mdp_read_u32le(bytes + 8u);
    height = mdp_read_u32le(bytes + 12u);
    cell_size = mdp_read_u32le(bytes + 16u);
    skybox_id = mdp_read_u32le(bytes + 20u);

    if (version != MDP_MAP_SCHEMA_VERSION) {
        return MDP_ERR_VERSION;
    }
    if ((flags & (uint16_t)~(uint16_t)MDP_MAP_KNOWN_FLAGS) != 0u) {
        return MDP_ERR_UNSUPPORTED;
    }
    if (cell_size == 0u) {
        return MDP_ERR_FORMAT;
    }
    result = mdp_map_cell_count(width, height, &cell_count);
    if (result != MDP_OK) {
        return result;
    }
    result = mdp_map_encoded_size(width, height, &expected_size);
    if (result != MDP_OK) {
        return result;
    }
    if (size < expected_size) {
        return MDP_ERR_BOUNDS;
    }
    if (size > expected_size) {
        return MDP_ERR_FORMAT;
    }

    for (i = 0u; i < cell_count; ++i) {
        const uint8_t *record = bytes + MDP_MAP_HEADER_SIZE +
                                (size_t)i * MDP_MAP_CELL_RECORD_SIZE;
        MdpMapCell candidate;

        if (mdp_read_u16le(record + MDP_MAP_CELL_RESERVED_OFFSET) != 0u) {
            return MDP_ERR_FORMAT;
        }
        mdp_map_read_cell(&candidate, record);
        result = mdp_map_validate_cell(&candidate);
        if (result != MDP_OK) {
            return result;
        }
    }

    info->flags = flags;
    info->width = width;
    info->height = height;
    info->cell_size = cell_size;
    info->skybox_id = skybox_id;
    info->cell_count = cell_count;
    return MDP_OK;
}

MdpResult mdp_map_validate(const void *data, size_t size)
{
    MdpMapWireInfo info;
    return mdp_map_validate_wire(data, size, &info);
}

MdpResult mdp_map_encode(const MdpMap *map,
                         void *output,
                         size_t output_capacity,
                         size_t *output_size)
{
    uint8_t *bytes = (uint8_t *)output;
    uint32_t expected_cell_count;
    size_t expected_size;
    size_t declared_cells_size;
    uint32_t i;
    MdpResult result;

    if (output_size == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    /* Aliasing is an argument error, not an ordinary failed publication.
       Detect what can be checked without dereferencing map first. */
    if ((map != NULL &&
         mdp_map_ranges_overlap(output_size, sizeof(*output_size),
                                map, sizeof(*map))) ||
        (output != NULL && output_capacity != 0u &&
         mdp_map_ranges_overlap(output_size, sizeof(*output_size),
                                output, output_capacity)) ||
        (map != NULL && output != NULL && output_capacity != 0u &&
         mdp_map_ranges_overlap(output, output_capacity,
                                map, sizeof(*map)))) {
        return MDP_ERR_ARGUMENT;
    }
    if (map == NULL || output == NULL) {
        *output_size = 0u;
        return MDP_ERR_ARGUMENT;
    }

    /* map->cell_count defines the caller-owned source-cell region even when
       another map field is malformed. Resolve all remaining aliasing before
       an ordinary error is allowed to clear *output_size. */
    if (map->cells != NULL) {
        if (!mdp_map_mul_size((size_t)map->cell_count,
                              sizeof(*map->cells),
                              &declared_cells_size)) {
            return MDP_ERR_ARGUMENT;
        }
        if (mdp_map_ranges_overlap(output, output_capacity,
                                   map->cells, declared_cells_size) ||
            mdp_map_ranges_overlap(map, sizeof(*map),
                                   map->cells, declared_cells_size) ||
            mdp_map_ranges_overlap(output_size, sizeof(*output_size),
                                   map->cells, declared_cells_size)) {
            return MDP_ERR_ARGUMENT;
        }
    }

    *output_size = 0u;

    result = mdp_map_cell_count(map->width, map->height,
                                &expected_cell_count);
    if (result != MDP_OK) {
        return result;
    }
    if (map->cells == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    result = mdp_map_encoded_size(map->width, map->height, &expected_size);
    if (result != MDP_OK) {
        return result;
    }
    if ((map->flags & (uint16_t)~(uint16_t)MDP_MAP_KNOWN_FLAGS) != 0u) {
        return MDP_ERR_UNSUPPORTED;
    }
    if (map->cell_size == 0u) {
        return MDP_ERR_FORMAT;
    }
    if (map->cell_count != expected_cell_count) {
        return MDP_ERR_FORMAT;
    }
    if (output_capacity < expected_size) {
        return MDP_ERR_BOUNDS;
    }

    for (i = 0u; i < expected_cell_count; ++i) {
        result = mdp_map_validate_cell(&map->cells[i]);
        if (result != MDP_OK) {
            return result;
        }
    }

    memcpy(bytes, "MAP1", 4u);
    mdp_write_u16le(bytes + 4u, MDP_MAP_SCHEMA_VERSION);
    mdp_write_u16le(bytes + 6u, map->flags);
    mdp_write_u32le(bytes + 8u, map->width);
    mdp_write_u32le(bytes + 12u, map->height);
    mdp_write_u32le(bytes + 16u, map->cell_size);
    mdp_write_u32le(bytes + 20u, map->skybox_id);

    for (i = 0u; i < expected_cell_count; ++i) {
        uint8_t *record = bytes + MDP_MAP_HEADER_SIZE +
                          (size_t)i * MDP_MAP_CELL_RECORD_SIZE;
        mdp_map_write_cell(record, &map->cells[i]);
    }

    *output_size = expected_size;
    return MDP_OK;
}

MdpResult mdp_map_decode(const void *data,
                         size_t size,
                         MdpMapCell *cells,
                         size_t cell_capacity,
                         MdpMap *map)
{
    const uint8_t *bytes = (const uint8_t *)data;
    MdpMapWireInfo info;
    size_t native_cells_size;
    uint32_t i;
    MdpResult result;

    if (data == NULL || cells == NULL || map == NULL) {
        return MDP_ERR_ARGUMENT;
    }
    result = mdp_map_validate_wire(data, size, &info);
    if (result != MDP_OK) {
        return result;
    }
    if (cell_capacity < (size_t)info.cell_count) {
        return MDP_ERR_BOUNDS;
    }
    if (!mdp_map_mul_size((size_t)info.cell_count,
                          sizeof(*cells),
                          &native_cells_size)) {
        return MDP_ERR_OVERFLOW;
    }
    if (mdp_map_ranges_overlap(data, size, cells, native_cells_size) ||
        mdp_map_ranges_overlap(data, size, map, sizeof(*map)) ||
        mdp_map_ranges_overlap(cells, native_cells_size,
                               map, sizeof(*map))) {
        return MDP_ERR_ARGUMENT;
    }

    /* Publication pass cannot fail after complete validation. */
    for (i = 0u; i < info.cell_count; ++i) {
        const uint8_t *record = bytes + MDP_MAP_HEADER_SIZE +
                                (size_t)i * MDP_MAP_CELL_RECORD_SIZE;
        mdp_map_read_cell(&cells[i], record);
    }

    map->width = info.width;
    map->height = info.height;
    map->cell_size = info.cell_size;
    map->skybox_id = info.skybox_id;
    map->cell_count = info.cell_count;
    map->flags = info.flags;
    map->cells = cells;
    return MDP_OK;
}
