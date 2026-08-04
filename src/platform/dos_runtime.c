#include "moon/dos_runtime.h"

#include <go32.h>
#include <string.h>
#include <sys/movedata.h>
#include <time.h>

#define MOON_DOS_VGA_MEMORY UINT32_C(0x000a0000)
#define MOON_DOS_BIOS_VIDEO_INTERRUPT 0x10
#define MOON_DOS_VIDEO_MODE_13H 0x13u
#define MOON_DOS_KEYBOARD_STATUS_PORT 0x64u
#define MOON_DOS_KEYBOARD_DATA_PORT 0x60u
#define MOON_DOS_KEYBOARD_CONTROL_PORT 0x61u
#define MOON_DOS_KEYBOARD_ACK_BIT 0x80u
#define MOON_DOS_STATUS_TIMEOUT_BIT 0x40u
#define MOON_DOS_STATUS_PARITY_BIT 0x80u
#define MOON_DOS_MASTER_PIC_COMMAND_PORT 0x20u
#define MOON_DOS_PIC_EOI 0x20u
#define MOON_DOS_ISR_LOCK_BYTES 4096ul

#define MOON_DOS_QUEUE_MASK (MOON_DOS_SCANCODE_QUEUE_CAPACITY - 1u)

#if (MOON_DOS_SCANCODE_QUEUE_CAPACITY == 0u) ||                         \
    ((MOON_DOS_SCANCODE_QUEUE_CAPACITY &                              \
      (MOON_DOS_SCANCODE_QUEUE_CAPACITY - 1u)) != 0u) ||              \
    (MOON_DOS_SCANCODE_QUEUE_CAPACITY > 256u)
#error MOON_DOS_SCANCODE_QUEUE_CAPACITY must be a power of two up to 256
#endif

static MoonDosRuntime *volatile moon_dos_active;

/*
 * DPMI maintains a count for every locked page.  Preserve the exact selector,
 * offset, and size tuple for each adapter acquisition, then issue one matching
 * unlock.  If DJGPP's iret wrapper locks an overlapping page, its independent
 * count therefore remains in force when the adapter releases only its count.
 */
static MoonDosResult moon_dos_set_region_lock(const void *address,
                                               unsigned long size,
                                               int selector,
                                               int lock_region)
{
    __dpmi_meminfo region;
    unsigned long segment_base;
    int dpmi_result;

    if (address == NULL || size == 0ul) {
        return MOON_DOS_ERR_ARGUMENT;
    }
    if (__dpmi_get_segment_base_address(selector, &segment_base) != 0) {
        return MOON_DOS_ERR_DPMI;
    }

    memset(&region, 0, sizeof(region));
    region.address = segment_base + (unsigned long)(uintptr_t)address;
    region.size = size;
    if (lock_region != 0) {
        dpmi_result = __dpmi_lock_linear_region(&region);
    } else {
        dpmi_result = __dpmi_unlock_linear_region(&region);
    }

    return dpmi_result == 0 ? MOON_DOS_OK : MOON_DOS_ERR_DPMI;
}

static MoonDosResult moon_dos_lock_data(const void *address,
                                        unsigned long size)
{
    return moon_dos_set_region_lock(address, size, _go32_my_ds(), 1);
}

static MoonDosResult moon_dos_unlock_data(const void *address,
                                          unsigned long size)
{
    return moon_dos_set_region_lock(address, size, _go32_my_ds(), 0);
}

static MoonDosResult moon_dos_lock_code(const void *address,
                                        unsigned long size)
{
    return moon_dos_set_region_lock(address, size, _go32_my_cs(), 1);
}

static MoonDosResult moon_dos_unlock_code(const void *address,
                                          unsigned long size)
{
    return moon_dos_set_region_lock(address, size, _go32_my_cs(), 0);
}

