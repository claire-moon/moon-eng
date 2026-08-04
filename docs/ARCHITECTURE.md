# ZEUS and MOON Architecture

Status: target architecture for active development. This document describes the
interfaces the project is moving toward; it does not claim that every subsystem
is implemented today.

## Audited baseline

The preserved prototype is a useful renderer and tooling experiment, but it is
not yet an integrated game-production stack.

- ZEUS currently builds around a monolithic 320x200, 256-colour Mode 13h game
  loop running at 35 Hz.
- The world renderer is a Wolfenstein-style DDA raycaster with variable wall
  height and colour, flat floor and ceiling treatment, palette effects,
  procedural star skies, movement, a console, and debug HUD features.
- There is no production entity/combat system, polygon renderer, depth buffer,
  sector world, campaign state machine, or shared runtime asset manager.
- The current MDP package is an experimental native-structure format. The
  in-progress map-format migration is incomplete and is to be preserved as
  historical WIP, not repaired in place.
- MDPed is presently a CGUI demonstration shell rather than a package editor.
- Tmuse presently demonstrates oscillator/ADSR synthesis, but its DMA, IRQ,
  buffering, sequencing, and integration layers require replacement.
- CGUI has a useful collection of drawing and widget experiments, but currently
  assumes global graphics ownership and lacks reliable focus, modal, clipping,
  and embedding contracts.

Live code, reproducible builds, and test evidence take precedence over claims
in old design notes, autosaves, or generated logs.

## System boundaries

```text
                  +-------------------------+
                  |       ZEUS game         |
                  | states, rules, campaign |
                  +------------+------------+
                               |
              +----------------+----------------+
              |                                 |
       +------v------+                   +------v------+
       |    CGUI     |                   |    Tmuse    |
       | game/tools  |                   | music/audio |
       +------+------+                   +------+------+
              |                                 |
       +------v---------------------------------v------+
       |                  MOON ENG                     |
       | platform, timing, input, memory, render, I/O |
       +------------------------+----------------------+
                                |
                         +------v------+
                         | MDP runtime |
                         | asset access|
                         +-------------+

       MDPed + host CLI --compile/validate--> MDP runtime package
       MOON TEST COCKPIT --records-----------> HITL evidence
```

All shipped DOS programs use the same platform, package, rendering, input, GUI,
and audio libraries. No executable maintains a private fork of those systems.

## MOON ENG

MOON ENG is the reusable DOS runtime and owns hardware-facing services.

### Runtime context

`MoonContext` is created by the executable and passed explicitly to subsystems.
It owns:

- selected video backend, colour buffer, 16-bit depth buffer, and active
  palette;
- fixed-step clock, presentation clock, and interpolation state;
- raw keyboard/mouse devices and the action-mapping layer;
- Tmuse device and mixer state;
- permanent, scene, frame, and scratch memory arenas;
- open MDP archives and resolved runtime assets;
- performance, missed-present, catch-up, and audio-underrun telemetry.

Subsystems must not declare competing framebuffer, VGA, timer, palette, input,
or audio globals. Initialization and shutdown are ordered through
`MoonContext`, and partial initialization must unwind safely.

### Timing contract

Simulation is deterministic and fixed at 35 ticks per second. Normal
presentation is capped at 60 frames per second and interpolates between the
previous and current simulation snapshots. Snapshots cover the player, actors,
projectiles, doors, rigid pieces, weather, and every other moving object.

`Legacy 35 FPS` is a whole-program option. It disables interpolation and
presents menus, ground missions, flight, recovery encounters, and results at
simulation cadence.

The accumulator may process no more than four catch-up ticks for one presented
frame. Additional elapsed time is clamped and recorded as a performance fault;
it may never produce an unbounded spiral. Gameplay code uses simulation ticks,
not presentation frames or host wall-clock time.

### Input contract

Hardware input is translated into `MoonActionState`. An action exposes pressed,
released, held, and repeat state without leaking scan-code checks into game or
GUI logic.

- ZEUS, its menus, and TEST COCKPIT are entirely keyboard operable and never
  require a mouse.
