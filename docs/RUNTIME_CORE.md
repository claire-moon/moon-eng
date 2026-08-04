# MOON Runtime Core and DOS Adapter

Status: implemented portable C99 timing/action-input core, DJGPP hardware
adapter, and MOON first consumer. ZEUS game-loop migration, custom 60 Hz VGA,
and physical smooth-60 acceptance are not implemented by this component.

## Scope

`include/moon/runtime.h` and `src/runtime/runtime.c` provide a caller-owned,
allocation-free `MoonContext`. The core performs no hardware access, file I/O,
or presentation. A platform adapter supplies samples from a wrapping 32-bit
monotonic clock and serializes raw keyboard updates with fixed-boundary input
consumption.

The implemented scheduler provides:

- exact integer-rational 35 Hz fixed boundaries at any supported source-clock
  frequency;
- 60 Hz presentation requests with unsigned Q0.16 snapshot interpolation, or
  whole-program `MOON_PRESENT_LEGACY_35` requests with no interpolation;
- no more than four fixed boundaries from one advance, with excess whole
  boundaries dropped while the fractional phase is retained;
- a configurable maximum source-clock delta and modular unsigned wrap handling;
- explicit telemetry for elapsed clamps, catch-up, dropped fixed ticks,
  presentations, and missed presentation intervals; and
- paused operation that freezes simulation and interpolation phase while a
  separate fixed input/UI phase and the selected presentation cadence continue.

One call to `moon_runtime_advance()` returns a `MoonFramePlan`. The caller runs
`moon_input_tick()` once for every reported `fixed_ticks` boundary. While the
runtime is not paused, game simulation also advances at those boundaries;
`simulation_ticks` is zero while paused. A `present` value of one requests at
most one presentation for that advance. It does not claim that a display swap
actually occurred.

Pause changes use `moon_runtime_set_paused()`, which first advances to the
supplied clock sample under the old state and returns that transition work in a
normal `MoonFramePlan`. This prevents elapsed time from being retroactively
classified as paused or running. On resume, fixed/input scheduling returns to
the frozen simulation phase. The possible one-boundary transition realignment
is intentional: it preserves the exact fractional active-simulation time
rather than making simulation cadence depend on how long a menu was open.
The presentation phase never realigns: both 60 Hz and legacy 35 Hz presentation
requests continue on their wall/UI cadence across pause transitions. Thus a
legacy request immediately after resume need not coincide with the first
resumed simulation boundary.

`fixed_ticks`, `simulation_ticks`, and `paused_simulation_ticks` count delivered
work after the four-boundary cap. Consequently the telemetry invariant
`fixed_ticks == simulation_ticks + paused_simulation_ticks` holds. Boundaries
discarded by either running or paused catch-up are counted only by
`dropped_fixed_ticks`.

## Action input

The input core supports 32 logical actions with two bindings per action. It
normalizes PC set-1 scan-code bytes into a 256-key namespace, keeping E0 keys
distinct from their non-extended counterparts. An exact DFA consumes only the
complete E1 Pause sequence and immediately reprocesses a mismatching byte.
Controller error bytes `00`/`FF` and synthetic E0 Shift wrappers are ignored.
Pressed and released edges are latched until a fixed boundary, including a
complete tap between boundaries. Held and deterministic repeat state are then
derived at the fixed cadence.

After initialization, an adapter must serialize every input mutation: bind,
direct set, scan-code feed, clear, hard resync, and fixed tick. `volatile` is not
synchronization. The intended DOS adapter queues bytes in its keyboard ISR and
drains and decodes them in the foreground. On queue overflow or a controller
parity/timeout error, it must flush the queue and call `moon_input_resync()`
before decoding more bytes; this drops unreliable pending history and releases
only actions already published as held. The portable core itself does not
install a keyboard ISR, read a hardware timer, or select a video mode.

## DJGPP adapter and MOON consumer

`include/moon/dos_runtime.h` and `src/platform/dos_runtime.c` implement the DOS
hardware boundary. The adapter is caller-owned but process-singleton while
active because it owns one physical keyboard interrupt vector. It:

- samples DJGPP `uclock()` at `UCLOCKS_PER_SEC` without changing IRQ0;
- derives IRQ1 from DJGPP's active master-PIC base rather than hard-coding
  vector 9;
- keeps the interrupt handler limited to controller status/data reads, a locked
  128-byte scan-code ring, the port 61h acknowledgement pulse, and master-PIC
  EOI;
- balances every adapter-owned DPMI page lock during partial initialization,
  normal shutdown, and repeated child-launch sessions;
- copies queued bytes under a saved virtual-interrupt state and performs all
  decoding in the foreground;
- hard-resynchronizes input after queue overflow or controller parity/timeout;
- saves the active BIOS video mode, verifies stock 320x200x8 Mode 13h, presents
  exactly 64,000 caller-owned bytes, and restores the saved mode; and
- unwinds acquired resources in reverse order without calling `exit()`.

`MOON.EXE` owns the `MoonContext`, DOS adapter, and framebuffer. Its launcher
uses action bindings and fixed input boundaries, renders only on presentation
requests, and consumes `alpha_q16` for its visual animation. It completely
restores DOS video and BIOS keyboard ownership before spawning another program,
then constructs a fresh session if that program returns. `/LEGACY35` selects
whole-program legacy presentation. `/RUNTIME-SMOKE` is a bounded noninteractive
adapter/core/presentation path used only for automated evidence.

## Validation

The same deterministic suite is built for the native host and DJGPP:

```sh
make CONFIG=release runtime-core-test-host
make CONFIG=release runtime-core-test-dos
make CONFIG=release runtime-core-test-dosbox
make CONFIG=release runtime-dos-test-dos
make CONFIG=release runtime-dos-test-dosbox
```

Use `CONFIG=debug` for the debug variant. The DOSBox target copies the prebuilt
`RTCORE.EXE` and `CWSDPMI.EXE` into a temporary directory, mounts it with an
empty configuration under vanilla DOSBox 0.74-3, and accepts only a passing
`RUNTIME.OUT`. The DOS adapter target also runs `RTDOS.EXE` and
`MOON.EXE /RUNTIME-SMOKE`, requires evidence from both, and proves the DOS shell
regained control after ordered teardown. Its adapter test performs two complete
init/shutdown cycles, but injects deterministic scan bytes into the IRQ ring;
physical keyboard IRQ1 delivery remains a manual acceptance case. `RTCORE.EXE`
and `RTDOS.EXE` are test artifacts and are absent from both game and tools
distributions.

The tests cover rational 35/60 cadence over a simulated hour, legacy cadence,
clock wrap, elapsed and catch-up clamps, fractional retention, interpolation
bounds, sampled pause transitions, scan-code namespaces and recovery, edge
latching, repeat timing, rebinding, and input processing across catch-up
boundaries.

These are deterministic API and compatibility gates. They do not measure VGA
refresh, frame-time variance, emulator pacing, renderer cost, or Pentium 90
performance, and therefore do not by themselves establish the project goal of
stutter-free physical 60 Hz presentation.

## Deferred integration and acceptance

ZEUS must still migrate from its prototype globals to `MoonContext`,
`MoonFramePlan`, action input, and interpolated snapshots. The primary video
backend must add a tested custom 320x200x8 60 Hz mode while retaining this stock
Mode 13h fallback. The automated adapter smoke cannot validate physical keyboard
IRQ behavior, display pacing, frame-time variance, or restoration across every
target DOS host; those remain explicit user-controlled gates on the supported
86Box profiles.
