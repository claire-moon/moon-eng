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
#define MAX_MENU_CATEGORIES 8
#define MAX_MENU_ITEMS 10

typedef enum {

  STATUS_EMPTY,
  STATUS_STRING,
  STATUS_CLOCK,
  STATUS_DATE,
  STATUS_DYNAMIC

} StatusType;

typedef struct {

    StatusType type;
    char text[64];
    char *dynPtr;
    int isClickable;
    void (*onClick)(void);
    int x, w;

} StatusItem;

typedef struct {

    char name[32];
    void (*onClick)(void);
    int isTogglable;
    int isChecked;

} MenuItem;

typedef struct {

    char name[32];
    MenuItem items[MAX_MENU_ITEMS];
    int itemCount, x, w;
    int dropW;

} MenuCategory;

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
    int titleColor;

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
extern MenuCategory sysMenu[MAX_MENU_CATEGORIES];
extern int sysMenuCount;
extern int sysDebugZ;
extern int sysBgMode;
extern int sysBgColor;
extern unsigned char sysBgGradient[200];

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
int  addMenuCategory(char *name);
void addMenuItem(int catIdx, char *name, void (*onClick)(void));
void addMenuItemToggle(int catIdx, char *name, void (*onClick)(void));
void drawDebugOverlay();
void drawStatusBar(void);
void setStatusLeft(StatusType type, char *text, char *dynPtr, int isClickable, void(*onClick)(void));
void setStatusRight(StatusType type, char *text, char *dynPtr, int isClickable, void(*onClick)(void));
void cguiToggleDebug();
void cguiExit(void);
void setBackgroundColor(int color);
void setBackgroundGradient(int c1, int c2);
void drawDesktop(void);
void drawBorder(int x, int y, int w, int h,
                int cHi, int cLo);
void packWindow(int winIdx);
int cguiMsgBox(char *title, char *msg);

#endif
