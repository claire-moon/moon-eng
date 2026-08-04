# MDP MAP v1 Cell-Map Payload

Status: implemented typed schema foundation.

This document freezes schema version 1 of the `MAP ` typed payload stored in an
MDP v1 directory record. The directory record's stable asset ID identifies the
map. Its schema version is `1`; the payload also begins with `MAP1` so it can be
identified and validated independently during editor and migration work.
Code uses the canonical `MDP_MAP_TYPE` FourCC constant rather than spelling the
type independently in the compiler and runtime.

The codec is portable C99 shared by host tools and DJGPP. It allocates nothing:
the caller supplies both encoded bytes and decoded cell storage. Native structs
are never copied to or from the wire.

## Canonical layout

All multi-byte values are explicitly little-endian. The payload is exactly one
24-byte header followed by `width * height` 16-byte cell records. Cells are
row-major: cell `(x, y)` has index `y * width + x`. Empty maps, gaps, and
trailing bytes are invalid.

### Header: 24 bytes

| Offset | Width | Field | v1 rule |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `MAP1` |
| 4 | 2 | schema version | `1` |
| 6 | 2 | map flags | zero; reserved for later schemas |
| 8 | 4 | width | nonzero cell count on X |
| 12 | 4 | height | nonzero cell count on Y |
| 16 | 4 | cell size | nonzero world units per cell |
| 20 | 4 | skybox asset ID | stable ID of the active skybox asset |

The product `width * height` must fit in 32 bits. The complete typed payload
must also fit the MDP v1 32-bit chunk-size model. Both conditions are checked
before any cell offset is formed.

### Cell record: 16 bytes

| Offset | Width | Field | v1 rule |
| ---: | ---: | --- | --- |
| 0 | 2 | floor height | signed 16-bit two's-complement world height |
| 2 | 2 | ceiling height | signed 16-bit two's-complement world height |
| 4 | 2 | wall height | unsigned 16-bit vertical wall extent |
| 6 | 1 | floor material | map-local material slot, 0 through 255 |
| 7 | 1 | ceiling material | map-local material slot, 0 through 255 |
| 8 | 1 | wall material | map-local material slot, 0 through 255 |
| 9 | 1 | light | authored light level, 0 through 255 |
| 10 | 2 | tag | gameplay/editor link tag |
| 12 | 2 | flags | only the v1 bits below are accepted |
| 14 | 2 | reserved | canonical zero |

`floor height` must not exceed `ceiling height`. Those two elevations are
decoded through an explicit signed conversion, so negative values and `-32768`
do not depend on the compiler's native struct representation. Wall height is a
nonnegative extent from 0 through 65535.

Cell flag bits preserve the map-format WIP vocabulary:

| Bit | Name | Meaning |
| ---: | --- | --- |
| `0001` | `SOLID` | cell contains solid geometry |
| `0002` | `SKY` | cell exposes the active sky |
| `0004` | `COLLIDE` | cell participates in collision |
| `0008` | `DOOR` | cell is controlled as a door |
| `0010` | `TRIGGER` | entering or using the cell can fire its tag |
| `0020` | `SECRET` | cell participates in secret discovery |
| `0040` | `DAMAGE` | cell applies an authored damage behavior |
| `0080` | `TRANS` | cell geometry/material is translucent |

Bits `0100` through `8000` are unknown in schema v1 and are rejected rather
than ignored.

## Validation and publication

Decoding performs these operations before publishing anything:

1. validate magic, exact schema, reserved header flags, and nonzero cell size;
2. validate the dimension product and exact bounded payload size;
3. reject truncation, insufficient cell capacity, overlap, and trailing bytes;
4. scan every cell for unknown flags, nonzero reserved bytes, and inverted
   floor/ceiling heights;
5. decode all cells, then publish the map header and cell pointer.

Consequently a failed decode leaves both the destination `MdpMap` and the full
caller-provided cell array unchanged. Input bytes, the map destination, and the
cell destination must be disjoint. Encoding likewise validates the complete
map before modifying its bounded destination. An encode alias error returns
`MDP_ERR_ARGUMENT` with every region, including `output_size`, untouched. Every
other encode failure sets `output_size` to zero and leaves the output, map, and
cell storage unchanged.

Failure classes are stable across host and DJGPP builds:

- `MDP_ERR_ARGUMENT`: null required pointer or overlapping regions;
- `MDP_ERR_VERSION`: schema other than exactly 1;
- `MDP_ERR_UNSUPPORTED`: nonzero header flags or unknown cell flags;
- `MDP_ERR_OVERFLOW`: dimension product or complete encoded size is too large;
- `MDP_ERR_BOUNDS`: truncated input, short output, or short cell array;
- `MDP_ERR_FORMAT`: bad magic, zero dimension/cell size, trailing bytes,
  inverted floor/ceiling heights, or nonzero reserved cell data.

This is structural typed validation. Later asset-manager work resolves the
skybox/material references and gameplay tags before making a map active.

## Frozen golden vector

The schema test freezes a 56-byte, two-cell payload. It deliberately covers
negative values, signed extremes, every v1 cell flag, all material fields,
light, tag, cell size, and a multi-byte skybox ID.

```text
0000: 4d 41 50 31 01 00 00 00 02 00 00 00 01 00 00 00
0010: 40 00 00 00 04 03 02 01 f0 ff 78 00 60 00 01 02
0020: 03 c8 34 12 25 00 00 00 00 80 ff 7f 2c 01 04 05
0030: 06 00 ef be da 00 00 00
```

The writer must reproduce these exact bytes, and the reader must decode this
fixed vector independently of writer output.
