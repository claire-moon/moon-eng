#include "moon/mdp_map.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while (0)

static const uint8_t golden_map[] = {
    /* 24-byte MAP1 header. */
    0x4du, 0x41u, 0x50u, 0x31u,
    0x01u, 0x00u, 0x00u, 0x00u,
    0x02u, 0x00u, 0x00u, 0x00u,
    0x01u, 0x00u, 0x00u, 0x00u,
    0x40u, 0x00u, 0x00u, 0x00u,
    0x04u, 0x03u, 0x02u, 0x01u,

    /* Cell 0: signed heights and SOLID|COLLIDE|SECRET. */
    0xf0u, 0xffu, 0x78u, 0x00u, 0x60u, 0x00u,
    0x01u, 0x02u, 0x03u, 0xc8u,
    0x34u, 0x12u, 0x25u, 0x00u, 0x00u, 0x00u,

    /* Cell 1: signed extremes and every remaining v1 flag. */
    0x00u, 0x80u, 0xffu, 0x7fu, 0x2cu, 0x01u,
    0x04u, 0x05u, 0x06u, 0x00u,
    0xefu, 0xbeu, 0xdau, 0x00u, 0x00u, 0x00u
};

static void build_sample(MdpMap *map, MdpMapCell cells[2])
{
    memset(map, 0, sizeof(*map));
    memset(cells, 0, 2u * sizeof(*cells));

    cells[0].floor_height = -16;
    cells[0].ceiling_height = 120;
    cells[0].wall_height = 96u;
    cells[0].floor_material = 1u;
    cells[0].ceiling_material = 2u;
    cells[0].wall_material = 3u;
    cells[0].light = 200u;
    cells[0].tag = UINT16_C(0x1234);
    cells[0].flags = MDP_MAP_CELL_SOLID |
                     MDP_MAP_CELL_COLLIDE |
                     MDP_MAP_CELL_SECRET;

    cells[1].floor_height = INT16_MIN;
    cells[1].ceiling_height = INT16_MAX;
    cells[1].wall_height = 300u;
    cells[1].floor_material = 4u;
    cells[1].ceiling_material = 5u;
    cells[1].wall_material = 6u;
    cells[1].light = 0u;
    cells[1].tag = UINT16_C(0xbeef);
    cells[1].flags = MDP_MAP_CELL_SKY |
                     MDP_MAP_CELL_DOOR |
                     MDP_MAP_CELL_TRIGGER |
                     MDP_MAP_CELL_DAMAGE |
                     MDP_MAP_CELL_TRANS;

    map->width = 2u;
    map->height = 1u;
    map->cell_size = 64u;
    map->skybox_id = UINT32_C(0x01020304);
    map->cell_count = 2u;
    map->flags = 0u;
    map->cells = cells;
}

static int test_golden_round_trip(void)
{
    MdpMap source;
    MdpMap decoded;
    MdpMapCell source_cells[2];
    MdpMapCell decoded_cells[2];
    uint8_t encoded[sizeof(golden_map)];
    size_t encoded_size = 0u;
    size_t expected_size = 0u;
    uint32_t count = 0u;

    CHECK(sizeof(golden_map) == 56u);
    CHECK(mdp_map_cell_count(2u, 1u, &count) == MDP_OK);
    CHECK(count == 2u);
    CHECK(mdp_map_encoded_size(2u, 1u, &expected_size) == MDP_OK);
    CHECK(expected_size == sizeof(golden_map));

    build_sample(&source, source_cells);
    memset(encoded, 0xa5, sizeof(encoded));
    CHECK(mdp_map_encode(&source, encoded, sizeof(encoded),
                         &encoded_size) == MDP_OK);
    CHECK(encoded_size == sizeof(golden_map));
    CHECK(memcmp(encoded, golden_map, sizeof(golden_map)) == 0);
    CHECK(MDP_MAP_TYPE == MDP_FOURCC('M', 'A', 'P', ' '));
    CHECK(mdp_map_validate(golden_map, sizeof(golden_map)) == MDP_OK);

    memset(&decoded, 0, sizeof(decoded));
    memset(decoded_cells, 0, sizeof(decoded_cells));
    CHECK(mdp_map_decode(golden_map, sizeof(golden_map),
                         decoded_cells, 2u, &decoded) == MDP_OK);
    CHECK(decoded.width == 2u);
    CHECK(decoded.height == 1u);
    CHECK(decoded.cell_size == 64u);
    CHECK(decoded.skybox_id == UINT32_C(0x01020304));
    CHECK(decoded.cell_count == 2u);
    CHECK(decoded.flags == 0u);
    CHECK(decoded.cells == decoded_cells);

    CHECK(decoded_cells[0].floor_height == -16);
    CHECK(decoded_cells[0].ceiling_height == 120);
    CHECK(decoded_cells[0].wall_height == 96u);
    CHECK(decoded_cells[0].floor_material == 1u);
    CHECK(decoded_cells[0].ceiling_material == 2u);
    CHECK(decoded_cells[0].wall_material == 3u);
    CHECK(decoded_cells[0].light == 200u);
    CHECK(decoded_cells[0].tag == UINT16_C(0x1234));
    CHECK(decoded_cells[0].flags == UINT16_C(0x0025));

    CHECK(decoded_cells[1].floor_height == INT16_MIN);
    CHECK(decoded_cells[1].ceiling_height == INT16_MAX);
    CHECK(decoded_cells[1].wall_height == 300u);
    CHECK(decoded_cells[1].floor_material == 4u);
    CHECK(decoded_cells[1].ceiling_material == 5u);
    CHECK(decoded_cells[1].wall_material == 6u);
    CHECK(decoded_cells[1].light == 0u);
    CHECK(decoded_cells[1].tag == UINT16_C(0xbeef));
    CHECK(decoded_cells[1].flags == UINT16_C(0x00da));
    CHECK((decoded_cells[0].flags | decoded_cells[1].flags) ==
          MDP_MAP_CELL_KNOWN_FLAGS);
    return 1;
}

