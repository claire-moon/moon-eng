# ZEUS and MOON Testing Policy

Testing combines deterministic host checks, vanilla DOSBox compatibility,
target-class 86Box acceptance, and user-controlled human-in-the-loop evidence.
Passing one layer never implies that another layer passed.

## Result authority

Every gated case has independent `AUTO` and `MANUAL` results. Valid values are
`PASS`, `FAIL`, `BLOCKED`, and `UNRUN`.

- Automation owns only `AUTO` results.
- Only the user operating TEST COCKPIT may assign MANUAL `PASS`.
- Codex, CI, scripts, and the game itself must never infer, copy, or self-mark a
  MANUAL `PASS` from automated success.
- Any changed build, test plan, or machine profile invalidates mismatched
  evidence. Stale evidence is `BLOCKED`, not grandfathered.
- A required gate passes only when its required AUTO result is `PASS` and its
  required MANUAL result is explicitly user-marked `PASS`.

Manual gates are required for changes to runtime behavior, rendering, input,
audio, CGUI, MDP runtime format/loading, packaging, milestones, and releases.
Documentation-only and host-only tooling changes may be automation-only when
they cannot affect a shipped DOS artifact.

The evidence exchange and file grammar are defined in
[`HITL_FORMAT.md`](HITL_FORMAT.md).

The canonical `tests/hitl/fixtures/HITL.IN` and `AUTO.OUT` files are stored
byte-for-byte with CRLF records. Repository attributes disable text
normalization for these fixtures so host, DJGPP, and DOSBox tests consume the
same canonical input bytes.

## Compatibility matrix

### Automated baseline

- Cross-compile with the pinned DJGPP GCC 12.2/C99 toolchain.
- Start the unpacked release candidate in vanilla DOSBox 0.74-3 using a fresh
  default configuration.
- Mount or copy the distribution directory and run `ZEUS.EXE` without
  DOSBox-X, patches, wrapper programs, or a mandatory project configuration.
- Test the custom 320x200x8 60 Hz path and force the stock Mode 13h fallback.
- Test Sound Blaster 16 16-bit stereo and force the 8-bit mono fallback.

Optional DOSBox configuration and launch scripts may be distributed as
conveniences, but automated success with them does not replace the default
configuration case.

### User acceptance profiles

These runs are performed by the user in named 86Box configurations:

| Profile ID | Operating system | Machine target | Memory | Audio reference |
| --- | --- | --- | --- | --- |
| `W98P90` | Windows 98 SE DOS session | Pentium 90 | 16 MB | SB16 `A220 I7 D1 H5` |
| `NT4P90` | Windows NT 4 SP6a DOS session | Pentium 90 | 16 MB | SB16 `A220 I7 D1 H5` |

Each profile records emulator version, machine configuration hash, BLASTER
value, video path, audio path, executable/package hashes, and whether it is a
cold or warmed run. A profile change requires new evidence.

### MOON runtime foundation acceptance

The M1 runtime adapter has four manual cases on both profiles. Automation may
prepare the build and evidence plan, but only the user assigns their MANUAL
result:

- `MOON.RUNTIME`: the launcher animation remains responsive, reports no
  catch-up/clamp fault, and exits cleanly through stock Mode 13h;
- `MOON.INPUT`: using the physical keyboard, E0 arrow keys wrap focus, holding
  an arrow visibly increments repeat events, release clears `H` and increments
  `R`, Enter activates focus, number-row shortcuts work, Escape exits, and the
  non-E0 keypad keys do not alias arrow navigation or leave stuck input;
- `MOON.RESTORE`: repeated launch/return cycles restore the text/video mode and
  a working BIOS keyboard before each child and at the final DOS prompt; and
- `MOON.LEGACY35`: `/LEGACY35` remains functional with interpolation disabled.

These cases validate the fallback and ownership boundary. They do not satisfy
the later custom-video smooth-60 performance gate.

### CGUI context foundation acceptance

Build `CGUIPRES.EXE`, copy it with `CWSDPMI.EXE`, and run
`CGUIPRES.EXE /INTERACTIVE` on both named profiles. The M1 CGUI foundation has
four manual cases; only the user assigns their MANUAL result:

- `CGUI.VISUAL`: the complete beveled menu, every text row, star field, focus
  highlight, disabled row, help panel, and confirmation panel render without
  missing right/bottom edges, corrupt glyphs, padding artifacts, or flicker;
