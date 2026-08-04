#include "moon/cgui.h"

/*
 * Compact 3-by-5 font inherited from the prototype CGUI artwork.  The
 * explicit ASCII table below deliberately avoids locale, signed-char, and
 * strchr-dependent lookup behavior.  Glyph 62 is a retained duplicate row
 * from the prototype data and is intentionally not selected by the map.
 */
static const uint8_t cgui_font_3x5_rows[] = {
    2u, 5u, 7u, 5u, 5u, /* A */
    6u, 5u, 6u, 5u, 6u,
    3u, 4u, 4u, 4u, 3u,
    6u, 5u, 5u, 5u, 6u,
    7u, 4u, 6u, 4u, 7u,
    7u, 4u, 6u, 4u, 4u,
    3u, 4u, 5u, 5u, 3u,
    5u, 5u, 7u, 5u, 5u,
    7u, 2u, 2u, 2u, 7u,
    1u, 1u, 1u, 5u, 2u,
    5u, 6u, 4u, 6u, 5u,
    4u, 4u, 4u, 4u, 7u,
    7u, 7u, 5u, 5u, 5u,
    6u, 5u, 5u, 5u, 5u,
    2u, 5u, 5u, 5u, 2u,
    6u, 5u, 6u, 4u, 4u,
    2u, 5u, 5u, 6u, 3u,
    6u, 5u, 6u, 6u, 5u,
    3u, 4u, 2u, 1u, 6u,
    7u, 2u, 2u, 2u, 2u,
    5u, 5u, 5u, 5u, 7u,
    5u, 5u, 5u, 5u, 2u,
    5u, 5u, 5u, 7u, 5u,
    5u, 5u, 2u, 5u, 5u,
    5u, 5u, 2u, 2u, 2u,
    7u, 1u, 2u, 4u, 7u, /* Z */

    2u, 5u, 5u, 5u, 2u, /* 0 */
    2u, 6u, 2u, 2u, 7u,
    6u, 1u, 2u, 4u, 7u,
    6u, 1u, 2u, 1u, 6u,
    5u, 5u, 7u, 1u, 1u,
    7u, 4u, 6u, 1u, 6u,
    3u, 4u, 6u, 5u, 2u,
    7u, 1u, 2u, 2u, 2u,
    2u, 5u, 2u, 5u, 2u,
    2u, 5u, 3u, 1u, 6u, /* 9 */

    0u, 0u, 0u, 0u, 0u, /* space */
    0u, 0u, 0u, 0u, 2u, /* . */
    0u, 0u, 0u, 2u, 4u, /* , */
    0u, 0u, 0u, 0u, 7u, /* _ */
    2u, 2u, 2u, 0u, 2u, /* ! */
    0u, 0u, 7u, 0u, 0u, /* - */
    0u, 2u, 7u, 2u, 0u, /* + */
    0u, 7u, 0u, 7u, 0u, /* = */
    0u, 2u, 0u, 2u, 0u, /* : */
    0u, 2u, 0u, 2u, 4u, /* ; */
    1u, 1u, 2u, 4u, 4u, /* / */
    4u, 4u, 2u, 1u, 1u, /* backslash */
    2u, 4u, 4u, 4u, 2u, /* ( */
    2u, 1u, 1u, 1u, 2u, /* ) */
    6u, 4u, 4u, 4u, 6u, /* [ */
    3u, 1u, 1u, 1u, 3u, /* ] */
    1u, 2u, 4u, 2u, 1u, /* < */
    4u, 2u, 1u, 2u, 4u, /* > */
    5u, 5u, 0u, 0u, 0u, /* double quote */
    2u, 2u, 0u, 0u, 0u, /* single quote */
    7u, 1u, 2u, 0u, 2u, /* ? */
    0u, 5u, 2u, 5u, 0u, /* * */
    7u, 5u, 5u, 1u, 7u, /* @ */
    5u, 7u, 5u, 7u, 5u, /* # */
    7u, 4u, 7u, 1u, 7u, /* $ */
    5u, 1u, 2u, 4u, 5u, /* % */
    5u, 1u, 2u, 4u, 5u, /* retained duplicate */
    2u, 5u, 0u, 0u, 0u, /* ^ */
    3u, 4u, 3u, 5u, 3u, /* & */
    5u, 2u, 0u, 0u, 0u, /* ~ */
    4u, 2u, 0u, 0u, 0u, /* ` */
    3u, 2u, 6u, 2u, 3u, /* { */
    6u, 2u, 3u, 2u, 6u, /* } */
    2u, 2u, 2u, 2u, 2u, /* | */

    0u, 3u, 5u, 7u, 5u, /* a */
    4u, 4u, 6u, 5u, 6u,
    0u, 3u, 4u, 4u, 3u,
    1u, 1u, 3u, 5u, 3u,
    0u, 3u, 7u, 4u, 3u,
    3u, 2u, 7u, 2u, 2u,
    0u, 3u, 5u, 3u, 6u,
    4u, 4u, 6u, 5u, 5u,
    2u, 0u, 6u, 2u, 7u,
    1u, 0u, 1u, 1u, 6u,
    4u, 4u, 5u, 6u, 5u,
    6u, 2u, 2u, 2u, 7u,
    0u, 7u, 7u, 5u, 5u,
    0u, 6u, 5u, 5u, 5u,
    0u, 3u, 5u, 5u, 3u,
    0u, 6u, 5u, 6u, 4u,
    0u, 3u, 5u, 3u, 1u,
    0u, 3u, 4u, 4u, 4u,
    0u, 3u, 4u, 1u, 6u,
    2u, 7u, 2u, 2u, 3u,
    0u, 5u, 5u, 5u, 3u,
    0u, 5u, 5u, 2u, 2u,
    0u, 5u, 5u, 7u, 5u,
    0u, 5u, 2u, 5u, 5u,
    0u, 5u, 5u, 3u, 6u,
    0u, 7u, 1u, 2u, 7u  /* z */
};

