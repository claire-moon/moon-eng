# ZEUS and MOON ENG

ZEUS is an original DOS action game built on MOON ENG. The repository also
contains the MDPed content editor, the Tmuse sound and music system, and the
CGUI user-interface library.

The project targets DJGPP and a 32-bit DOS runtime. A fresh installation of
vanilla DOSBox 0.74-3 is the primary public compatibility target; 86Box
Windows 98 SE and Windows NT 4 SP6a profiles provide the manual Pentium 90
acceptance gate.

## Status

The checked-in game is currently a pre-alpha engine prototype. It includes a
320x200 indexed-color raycaster, movement, a developer console, procedural
sky rendering, early CGUI tools, and an experimental Sound Blaster synthesizer.
The newer foundation code now includes MDP v1 codecs, HITL evidence parsing,
the shared fixed-step/DOS runtime boundary, and a portable caller-owned CGUI
menu/modal core; the legacy game and tool consumers have not all migrated yet.
The active roadmap is in [docs/ROADMAP.md](docs/ROADMAP.md).

Historical design notes are retained under `docs/archive/pre-bootstrap/` for
provenance. They are not the current implementation specification.

## Build

The default Unix-side cross compiler is `i586-pc-msdosdjgpp-gcc`.

```sh
make all
```

Build products are written below `build/`; building never stages or commits
files. Use `make clean` to remove generated output and `make dist` to assemble
the floppy-targeted game package under `dist/game/` and a complete developer
suite under `dist/tools/`. The developer suite includes ZEUS and `GAME.MDP` so
every option in `MOON.EXE` works from that one directory. Each staging target
resets its generated configuration directory before copying the manifest, so
removed or test-only artifacts cannot survive from an older build.

On a DOS/Windows DJGPP installation, run `BUILD.BAT`. On a modern system, the
generated distribution can be mounted and started with vanilla DOSBox.

Run `make dosbox-smoke` to cross-build the game-only distribution and exercise
`ZEUS.EXE /SMOKE` through vanilla DOSBox with an empty configuration. The
optional `dosbox-dev.conf` is for interactive development convenience and is
not used by the default-configuration compatibility gate.

`make test` runs the native MDP container, typed cell-map, deterministic
host-CLI, HITL evidence, portable runtime-core, and contextual CGUI tests and
cross-builds their DOS equivalents. `make test-full` additionally executes
ZEUS, every MDP/HITL/runtime/CGUI test, the DJGPP hardware adapter, the MOON
first consumer, and a full VGA CGUI presentation/readback smoke in fresh
default-config vanilla DOSBox 0.74-3. Test executables and generated evidence
are never placed in either distribution. The native package compiler is
written to `build/host/bin/release/mdpc`; `MDPC.EXE` is staged only in the tools
package.

## Components

- `ZEUS.EXE`: the game and current MOON ENG runtime prototype
- `MOON.EXE`: runtime-backed developer-suite launcher and future TEST COCKPIT
  host
- `MDPED.EXE`: MDP content editor
- `TMUSE.EXE` / `TMUSEGUI.EXE`: audio engine diagnostics and editor frontends
- `MDPC.EXE`: shared MDP v1 package compiler and validator CLI
- `GAME.MDP`: legacy v0 prototype data package retained for migration tests

The user/automation authority boundary and DOS-safe evidence grammar are
specified in [docs/HITL_FORMAT.md](docs/HITL_FORMAT.md).

The deterministic 35 Hz simulation, 60/35 Hz presentation scheduler, action
input, DJGPP `uclock()`/IRQ1/Mode 13h adapter, and first MOON consumer are
specified in [docs/RUNTIME_CORE.md](docs/RUNTIME_CORE.md). ZEUS has not yet
migrated to this shared runtime.

The portable indexed-surface, clipping, semantic-palette, sparse-menu, help,
and default-No confirmation contract is specified in
[docs/CGUI_CORE.md](docs/CGUI_CORE.md). Legacy MDPed/TmuseGUI migration and the
ZEUS front end remain downstream work.

The canonical package layout is specified in
[docs/MDP_V1.md](docs/MDP_V1.md). The implemented height-aware cell-map
payload is specified independently in
[docs/MDP_MAP_V1.md](docs/MDP_MAP_V1.md).

## License

Original code and project assets are licensed under GPL-3.0-or-later. See
[LICENSE](LICENSE) and [NOTICE.md](NOTICE.md). Third-party runtime files retain
their own copyright and redistribution terms.
