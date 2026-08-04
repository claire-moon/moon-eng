#include "moon/runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
            return 0;                                                        \
        }                                                                    \
    } while (0)

static int make_config(MoonRuntimeConfig *config,
                       uint32_t frequency,
                       uint32_t maximum_elapsed,
                       MoonPresentMode mode,
                       uint16_t action_count,
                       uint16_t repeat_delay,
                       uint16_t repeat_rate)
{
    CHECK(config != NULL);
    config->clock_frequency = frequency;
    config->max_elapsed_ticks = maximum_elapsed;
    config->present_mode = mode;
    config->action_count = action_count;
    config->repeat_delay_ticks = repeat_delay;
    config->repeat_rate_ticks = repeat_rate;
    return 1;
}

static int test_configuration(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    MoonFramePlan zero_plan;
    MoonInput input;

    memset(&config, 0xa5, sizeof(config));
    CHECK(moon_runtime_config_defaults(NULL, 1000u) == MOON_ERR_ARGUMENT);
    CHECK(moon_runtime_config_defaults(&config, 0u) == MOON_ERR_CONFIG);
    CHECK(config.clock_frequency == 0u);
    memset(&config, 0xa5, sizeof(config));
    CHECK(moon_runtime_config_defaults(&config, 59u) == MOON_ERR_CONFIG);
    CHECK(config.clock_frequency == 0u);
    CHECK(moon_runtime_config_defaults(&config, 60u) == MOON_OK);
    CHECK(config.clock_frequency == 60u);
    CHECK(config.max_elapsed_ticks == 15u);

    CHECK(moon_runtime_config_defaults(&config, 1193180u) == MOON_OK);
    CHECK(config.clock_frequency == 1193180u);
    CHECK(config.max_elapsed_ticks == 298295u);
    CHECK(config.present_mode == MOON_PRESENT_60);
    CHECK(config.action_count == MOON_ACTION_CAPACITY);
    CHECK(config.repeat_delay_ticks == 12u);
    CHECK(config.repeat_rate_ticks == 3u);
    CHECK(moon_runtime_config_defaults(&config, UINT32_MAX) == MOON_OK);
    CHECK(config.clock_frequency == UINT32_MAX);
    CHECK(config.max_elapsed_ticks == UINT32_MAX / 4u);
    CHECK(moon_runtime_config_defaults(&config, 1193180u) == MOON_OK);
    CHECK(moon_runtime_init(&context, &config, UINT32_C(0x12345678)) ==
          MOON_OK);
    CHECK(context.clock.last_tick == UINT32_C(0x12345678));

    CHECK(moon_runtime_init(NULL, &config, 0u) == MOON_ERR_ARGUMENT);
    CHECK(moon_runtime_init(&context, NULL, 0u) == MOON_ERR_ARGUMENT);
    memset(&zero_plan, 0, sizeof(zero_plan));
    memset(&plan, 0xa5, sizeof(plan));
    CHECK(moon_runtime_advance(NULL, 0u, &plan) == MOON_ERR_ARGUMENT);
    CHECK(memcmp(&plan, &zero_plan, sizeof(plan)) == 0);
    CHECK(moon_runtime_advance(&context, 0u, NULL) == MOON_ERR_ARGUMENT);

    config.clock_frequency = 59u;
    memset(&context, 0xa5, sizeof(context));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    CHECK(context.clock.frequency == 0u);
    config.clock_frequency = 60u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    config.clock_frequency = 1000u;
    config.max_elapsed_ticks = 0u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    config.max_elapsed_ticks = UINT32_MAX / 2u + 1u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    config.max_elapsed_ticks = 100u;
    config.present_mode = (MoonPresentMode)99;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    config.present_mode = MOON_PRESENT_60;
    config.action_count = MOON_ACTION_CAPACITY + 1u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    config.action_count = 1u;
    config.repeat_delay_ticks = 0u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);
    config.repeat_delay_ticks = 1u;
    config.repeat_rate_ticks = 0u;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_ERR_CONFIG);

    memset(&context, 0xa5, sizeof(context));
    CHECK(moon_runtime_init(&context, NULL, 0u) == MOON_ERR_ARGUMENT);
    CHECK(context.clock.frequency == 0u);
    memset(&input, 0xa5, sizeof(input));
    CHECK(moon_input_init(&input, 1u, 0u, 1u) == MOON_ERR_CONFIG);
    CHECK(input.action_count == 0u);

    memset(&context, 0, sizeof(context));
    memset(&plan, 0xa5, sizeof(plan));
    CHECK(moon_runtime_advance(&context, 0u, &plan) == MOON_ERR_CONFIG);
    CHECK(memcmp(&plan, &zero_plan, sizeof(plan)) == 0);

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    context.clock.present_mode = (MoonPresentMode)99;
    memset(&plan, 0xa5, sizeof(plan));
    CHECK(moon_runtime_advance(&context, 1u, &plan) == MOON_ERR_CONFIG);
    CHECK(memcmp(&plan, &zero_plan, sizeof(plan)) == 0);
    CHECK(context.clock.last_tick == 0u);

    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    context.clock.simulation_phase = context.clock.frequency;
    CHECK(moon_runtime_advance(&context, 1u, &plan) == MOON_ERR_CONFIG);
    return 1;
}

