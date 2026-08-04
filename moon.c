#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moon/dos_runtime.h"
#include "moon/runtime.h"

enum {
    MOON_ACTION_ZEUS = 0,
    MOON_ACTION_MDPED,
    MOON_ACTION_TMUSE,
    MOON_ACTION_EXIT,
    MOON_ACTION_ESCAPE,
    MOON_ACTION_UP,
    MOON_ACTION_DOWN,
    MOON_ACTION_SELECT,
    MOON_ACTION_COUNT
};

typedef enum MoonLaunch {
    MOON_LAUNCH_NONE = 0,
    MOON_LAUNCH_ZEUS,
    MOON_LAUNCH_MDPED,
    MOON_LAUNCH_TMUSE,
    MOON_LAUNCH_EXIT
} MoonLaunch;

typedef enum MoonAppResult {
    MOON_APP_OK = 0,
    MOON_APP_DOS_INIT,
    MOON_APP_CORE_CONFIG,
    MOON_APP_CORE_INIT,
    MOON_APP_INPUT_BIND,
    MOON_APP_INPUT_DRAIN,
    MOON_APP_CORE_ADVANCE,
    MOON_APP_PRESENT,
    MOON_APP_TELEMETRY,
    MOON_APP_SHUTDOWN,
    MOON_APP_SMOKE_COUNTERS,
    MOON_APP_EVIDENCE
} MoonAppResult;

typedef struct MoonVisualState {
    int previous_x;
    int current_x;
    int direction;
    uint32_t simulation_ticks;
} MoonVisualState;

typedef struct MoonMenuState {
    int selection;
    uint32_t pressed_events;
    uint32_t released_events;
    uint32_t repeat_events;
    uint8_t navigation_held;
} MoonMenuState;

static MoonContext moon_context;
static MoonDosRuntime moon_dos_runtime;
static uint8_t moon_framebuffer[MOON_DOS_FRAMEBUFFER_BYTES];
static MoonDosResult moon_last_dos_result = MOON_DOS_OK;
static MoonResult moon_last_core_result = MOON_OK;

static const char *moon_core_result_name(MoonResult result)
{
    switch (result) {
        case MOON_OK:
            return "MOON_OK";
        case MOON_ERR_ARGUMENT:
            return "MOON_ERR_ARGUMENT";
        case MOON_ERR_CONFIG:
            return "MOON_ERR_CONFIG";
        case MOON_ERR_RANGE:
            return "MOON_ERR_RANGE";
        default:
            return "MOON_ERR_UNKNOWN";
    }
}

static const char *moon_app_result_name(MoonAppResult result)
{
    switch (result) {
        case MOON_APP_OK:
            return "ok";
        case MOON_APP_DOS_INIT:
            return "DOS runtime initialization";
        case MOON_APP_CORE_CONFIG:
            return "runtime configuration";
        case MOON_APP_CORE_INIT:
            return "runtime initialization";
        case MOON_APP_INPUT_BIND:
            return "input binding";
        case MOON_APP_INPUT_DRAIN:
            return "input drain";
        case MOON_APP_CORE_ADVANCE:
            return "runtime advance";
        case MOON_APP_PRESENT:
            return "frame presentation";
        case MOON_APP_TELEMETRY:
            return "DOS telemetry capture";
        case MOON_APP_SHUTDOWN:
            return "DOS runtime shutdown";
        case MOON_APP_SMOKE_COUNTERS:
            return "runtime smoke counters";
        case MOON_APP_EVIDENCE:
            return "runtime smoke evidence";
        default:
            return "unknown launcher operation";
    }
}