- MDPed and Tmuse are keyboard-first; a mouse is permitted on editing canvases.
- Every ZEUS gameplay action is rebindable.

## Rendering and world representation

The engine uses one caller-owned colour/depth rendering context for world,
polygon, sprite, particle, and GUI passes.

### Video backends

The primary backend is a tested 320x200x8 custom 60 Hz VGA/Mode-X-compatible
presentation path. A stock 320x200x8 Mode 13h backend is mandatory and selected
automatically when the custom mode cannot be established. Projection, artwork,
screenshots, and editor previews assume 4:3 display correction.

### World backends

Development proceeds through compatible stages:

1. Stabilize the height-aware cell renderer, collision, and MDP loading.
2. Add a sector/portal backend with arbitrary angled walls, varying floor and
   ceiling heights, doors, and lifts.

Room-over-room geometry is outside the target. Cell maps remain loadable after
the sector backend lands so the first vertical slice and test fixtures continue
to work.

### Polygon pipeline

The polygon renderer uses fixed-point transforms, frustum clipping, back-face
culling, a 16-bit depth buffer, and deterministic rasterization.

- Enemies and ships use untextured rigid parts with flat face shading.
- Gouraud shading is opt-in for tagged celestial bodies, cockpit pieces, and
  exceptional models.
- Textured world and cockpit surfaces use intentionally affine mapping.
- Saturn-style dithering is a named style preset with per-material overrides.
- First-person weapon frames are the only planned hand-authored sprite
  exception to procedural surface textures.

Map geometry, models, projectiles, particles, and weapon sprites share the same
depth convention. Fixed pools, distance LOD, culling, and map/editor budgets are
set from measured Pentium 90 results rather than arbitrary desktop limits.

### Procedural textures and celestial scenes

Runtime surface textures are deterministic recipes plus seeds stored in MDP.
Recipes support value and cellular noise, fBm/turbulence, primitives, warp,
masks, threshold/blend operations, and palette mapping. Valid texture sizes are
16, 32, 64, and 128 pixels per side; 64 is the default.

Textures are generated and cached during scene load only. Active gameplay may
not synthesize textures, allocate storage for them, or perform package I/O.
MDPed presents recipes as a layer/modifier stack, not a node graph.

Celestial definitions are reusable assets. Authored campaign nodes control
apparent size, atmosphere, and transitions. The Jupiter sequence uses
procedural gas bands plus spherical cloud/weather particles with wind bands,
turbulence, vortices, and storms, progressing continuously from distant sky to
high-atmosphere flight.

## MDP packages and source projects

An `.MDP` is a deterministic compiled runtime artifact. It is never the
canonical editable project.

The implemented byte-level v1 rules are frozen in
[`MDP_V1.md`](MDP_V1.md); this section defines the surrounding compiler and
runtime architecture.

The first implemented typed payload is the allocation-free `MAP ` cell-map
schema frozen in [`MDP_MAP_V1.md`](MDP_MAP_V1.md). It preserves the useful
height, material, light, tag, and behavior-flag intent of the archived WIP
without copying its invalid native structures.

- A project directory contains an 8.3-safe `.MPR` manifest and separate source
  assets.
- The shared compiler/validator core is portable C99 and is used by DOS MDPed
  and the modern host CLI.
- MDP v1 uses `MDP1` magic, explicit little-endian fields, schema versions,
  typed directory records, stable 32-bit asset IDs, offsets and sizes, flags,
  and CRC-32.
- Payloads are four-byte aligned and independently stored raw, RLE-compressed,
  or with the project's small LZSS codec.
- Native structs, pointers, compiler padding, and host-sized `int` fields are
  never serialized.
- The loader validates the complete directory, bounds, sizes, schemas,
  references, decompression limits, and CRCs before publishing any asset.

The v0 package receives import/read support only during migration. Tooling writes
v1 exclusively; runtime v0 loading is removed once fixtures and source projects
have been converted.

OBJ is an editor/compiler input subset, never a runtime format. It is compiled
to quantized fixed-point vertices, indexed faces, rigid parts, hit zones,
fracture seams, and capped interior surfaces. Sidecar metadata supplies the
game-specific information OBJ cannot represent.

## CGUI

