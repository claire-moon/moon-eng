#include "moon/cgui.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n",                           \
                    __FILE__, __LINE__, #condition);                         \
            return 0;                                                        \
        }                                                                    \
    } while (0)

#define TEST_GUARD_SIZE 32u
#define TEST_STORAGE_BYTES 8256u
#define TEST_SENTINEL 0xd7u
#define FNV1A_OFFSET UINT32_C(2166136261)
#define FNV1A_PRIME UINT32_C(16777619)

typedef struct TestFixture {
    uint8_t storage[TEST_STORAGE_BYTES];
    CguiSurface surface;
    CguiPalette palette;
    CguiContext context;
} TestFixture;

typedef struct ReplayOutcome {
    uint32_t trace_hash;
    uint32_t surface_hash;
    CguiId focus_id;
    CguiModalKind modal_kind;
    size_t event_count;
} ReplayOutcome;

static CguiText text_span(const char *data, size_t length)
{
    CguiText text;

    text.data = data;
    text.length = length;
    return text;
}

static CguiRect make_rect(int32_t x0, int32_t y0,
                          int32_t x1, int32_t y1)
{
    CguiRect rect;

    rect.x0 = x0;
    rect.y0 = y0;
    rect.x1 = x1;
    rect.y1 = y1;
    return rect;
}

static int rect_equal(CguiRect left, CguiRect right)
{
    return left.x0 == right.x0 && left.y0 == right.y0 &&
           left.x1 == right.x1 && left.y1 == right.y1;
}

static int menu_equal(const CguiMenuState *left,
                      const CguiMenuState *right)
{
    size_t index;

    if (left->item_count != right->item_count ||
        left->owner_id != right->owner_id ||
        left->focus_id != right->focus_id) {
        return 0;
    }
    for (index = 0u; index < left->item_count; ++index) {
        if (left->items[index].id != right->items[index].id ||
            left->items[index].label.data != right->items[index].label.data ||
            left->items[index].label.length !=
                right->items[index].label.length ||
            left->items[index].enabled != right->items[index].enabled) {
            return 0;
        }
    }
    return 1;
}

static int modal_observable_equal(const CguiModalState *left,
                                  const CguiModalState *right)
{
    return left->kind == right->kind && left->focus_id == right->focus_id;
}

static int fixture_init(TestFixture *fixture,
                        uint32_t width,
                        uint32_t height,
                        size_t stride,
                        uint8_t visible_value)
{
    size_t used;
    uint32_t y;

    CHECK(fixture != NULL);
    CHECK(stride >= (size_t)width);
    CHECK(height != 0u);
    CHECK(stride <= (TEST_STORAGE_BYTES - TEST_GUARD_SIZE) /
                    (size_t)height);

    used = stride * (size_t)height;
    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->storage, (int)TEST_SENTINEL,
           sizeof(fixture->storage));
    fixture->surface.pixels = fixture->storage + TEST_GUARD_SIZE;
    fixture->surface.width = width;
    fixture->surface.height = height;
    fixture->surface.stride = stride;
    fixture->surface.byte_count = used;

    for (y = 0u; y < height; ++y) {
        memset(fixture->surface.pixels + (size_t)y * stride,
               (int)visible_value, (size_t)width);
    }

    cgui_palette_classic(&fixture->palette);
    CHECK(cgui_context_init(&fixture->context,
                            &fixture->surface,
                            cgui_font_builtin_3x5(),
                            &fixture->palette) == CGUI_OK);
    return 1;
}

static void fixture_clear_visible(TestFixture *fixture, uint8_t value)
{
    uint32_t y;

    for (y = 0u; y < fixture->surface.height; ++y) {
        memset(fixture->surface.pixels +
                   (size_t)y * fixture->surface.stride,
               (int)value,
               (size_t)fixture->surface.width);
    }
}

static uint8_t fixture_pixel(const TestFixture *fixture,
                             uint32_t x,
                             uint32_t y)
{
    return fixture->surface.pixels[
        (size_t)y * fixture->surface.stride + (size_t)x];
}

static int fixture_guards_intact(const TestFixture *fixture)
{
    size_t used;
    size_t index;
    uint32_t y;

    used = fixture->surface.stride * (size_t)fixture->surface.height;
    for (index = 0u; index < TEST_GUARD_SIZE; ++index) {
        if (fixture->storage[index] != TEST_SENTINEL) {
            return 0;
        }
    }
    for (index = TEST_GUARD_SIZE + used;
         index < sizeof(fixture->storage); ++index) {
        if (fixture->storage[index] != TEST_SENTINEL) {
            return 0;
        }
    }
    for (y = 0u; y < fixture->surface.height; ++y) {
        const uint8_t *row;

        row = fixture->surface.pixels +
              (size_t)y * fixture->surface.stride;
        for (index = (size_t)fixture->surface.width;
             index < fixture->surface.stride; ++index) {
            if (row[index] != TEST_SENTINEL) {
                return 0;
            }
        }
    }
    return 1;
}

static uint32_t visible_fnv(const CguiSurface *surface)
{
    uint32_t hash;
    uint32_t y;

    hash = FNV1A_OFFSET;
    for (y = 0u; y < surface->height; ++y) {
        const uint8_t *row;
        uint32_t x;

        row = surface->pixels + (size_t)y * surface->stride;
        for (x = 0u; x < surface->width; ++x) {
            hash ^= row[x];
            hash *= FNV1A_PRIME;
        }
    }
    return hash;
}

static void trace_u32(uint32_t *hash, uint32_t value)
{
    unsigned int shift;

    for (shift = 0u; shift < 32u; shift += 8u) {
        *hash ^= (uint8_t)(value >> shift);
        *hash *= FNV1A_PRIME;
    }
}

static void input_clear(CguiInputFrame *input)
{
    memset(input, 0, sizeof(*input));
}

static void input_press(CguiInputFrame *input, CguiAction action)
{
    input->actions[action].pressed = 1u;
}

static void input_repeat(CguiInputFrame *input, CguiAction action)
{
    input->actions[action].repeat = 1u;
}

static int event_equal(const CguiEvent *event,
                       CguiEventType type,
                       CguiId owner_id,
                       CguiId item_id)
{
    return event->type == type && event->owner_id == owner_id &&
           event->item_id == item_id;
}

static int update_expect(CguiContext *context,
                         const CguiInputFrame *input,
                         CguiEventType type,
                         CguiId owner_id,
                         CguiId item_id)
{
    CguiEvent event;

    memset(&event, 0xa5, sizeof(event));
    CHECK(cgui_update(context, input, &event) == CGUI_OK);
    CHECK(event_equal(&event, type, owner_id, item_id));
    return 1;
}