- `CGUI.INPUT`: physical E0 Up/Down wrap and skip the disabled item, key repeat
  moves one row per repeat pulse, Enter and Space each activate once, number
  accelerators target the matching sparse-ID item, and F1 opens/closes help;
- `CGUI.MODAL`: confirmation initially focuses No, consumes background input,
  requires one tick to move to Yes and a later tick to accept it, distinguishes
  the `N` accelerator/explicit No from Escape cancellation, accepts `Y` only
  as the explicit Yes accelerator, and does not let a close-producing key
  activate the backing menu; and
- `CGUI.RESTORE`: repeated help/confirm/exit cycles return to the original
  video mode and a responsive DOS keyboard with no stuck key.

The `/SMOKE` path and DOSBox readback prove deterministic raster output and
ordered restoration, but synthetic input and a dummy video driver do not
assign or substitute for these physical cases.

### TEST COCKPIT acceptance

Prepare matching `HITL.IN` and `AUTO.OUT` files for each named profile, copy
them beside the tools build, and run `MOON.EXE /HITL HITL.IN`. The cockpit gate
has four manual cases on both W98P90 and NT4P90; only the user assigns them:

- `HITL.VISUAL`: the identity header, LIVE DOS banner, ordered 12-row pages,
  AUTO/MANUAL/combined columns, full detail pane, stale warning, help, and both
  default-No confirmations remain legible and deterministic;
- `HITL.AUTH`: one fresh physical action opens a PASS/FAIL/BLOCKED confirmation
  but changes no result, No/Escape/repeat/held input changes no result, and only
  a later distinct physical Yes/Enter/Space action completes the decision;
- `HITL.JOURNAL`: a deliberately completed decision appears once in
  `HITL.JRN`, the committed `HITL.OUT` has the matching lane/result/count/CRC,
  a stale identity cannot be marked PASS, and reopening does not silently
  restore a prior process's MANUAL authority; and
- `HITL.RESTORE`: cancel/commit/error paths return to the original video mode
  and responsive DOS keyboard, leave no `HITL.NEW`, and retain `HITL.OLD` only
  when it is needed to recover a failed replacement.

`MOON.EXE /HITL HITL.IN /SMOKE` is automation-only. It intentionally navigates,
renders, and attempts a synthetic decision, then proves `MANUAL=0`, verifies
the framebuffer/readback hash, commits only UNRUN manual fields, restores the
DOS boundary, and returns to the shell. It can never assign one of the cases
above.

##### Reconstructed controller coverage

`tests/hitl/test_hitl_cockpit.c` covers synthetic authority denial,
two-distinct-edge MANUAL publication, pending-token consumption after a failed
confirmation, stale-AUTO PASS denial, diagnostic FAIL/BLOCKED eligibility,
journal CRC verification, and the rule that imported journal history never
restores current-process MANUAL authority.

The controller test passes in debug and release host configurations and
compiles and links for debug and release DJGPP configurations. The separate
CGUI cockpit test remains to be reconstructed before the combined TEST COCKPIT
targets are considered complete.

## Performance acceptance

The strict gameplay gate is measured on the Pentium 90 profiles after scene and
audio warm-up. Capture exactly 60 seconds of representative active gameplay,
including actors, projectiles, particles, procedural sky/weather already loaded,
and music.

All of the following are required:

- presented frame rate remains between 59 and 61 FPS;
- 95th-percentile frame time is at most 18 ms;
- zero frames take 33 ms or longer;
- zero missed presents;
- zero audio underruns.

The telemetry capture also reports 35 Hz simulation ticks, catch-up ticks,
clamped accumulator events, active/high-water pool counts, memory-arena
high-water marks, render pass times, and the selected video/audio fallbacks.
Any clamped accumulator event fails the capture even when the displayed average
frame rate looks acceptable.

`Legacy 35 FPS` has a separate correctness case: gameplay state hashes must
match the 60 FPS presentation run for the same input recording. It is not
required to satisfy a 60 FPS presentation measurement.

Entity, particle, fragment, gore, weather, and audio capacities are calibrated
upward only while this gate continues to pass. The highest passing capacities
become release constants and MDPed validation limits.

## Automated suites

### Build and repository

- fresh-clone debug and release builds;
- dependency rebuild after a public header change;
- clean target removes all declared outputs;
- build/test targets leave `git status --short` unchanged;
- reproducible source/package outputs for identical inputs;
- compiler and third-party provenance recorded in `BUILD.TXT`.

