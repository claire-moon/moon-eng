#ifndef CGUI_WIDGETS_H
#define CGUI_WIDGETS_H

#define WIDGETS_MAX 32

/*
   TODO:
   button done
   need button held down gfx
   still need label, input,
   canvas
*/

typedef enum {

  WIDGET_BUTTON,
  WIDGET_TOGGLE,
  WIDGET_CHECKBOX,
  WIDGET_LABEL,
  WIDGET_INPUT,
  WIDGET_LISTBOX,
  WIDGET_SLIDER,
  WIDGET_CANVAS

} WidgetType;

typedef struct Widget {

  int id;
  WidgetType type;
  int x, y, w, h;

  char text[32];
  
  int val;
  int min;
  int max;

  void *data;

  /* STATES */
  
  int isHovered;
  int isFocused;
  int isPressed;
  int isChecked;

  /* LISTBOX */
  
  int listCount;
  int listScroll;
  int listSelected;

  char listItems[32][64];

  void (*onClick)(struct Widget *self);
  
} Widget;

void addWidget(int winIdx, WidgetType type, int x, int y, int w, int h, char *text);
void drawWidget(Widget *w, int winX, int winY);

#endif
