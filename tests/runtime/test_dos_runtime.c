#include "moon/dos_runtime.h"

#include <go32.h>
#include <stdio.h>
#include <string.h>
#include <sys/movedata.h>
#include <time.h>

#define TEST_VGA_MEMORY 0x000a0000ul

static uint8_t test_framebuffer[MOON_DOS_FRAMEBUFFER_BYTES];
static uint8_t test_readback[MOON_DOS_FRAMEBUFFER_BYTES];
static unsigned int test_failures;

static void test_check(int condition, const char *message)
{
    if (!condition) {
        ++test_failures;
        printf("runtime DOS adapter test failure: %s\n", message);
    }
}

static int test_query_video_mode(uint8_t *mode)
{
    __dpmi_regs registers;

    if (mode == NULL) {
        return -1;
    }
    memset(&registers, 0, sizeof(registers));
    registers.h.ah = 0x0fu;
    if (__dpmi_int(0x10, &registers) != 0) {
        return -1;
    }
    *mode = registers.h.al;
    return 0;
}

static int test_vectors_equal(const _go32_dpmi_seginfo *left,
                              const _go32_dpmi_seginfo *right)
{
    return left->pm_selector == right->pm_selector &&
           left->pm_offset == right->pm_offset;
}

static int test_inject_scan_codes(MoonDosRuntime *runtime,
                                  const uint8_t *scan_codes,
                                  size_t count)
{
    int old_interrupt_state;
    size_t index;

    if (runtime == NULL || scan_codes == NULL || count >=
            (size_t)MOON_DOS_SCANCODE_QUEUE_CAPACITY) {
        return -1;
    }

    old_interrupt_state =
        __dpmi_get_and_disable_virtual_interrupt_state();
    if (old_interrupt_state < 0) {
        return -1;
    }

    runtime->queue_tail = 0u;
    runtime->queue_head = (uint8_t)count;
    runtime->queue_faulted = 0u;
    for (index = 0u; index < count; ++index) {
        runtime->scan_codes[index] = scan_codes[index];
    }

    if (__dpmi_get_and_set_virtual_interrupt_state(old_interrupt_state) < 0) {
        return -1;
    }
    return 0;
}

static int test_inject_faulted_scan_code(MoonDosRuntime *runtime,
                                         uint8_t scan_code)
{
    int old_interrupt_state;

    if (runtime == NULL) {
        return -1;
    }

    old_interrupt_state =
        __dpmi_get_and_disable_virtual_interrupt_state();
    if (old_interrupt_state < 0) {
        return -1;
    }

    runtime->queue_tail = 0u;
    runtime->queue_head = 1u;
    runtime->scan_codes[0] = scan_code;
    runtime->queue_faulted = 1u;

    if (__dpmi_get_and_set_virtual_interrupt_state(old_interrupt_state) < 0) {
        return -1;
    }
    return 0;
}

static void test_argument_and_clock_contract(void)
{
    MoonDosRuntime runtime;
    MoonInput input;
    MoonDosTelemetry telemetry;
    uint32_t first_tick;
    uint32_t second_tick;
    volatile uint32_t spin;

    memset(&runtime, 0, sizeof(runtime));
    memset(&input, 0, sizeof(input));
    memset(&telemetry, 0, sizeof(telemetry));

    test_check(moon_dos_clock_frequency() == (uint32_t)UCLOCKS_PER_SEC,
               "clock frequency is UCLOCKS_PER_SEC");
    first_tick = moon_dos_clock_now();
    second_tick = first_tick;
    for (spin = 0u; spin < UINT32_C(1000000) &&
                    second_tick == first_tick; ++spin) {
        second_tick = moon_dos_clock_now();
    }
    test_check(second_tick != first_tick, "uclock-backed clock advances");

    test_check(moon_dos_runtime_init(NULL, &input) ==
                   MOON_DOS_ERR_ARGUMENT,
               "init rejects a null runtime");
    test_check(moon_dos_runtime_init(&runtime, NULL) ==
                   MOON_DOS_ERR_ARGUMENT,
               "init rejects a null input");
    test_check(moon_dos_runtime_drain_input(NULL, NULL) ==
                   MOON_DOS_ERR_ARGUMENT,
               "drain rejects a null runtime");
    test_check(moon_dos_runtime_present(NULL, test_framebuffer,
                                        sizeof(test_framebuffer)) ==
                   MOON_DOS_ERR_ARGUMENT,
               "present rejects a null runtime");
    test_check(moon_dos_runtime_get_telemetry(NULL, &telemetry) ==
                   MOON_DOS_ERR_ARGUMENT,
               "telemetry rejects a null runtime");
    test_check(moon_dos_runtime_get_telemetry(&runtime, NULL) ==
                   MOON_DOS_ERR_ARGUMENT,
               "telemetry rejects a null destination");
    test_check(moon_dos_runtime_clear_telemetry(NULL) ==
                   MOON_DOS_ERR_ARGUMENT,
               "telemetry clear rejects a null runtime");
    test_check(moon_dos_runtime_shutdown(NULL) == MOON_DOS_ERR_ARGUMENT,
               "shutdown rejects a null runtime");
    test_check(strcmp(moon_dos_result_name(MOON_DOS_ERR_VIDEO),
                      "video mode failure") == 0,
               "result names are exposed");
}