static int test_golden_rational_sequence(void)
{
    static const uint32_t deltas[] = {
        16u, 1u, 11u, 1u, 4u, 17u, 13u, 20u, 17u
    };
    static const uint32_t expected_fixed[] = {
        0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u
    };
    static const uint8_t expected_present[] = {
        0u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u
    };
    static const uint16_t expected_alpha[] = {
        36700u, 38993u, 64225u, 983u, 10158u,
        49152u, 13434u, 59310u, 32768u
    };
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint32_t now;
    size_t index;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    now = 0u;

    for (index = 0u; index < sizeof(deltas) / sizeof(deltas[0]); ++index) {
        now += deltas[index];
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        CHECK(plan.fixed_ticks == expected_fixed[index]);
        CHECK(plan.simulation_ticks == expected_fixed[index]);
        CHECK(plan.present == expected_present[index]);
        CHECK(plan.alpha_q16 == expected_alpha[index]);
        CHECK(plan.elapsed_clamped == 0u);
        CHECK(plan.catchup_clamped == 0u);
    }

    CHECK(now == 100u);
    CHECK(context.clock.simulation_phase == 500u);
    CHECK(context.clock.presentation_phase == 0u);
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(100));
    CHECK(context.telemetry.fixed_ticks == UINT64_C(3));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(3));
    CHECK(context.telemetry.presented_frames == UINT64_C(4));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(2));
    CHECK(context.telemetry.catchup_clamp_events == UINT64_C(0));
    return 1;
}

static int test_minimum_clock_cadence(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint32_t fixed;
    uint32_t presented;
    uint32_t now;

    CHECK(make_config(&config, MOON_MIN_CLOCK_HZ, 15u,
                      MOON_PRESENT_60, 0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    fixed = 0u;
    presented = 0u;
    for (now = 1u; now <= MOON_MIN_CLOCK_HZ; ++now) {
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        fixed += plan.fixed_ticks;
        presented += plan.present;
    }
    CHECK(fixed == MOON_SIMULATION_HZ);
    CHECK(presented == MOON_PRESENTATION_HZ);
    CHECK(context.clock.simulation_phase == 0u);
    CHECK(context.clock.presentation_phase == 0u);
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(0));

    config.present_mode = MOON_PRESENT_LEGACY_35;
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    fixed = 0u;
    presented = 0u;
    for (now = 1u; now <= MOON_MIN_CLOCK_HZ; ++now) {
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        fixed += plan.fixed_ticks;
        presented += plan.present;
        CHECK(plan.alpha_q16 == 0u);
    }
    CHECK(fixed == MOON_SIMULATION_HZ);
    CHECK(presented == MOON_SIMULATION_HZ);
    CHECK(context.clock.simulation_phase == 0u);
    CHECK(context.clock.presentation_phase == 0u);
    return 1;
}

static int test_legacy_35_sequence(void)
{
    static const uint32_t deltas[] = {
        16u, 1u, 11u, 1u, 4u, 17u, 13u, 20u, 17u
    };
    static const uint8_t expected_present[] = {
        0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u
    };
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint32_t now;
    size_t index;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_LEGACY_35,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    now = 0u;

    for (index = 0u; index < sizeof(deltas) / sizeof(deltas[0]); ++index) {
        now += deltas[index];
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        CHECK(plan.present == expected_present[index]);
        CHECK(plan.alpha_q16 == 0u);
    }

    CHECK(context.telemetry.simulation_ticks == UINT64_C(3));
    CHECK(context.telemetry.presented_frames == UINT64_C(3));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(0));
    CHECK(context.clock.simulation_phase == context.clock.presentation_phase);
    return 1;
}

static int test_long_duration_exactness_and_wrap(void)
{
    static const uint32_t golden_deltas[] = {
        130001u, 121003u, 110017u, 90001u,
        127997u, 100003u, 80009u, 136001u
    };
    const uint32_t frequency = 1193180u;
    const uint64_t duration = UINT64_C(3600);
    const uint64_t target_source_ticks =
        (uint64_t)frequency * duration;
    const uint32_t initial_tick = UINT32_C(0xffff0000);
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint64_t consumed;
    uint64_t simulated;
    uint64_t presented;
    uint32_t now;
    uint32_t wraps;
    size_t index;

    CHECK(moon_runtime_config_defaults(&config, frequency) == MOON_OK);
    config.action_count = 0u;
    CHECK(moon_runtime_init(&context, &config, initial_tick) == MOON_OK);

    consumed = 0u;
    simulated = 0u;
    presented = 0u;
    now = initial_tick;
    wraps = 0u;
    index = 0u;

    while (consumed < target_source_ticks) {
        uint64_t remaining;
        uint32_t delta;
        uint32_t next;

        delta = golden_deltas[index];
        remaining = target_source_ticks - consumed;
        if ((uint64_t)delta > remaining) {
            delta = (uint32_t)remaining;
        }

        next = now + delta;
        if (next < now) {
            ++wraps;
        }
        now = next;
        consumed += delta;
        index = (index + 1u) %
                (sizeof(golden_deltas) / sizeof(golden_deltas[0]));

        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        CHECK(plan.fixed_ticks <= MOON_MAX_FIXED_TICKS);
        CHECK(plan.catchup_clamped == 0u);
        CHECK(plan.elapsed_clamped == 0u);
        simulated += plan.simulation_ticks;
        presented += plan.present;
    }

    CHECK(wraps == 2u);
    CHECK(now == (uint32_t)((uint64_t)initial_tick + target_source_ticks));
    CHECK(simulated == UINT64_C(126000));
    CHECK(presented > UINT64_C(0) && presented < UINT64_C(216000));
    CHECK(context.clock.simulation_phase == 0u);
    CHECK(context.clock.presentation_phase == 0u);
    CHECK(context.telemetry.accepted_source_ticks == target_source_ticks);
    CHECK(context.telemetry.simulation_ticks == UINT64_C(126000));
    CHECK(context.telemetry.presented_frames == presented);
    CHECK(context.telemetry.presented_frames +
              context.telemetry.missed_present_intervals ==
          UINT64_C(216000));
    CHECK(context.telemetry.elapsed_clamp_events == UINT64_C(0));
    CHECK(context.telemetry.catchup_clamp_events == UINT64_C(0));
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(0));
    return 1;
}