#define CGUI_FALLBACK_GLYPH 56u

static const uint8_t cgui_font_3x5_ascii[CGUI_ASCII_MAP_SIZE] = {
    /* 00-0f */
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    /* 10-1f */
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    CGUI_FALLBACK_GLYPH, CGUI_FALLBACK_GLYPH,
    /* 20-2f */
    36u, 40u, 54u, 59u, 60u, 61u, 64u, 55u,
    48u, 49u, 57u, 42u, 38u, 41u, 37u, 46u,
    /* 30-3f */
    26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u,
    34u, 35u, 44u, 45u, 52u, 43u, 53u, 56u,
    /* 40-4f */
    58u, 0u, 1u, 2u, 3u, 4u, 5u, 6u,
    7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u,
    /* 50-5f */
    15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u,
    23u, 24u, 25u, 50u, 47u, 51u, 63u, 39u,
    /* 60-6f */
    66u, 70u, 71u, 72u, 73u, 74u, 75u, 76u,
    77u, 78u, 79u, 80u, 81u, 82u, 83u, 84u,
    /* 70-7f */
    85u, 86u, 87u, 88u, 89u, 90u, 91u, 92u,
    93u, 94u, 95u, 67u, 69u, 68u, 65u,
    CGUI_FALLBACK_GLYPH
};

static const CguiFont cgui_font_3x5 = {
    cgui_font_3x5_rows,
    cgui_font_3x5_ascii,
    sizeof(cgui_font_3x5_rows),
    sizeof(cgui_font_3x5_ascii),
    96u,
    3u,
    5u,
    4u,
    6u,
    CGUI_FALLBACK_GLYPH
};

const CguiFont *cgui_font_builtin_3x5(void)
{
    return &cgui_font_3x5;
}

#undef CGUI_FALLBACK_GLYPH