/* Always inline so even the -O0 debug IRQ path has no libc dependencies. */
static __inline__ __attribute__((always_inline)) uint8_t
moon_dos_in8(uint16_t port)
{
    uint8_t value;

    __asm__ __volatile__("inb %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static __inline__ __attribute__((always_inline)) void
moon_dos_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %w1" : : "a"(value), "Nd"(port));
}

static MoonDosResult moon_dos_disable_virtual_interrupts(int *old_state)
{
    int state;

    if (old_state == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }

    state = __dpmi_get_and_disable_virtual_interrupt_state();
    if (state < 0) {
        return MOON_DOS_ERR_DPMI;
    }

    *old_state = state;
    return MOON_DOS_OK;
}

static MoonDosResult moon_dos_restore_virtual_interrupts(int old_state)
{
    if (__dpmi_get_and_set_virtual_interrupt_state(old_state) < 0) {
        return MOON_DOS_ERR_DPMI;
    }

    return MOON_DOS_OK;
}

static MoonDosResult moon_dos_query_video_mode(uint8_t *mode)
{
    __dpmi_regs registers;

    if (mode == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }

    memset(&registers, 0, sizeof(registers));
    registers.h.ah = 0x0fu;
    if (__dpmi_int(MOON_DOS_BIOS_VIDEO_INTERRUPT, &registers) != 0) {
        return MOON_DOS_ERR_DPMI;
    }

    *mode = registers.h.al;
    return MOON_DOS_OK;
}

static MoonDosResult moon_dos_set_video_mode(uint8_t mode)
{
    __dpmi_regs registers;
    uint8_t verified_mode;
    MoonDosResult result;

    memset(&registers, 0, sizeof(registers));
    registers.h.ah = 0x00u;
    registers.h.al = mode;
    if (__dpmi_int(MOON_DOS_BIOS_VIDEO_INTERRUPT, &registers) != 0) {
        return MOON_DOS_ERR_DPMI;
    }

    result = moon_dos_query_video_mode(&verified_mode);
    if (result != MOON_DOS_OK) {
        return result;
    }
    if (verified_mode != mode) {
        return MOON_DOS_ERR_VIDEO;
    }

    return MOON_DOS_OK;
}

/* This function and every object it touches are locked before installation. */
static void moon_dos_keyboard_isr(void)
{
    MoonDosRuntime *runtime;
    unsigned int status;
    unsigned int scan_code;
    unsigned int control;
    unsigned int next_head;

    runtime = moon_dos_active;
    status = moon_dos_in8(MOON_DOS_KEYBOARD_STATUS_PORT);
    scan_code = moon_dos_in8(MOON_DOS_KEYBOARD_DATA_PORT);

    if (runtime != NULL) {
        ++runtime->keyboard_interrupts;

        if ((status & MOON_DOS_STATUS_PARITY_BIT) != 0u) {
            ++runtime->controller_parity_errors;
            runtime->queue_faulted = 1u;
        }
        if ((status & MOON_DOS_STATUS_TIMEOUT_BIT) != 0u) {
            ++runtime->controller_timeout_errors;
            runtime->queue_faulted = 1u;
        }

        if (runtime->queue_faulted == 0u) {
            next_head = ((unsigned int)runtime->queue_head + 1u) &
                        MOON_DOS_QUEUE_MASK;
            if (next_head == (unsigned int)runtime->queue_tail) {
                ++runtime->queue_overflow_events;
                runtime->queue_faulted = 1u;
            } else {
                runtime->scan_codes[runtime->queue_head] =
                    (uint8_t)scan_code;
                runtime->queue_head = (uint8_t)next_head;
                ++runtime->scan_codes_queued;
            }
        }
    }

    control = moon_dos_in8(MOON_DOS_KEYBOARD_CONTROL_PORT);
    moon_dos_out8(MOON_DOS_KEYBOARD_CONTROL_PORT,
                  (uint8_t)(control | MOON_DOS_KEYBOARD_ACK_BIT));
    moon_dos_out8(MOON_DOS_KEYBOARD_CONTROL_PORT, (uint8_t)control);
    moon_dos_out8(MOON_DOS_MASTER_PIC_COMMAND_PORT, MOON_DOS_PIC_EOI);
}

uint32_t moon_dos_clock_frequency(void)
{
    return (uint32_t)UCLOCKS_PER_SEC;
}

uint32_t moon_dos_clock_now(void)
{
    return (uint32_t)(uint64_t)uclock();
}

static MoonDosResult moon_dos_init_failed(MoonDosRuntime *runtime,
                                          MoonDosResult original_result)
{
    MoonDosResult cleanup_result;

    cleanup_result = moon_dos_runtime_shutdown(runtime);
    if (cleanup_result != MOON_DOS_OK) {
        return cleanup_result;
    }
    return original_result;
}

static MoonDosResult moon_dos_init_failed_after_interrupt_restore(
    MoonDosRuntime *runtime,
    int desired_interrupt_state)
{
    MoonDosResult cleanup_result;
    MoonDosResult retry_result;

    /*
     * A host which rejects virtual-interrupt restoration is already outside
     * the adapter's recoverable contract.  Still tear down every resource and
     * retry the caller's saved state so a transient DPMI failure cannot leak
     * the wrapper, vector ownership, or video mode.
     */
    cleanup_result = moon_dos_runtime_shutdown(runtime);
    retry_result =
        moon_dos_restore_virtual_interrupts(desired_interrupt_state);
    if (cleanup_result != MOON_DOS_OK) {
        return cleanup_result;
    }
    if (retry_result != MOON_DOS_OK) {
        return retry_result;
    }
    return MOON_DOS_ERR_DPMI;
}

MoonDosResult moon_dos_runtime_init(MoonDosRuntime *runtime,
                                    MoonInput *input)
{
    MoonDosResult result;
    int old_interrupt_state;

    if (runtime == NULL || input == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }
    if (moon_dos_active != NULL) {
        return MOON_DOS_ERR_STATE;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->input = input;
    runtime->keyboard_vector_number =
        (uint8_t)(_go32_info_block.master_interrupt_controller_base + 1u);

    result = moon_dos_lock_data((const void *)&moon_dos_active,
                                (unsigned long)sizeof(moon_dos_active));
    if (result != MOON_DOS_OK) {
        memset(runtime, 0, sizeof(*runtime));
        return result;
    }
    runtime->active_pointer_locked = 1u;

    result = moon_dos_lock_data(runtime, (unsigned long)sizeof(*runtime));
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }
    runtime->runtime_data_locked = 1u;

    result = moon_dos_lock_code(
        (const void *)(uintptr_t)moon_dos_keyboard_isr,
        MOON_DOS_ISR_LOCK_BYTES);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }
    runtime->isr_code_locked = 1u;

    result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }
    if (moon_dos_active != NULL) {
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return moon_dos_init_failed_after_interrupt_restore(
                runtime, old_interrupt_state);
        }
        return moon_dos_init_failed(runtime, MOON_DOS_ERR_STATE);
    }
    moon_dos_active = runtime;
    result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed_after_interrupt_restore(
            runtime, old_interrupt_state);
    }

    result = moon_dos_query_video_mode(&runtime->saved_video_mode);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }
    runtime->video_mode_active = 1u;
    result = moon_dos_set_video_mode(MOON_DOS_VIDEO_MODE_13H);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }

    if (_go32_dpmi_get_protected_mode_interrupt_vector(
            runtime->keyboard_vector_number,
            &runtime->old_keyboard_vector) != 0) {
        return moon_dos_init_failed(runtime, MOON_DOS_ERR_DPMI);
    }
    runtime->vector_saved = 1u;

    memset(&runtime->keyboard_wrapper, 0,
           sizeof(runtime->keyboard_wrapper));
    runtime->keyboard_wrapper.pm_offset =
        (unsigned long)(uintptr_t)moon_dos_keyboard_isr;
    runtime->keyboard_wrapper.pm_selector =
        (unsigned short)_go32_my_cs();
    if (_go32_dpmi_allocate_iret_wrapper(&runtime->keyboard_wrapper) != 0) {
        return moon_dos_init_failed(runtime, MOON_DOS_ERR_DPMI);
    }
    runtime->wrapper_allocated = 1u;

    result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed(runtime, result);
    }
    if (_go32_dpmi_set_protected_mode_interrupt_vector(
            runtime->keyboard_vector_number,
            &runtime->keyboard_wrapper) != 0) {
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return moon_dos_init_failed_after_interrupt_restore(
                runtime, old_interrupt_state);
        }
        return moon_dos_init_failed(runtime, MOON_DOS_ERR_DPMI);
    }
    runtime->vector_installed = 1u;
    runtime->initialized = 1u;
    result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return moon_dos_init_failed_after_interrupt_restore(
            runtime, old_interrupt_state);
    }

    return MOON_DOS_OK;
}