static int test_catchup_drop_retains_fraction(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);

    CHECK(moon_runtime_advance(&context, 300u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.simulation_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.catchup_clamped == 1u);
    CHECK(plan.elapsed_clamped == 0u);
    CHECK(plan.alpha_q16 == 32768u);
    CHECK(context.clock.simulation_phase == 500u);
    CHECK(context.telemetry.catchup_ticks == UINT64_C(3));
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(6));
    CHECK(context.telemetry.catchup_clamp_events == UINT64_C(1));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(4));
    CHECK(context.telemetry.presented_frames == UINT64_C(1));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(17));

    CHECK(moon_runtime_advance(&context, 315u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 1u);
    CHECK(plan.simulation_ticks == 1u);
    CHECK(plan.catchup_clamped == 0u);
    CHECK(context.clock.simulation_phase == 25u);
    CHECK(plan.alpha_q16 == 1638u);
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(6));
    return 1;
}

static int test_elapsed_clamp_and_modular_wrap(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint32_t start;

    CHECK(make_config(&config, 1000u, 100u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    CHECK(moon_runtime_advance(&context, 250u, &plan) == MOON_OK);
    CHECK(plan.elapsed_clamped == 1u);
    CHECK(plan.fixed_ticks == 3u);
    CHECK(context.clock.simulation_phase == 500u);
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(100));
    CHECK(context.telemetry.discarded_source_ticks == UINT64_C(150));
    CHECK(context.telemetry.elapsed_clamp_events == UINT64_C(1));

    /* last_tick follows the supplied clock, not the shortened interval. */
    CHECK(moon_runtime_advance(&context, 260u, &plan) == MOON_OK);
    CHECK(plan.elapsed_clamped == 0u);
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(110));

    start = UINT32_MAX - 10u;
    CHECK(moon_runtime_init(&context, &config, start) == MOON_OK);
    CHECK(moon_runtime_advance(&context, 5u, &plan) == MOON_OK);
    CHECK(plan.elapsed_clamped == 0u);
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(16));
    CHECK(context.clock.simulation_phase == 560u);

    /* A small backward sample becomes a huge modular delta and is clamped. */
    CHECK(moon_runtime_advance(&context, 4u, &plan) == MOON_OK);
    CHECK(plan.elapsed_clamped == 1u);
    CHECK(plan.fixed_ticks == 4u);
    CHECK(plan.catchup_clamped == 0u);
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(116));
    CHECK(context.telemetry.discarded_source_ticks ==
          (uint64_t)UINT32_MAX - UINT64_C(100));
    return 1;
}

static int test_pause_keeps_fixed_ui_and_sixty_hz_present(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonContext before;
    MoonFramePlan plan;
    MoonFramePlan transition;
    MoonFramePlan zero_plan;
    uint32_t fixed_count;
    uint32_t present_count;
    uint32_t now;
    unsigned int step;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    CHECK(!moon_runtime_is_paused(&context));
    CHECK(moon_runtime_set_paused(&context, 1, 0u, &transition) == MOON_OK);
    CHECK(transition.fixed_ticks == 0u);
    CHECK(transition.simulation_ticks == 0u);
    CHECK(moon_runtime_is_paused(&context));

    fixed_count = 0u;
    present_count = 0u;
    now = 0u;
    for (step = 0u; step < 100u; ++step) {
        now += 10u;
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        CHECK(plan.simulation_ticks == 0u);
        CHECK(plan.alpha_q16 == 0u);
        fixed_count += plan.fixed_ticks;
        present_count += plan.present;
    }

    CHECK(fixed_count == 35u);
    CHECK(present_count == 60u);
    CHECK(context.telemetry.fixed_ticks == UINT64_C(35));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(0));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(35));
    CHECK(context.telemetry.presented_frames == UINT64_C(60));

    CHECK(moon_runtime_set_paused(&context, 0, now, &transition) == MOON_OK);
    CHECK(transition.fixed_ticks == 0u);
    CHECK(!moon_runtime_is_paused(&context));
    fixed_count = 0u;
    present_count = 0u;
    for (step = 0u; step < 100u; ++step) {
        now += 10u;
        CHECK(moon_runtime_advance(&context, now, &plan) == MOON_OK);
        fixed_count += plan.fixed_ticks;
        present_count += plan.present;
    }

    CHECK(fixed_count == 35u);
    CHECK(present_count == 60u);
    CHECK(context.telemetry.fixed_ticks == UINT64_C(70));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(35));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(35));
    CHECK(context.telemetry.presented_frames == UINT64_C(120));
    CHECK(context.clock.simulation_phase == 0u);
    CHECK(context.clock.presentation_phase == 0u);

    memset(&transition, 0xa5, sizeof(transition));
    memset(&zero_plan, 0, sizeof(zero_plan));
    CHECK(moon_runtime_set_paused(NULL, 1, now, &transition) ==
          MOON_ERR_ARGUMENT);
    CHECK(memcmp(&transition, &zero_plan, sizeof(transition)) == 0);
    before = context;
    CHECK(moon_runtime_set_paused(&context, 1, now, NULL) ==
          MOON_ERR_ARGUMENT);
    CHECK(memcmp(&context, &before, sizeof(context)) == 0);

    context.clock.present_mode = (MoonPresentMode)99;
    before = context;
    memset(&transition, 0xa5, sizeof(transition));
    CHECK(moon_runtime_set_paused(&context, 1, now + 1u, &transition) ==
          MOON_ERR_CONFIG);
    CHECK(memcmp(&transition, &zero_plan, sizeof(transition)) == 0);
    CHECK(memcmp(&context, &before, sizeof(context)) == 0);
    CHECK(!moon_runtime_is_paused(NULL));
    return 1;
}