static void make_custom_font(CguiFont *font)
{
    static const uint8_t rows[] = {
        7u, 1u, 2u, 0u, 2u, /* fallback question mark */
        2u, 5u, 7u, 5u, 5u, /* A */
        6u, 5u, 6u, 5u, 6u, /* B */
        0u, 0u, 0u, 0u, 0u  /* space */
    };
    static uint8_t ascii_map[CGUI_ASCII_MAP_SIZE];
    size_t index;

    for (index = 0u; index < CGUI_ASCII_MAP_SIZE; ++index) {
        ascii_map[index] = 0u;
    }
    ascii_map[(uint8_t)'A'] = 1u;
    ascii_map[(uint8_t)'B'] = 2u;
    ascii_map[(uint8_t)'x'] = 2u;
    ascii_map[(uint8_t)' '] = 3u;

    font->row_bits = rows;
    font->ascii_map = ascii_map;
    font->row_bits_size = sizeof(rows);
    font->ascii_map_size = sizeof(ascii_map);
    font->glyph_count = 4u;
    font->glyph_width = 3u;
    font->glyph_height = 5u;
    font->advance_x = 4u;
    font->line_advance = 6u;
    font->fallback_glyph = 0u;
}

static int test_initialization_and_transactionality(void)
{
    TestFixture fixture;
    CguiContext context;
    CguiSurface surface;
    CguiSurface old_surface;
    CguiFont font;
    CguiFont bad_font;
    CguiPalette palette;
    CguiRect old_clip;
    CguiRect clip;
    uint8_t pixels[64];
    uint8_t bad_map[CGUI_ASCII_MAP_SIZE];
    size_t index;

    CHECK(strcmp(cgui_result_name(CGUI_OK), "OK") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_ARGUMENT), "ARGUMENT") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_CONFIG), "CONFIG") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_RANGE), "RANGE") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_CAPACITY), "CAPACITY") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_DUPLICATE), "DUPLICATE") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_NOT_FOUND), "NOT_FOUND") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_DISABLED), "DISABLED") == 0);
    CHECK(strcmp(cgui_result_name(CGUI_ERR_STATE), "STATE") == 0);
    CHECK(strcmp(cgui_result_name((CguiResult)99), "INVALID") == 0);

    memset(&palette, 0xa5, sizeof(palette));
    cgui_palette_classic(&palette);
    CHECK(palette.index[CGUI_ROLE_DESKTOP] == 1u);
    CHECK(palette.index[CGUI_ROLE_PANEL] == 7u);
    CHECK(palette.index[CGUI_ROLE_BORDER_LIGHT] == 15u);
    CHECK(palette.index[CGUI_ROLE_BORDER_DARK] == 8u);
    CHECK(palette.index[CGUI_ROLE_TEXT] == 0u);
    CHECK(palette.index[CGUI_ROLE_TEXT_DISABLED] == 8u);
    CHECK(palette.index[CGUI_ROLE_SELECTION] == 1u);
    CHECK(palette.index[CGUI_ROLE_SELECTION_TEXT] == 15u);
    CHECK(palette.index[CGUI_ROLE_FOCUS] == 14u);
    CHECK(palette.index[CGUI_ROLE_WARNING] == 12u);
    CHECK(palette.index[CGUI_ROLE_MODAL_SHADE] == 0u);
    cgui_palette_classic(NULL);

    CHECK(cgui_font_builtin_3x5() != NULL);
    CHECK(cgui_font_builtin_3x5()->glyph_width == 3u);
    CHECK(cgui_font_builtin_3x5()->glyph_height == 5u);
    CHECK(cgui_font_builtin_3x5()->advance_x == 4u);
    CHECK(cgui_font_builtin_3x5()->line_advance == 6u);
    CHECK(cgui_font_builtin_3x5()->ascii_map_size == CGUI_ASCII_MAP_SIZE);

    memset(pixels, 0, sizeof(pixels));
    surface.pixels = pixels;
    surface.width = 4u;
    surface.height = 4u;
    surface.stride = 6u;
    surface.byte_count = 24u;
    font = *cgui_font_builtin_3x5();

    CHECK(cgui_context_init(NULL, &surface, &font, &palette) ==
          CGUI_ERR_ARGUMENT);
    memset(&context, 0xa5, sizeof(context));
    CHECK(cgui_context_init(&context, NULL, &font, &palette) ==
          CGUI_ERR_ARGUMENT);
    CHECK(context.initialized_cookie == 0u);
    CHECK(cgui_context_init(&context, &surface, NULL, &palette) ==
          CGUI_ERR_ARGUMENT);
    CHECK(context.initialized_cookie == 0u);
    CHECK(cgui_context_init(&context, &surface, &font, NULL) ==
          CGUI_ERR_ARGUMENT);
    CHECK(context.initialized_cookie == 0u);

    surface.pixels = NULL;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_ARGUMENT);
    CHECK(context.initialized_cookie == 0u);
    surface.pixels = pixels;
    surface.width = 0u;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_CONFIG);
    surface.width = 4u;
    surface.height = 0u;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_CONFIG);
    surface.height = 4u;
    surface.stride = 3u;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_CONFIG);
    surface.stride = 6u;
    surface.byte_count = 21u;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_CONFIG);
    surface.byte_count = 24u;
    surface.width = (uint32_t)INT32_MAX + 1u;
    surface.stride = (size_t)surface.width;
    surface.byte_count = (size_t)surface.width;
    CHECK(cgui_context_init(&context, &surface, &font, &palette) ==
          CGUI_ERR_RANGE);

    surface.width = 4u;
    surface.stride = 6u;
    surface.byte_count = 24u;
    bad_font = font;
    bad_font.row_bits = NULL;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_ARGUMENT);
    bad_font = font;
    bad_font.ascii_map = NULL;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_ARGUMENT);
    bad_font = font;
    bad_font.glyph_count = 0u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.glyph_width = 9u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.glyph_height = 9u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.advance_x = 2u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.line_advance = 4u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.row_bits_size -= 1u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.ascii_map_size -= 1u;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);
    bad_font = font;
    bad_font.fallback_glyph = (uint8_t)bad_font.glyph_count;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);

    memcpy(bad_map, font.ascii_map, sizeof(bad_map));
    for (index = 0u; index < sizeof(bad_map); ++index) {
        if ((uint16_t)bad_map[index] < font.glyph_count) {
            bad_map[index] = (uint8_t)font.glyph_count;
            break;
        }
    }
    bad_font = font;
    bad_font.ascii_map = bad_map;
    CHECK(cgui_context_init(&context, &surface, &bad_font, &palette) ==
          CGUI_ERR_CONFIG);

    CHECK(fixture_init(&fixture, 8u, 6u, 11u, 0u));
    CHECK(cgui_context_init(&fixture.context,
                            &fixture.context.surface,
                            &fixture.context.font,
                            &fixture.context.palette) == CGUI_OK);
    CHECK(fixture.context.surface.pixels == fixture.surface.pixels);
    CHECK(fixture.context.surface.width == 8u);
    CHECK(fixture.context.font.glyph_width == 3u);
    CHECK(fixture.context.palette.index[CGUI_ROLE_PANEL] == 7u);
    old_surface = fixture.context.surface;
    CHECK(cgui_clip_get(&fixture.context, &old_clip) == CGUI_OK);
    surface = fixture.surface;
    surface.byte_count = 1u;
    CHECK(cgui_set_surface(&fixture.context, &surface) == CGUI_ERR_CONFIG);
    CHECK(fixture.context.surface.pixels == old_surface.pixels);
    CHECK(fixture.context.surface.width == old_surface.width);
    CHECK(fixture.context.surface.height == old_surface.height);
    CHECK(fixture.context.surface.stride == old_surface.stride);
    CHECK(fixture.context.surface.byte_count == old_surface.byte_count);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, old_clip));
    CHECK(cgui_set_surface(NULL, &fixture.surface) == CGUI_ERR_ARGUMENT);
    CHECK(cgui_set_surface(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);
    CHECK(cgui_set_palette(NULL, &palette) == CGUI_ERR_ARGUMENT);
    CHECK(cgui_set_palette(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);

    cgui_context_reset(&fixture.context);
    CHECK(cgui_fill_rect(&fixture.context,
                         make_rect(0, 0, 1, 1),
                         CGUI_ROLE_PANEL) == CGUI_ERR_STATE);
    cgui_context_reset(NULL);
    return 1;
}

static int test_exclusive_nested_clipping_and_guards(void)
{
    TestFixture fixture;
    CguiRect clip;
    CguiRect before;
    uint32_t x;
    uint32_t y;
    size_t index;

    CHECK(fixture_init(&fixture, 8u, 6u, 11u, 0u));
    fixture.palette.index[CGUI_ROLE_WARNING] = 22u;
    fixture.palette.index[CGUI_ROLE_FOCUS] = 33u;
    fixture.palette.index[CGUI_ROLE_DESKTOP] = 44u;
    CHECK(cgui_set_palette(&fixture.context, &fixture.palette) == CGUI_OK);

    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, make_rect(0, 0, 8, 6)));
    CHECK(cgui_clip_pop(&fixture.context) == CGUI_ERR_STATE);
    memset(&clip, 0xa5, sizeof(clip));
    CHECK(cgui_clip_get(NULL, &clip) == CGUI_ERR_ARGUMENT);
    CHECK(rect_equal(clip, make_rect(0, 0, 0, 0)));
    CHECK(cgui_clip_get(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);

    CHECK(cgui_clip_push(&fixture.context, make_rect(1, 1, 7, 5)) ==
          CGUI_OK);
    CHECK(cgui_clip_push(&fixture.context,
                         make_rect(3, INT32_MIN, INT32_MAX, 4)) ==
          CGUI_OK);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, make_rect(3, 1, 7, 4)));
    CHECK(cgui_fill_rect(&fixture.context,
                         make_rect(INT32_MIN, INT32_MIN,
                                   INT32_MAX, INT32_MAX),
                         CGUI_ROLE_WARNING) == CGUI_OK);

    CHECK(cgui_clip_push(&fixture.context,
                         make_rect(INT32_MIN, 2, 5, INT32_MAX)) ==
          CGUI_OK);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, make_rect(3, 2, 5, 4)));
    CHECK(cgui_fill_rect(&fixture.context,
                         make_rect(INT32_MIN, INT32_MIN,
                                   INT32_MAX, INT32_MAX),
                         CGUI_ROLE_FOCUS) == CGUI_OK);

    for (y = 0u; y < 6u; ++y) {
        for (x = 0u; x < 8u; ++x) {
            uint8_t expected;

            expected = 0u;
            if (x >= 3u && x < 7u && y >= 1u && y < 4u) {
                expected = 22u;
            }
            if (x >= 3u && x < 5u && y >= 2u && y < 4u) {
                expected = 33u;
            }
            CHECK(fixture_pixel(&fixture, x, y) == expected);
        }
    }

    CHECK(cgui_clip_pop(&fixture.context) == CGUI_OK);
    CHECK(cgui_clip_get(&fixture.context, &before) == CGUI_OK);
    CHECK(cgui_clip_push(&fixture.context,
                         make_rect(4, 4, 4, INT32_MAX)) == CGUI_OK);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, make_rect(0, 0, 0, 0)));
    CHECK(cgui_fill_rect(&fixture.context,
                         make_rect(INT32_MIN, INT32_MIN,
                                   INT32_MAX, INT32_MAX),
                         CGUI_ROLE_DESKTOP) == CGUI_OK);
    CHECK(cgui_clip_pop(&fixture.context) == CGUI_OK);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, before));

    CHECK(cgui_clip_push(&fixture.context, make_rect(5, 0, 4, 1)) ==
          CGUI_ERR_RANGE);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, before));

    cgui_clip_reset(&fixture.context);
    for (index = 1u; index < CGUI_CLIP_STACK_CAPACITY; ++index) {
        CHECK(cgui_clip_push(&fixture.context,
                             make_rect(INT32_MIN, INT32_MIN,
                                       INT32_MAX, INT32_MAX)) == CGUI_OK);
    }
    CHECK(cgui_clip_push(&fixture.context, make_rect(0, 0, 1, 1)) ==
          CGUI_ERR_CAPACITY);
    cgui_clip_reset(&fixture.context);
    CHECK(cgui_clip_get(&fixture.context, &clip) == CGUI_OK);
    CHECK(rect_equal(clip, make_rect(0, 0, 8, 6)));

    fixture_clear_visible(&fixture, 0u);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(0, 0, 1, 1),
                         CGUI_ROLE_DESKTOP) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 0u, 0u) == 44u);
    CHECK(fixture_pixel(&fixture, 1u, 0u) == 0u);
    CHECK(fixture_pixel(&fixture, 0u, 1u) == 0u);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(2, 2, 2, 5),
                         CGUI_ROLE_DESKTOP) == CGUI_OK);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(2, 4, 1, 5),
                         CGUI_ROLE_DESKTOP) == CGUI_ERR_RANGE);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(0, 0, 8, 6),
                         (CguiPaletteRole)CGUI_PALETTE_ROLE_COUNT) ==
          CGUI_ERR_RANGE);
    CHECK(fixture_guards_intact(&fixture));
    return 1;
}