MoonDosResult moon_dos_runtime_drain_input(MoonDosRuntime *runtime,
                                          uint32_t *bytes_drained)
{
    uint8_t local_scan_codes[MOON_DOS_SCANCODE_QUEUE_CAPACITY];
    uint32_t local_count;
    uint32_t index;
    uint8_t tail;
    int resync_required;
    int old_interrupt_state;
    MoonDosResult result;

    if (bytes_drained != NULL) {
        *bytes_drained = 0u;
    }
    if (runtime == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }

    result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return result;
    }
    if (moon_dos_active != runtime || runtime->initialized == 0u ||
        runtime->vector_installed == 0u || runtime->input == NULL) {
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        return result == MOON_DOS_OK ? MOON_DOS_ERR_STATE : result;
    }

    local_count = 0u;
    resync_required = runtime->queue_faulted != 0u;
    tail = runtime->queue_tail;
    if (resync_required != 0) {
        runtime->queue_tail = runtime->queue_head;
        runtime->queue_faulted = 0u;
        ++runtime->input_resync_events;
    } else {
        while (tail != runtime->queue_head) {
            local_scan_codes[local_count] = runtime->scan_codes[tail];
            ++local_count;
            tail = (uint8_t)(((unsigned int)tail + 1u) &
                             MOON_DOS_QUEUE_MASK);
        }
        runtime->queue_tail = tail;
        runtime->scan_codes_drained += local_count;
    }

    result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return result;
    }

    if (resync_required != 0) {
        moon_input_resync(runtime->input);
    } else {
        for (index = 0u; index < local_count; ++index) {
            if (moon_input_feed_scancode(runtime->input,
                                         local_scan_codes[index]) != MOON_OK) {
                moon_input_resync(runtime->input);
                return MOON_DOS_ERR_STATE;
            }
        }
    }

    if (bytes_drained != NULL) {
        *bytes_drained = local_count;
    }
    return MOON_DOS_OK;
}