static int test_pause_freezes_nonaligned_simulation_phase(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    uint32_t frozen_phase;
    uint16_t frozen_alpha;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);

    /* The transition sample is accounted under the old, running state. */
    CHECK(moon_runtime_set_paused(&context, 1, 20u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 0u);
    CHECK(plan.simulation_ticks == 0u);
    CHECK(plan.present == 1u);
    CHECK(moon_runtime_is_paused(&context));
    frozen_phase = context.clock.simulation_phase;
    frozen_alpha = plan.alpha_q16;
    CHECK(frozen_phase == 700u);
    CHECK(frozen_alpha == 45875u);

    CHECK(moon_runtime_advance(&context, 40u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 1u);
    CHECK(plan.simulation_ticks == 0u);
    CHECK(context.clock.simulation_phase == frozen_phase);
    CHECK(plan.alpha_q16 == frozen_alpha);
    CHECK(context.clock.paused_ui_phase == 400u);

    /* The transition sample is still accounted under the paused state. */
    CHECK(moon_runtime_set_paused(&context, 0, 140u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 3u);
    CHECK(plan.simulation_ticks == 0u);
    CHECK(context.clock.simulation_phase == frozen_phase);
    CHECK(plan.alpha_q16 == frozen_alpha);
    CHECK(context.clock.paused_ui_phase == 900u);
    CHECK(!moon_runtime_is_paused(&context));
    CHECK(moon_runtime_advance(&context, 150u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 1u);
    CHECK(plan.simulation_ticks == 1u);
    CHECK(context.clock.simulation_phase == 50u);
    CHECK(context.clock.paused_ui_phase == 50u);
    CHECK(plan.alpha_q16 == 3276u);
    CHECK(context.telemetry.simulation_ticks == UINT64_C(1));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(4));
    return 1;
}

static int test_paused_catchup_accounting(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    CHECK(moon_runtime_set_paused(&context, 1, 0u, &plan) == MOON_OK);
    CHECK(moon_runtime_advance(&context, 300u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.simulation_ticks == 0u);
    CHECK(plan.catchup_clamped == 1u);
    CHECK(plan.elapsed_clamped == 0u);
    CHECK(plan.present == 1u);
    CHECK(plan.alpha_q16 == 0u);
    CHECK(context.clock.simulation_phase == 0u);
    CHECK(context.clock.paused_ui_phase == 500u);
    CHECK(context.telemetry.fixed_ticks == UINT64_C(4));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(0));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(4));
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(6));
    CHECK(context.telemetry.catchup_ticks == UINT64_C(3));
    CHECK(context.telemetry.catchup_clamp_events == UINT64_C(1));
    CHECK(context.telemetry.presented_frames == UINT64_C(1));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(17));
    CHECK(context.telemetry.fixed_ticks ==
          context.telemetry.simulation_ticks +
              context.telemetry.paused_simulation_ticks);
    return 1;
}

static int test_legacy_pause_phase_contract(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_LEGACY_35,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);

    CHECK(moon_runtime_set_paused(&context, 1, 20u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 0u && plan.simulation_ticks == 0u);
    CHECK(plan.present == 0u && plan.alpha_q16 == 0u);
    CHECK(context.clock.simulation_phase == 700u);
    CHECK(context.clock.presentation_phase == 700u);

    CHECK(moon_runtime_advance(&context, 40u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 1u && plan.simulation_ticks == 0u);
    CHECK(plan.present == 1u && plan.alpha_q16 == 0u);
    CHECK(context.clock.simulation_phase == 700u);
    CHECK(context.clock.paused_ui_phase == 400u);
    CHECK(context.clock.presentation_phase == 400u);

    CHECK(moon_runtime_set_paused(&context, 0, 140u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 3u && plan.simulation_ticks == 0u);
    CHECK(plan.present == 1u && plan.alpha_q16 == 0u);
    CHECK(context.clock.simulation_phase == 700u);
    CHECK(context.clock.paused_ui_phase == 900u);
    CHECK(context.clock.presentation_phase == 900u);

    CHECK(moon_runtime_advance(&context, 150u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 1u && plan.simulation_ticks == 1u);
    CHECK(plan.present == 1u && plan.alpha_q16 == 0u);
    CHECK(context.clock.simulation_phase == 50u);
    CHECK(context.clock.presentation_phase == 250u);
    CHECK(context.telemetry.fixed_ticks == UINT64_C(5));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(1));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(4));
    CHECK(context.telemetry.presented_frames == UINT64_C(3));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(2));
    return 1;
}

