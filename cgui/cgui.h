#ifndef CGUI_H
#define CGUI_H

#include <conio.h>
#include <dos.h>
#include <pc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nearptr.h>

#include "cgui-widgets.h"

#define SCR_W 320
#define SCR_H 200
#define MAX_WINDOWS 10

/* STRUCTS */

typedef struct {
  
    int id;
    int x, y;
    int w, h;
    char title[32];
    int visible;
    int inUse;
    int dragging;

    Widget widgets[WIDGETS_MAX];
    int widgetCount;
  
} Window;

/* GLOBALS */

extern unsigned char *VGA;
extern unsigned char *VIR_SCR;
extern int mouseX, mouseY, mouseB, prevMouseB;
extern int clipX1, clipY1, clipX2, clipY2;
extern int appRunning;
extern int windowCount, topWindowIdx, activeDropdown, isTyping;
extern unsigned long frameCount, lastClickFrame;
extern char inputBuffer[32];
extern Window windows[MAX_WINDOWS];
extern int zOrder[MAX_WINDOWS];
extern int cguiDebugMode;

/* CORE API */

void initCGUIPalette();
void initWindowManager();
int  createWindow(int x, int y, int w, int h, char *title);
void drawWindow(Window *w);
void drawRect(int x, int y, int w, int h, int color);
void drawPixel(int x, int y, int color);
void drawLine(int x0, int y0, int x1, int y1, int color);
void drawChar(int x, int y, char c, int color);
void drawString(int x, int y, char *str, int color);
void drawCursor();
void drawMenuBar();
void drawDropdown();
void destroyWindow();
void bringToFront();
void setClip(int x, int y, int w, int h);
void resetClip();
void updateMouse();
void updateKeyboard();
void updateGUI();
void renderGUI();
void waitVsync();

#endif