static int moon_ascii_equal(const char *left, const char *right)
{
    unsigned char left_char;
    unsigned char right_char;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        left_char = (unsigned char)*left;
        right_char = (unsigned char)*right;
        if (left_char >= (unsigned char)'a' &&
            left_char <= (unsigned char)'z') {
            left_char = (unsigned char)(left_char - (unsigned char)'a' +
                                        (unsigned char)'A');
        }
        if (right_char >= (unsigned char)'a' &&
            right_char <= (unsigned char)'z') {
            right_char = (unsigned char)(right_char - (unsigned char)'a' +
                                         (unsigned char)'A');
        }
        if (left_char != right_char) {
            return 0;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int moon_parse_arguments(int argc,
                                char **argv,
                                MoonPresentMode *present_mode,
                                int *runtime_smoke)
{
    int argument;
    int legacy_seen = 0;
    int smoke_seen = 0;

    if (argv == NULL || present_mode == NULL || runtime_smoke == NULL) {
        return 0;
    }

    *present_mode = MOON_PRESENT_60;
    *runtime_smoke = 0;
    for (argument = 1; argument < argc; ++argument) {
        if (moon_ascii_equal(argv[argument], "/LEGACY35") && !legacy_seen) {
            *present_mode = MOON_PRESENT_LEGACY_35;
            legacy_seen = 1;
        } else if (moon_ascii_equal(argv[argument], "/RUNTIME-SMOKE") &&
                   !smoke_seen) {
            *runtime_smoke = 1;
            smoke_seen = 1;
        } else {
            return 0;
        }
    }

    return 1;
}

static void moon_put_pixel(int x, int y, uint8_t color)
{
    size_t offset;

    if (x < 0 || x >= (int)MOON_DOS_FRAMEBUFFER_WIDTH ||
        y < 0 || y >= (int)MOON_DOS_FRAMEBUFFER_HEIGHT) {
        return;
    }
    offset = (size_t)y * (size_t)MOON_DOS_FRAMEBUFFER_WIDTH + (size_t)x;
    moon_framebuffer[offset] = color;
}

static void moon_draw_rectangle(int x,
                                int y,
                                int width,
                                int height,
                                uint8_t color)
{
    int row;
    int column;

    for (row = 0; row < height; ++row) {
        for (column = 0; column < width; ++column) {
            moon_put_pixel(x + column, y + row, color);
        }
    }
}

/* Each returned string is a row-major 3x5 bitmap. */
static const char *moon_glyph(char character)
{
    switch (character) {
        case '0': return "111101101101111";
        case '1': return "010110010010111";
        case '2': return "111001111100111";
        case '3': return "111001111001111";
        case '4': return "101101111001001";
        case '5': return "111100111001111";
        case '6': return "111100111101111";
        case '7': return "111001010010010";
        case '8': return "111101111101111";
        case '9': return "111101111001111";
        case 'A': return "010101111101101";
        case 'B': return "110101110101110";
        case 'C': return "111100100100111";
        case 'D': return "110101101101110";
        case 'E': return "111100110100111";
        case 'F': return "111100110100100";
        case 'G': return "111100101101111";
        case 'H': return "101101111101101";
        case 'I': return "111010010010111";
        case 'J': return "001001001101111";
        case 'K': return "101101110101101";
        case 'L': return "100100100100111";
        case 'M': return "101111111101101";
        case 'N': return "101111111111101";
        case 'O': return "111101101101111";
        case 'P': return "111101111100100";
        case 'Q': return "111101101111001";
        case 'R': return "110101110101101";
        case 'S': return "111100111001111";
        case 'T': return "111010010010010";
        case 'U': return "101101101101111";
        case 'V': return "101101101101010";
        case 'W': return "101101111111101";
        case 'X': return "101101010101101";
        case 'Y': return "101101010010010";
        case 'Z': return "111001010100111";
        case '-': return "000000111000000";
        case '.': return "000000000000010";
        case '/': return "001001010100100";
        case ' ': return "000000000000000";
        default:  return "111001010000010";
    }
}

static void moon_draw_character(int x,
                                int y,
                                char character,
                                int scale,
                                uint8_t color)
{
    const char *bitmap = moon_glyph(character);
    int row;
    int column;

    for (row = 0; row < 5; ++row) {
        for (column = 0; column < 3; ++column) {
            if (bitmap[row * 3 + column] == '1') {
                moon_draw_rectangle(x + column * scale,
                                    y + row * scale,
                                    scale,
                                    scale,
                                    color);
            }
        }
    }
}

static void moon_draw_text(int x,
                           int y,
                           const char *text,
                           int scale,
                           uint8_t color)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        moon_draw_character(x, y, *text, scale, color);
        x += 4 * scale;
        ++text;
    }
}

