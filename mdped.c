/*
 *    M D P * E D I T
 *     M O O N  E N G
 */

#include <conio.h>
#include <dos.h>
#include <pc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nearptr.h>

#define SCR_W 320
#define SCR_H 200
#define MAX_WINDOWS 10

/* STRUCTS */

typedef struct {

    char magic[4];
    int numLumps;
    int dirOffset;

} MoonHeader;

typedef struct {

    int offset;
    int size;
    char name[16];

} MoonEntry;

typedef struct {

    int id;
    int x, y;
    int w, h;
    char title[32];
    int visible;
    int dragging;

} Window;

/* GLOBALS */

unsigned char *VGA = (unsigned char *)0xA0000;
unsigned char *VIR_SCR = NULL;

int mouseX = 160, mouseY = 100, mouseB = 0;
int appRunning = 1;
char currentMdp[32] = "zeus.mdp";

int prevMouseB = 0;
int windowCount = 0;
int topWindowIdx = -1;

int activeDropdown = 0;

unsigned long frameCount = 0;
unsigned long lastClickFrame = 0;

Window windows[MAX_WINDOWS];

void drawDropdown();

/* MOUSE DRIVER */

void updateMouse() {

    union REGS r;
    r.x.ax = 3;
    int86(0x33, &r, &r);

    mouseX = (short)r.x.cx / 2;
    mouseY = (short)r.x.dx;

    prevMouseB = mouseB;

    mouseB = r.x.bx;

    if (mouseX < 0)
        mouseX = 0;

    if (mouseX > 319)
        mouseX = 319;

    if (mouseY < 0)
        mouseY = 0;

    if (mouseY > 199)
        mouseY = 199;
}

/* GFX PRIMATIVES */

void drawRect(int x, int y, int w, int h, int color) {

    int i, j;

    for (j = 0; j < h; j++) {

        if (y + j < 0 || y + j >= SCR_H)
            continue;

        for (i = 0; i < w; i++) {

            if (x + i < 0 || x + i >= SCR_W)
                continue;

            VIR_SCR[(y + j) * SCR_W + (x + i)] = color;
        }
    }
}

void drawChar(int x, int y, char c, int color) {

    unsigned char font[37][5] = {

        {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6},
        {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5},
        {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2}, {5, 6, 4, 6, 5}, {4, 4, 4, 4, 7},
        {7, 7, 5, 5, 5}, {6, 5, 5, 5, 5}, {2, 5, 5, 5, 2}, {6, 5, 6, 4, 4},
        {2, 5, 5, 6, 3}, {6, 5, 6, 6, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2},
        {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 5, 7, 5}, {5, 5, 2, 5, 5},
        {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}, {2, 5, 5, 5, 2}, {2, 6, 2, 2, 7},
        {6, 1, 2, 4, 7}, {6, 1, 2, 1, 6}, {5, 5, 7, 1, 1}, {7, 4, 6, 1, 6},
        {3, 4, 6, 5, 2}, {7, 1, 2, 2, 2}, {2, 5, 2, 5, 2}, {2, 5, 3, 1, 6},
        {0, 0, 0, 0, 0}};

    int idx = 36;
    int i, j, row;

    if (c >= 'A' && c <= 'z')
        idx = c - 'A';

    else if (c >= 'a' && c <= 'z')
        idx = c - 'a';

    else if (c >= '0' && c <= '9')
        idx = c - '0' + 26;

    /* DRAW THE DANG PIXELS ! */

    for (j = 0; j < 5; j++) {

        row = font[idx][j];

        for (i = 0; i < 3; i++) {

            if (row & (1 << (2 - i))) {

                if (x + i >= 0 && x + i < SCR_W && y + j >= 0 &&
                    y + j < SCR_H) {

                    VIR_SCR[(y + j) * SCR_W + (x + i)] = color;
                }
            }
        }
    }
}

void drawString(int x, int y, char *str, int color) {

    int i = 0;

    while (str[i] != '\0') {

        drawChar(x + (i * 4), y, str[i], color);

        i++;
    }
}

