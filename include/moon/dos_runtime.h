#ifndef MOON_DOS_RUNTIME_H
#define MOON_DOS_RUNTIME_H

/* DJGPP hardware boundary for the portable MOON runtime. */

#include <stddef.h>
#include <stdint.h>

#include <dpmi.h>

#include "moon/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOON_DOS_FRAMEBUFFER_WIDTH 320u
#define MOON_DOS_FRAMEBUFFER_HEIGHT 200u
#define MOON_DOS_FRAMEBUFFER_BYTES 64000u
#define MOON_DOS_SCANCODE_QUEUE_CAPACITY 128u

typedef enum MoonDosResult {
    MOON_DOS_OK = 0,
    MOON_DOS_ERR_ARGUMENT,
    MOON_DOS_ERR_STATE,
    MOON_DOS_ERR_DPMI,
    MOON_DOS_ERR_VIDEO
} MoonDosResult;

typedef struct MoonDosTelemetry {
    uint32_t keyboard_interrupts;
    uint32_t scan_codes_queued;
    uint32_t scan_codes_drained;
    uint32_t queue_overflow_events;
    uint32_t controller_parity_errors;
    uint32_t controller_timeout_errors;
    uint32_t input_resync_events;
    uint32_t frames_presented;
} MoonDosTelemetry;

/*
 * Caller-owned adapter storage.  Only one initialized instance may be active
 * in a process because IRQ1 has a single protected-mode vector.  Treat fields
 * below as private and use the accessors in this header.
 */
typedef struct MoonDosRuntime {
    MoonInput *input;
    _go32_dpmi_seginfo old_keyboard_vector;
    _go32_dpmi_seginfo keyboard_wrapper;
    volatile uint32_t keyboard_interrupts;
    volatile uint32_t scan_codes_queued;
    volatile uint32_t scan_codes_drained;
    volatile uint32_t queue_overflow_events;
    volatile uint32_t controller_parity_errors;
    volatile uint32_t controller_timeout_errors;
    volatile uint32_t input_resync_events;
    volatile uint32_t frames_presented;
    volatile uint8_t scan_codes[MOON_DOS_SCANCODE_QUEUE_CAPACITY];
    volatile uint8_t queue_head;
    volatile uint8_t queue_tail;
    volatile uint8_t queue_faulted;
    uint8_t saved_video_mode;
    uint8_t keyboard_vector_number;
    uint8_t video_mode_active;
    uint8_t vector_saved;
    uint8_t wrapper_allocated;
    uint8_t vector_installed;
    uint8_t initialized;
    uint8_t active_pointer_locked;
    uint8_t runtime_data_locked;
    uint8_t isr_code_locked;
} MoonDosRuntime;

/* Wrapping foreground clock; the adapter never reprograms IRQ0. */
uint32_t moon_dos_clock_frequency(void);
uint32_t moon_dos_clock_now(void);

MoonDosResult moon_dos_runtime_init(MoonDosRuntime *runtime,
                                    MoonInput *input);

/*
 * Moves a stable foreground copy out of the IRQ ring, then feeds the portable
 * decoder.  Any overflow or controller error discards the batch and performs
 * a hard input resynchronization.  bytes_drained may be NULL.
 */
MoonDosResult moon_dos_runtime_drain_input(MoonDosRuntime *runtime,
                                          uint32_t *bytes_drained);

/* Copies exactly 64,000 bytes to VGA mode 13h memory at A000:0000. */
MoonDosResult moon_dos_runtime_present(MoonDosRuntime *runtime,
                                       const uint8_t *framebuffer,
                                       size_t framebuffer_size);

MoonDosResult moon_dos_runtime_get_telemetry(
    const MoonDosRuntime *runtime,
    MoonDosTelemetry *telemetry);
MoonDosResult moon_dos_runtime_clear_telemetry(MoonDosRuntime *runtime);

/* Reverse-order, idempotent teardown. */
MoonDosResult moon_dos_runtime_shutdown(MoonDosRuntime *runtime);

const char *moon_dos_result_name(MoonDosResult result);

#ifdef __cplusplus
}
#endif

#endif
