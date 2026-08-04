# MDP v1 Binary Contract

Status: implemented structural foundation. The current codec reads and writes
canonical raw chunks. RLE and LZSS flag values are reserved by this contract
but are rejected until their bounded decoders land in Milestone 1.

An `.MDP` is a compiled runtime package, not an editable project. MDPed and the
host compiler consume an 8.3-safe source project and emit the same canonical
bytes through the shared C99 codec.

## Integer and layout rules

- Every integer is unsigned little-endian with an explicit width.
- File offsets and stored sizes are 32-bit values.
- The complete file size must equal the header's `file_size` value.
- The header and every payload begin on a four-byte boundary.
- Padding bytes are zero. Unreferenced gaps and trailing bytes are invalid.
- Native structs, compiler padding, pointers, and host-sized integers never
  appear in the file.

## Header

The header is exactly 32 bytes.

| Offset | Width | Field | Required v1.0 value |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `MDP1` |
| 4 | 2 | major | `1` |
| 6 | 2 | minor | `0` |
| 8 | 4 | header size | `32` |
| 12 | 4 | directory offset | `32` |
| 16 | 4 | directory record count | package-specific |
| 20 | 4 | directory record size | `28` |
| 24 | 4 | complete file size | exact byte count |
| 28 | 4 | directory CRC-32 | CRC-32 of all directory bytes |

The CRC polynomial and initialization match the standard CRC-32 check value
`cbf43926` for ASCII `123456789`.

## Directory record

Each record is exactly 28 bytes.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | FourCC type |
| 4 | 2 | schema version |
| 6 | 2 | flags |
| 8 | 4 | stable asset ID |
| 12 | 4 | payload offset |
| 16 | 4 | packed size |
| 20 | 4 | unpacked size |
| 24 | 4 | payload CRC-32 |

Schema zero is invalid. A runtime asset is uniquely identified by `(FourCC,
asset_id)`; schema version is metadata and is not part of the key. Two records
with the same asset key are invalid even when their schema versions differ.

Records use this strict canonical order:

1. compare the four FourCC bytes from first character to fourth;
2. compare numeric asset ID.

Payloads occur in that same record order. The first offset is the aligned byte
immediately after the directory. Every later offset is the four-byte-aligned
end of the previous packed payload. A zero-length payload occupies no bytes but
still records the current canonical offset. This rule makes duplicate,
ordering, overlap, gap, and trailing-data validation linear in record count.

## Flags and payload integrity

The low two flag bits select compression:

| Value | Meaning | Current support |
| ---: | --- | --- |
| 0 | raw | read and write |
| 1 | RLE | reserved; rejected |
| 2 | LZSS | reserved; rejected |
| 3 | invalid/reserved | rejected |

All other flag bits are currently unknown and cause `MDP_ERR_UNSUPPORTED`.
The payload CRC always covers the canonical, fully unpacked asset bytes. For a
raw chunk those are the stored bytes, so packed and unpacked sizes must match.
Future RLE/LZSS decoders must produce exactly `unpacked_size` bytes and verify
this CRC before a typed asset can be published.

## Validation and publication

`mdp_archive_open` validates the complete header, directory, canonical order,
asset uniqueness, offsets, bounds, padding, flags, sizes, and every currently
supported payload CRC before publishing the structural archive. Failure frees
temporary directory state and publishes no entry. The function is linear in
file bytes plus directory count; it never compares every record pair.

Typed schema, cross-reference, and asset-specific bounds validation forms a
second transaction. Game/editor asset registries publish nothing until every
required typed chunk in that transaction validates. Structural success alone
does not make an unknown schema usable.

`MDPC` currently recognizes `MAP ` schema 1 and applies its allocation-free
typed validator during both `pack` and `validate`. Its payload contract is
frozen separately in [`MDP_MAP_V1.md`](MDP_MAP_V1.md). Unknown generic FourCCs
remain structurally inspectable; a known typed FourCC with an unsupported
schema is not accepted as a usable package.

The archive borrows its input byte buffer. Callers keep that buffer alive and
unchanged until `mdp_archive_close`, and close an archive before reopening it.
`mdp_archive_stored_chunk` accepts only an entry owned by that archive and
returns stored bytes plus `packed_size`. Compression uses a separate bounded
caller-buffer decode API; this accessor will never pretend stored bytes are
already unpacked.

## Determinism

`mdp_write_archive` sorts input descriptions into canonical order and clears
all padding. Reversing the input chunk order therefore produces byte-identical
output. Duplicate keys, schema zero, unsupported flags, a missing data pointer,
and any size that cannot be represented safely are rejected before output is
published.

The host and DOS `MDPC` builds share this codec. `MDPTEST.EXE` executes the same
golden, mutation, scale, and deterministic-order tests in vanilla DOSBox.
`MDPC pack` writes a same-directory `.$$$` temporary, preserves an existing
package as `.$BK`, commits the new file, then removes the backup. A write or
rename failure leaves the previous package intact or explicitly recoverable;
stale sidecars block a later write instead of being silently destroyed.

## Migration boundary

The current repository `GAME.MDP` uses the preserved prototype v0 `MOON`
layout and is not a positive v1 fixture. Migration reads v0 only as an import
source and writes v1 exclusively. The runtime will not expose v0 assets after
source projects and fixtures have been converted.
