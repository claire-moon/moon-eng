#include "cgui.h"
#include "cgui-pal.h"

/* GLOBALS */

unsigned char *VGA = (unsigned char *)0xA0000;
unsigned char *VIR_SCR = NULL;

int mouseX = 160, mouseY = 100, mouseB = 0;
int appRunning = 1;

int prevMouseB = 0;
int windowCount = 0;
int topWindowIdx = -1;

int activeDropdown = 0;

int isTyping = 1;

unsigned long frameCount = 0;
unsigned long lastClickFrame = 0;

char inputBuffer[32];

Window windows[MAX_WINDOWS];

void initCGUIPalette() {

  int i;

  outportb(0x3C8, 0);

  for (i = 0; i < 768; i++) {

    outportb(0x3C9, cgui_palette[i]);
    
  }
  
}

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

void updateKeyboard() {

    if (!isTyping)
        return;

    /* KEYBOARD LOGIC */

    if (kbhit()) {

        char c = getch();

        int len = strlen(inputBuffer);

        /* BACKSPACE */

        if (c == 8 && len > 0) {

            inputBuffer[len - 1] = '\0';
        }

        /* ENTER */

        else if (c == 13) {

        }

        /* GENERAL TYPE */

        else if (c >= 31 && c <= 126 && len < 31) {

            inputBuffer[len] = c;
            inputBuffer[len + 1] = '\0';
        }
    }
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

    drawRect(w->x, w->y, w->w, w->h, 154);

    /* BORDER */

    drawRect(w->x, w->y, w->w, 1, 248);
    drawRect(w->x, w->y, 1, w->h, 248);
    drawRect(w->x, w->y + w->h - 1, w->w, 1, 0);
    drawRect(w->x + w->w - 1, w->y, 1, w->h, 0);

    /* TITLE BAR */

    drawRect(w->x + 2, w->y + 2, w->w - 4, 10, 1);

    /* CLOSE BUTTON */

    drawRect(w->x + w->w - 12, w->y + 3, 8, 8, 4);

    /* TITLE TEXT */

    drawString(w->x + 4, w->y + 4, w->title, 248);
}

void drawCursor() {

    int x = mouseX;
    int y = mouseY;
    int i;

    for (i = -3; i <= 3; i++) {

        if (x + i >= 0 && x + i < SCR_W)
            VIR_SCR[y * SCR_W + (x + i)] = 248;

        if (y + i >= 0 && y + i < SCR_H)
            VIR_SCR[(y + i) * SCR_W + x] = 248;
    }
}

void waitVsync() {

    while (inportb(0x3DA) & 8)
        ;
    while (!(inportb(0x3DA) & 8))
        ;
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