static int test_extreme_clock_bounds(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;

    CHECK(make_config(&config, UINT32_MAX, UINT32_MAX / 2u,
                      MOON_PRESENT_60, 0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    CHECK(moon_runtime_advance(&context, UINT32_MAX / 2u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.simulation_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.catchup_clamped == 1u);
    CHECK(plan.elapsed_clamped == 0u);
    CHECK(plan.present == 1u);
    CHECK(plan.alpha_q16 == 32767u);
    CHECK(context.clock.simulation_phase == UINT32_C(2147483630));
    CHECK(context.clock.presentation_phase == UINT32_C(4294967265));
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(13));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(28));

    CHECK(moon_runtime_set_paused(&context, 1, UINT32_MAX / 2u, &plan) ==
          MOON_OK);
    CHECK(plan.fixed_ticks == 0u && plan.present == 0u);
    CHECK(moon_runtime_advance(&context, UINT32_MAX - 1u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == MOON_MAX_FIXED_TICKS);
    CHECK(plan.simulation_ticks == 0u);
    CHECK(plan.catchup_clamped == 1u);
    CHECK(plan.alpha_q16 == 32767u);
    CHECK(context.clock.simulation_phase == UINT32_C(2147483630));
    CHECK(context.clock.paused_ui_phase == UINT32_C(4294967260));
    CHECK(context.clock.presentation_phase == UINT32_C(4294967235));
    CHECK(context.telemetry.accepted_source_ticks == UINT64_C(4294967294));
    CHECK(context.telemetry.fixed_ticks == UINT64_C(8));
    CHECK(context.telemetry.simulation_ticks == UINT64_C(4));
    CHECK(context.telemetry.paused_simulation_ticks == UINT64_C(4));
    CHECK(context.telemetry.dropped_fixed_ticks == UINT64_C(26));
    CHECK(context.telemetry.catchup_ticks == UINT64_C(6));
    CHECK(context.telemetry.catchup_clamp_events == UINT64_C(2));
    CHECK(context.telemetry.presented_frames == UINT64_C(2));
    CHECK(context.telemetry.missed_present_intervals == UINT64_C(57));
    return 1;
}

static int test_q16_bounds(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;

    CHECK(make_config(&config, 65536u, 32768u, MOON_PRESENT_60,
                      0u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 10u) == MOON_OK);

    context.clock.simulation_phase = 65535u;
    CHECK(moon_runtime_advance(&context, 10u, &plan) == MOON_OK);
    CHECK(plan.alpha_q16 == UINT16_MAX);

    context.clock.simulation_phase = 0u;
    CHECK(moon_runtime_advance(&context, 10u, &plan) == MOON_OK);
    CHECK(plan.alpha_q16 == 0u);

    context.clock.present_mode = MOON_PRESENT_LEGACY_35;
    context.clock.simulation_phase = 65535u;
    CHECK(moon_runtime_advance(&context, 10u, &plan) == MOON_OK);
    CHECK(plan.alpha_q16 == 0u);
    return 1;
}

static int test_input_edges_and_e0_namespace(void)
{
    MoonInput input;
    const MoonButtonState *normal_up;
    const MoonButtonState *extended_up;
    const MoonButtonState *pause_control;
    const MoonButtonState *pause_num_lock;
    const MoonButtonState *print_screen;
    const MoonButtonState *fake_left_shift;
    const MoonButtonState *fake_right_shift;
    static const uint8_t pause_sequence[] = {
        UINT8_C(0xe1), UINT8_C(0x1d), UINT8_C(0x45),
        UINT8_C(0xe1), UINT8_C(0x9d), UINT8_C(0xc5)
    };
    size_t index;

    CHECK(moon_input_init(NULL, 1u, 3u, 2u) == MOON_ERR_ARGUMENT);
    CHECK(moon_input_init(&input, MOON_ACTION_CAPACITY + 1u, 3u, 2u) ==
          MOON_ERR_CONFIG);
    CHECK(moon_input_init(&input, 3u, 0u, 2u) == MOON_ERR_CONFIG);
    CHECK(moon_input_init(&input, 3u, 3u, 0u) == MOON_ERR_CONFIG);
    CHECK(moon_input_init(&input, 7u, 3u, 2u) == MOON_OK);

    CHECK(MOON_KEY_NORMAL(0x48u) == 0x48u);
    CHECK(MOON_KEY_E0(0x48u) == 0xc8u);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x48u)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 1u, 0u, MOON_KEY_E0(0x48u)) == MOON_OK);
    CHECK(moon_input_bind(&input, 2u, 0u, MOON_KEY_NORMAL(0x1du)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 3u, 0u, MOON_KEY_E0(0x37u)) == MOON_OK);
    CHECK(moon_input_bind(&input, 4u, 0u, MOON_KEY_E0(0x2au)) == MOON_OK);
    CHECK(moon_input_bind(&input, 5u, 0u, MOON_KEY_E0(0x36u)) == MOON_OK);
    CHECK(moon_input_bind(&input, 6u, 0u, MOON_KEY_NORMAL(0x45u)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 7u, 0u, 0u) == MOON_ERR_RANGE);
    CHECK(moon_input_bind(&input, 0u, MOON_BINDINGS_PER_ACTION, 0u) ==
          MOON_ERR_RANGE);
    CHECK(moon_input_bind(&input, 0u, 1u, (MoonKey)MOON_KEY_COUNT) ==
          MOON_ERR_RANGE);
    CHECK(moon_input_bind(NULL, 0u, 0u, 0u) == MOON_ERR_ARGUMENT);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    normal_up = moon_input_action(&input, 0u);
    extended_up = moon_input_action(&input, 1u);
    CHECK(normal_up != NULL && normal_up->held && normal_up->pressed);
    CHECK(extended_up != NULL && !extended_up->held);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && !normal_up->pressed);
    CHECK(extended_up->held && extended_up->pressed);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!normal_up->held && normal_up->released);
    CHECK(extended_up->held && !extended_up->released);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!extended_up->held && extended_up->released);

    for (index = 0u;
         index < sizeof(pause_sequence) / sizeof(pause_sequence[0]);
         ++index) {
        CHECK(moon_input_feed_scancode(&input, pause_sequence[index]) ==
              MOON_OK);
    }
    moon_input_tick(&input);
    pause_control = moon_input_action(&input, 2u);
    pause_num_lock = moon_input_action(&input, 6u);
    CHECK(pause_control != NULL && pause_num_lock != NULL);
    CHECK(!pause_control->held && !pause_control->pressed &&
          !pause_control->released);
    CHECK(!pause_num_lock->held && !pause_num_lock->pressed &&
          !pause_num_lock->released);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x1du)] == 0u);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x45u)] == 0u);

    /* Pause's component breaks must not release independently held keys. */
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1du), 1) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x45u), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(pause_control->held && pause_control->pressed);
    CHECK(pause_num_lock->held && pause_num_lock->pressed);
    for (index = 0u;
         index < sizeof(pause_sequence) / sizeof(pause_sequence[0]);
         ++index) {
        CHECK(moon_input_feed_scancode(&input, pause_sequence[index]) ==
              MOON_OK);
    }
    moon_input_tick(&input);
    CHECK(pause_control->held && !pause_control->pressed &&
          !pause_control->released);
    CHECK(pause_num_lock->held && !pause_num_lock->pressed &&
          !pause_num_lock->released);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1du), 0) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x45u), 0) == MOON_OK);
    moon_input_tick(&input);

    print_screen = moon_input_action(&input, 3u);
    fake_left_shift = moon_input_action(&input, 4u);
    fake_right_shift = moon_input_action(&input, 5u);
    CHECK(print_screen != NULL && fake_left_shift != NULL);
    CHECK(fake_right_shift != NULL);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x2a)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x37)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(print_screen->held && print_screen->pressed);
    CHECK(!fake_left_shift->held && !fake_left_shift->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xb7)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xaa)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!print_screen->held && print_screen->released);
    CHECK(!fake_left_shift->held && !fake_left_shift->released);

    /* Edit-pad wrappers expose only the real E0 key, never fake Shift. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x2a)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(extended_up->held && extended_up->pressed);
    CHECK(!fake_left_shift->held && !fake_left_shift->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xaa)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!extended_up->held && extended_up->released);
    CHECK(!fake_left_shift->held && !fake_left_shift->released);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x36)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xb6)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!fake_right_shift->held && !fake_right_shift->pressed);
    CHECK(!fake_right_shift->released);

    /* Ordinary non-E0 Shift scan codes remain ordinary keys. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x2a)) == MOON_OK);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x2au)] == 1u);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xaa)) == MOON_OK);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x2au)] == 0u);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x36)) == MOON_OK);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x36u)] == 1u);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xb6)) == MOON_OK);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x36u)] == 0u);

    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1du), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(pause_control->held && pause_control->pressed);
    moon_input_clear_raw(&input);
    moon_input_tick(&input);
    CHECK(!pause_control->held && pause_control->released);

    CHECK(moon_input_set_key(&input, (MoonKey)MOON_KEY_COUNT, 1) ==
          MOON_ERR_RANGE);
    CHECK(moon_input_set_key(NULL, 0u, 1) == MOON_ERR_ARGUMENT);
    CHECK(moon_input_feed_scancode(NULL, 0u) == MOON_ERR_ARGUMENT);
    CHECK(moon_input_action(&input, 7u) == NULL);
    CHECK(moon_input_action(NULL, 0u) == NULL);
    moon_input_clear_raw(NULL);
    moon_input_resync(NULL);
    moon_input_tick(NULL);
    return 1;
}

static int test_scancode_resynchronization(void)
{
    static const uint8_t pause_tail[] = {
        UINT8_C(0x1d), UINT8_C(0x45), UINT8_C(0xe1),
        UINT8_C(0x9d), UINT8_C(0xc5)
    };
    MoonInput input;
    const MoonButtonState *normal_up;
    const MoonButtonState *extended_up;
    const MoonButtonState *zero_key;
    const MoonButtonState *highest_key;
    const MoonButtonState *pause_control;
    const MoonButtonState *pause_num_lock;
    size_t mismatch;
    size_t prefix;

    CHECK(moon_input_init(&input, 6u, 3u, 2u) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x48u)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 1u, 0u, MOON_KEY_E0(0x48u)) == MOON_OK);
    CHECK(moon_input_bind(&input, 2u, 0u, MOON_KEY_NORMAL(0x00u)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 3u, 0u, MOON_KEY_NORMAL(0x7fu)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 4u, 0u, MOON_KEY_NORMAL(0x1du)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 5u, 0u, MOON_KEY_NORMAL(0x45u)) ==
          MOON_OK);
    normal_up = moon_input_action(&input, 0u);
    extended_up = moon_input_action(&input, 1u);
    zero_key = moon_input_action(&input, 2u);
    highest_key = moon_input_action(&input, 3u);
    pause_control = moon_input_action(&input, 4u);
    pause_num_lock = moon_input_action(&input, 5u);
    CHECK(normal_up != NULL && extended_up != NULL && zero_key != NULL);
    CHECK(highest_key != NULL && pause_control != NULL);
    CHECK(pause_num_lock != NULL);

    /* Every malformed Pause tail reprocesses its mismatching normal byte. */
    for (mismatch = 0u; mismatch < sizeof(pause_tail); ++mismatch) {
        moon_input_resync(&input);
        moon_input_tick(&input);
        CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
        for (prefix = 0u; prefix < mismatch; ++prefix) {
            CHECK(moon_input_feed_scancode(&input, pause_tail[prefix]) ==
                  MOON_OK);
        }
        CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
        moon_input_tick(&input);
        CHECK(normal_up->held && normal_up->pressed);
        CHECK(!pause_control->held && !pause_control->pressed &&
              !pause_control->released);
        CHECK(!pause_num_lock->held && !pause_num_lock->pressed &&
              !pause_num_lock->released);
        CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
        moon_input_tick(&input);
        CHECK(!normal_up->held && normal_up->released);
    }

    /* An E0 mismatch becomes a real prefix for the following byte. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x45)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(extended_up->held && extended_up->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!extended_up->held && extended_up->released);

    /* A mismatching E1 restarts the candidate and a full tail stays hidden. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    for (prefix = 0u; prefix < sizeof(pause_tail); ++prefix) {
        CHECK(moon_input_feed_scancode(&input, pause_tail[prefix]) ==
              MOON_OK);
    }
    moon_input_tick(&input);
    CHECK(!pause_control->held && !pause_control->pressed &&
          !pause_control->released);
    CHECK(!pause_num_lock->held && !pause_num_lock->pressed &&
          !pause_num_lock->released);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x1du)] == 0u);
    CHECK(input.raw_keys[MOON_KEY_NORMAL(0x45u)] == 0u);

    /* Clear during a partial candidate restores ordinary decoding. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    moon_input_clear_raw(&input);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    /* Error bytes never become keys and reset any pending prefix. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x00)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!zero_key->held && !zero_key->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x7f)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(highest_key->held && highest_key->pressed);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xff)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(highest_key->held && !highest_key->released);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x7fu), 0) == MOON_OK);
    moon_input_tick(&input);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x00)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(!extended_up->held && !zero_key->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xff)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(!extended_up->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x00)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(!pause_control->held && !pause_num_lock->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xff)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(!pause_control->held && !pause_num_lock->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    /* Hard resync clears both E0 and partial E1 decoder state. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    moon_input_resync(&input);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed && !extended_up->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe1)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1d)) == MOON_OK);
    moon_input_resync(&input);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    CHECK(!pause_control->held && !pause_num_lock->held);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);

    /* Hard recovery drops unpublished history but releases published holds. */
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_resync(&input);
    moon_input_tick(&input);
    CHECK(!normal_up->held && !normal_up->pressed && !normal_up->released);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal_up->held && normal_up->pressed);
    input.held_ticks[0] = 2u;
    moon_input_resync(&input);
    moon_input_tick(&input);
    CHECK(!normal_up->held && !normal_up->pressed && normal_up->released);
    CHECK(!normal_up->repeat && input.held_ticks[0] == 0u);
    return 1;
}