static int test_borders_palette_and_visible_hash(void)
{
    TestFixture fixture;
    CguiPalette remapped;
    uint32_t hash;
    uint32_t x;
    uint32_t y;

    CHECK(fixture_init(&fixture, 8u, 6u, 11u, 99u));
    remapped = fixture.palette;
    remapped.index[CGUI_ROLE_BORDER_LIGHT] = 11u;
    remapped.index[CGUI_ROLE_BORDER_DARK] = 22u;
    remapped.index[CGUI_ROLE_PANEL] = 33u;
    CHECK(cgui_set_palette(&fixture.context, &remapped) == CGUI_OK);
    CHECK(cgui_draw_border(&fixture.context, make_rect(1, 1, 5, 4),
                           CGUI_ROLE_BORDER_LIGHT,
                           CGUI_ROLE_BORDER_DARK) == CGUI_OK);

    for (y = 0u; y < 6u; ++y) {
        for (x = 0u; x < 8u; ++x) {
            uint8_t expected;

            expected = 99u;
            if (x >= 1u && x < 5u && y == 1u) {
                expected = 11u;
            }
            if (x == 1u && y >= 1u && y < 4u) {
                expected = 11u;
            }
            if (x >= 1u && x < 5u && y == 3u) {
                expected = 22u;
            }
            if (x == 4u && y >= 1u && y < 4u) {
                expected = 22u;
            }
            CHECK(fixture_pixel(&fixture, x, y) == expected);
        }
    }

    CHECK(cgui_draw_border(&fixture.context, make_rect(6, 1, 7, 5),
                           CGUI_ROLE_BORDER_LIGHT,
                           CGUI_ROLE_BORDER_DARK) == CGUI_OK);
    for (y = 1u; y < 5u; ++y) {
        CHECK(fixture_pixel(&fixture, 6u, y) == 22u);
    }
    CHECK(cgui_draw_border(&fixture.context, make_rect(0, 5, 4, 6),
                           CGUI_ROLE_BORDER_LIGHT,
                           CGUI_ROLE_BORDER_DARK) == CGUI_OK);
    for (x = 0u; x < 4u; ++x) {
        CHECK(fixture_pixel(&fixture, x, 5u) == 22u);
    }
    CHECK(cgui_draw_border(&fixture.context, make_rect(2, 2, 2, 2),
                           CGUI_ROLE_BORDER_LIGHT,
                           CGUI_ROLE_BORDER_DARK) == CGUI_OK);
    CHECK(cgui_draw_border(&fixture.context, make_rect(3, 2, 2, 3),
                           CGUI_ROLE_BORDER_LIGHT,
                           CGUI_ROLE_BORDER_DARK) == CGUI_ERR_RANGE);

    fixture_clear_visible(&fixture, 0u);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(0, 0, 2, 2),
                         CGUI_ROLE_PANEL) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 0u, 0u) == 33u);
    remapped.index[CGUI_ROLE_PANEL] = 77u;
    CHECK(cgui_set_palette(&fixture.context, &remapped) == CGUI_OK);
    CHECK(cgui_fill_rect(&fixture.context, make_rect(2, 0, 4, 2),
                         CGUI_ROLE_PANEL) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 2u, 0u) == 77u);
    CHECK(fixture_pixel(&fixture, 0u, 0u) == 33u);

    CHECK(fixture_init(&fixture, 4u, 3u, 7u, 0u));
    for (y = 0u; y < 3u; ++y) {
        for (x = 0u; x < 4u; ++x) {
            fixture.surface.pixels[(size_t)y * fixture.surface.stride + x] =
                (uint8_t)(y * 4u + x + 1u);
        }
    }
    hash = 0u;
    CHECK(cgui_surface_hash(&fixture.context, &hash) == CGUI_OK);
    CHECK(hash == UINT32_C(0x551d7a95));
    CHECK(hash == visible_fnv(&fixture.surface));
    CHECK(fixture_guards_intact(&fixture));

    hash = UINT32_MAX;
    CHECK(cgui_surface_hash(NULL, &hash) == CGUI_ERR_ARGUMENT);
    CHECK(hash == 0u);
    CHECK(cgui_surface_hash(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);
    return 1;
}