static void moon_visual_init(MoonVisualState *visual)
{
    if (visual == NULL) {
        return;
    }
    visual->previous_x = 24;
    visual->current_x = 24;
    visual->direction = 1;
    visual->simulation_ticks = 0u;
}

static void moon_visual_tick(MoonVisualState *visual)
{
    if (visual == NULL) {
        return;
    }

    visual->previous_x = visual->current_x;
    visual->current_x += visual->direction * 5;
    if (visual->current_x >= 284) {
        visual->current_x = 284;
        visual->direction = -1;
    } else if (visual->current_x <= 24) {
        visual->current_x = 24;
        visual->direction = 1;
    }
    ++visual->simulation_ticks;
}

static int moon_interpolated_x(const MoonVisualState *visual,
                               uint16_t alpha_q16)
{
    int delta;
    int32_t scaled_delta;

    if (visual == NULL) {
        return 0;
    }
    delta = visual->current_x - visual->previous_x;
    scaled_delta = (int32_t)delta * (int32_t)alpha_q16;
    return visual->previous_x + (int)(scaled_delta / INT32_C(65536));
}

static void moon_render(const MoonVisualState *visual,
                        const MoonMenuState *menu,
                        uint16_t alpha_q16,
                        MoonPresentMode present_mode)
{
    char input_status[80];
    char runtime_status[80];
    uint8_t runtime_status_color;
    int star;
    int star_x;
    int star_y;
    int moving_x;

    if (present_mode == MOON_PRESENT_LEGACY_35 && visual != NULL) {
        moving_x = visual->current_x;
    } else {
        moving_x = moon_interpolated_x(visual, alpha_q16);
    }

    memset(moon_framebuffer, 0, sizeof(moon_framebuffer));

    runtime_status_color =
        moon_context.telemetry.catchup_clamp_events != 0u ||
                moon_context.telemetry.elapsed_clamp_events != 0u ||
                moon_context.telemetry.dropped_fixed_ticks != 0u
            ? 12u
            : 10u;
    (void)snprintf(
        runtime_status,
        sizeof(runtime_status),
        "RT C%lu E%lu D%lu",
        (unsigned long)moon_context.telemetry.catchup_clamp_events,
        (unsigned long)moon_context.telemetry.elapsed_clamp_events,
        (unsigned long)moon_context.telemetry.dropped_fixed_ticks);

    for (star = 0; star < 48; ++star) {
        star_x = (star * 67 - moving_x * (1 + star % 3)) %
                 (int)MOON_DOS_FRAMEBUFFER_WIDTH;
        if (star_x < 0) {
            star_x += (int)MOON_DOS_FRAMEBUFFER_WIDTH;
        }
        star_y = 8 + (star * 43) % 184;
        moon_put_pixel(star_x, star_y, (uint8_t)(7 + star % 9));
        if (star % 11 == 0) {
            moon_put_pixel(star_x + 1, star_y, 15u);
        }
    }

    moon_draw_rectangle(73, 16, 174, 25, 1u);
    moon_draw_rectangle(76, 19, 168, 19, 0u);
    moon_draw_text(112, 24, "MOON ENG", 3, 15u);

    moon_draw_text(104, 62, "1 ZEUS", 2, 14u);
    moon_draw_text(104, 80, "2 MDPED", 2, 11u);
    moon_draw_text(104, 98, "3 TMUSE", 2, 13u);
    moon_draw_text(104, 116, "4 EXIT", 2, 12u);
    if (menu != NULL) {
        moon_draw_rectangle(92, 64 + menu->selection * 18, 6, 6, 15u);
    }
    moon_draw_text(104, 136, "ARROWS ENTER", 1, 8u);
    moon_draw_text(104, 146, "ESC EXIT", 1, 8u);
    if (present_mode == MOON_PRESENT_LEGACY_35) {
        moon_draw_text(132, 157, "35 FPS", 1, 7u);
    } else {
        moon_draw_text(132, 157, "60 FPS", 1, 7u);
    }

    /* This marker moves at the interpolated position between 35 Hz states. */
    moon_draw_rectangle(moving_x, 174, 12, 5, 9u);
    moon_draw_rectangle(moving_x + 3, 171, 6, 11, 9u);
    moon_put_pixel(moving_x + 9, 176, 15u);

    if (menu != NULL) {
        (void)snprintf(input_status,
                       sizeof(input_status),
                       "E0 P%lu R%lu T%lu H%u",
                       (unsigned long)menu->pressed_events,
                       (unsigned long)menu->released_events,
                       (unsigned long)menu->repeat_events,
                       (unsigned int)menu->navigation_held);
        moon_draw_text(8, 188, input_status, 1, 7u);
    }
    moon_draw_text(8, 5, runtime_status, 1, runtime_status_color);
}

