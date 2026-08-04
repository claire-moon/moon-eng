#ifndef MOON_CGUI_H
#define MOON_CGUI_H

/*
 * Portable, caller-owned CGUI core.
 *
 * This interface deliberately has no DOS, VGA, input-device, clock,
 * allocation, callback, or process-global dependencies.  Callers supply an
 * indexed surface and one normalized action frame per logical UI tick.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CGUI_CLIP_STACK_CAPACITY 16u
#define CGUI_MENU_ITEM_CAPACITY 32u
#define CGUI_ASCII_MAP_SIZE 128u
#define CGUI_TEXT_BYTE_CAPACITY 4096u

typedef uint32_t CguiId;
#define CGUI_ID_NONE UINT32_C(0)

typedef enum CguiResult {
    CGUI_OK = 0,
    CGUI_ERR_ARGUMENT,
    CGUI_ERR_CONFIG,
    CGUI_ERR_RANGE,
    CGUI_ERR_CAPACITY,
    CGUI_ERR_DUPLICATE,
    CGUI_ERR_NOT_FOUND,
    CGUI_ERR_DISABLED,
    CGUI_ERR_STATE
} CguiResult;

/* Every rectangle is [x0, x1) by [y0, y1). */
typedef struct CguiRect {
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
} CguiRect;

/*
 * One byte per indexed pixel. Padding at the end of a row is never drawn.
 * The validated addressable extent is capped at UINT32_MAX for DOS/host parity.
 * The descriptor is copied; pixels remain borrowed writable storage and must
 * outlive every context which currently references this surface.
 */
typedef struct CguiSurface {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    size_t stride;
    size_t byte_count;
} CguiSurface;

/*
 * Explicit bounded spans keep model validation and rendering independent of
 * strlen. A span longer than CGUI_TEXT_BYTE_CAPACITY is invalid.
 */
typedef struct CguiText {
    const char *data;
    size_t length;
} CguiText;

/*
 * row_bits contains glyph_count * glyph_height bytes.  Each byte describes
 * one row; bit (glyph_width - 1 - x) is column x.  ascii_map has exactly 128
 * entries and maps input bytes to glyph indices.  The descriptor is copied,
 * but these two immutable buffers are borrowed and must outlive the context.
 */
typedef struct CguiFont {
    const uint8_t *row_bits;
    const uint8_t *ascii_map;
    size_t row_bits_size;
    size_t ascii_map_size;
    uint16_t glyph_count;
    uint8_t glyph_width;
    uint8_t glyph_height;
    uint8_t advance_x;
    uint8_t line_advance;
    uint8_t fallback_glyph;
} CguiFont;

typedef enum CguiPaletteRole {
    CGUI_ROLE_DESKTOP = 0,
    CGUI_ROLE_PANEL,
    CGUI_ROLE_BORDER_LIGHT,
    CGUI_ROLE_BORDER_DARK,
    CGUI_ROLE_TEXT,
    CGUI_ROLE_TEXT_DISABLED,
    CGUI_ROLE_SELECTION,
    CGUI_ROLE_SELECTION_TEXT,
    CGUI_ROLE_FOCUS,
    CGUI_ROLE_WARNING,
    CGUI_ROLE_MODAL_SHADE,
    CGUI_PALETTE_ROLE_COUNT
} CguiPaletteRole;

typedef struct CguiPalette {
    uint8_t index[CGUI_PALETTE_ROLE_COUNT];
} CguiPalette;

typedef struct CguiMenuItem {
    CguiId id;
    CguiText label;
    uint8_t enabled;
} CguiMenuItem;

typedef struct CguiMenuState {
    CguiMenuItem items[CGUI_MENU_ITEM_CAPACITY];
    size_t item_count;
    CguiId owner_id;
    CguiId focus_id;
} CguiMenuState;

/* Modal descriptors are copied; every CguiText buffer is borrowed until close. */
typedef struct CguiHelpSpec {
    CguiId owner_id;
    CguiText title;
    CguiText body;
    CguiText close_label;
} CguiHelpSpec;

typedef struct CguiConfirmSpec {
    CguiId owner_id;
    CguiId yes_id;
    CguiId no_id;
    CguiText title;
    CguiText message;
    CguiText yes_label;
    CguiText no_label;
} CguiConfirmSpec;

typedef enum CguiModalKind {
    CGUI_MODAL_NONE = 0,
    CGUI_MODAL_HELP,
    CGUI_MODAL_CONFIRM
} CguiModalKind;

typedef union CguiModalSpec {
    CguiHelpSpec help;
    CguiConfirmSpec confirm;
} CguiModalSpec;

typedef struct CguiModalState {
    CguiModalSpec spec;
    CguiId focus_id;
    CguiModalKind kind;
} CguiModalState;

typedef enum CguiAction {
    CGUI_ACTION_UP = 0,
    CGUI_ACTION_DOWN,
    CGUI_ACTION_ENTER,
    CGUI_ACTION_SPACE,
    CGUI_ACTION_ESCAPE,
    CGUI_ACTION_F1,
    CGUI_ACTION_COUNT
} CguiAction;