static int test_font_map_newline_and_custom_font(void)
{
    TestFixture fixture;
    CguiFont font;
    CguiText text;
    uint32_t before;
    uint32_t after;
    static const char sequence[] = { 'A', '\r', 'B', '\n', 'x' };

    CHECK(fixture_init(&fixture, 24u, 20u, 29u, 0u));
    make_custom_font(&font);
    CHECK(cgui_context_init(&fixture.context,
                            &fixture.surface,
                            &font,
                            &fixture.palette) == CGUI_OK);
    fixture.palette.index[CGUI_ROLE_TEXT] = 9u;
    CHECK(cgui_set_palette(&fixture.context, &fixture.palette) == CGUI_OK);

    CHECK(cgui_draw_char(&fixture.context, 1, 1, (uint8_t)'A',
                         CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 2u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 1u, 2u) == 9u);
    CHECK(fixture_pixel(&fixture, 3u, 2u) == 9u);
    CHECK(fixture_pixel(&fixture, 2u, 2u) == 0u);
    CHECK(fixture_pixel(&fixture, 1u, 3u) == 9u);
    CHECK(fixture_pixel(&fixture, 2u, 3u) == 9u);
    CHECK(fixture_pixel(&fixture, 3u, 3u) == 9u);

    /* The custom ASCII map maps x to the B glyph. */
    CHECK(cgui_draw_char(&fixture.context, 6, 1, (uint8_t)'x',
                         CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 6u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 7u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 8u, 1u) == 0u);

    /* High bytes and unmapped controls use the configured fallback glyph. */
    CHECK(cgui_draw_char(&fixture.context, 11, 1, UINT8_C(0x80),
                         CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(fixture_pixel(&fixture, 11u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 12u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 13u, 1u) == 9u);
    CHECK(fixture_pixel(&fixture, 13u, 2u) == 9u);

    text = text_span(sequence, sizeof(sequence));
    CHECK(cgui_draw_text(&fixture.context, 0, 7, text,
                         CGUI_ROLE_TEXT) == CGUI_OK);
    /* CR does not advance: B begins one ordinary advance after A. */
    CHECK(fixture_pixel(&fixture, 4u, 7u) == 9u);
    CHECK(fixture_pixel(&fixture, 5u, 7u) == 9u);
    /* LF resets X and advances by the custom line advance of six. */
    CHECK(fixture_pixel(&fixture, 0u, 13u) == 9u);
    CHECK(fixture_pixel(&fixture, 1u, 13u) == 9u);

    text = text_span(NULL, 0u);
    CHECK(cgui_draw_text(&fixture.context, 0, 0, text,
                         CGUI_ROLE_TEXT) == CGUI_OK);
    text = text_span(NULL, 1u);
    CHECK(cgui_draw_text(&fixture.context, 0, 0, text,
                         CGUI_ROLE_TEXT) == CGUI_ERR_ARGUMENT);

    CHECK(cgui_surface_hash(&fixture.context, &before) == CGUI_OK);
    CHECK(cgui_draw_char(&fixture.context, INT32_MIN, INT32_MAX,
                         (uint8_t)'A', CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(cgui_draw_text(&fixture.context, INT32_MAX, INT32_MIN,
                         text_span("AB", 2u), CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(cgui_draw_char(&fixture.context, 0, 0, (uint8_t)'A',
                         (CguiPaletteRole)-1) == CGUI_ERR_RANGE);
    CHECK(cgui_surface_hash(&fixture.context, &after) == CGUI_OK);
    CHECK(after == before);
    CHECK(fixture_guards_intact(&fixture));
    return 1;
}

static void sample_menu_items(CguiMenuItem items[4])
{
    items[0].id = UINT32_C(7);
    items[0].label = text_span("ONE", 3u);
    items[0].enabled = 1u;
    items[1].id = UINT32_C(4000000000);
    items[1].label = text_span("DISABLED", 8u);
    items[1].enabled = 0u;
    items[2].id = UINT32_C(42);
    items[2].label = text_span("THREE", 5u);
    items[2].enabled = 1u;
    items[3].id = UINT32_C(9001);
    items[3].label = text_span("FOUR", 4u);
    items[3].enabled = 1u;
}

static int test_menu_capacity_sparse_ids_and_focus(void)
{
    TestFixture fixture;
    CguiMenuItem items[CGUI_MENU_ITEM_CAPACITY + 1u];
    CguiMenuItem sample[4];
    CguiMenuItem reordered[3];
    CguiMenuState before;
    size_t index;

    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));
    sample_menu_items(sample);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(7));
    CHECK(fixture.context.menu.owner_id == UINT32_C(100));

    before = fixture.context.menu;
    CHECK(cgui_menu_set_items(&fixture.context, CGUI_ID_NONE,
                              sample, 4u) == CGUI_ERR_ARGUMENT);
    CHECK(menu_equal(&fixture.context.menu, &before));
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              NULL, 1u) == CGUI_ERR_ARGUMENT);
    CHECK(menu_equal(&fixture.context.menu, &before));

    sample[0].id = CGUI_ID_NONE;
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_ERR_ARGUMENT);
    CHECK(menu_equal(&fixture.context.menu, &before));
    sample_menu_items(sample);
    sample[0].label = text_span(NULL, 1u);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_ERR_CONFIG);
    CHECK(menu_equal(&fixture.context.menu, &before));
    sample_menu_items(sample);
    sample[0].enabled = 2u;
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_ERR_CONFIG);
    CHECK(menu_equal(&fixture.context.menu, &before));
    sample_menu_items(sample);
    sample[3].id = sample[0].id;
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_ERR_DUPLICATE);
    CHECK(menu_equal(&fixture.context.menu, &before));

    for (index = 0u; index < CGUI_MENU_ITEM_CAPACITY + 1u; ++index) {
        items[index].id = (CguiId)(index + 1u);
        items[index].label = text_span("X", 1u);
        items[index].enabled = 1u;
    }
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100), items,
                              CGUI_MENU_ITEM_CAPACITY + 1u) ==
          CGUI_ERR_CAPACITY);
    CHECK(menu_equal(&fixture.context.menu, &before));
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(200), items,
                              CGUI_MENU_ITEM_CAPACITY) == CGUI_OK);
    CHECK(fixture.context.menu.item_count == CGUI_MENU_ITEM_CAPACITY);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(1));

    sample_menu_items(sample);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_OK);
    CHECK(cgui_menu_set_focus(&fixture.context,
                              UINT32_C(4000000000)) == CGUI_ERR_DISABLED);
    CHECK(cgui_menu_set_focus(&fixture.context,
                              UINT32_C(123456)) == CGUI_ERR_NOT_FOUND);
    CHECK(cgui_menu_set_focus(&fixture.context, CGUI_ID_NONE) ==
          CGUI_ERR_ARGUMENT);
    CHECK(cgui_menu_set_focus(&fixture.context, UINT32_C(42)) == CGUI_OK);

    reordered[0] = sample[3];
    reordered[1] = sample[2];
    reordered[2] = sample[0];
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              reordered, 3u) == CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              reordered, 1u) == CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(101),
                              reordered, 3u) == CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));

    sample_menu_items(sample);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              sample, 4u) == CGUI_OK);
    CHECK(cgui_menu_set_enabled(&fixture.context, UINT32_C(7), 0) ==
          CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));
    CHECK(cgui_menu_set_enabled(&fixture.context, UINT32_C(42), 0) ==
          CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));
    CHECK(cgui_menu_set_enabled(&fixture.context, UINT32_C(9001), 0) ==
          CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == CGUI_ID_NONE);
    CHECK(cgui_menu_set_enabled(&fixture.context,
                                UINT32_C(4000000000), 2) == CGUI_OK);
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(4000000000));
    CHECK(cgui_menu_set_enabled(&fixture.context, CGUI_ID_NONE, 1) ==
          CGUI_ERR_ARGUMENT);
    CHECK(cgui_menu_set_enabled(&fixture.context, UINT32_C(123456), 1) ==
          CGUI_ERR_NOT_FOUND);

    cgui_menu_clear(&fixture.context);
    CHECK(cgui_menu_focus(&fixture.context) == CGUI_ID_NONE);
    CHECK(fixture.context.menu.item_count == 0u);
    CHECK(fixture.context.menu.owner_id == CGUI_ID_NONE);
    CHECK(cgui_draw_menu(&fixture.context, make_rect(0, 0, 20, 20)) ==
          CGUI_ERR_STATE);
    return 1;
}

