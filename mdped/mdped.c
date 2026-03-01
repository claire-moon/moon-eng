/*

    M D P * E D I T
     M O O N  E N G


        elastic
        softworks
             2026

 */

/*
 * SPDX-License-Identifier: ACSL-1.4 OR FAFOL-0.1 OR Hippocratic-3.0
 * Multi-licensed under ACSL-1.4, FAFOL-0.1, and Hippocratic-3.0
 * See LICENSE.txt for full license texts
 */

#include <stdio.h>

#include "../cgui/cgui.h"

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

char currentMdp[32] = "zeus.mdp";

void loadPalette(const char *filename) {

    FILE *f;
    unsigned char header[54];
    unsigned char bmpPal[1024];

    int i;

    f = fopen(filename, "rb");

    if (!f) {

        return;
    }

    fread(header, 1, 54, f);

    fread(bmpPal, 1, 1024, f);

    fclose(f);

    outportb(0x3CD, 0);

    for (i = 0; i < 256; i++) {

        /* bitshifting (>> 2)
           to convert 8-bit
           color 6-bit color*/

        outportb(0x3C9, bmpPal[(i * 4) + 2] >> 2);
        outportb(0x3C9, bmpPal[(i * 4) + 1] >> 2);
        outportb(0x3C9, bmpPal[(i * 4) + 0] >> 2);
    }
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

    /* DRAW STATUS BAR & TXT ! */

    drawRect(0, 189, SCR_W, 11, 7);
    drawRect(0, 189, SCR_W, 1, 15);
    drawString(4, 192, "YOU TYPED:", 0);
    drawString(32, 192, inputBuffer, 1);

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
    loadPalette("palette.bmp");
    initWindowManager();

    /* TEST WINDOWS !!! */

    createWindow(20, 20, 150, 100, "FILE BROWSER");
    createWindow(100, 80, 120, 80, "PALETTE");

    while (appRunning) {

        updateMouse();
        updateKeyboard();

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
