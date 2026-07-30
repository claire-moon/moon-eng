/*

      M D P e d i t
     M O O N  E N G


        C  G
         M O O N
           2 0 2 6

 */

/*
 * SPDX-License-Identifier: ACSL-1.4 OR FAFOL-0.1 OR Hippocratic-3.0
 * Multi-licensed under ACSL-1.4, FAFOL-0.1, and Hippocratic-3.0
 * See LICENSE.txt for full license texts
 */

#include <stdio.h>
#include <dir.h>
#include <time.h>

#include "../cgui/cgui.h"

#define TICKS_PER_SECOND 60
#define TICK_INTERVAL (UCLOCKS_PER_SEC / TICKS_PER_SECOND)

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

/* GLOBALS  */

char currentMdp[32] = "zeus.mdp";
unsigned char testPixels[1024];
cguiImage testImg = {32, 32, testPixels};

unsigned char mapData[256] = {0};
cguiGrid mapGrid = {16, 16, 8, -1, -1, mapData};

/* FUNCS */

void loadPalette(const char *filename) {

    FILE *f;
    unsigned char header[54];
    unsigned char bmpPal[1024];

    unsigned int dibSize;

    int i;

    f = fopen(filename, "rb");

    if (!f) {

        return;
    }

    fread(header, 1, 18, f);

    dibSize = header[14] | (header[15] << 8) | (header[16] << 16) |
              (header[17] << 24);

    fseek(f, 14 + dibSize, SEEK_SET);

    fread(bmpPal, 1, 1024, f);

    fclose(f);

    outportb(0x3C8, 0);

    for (i = 0; i < 256; i++) {

        /* bitshifting (>> 2)
           to convert 8-bit
           color 6-bit color*/

        outportb(0x3C9, bmpPal[(i * 4) + 2] >> 2);
        outportb(0x3C9, bmpPal[(i * 4) + 1] >> 2);
        outportb(0x3C9, bmpPal[(i * 4) + 0] >> 2);
    }
}

void initMDPED() {

    /* temporary test for image canvas */

    int i, x, y;

    for (i = 0; i < 1024; i++) {

        x = i % 32;
        y = i / 32;

        testPixels[i] = ((x / 8) + (y / 8)) % 2 ? 5 : 8;

    }

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

void renderGUI() {

    int i;

    drawDesktop();

    /* DRAW WINDOWS ! */

    for (i = windowCount - 1; i >= 0; i--) {

        drawWindow(&windows[zOrder[i]]);

    }

    drawDebugOverlay();
    drawMenuBar();
    drawDropdown();
    drawStatusBar();
    drawCursor();

    /* FLIP BUFFER */

    memcpy((void *)(__djgpp_conventional_base + 0xA0000), VIR_SCR, 64000);
}

void showAbout() { cguiMsgBox("ABOUT", "MDPed v0.1 -- CG MOON 2026"); }

void test_onSliderMove(Widget *w) {

    windows[w->parentWin].widgets[w->id + 1].val = w->val;
}

void test_showProgress() {

    int win = createWindow(60, 60, 120, 70, "TEST: PROGRESS");

    int s_idx = addWidget(win, WIDGET_SLIDER, 10, 20, 100, 10, "");
    int p_idx = addWidget(win, WIDGET_PROGRESS, 10, 40, 100, 10, "");

    (void)p_idx;
    windows[win].widgets[s_idx].onClick = test_onSliderMove;

    packWindow(win);

}

int main() {

  int win1, win2, win3, done;
  int fileMenu, editMenu, viewMenu;
  int debugMenu, cWid;

  union REGS r;

  if (__djgpp_nearptr_enable() == 0)
    return 1;

  VIR_SCR = (unsigned char *)malloc(64000);

  /* SYS INIT */

  initMDPED();
  initCGUIPalette();
  initWindowManager();

  cguiDebugMode = 0;

  setBackgroundGradient(1, 0);

  setStatusLeft(STATUS_STRING, "MDP EDIT v0.1", NULL, 0, NULL);
  setStatusRight(STATUS_CLOCK, "", NULL, 0, NULL);

  /* MENU BAR */

  fileMenu  = addMenuCategory("FILE");
  editMenu  = addMenuCategory("EDIT");
  viewMenu  = addMenuCategory("VIEW");
  debugMenu = addMenuCategory("DEBUG");

  addMenuItem(fileMenu, "ABOUT", showAbout);
  addMenuItem(fileMenu, "EXIT", cguiExit);
  addMenuItem(editMenu, "DUMMY", NULL);
  addMenuItem(viewMenu, "DUMMY", NULL);

  addMenuItem(viewMenu, "PROGRESS TEST", test_showProgress);

  addMenuItemToggle(debugMenu, "Z-ORDER", cguiToggleDebug);

  /* TODO: edit stuff goes here */

  /* TODO: view stuff goes here */


  /* WINDOWS / WIDGETS */

  /* put all your windows and widgets here, this is the
     top layer */

  win1 = createWindow(20, 20, 160, 120, "FILE BROWSER");
  addWidget(win1, WIDGET_LABEL, 10, 20, 0, 0, "SELECT ARCHIVE:");
  addWidget(win1, WIDGET_BUTTON, 10, 40, 60, 15, "OPEN MDP");
  addWidget(win1, WIDGET_CHECKBOX, 10, 60, 10, 10, "READ ONLY");
  addWidget(win1, WIDGET_TOGGLE, 10, 80, 80, 15, "GRID SNAP");
  addWidget(win1, WIDGET_INPUT, 10, 100, 100, 15, "");

  addWidget(win1, WIDGET_LISTBOX, 100, 40, 50, 54, "");

  /* TODO: move this shit elsewhere */

  win3 = createWindow(130, 30, 150, 140, "STRESS TEST");

  addWidget(win3, WIDGET_LISTBOX, 10, 20, 130, 104, "");


  /* DIR SCAN (MDP FILES) */

  {

    struct ffblk ffblk;

    Widget *listWid = &windows[win3].widgets[0];

    listWid->listCount = 0;
    listWid->listScroll = 0;
    listWid->listSelected = -1;

    done = findfirst("*.*", &ffblk, 0);

    while (!done && listWid->listCount < 32) {

      size_t nameLen = strlen(ffblk.ff_name);

      if (nameLen > 63u) nameLen = 63u;

      memcpy(listWid->listItems[listWid->listCount], ffblk.ff_name, nameLen);
      listWid->listItems[listWid->listCount][nameLen] = '\0';
      listWid->listCount++;
      done = findnext(&ffblk);

    }

  }

  win2 = createWindow(100, 80, 120, 80, "PALEDIT");
  addWidget(win2, WIDGET_BUTTON, 10, 25, 100, 15, "IMPORT PAL");
  addWidget(win2, WIDGET_BUTTON, 10, 45, 100, 15, "EXPORT CHUNK");

  cWid = addWidget(win2, WIDGET_CANVAS_IMG, 10, 65, 34, 34, "");
  windows[win2].widgets[cWid].data = &testImg;

  packWindow(win2);

  /* ENGINE LOOP */


  uclock_t nextTick = uclock();
  int catchUpLoops;

  while (appRunning) {

    catchUpLoops = 0;

    while (uclock() >= nextTick && catchUpLoops < 10) {

      updateMouse();
      updateKeyboard();

      if (mouseB & 2)
	appRunning = 0;

      updateGUI();

      nextTick += TICK_INTERVAL;
      catchUpLoops++;

    }

    renderGUI();

  }


  /* CLEANUP */

  r.h.ah = 0x00;
  r.h.al = 0x03;
  int86(0x10, &r, &r);

  return 0;

}