static int test_input_repeat_and_binding_union(void)
{
    static const uint8_t expected_repeat[] = {0u, 0u, 0u, 1u, 0u, 1u};
    MoonInput input;
    const MoonButtonState *state;
    size_t tick;

    CHECK(moon_input_init(&input, 1u, 3u, 2u) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 1u, MOON_KEY_NORMAL(0x30u)) ==
          MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);

    state = moon_input_action(&input, 0u);
    CHECK(state != NULL);
    for (tick = 0u;
         tick < sizeof(expected_repeat) / sizeof(expected_repeat[0]);
         ++tick) {
        moon_input_tick(&input);
        CHECK(state->held == 1u);
        CHECK(state->pressed == (tick == 0u ? 1u : 0u));
        CHECK(state->released == 0u);
        CHECK(state->repeat == expected_repeat[tick]);
    }

    /* A second held binding prevents a false release when the first lifts. */
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x30u), 1) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 0) == MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && !state->pressed && !state->released);

    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x30u), 0) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!state->held && state->released && !state->repeat);

    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && state->pressed && !state->repeat);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_UNBOUND) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!state->held && state->released);

    /* Rebinding to an already-held raw key produces a logical press edge. */
    CHECK(moon_input_bind(&input, 0u, 1u, MOON_KEY_UNBOUND) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x30u), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!state->held && !state->pressed);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x30u)) ==
          MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && state->pressed && !state->released);

    moon_input_resync(&input);
    moon_input_tick(&input);
    CHECK(!state->held && state->released);

    /* An unconsumed physical make followed by unbind preserves both edges. */
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_UNBOUND) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!state->held && state->pressed && state->released);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 0) == MOON_OK);

    /* A release plus rebind to an already-held key reports both edges. */
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && state->pressed);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x30u), 1) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 0) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x30u)) ==
          MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && state->pressed && state->released);
    CHECK(!state->repeat && input.held_ticks[0] == 0u);

    /* Replacing one held key with another never drops the OR union. */
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && !state->pressed && !state->released);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x30u)) ==
          MOON_OK);
    moon_input_tick(&input);
    CHECK(state->held && !state->pressed && !state->released);
    return 1;
}