static int expect_decode_failure(const uint8_t *bytes,
                                 size_t size,
                                 size_t capacity,
                                 MdpResult expected)
{
    MdpMap map;
    MdpMapCell cells[2];
    uint8_t map_before[sizeof(map)];
    uint8_t cells_before[sizeof(cells)];

    memset(&map, 0xa5, sizeof(map));
    memset(cells, 0x5a, sizeof(cells));
    memcpy(map_before, &map, sizeof(map));
    memcpy(cells_before, cells, sizeof(cells));

    CHECK(mdp_map_decode(bytes, size, cells, capacity, &map) == expected);
    CHECK(memcmp(&map, map_before, sizeof(map)) == 0);
    CHECK(memcmp(cells, cells_before, sizeof(cells)) == 0);
    return 1;
}

static int expect_wire_failure(const uint8_t *bytes,
                               size_t size,
                               MdpResult expected)
{
    CHECK(mdp_map_validate(bytes, size) == expected);
    CHECK(expect_decode_failure(bytes, size, 2u, expected));
    return 1;
}

static int test_decode_rejections(void)
{
    uint8_t copy[sizeof(golden_map) + 1u];

    memcpy(copy, golden_map, sizeof(golden_map));
    copy[0] ^= 1u;
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_FORMAT));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u16le(copy + 4u, 2u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_VERSION));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u16le(copy + 6u, 1u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map),
                              MDP_ERR_UNSUPPORTED));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u32le(copy + 8u, 0u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_FORMAT));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u32le(copy + 16u, 0u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_FORMAT));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u32le(copy + 8u, UINT32_MAX);
    mdp_write_u32le(copy + 12u, 2u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_OVERFLOW));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u32le(copy + 8u, UINT32_C(268435455));
    mdp_write_u32le(copy + 12u, 1u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_OVERFLOW));

    CHECK(expect_wire_failure(golden_map, sizeof(golden_map) - 1u,
                              MDP_ERR_BOUNDS));

    memcpy(copy, golden_map, sizeof(golden_map));
    copy[sizeof(golden_map)] = 0u;
    CHECK(expect_wire_failure(copy, sizeof(copy), MDP_ERR_FORMAT));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u16le(copy + MDP_MAP_HEADER_SIZE + 0u, 121u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_FORMAT));

    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u16le(copy + MDP_MAP_HEADER_SIZE + 12u, 0x0100u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map),
                              MDP_ERR_UNSUPPORTED));

    /* Corrupt the final field of the final cell to prove late failures
       cannot publish the already-valid first cell. */
    memcpy(copy, golden_map, sizeof(golden_map));
    mdp_write_u16le(copy + MDP_MAP_HEADER_SIZE +
                    MDP_MAP_CELL_RECORD_SIZE + 14u, 1u);
    CHECK(expect_wire_failure(copy, sizeof(golden_map), MDP_ERR_FORMAT));

    CHECK(expect_decode_failure(golden_map, sizeof(golden_map), 1u,
                                MDP_ERR_BOUNDS));
    return 1;
}