static int test_menu_input_priority_and_accelerators(void)
{
    TestFixture fixture;
    CguiMenuItem items[4];
    CguiInputFrame input;

    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));
    sample_menu_items(items);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              items, 4u) == CGUI_OK);

    input_clear(&input);
    input_press(&input, CGUI_ACTION_ESCAPE);
    input_press(&input, CGUI_ACTION_F1);
    input_press(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    input.accelerator_id = UINT32_C(42);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_MENU_CANCEL,
                        UINT32_C(100), CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(7));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_F1);
    input_press(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    input.accelerator_id = UINT32_C(42);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_HELP_REQUEST,
                        UINT32_C(100), CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(7));

    input_clear(&input);
    input.accelerator_id = UINT32_C(42);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_MENU_ACTIVATE,
                        UINT32_C(100), UINT32_C(42)));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));

    input_clear(&input);
    input.accelerator_id = UINT32_C(4000000000);
    input_press(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));

    input_clear(&input);
    input.accelerator_id = UINT32_C(123456);
    input_press(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));

    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_DOWN);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));
    input_clear(&input);
    input_press(&input, CGUI_ACTION_DOWN);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(7));
    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_UP);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_UP);
    input_repeat(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(9001));

    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    input_clear(&input);
    input_press(&input, CGUI_ACTION_SPACE);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_MENU_ACTIVATE,
                        UINT32_C(100), UINT32_C(9001)));

    CHECK(cgui_draw_menu(&fixture.context, make_rect(2, 2, 52, 40)) ==
          CGUI_OK);
    CHECK(fixture_guards_intact(&fixture));
    return 1;
}