static int test_input_latched_short_taps(void)
{
    MoonInput input;
    const MoonButtonState *normal;
    const MoonButtonState *extended;

    CHECK(moon_input_init(&input, 2u, 3u, 2u) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    CHECK(moon_input_bind(&input, 1u, 0u, MOON_KEY_E0(0x48u)) == MOON_OK);
    normal = moon_input_action(&input, 0u);
    extended = moon_input_action(&input, 1u);
    CHECK(normal != NULL && extended != NULL);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x1e)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x9e)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!normal->held && normal->pressed && normal->released);
    CHECK(!normal->repeat);
    CHECK(!extended->held && !extended->pressed && !extended->released);

    moon_input_tick(&input);
    CHECK(!normal->held && !normal->pressed && !normal->released);

    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0x48)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xe0)) == MOON_OK);
    CHECK(moon_input_feed_scancode(&input, UINT8_C(0xc8)) == MOON_OK);
    moon_input_tick(&input);
    CHECK(!extended->held && extended->pressed && extended->released);
    CHECK(!normal->held && !normal->pressed && !normal->released);

    /* A release/re-press cycle is visible and restarts repeat timing. */
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal->held && normal->pressed && !normal->released);
    input.held_ticks[0] = 2u;
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 0) == MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    moon_input_tick(&input);
    CHECK(normal->held && normal->pressed && normal->released);
    CHECK(!normal->repeat && input.held_ticks[0] == 0u);
    return 1;
}