void initWindowManager() {

    int i;

    for (i = 0; i < MAX_WINDOWS; i++)
        windows[i].visible = 0;
}

int createWindow(int x, int y, int w, int h, char *title) {

    int idx = windowCount;

    if (idx >= MAX_WINDOWS)
        return -1;

    windows[idx].id = idx;
    windows[idx].x = x;
    windows[idx].y = y;
    windows[idx].w = w;
    windows[idx].h = h;

    strcpy(windows[idx].title, title);

    windows[idx].visible = 1;
    windows[idx].dragging = 0;

    windowCount++;

    return idx;
}

void drawWindow(Window *w) {

    if (!w->visible)
        return;

    /* MAIN BODY */

    drawRect(w->x, w->y, w->w, w->h, 7);

    /* BORDER */

    drawRect(w->x, w->y, w->w, 1, 15);
    drawRect(w->x, w->y, 1, w->h, 15);
    drawRect(w->x, w->y + w->h - 1, w->w, 1, 0);
    drawRect(w->x + w->w - 1, w->y, 1, w->h, 0);

    /* TITLE BAR */

    drawRect(w->x + 2, w->y + 2, w->w - 4, 10, 1);

    /* CLOSE BUTTON */

    drawRect(w->x + w->w - 12, w->y + 3, 8, 8, 4);

    /* TITLE TEXT */

    drawString(w->x + 4, w->y + 4, w->title, 15);
}

void drawCursor() {

    int x = mouseX;
    int y = mouseY;
    int i;

    for (i = -3; i <= 3; i++) {

        if (x + i >= 0 && x + i < SCR_W)
            VIR_SCR[y * SCR_W + (x + i)] = 15;

        if (y + i >= 0 && y + i < SCR_H)
            VIR_SCR[(y + i) * SCR_W + x] = 15;
    }
}

void waitVsync() {

    while (inportb(0x3DA) & 8)
        ;
    while (!(inportb(0x3DA) & 8))
        ;
}

/* SYSTEM VARS */

void initMDPED() {

    /* MODE 13 INNIT */

    union REGS r;
    r.h.ah = 0x00;
    r.h.al = 0x13;
    int86(0x10, &r, &r);

    /* MOUSE INNIT */

    r.x.ax = 0;
    int86(0x33, &r, &r);

    /* VERT BOUNDARY */

    r.x.ax = 8;
    r.x.cx = 0;
    r.x.dx = 199;

    int86(0x33, &r, &r);

    /* HORZ BOUNDARY */

    r.x.ax = 7;
    r.x.cx = 0;
    r.x.dx = 639;

    int86(0x33, &r, &r);

    /* CENTER MOUSE */

    r.x.ax = 4;
    r.x.cx = 320;
    r.x.dx = 100;

    int86(0x33, &r, &r);
}