static CguiHelpSpec make_help_spec(void)
{
    CguiHelpSpec spec;

    spec.owner_id = UINT32_C(700);
    spec.title = text_span("HELP ME!", 8u);
    spec.body = text_span("UP AND DOWN MOVE.\nENTER SELECTS.", 32u);
    spec.close_label = text_span("CLOSE", 5u);
    return spec;
}

static CguiConfirmSpec make_confirm_spec(void)
{
    CguiConfirmSpec spec;

    spec.owner_id = UINT32_C(800);
    spec.yes_id = UINT32_C(801);
    spec.no_id = UINT32_C(802);
    spec.title = text_span("CONFIRM", 7u);
    spec.message = text_span("DO THE DANGEROUS THING?", 23u);
    spec.yes_label = text_span("YES", 3u);
    spec.no_label = text_span("NO", 2u);
    return spec;
}

static int test_help_modal_ownership(void)
{
    TestFixture fixture;
    CguiMenuItem items[4];
    CguiHelpSpec help;
    CguiHelpSpec invalid;
    CguiConfirmSpec confirm;
    CguiInputFrame input;
    CguiId background_focus;

    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));
    sample_menu_items(items);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              items, 4u) == CGUI_OK);
    help = make_help_spec();
    confirm = make_confirm_spec();

    invalid = help;
    invalid.owner_id = CGUI_ID_NONE;
    CHECK(cgui_help_open(&fixture.context, &invalid) == CGUI_ERR_CONFIG);
    CHECK(cgui_modal_kind(&fixture.context) == CGUI_MODAL_NONE);
    invalid = help;
    invalid.body = text_span(NULL, 1u);
    CHECK(cgui_help_open(&fixture.context, &invalid) == CGUI_ERR_CONFIG);
    CHECK(cgui_help_open(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);

    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_OK);
    CHECK(cgui_help_is_open(&fixture.context));
    CHECK(cgui_modal_kind(&fixture.context) == CGUI_MODAL_HELP);
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_ERR_STATE);
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_ERR_STATE);
    background_focus = cgui_menu_focus(&fixture.context);

    input_clear(&input);
    input_press(&input, CGUI_ACTION_DOWN);
    input.accelerator_id = UINT32_C(42);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_help_is_open(&fixture.context));
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);
    CHECK(cgui_draw_menu(&fixture.context, make_rect(2, 2, 52, 40)) ==
          CGUI_OK);
    CHECK(cgui_draw_help(&fixture.context, make_rect(6, 4, 74, 56)) ==
          CGUI_OK);
    CHECK(fixture_guards_intact(&fixture));

    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_SPACE);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_help_is_open(&fixture.context));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_F1);
    input.accelerator_id = UINT32_C(42);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_HELP_CLOSE,
                        UINT32_C(700), CGUI_ID_NONE));
    CHECK(!cgui_help_is_open(&fixture.context));
    CHECK(cgui_modal_kind(&fixture.context) == CGUI_MODAL_NONE);
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);
    CHECK(cgui_help_close(&fixture.context) == CGUI_ERR_STATE);

    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_OK);
    input_clear(&input);
    input_press(&input, CGUI_ACTION_ESCAPE);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_HELP_CLOSE,
                        UINT32_C(700), CGUI_ID_NONE));
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_OK);
    input_clear(&input);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_HELP_CLOSE,
                        UINT32_C(700), CGUI_ID_NONE));
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);
    return 1;
}

static int test_confirmation_exclusive_and_deliberate(void)
{
    TestFixture fixture;
    CguiMenuItem items[4];
    CguiConfirmSpec confirm;
    CguiConfirmSpec invalid;
    CguiHelpSpec help;
    CguiInputFrame input;
    CguiId background_focus;

    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));
    sample_menu_items(items);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              items, 4u) == CGUI_OK);
    confirm = make_confirm_spec();
    help = make_help_spec();

    invalid = confirm;
    invalid.owner_id = CGUI_ID_NONE;
    CHECK(cgui_confirm_open(&fixture.context, &invalid) ==
          CGUI_ERR_CONFIG);
    invalid = confirm;
    invalid.yes_id = invalid.no_id;
    CHECK(cgui_confirm_open(&fixture.context, &invalid) ==
          CGUI_ERR_CONFIG);
    invalid = confirm;
    invalid.yes_label = text_span(NULL, 1u);
    CHECK(cgui_confirm_open(&fixture.context, &invalid) ==
          CGUI_ERR_CONFIG);
    CHECK(cgui_confirm_open(&fixture.context, NULL) == CGUI_ERR_ARGUMENT);

    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    CHECK(cgui_confirm_is_open(&fixture.context));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);
    CHECK(cgui_modal_kind(&fixture.context) == CGUI_MODAL_CONFIRM);
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_ERR_STATE);
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_ERR_STATE);
    background_focus = cgui_menu_focus(&fixture.context);

    /* Repeats cannot accidentally accept the default-No confirmation. */
    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_confirm_is_open(&fixture.context));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);

    /* F1 and background accelerators are modal-owned and never leak. */
    input_clear(&input);
    input_press(&input, CGUI_ACTION_F1);
    input_press(&input, CGUI_ACTION_UP);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);
    input_clear(&input);
    input.accelerator_id = UINT32_C(42);
    input_press(&input, CGUI_ACTION_UP);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);

    input_clear(&input);
    input_press(&input, CGUI_ACTION_UP);
    input_repeat(&input, CGUI_ACTION_DOWN);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);

    input_clear(&input);
    input_repeat(&input, CGUI_ACTION_UP);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_NONE, CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.yes_id);
    CHECK(cgui_draw_menu(&fixture.context, make_rect(2, 2, 52, 40)) ==
          CGUI_OK);
    CHECK(cgui_draw_confirm(&fixture.context, make_rect(6, 4, 74, 56)) ==
          CGUI_OK);
    CHECK(fixture_guards_intact(&fixture));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_CONFIRM_YES,
                        confirm.owner_id, confirm.yes_id));
    CHECK(!cgui_confirm_is_open(&fixture.context));
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);

    /* Every open begins safely on No; Space explicitly accepts that choice. */
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);
    input_clear(&input);
    input_press(&input, CGUI_ACTION_SPACE);
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_CONFIRM_NO,
                        confirm.owner_id, confirm.no_id));

    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    input_clear(&input);
    input.accelerator_id = confirm.yes_id;
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_CONFIRM_YES,
                        confirm.owner_id, confirm.yes_id));
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    input_clear(&input);
    input.accelerator_id = confirm.no_id;
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_CONFIRM_NO,
                        confirm.owner_id, confirm.no_id));

    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    input_clear(&input);
    input_press(&input, CGUI_ACTION_ESCAPE);
    input.accelerator_id = confirm.yes_id;
    CHECK(update_expect(&fixture.context, &input,
                        CGUI_EVENT_CONFIRM_CANCEL,
                        confirm.owner_id, CGUI_ID_NONE));
    CHECK(!cgui_confirm_is_open(&fixture.context));

    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    CHECK(cgui_confirm_close(&fixture.context) == CGUI_OK);
    CHECK(cgui_confirm_close(&fixture.context) == CGUI_ERR_STATE);
    CHECK(cgui_modal_kind(&fixture.context) == CGUI_MODAL_NONE);
    CHECK(cgui_menu_focus(&fixture.context) == background_focus);
    return 1;
}

