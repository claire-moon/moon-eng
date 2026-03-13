#include <stdio.h>
#include <string.h>

#include "tmuse-defs.h"
#include "tmuse-mix.h"
#include "tmuse-dash.h"

extern int selectedMode, activeMode, chordSubState;
extern char chordName[64];
extern int envA, envD, envS, envR;
extern int isPlaying, currentWave;

void buildDashboard(char buffer[25][80]) {

    int i;

    for (i = 0; i < 25; i++)
      buffer[i][0] = '\0';

    char modeStr[32];

    if (activeMode) {

        strcpy(modeStr, chordSubState == 0 ? "[WAITING ROOT]" : "[EDITING EXT]");

    } else {

        strcpy(modeStr, selectedMode == 0 ? "[ASDR TUNING]" : "[WAVE SELECTION]");

    }

    sprintf(buffer[0],  " =========================");
    sprintf(buffer[2],  "    MODE: %-22s ", modeStr);
    sprintf(buffer[3],  "    CHORD: %-15s ", chordName);
    sprintf(buffer[5],  "    WAVE SELECTED:  %-5d  ", currentWave);
    sprintf(buffer[6],  "    - - - - - - - - - -   ");
    sprintf(buffer[7],  "        [ENV CONTROL]     ");
    sprintf(buffer[8],  "    (Q/A) ATTACK  : %-5d  ", envA);
    sprintf(buffer[9],  "    (W/S) DECAY   : %-5d  ", envD);
    sprintf(buffer[10], "    (E/D) SUSTAIN : %-5d  ", envS);
    sprintf(buffer[11], "    (R/F) RELEASE : %-5d  ", envR);
    sprintf(buffer[12], " =========================");
    sprintf(buffer[14], "    STATUS: %-10s ", isPlaying ?
            "CHORD ON " : "CHORD OFF");


}