void updateGUI() {

    int i, j;

    Window *w;
    Window temp;

    int click = (mouseB & 1) && !(prevMouseB & 1);
    int hold = (mouseB & 1);
    int release = !(mouseB & 1) && (prevMouseB & 1);
    int doubleClick = 0;

    /* DOUBLE CLICK TRACKER */

    frameCount++;

    if (click) {

        if (frameCount - lastClickFrame < 15) {

            doubleClick = 1;

            lastClickFrame = 0;

        } else {

            lastClickFrame = frameCount;
        }
    }

    /* DRAG HANDLER */

    for (i = 0; i < windowCount; i++) {

        if (windows[i].dragging) {

            if (!hold) {

                windows[i].dragging = 0;

            } else {

                windows[i].x = mouseX - (windows[i].w / 2);
                windows[i].y = mouseY - 5;
            }
        }
    }

    /* DROPDOWNS */

    if (click) {

        if (activeDropdown) {

            if (activeDropdown == 1) {

                if (mouseX >= 4 && mouseX <= 94 && mouseY >= 11 &&
                    mouseY <= 41) {

                    if (mouseY > 23) {

                        /* TODO: IMPORTING PAL */
                    } else {
                        appRunning = 0;
                    }
                }
            }

            activeDropdown = 0;

            return;
        }

        if (mouseY <= 11) {

            /* MENU BAR */

            if (mouseX >= 4 && mouseX <= 30)
                activeDropdown = 1;
            if (mouseX >= 36 && mouseX <= 62)
                activeDropdown = 2;
            if (mouseX >= 72 && mouseX <= 98)
                activeDropdown = 3;

            return;
        }

        /* WINDOW CHECKER */

        for (i = windowCount - 1; i >= 0; i--) {

            w = &windows[i];

            if (!w->visible)
                continue;

            /* FIXED BOUNADRIES */

            if (mouseX >= w->x && mouseX <= w->x + w->w && mouseY >= w->y &&
                mouseY < w->y + w->h) {

                /* Z - ORDER */

                if (i != windowCount - 1) {

                    temp = windows[i];

                    for (j = i; j < windowCount - 1; j++) {

                        windows[j] = windows[j + 1];
                    }

                    windows[windowCount - 1] = temp;
                    w = &windows[windowCount - 1];
                }

                /* CLOSE BUTTON CLICKED */

                if (mouseX > w->x + w->w - 12 && mouseY < w->y + 12) {

                    w->visible = 0;
                }

                /* TITLE BAR CLICKED */

                else if (mouseY < w->y + 12) {

                    if (doubleClick) {

                        strcpy(w->title, "THAT TICKLES..!");
                    }

                    w->dragging = 1;
                }

                return;
            }
        }
    }
}
void drawMenuBar() {

    /* BACKGROUND BAR */

    drawRect(0, 0, SCR_W, 11, 7);

    /* SHADOW */

    drawRect(0, 11, SCR_W, 1, 0);

    /* MENU ITEMS */

    drawString(8, 4, "FILE", 0);
    drawString(40, 4, "EDIT", 0);
    drawString(72, 4, "VIEW", 0);
}

void renderGUI() {

    int i;

    waitVsync();
    waitVsync();

    memset(VIR_SCR, 3, 64000);

    /* DRAW WINDOWS ! */

    for (i = 0; i < windowCount; i++) {

        drawWindow(&windows[i]);
    }

    drawMenuBar();

    drawDropdown();

    /* DRAW MOUSE ! */

    for (i = -3; i <= 3; i++) {

        if (mouseX + i >= 0 && mouseX + i < SCR_W)
            VIR_SCR[mouseY * SCR_W + (mouseX + i)] = 15;

        if (mouseY + i >= 0 && mouseY + i < SCR_H)
            VIR_SCR[(mouseY + i) * SCR_W + mouseX] = 15;
    }

    /* FLIP BUFFER */

    memcpy((void *)(__djgpp_conventional_base + 0xA0000), VIR_SCR, 64000);
}

void drawDropdown() {

    if (activeDropdown == 0)
        return;

    if (activeDropdown == 1) {

        /* FILE MENU */

        drawRect(4, 11, 90, 30, 7);
        drawRect(4, 11, 90, 1, 15);
        drawRect(4, 11, 1, 30, 15);
        drawRect(4, 40, 90, 1, 0);
        drawRect(93, 11, 1, 30, 0);

        drawString(10, 16, "IMPORT PAL", 0);
        drawString(10, 26, "EXIT", 0);
    }
}

int main() {

    if (__djgpp_nearptr_enable() == 0)
        return 1;

    VIR_SCR = (unsigned char *)malloc(64000);

    union REGS r;

    initMDPED();
    initWindowManager();

    /* TEST WINDOWS !!! */

    createWindow(20, 20, 150, 100, "FILE BROWSER");
    createWindow(100, 80, 120, 80, "PALETTE");

    while (appRunning) {

        updateMouse();

        /* RIGHT CLICK EXIT (change this later) */

        if (mouseB & 2)
            appRunning = 0;
        updateGUI();
        renderGUI();
    }

    r.h.ah = 0x00;
    r.h.al = 0x03;

    int86(0x10, &r, &r);

    return 0;
}