typedef struct CguiActionState {
    uint8_t pressed;
    uint8_t repeat;
} CguiActionState;

/*
 * accelerator_id is resolved by the caller from its rebindable actions.  It
 * is a stable target ID, never an ASCII value or hardware scan code.  Zero
 * means that no accelerator edge occurred on this tick.
 */
typedef struct CguiInputFrame {
    CguiActionState actions[CGUI_ACTION_COUNT];
    CguiId accelerator_id;
} CguiInputFrame;

typedef enum CguiEventType {
    CGUI_EVENT_NONE = 0,
    CGUI_EVENT_MENU_ACTIVATE,
    CGUI_EVENT_MENU_CANCEL,
    CGUI_EVENT_HELP_REQUEST,
    CGUI_EVENT_HELP_CLOSE,
    CGUI_EVENT_CONFIRM_YES,
    CGUI_EVENT_CONFIRM_NO,
    CGUI_EVENT_CONFIRM_CANCEL
} CguiEventType;

typedef struct CguiEvent {
    CguiEventType type;
    CguiId owner_id;
    CguiId item_id;
} CguiEvent;

/* All mutable GUI state is stored in this caller-owned object. */
typedef struct CguiContext {
    CguiSurface surface;
    CguiFont font;
    CguiPalette palette;
    CguiRect clip_stack[CGUI_CLIP_STACK_CAPACITY];
    CguiMenuState menu;
    CguiModalState modal;
    uint32_t initialized_cookie;
    uint8_t clip_depth;
} CguiContext;

const char *cgui_result_name(CguiResult result);

/* Immutable built-ins; neither function establishes mutable global state. */
void cgui_palette_classic(CguiPalette *palette);
const CguiFont *cgui_font_builtin_3x5(void);

CguiResult cgui_context_init(CguiContext *context,
                             const CguiSurface *surface,
                             const CguiFont *font,
                             const CguiPalette *palette);
void cgui_context_reset(CguiContext *context);
CguiResult cgui_set_surface(CguiContext *context,
                            const CguiSurface *surface);
CguiResult cgui_set_palette(CguiContext *context,
                            const CguiPalette *palette);

/* Push intersects with the active clip; it can never widen that clip. */
CguiResult cgui_clip_push(CguiContext *context, CguiRect clip);
CguiResult cgui_clip_pop(CguiContext *context);
void cgui_clip_reset(CguiContext *context);
CguiResult cgui_clip_get(const CguiContext *context, CguiRect *clip);

CguiResult cgui_fill_rect(CguiContext *context,
                          CguiRect rect,
                          CguiPaletteRole role);
CguiResult cgui_draw_border(CguiContext *context,
                            CguiRect rect,
                            CguiPaletteRole top_left,
                            CguiPaletteRole bottom_right);
CguiResult cgui_draw_char(CguiContext *context,
                          int32_t x,
                          int32_t y,
                          uint8_t character,
                          CguiPaletteRole role);
CguiResult cgui_draw_text(CguiContext *context,
                          int32_t x,
                          int32_t y,
                          CguiText text,
                          CguiPaletteRole role);

/* 32-bit FNV-1a of visible pixels in row order; row padding is excluded. */
CguiResult cgui_surface_hash(const CguiContext *context, uint32_t *hash);

/* Menu labels are borrowed immutable spans and must outlive the model. */
CguiResult cgui_menu_set_items(CguiContext *context,
                               CguiId owner_id,
                               const CguiMenuItem *items,
                               size_t item_count);
void cgui_menu_clear(CguiContext *context);
CguiResult cgui_menu_set_enabled(CguiContext *context,
                                 CguiId item_id,
                                 int enabled);
CguiResult cgui_menu_set_focus(CguiContext *context, CguiId item_id);
CguiId cgui_menu_focus(const CguiContext *context);
CguiResult cgui_draw_menu(CguiContext *context, CguiRect bounds);

/* Only one modal may be open. Help and confirmation own every input action. */
CguiResult cgui_help_open(CguiContext *context,
                          const CguiHelpSpec *spec);
CguiResult cgui_help_close(CguiContext *context);
int cgui_help_is_open(const CguiContext *context);
CguiResult cgui_draw_help(CguiContext *context, CguiRect bounds);

CguiResult cgui_confirm_open(CguiContext *context,
                             const CguiConfirmSpec *spec);
CguiResult cgui_confirm_close(CguiContext *context);
int cgui_confirm_is_open(const CguiContext *context);
CguiId cgui_confirm_focus(const CguiContext *context);
CguiResult cgui_draw_confirm(CguiContext *context, CguiRect bounds);
CguiModalKind cgui_modal_kind(const CguiContext *context);

/* Clears event first and emits at most one event per logical UI tick. */
CguiResult cgui_update(CguiContext *context,
                       const CguiInputFrame *input,
                       CguiEvent *event);

#ifdef __cplusplus
}
#endif

#endif