static int test_text_capacity_and_modal_self_aliasing(void)
{
    static char boundary[CGUI_TEXT_BYTE_CAPACITY + 1u];
    TestFixture fixture;
    CguiMenuItem item;
    CguiMenuState menu_before;
    CguiModalState modal_before;
    CguiHelpSpec help;
    CguiConfirmSpec confirm;
    uint32_t before_hash;
    uint32_t after_hash;
    size_t index;

    for (index = 0u; index < sizeof(boundary); ++index) {
        boundary[index] = 'A';
    }
    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));

    CHECK(cgui_draw_text(
              &fixture.context, 0, 0,
              text_span(boundary, CGUI_TEXT_BYTE_CAPACITY),
              CGUI_ROLE_TEXT) == CGUI_OK);
    CHECK(cgui_surface_hash(&fixture.context, &before_hash) == CGUI_OK);
    CHECK(cgui_draw_text(
              &fixture.context, 0, 0,
              text_span(boundary, CGUI_TEXT_BYTE_CAPACITY + 1u),
              CGUI_ROLE_TEXT) == CGUI_ERR_ARGUMENT);
    CHECK(cgui_surface_hash(&fixture.context, &after_hash) == CGUI_OK);
    CHECK(after_hash == before_hash);
    CHECK(fixture_guards_intact(&fixture));

    item.id = UINT32_C(3001);
    item.label = text_span(boundary, CGUI_TEXT_BYTE_CAPACITY);
    item.enabled = 1u;
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(3000),
                              &item, 1u) == CGUI_OK);
    menu_before = fixture.context.menu;
    item.label = text_span(boundary, CGUI_TEXT_BYTE_CAPACITY + 1u);
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(3000),
                              &item, 1u) == CGUI_ERR_CONFIG);
    CHECK(menu_equal(&fixture.context.menu, &menu_before));

    help = make_help_spec();
    help.body = text_span(boundary, CGUI_TEXT_BYTE_CAPACITY);
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_OK);
    CHECK(cgui_help_close(&fixture.context) == CGUI_OK);
    modal_before = fixture.context.modal;
    help.body = text_span(boundary, CGUI_TEXT_BYTE_CAPACITY + 1u);
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_ERR_CONFIG);
    CHECK(modal_observable_equal(&fixture.context.modal, &modal_before));

    confirm = make_confirm_spec();
    confirm.message = text_span(boundary, CGUI_TEXT_BYTE_CAPACITY);
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    CHECK(cgui_confirm_close(&fixture.context) == CGUI_OK);
    modal_before = fixture.context.modal;
    confirm.message = text_span(boundary,
                                CGUI_TEXT_BYTE_CAPACITY + 1u);
    CHECK(cgui_confirm_open(&fixture.context, &confirm) ==
          CGUI_ERR_CONFIG);
    CHECK(modal_observable_equal(&fixture.context.modal, &modal_before));

    /* The public descriptor may deliberately reside in the destination
       union. Opening must stage it before clearing that union. */
    help = make_help_spec();
    fixture.context.modal.spec.help = help;
    CHECK(cgui_help_open(&fixture.context,
                         &fixture.context.modal.spec.help) == CGUI_OK);
    CHECK(cgui_help_is_open(&fixture.context));
    CHECK(fixture.context.modal.spec.help.owner_id == help.owner_id);
    CHECK(fixture.context.modal.spec.help.title.data == help.title.data);
    CHECK(fixture.context.modal.spec.help.title.length == help.title.length);
    CHECK(cgui_help_close(&fixture.context) == CGUI_OK);

    confirm = make_confirm_spec();
    fixture.context.modal.spec.confirm = confirm;
    CHECK(cgui_confirm_open(&fixture.context,
                            &fixture.context.modal.spec.confirm) == CGUI_OK);
    CHECK(cgui_confirm_is_open(&fixture.context));
    CHECK(fixture.context.modal.spec.confirm.owner_id == confirm.owner_id);
    CHECK(fixture.context.modal.spec.confirm.yes_id == confirm.yes_id);
    CHECK(fixture.context.modal.spec.confirm.no_id == confirm.no_id);
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.no_id);
    CHECK(cgui_confirm_close(&fixture.context) == CGUI_OK);
    CHECK(fixture_guards_intact(&fixture));
    return 1;
}

static int replay_render(TestFixture *fixture)
{
    CguiRect full;
    CguiRect menu_bounds;
    CguiRect modal_bounds;
    CguiResult result;

    full = make_rect(0, 0,
                     (int32_t)fixture->surface.width,
                     (int32_t)fixture->surface.height);
    menu_bounds = make_rect(3, 3, 53, 41);
    modal_bounds = make_rect(7, 5, 73, 56);
    result = cgui_fill_rect(&fixture->context, full, CGUI_ROLE_DESKTOP);
    if (result != CGUI_OK) {
        return 0;
    }
    result = cgui_draw_menu(&fixture->context, menu_bounds);
    if (result != CGUI_OK) {
        return 0;
    }
    if (cgui_modal_kind(&fixture->context) == CGUI_MODAL_HELP) {
        result = cgui_draw_help(&fixture->context, modal_bounds);
    } else if (cgui_modal_kind(&fixture->context) == CGUI_MODAL_CONFIRM) {
        result = cgui_draw_confirm(&fixture->context, modal_bounds);
    }
    return result == CGUI_OK;
}

