# CGUI Context Core

Status: implemented portable foundation; physical visual/input acceptance is
still user-controlled. The legacy hardware-owning sources under `cgui/` remain
unchanged for provenance and for the separately scoped MDPed/TmuseGUI
migration.

## Boundary

The public contract is `include/moon/cgui.h`. Its implementation is split
between the portable render/font and state-reducer sources under `src/cgui/`.
The library:

- writes only to a caller-provided 8-bit indexed surface;
- consumes only caller-normalized action frames and stable accelerator IDs;
- stores all mutable state in a caller-owned `CguiContext`;
- returns events instead of calling application callbacks; and
- has no DOS headers, VGA access, device input, clocks, allocation, floating
  point, locale state, or mutable file-scope state.

The caller owns the framebuffer, presentation backend, input bindings, fixed
UI tick, application state, and the lifetime of borrowed label/help strings
and any custom font row/map buffers. Immutable built-in font data is the only
file-scope data in the core.

## Surface and raster contract

`CguiSurface` describes one byte per pixel with an explicit stride and byte
count. Initialization rejects null storage, zero dimensions, a stride smaller
than the width, dimensions outside signed coordinate range, or any final pixel
offset beyond the supplied storage. Stride, byte count, and the final addressed
extent are capped at `UINT32_MAX` so 64-bit hosts and 32-bit DJGPP accept the
same surface descriptions. Surface changes are transactional and reset the
clip stack to the new surface.

Every rectangle is half-open: `[x0,x1) x [y0,y1)`. Equal endpoints are valid
empty rectangles. Reversed endpoints return `CGUI_ERR_RANGE`. Clip pushes
intersect the current clip and therefore never widen it; empty intersections
are valid. Overflow, underflow, invalid roles, and invalid coordinates leave
state and pixels unchanged.

Fill, border, character, and text rendering clip before pointer arithmetic and
never touch row padding. Border overlap order is top, left, bottom, then right,
so bottom/right shadow pixels deliberately win on degenerate one-pixel
borders. `cgui_surface_hash()` is 32-bit FNV-1a over visible row pixels and
excludes padding.

The built-in font is a transparent 3x5 ASCII font with a 4-pixel horizontal
advance and 6-pixel line advance. Its 128-entry byte map is explicit rather
than inheriting the legacy symbol-index bug. LF begins a new line, CR is
ignored, and unsupported bytes render the replacement glyph.

Every text span is explicit and bounded to `CGUI_TEXT_BYTE_CAPACITY` (4096
bytes). Oversized or nonempty null spans are rejected before drawing or model
publication, keeping corrupt caller metadata from turning clipped text into an
unbounded hot path.

## Semantic palette

Drawing takes `CguiPaletteRole` values, never hard-coded application colors.
The palette is copied into the context. The built-in classic mapping uses
stock Mode 13h/EGA-compatible indices, but ZEUS and tools may install different
role mappings without changing layout or model state.

## Menu model and input reducer

A menu contains at most 32 copied descriptors. Label bytes remain borrowed.
Each item ID and menu owner ID is an arbitrary nonzero `uint32_t`; visual array
order controls drawing and traversal, never numeric ID order. Model replacement
is transactional and rejects zero/duplicate IDs, invalid spans, invalid enabled
flags, and over-capacity input.

Focus always names an enabled item or is zero. Reordering preserves a still
enabled stable ID. Replacement repairs invalid focus to the first enabled item;
disabling the focused item moves to the next enabled item with wrap. Up and
Down wrap and skip disabled items.

Call `cgui_update()` exactly once per logical 35 Hz UI/input tick. Up and Down
respond to `pressed || repeat`; Enter, Space, Escape, F1, and accelerators
respond only to pressed edges. The caller resolves a rebindable accelerator to
one stable target ID before the call. Menu priority is:

1. Escape;
2. F1 help request;
3. accelerator;
4. navigation; and
5. Enter/Space activation.

Every selected branch consumes that frame, even when it is a neutral Up+Down
conflict or a stale/disabled accelerator. One call clears the output first and
emits at most one event.

## Modal ownership

Only one help or confirmation modal may be open. While open, it consumes the
entire input frame; backing menu focus and events cannot leak through.

Help closes on Escape, F1, Enter, or Space and returns one `HELP_CLOSE` event.
Confirmation requires distinct nonzero owner, Yes, and No IDs and always opens
focused on No. Its priority is Escape, accelerator, F1 consumption,
navigation, then activation. Escape returns Cancel, not No. Navigation and
activation cannot occur in one update, so reaching Yes and confirming it
requires distinct logical input ticks unless the caller supplied the explicit
Yes accelerator.

## Validation and DOS presentation

The same deterministic core tests build for the host and DJGPP:

```sh
make CONFIG=release cgui-test-host
make CONFIG=release cgui-test-dos
make CONFIG=release cgui-test-dosbox
```

Use `CONFIG=debug` for the debug variant. The vanilla DOSBox 0.74-3 gate also
runs `CGUIPRES.EXE /SMOKE`. That DOS-owned harness renders a fixed menu and
confirmation scene, presents it through `MoonDosRuntime`, reads all 64,000 VGA
bytes back against the frozen `EC5F78FB` hash, shuts down, and proves the DOS
shell regained control. `CGUITEST.EXE` and `CGUIPRES.EXE` are validation
artifacts and are not staged in the game or tools distributions.

For the user-controlled visual and physical-input case, run this from the
matching DJGPP build directory inside a supported 86Box profile:

```text
CGUIPRES.EXE /INTERACTIVE
```

The interactive harness exposes arrow traversal, Enter/Space activation, F1
help, Escape dismissal, direct accelerators, disabled-item skipping, and the
default-No confirmation path. It must return to the original video mode and
DOS shell. Automation does not assign its MANUAL result.

## Deferred work

TEST COCKPIT is the first integrated CGUI consumer and keeps physical evidence
authority in its own controller. This foundation still does not migrate legacy
MDPed/TmuseGUI widgets, implement file/grid/canvas controls, add mouse routing,
replace the ordinary MOON launcher, or build the ZEUS front end and HELP ME!
artwork. Those are downstream issues using this contract, not hidden claims of
the original CGUI slice.
