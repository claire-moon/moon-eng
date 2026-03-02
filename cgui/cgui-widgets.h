#ifndef CGUI_WIDGETS_H
#define CGUI_WIDGETS_H

#define WIDGETS_MAX 32

typedef enum {

  WIDGET_BUTTON,
  WIDGET_LABEL,
  WIDGET_INPUT,
  WIDGET_LISTBOX,
  WIDGET_CANVAS

} WidgetType;

typedef struct Widget {

  int id;
  WidgetType type;
  int x, y, w, h;

  char text[32];
  void *data;

  int isHovered;
  int isFocused;

  void (*onClick)(struct Widget *self);
  
} Widget;

void addWidget(int winIdx, WidgetType type, int x, int y, int w, int h, char *text);
void drawWidget(Widget *w, int winX, int winY);

#endif