### MDP

- golden encode/decode round trips for every typed chunk and compression mode;
- deterministic package byte/hash equality from identical `.MPR` projects;
- v0 import fixtures followed by v1-only output;
- unknown optional versus required schema behavior;
- duplicate IDs, invalid references, overflow, overlap, truncation, bad
  alignment, decompression overrun, CRC mismatch, and malformed directory cases;
- fuzzed package input with bounded memory/time and no partially exposed assets.

### Timing and game state

- the portable runtime's host/DJGPP golden suite and fresh-config DOSBox gate;
- the DOS adapter enters/restores Mode 13h and IRQ1 ownership, copies its full
  framebuffer, rejects a second active owner, and returns control to DOS;
- `MOON.EXE /RUNTIME-SMOKE` consumes nonzero fixed/presentation work and writes
  automation-owned evidence after teardown;
- recorded action input produces identical per-tick state hashes under 35 and
  60 FPS presentation;
- interpolation never changes authoritative positions or collision;
- long-frame catch-up is capped at four ticks and records a fault;
- pause freezes simulation while the pause UI remains responsive;
- run reset preserves only settings, high scores, and test evidence.

### Rendering

- colour/depth buffer hashes for cell maps, sector fixtures, near-plane
  clipping, back-face culling, depth ordering, affine textures, dithering,
  sprites, and particles;
- primary/fallback video selection and palette restoration after exit;
- procedural texture recipe/seed determinism for 16, 32, 64, and 128 sizes;
- generation occurs during scene load and never during active-frame telemetry;
- celestial LOD transitions do not pop, overflow pools, or change simulation
  state with presentation rate.

### Input and CGUI

- keyboard-only traversal reaches and operates every ZEUS and TEST COCKPIT
  control;
- focus order, accelerators, disabled items, nested menus, modal ownership,
  clipping, and sparse widget IDs;
- full rebinding, duplicate-binding resolution, and restart persistence;
- F1 and `HELP ME!` open the same guide; pause freezes/dims its backing game.

### Audio

- offline mixer waveform and clipping checks;
- DSP timeout, DMA boundary, IRQ/EOI, teardown, and device-loss tests;
- primary 16-bit stereo and required 8-bit mono fallback startup;
- double-buffer stress with zero underruns;
- voice-priority stealing and deterministic cue output;
- motif/intensity transitions occur on the requested beat/bar boundary.

### Gameplay and campaign

- every player and enemy attack produces a simulated visible projectile;
- early auto-target shots originate from the easing weapon position and can
  miss;
- role-sensitive headshots, limb impairment, fracture seams, capped interiors,
  bounce/sleep/reawakening, and fixed-pool exhaustion behavior;
- map revisit preserves active-run corpses, pieces, and stains through static
  batching;
- objective-before-exit gating, secrets, direct pickups, XP, and upgrade choice;
- score/secret route branches and base-map delta application;
- flight zero health advances to the next ground map at exactly 1 HP;
- ground death enters recovery, successful recovery respawns near the death
  location, and failed recovery resets the run;
- hybrid boss transfers from flight directly into its ground phase.

## Packaging and release cases

- Generate the actual FAT12 1.44 MB image in CI and inspect its filesystem.
- Enforce a game payload maximum of 1,310,720 bytes before image creation.
- Compare the image contents against the DOS-directory ZIP manifest and hashes.
- Confirm the game disk excludes MDPed, Tmuse development UI, SDK material,
  source projects, logs, autosaves, and build tools.
- Install/copy to a writable DOS directory, then test settings, high scores, and
  HITL evidence output there.
- Test missing/invalid MDP, low memory, no supported custom video mode, no SB16,
  read-only source media, cold start, repeated runs, clean exit, and recovery
  after interrupted evidence writing.
- Test packed executables only after the corresponding unpacked build passes the
  entire vanilla DOSBox and 86Box matrix.

## PR and release enforcement

Protected `main` is PR-only. CI attaches build products, package reports,
telemetry, and AUTO evidence to the PR rather than committing generated evidence
to source control.

A runtime-sensitive PR may be technically green while waiting for the user. It
remains unmergeable with MANUAL `UNRUN`, `BLOCKED`, or `FAIL`. A new commit that
changes a tested artifact invalidates earlier manual evidence. Release tags are
created only from a commit whose required build, plan, profile, automated, and
manual evidence hashes agree.