static int test_size_boundaries(void)
{
    uint32_t cell_count = 99u;
    size_t encoded_size = 99u;

    CHECK(mdp_map_cell_count(0u, 1u, &cell_count) == MDP_ERR_FORMAT);
    CHECK(cell_count == 0u);
    CHECK(mdp_map_cell_count(UINT32_MAX, 2u, &cell_count) ==
          MDP_ERR_OVERFLOW);
    CHECK(cell_count == 0u);
    CHECK(mdp_map_cell_count(UINT32_MAX, 1u, &cell_count) == MDP_OK);
    CHECK(cell_count == UINT32_MAX);

    CHECK(mdp_map_encoded_size(UINT32_MAX, 1u, &encoded_size) ==
          MDP_ERR_OVERFLOW);
    CHECK(encoded_size == 0u);
    CHECK(mdp_map_encoded_size(UINT32_C(268435455), 1u,
                               &encoded_size) == MDP_ERR_OVERFLOW);
    CHECK(encoded_size == 0u);
    CHECK(mdp_map_encoded_size(UINT32_C(268435454), 1u,
                               &encoded_size) == MDP_OK);
    CHECK(encoded_size == (size_t)UINT32_C(4294967288));
    CHECK(mdp_map_cell_count(1u, 1u, NULL) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_encoded_size(1u, 1u, NULL) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_validate(NULL, 0u) == MDP_ERR_ARGUMENT);
    return 1;
}

static int test_encode_rejections(void)
{
    MdpMap map;
    MdpMapCell cells[2];
    uint8_t output[sizeof(golden_map)];
    uint8_t before[sizeof(output)];
    size_t written;

    build_sample(&map, cells);
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(output));

    written = 99u;
    CHECK(mdp_map_encode(&map, output, sizeof(output) - 1u,
                         &written) == MDP_ERR_BOUNDS);
    CHECK(written == 0u);
    CHECK(memcmp(output, before, sizeof(output)) == 0);

    map.cell_count = 1u;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_FORMAT);
    CHECK(written == 0u);
    map.cell_count = 2u;

    map.cell_size = 0u;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_FORMAT);
    map.cell_size = 64u;

    map.flags = 1u;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_UNSUPPORTED);
    map.flags = 0u;

    cells[1].flags |= 0x0100u;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_UNSUPPORTED);
    cells[1].flags &= MDP_MAP_CELL_KNOWN_FLAGS;

    cells[1].floor_height = INT16_MAX;
    cells[1].ceiling_height = INT16_MIN;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_FORMAT);
    cells[1].floor_height = INT16_MIN;
    cells[1].ceiling_height = INT16_MAX;

    map.cells = NULL;
    CHECK(mdp_map_encode(&map, output, sizeof(output),
                         &written) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_encode(NULL, output, sizeof(output),
                         &written) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_encode(&map, NULL, sizeof(output),
                         &written) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_encode(&map, output, sizeof(output), NULL) ==
          MDP_ERR_ARGUMENT);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    return 1;
}

static int test_decode_argument_and_overlap_rejections(void)
{
    union AlignedBytes {
        uint8_t bytes[sizeof(golden_map)];
        MdpMap map_alignment;
        MdpMapCell cell_alignment;
    } shared;
    union OverlappingDestination {
        MdpMap map;
        MdpMapCell cells[4];
    } destination;
    MdpMap map;
    MdpMapCell cells[2];
    uint8_t destination_before[sizeof(destination)];

    memcpy(shared.bytes, golden_map, sizeof(golden_map));
    memset(&map, 0, sizeof(map));
    memset(cells, 0, sizeof(cells));

    CHECK(mdp_map_decode(NULL, sizeof(golden_map), cells, 2u, &map) ==
          MDP_ERR_ARGUMENT);
    CHECK(mdp_map_decode(golden_map, sizeof(golden_map), NULL, 2u, &map) ==
          MDP_ERR_ARGUMENT);
    CHECK(mdp_map_decode(golden_map, sizeof(golden_map), cells, 2u, NULL) ==
          MDP_ERR_ARGUMENT);
    CHECK(mdp_map_decode(golden_map, MDP_MAP_HEADER_SIZE - 1u,
                         cells, 2u, &map) == MDP_ERR_BOUNDS);

    CHECK(mdp_map_decode(shared.bytes, sizeof(shared.bytes),
                         (MdpMapCell *)(void *)(shared.bytes +
                                                MDP_MAP_HEADER_SIZE),
                         2u, &map) == MDP_ERR_ARGUMENT);
    CHECK(mdp_map_decode(shared.bytes, sizeof(shared.bytes), cells, 2u,
                         (MdpMap *)(void *)shared.bytes) ==
          MDP_ERR_ARGUMENT);

    memset(&destination, 0xa5, sizeof(destination));
    memcpy(destination_before, &destination, sizeof(destination));
    CHECK(mdp_map_decode(golden_map, sizeof(golden_map),
                         destination.cells, 2u, &destination.map) ==
          MDP_ERR_ARGUMENT);
    CHECK(memcmp(&destination, destination_before,
                 sizeof(destination)) == 0);
    return 1;
}