static MoonLaunch moon_launch_for_selection(int selection)
{
    switch (selection) {
        case 0: return MOON_LAUNCH_ZEUS;
        case 1: return MOON_LAUNCH_MDPED;
        case 2: return MOON_LAUNCH_TMUSE;
        case 3: return MOON_LAUNCH_EXIT;
        default: return MOON_LAUNCH_NONE;
    }
}

static int moon_button_triggered(const MoonButtonState *button)
{
    return button != NULL &&
           (button->pressed != 0u || button->repeat != 0u);
}

static void moon_menu_observe_navigation(MoonMenuState *menu,
                                         const MoonButtonState *button)
{
    if (menu == NULL || button == NULL) {
        return;
    }
    if (button->pressed != 0u) {
        ++menu->pressed_events;
    }
    if (button->released != 0u) {
        ++menu->released_events;
    }
    if (button->repeat != 0u) {
        ++menu->repeat_events;
    }
}

static MoonLaunch moon_menu_tick(MoonMenuState *menu)
{
    const MoonButtonState *button;
    const MoonButtonState *up;
    const MoonButtonState *down;
    int up_triggered;
    int down_triggered;

    if (menu == NULL) {
        return MOON_LAUNCH_NONE;
    }

    button = moon_input_action(&moon_context.input, MOON_ACTION_ESCAPE);
    if (button != NULL && button->pressed) {
        return MOON_LAUNCH_EXIT;
    }
    button = moon_input_action(&moon_context.input, MOON_ACTION_EXIT);
    if (button != NULL && button->pressed) {
        return MOON_LAUNCH_EXIT;
    }
    button = moon_input_action(&moon_context.input, MOON_ACTION_ZEUS);
    if (button != NULL && button->pressed) {
        return MOON_LAUNCH_ZEUS;
    }
    button = moon_input_action(&moon_context.input, MOON_ACTION_MDPED);
    if (button != NULL && button->pressed) {
        return MOON_LAUNCH_MDPED;
    }
    button = moon_input_action(&moon_context.input, MOON_ACTION_TMUSE);
    if (button != NULL && button->pressed) {
        return MOON_LAUNCH_TMUSE;
    }

    up = moon_input_action(&moon_context.input, MOON_ACTION_UP);
    down = moon_input_action(&moon_context.input, MOON_ACTION_DOWN);
    moon_menu_observe_navigation(menu, up);
    moon_menu_observe_navigation(menu, down);
    menu->navigation_held =
        (uint8_t)((up != NULL && up->held != 0u) ||
                  (down != NULL && down->held != 0u));
    up_triggered = moon_button_triggered(up);
    down_triggered = moon_button_triggered(down);
    if (up_triggered != 0 && down_triggered == 0) {
        menu->selection = (menu->selection + 3) % 4;
    } else if (down_triggered != 0 && up_triggered == 0) {
        menu->selection = (menu->selection + 1) % 4;
    }

    button = moon_input_action(&moon_context.input, MOON_ACTION_SELECT);
    if (button != NULL && button->pressed != 0u) {
        return moon_launch_for_selection(menu->selection);
    }

    return MOON_LAUNCH_NONE;
}

