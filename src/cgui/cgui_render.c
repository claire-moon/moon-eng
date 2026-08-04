#include "moon/cgui.h"

#include "cgui_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define CGUI_FNV1A_OFFSET UINT32_C(2166136261)
#define CGUI_FNV1A_PRIME UINT32_C(16777619)

static CguiResult cgui_surface_validate(const CguiSurface *surface)
{
    uint64_t required;

    if (surface == NULL || surface->pixels == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (surface->width == 0u || surface->height == 0u ||
        surface->stride < (size_t)surface->width) {
        return CGUI_ERR_CONFIG;
    }
    if (surface->width > (uint32_t)INT32_MAX ||
        surface->height > (uint32_t)INT32_MAX ||
        surface->stride > (size_t)UINT32_MAX ||
        surface->byte_count > (size_t)UINT32_MAX) {
        return CGUI_ERR_RANGE;
    }

    required = ((uint64_t)surface->height - UINT64_C(1)) *
               (uint64_t)surface->stride + (uint64_t)surface->width;
    if (required > UINT64_C(0xffffffff)) {
        return CGUI_ERR_RANGE;
    }
    if ((uint64_t)surface->byte_count < required) {
        return CGUI_ERR_CONFIG;
    }
    return CGUI_OK;
}

static CguiResult cgui_font_descriptor_validate(const CguiFont *font)
{
    size_t expected_rows;

    if (font == NULL || font->row_bits == NULL || font->ascii_map == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (font->glyph_count == 0u || font->glyph_count > 256u ||
        font->glyph_width == 0u || font->glyph_width > 8u ||
        font->glyph_height == 0u || font->glyph_height > 8u ||
        font->advance_x < font->glyph_width ||
        font->line_advance < font->glyph_height ||
        (uint16_t)font->fallback_glyph >= font->glyph_count) {
        return CGUI_ERR_CONFIG;
    }

    expected_rows = (size_t)font->glyph_count *
                    (size_t)font->glyph_height;
    if (font->row_bits_size != expected_rows ||
        font->ascii_map_size != CGUI_ASCII_MAP_SIZE) {
        return CGUI_ERR_CONFIG;
    }
    return CGUI_OK;
}

static CguiResult cgui_font_validate(const CguiFont *font)
{
    CguiResult result;
    size_t index;

    result = cgui_font_descriptor_validate(font);
    if (result != CGUI_OK) {
        return result;
    }
    for (index = 0u; index < CGUI_ASCII_MAP_SIZE; ++index) {
        if ((uint16_t)font->ascii_map[index] >= font->glyph_count) {
            return CGUI_ERR_CONFIG;
        }
    }
    return CGUI_OK;
}

static int cgui_palette_role_valid(CguiPaletteRole role)
{
    return (int)role >= 0 && role < CGUI_PALETTE_ROLE_COUNT;
}

static CguiRect cgui_surface_rect(const CguiSurface *surface)
{
    CguiRect rect;

    rect.x0 = 0;
    rect.y0 = 0;
    rect.x1 = (int32_t)surface->width;
    rect.y1 = (int32_t)surface->height;
    return rect;
}

static CguiRect cgui_rect_intersection(CguiRect left, CguiRect right)
{
    CguiRect result;

    result.x0 = left.x0 > right.x0 ? left.x0 : right.x0;
    result.y0 = left.y0 > right.y0 ? left.y0 : right.y0;
    result.x1 = left.x1 < right.x1 ? left.x1 : right.x1;
    result.y1 = left.y1 < right.y1 ? left.y1 : right.y1;
    if (result.x0 >= result.x1 || result.y0 >= result.y1) {
        result.x0 = 0;
        result.y0 = 0;
        result.x1 = 0;
        result.y1 = 0;
    }
    return result;
}

static int cgui_rect_inside(CguiRect inner, CguiRect outer)
{
    if (inner.x0 == inner.x1 || inner.y0 == inner.y1) {
        return inner.x0 == 0 && inner.y0 == 0 &&
               inner.x1 == 0 && inner.y1 == 0;
    }
    return inner.x0 >= outer.x0 && inner.y0 >= outer.y0 &&
           inner.x1 <= outer.x1 && inner.y1 <= outer.y1;
}

static void cgui_fill_clipped(CguiContext *context,
                              CguiRect rect,
                              uint8_t color)
{
    CguiRect clipped;
    int32_t y;
    size_t count;

    clipped = cgui_rect_intersection(
        rect, context->clip_stack[context->clip_depth - 1u]);
    if (clipped.x0 == clipped.x1 || clipped.y0 == clipped.y1) {
        return;
    }

    count = (size_t)(clipped.x1 - clipped.x0);
    for (y = clipped.y0; y < clipped.y1; ++y) {
        size_t offset = (size_t)y * context->surface.stride +
                        (size_t)clipped.x0;
        memset(context->surface.pixels + offset, (int)color, count);
    }
}

static void cgui_draw_glyph(CguiContext *context,
                            int64_t x,
                            int64_t y,
                            uint8_t character,
                            uint8_t color)
{
    const CguiFont *font = &context->font;
    CguiRect clip = context->clip_stack[context->clip_depth - 1u];
    uint16_t glyph;
    uint8_t row;

    /* Saturated long-text cursors are wholly beyond the active clip. */
    if (x >= (int64_t)clip.x1 || y >= (int64_t)clip.y1) {
        return;
    }
    if (character < CGUI_ASCII_MAP_SIZE) {
        glyph = font->ascii_map[character];
    } else {
        glyph = font->fallback_glyph;
    }
    if (glyph >= font->glyph_count) {
        glyph = font->fallback_glyph;
    }

    for (row = 0u; row < font->glyph_height; ++row) {
        uint8_t bits = font->row_bits[(size_t)glyph *
                                      (size_t)font->glyph_height + row];
        uint8_t column;
        int64_t pixel_y = y + (int64_t)row;

        if (pixel_y < (int64_t)clip.y0 || pixel_y >= (int64_t)clip.y1) {
            continue;
        }
        for (column = 0u; column < font->glyph_width; ++column) {
            int64_t pixel_x;
            unsigned int shift;
            size_t offset;

            shift = (unsigned int)font->glyph_width - 1u -
                    (unsigned int)column;
            if ((bits & (uint8_t)(1u << shift)) == 0u) {
                continue;
            }
            pixel_x = x + (int64_t)column;
            if (pixel_x < (int64_t)clip.x0 ||
                pixel_x >= (int64_t)clip.x1) {
                continue;
            }
            offset = (size_t)pixel_y * context->surface.stride +
                     (size_t)pixel_x;
            context->surface.pixels[offset] = color;
        }
    }
}

static int64_t cgui_saturating_add(int64_t value, uint8_t increment)
{
    if (value > INT64_MAX - (int64_t)increment) {
        return INT64_MAX;
    }
    return value + (int64_t)increment;
}

int cgui_internal_context_valid(const CguiContext *context)
{
    CguiRect root;
    size_t index;

    if (context == NULL ||
        context->initialized_cookie != CGUI_CONTEXT_COOKIE ||
        cgui_surface_validate(&context->surface) != CGUI_OK ||
        cgui_font_descriptor_validate(&context->font) != CGUI_OK ||
        context->clip_depth == 0u ||
        context->clip_depth > CGUI_CLIP_STACK_CAPACITY) {
        return 0;
    }

    root = cgui_surface_rect(&context->surface);
    if (context->clip_stack[0].x0 != root.x0 ||
        context->clip_stack[0].y0 != root.y0 ||
        context->clip_stack[0].x1 != root.x1 ||
        context->clip_stack[0].y1 != root.y1) {
        return 0;
    }
    for (index = 1u; index < (size_t)context->clip_depth; ++index) {
        if (!cgui_internal_rect_valid(context->clip_stack[index]) ||
            !cgui_rect_inside(context->clip_stack[index],
                              context->clip_stack[index - 1u])) {
            return 0;
        }
    }
    return 1;
}

int cgui_internal_text_valid(CguiText text)
{
    return text.length <= CGUI_TEXT_BYTE_CAPACITY &&
           (text.length == 0u || text.data != NULL);
}

int cgui_internal_rect_valid(CguiRect rect)
{
    return rect.x0 <= rect.x1 && rect.y0 <= rect.y1;
}

const char *cgui_result_name(CguiResult result)
{
    switch (result) {
    case CGUI_OK:
        return "OK";
    case CGUI_ERR_ARGUMENT:
        return "ARGUMENT";
    case CGUI_ERR_CONFIG:
        return "CONFIG";
    case CGUI_ERR_RANGE:
        return "RANGE";
    case CGUI_ERR_CAPACITY:
        return "CAPACITY";
    case CGUI_ERR_DUPLICATE:
        return "DUPLICATE";
    case CGUI_ERR_NOT_FOUND:
        return "NOT_FOUND";
    case CGUI_ERR_DISABLED:
        return "DISABLED";
    case CGUI_ERR_STATE:
        return "STATE";
    default:
        return "INVALID";
    }
}

void cgui_palette_classic(CguiPalette *palette)
{
    if (palette == NULL) {
        return;
    }
    palette->index[CGUI_ROLE_DESKTOP] = 1u;
    palette->index[CGUI_ROLE_PANEL] = 7u;
    palette->index[CGUI_ROLE_BORDER_LIGHT] = 15u;
    palette->index[CGUI_ROLE_BORDER_DARK] = 8u;
    palette->index[CGUI_ROLE_TEXT] = 0u;
    palette->index[CGUI_ROLE_TEXT_DISABLED] = 8u;
    palette->index[CGUI_ROLE_SELECTION] = 1u;
    palette->index[CGUI_ROLE_SELECTION_TEXT] = 15u;
    palette->index[CGUI_ROLE_FOCUS] = 14u;
    palette->index[CGUI_ROLE_WARNING] = 12u;
    palette->index[CGUI_ROLE_MODAL_SHADE] = 0u;
}

CguiResult cgui_context_init(CguiContext *context,
                             const CguiSurface *surface,
                             const CguiFont *font,
                             const CguiPalette *palette)
{
    CguiResult result;
    CguiSurface staged_surface;
    CguiFont staged_font;
    CguiPalette staged_palette;

    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (surface == NULL || font == NULL || palette == NULL) {
        memset(context, 0, sizeof(*context));
        return CGUI_ERR_ARGUMENT;
    }

    /* Permit reinitialization from descriptors already owned by context. */
    staged_surface = *surface;
    staged_font = *font;
    staged_palette = *palette;
    memset(context, 0, sizeof(*context));

    result = cgui_surface_validate(&staged_surface);
    if (result != CGUI_OK) {
        return result;
    }
    result = cgui_font_validate(&staged_font);
    if (result != CGUI_OK) {
        return result;
    }

    context->surface = staged_surface;
    context->font = staged_font;
    context->palette = staged_palette;
    context->clip_stack[0] = cgui_surface_rect(&staged_surface);
    context->clip_depth = 1u;
    context->initialized_cookie = CGUI_CONTEXT_COOKIE;
    return CGUI_OK;
}

void cgui_context_reset(CguiContext *context)
{
    if (context != NULL) {
        memset(context, 0, sizeof(*context));
    }
}

CguiResult cgui_set_surface(CguiContext *context,
                            const CguiSurface *surface)
{
    CguiResult result;

    if (context == NULL || surface == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    result = cgui_surface_validate(surface);
    if (result != CGUI_OK) {
        return result;
    }

    context->surface = *surface;
    context->clip_stack[0] = cgui_surface_rect(surface);
    context->clip_depth = 1u;
    return CGUI_OK;
}

CguiResult cgui_set_palette(CguiContext *context,
                            const CguiPalette *palette)
{
    if (context == NULL || palette == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    context->palette = *palette;
    return CGUI_OK;
}

CguiResult cgui_clip_push(CguiContext *context, CguiRect clip)
{
    CguiRect intersection;

    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (!cgui_internal_rect_valid(clip)) {
        return CGUI_ERR_RANGE;
    }
    if (context->clip_depth >= CGUI_CLIP_STACK_CAPACITY) {
        return CGUI_ERR_CAPACITY;
    }

    intersection = cgui_rect_intersection(
        context->clip_stack[context->clip_depth - 1u], clip);
    context->clip_stack[context->clip_depth] = intersection;
    ++context->clip_depth;
    return CGUI_OK;
}

CguiResult cgui_clip_pop(CguiContext *context)
{
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (context->clip_depth <= 1u) {
        return CGUI_ERR_STATE;
    }

    --context->clip_depth;
    memset(&context->clip_stack[context->clip_depth], 0,
           sizeof(context->clip_stack[context->clip_depth]));
    return CGUI_OK;
}

void cgui_clip_reset(CguiContext *context)
{
    if (!cgui_internal_context_valid(context)) {
        return;
    }
    memset(&context->clip_stack[1], 0,
           sizeof(context->clip_stack) - sizeof(context->clip_stack[0]));
    context->clip_depth = 1u;
}

CguiResult cgui_clip_get(const CguiContext *context, CguiRect *clip)
{
    if (clip == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    memset(clip, 0, sizeof(*clip));
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }

    *clip = context->clip_stack[context->clip_depth - 1u];
    return CGUI_OK;
}

CguiResult cgui_fill_rect(CguiContext *context,
                          CguiRect rect,
                          CguiPaletteRole role)
{
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (!cgui_internal_rect_valid(rect)) {
        return CGUI_ERR_RANGE;
    }
    if (!cgui_palette_role_valid(role)) {
        return CGUI_ERR_RANGE;
    }

    cgui_fill_clipped(context, rect, context->palette.index[role]);
    return CGUI_OK;
}

CguiResult cgui_draw_border(CguiContext *context,
                            CguiRect rect,
                            CguiPaletteRole top_left,
                            CguiPaletteRole bottom_right)
{
    CguiRect edge;

    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (!cgui_internal_rect_valid(rect)) {
        return CGUI_ERR_RANGE;
    }
    if (!cgui_palette_role_valid(top_left) ||
        !cgui_palette_role_valid(bottom_right)) {
        return CGUI_ERR_RANGE;
    }
    if (rect.x0 == rect.x1 || rect.y0 == rect.y1) {
        return CGUI_OK;
    }

    edge.x0 = rect.x0;
    edge.y0 = rect.y0;
    edge.x1 = rect.x1;
    edge.y1 = rect.y0 + 1;
    cgui_fill_clipped(context, edge, context->palette.index[top_left]);

    edge.x0 = rect.x0;
    edge.y0 = rect.y0;
    edge.x1 = rect.x0 + 1;
    edge.y1 = rect.y1;
    cgui_fill_clipped(context, edge, context->palette.index[top_left]);

    edge.x0 = rect.x0;
    edge.y0 = rect.y1 - 1;
    edge.x1 = rect.x1;
    edge.y1 = rect.y1;
    cgui_fill_clipped(context, edge,
                      context->palette.index[bottom_right]);

    edge.x0 = rect.x1 - 1;
    edge.y0 = rect.y0;
    edge.x1 = rect.x1;
    edge.y1 = rect.y1;
    cgui_fill_clipped(context, edge,
                      context->palette.index[bottom_right]);
    return CGUI_OK;
}

CguiResult cgui_draw_char(CguiContext *context,
                          int32_t x,
                          int32_t y,
                          uint8_t character,
                          CguiPaletteRole role)
{
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (!cgui_palette_role_valid(role)) {
        return CGUI_ERR_RANGE;
    }

    cgui_draw_glyph(context, (int64_t)x, (int64_t)y, character,
                    context->palette.index[role]);
    return CGUI_OK;
}

CguiResult cgui_draw_text(CguiContext *context,
                          int32_t x,
                          int32_t y,
                          CguiText text,
                          CguiPaletteRole role)
{
    int64_t cursor_x;
    int64_t cursor_y;
    int64_t origin_x;
    size_t index;
    uint8_t color;

    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    if (!cgui_internal_text_valid(text)) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_palette_role_valid(role)) {
        return CGUI_ERR_RANGE;
    }

    origin_x = (int64_t)x;
    cursor_x = origin_x;
    cursor_y = (int64_t)y;
    color = context->palette.index[role];
    for (index = 0u; index < text.length; ++index) {
        uint8_t character = (uint8_t)text.data[index];

        if (character == (uint8_t)'\r') {
            continue;
        }
        if (character == (uint8_t)'\n') {
            cursor_x = origin_x;
            cursor_y = cgui_saturating_add(
                cursor_y, context->font.line_advance);
            continue;
        }
        cgui_draw_glyph(context, cursor_x, cursor_y, character, color);
        cursor_x = cgui_saturating_add(cursor_x,
                                       context->font.advance_x);
    }
    return CGUI_OK;
}

CguiResult cgui_surface_hash(const CguiContext *context, uint32_t *hash)
{
    uint32_t value;
    uint32_t y;

    if (hash == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    *hash = 0u;
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }

    value = CGUI_FNV1A_OFFSET;
    for (y = 0u; y < context->surface.height; ++y) {
        const uint8_t *row = context->surface.pixels +
                             (size_t)y * context->surface.stride;
        uint32_t x;

        for (x = 0u; x < context->surface.width; ++x) {
            value ^= row[x];
            value *= CGUI_FNV1A_PRIME;
        }
    }
    *hash = value;
    return CGUI_OK;
}