MoonDosResult moon_dos_runtime_present(MoonDosRuntime *runtime,
                                       const uint8_t *framebuffer,
                                       size_t framebuffer_size)
{
    if (runtime == NULL || framebuffer == NULL ||
        framebuffer_size != (size_t)MOON_DOS_FRAMEBUFFER_BYTES) {
        return MOON_DOS_ERR_ARGUMENT;
    }
    if (moon_dos_active != runtime || runtime->initialized == 0u ||
        runtime->video_mode_active == 0u) {
        return MOON_DOS_ERR_STATE;
    }

    dosmemput(framebuffer, (size_t)MOON_DOS_FRAMEBUFFER_BYTES,
              MOON_DOS_VGA_MEMORY);
    ++runtime->frames_presented;
    return MOON_DOS_OK;
}

MoonDosResult moon_dos_runtime_get_telemetry(
    const MoonDosRuntime *runtime,
    MoonDosTelemetry *telemetry)
{
    MoonDosResult result;
    int old_interrupt_state;

    if (runtime == NULL || telemetry == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }

    result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return result;
    }
    if (moon_dos_active != runtime || runtime->initialized == 0u) {
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        return result == MOON_DOS_OK ? MOON_DOS_ERR_STATE : result;
    }

    telemetry->keyboard_interrupts = runtime->keyboard_interrupts;
    telemetry->scan_codes_queued = runtime->scan_codes_queued;
    telemetry->scan_codes_drained = runtime->scan_codes_drained;
    telemetry->queue_overflow_events = runtime->queue_overflow_events;
    telemetry->controller_parity_errors =
        runtime->controller_parity_errors;
    telemetry->controller_timeout_errors =
        runtime->controller_timeout_errors;
    telemetry->input_resync_events = runtime->input_resync_events;
    telemetry->frames_presented = runtime->frames_presented;

    return moon_dos_restore_virtual_interrupts(old_interrupt_state);
}

MoonDosResult moon_dos_runtime_clear_telemetry(MoonDosRuntime *runtime)
{
    MoonDosResult result;
    int old_interrupt_state;

    if (runtime == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }

    result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
    if (result != MOON_DOS_OK) {
        return result;
    }
    if (moon_dos_active != runtime || runtime->initialized == 0u) {
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        return result == MOON_DOS_OK ? MOON_DOS_ERR_STATE : result;
    }

    runtime->keyboard_interrupts = 0u;
    runtime->scan_codes_queued = 0u;
    runtime->scan_codes_drained = 0u;
    runtime->queue_overflow_events = 0u;
    runtime->controller_parity_errors = 0u;
    runtime->controller_timeout_errors = 0u;
    runtime->input_resync_events = 0u;
    runtime->frames_presented = 0u;

    return moon_dos_restore_virtual_interrupts(old_interrupt_state);
}

