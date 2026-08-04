#ifndef MOON_CGUI_INTERNAL_H
#define MOON_CGUI_INTERNAL_H

#include "moon/cgui.h"

#define CGUI_CONTEXT_COOKIE UINT32_C(0x43475549)

int cgui_internal_context_valid(const CguiContext *context);
int cgui_internal_text_valid(CguiText text);
int cgui_internal_rect_valid(CguiRect rect);

#endif
