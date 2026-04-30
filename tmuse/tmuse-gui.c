#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <dos.h>
#include <sys/movedata.h>

#include "tmuse-dash.h"
#include "tmuse-dsp.h"
#include "tmuse-mix.h"
#include "tmuse-defs.h"

#include "../defs.h"
#include "../cgui/cgui.h"

/* KEYBOARD STATE PULL (see ../input.c) */

extern volatile char keys[128];
char oldKeys[128] = {0};
extern void initKeyboard();
extern void cleanupKeyboard();

/* INIT TMUSE API */

extern int tmuseInit();
extern void tmuseCleanup();

extern int selectedMode;
extern int activeMode;
extern int chordSubState;

extern char chordName[64];

char tmuseModeStr[64] = "MODE: INIT";

/* ASDR GLOBALS (see tmuse-mix.c) */

extern int envA, envD, envS, envR;
extern void mixSetEnv(int a, int d, int s, int r);

void runConsole(Player *p) {}

void initVideo() {

    union REGS r;
    r.x.ax = 0x0013;
    int86(0x10, &r, &r);
    VIR_SCR = (unsigned char *)malloc(64000);

}

void cleanupVideo() {

    union REGS r;
    r.x.ax = 0x0003;
    int86(0x10, &r, &r);
    if (VIR_SCR) free(VIR_SCR);

}

int main() {

    int needsRedraw = 1;

    int winIdx, i;
    Widget *console;

    extern void cguiInputUpdate();
    extern int tmuseProcessControls();

    clrscr();

    cprintf("\r\n    T M U S E e d i t\r\n");
    cprintf("                  v.0.1\r\n");

    if (!tmuseInit()) {

        cprintf("[ERROR] SOUND BLASTER HARDWARE NOT FOUND!\r\n");
    
        return 1;
    
    }

    initKeyboard();

    mBuildChord();

    initVideo();
    initCGUIPalette();
    initWindowManager();

    setBackgroundGradient(4, 0);

    setStatusLeft(STATUS_EMPTY, "", NULL, 0, NULL);
    setStatusRight(STATUS_DYNAMIC, "", tmuseModeStr, 0, NULL);

    winIdx = createWindow(10, 10, 300, 180, "TMUSE GUI");
    windows[winIdx].titleColor = 4;
    addWidget(winIdx, WIDGET_CONSOLE, 5, 15, 290, 160, NULL);
    console = &windows[winIdx].widgets[0];

    while (appRunning && !keys[KEY_ESC]) {

        mixUpdate();

        cguiInputUpdate();

        updateMouse();

        if (tmuseProcessControls()) {

            needsRedraw = 1;
      
        }

        /* DASHBOARD DISPLAY */

        if (needsRedraw) {

            if (activeMode) {

                sprintf(tmuseModeStr, "MODE: %s", chordSubState == 0 ? "[WAITING ROOT]" : "[EDITING EXT]");

            } else {

                sprintf(tmuseModeStr, "MODE: %s", selectedMode == 0 ? "[ASDR TUNING]" : "[WAVE SELECTION]");

            }

            buildDashboard(console->consoleLines);

            needsRedraw = 0;

        }

            updateGUI();

            drawDesktop();

            for (i = windowCount - 1; i >= 0; i--) {

                drawWindow(&windows[zOrder[i]]);

            }

            drawStatusBar();
            drawCursor();
            waitVsync();
            dosmemput(VIR_SCR, 64000,  0xA0000);
      
    }

    /* SAFE SHUTDOWN */

    cleanupKeyboard();
    tmuseCleanup();
    cleanupVideo();

    return 0;
  
}
