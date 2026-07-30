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
the DOS runtime directory.

On a DOS/Windows DJGPP installation, run `BUILD.BAT`. On a modern system, the
generated distribution can be mounted and started with vanilla DOSBox.

## Components

- `ZEUS.EXE`: the game and current MOON ENG runtime prototype
- `MOON.EXE`: developer-suite launcher, ultimately the TEST COCKPIT host
- `MDPED.EXE`: MDP content editor
- `TMUSE.EXE` / `TMUSEGUI.EXE`: audio engine diagnostics and editor frontends
- `GAME.MDP`: legacy v0 prototype data package retained for migration tests

## License

Original code and project assets are licensed under GPL-3.0-or-later. See
[LICENSE](LICENSE) and [NOTICE.md](NOTICE.md). Third-party runtime files retain
their own copyright and redistribution terms.
