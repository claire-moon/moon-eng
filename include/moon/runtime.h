#ifndef MOON_RUNTIME_H
#define MOON_RUNTIME_H

/*
 * Portable fixed-step and action-input core for MOON ENG.
 *
 * This interface contains no hardware access and performs no allocation.
 * The executable owns MoonContext and supplies a wrapping, monotonically
 * advancing 32-bit clock value to moon_runtime_advance().
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOON_SIMULATION_HZ 35u
#define MOON_PRESENTATION_HZ 60u
#define MOON_MIN_CLOCK_HZ MOON_PRESENTATION_HZ
#define MOON_MAX_FIXED_TICKS 4u

#define MOON_KEY_COUNT 256u
#define MOON_ACTION_CAPACITY 32u
#define MOON_BINDINGS_PER_ACTION 2u

typedef uint16_t MoonKey;

#define MOON_KEY_UNBOUND ((MoonKey)UINT16_MAX)
#define MOON_KEY_NORMAL(scan_code) \
    ((MoonKey)((uint16_t)(scan_code) & UINT16_C(0x007f)))
#define MOON_KEY_E0(scan_code) \
    ((MoonKey)(UINT16_C(0x0080) | \
               ((uint16_t)(scan_code) & UINT16_C(0x007f))))

typedef enum MoonResult {
    MOON_OK = 0,
    MOON_ERR_ARGUMENT,
    MOON_ERR_CONFIG,
    MOON_ERR_RANGE
} MoonResult;

typedef enum MoonPresentMode {
    MOON_PRESENT_60 = 0,
    MOON_PRESENT_LEGACY_35
} MoonPresentMode;

typedef struct MoonButtonState {
    uint8_t held;
    uint8_t pressed;
    uint8_t released;
    uint8_t repeat;
} MoonButtonState;

typedef struct MoonInput {
    uint8_t raw_keys[MOON_KEY_COUNT];
    MoonKey bindings[MOON_ACTION_CAPACITY][MOON_BINDINGS_PER_ACTION];
    MoonButtonState actions[MOON_ACTION_CAPACITY];
    uint8_t pending_pressed[MOON_ACTION_CAPACITY];
    uint8_t pending_released[MOON_ACTION_CAPACITY];
    /* Bounded repeat-phase counters, not lifetime hold telemetry. */
    uint32_t held_ticks[MOON_ACTION_CAPACITY];
    uint16_t action_count;
    uint16_t repeat_delay_ticks;
    uint16_t repeat_rate_ticks;
    uint8_t e0_pending;
    uint8_t e1_sequence_index;
} MoonInput;

typedef struct MoonRuntimeConfig {
    /*
     * Frequency of the caller's wrapping 32-bit monotonic clock.  It must be
     * at least MOON_MIN_CLOCK_HZ so every presentation interval can be issued
     * as a distinct frame request.
     */
    uint32_t clock_frequency;

    /*
     * Largest modular clock delta accepted by one advance.  Larger deltas
     * are reduced to this value and reported in telemetry.  This must be no
     * greater than UINT32_MAX / 2 so a small backward jump is not accepted as
     * an ordinary forward interval.
     */
    uint32_t max_elapsed_ticks;

    MoonPresentMode present_mode;
    uint16_t action_count;
    uint16_t repeat_delay_ticks;
    uint16_t repeat_rate_ticks;
} MoonRuntimeConfig;

typedef struct MoonClock {
    uint32_t frequency;
    uint32_t max_elapsed_ticks;
    uint32_t last_tick;
    uint32_t simulation_phase;
    uint32_t paused_ui_phase;
    uint32_t presentation_phase;
    MoonPresentMode present_mode;
    uint8_t paused;
} MoonClock;

typedef struct MoonTelemetry {
    uint64_t advances;
    uint64_t accepted_source_ticks;
    uint64_t discarded_source_ticks;
    uint64_t elapsed_clamp_events;

    uint64_t fixed_ticks;
    uint64_t simulation_ticks;
    /* Delivered fixed boundaries whose simulation work pause suppressed. */
    uint64_t paused_simulation_ticks;
    uint64_t catchup_ticks;
    uint64_t dropped_fixed_ticks;
    uint64_t catchup_clamp_events;

    uint64_t presented_frames;
    uint64_t missed_present_intervals;
} MoonTelemetry;

