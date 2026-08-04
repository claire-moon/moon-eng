#include "moon/runtime.h"

#include <limits.h>
#include <string.h>

static int moon_runtime_config_valid(const MoonRuntimeConfig *config)
{
    if (config == NULL || config->clock_frequency < MOON_MIN_CLOCK_HZ ||
        config->max_elapsed_ticks == 0u ||
        config->max_elapsed_ticks > UINT32_MAX / 2u) {
        return 0;
    }

    if (config->present_mode != MOON_PRESENT_60 &&
        config->present_mode != MOON_PRESENT_LEGACY_35) {
        return 0;
    }

    if (config->action_count > MOON_ACTION_CAPACITY ||
        config->repeat_delay_ticks == 0u ||
        config->repeat_rate_ticks == 0u) {
        return 0;
    }

    return 1;
}

static int moon_runtime_clock_valid(const MoonClock *clock)
{
    if (clock == NULL || clock->frequency < MOON_MIN_CLOCK_HZ ||
        clock->max_elapsed_ticks == 0u ||
        clock->max_elapsed_ticks > UINT32_MAX / 2u ||
        clock->simulation_phase >= clock->frequency ||
        clock->paused_ui_phase >= clock->frequency ||
        clock->presentation_phase >= clock->frequency ||
        clock->paused > 1u) {
        return 0;
    }

    return clock->present_mode == MOON_PRESENT_60 ||
           clock->present_mode == MOON_PRESENT_LEGACY_35;
}