static int replay_step(TestFixture *fixture,
                       CguiInputFrame *input,
                       uint32_t *trace,
                       size_t *event_count,
                       CguiEvent *event)
{
    uint32_t surface_hash;

    memset(event, 0xa5, sizeof(*event));
    CHECK(cgui_update(&fixture->context, input, event) == CGUI_OK);
    if (event->type != CGUI_EVENT_NONE) {
        ++*event_count;
    }
    CHECK(replay_render(fixture));
    CHECK(cgui_surface_hash(&fixture->context, &surface_hash) == CGUI_OK);
    trace_u32(trace, (uint32_t)event->type);
    trace_u32(trace, event->owner_id);
    trace_u32(trace, event->item_id);
    trace_u32(trace, surface_hash);
    return 1;
}

static int run_replay(ReplayOutcome *outcome)
{
    TestFixture fixture;
    CguiMenuItem items[4];
    CguiHelpSpec help;
    CguiConfirmSpec confirm;
    CguiInputFrame input;
    CguiEvent event;
    uint32_t trace;
    size_t event_count;

    CHECK(outcome != NULL);
    CHECK(fixture_init(&fixture, 80u, 60u, 83u, 0u));
    sample_menu_items(items);
    help = make_help_spec();
    confirm = make_confirm_spec();
    CHECK(cgui_menu_set_items(&fixture.context, UINT32_C(100),
                              items, 4u) == CGUI_OK);
    trace = FNV1A_OFFSET;
    event_count = 0u;

    input_clear(&input);
    input_press(&input, CGUI_ACTION_DOWN);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(cgui_menu_focus(&fixture.context) == UINT32_C(42));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_F1);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(event.type == CGUI_EVENT_HELP_REQUEST);
    CHECK(cgui_help_open(&fixture.context, &help) == CGUI_OK);
    CHECK(replay_render(&fixture));

    input_clear(&input);
    input.accelerator_id = UINT32_C(9001);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(event.type == CGUI_EVENT_NONE);
    CHECK(cgui_help_is_open(&fixture.context));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_ENTER);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(event.type == CGUI_EVENT_HELP_CLOSE);

    input_clear(&input);
    input.accelerator_id = UINT32_C(9001);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(event.type == CGUI_EVENT_MENU_ACTIVATE);
    CHECK(cgui_confirm_open(&fixture.context, &confirm) == CGUI_OK);
    CHECK(replay_render(&fixture));

    input_clear(&input);
    input_press(&input, CGUI_ACTION_UP);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(cgui_confirm_focus(&fixture.context) == confirm.yes_id);

    input_clear(&input);
    input_press(&input, CGUI_ACTION_SPACE);
    CHECK(replay_step(&fixture, &input, &trace, &event_count, &event));
    CHECK(event.type == CGUI_EVENT_CONFIRM_YES);

    CHECK(fixture_guards_intact(&fixture));
    CHECK(cgui_surface_hash(&fixture.context, &outcome->surface_hash) ==
          CGUI_OK);
    outcome->trace_hash = trace;
    outcome->focus_id = cgui_menu_focus(&fixture.context);
    outcome->modal_kind = cgui_modal_kind(&fixture.context);
    outcome->event_count = event_count;
    return 1;
}

static int test_fresh_event_output_and_deterministic_replay(void)
{
    TestFixture fixture;
    CguiInputFrame input;
    CguiEvent event;
    CguiContext invalid_context;
    ReplayOutcome first;
    ReplayOutcome second;

    CHECK(fixture_init(&fixture, 8u, 6u, 11u, 0u));
    input_clear(&input);
    memset(&event, 0xa5, sizeof(event));
    CHECK(cgui_update(&fixture.context, &input, &event) == CGUI_OK);
    CHECK(event_equal(&event, CGUI_EVENT_NONE,
                      CGUI_ID_NONE, CGUI_ID_NONE));

    event.type = CGUI_EVENT_CONFIRM_YES;
    event.owner_id = UINT32_MAX;
    event.item_id = UINT32_MAX;
    CHECK(cgui_update(&fixture.context, NULL, &event) ==
          CGUI_ERR_ARGUMENT);
    CHECK(event_equal(&event, CGUI_EVENT_NONE,
                      CGUI_ID_NONE, CGUI_ID_NONE));
    memset(&invalid_context, 0, sizeof(invalid_context));
    event.type = CGUI_EVENT_CONFIRM_YES;
    event.owner_id = UINT32_MAX;
    event.item_id = UINT32_MAX;
    CHECK(cgui_update(&invalid_context, &input, &event) == CGUI_ERR_STATE);
    CHECK(event_equal(&event, CGUI_EVENT_NONE,
                      CGUI_ID_NONE, CGUI_ID_NONE));
    CHECK(cgui_update(&fixture.context, &input, NULL) ==
          CGUI_ERR_ARGUMENT);

    CHECK(run_replay(&first));
    CHECK(run_replay(&second));
    CHECK(first.trace_hash == second.trace_hash);
    CHECK(first.surface_hash == second.surface_hash);
    CHECK(first.focus_id == second.focus_id);
    CHECK(first.modal_kind == second.modal_kind);
    CHECK(first.event_count == second.event_count);
    CHECK(first.focus_id == UINT32_C(9001));
    CHECK(first.modal_kind == CGUI_MODAL_NONE);
    CHECK(first.event_count == 4u);

    /* These hashes are intentionally frozen after the complete scripted
       replay; a change requires an explicit visual-contract review. */
    CHECK(first.trace_hash == UINT32_C(0x0df475d8));
    CHECK(first.surface_hash == UINT32_C(0x1637a23b));
    return 1;
}

int main(void)
{
    int passed;
    int total;

    passed = 0;
    total = 0;

#define RUN_TEST(test_function)                                              \
    do {                                                                     \
        ++total;                                                             \
        if (test_function()) {                                               \
            ++passed;                                                        \
        }                                                                    \
    } while (0)

    RUN_TEST(test_initialization_and_transactionality);
    RUN_TEST(test_exclusive_nested_clipping_and_guards);
    RUN_TEST(test_borders_palette_and_visible_hash);
    RUN_TEST(test_font_map_newline_and_custom_font);
    RUN_TEST(test_menu_capacity_sparse_ids_and_focus);
    RUN_TEST(test_menu_input_priority_and_accelerators);
    RUN_TEST(test_help_modal_ownership);
    RUN_TEST(test_confirmation_exclusive_and_deliberate);
    RUN_TEST(test_text_capacity_and_modal_self_aliasing);
    RUN_TEST(test_fresh_event_output_and_deterministic_replay);

#undef RUN_TEST

    if (passed != total) {
        fprintf(stderr, "CGUI core tests: FAIL (%d/%d)\n", passed, total);
        return 1;
    }
    printf("CGUI core tests: PASS\n");
    return 0;
}