static MoonAppResult moon_shutdown_after_failure(MoonAppResult failure)
{
    MoonDosResult shutdown_result;

    shutdown_result = moon_dos_runtime_shutdown(&moon_dos_runtime);
    if (shutdown_result != MOON_DOS_OK) {
        moon_last_dos_result = shutdown_result;
        return MOON_APP_SHUTDOWN;
    }
    return failure;
}

static MoonAppResult moon_session_start(MoonPresentMode present_mode)
{
    MoonRuntimeConfig config;
    MoonDosResult dos_result;
    MoonResult core_result;
    uint32_t clock_frequency;
    uint32_t initial_tick;

    memset(&moon_context, 0, sizeof(moon_context));
    memset(&moon_dos_runtime, 0, sizeof(moon_dos_runtime));
    memset(moon_framebuffer, 0, sizeof(moon_framebuffer));

    dos_result = moon_dos_runtime_init(&moon_dos_runtime,
                                       &moon_context.input);
    if (dos_result != MOON_DOS_OK) {
        moon_last_dos_result = dos_result;
        return moon_shutdown_after_failure(MOON_APP_DOS_INIT);
    }

    clock_frequency = moon_dos_clock_frequency();
    core_result = moon_runtime_config_defaults(&config, clock_frequency);
    if (core_result != MOON_OK) {
        moon_last_core_result = core_result;
        return moon_shutdown_after_failure(MOON_APP_CORE_CONFIG);
    }
    config.present_mode = present_mode;
    config.action_count = (uint16_t)MOON_ACTION_COUNT;

    initial_tick = moon_dos_clock_now();
    core_result = moon_runtime_init(&moon_context, &config, initial_tick);
    if (core_result != MOON_OK) {
        moon_last_core_result = core_result;
        return moon_shutdown_after_failure(MOON_APP_CORE_INIT);
    }

    core_result = moon_input_bind(&moon_context.input,
                                  MOON_ACTION_ZEUS,
                                  0u,
                                  MOON_KEY_NORMAL(0x02u));
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_MDPED,
                                      0u,
                                      MOON_KEY_NORMAL(0x03u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_TMUSE,
                                      0u,
                                      MOON_KEY_NORMAL(0x04u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_EXIT,
                                      0u,
                                      MOON_KEY_NORMAL(0x05u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_ESCAPE,
                                      0u,
                                      MOON_KEY_NORMAL(0x01u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_UP,
                                      0u,
                                      MOON_KEY_E0(0x48u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_DOWN,
                                      0u,
                                      MOON_KEY_E0(0x50u));
    }
    if (core_result == MOON_OK) {
        core_result = moon_input_bind(&moon_context.input,
                                      MOON_ACTION_SELECT,
                                      0u,
                                      MOON_KEY_NORMAL(0x1cu));
    }
    if (core_result != MOON_OK) {
        moon_last_core_result = core_result;
        return moon_shutdown_after_failure(MOON_APP_INPUT_BIND);
    }

    return MOON_APP_OK;
}

static MoonAppResult moon_session_run(MoonPresentMode present_mode,
                                      int runtime_smoke,
                                      MoonLaunch *launch,
                                      MoonTelemetry *core_telemetry,
                                      MoonDosTelemetry *dos_telemetry)
{
    MoonMenuState menu;
    MoonVisualState visual;
    MoonFramePlan plan;
    MoonDosResult dos_result;
    MoonResult core_result;
    MoonLaunch pressed = MOON_LAUNCH_NONE;
    uint32_t fixed_tick;
    uint32_t now;
    uint32_t smoke_start;
    uint32_t smoke_duration;
    uint32_t clock_frequency;

    if (launch == NULL) {
        return MOON_APP_CORE_ADVANCE;
    }
    *launch = MOON_LAUNCH_NONE;
    memset(&menu, 0, sizeof(menu));
    moon_visual_init(&visual);

    clock_frequency = moon_dos_clock_frequency();
    smoke_start = moon_dos_clock_now();
    smoke_duration = clock_frequency / 4u;
    if (smoke_duration == 0u) {
        smoke_duration = 1u;
    }

    for (;;) {
        dos_result = moon_dos_runtime_drain_input(&moon_dos_runtime, NULL);
        if (dos_result != MOON_DOS_OK) {
            moon_last_dos_result = dos_result;
            return MOON_APP_INPUT_DRAIN;
        }

        now = moon_dos_clock_now();
        core_result = moon_runtime_advance(&moon_context, now, &plan);
        if (core_result != MOON_OK) {
            moon_last_core_result = core_result;
            return MOON_APP_CORE_ADVANCE;
        }

        for (fixed_tick = 0u; fixed_tick < plan.fixed_ticks; ++fixed_tick) {
            moon_input_tick(&moon_context.input);
            if (fixed_tick < plan.simulation_ticks) {
                moon_visual_tick(&visual);
            }
            if (!runtime_smoke && pressed == MOON_LAUNCH_NONE) {
                pressed = moon_menu_tick(&menu);
            }
        }

        if (plan.present) {
            moon_render(&visual, &menu, plan.alpha_q16, present_mode);
            dos_result = moon_dos_runtime_present(&moon_dos_runtime,
                                                  moon_framebuffer,
                                                  sizeof(moon_framebuffer));
            if (dos_result != MOON_DOS_OK) {
                moon_last_dos_result = dos_result;
                return MOON_APP_PRESENT;
            }
        }

        if (pressed != MOON_LAUNCH_NONE) {
            *launch = pressed;
            return MOON_APP_OK;
        }

        if (runtime_smoke && (uint32_t)(now - smoke_start) >= smoke_duration) {
            if (core_telemetry != NULL) {
                *core_telemetry = moon_context.telemetry;
            }
            if (dos_telemetry == NULL) {
                return MOON_APP_TELEMETRY;
            }
            dos_result = moon_dos_runtime_get_telemetry(&moon_dos_runtime,
                                                        dos_telemetry);
            if (dos_result != MOON_DOS_OK) {
                moon_last_dos_result = dos_result;
                return MOON_APP_TELEMETRY;
            }
            return MOON_APP_OK;
        }
    }
}

static MoonAppResult moon_session_shutdown(void)
{
    MoonDosResult dos_result;

    dos_result = moon_dos_runtime_shutdown(&moon_dos_runtime);
    if (dos_result != MOON_DOS_OK) {
        moon_last_dos_result = dos_result;
        return MOON_APP_SHUTDOWN;
    }
    return MOON_APP_OK;
}

static int moon_prepare_smoke_evidence(void)
{
    FILE *evidence = fopen("MOONRT.OUT", "w");

    if (evidence == NULL) {
        return 0;
    }
    return fclose(evidence) == 0;
}

static int moon_write_smoke_evidence(const MoonTelemetry *core_telemetry,
                                     const MoonDosTelemetry *dos_telemetry)
{
    FILE *evidence;
    int write_result;
    int close_result;

    if (core_telemetry == NULL || dos_telemetry == NULL) {
        return 0;
    }

    evidence = fopen("MOONRT.OUT", "w");
    if (evidence == NULL) {
        return 0;
    }
    write_result = fprintf(
        evidence,
        "MOON_RUNTIME_SMOKE\tPASS\tADVANCES=%lu\tFIXED=%lu\t"
        "SIMULATION=%lu\tPRESENTED=%lu\tDOS_PRESENTED=%lu\tDRAINED=%lu\n",
        (unsigned long)core_telemetry->advances,
        (unsigned long)core_telemetry->fixed_ticks,
        (unsigned long)core_telemetry->simulation_ticks,
        (unsigned long)core_telemetry->presented_frames,
        (unsigned long)dos_telemetry->frames_presented,
        (unsigned long)dos_telemetry->scan_codes_drained);
    close_result = fclose(evidence);
    return write_result >= 0 && close_result == 0;
}

static int moon_smoke_counters_pass(const MoonTelemetry *core_telemetry,
                                    const MoonDosTelemetry *dos_telemetry)
{
    if (core_telemetry == NULL || dos_telemetry == NULL) {
        return 0;
    }
    return core_telemetry->advances != 0u &&
           core_telemetry->fixed_ticks != 0u &&
           core_telemetry->simulation_ticks != 0u &&
           core_telemetry->presented_frames != 0u &&
           dos_telemetry->frames_presented != 0u &&
           core_telemetry->fixed_ticks ==
               core_telemetry->simulation_ticks &&
           core_telemetry->presented_frames ==
               (uint64_t)dos_telemetry->frames_presented;
}

static const char *moon_child_command(MoonLaunch launch)
{
    switch (launch) {
        case MOON_LAUNCH_ZEUS:
            return "ZEUS.EXE";
        case MOON_LAUNCH_MDPED:
            return "MDPED.EXE";
        case MOON_LAUNCH_TMUSE:
            return "TMUSEGUI.EXE";
        default:
            return NULL;
    }
}

static void moon_report_error(MoonAppResult result)
{
    fprintf(stderr, "MOON: %s failed", moon_app_result_name(result));
    if (result == MOON_APP_DOS_INIT || result == MOON_APP_INPUT_DRAIN ||
        result == MOON_APP_PRESENT || result == MOON_APP_TELEMETRY ||
        result == MOON_APP_SHUTDOWN) {
        fprintf(stderr, ": %s", moon_dos_result_name(moon_last_dos_result));
    } else if (result == MOON_APP_CORE_CONFIG ||
               result == MOON_APP_CORE_INIT ||
               result == MOON_APP_INPUT_BIND ||
               result == MOON_APP_CORE_ADVANCE) {
        fprintf(stderr, ": %s", moon_core_result_name(moon_last_core_result));
    }
    fputc('\n', stderr);
}

int main(int argc, char **argv)
{
    MoonPresentMode present_mode;
    MoonTelemetry core_telemetry;
    MoonDosTelemetry dos_telemetry;
    MoonLaunch launch;
    MoonAppResult app_result;
    const char *child_command;
    int child_result;
    int runtime_smoke;

    if (!moon_parse_arguments(argc, argv, &present_mode, &runtime_smoke)) {
        fprintf(stderr, "usage: MOON.EXE [/LEGACY35] [/RUNTIME-SMOKE]\n");
        return 2;
    }

    if (runtime_smoke && !moon_prepare_smoke_evidence()) {
        moon_report_error(MOON_APP_EVIDENCE);
        return 1;
    }

    for (;;) {
        memset(&core_telemetry, 0, sizeof(core_telemetry));
        memset(&dos_telemetry, 0, sizeof(dos_telemetry));

        app_result = moon_session_start(present_mode);
        if (app_result != MOON_APP_OK) {
            moon_report_error(app_result);
            return 1;
        }

        app_result = moon_session_run(present_mode,
                                      runtime_smoke,
                                      &launch,
                                      &core_telemetry,
                                      &dos_telemetry);
        {
            MoonAppResult shutdown_result = moon_session_shutdown();
            if (shutdown_result != MOON_APP_OK) {
                moon_report_error(shutdown_result);
                return 1;
            }
        }
        if (app_result != MOON_APP_OK) {
            moon_report_error(app_result);
            return 1;
        }

        if (runtime_smoke) {
            if (!moon_smoke_counters_pass(&core_telemetry, &dos_telemetry)) {
                moon_report_error(MOON_APP_SMOKE_COUNTERS);
                return 1;
            }
            if (!moon_write_smoke_evidence(&core_telemetry, &dos_telemetry)) {
                moon_report_error(MOON_APP_EVIDENCE);
                return 1;
            }
            return 0;
        }

        if (launch == MOON_LAUNCH_EXIT) {
            puts("SEE YOU, SPACE COWBOY...");
            return 0;
        }

        child_command = moon_child_command(launch);
        if (child_command == NULL) {
            fprintf(stderr, "MOON: invalid launcher selection\n");
            return 1;
        }
        child_result = system(child_command);
        if (child_result == -1) {
            fprintf(stderr, "MOON: could not start %s\n", child_command);
            return 1;
        }
        if (child_result != 0) {
            fprintf(stderr,
                    "MOON: %s returned status %d\n",
                    child_command,
                    child_result);
        }
        /* The next loop iteration creates a fresh IRQ/clock/video session. */
    }
}
