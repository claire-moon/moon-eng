#include "cgui.h"

#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

void addWidget(int winIdx, WidgetType type,
               int x, int y, int w, int h, char *text) {

  Window *win;
  Widget *w_new;

  if (winIdx < 0 || winIdx >= windowCount)
    return;

  win = &windows[winIdx];

  if (win->widgetCount >= WIDGETS_MAX)
    return;

  w_new        = &win->widgets[win->widgetCount];
  w_new->id    = win->widgetCount;
  w_new->type  = type;
  w_new->x     = x;
  w_new->y     = y;
  w_new->w     = w;
  w_new->h     = h;

  if (text)
    strcpy(w_new->text, text);

  w_new->isHovered = 0;
  w_new->isFocused = 0;
  w_new->onClick   = NULL;

  win->widgetCount++;
  
}

void drawWidget(Widget *w, int winX, int winY) {

  int absX = winX + w->x;
  int absY = winY + w->y;

  if (w->type == WIDGET_LABEL) {

    drawString(absX, absY, w->text, 248);
    
  }

  else if (w->type == WIDGET_BUTTON) {

    /* BUTTON BACKGROUND */

    drawRect(absX, absY, w->w, w->h, w->isHovered ? 248 : 154);
    
    /* BUTTON BORDER */

    drawRect(absX, absY + w->w, w->h, 1, 0);
    drawRect(absX + w->w, absY, 1, w->h, 0);
    
    /* DRAW LABEL */

    drawString(absX + 4, absY + 4, w->text, 0);
    
  }
  
}