typedef struct MoonFramePlan {
    /*
     * Number of 35 Hz boundaries the caller must consume, never greater than
     * MOON_MAX_FIXED_TICKS.  Call moon_input_tick() once per fixed tick.
     */
    uint32_t fixed_ticks;

    /* Equal to fixed_ticks while running and zero while paused. */
    uint32_t simulation_ticks;

    /* At most one presentation is requested from a single advance. */
    uint8_t present;
    uint8_t elapsed_clamped;
    uint8_t catchup_clamped;

    /*
     * Current/previous snapshot interpolation weight in unsigned Q0.16.
     * This remains frozen with simulation_phase while paused.
     */
    uint16_t alpha_q16;
} MoonFramePlan;

typedef struct MoonContext {
    MoonClock clock;
    MoonInput input;
    MoonTelemetry telemetry;
} MoonContext;

/* Fills a practical default configuration for the supplied clock frequency. */
MoonResult moon_runtime_config_defaults(MoonRuntimeConfig *config,
                                        uint32_t clock_frequency);

/* Initializes all caller-owned state and establishes the first clock sample. */
MoonResult moon_runtime_init(MoonContext *context,
                             const MoonRuntimeConfig *config,
                             uint32_t initial_tick);

/*
 * Advances both rational clocks from a wrapping unsigned tick sample.  The
 * modular subtraction is wrap-safe provided consecutive real samples are no
 * farther apart than UINT32_MAX / 2 source ticks.
 */
MoonResult moon_runtime_advance(MoonContext *context,
                                uint32_t current_tick,
                                MoonFramePlan *plan);

/*
 * Accounts for elapsed time under the old pause state at current_tick, returns
 * that work in plan, and then applies the requested state.  The caller must
 * consume the returned plan exactly like one from moon_runtime_advance().
 *
 * Pause freezes the simulation phase and interpolation alpha while a separate
 * 35 Hz input/UI phase and the selected presentation cadence continue.  On
 * resume, fixed/input boundaries intentionally return to the frozen simulation
 * phase; this one transition realignment preserves active-time determinism.
 */
MoonResult moon_runtime_set_paused(MoonContext *context,
                                   int paused,
                                   uint32_t current_tick,
                                   MoonFramePlan *plan);
int moon_runtime_is_paused(const MoonContext *context);

/* Standalone input initialization is available to host-side tools and tests. */
MoonResult moon_input_init(MoonInput *input,
                           uint16_t action_count,
                           uint16_t repeat_delay_ticks,
                           uint16_t repeat_rate_ticks);

/*
 * Binds one of two key slots.  MOON_KEY_UNBOUND clears a slot.  A binding
 * change that changes the logical held state latches the corresponding edge.
 */
MoonResult moon_input_bind(MoonInput *input,
                           uint16_t action,
                           uint16_t slot,
                           MoonKey key);

/*
 * Direct normalized-key update for a platform adapter or deterministic test.
 * Logical action edges are latched until moon_input_tick() consumes them.
 */
MoonResult moon_input_set_key(MoonInput *input, MoonKey key, int down);

/*
 * Consumes one PC set-1 scan-code byte.  E0 keys occupy indices 128..255,
 * distinct from their non-extended 0..127 counterparts.  The E1 Pause
 * sequence, controller error bytes 00/FF, and synthetic E0 Shift wrappers are
 * ignored rather than leaking their component bytes as keys.
 */
MoonResult moon_input_feed_scancode(MoonInput *input, uint8_t scan_code);

/* Releases all raw keys; releases become visible on the next input tick. */
void moon_input_clear_raw(MoonInput *input);

/*
 * Hard decoder recovery for a lost/corrupt byte stream.  Pending raw history,
 * decoder prefixes, and repeat phase are discarded.  At the next input tick,
 * only actions that were already published as held report a release.
 */
void moon_input_resync(MoonInput *input);

/*
 * Converts raw keys to held/pressed/released/repeat at one fixed boundary.
 * A complete tap between boundaries reports both pressed and released while
 * held reflects the final raw state.  After initialization, every mutation
 * (bind, set, feed, clear, resync, or tick) must be serialized by the platform
 * adapter.  Volatile storage is not synchronization; the preferred DOS design
 * queues bytes in the IRQ handler and drains them on the foreground thread.
 */
void moon_input_tick(MoonInput *input);

const MoonButtonState *moon_input_action(const MoonInput *input,
                                         uint16_t action);

#ifdef __cplusplus
}
#endif

#endif