MoonDosResult moon_dos_runtime_shutdown(MoonDosRuntime *runtime)
{
    MoonDosResult result;
    int old_interrupt_state;

    if (runtime == NULL) {
        return MOON_DOS_ERR_ARGUMENT;
    }
    if (moon_dos_active != runtime) {
        if (runtime->initialized == 0u && runtime->vector_installed == 0u &&
            runtime->wrapper_allocated == 0u &&
            runtime->video_mode_active == 0u &&
            runtime->active_pointer_locked == 0u &&
            runtime->runtime_data_locked == 0u &&
            runtime->isr_code_locked == 0u) {
            return MOON_DOS_OK;
        }
        if (runtime->initialized != 0u || runtime->vector_installed != 0u ||
            runtime->wrapper_allocated != 0u ||
            runtime->video_mode_active != 0u) {
            return MOON_DOS_ERR_STATE;
        }
    }

    if (runtime->vector_installed != 0u) {
        if (runtime->vector_saved == 0u) {
            return MOON_DOS_ERR_STATE;
        }
        result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return result;
        }
        if (_go32_dpmi_set_protected_mode_interrupt_vector(
                runtime->keyboard_vector_number,
                &runtime->old_keyboard_vector) != 0) {
            result = moon_dos_restore_virtual_interrupts(
                old_interrupt_state);
            return result == MOON_DOS_OK ? MOON_DOS_ERR_DPMI : result;
        }
        runtime->vector_installed = 0u;
        runtime->initialized = 0u;
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return result;
        }
    }

    /* The wrapper remains allocated unless the original vector is restored. */
    if (runtime->wrapper_allocated != 0u) {
        if (_go32_dpmi_free_iret_wrapper(&runtime->keyboard_wrapper) != 0) {
            return MOON_DOS_ERR_DPMI;
        }
        runtime->wrapper_allocated = 0u;
    }

    if (runtime->video_mode_active != 0u) {
        result = moon_dos_set_video_mode(runtime->saved_video_mode);
        if (result != MOON_DOS_OK) {
            return result;
        }
        runtime->video_mode_active = 0u;
    }

    if (runtime->isr_code_locked != 0u) {
        result = moon_dos_unlock_code(
            (const void *)(uintptr_t)moon_dos_keyboard_isr,
            MOON_DOS_ISR_LOCK_BYTES);
        if (result != MOON_DOS_OK) {
            return result;
        }
        runtime->isr_code_locked = 0u;
    }

    if (runtime->runtime_data_locked != 0u) {
        result = moon_dos_unlock_data(runtime,
                                      (unsigned long)sizeof(*runtime));
        if (result != MOON_DOS_OK) {
            return result;
        }
        runtime->runtime_data_locked = 0u;
    }

    if (moon_dos_active == runtime) {
        result = moon_dos_disable_virtual_interrupts(&old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return result;
        }
        moon_dos_active = NULL;
        result = moon_dos_restore_virtual_interrupts(old_interrupt_state);
        if (result != MOON_DOS_OK) {
            return result;
        }
    }

    if (runtime->active_pointer_locked != 0u) {
        result = moon_dos_unlock_data(
            (const void *)&moon_dos_active,
            (unsigned long)sizeof(moon_dos_active));
        if (result != MOON_DOS_OK) {
            return result;
        }
        runtime->active_pointer_locked = 0u;
    }

    memset(runtime, 0, sizeof(*runtime));
    return MOON_DOS_OK;
}

const char *moon_dos_result_name(MoonDosResult result)
{
    switch (result) {
        case MOON_DOS_OK:
            return "ok";
        case MOON_DOS_ERR_ARGUMENT:
            return "invalid argument";
        case MOON_DOS_ERR_STATE:
            return "invalid state";
        case MOON_DOS_ERR_DPMI:
            return "DPMI failure";
        case MOON_DOS_ERR_VIDEO:
            return "video mode failure";
        default:
            return "unknown DOS runtime result";
    }
}
