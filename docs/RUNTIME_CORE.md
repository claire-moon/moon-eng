# MOON Portable Runtime Core

Status: implemented portable C99 timing and action-input core. The DOS hardware
adapter, ZEUS game-loop migration, and physical smooth-60 acceptance are not
implemented by this component.

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
only actions already published as held. The portable core does not install a
keyboard ISR, read a hardware timer, or select a video mode.

## Validation

The same deterministic suite is built for the native host and DJGPP:

```sh
make CONFIG=release runtime-core-test-host
make CONFIG=release runtime-core-test-dos
make CONFIG=release runtime-core-test-dosbox
```

Use `CONFIG=debug` for the debug variant. The DOSBox target copies the prebuilt
`RTCORE.EXE` and `CWSDPMI.EXE` into a temporary directory, mounts it with an
empty configuration under vanilla DOSBox 0.74-3, and accepts only a passing
`RUNTIME.OUT`. `RTCORE.EXE` is a test artifact and is absent from both game and
tools distributions.

The tests cover rational 35/60 cadence over a simulated hour, legacy cadence,
clock wrap, elapsed and catch-up clamps, fractional retention, interpolation
bounds, sampled pause transitions, scan-code namespaces and recovery, edge
latching, repeat timing, rebinding, and input processing across catch-up
boundaries.

These are deterministic API and compatibility gates. They do not measure VGA
refresh, frame-time variance, emulator pacing, renderer cost, or Pentium 90
performance, and therefore do not by themselves establish the project goal of
stutter-free physical 60 Hz presentation.

## Deferred integration

The next runtime layer must provide the DJGPP/DOS clock source, interrupt-safe
keyboard capture, and presentation adapter, then migrate ZEUS to consume
`MoonFramePlan` and interpolated snapshots. Physical 60 Hz acceptance remains a
separate manual gate on the supported DOSBox and 86Box hardware profiles.