MoonResult moon_runtime_config_defaults(MoonRuntimeConfig *config,
                                        uint32_t clock_frequency)
{
    uint32_t maximum_elapsed;

    if (config == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    memset(config, 0, sizeof(*config));
    if (clock_frequency < MOON_MIN_CLOCK_HZ) {
        return MOON_ERR_CONFIG;
    }

    maximum_elapsed = clock_frequency / 4u;
    if (maximum_elapsed == 0u) {
        maximum_elapsed = 1u;
    }
    if (maximum_elapsed > UINT32_MAX / 2u) {
        maximum_elapsed = UINT32_MAX / 2u;
    }

    config->clock_frequency = clock_frequency;
    config->max_elapsed_ticks = maximum_elapsed;
    config->present_mode = MOON_PRESENT_60;
    config->action_count = MOON_ACTION_CAPACITY;
    config->repeat_delay_ticks = 12u;
    config->repeat_rate_ticks = 3u;

    return MOON_OK;
}

MoonResult moon_input_init(MoonInput *input,
                           uint16_t action_count,
                           uint16_t repeat_delay_ticks,
                           uint16_t repeat_rate_ticks)
{
    uint16_t action;
    uint16_t slot;

    if (input == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    memset(input, 0, sizeof(*input));
    if (action_count > MOON_ACTION_CAPACITY || repeat_delay_ticks == 0u ||
        repeat_rate_ticks == 0u) {
        return MOON_ERR_CONFIG;
    }

    input->action_count = action_count;
    input->repeat_delay_ticks = repeat_delay_ticks;
    input->repeat_rate_ticks = repeat_rate_ticks;

    for (action = 0u; action < MOON_ACTION_CAPACITY; ++action) {
        for (slot = 0u; slot < MOON_BINDINGS_PER_ACTION; ++slot) {
            input->bindings[action][slot] = MOON_KEY_UNBOUND;
        }
    }

    return MOON_OK;
}

MoonResult moon_runtime_init(MoonContext *context,
                             const MoonRuntimeConfig *config,
                             uint32_t initial_tick)
{
    MoonResult input_result;

    if (context == NULL) {
        return MOON_ERR_ARGUMENT;
    }
    memset(context, 0, sizeof(*context));
    if (config == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    if (!moon_runtime_config_valid(config)) {
        return MOON_ERR_CONFIG;
    }

    context->clock.frequency = config->clock_frequency;
    context->clock.max_elapsed_ticks = config->max_elapsed_ticks;
    context->clock.last_tick = initial_tick;
    context->clock.present_mode = config->present_mode;

    input_result = moon_input_init(&context->input,
                                   config->action_count,
                                   config->repeat_delay_ticks,
                                   config->repeat_rate_ticks);
    if (input_result != MOON_OK) {
        memset(context, 0, sizeof(*context));
        return input_result;
    }

    return MOON_OK;
}

MoonResult moon_runtime_advance(MoonContext *context,
                                uint32_t current_tick,
                                MoonFramePlan *plan)
{
    uint32_t elapsed;
    uint32_t accepted_elapsed;
    uint32_t presentation_rate;
    uint64_t fixed_total;
    uint64_t fixed_due;
    uint64_t presentation_total;
    uint64_t presentation_due;
    uint64_t scheduled_fixed;

    if (plan == NULL) {
        return MOON_ERR_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));

    if (context == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    if (!moon_runtime_clock_valid(&context->clock)) {
        return MOON_ERR_CONFIG;
    }

    ++context->telemetry.advances;

    /* Unsigned subtraction intentionally handles one low-end wrap. */
    elapsed = current_tick - context->clock.last_tick;
    context->clock.last_tick = current_tick;
    accepted_elapsed = elapsed;

    if (accepted_elapsed > context->clock.max_elapsed_ticks) {
        context->telemetry.discarded_source_ticks +=
            (uint64_t)(accepted_elapsed - context->clock.max_elapsed_ticks);
        accepted_elapsed = context->clock.max_elapsed_ticks;
        ++context->telemetry.elapsed_clamp_events;
        plan->elapsed_clamped = 1u;
    }

    context->telemetry.accepted_source_ticks += (uint64_t)accepted_elapsed;

    fixed_total =
        (uint64_t)(context->clock.paused != 0u
                       ? context->clock.paused_ui_phase
                       : context->clock.simulation_phase) +
        (uint64_t)accepted_elapsed * (uint64_t)MOON_SIMULATION_HZ;
    fixed_due = fixed_total / (uint64_t)context->clock.frequency;
    if (context->clock.paused != 0u) {
        context->clock.paused_ui_phase =
            (uint32_t)(fixed_total % (uint64_t)context->clock.frequency);
    } else {
        context->clock.simulation_phase =
            (uint32_t)(fixed_total % (uint64_t)context->clock.frequency);
        context->clock.paused_ui_phase = context->clock.simulation_phase;
    }

    scheduled_fixed = fixed_due;
    if (scheduled_fixed > (uint64_t)MOON_MAX_FIXED_TICKS) {
        scheduled_fixed = (uint64_t)MOON_MAX_FIXED_TICKS;
        context->telemetry.dropped_fixed_ticks +=
            fixed_due - scheduled_fixed;
        ++context->telemetry.catchup_clamp_events;
        plan->catchup_clamped = 1u;
    }

    plan->fixed_ticks = (uint32_t)scheduled_fixed;
    context->telemetry.fixed_ticks += scheduled_fixed;
    if (scheduled_fixed > 1u) {
        context->telemetry.catchup_ticks += scheduled_fixed - 1u;
    }

    if (context->clock.paused != 0u) {
        context->telemetry.paused_simulation_ticks += scheduled_fixed;
    } else {
        plan->simulation_ticks = plan->fixed_ticks;
        context->telemetry.simulation_ticks += scheduled_fixed;
    }

    presentation_rate =
        context->clock.present_mode == MOON_PRESENT_LEGACY_35
            ? MOON_SIMULATION_HZ
            : MOON_PRESENTATION_HZ;
    presentation_total = (uint64_t)context->clock.presentation_phase +
                         (uint64_t)accepted_elapsed *
                             (uint64_t)presentation_rate;
    presentation_due =
        presentation_total / (uint64_t)context->clock.frequency;
    context->clock.presentation_phase =
        (uint32_t)(presentation_total % (uint64_t)context->clock.frequency);

    if (presentation_due != 0u) {
        plan->present = 1u;
        ++context->telemetry.presented_frames;
        context->telemetry.missed_present_intervals +=
            presentation_due - 1u;
    }

    if (context->clock.present_mode == MOON_PRESENT_60) {
        plan->alpha_q16 = (uint16_t)(
            ((uint64_t)context->clock.simulation_phase * UINT64_C(65536)) /
            (uint64_t)context->clock.frequency);
    }

    return MOON_OK;
}

MoonResult moon_runtime_set_paused(MoonContext *context,
                                   int paused,
                                   uint32_t current_tick,
                                   MoonFramePlan *plan)
{
    MoonResult result;
    uint8_t new_paused;

    result = moon_runtime_advance(context, current_tick, plan);
    if (result != MOON_OK) {
        return result;
    }

    new_paused = paused != 0 ? 1u : 0u;
    if (new_paused != 0u && context->clock.paused == 0u) {
        context->clock.paused_ui_phase = context->clock.simulation_phase;
    }
    context->clock.paused = new_paused;
    return MOON_OK;
}

static int moon_input_action_uses_key(const MoonInput *input,
                                      uint16_t action,
                                      MoonKey key)
{
    uint16_t slot;

    for (slot = 0u; slot < MOON_BINDINGS_PER_ACTION; ++slot) {
        if (input->bindings[action][slot] == key) {
            return 1;
        }
    }

    return 0;
}

static int moon_input_action_raw_down(const MoonInput *input,
                                      uint16_t action)
{
    uint16_t slot;

    for (slot = 0u; slot < MOON_BINDINGS_PER_ACTION; ++slot) {
        MoonKey key;

        key = input->bindings[action][slot];
        if (key != MOON_KEY_UNBOUND && input->raw_keys[key] != 0u) {
            return 1;
        }
    }

    return 0;
}

static void moon_input_apply_key(MoonInput *input, MoonKey key, int down)
{
    uint16_t action;
    uint8_t new_down;
    uint8_t old_down;

    new_down = down != 0 ? 1u : 0u;
    old_down = input->raw_keys[key] != 0u ? 1u : 0u;
    if (new_down == old_down) {
        return;
    }

    if (new_down != 0u) {
        for (action = 0u; action < input->action_count; ++action) {
            if (moon_input_action_uses_key(input, action, key) &&
                !moon_input_action_raw_down(input, action)) {
                input->pending_pressed[action] = 1u;
            }
        }
        input->raw_keys[key] = 1u;
    } else {
        input->raw_keys[key] = 0u;
        for (action = 0u; action < input->action_count; ++action) {
            if (moon_input_action_uses_key(input, action, key) &&
                !moon_input_action_raw_down(input, action)) {
                input->pending_released[action] = 1u;
            }
        }
    }
}

int moon_runtime_is_paused(const MoonContext *context)
{
    return context != NULL && context->clock.paused != 0u;
}

MoonResult moon_input_bind(MoonInput *input,
                           uint16_t action,
                           uint16_t slot,
                           MoonKey key)
{
    int old_down;
    int new_down;

    if (input == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    if (action >= input->action_count || slot >= MOON_BINDINGS_PER_ACTION) {
        return MOON_ERR_RANGE;
    }

    if (key != MOON_KEY_UNBOUND && key >= MOON_KEY_COUNT) {
        return MOON_ERR_RANGE;
    }

    old_down = moon_input_action_raw_down(input, action);
    input->bindings[action][slot] = key;
    new_down = moon_input_action_raw_down(input, action);
    if (!old_down && new_down) {
        input->pending_pressed[action] = 1u;
    } else if (old_down && !new_down) {
        input->pending_released[action] = 1u;
    }
    return MOON_OK;
}

MoonResult moon_input_set_key(MoonInput *input, MoonKey key, int down)
{
    if (input == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    if (key >= MOON_KEY_COUNT) {
        return MOON_ERR_RANGE;
    }

    moon_input_apply_key(input, key, down);
    return MOON_OK;
}

MoonResult moon_input_feed_scancode(MoonInput *input, uint8_t scan_code)
{
    static const uint8_t pause_tail[] = {
        UINT8_C(0x1d), UINT8_C(0x45), UINT8_C(0xe1),
        UINT8_C(0x9d), UINT8_C(0xc5)
    };
    MoonKey key;
    int down;

    if (input == NULL) {
        return MOON_ERR_ARGUMENT;
    }

    if (input->e1_sequence_index != 0u) {
        size_t expected_index;

        expected_index = (size_t)input->e1_sequence_index - 1u;
        if (expected_index < sizeof(pause_tail) &&
            scan_code == pause_tail[expected_index]) {
            ++input->e1_sequence_index;
            if ((size_t)input->e1_sequence_index > sizeof(pause_tail)) {
                input->e1_sequence_index = 0u;
            }
            return MOON_OK;
        }

        /* Reprocess the mismatching byte as a fresh decoder byte. */
        input->e1_sequence_index = 0u;
        input->e0_pending = 0u;
    }

    if (scan_code == UINT8_C(0x00) || scan_code == UINT8_C(0xff)) {
        input->e0_pending = 0u;
        input->e1_sequence_index = 0u;
        return MOON_OK;
    }

    if (scan_code == UINT8_C(0xe1)) {
        input->e0_pending = 0u;
        input->e1_sequence_index = 1u;
        return MOON_OK;
    }

    if (scan_code == UINT8_C(0xe0)) {
        input->e0_pending = 1u;
        return MOON_OK;
    }

    /* Set-1 extended sequences can wrap real keys in fake Shift bytes. */
    if (input->e0_pending != 0u &&
        ((scan_code & UINT8_C(0x7f)) == UINT8_C(0x2a) ||
         (scan_code & UINT8_C(0x7f)) == UINT8_C(0x36))) {
        input->e0_pending = 0u;
        return MOON_OK;
    }

    down = (scan_code & UINT8_C(0x80)) == 0u;
    key = input->e0_pending != 0u
              ? MOON_KEY_E0(scan_code)
              : MOON_KEY_NORMAL(scan_code);
    input->e0_pending = 0u;
    moon_input_apply_key(input, key, down);

    return MOON_OK;
}

void moon_input_clear_raw(MoonInput *input)
{
    uint16_t action;
    size_t key;

    if (input == NULL) {
        return;
    }

    for (action = 0u; action < input->action_count; ++action) {
        if (moon_input_action_raw_down(input, action)) {
            input->pending_released[action] = 1u;
        }
    }

    for (key = 0u; key < MOON_KEY_COUNT; ++key) {
        input->raw_keys[key] = 0u;
    }
    input->e0_pending = 0u;
    input->e1_sequence_index = 0u;
}

void moon_input_resync(MoonInput *input)
{
    if (input == NULL) {
        return;
    }

    memset(input->raw_keys, 0, sizeof(input->raw_keys));
    memset(input->pending_pressed, 0, sizeof(input->pending_pressed));
    memset(input->pending_released, 0, sizeof(input->pending_released));
    memset(input->held_ticks, 0, sizeof(input->held_ticks));
    input->e0_pending = 0u;
    input->e1_sequence_index = 0u;
}

void moon_input_tick(MoonInput *input)
{
    uint16_t action;

    if (input == NULL) {
        return;
    }

    for (action = 0u; action < input->action_count; ++action) {
        MoonButtonState *state;
        uint16_t slot;
        uint8_t down;
        uint8_t pending_pressed;
        uint8_t pending_released;
        uint8_t was_held;

        state = &input->actions[action];
        down = 0u;
        for (slot = 0u; slot < MOON_BINDINGS_PER_ACTION; ++slot) {
            MoonKey key;

            key = input->bindings[action][slot];
            if (key != MOON_KEY_UNBOUND && input->raw_keys[key] != 0u) {
                down = 1u;
                break;
            }
        }

        was_held = state->held;
        pending_pressed = input->pending_pressed[action];
        pending_released = input->pending_released[action];
        input->pending_pressed[action] = 0u;
        input->pending_released[action] = 0u;
        state->held = down;
        state->pressed = pending_pressed != 0u ||
                                 (down != 0u && was_held == 0u)
                             ? 1u
                             : 0u;
        state->released = pending_released != 0u ||
                                  (down == 0u && was_held != 0u)
                              ? 1u
                              : 0u;
        state->repeat = 0u;

        if (down == 0u) {
            input->held_ticks[action] = 0u;
        } else if (was_held == 0u || pending_released != 0u) {
            input->held_ticks[action] = 0u;
        } else if (input->held_ticks[action] <
                   (uint32_t)input->repeat_delay_ticks) {
            ++input->held_ticks[action];
            if (input->held_ticks[action] ==
                (uint32_t)input->repeat_delay_ticks) {
                state->repeat = 1u;
            }
        } else {
            uint32_t repeat_phase;

            repeat_phase =
                (input->held_ticks[action] -
                 (uint32_t)input->repeat_delay_ticks) %
                (uint32_t)input->repeat_rate_ticks;
            ++repeat_phase;
            if (repeat_phase == (uint32_t)input->repeat_rate_ticks) {
                repeat_phase = 0u;
                state->repeat = 1u;
            }
            input->held_ticks[action] =
                (uint32_t)input->repeat_delay_ticks + repeat_phase;
        }
    }
}

const MoonButtonState *moon_input_action(const MoonInput *input,
                                         uint16_t action)
{
    if (input == NULL || action >= input->action_count) {
        return NULL;
    }

    return &input->actions[action];
}
