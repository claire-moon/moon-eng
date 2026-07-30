#include <stdio.h>
#include <conio.h>

#include "tmuse-dsp.h"
#include "tmuse-mix.h"
#include "tmuse-defs.h"
#include "tmuse-dash.h"

#include "../defs.h"

/* KEYBOARD STATE PULL (see ../input.c) */

extern volatile char keys[128];
char oldKeys[128] = {0};
extern void initKeyboard();
extern void cleanupKeyboard();

void cleanupVideo() { }
void runConsole(Player *p) { }

/* INIT TMUSE API */

extern int tmuseInit();
extern void tmuseCleanup();

extern int selectedMode;
extern int activeMode;
extern int chordSubState;

extern char chordName[64];

/* ASDR GLOBALS (see tmuse-mix.c) */

extern int envA, envD, envS, envR;
extern void mixSetEnv(int a, int d, int s, int r);

int main() {

  int needsRedraw = 1;

  extern void cguiInputUpdate();
  extern int tmuseProcessControls();

  clrscr();

  cprintf("\r\n   T M U S E e d i t\r\n");
  cprintf("                  v.0.1\r\n");

  if (!tmuseInit()) {

    cprintf("[ERROR] SOUND BLASTER HARDWARE NOT FOUND!\r\n");

    return 1;

  }

  initKeyboard();

  mBuildChord();

  while (!keys[KEY_ESC]) {

    mixUpdate();

    cguiInputUpdate();

    if (tmuseProcessControls()) {

      needsRedraw = 1;

    }

    /* DASHBOARD DISPLAY */

    if (needsRedraw) {

        char tuiBuffer[25][80];
        int line;

        clrscr();
        gotoxy(1, 1);

        buildDashboard(tuiBuffer);

        for (line = 0; line <= 15; line++) {

            if (tuiBuffer[line][0] != '\0') {

                cprintf("%s\r\n", tuiBuffer[line]);

            } else {

                cprintf("\r\n");

            }

        }

        needsRedraw = 0;

    }

  }

  /* SAFE SHUTDOWN */

  cleanupKeyboard();
  tmuseCleanup();

  textbackground(0);
  textcolor(7);
  clrscr();

  return 0;

}
