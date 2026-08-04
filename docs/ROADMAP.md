# ZEUS and MOON Development Roadmap

This is the implementation order for the ZEUS game, MOON ENG, MDPed, Tmuse,
CGUI, and the MDP toolchain. GitHub milestones and issues are the operational
copy of this roadmap; this document defines their structure and completion
criteria.

## GitHub issue structure

Milestone issue IDs use `M<stage>-<track>-<number>`, for example
`M1-MOON-03`. Issue titles begin with that ID so DOS-side evidence, commit
messages, and GitHub discussion can refer to the same work without URLs.

Track codes and labels are:

| Code | GitHub label | Responsibility |
| --- | --- | --- |
| REPO | `track:repo` | history, licensing, builds, CI, packaging |
| MOON | `track:moon` | platform, timing, memory, input, rendering |
| ZEUS | `track:zeus` | game states, combat, campaign, content |
| MDP | `track:mdp` | package schema, compiler, loader, validator |
| EDIT | `track:mdped` | MDPed project and content editors |
| TMUS | `track:tmuse` | Sound Blaster output, mixer, music system |
| CGUI | `track:cgui` | shared DOS GUI library |
| HITL | `track:hitl` | TEST COCKPIT and acceptance evidence |

Every issue also has one `type:*` label (`feature`, `defect`, `test`, `docs`, or
`chore`), one `priority:p0` through `priority:p3`, and `gate:manual` when user
acceptance is required. A cross-track issue names one owning track and links
blocked companion issues rather than acquiring several owners.

Each issue records:

1. observable outcome and explicit exclusions;
2. affected public data/interface contract;
3. automated tests and expected artifacts;
4. applicable DOSBox and 86Box cases;
5. disk, memory, and frame-time budget impact;
6. documentation and migration work;
7. manual TEST COCKPIT case IDs, if gated.

An issue is complete only when its implementation, tests, documentation, and
evidence are present. A GitHub checkbox or agent report is not a substitute for
manual acceptance.

## Milestone 0: Preserve and bootstrap

Objective: establish a recoverable, comprehensible, buildable repository
without destroying the historical prototype or laundering incomplete WIP into
the new baseline.

Planned issue set:

- `M0-REPO-01` Create a checksummed source archive and `git bundle --all`.
- `M0-REPO-02` Preserve the local prototype head, dirty WIP, and old GitHub tip
  under immutable archival references.
- `M0-REPO-03` Import a curated buildable snapshot through
  `chore/repository-bootstrap`; do not force-push or merge unrelated histories.
- `M0-REPO-04` Replace build-time Git commits with side-effect-free debug,
  release, test, clean, and distribution targets.
- `M0-REPO-05` Add ignore rules and stop tracking executables, logs, autosaves,
  and other generated output.
- `M0-REPO-06` Archive, then remove inactive `moon/`, `legacy/`, obsolete sound
  experiments, `reaper`, and orphan prototypes after their contents are
  classified and verified recoverable.
- `M0-REPO-07` Adopt GPLv3-or-later for original code/assets/data, record all
  third-party provenance, and retain CWSDPMI only with checksum and notice.
- `M0-REPO-08` Publish the audited architecture, roadmap, testing policy, and
  HITL evidence format.
- `M0-MDP-01` Preserve the current map-format WIP unchanged, then re-port its
  useful intent on `feat/mdp-map-format-v1` after bootstrap.

Exit gate:

- archives restore and verify independently;
- `main` is clean, protected, and buildable from a fresh clone;
- ordinary builds do not stage or commit files;
- generated files live under ignored build/distribution directories;
- current documentation distinguishes implemented behavior from target design.

## Milestone 1: Platform and format foundation

Objective: replace prototype-global boundaries with shared, testable contracts.

Planned issue groups:

- `M1-MOON-*`: C99/DJGPP GCC 12.2 baseline, `MoonContext`, memory arenas,
  action input, deterministic 35 Hz simulation, interpolated 60 FPS
  presentation, telemetry, custom 60 Hz video, and Mode 13h fallback.
- `M1-MDP-*`: MDP v1 little-endian codecs, directory and CRC validation, raw,
  RLE and LZSS chunks, v0 importer, golden fixtures, and shared host/DOS
  compiler core.
- `M1-CGUI-*`: context-based embedding, keyboard focus/tab order, clipping,
  modal ownership, semantic palette roles, menus, and file/help widgets.
- `M1-TMUS-*`: correct DSP, DMA, IRQ, EOI and restoration handling; double
  buffering; 16-bit stereo reference path; 8-bit mono fallback; underrun
  telemetry.
- `M1-HITL-*`: `MOON.EXE /HITL HITL.IN`, TEST COCKPIT UI, evidence parser and
  writer, stale-hash checks, and host artifact collection.
- `M1-REPO-*`: deterministic builds, dependency generation, CI package
  validation, and vanilla DOSBox smoke automation.

`M1-CGUI-01` is the implemented first CGUI slice: caller-owned indexed
surfaces, deterministic clipping/raster primitives, sparse-ID keyboard menus,
F1 help, and exclusive default-No confirmation modals. Its host, DJGPP, and
vanilla DOSBox automation is present; the two-profile user acceptance gate is
still open. Tab order, file/grid/canvas widgets, legacy tool migration, and
TEST COCKPIT remain later issues rather than implicit scope in that slice.

Exit gate:

- ZEUS, MDPed, Tmuse, and MOON build reproducibly;
- `ZEUS.EXE` starts in a fresh default-config vanilla DOSBox 0.74-3 setup;
- primary and fallback video/audio initialization fail safely;
- MDP corruption is rejected without publishing partial assets;
- TEST COCKPIT keeps AUTO and MANUAL authority separate.

## Milestone 2: Hybrid foundation slice

Objective: prove that all new foundations work together before broadening game
content.

Planned issue groups:

- `M2-ZEUS-*`: state stack, five-item main menu, animated space background,
  one ground objective, targeting, one weapon, visible projectiles, and basic
  damage.
- `M2-MOON-*`: shared cell/polygon/depth rendering, interpolation snapshots,
  fixed pools, scene loading, and performance counters.
- `M2-MDP-*`: one v1 project containing palette, procedural texture, cell map,
  model, enemy, weapon, and music assets.
- `M2-EDIT-*`: project browser plus minimum palette, procedural texture, map,
  and OBJ import/metadata flows needed to rebuild that project.
- `M2-TMUS-*`: one adaptive cue with deterministic transitions.
- `M2-HITL-*`: end-to-end launch, input, rendering, audio, and evidence cases.

The playable result is a CGUI title screen leading to one height-aware ground
map with one low-poly projectile skirmisher. It includes one weapon, targeting,
basic fracture, a procedural palette/texture set generated at scene load, and a
Tmuse cue.

Exit gate: the slice is reproducible from source assets, produces matching
deterministic evidence in both presentation modes, and passes its required user
TEST COCKPIT cases.

## Milestone 3: Ground game and editor

Objective: complete the repeatable ground-combat loop and the tools required to
author it.

Planned issue groups:

- `M3-ZEUS-*`: melee rusher, projectile skirmisher, burst/spread suppressor,
  heavy area-denial enemy, flying harasser, three projectile weapons, separate
  ammunition, automatic reload, objectives/exits, secrets, pickups, and five
  upgrade tracks (`Speed`, `Reload`, `Damage`, `Health`, `Tech`).
- `M3-ZEUS-*`: part collision, role-sensitive headshots, live dismemberment,
  authored fracture seams, capped interiors, particles, sleeping rigid pieces,
  static gore batches, reawakening, and active-run map persistence.