`CguiContext` receives a caller-provided surface, event stream, font, and
semantic palette roles. Embedded CGUI never changes video mode and never owns
MOON ENG global state.

The shared library is responsible for:

- deterministic keyboard focus and tab order;
- menu traversal, accelerators, and disabled-item handling;
- clipped window/widget painting;
- correct modal ownership and dismissal;
- sparse stable widget IDs;
- file selection, list/grid editing, help pages, and canvas events;
- semantic palette remapping between editor, game, and scene palettes.

CGUI powers the ZEUS front end and pause UI, MDPed, standalone Tmuse, and the
MOON developer hub/TEST COCKPIT.

## Tmuse

`TmuseContext` owns the Sound Blaster device, mixer, instruments, patterns,
current cue, intensity, deterministic music seed, buses, and underrun telemetry.

The reference path is Sound Blaster 16 at approximately 22.05 kHz, 16-bit
stereo, using BLASTER profile `A220 I7 D1 H5`. An 8-bit mono DMA-1 fallback is
mandatory. The initial design exposes 16 logical voices with priority stealing;
fallback mode exposes eight. Double buffering uses 512-frame halves.

Music combines tracker-authored patterns and instruments with deterministic
rules for motifs, transitions, orchestration, and intensity. Changes are
sample-clocked and committed at beat/bar boundaries. Synthesis is the release
baseline; PCM additions require explicit disk and runtime measurement.

The same editing core powers MDPed `AUDIOedit` and standalone Tmuse. The old
text display may remain a diagnostic tool but is not placed on the game disk.
SSGE speech is post-episode scope.

## HITL evidence

The implemented portable HITL core parses bounded DOS-safe `HITL.IN` plans and
`AUTO.OUT` results, rejects stale/missing/duplicate/malformed evidence, and
evaluates AUTO and MANUAL lanes independently. Zero-initialized status is
`UNRUN`, never `PASS`, and the portable API deliberately exposes no operation
that assigns a manual result.

The byte grammar and authority rules are frozen in
[`HITL_FORMAT.md`](HITL_FORMAT.md). The CGUI TEST COCKPIT, physical-input gate,
append-only journal, transactional `HITL.OUT` writer, captures, and host
collector are target work and must not be inferred from parser availability.

## ZEUS

ZEUS owns game rules and content orchestration, not platform hardware.

Its state stack contains `BOOT`, `TITLE`, `OPTIONS`, `HIGH_SCORES`, `HELP`,
`GROUND`, `PAUSE`, `GROUND_RESULTS`, `ROUTE`, `FLIGHT`, `FLIGHT_RESULTS`,
`RECOVERY`, `BOSS_TRANSITION`, `RUN_RESULTS`, and `QUIT`. There are no save/load,
lives, inventory-screen, or difficulty states.

The exact title choices are `NEW GAME`, `OPTIONS`, `HIGH SCORES`, `HELP ME!`,
and `QUIT GAME`. A deterministic low-poly space scene runs behind title menus.
Pause freezes and dims gameplay. `HELP ME!` and F1 open the same multi-page
controls/HUD/combat/ground/flight/progression guide.

Ground and flight encounters use visible simulated projectiles. Ground systems
own objectives, exits, secrets, pickups, five upgrade tracks, per-part damage,
headshots, dismemberment, rigid fragments, particles, and active-run permagore.
Settled gore is converted into static per-map batches and can be reawakened
locally; it is not written as a campaign save.

Campaign flow alternates ground missions, results, an animated route map, and
on-rails cockpit flight. Branches are earned by thresholds and secrets. Ship and
player health are shared; flight failure advances to the next ground mission at
exactly 1 HP. Ground death enters the target-hunt recovery encounter, and only a
failed recovery resets the run.

## Runtime invariants

- No allocation, file I/O, decompression, or procedural texture generation in
  active gameplay.
- No subsystem reads a partially validated MDP asset.
- No presentation-rate choice changes simulation results.
- No editor writes an MDP directly without the shared compiler/validator.
- No automated agent or test process assigns a manual HITL `PASS`.
- No release-sensitive change merges without required automated evidence and
  explicit user manual acceptance.