static void test_live_hardware_contract(void)
{
    static const uint8_t tap_a[] = {0x1eu, 0x9eu};
    static const uint8_t make_a[] = {0x1eu};
    MoonDosRuntime runtime;
    MoonDosRuntime duplicate_runtime;
    MoonInput input;
    MoonDosTelemetry telemetry;
    _go32_dpmi_seginfo vector_before;
    _go32_dpmi_seginfo vector_active;
    _go32_dpmi_seginfo vector_after;
    const MoonButtonState *action;
    const MoonButtonState *unreliable_action;
    MoonDosResult dos_result;
    uint8_t video_before;
    uint8_t video_current;
    uint32_t drained;
    size_t index;
    int adapter_live;
    int vector_number;

    memset(&runtime, 0, sizeof(runtime));
    memset(&duplicate_runtime, 0, sizeof(duplicate_runtime));
    memset(&input, 0, sizeof(input));
    memset(&telemetry, 0, sizeof(telemetry));
    memset(&vector_before, 0, sizeof(vector_before));
    memset(&vector_active, 0, sizeof(vector_active));
    memset(&vector_after, 0, sizeof(vector_after));
    video_before = 0u;
    video_current = 0u;
    drained = 0u;
    adapter_live = 0;
    vector_number =
        (int)_go32_info_block.master_interrupt_controller_base + 1;

    test_check(moon_input_init(&input, 2u, 12u, 3u) == MOON_OK,
               "portable input initializes");
    test_check(moon_input_bind(&input, 0u, 0u,
                               MOON_KEY_NORMAL(0x1eu)) == MOON_OK,
               "test action binds to A");
    test_check(moon_input_bind(&input, 1u, 0u,
                               MOON_KEY_NORMAL(0x30u)) == MOON_OK,
               "unreliable test action binds to B");
    test_check(test_query_video_mode(&video_before) == 0,
               "BIOS video mode can be saved");
    test_check(_go32_dpmi_get_protected_mode_interrupt_vector(
                   vector_number, &vector_before) == 0,
               "IRQ1 vector can be saved");

    if (moon_dos_runtime_init(&runtime, &input) == MOON_DOS_OK) {
        adapter_live = 1;
    } else {
        test_check(0, "adapter initializes");
    }

    if (adapter_live != 0) {
        test_check(runtime.keyboard_vector_number ==
                       (uint8_t)vector_number,
                   "adapter uses the remapped master PIC IRQ1 vector");
        test_check(test_query_video_mode(&video_current) == 0 &&
                       video_current == 0x13u,
                   "adapter enters and verifies stock mode 13h");
        test_check(_go32_dpmi_get_protected_mode_interrupt_vector(
                       vector_number, &vector_active) == 0 &&
                       test_vectors_equal(&vector_active,
                                          &runtime.keyboard_wrapper),
                   "adapter installs its protected-mode IRQ1 wrapper");
        test_check(moon_dos_runtime_init(&duplicate_runtime, &input) ==
                       MOON_DOS_ERR_STATE,
                   "a second live adapter is rejected");

        test_check(moon_dos_runtime_present(&runtime, NULL,
                                            sizeof(test_framebuffer)) ==
                       MOON_DOS_ERR_ARGUMENT,
                   "present rejects a null framebuffer");
        test_check(moon_dos_runtime_present(&runtime, test_framebuffer,
                                            sizeof(test_framebuffer) - 1u) ==
                       MOON_DOS_ERR_ARGUMENT,
                   "present rejects any size other than 64000 bytes");

        for (index = 0u; index < sizeof(test_framebuffer); ++index) {
            test_framebuffer[index] =
                (uint8_t)(((uint32_t)index * UINT32_C(37)) ^
                          ((uint32_t)index >> 8));
        }
        test_check(moon_dos_runtime_present(&runtime, test_framebuffer,
                                            sizeof(test_framebuffer)) ==
                       MOON_DOS_OK,
                   "present accepts exactly 64000 bytes");
        dosmemget(TEST_VGA_MEMORY, sizeof(test_readback), test_readback);
        test_check(memcmp(test_framebuffer, test_readback,
                          sizeof(test_framebuffer)) == 0,
                   "the complete 64000-byte framebuffer reaches VGA memory");

        test_check(moon_dos_runtime_drain_input(&runtime, NULL) ==
                       MOON_DOS_OK,
                   "foreground can drain any startup scan codes");
        moon_input_resync(&input);
        test_check(test_inject_scan_codes(&runtime, tap_a,
                                          sizeof(tap_a)) == 0,
                   "test scan codes enter the IRQ ring atomically");
        test_check(moon_dos_runtime_drain_input(&runtime, &drained) ==
                       MOON_DOS_OK &&
                       drained == (uint32_t)sizeof(tap_a),
                   "foreground drains a stable local scan-code batch");
        moon_input_tick(&input);
        action = moon_input_action(&input, 0u);
        test_check(action != NULL && action->pressed != 0u &&
                       action->released != 0u && action->held == 0u,
                   "drained make/break bytes reach the portable decoder");

        test_check(test_inject_scan_codes(&runtime, make_a,
                                          sizeof(make_a)) == 0,
                   "a reliable make enters the IRQ ring");
        test_check(moon_dos_runtime_drain_input(&runtime, &drained) ==
                       MOON_DOS_OK && drained == 1u,
                   "the reliable make drains normally");
        moon_input_tick(&input);
        action = moon_input_action(&input, 0u);
        test_check(action != NULL && action->pressed != 0u &&
                       action->held != 0u,
                   "the reliable make publishes a held action");

        test_check(test_inject_faulted_scan_code(&runtime, 0x30u) == 0,
                   "an unreliable make is marked as a faulted IRQ batch");
        drained = UINT32_MAX;
        test_check(moon_dos_runtime_drain_input(&runtime, &drained) ==
                       MOON_DOS_OK && drained == 0u,
                   "a faulted batch is discarded instead of decoded");
        moon_input_tick(&input);
        action = moon_input_action(&input, 0u);
        unreliable_action = moon_input_action(&input, 1u);
        test_check(action != NULL && action->held == 0u &&
                       action->released != 0u && action->pressed == 0u,
                   "resync releases the previously published hold only");
        test_check(unreliable_action != NULL &&
                       unreliable_action->held == 0u &&
                       unreliable_action->pressed == 0u &&
                       unreliable_action->released == 0u,
                   "resync never publishes the unreliable make");

        test_check(moon_dos_runtime_get_telemetry(&runtime, &telemetry) ==
                       MOON_DOS_OK &&
                       telemetry.frames_presented == 1u &&
                       telemetry.scan_codes_drained >=
                           (uint32_t)(sizeof(tap_a) + sizeof(make_a)) &&
                       telemetry.input_resync_events == 1u,
                   "telemetry records presentation, drain, and resync work");
        test_check(moon_dos_runtime_clear_telemetry(&runtime) ==
                       MOON_DOS_OK,
                   "adapter telemetry can be cleared");
        test_check(moon_dos_runtime_get_telemetry(&runtime, &telemetry) ==
                       MOON_DOS_OK &&
                       telemetry.frames_presented == 0u &&
                       telemetry.scan_codes_drained == 0u,
                   "telemetry clear is observable");

        test_check(moon_dos_runtime_shutdown(&runtime) == MOON_DOS_OK,
                   "adapter shuts down cleanly");
        adapter_live = 0;
        test_check(_go32_dpmi_get_protected_mode_interrupt_vector(
                       vector_number, &vector_after) == 0 &&
                       test_vectors_equal(&vector_after, &vector_before),
                   "shutdown restores the original IRQ1 vector");
        test_check(test_query_video_mode(&video_current) == 0 &&
                       video_current == video_before,
                   "shutdown restores and verifies the original video mode");
        test_check(moon_dos_runtime_shutdown(&runtime) == MOON_DOS_OK,
                   "shutdown is idempotent");

        dos_result = moon_dos_runtime_init(&runtime, &input);
        test_check(dos_result == MOON_DOS_OK,
                   "adapter can initialize again after complete shutdown");
        if (dos_result == MOON_DOS_OK) {
            adapter_live = 1;
            test_check(test_query_video_mode(&video_current) == 0 &&
                           video_current == 0x13u,
                       "reinitialized adapter re-enters mode 13h");
            test_check(moon_dos_runtime_shutdown(&runtime) ==
                           MOON_DOS_OK,
                       "reinitialized adapter balances teardown resources");
            adapter_live = 0;
            test_check(_go32_dpmi_get_protected_mode_interrupt_vector(
                           vector_number, &vector_after) == 0 &&
                           test_vectors_equal(&vector_after,
                                              &vector_before),
                       "reinitialized adapter restores IRQ1 again");
            test_check(test_query_video_mode(&video_current) == 0 &&
                           video_current == video_before,
                       "reinitialized adapter restores video again");
        }
    }

    if (adapter_live != 0) {
        test_check(moon_dos_runtime_shutdown(&runtime) == MOON_DOS_OK,
                   "failed test cleanup restores adapter hardware");
    }
}

int main(void)
{
    test_argument_and_clock_contract();
    test_live_hardware_contract();

    if (test_failures != 0u) {
        printf("runtime DOS adapter tests: FAIL (%u)\n", test_failures);
        return 1;
    }

    puts("runtime DOS adapter tests: PASS");
    return 0;
}