static int test_repeat_counter_normalizes_at_saturation(void)
{
    MoonInput input;
    const MoonButtonState *state;

    CHECK(moon_input_init(&input, 1u, 3u, 2u) == MOON_OK);
    CHECK(moon_input_bind(&input, 0u, 0u, MOON_KEY_NORMAL(0x1eu)) ==
          MOON_OK);
    CHECK(moon_input_set_key(&input, MOON_KEY_NORMAL(0x1eu), 1) == MOON_OK);
    moon_input_tick(&input);
    state = moon_input_action(&input, 0u);
    CHECK(state != NULL && state->held);

    input.held_ticks[0] = UINT32_MAX;
    moon_input_tick(&input);
    CHECK(!state->repeat);
    CHECK(input.held_ticks[0] == 4u);
    moon_input_tick(&input);
    CHECK(state->repeat);
    CHECK(input.held_ticks[0] == 3u);
    moon_input_tick(&input);
    CHECK(!state->repeat);
    CHECK(input.held_ticks[0] == 4u);
    return 1;
}

static int test_input_at_catchup_boundaries(void)
{
    MoonRuntimeConfig config;
    MoonContext context;
    MoonFramePlan plan;
    const MoonButtonState *state;
    uint32_t pressed_count;
    uint32_t repeat_count;
    uint32_t tick;

    CHECK(make_config(&config, 1000u, 500u, MOON_PRESENT_60,
                      1u, 3u, 2u));
    CHECK(moon_runtime_init(&context, &config, 0u) == MOON_OK);
    CHECK(moon_input_bind(&context.input, 0u, 0u,
                          MOON_KEY_NORMAL(0x1eu)) == MOON_OK);
    CHECK(moon_input_set_key(&context.input, MOON_KEY_NORMAL(0x1eu), 1) ==
          MOON_OK);
    CHECK(moon_runtime_advance(&context, 300u, &plan) == MOON_OK);
    CHECK(plan.fixed_ticks == 4u);

    pressed_count = 0u;
    repeat_count = 0u;
    state = moon_input_action(&context.input, 0u);
    CHECK(state != NULL);
    for (tick = 0u; tick < plan.fixed_ticks; ++tick) {
        moon_input_tick(&context.input);
        pressed_count += state->pressed;
        repeat_count += state->repeat;
    }

    CHECK(pressed_count == 1u);
    CHECK(repeat_count == 1u);
    return 1;
}

typedef int (*TestFunction)(void);

typedef struct TestCase {
    const char *name;
    TestFunction function;
} TestCase;

int main(void)
{
    static const TestCase tests[] = {
        {"configuration", test_configuration},
        {"golden rational sequence", test_golden_rational_sequence},
        {"minimum clock cadence", test_minimum_clock_cadence},
        {"legacy 35 sequence", test_legacy_35_sequence},
        {"long duration and wrap", test_long_duration_exactness_and_wrap},
        {"catchup and fraction", test_catchup_drop_retains_fraction},
        {"elapsed clamp and wrap", test_elapsed_clamp_and_modular_wrap},
        {"pause presentation", test_pause_keeps_fixed_ui_and_sixty_hz_present},
        {"pause frozen phase", test_pause_freezes_nonaligned_simulation_phase},
        {"paused catchup", test_paused_catchup_accounting},
        {"legacy pause phase", test_legacy_pause_phase_contract},
        {"extreme clock bounds", test_extreme_clock_bounds},
        {"Q16 bounds", test_q16_bounds},
        {"input edges and E0", test_input_edges_and_e0_namespace},
        {"scancode resynchronization", test_scancode_resynchronization},
        {"input repeat", test_input_repeat_and_binding_union},
        {"input short taps", test_input_latched_short_taps},
        {"repeat saturation", test_repeat_counter_normalizes_at_saturation},
        {"input catchup", test_input_at_catchup_boundaries}
    };
    size_t index;

    for (index = 0u; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].function()) {
            printf("runtime core tests: FAIL (%s)\n", tests[index].name);
            return 1;
        }
    }

    printf("runtime core tests: PASS (%lu tests)\n",
           (unsigned long)(sizeof(tests) / sizeof(tests[0])));
    return 0;
}