- `M3-EDIT-*`: `PALedit`, layered `TEXedit`, cell `MAPedit`, OBJ
  import/preview, hit-zone/fracture editing, and shared `AUDIOedit`.
- `M3-MOON-*`: deterministic replay/input recording, state hashes, pool
  telemetry, and navigation/collision treatment for persistent debris.

Exit gate: multiple ground encounters can be authored without hand-editing an
MDP; active-run revisits preserve gore; stress scenes remain within measured
pool and Pentium 90 budgets.

## Milestone 4: Sector worlds, flight, and campaign

Objective: deliver the complete alternating ground/flight structure.

Planned issue groups:

- `M4-MOON-*`: sector/portal maps, arbitrary angled walls, floor/ceiling
  variation, doors, lifts, cell compatibility, and affine/dither material
  controls. Room-over-room remains excluded.
- `M4-EDIT-*`: sector editing and validation, cell-to-sector assistance,
  flight-rail/wave editing, campaign graph/deltas, and celestial/weather tools.
- `M4-ZEUS-*`: on-rails first-person cockpit flight, route map, arcade results,
  score/secret thresholds, shared health, 1 HP flight-abort behavior, ground
  death recovery, and failed-recovery run reset.
- `M4-MOON-*`: generic celestial renderer, continuous LOD, spherical cloud
  weather, wind bands, turbulence, vortices, and storms.
- `M4-TMUS-*`: tracker/generative authoring, beat/bar transitions, intensity
  cues, bus control, and flight/ground transition music.

Exit gate: an authored route alternates ground and flight, takes a measurable
branch, revisits map deltas correctly, and approaches Jupiter continuously from
space into high atmosphere.

## Milestone 5: Public floppy demo

Objective: produce a polished 20-30 minute successful run.

Required content:

- three ground missions and two flight encounters;
- one performance branch and one secret route;
- a convergent hybrid boss that begins in flight and transitions directly to a
  ground phase;
- complete `NEW GAME`, `OPTIONS`, `HIGH SCORES`, `HELP ME!`, and `QUIT GAME`
  flows, pause behavior, F1 help, rebinding, results, progression, and reset;
- original final or release-quality placeholder art/audio with no copied trade
  dress or assets.

Packaging issues produce a game-only FAT12 1.44 MB image and an equivalent DOS
directory ZIP. Game payload may not exceed 1,310,720 bytes. MDPed, Tmuse, and
the SDK ship in separate archives. Executable packing is considered only after
unpacked builds pass vanilla DOSBox and both 86Box profiles.

Exit gate: the actual image is generated and inspected in CI, installs/copies to
a writable DOS directory, completes the compatibility matrix, meets the strict
performance gate, and receives explicit user manual release acceptance.

## Milestone 6: Release hardening

Objective: freeze the public demo as a reproducible, recoverable release.

- Exercise corrupt/truncated packages, missing audio, low memory, unavailable
  custom video mode, absent SB16, read-only media, cold start, repeated runs,
  and settings/high-score persistence.
- Freeze MDP v1, control defaults, package budgets, compiler version, and the
  compatibility matrix.
- Verify the clean user path is mount/copy and run `ZEUS.EXE`; optional tuning
  files may improve convenience but cannot be required.
- Attach automation and user TEST COCKPIT evidence to the release candidate.

Exit gate: release artifacts reproduce from the tagged source, hashes match,
all automated checks pass, all required manual cases are user-marked `PASS`, and
the demo is published without development tools on its disk.

## Post-demo roadmap

- Expand into a 60-90 minute continuous no-save campaign distributed as a
  larger ZIP while retaining the floppy demo.
- Add native vertex/face modeling after import-and-edit workflows mature.
- Expand celestial types, routes, enemies, and bosses.
- Write final factions, names, and story after systems and content budgets
  stabilize.
- Reconsider SSGE speech only after complete-game CPU and size measurements.
- Do not restore the abandoned voxel-renderer direction.