static int test_encode_overlap_rejections(void)
{
    union OutputWithAlignment {
        uint8_t bytes[sizeof(golden_map)];
        size_t size_alignment;
    } output_store;
    union CellsWithAlignment {
        uint8_t bytes[sizeof(golden_map)];
        MdpMapCell cells[4];
        size_t size_alignment;
    } cell_store;
    union MapWithOutput {
        uint8_t bytes[sizeof(golden_map)];
        MdpMap map;
    } map_store;
    MdpMap map;
    MdpMapCell cells[2];
    uint8_t output_before[sizeof(output_store.bytes)];
    uint8_t cells_before[sizeof(cell_store)];
    uint8_t map_before[sizeof(map)];
    size_t written;

    build_sample(&map, cells);
    memset(output_store.bytes, 0xa5, sizeof(output_store.bytes));
    memcpy(output_before, output_store.bytes, sizeof(output_before));
    CHECK(mdp_map_encode(&map, output_store.bytes,
                         sizeof(output_store.bytes),
                         (size_t *)(void *)output_store.bytes) ==
          MDP_ERR_ARGUMENT);
    CHECK(memcmp(output_store.bytes, output_before,
                 sizeof(output_before)) == 0);

    build_sample(&map, cells);
    memset(output_store.bytes, 0xa5, sizeof(output_store.bytes));
    memcpy(map_before, &map, sizeof(map));
    memcpy(output_before, output_store.bytes, sizeof(output_before));
    written = 99u;
    CHECK(mdp_map_encode(&map, output_store.bytes,
                         sizeof(output_store.bytes),
                         (size_t *)(void *)&map) == MDP_ERR_ARGUMENT);
    CHECK(memcmp(&map, map_before, sizeof(map)) == 0);
    CHECK(memcmp(output_store.bytes, output_before,
                 sizeof(output_before)) == 0);

    build_sample(&map, cell_store.cells);
    memset(output_store.bytes, 0xa5, sizeof(output_store.bytes));
    memcpy(cells_before, &cell_store, sizeof(cell_store));
    memcpy(output_before, output_store.bytes, sizeof(output_before));
    CHECK(mdp_map_encode(&map, output_store.bytes,
                         sizeof(output_store.bytes),
                         (size_t *)(void *)cell_store.cells) ==
          MDP_ERR_ARGUMENT);
    CHECK(memcmp(&cell_store, cells_before, sizeof(cell_store)) == 0);
    CHECK(memcmp(output_store.bytes, output_before,
                 sizeof(output_before)) == 0);

    /* Aliasing takes precedence even when a later map field is malformed. */
    map.width = 0u;
    CHECK(mdp_map_encode(&map, output_store.bytes,
                         sizeof(output_store.bytes),
                         (size_t *)(void *)cell_store.cells) ==
          MDP_ERR_ARGUMENT);
    CHECK(memcmp(&cell_store, cells_before, sizeof(cell_store)) == 0);
    CHECK(memcmp(output_store.bytes, output_before,
                 sizeof(output_before)) == 0);

    memset(&cell_store, 0, sizeof(cell_store));
    build_sample(&map, cell_store.cells);
    memcpy(cells_before, &cell_store, sizeof(cell_store));
    written = 99u;
    CHECK(mdp_map_encode(&map, cell_store.bytes, sizeof(cell_store.bytes),
                         &written) == MDP_ERR_ARGUMENT);
    CHECK(written == 99u);
    CHECK(memcmp(&cell_store, cells_before, sizeof(cell_store)) == 0);

    memset(&map_store, 0, sizeof(map_store));
    build_sample(&map_store.map, cells);
    memcpy(output_before, map_store.bytes, sizeof(output_before));
    written = 99u;
    CHECK(mdp_map_encode(&map_store.map, map_store.bytes,
                         sizeof(map_store.bytes), &written) ==
          MDP_ERR_ARGUMENT);
    CHECK(written == 99u);
    CHECK(memcmp(map_store.bytes, output_before, sizeof(output_before)) == 0);

    build_sample(&map, cells);
    map.cells = (MdpMapCell *)(void *)&map;
    memcpy(map_before, &map, sizeof(map));
    memset(output_store.bytes, 0xa5, sizeof(output_store.bytes));
    memcpy(output_before, output_store.bytes, sizeof(output_before));
    written = 99u;
    CHECK(mdp_map_encode(&map, output_store.bytes,
                         sizeof(output_store.bytes), &written) ==
          MDP_ERR_ARGUMENT);
    CHECK(written == 99u);
    CHECK(memcmp(&map, map_before, sizeof(map)) == 0);
    CHECK(memcmp(output_store.bytes, output_before,
                 sizeof(output_before)) == 0);
    return 1;
}

int main(void)
{
    if (!test_golden_round_trip() ||
        !test_decode_rejections() ||
        !test_size_boundaries() ||
        !test_encode_rejections() ||
        !test_decode_argument_and_overlap_rejections() ||
        !test_encode_overlap_rejections()) {
        return 1;
    }
    puts("PASS: MDP MAP v1 codec tests");
    return 0;
}
