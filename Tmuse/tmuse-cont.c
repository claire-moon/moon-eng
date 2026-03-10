#include <string.h>
#include <stdio.h>
#include "tmuse-mix.h"
#include "tmuse-notes.h"
#include "../defs.h"

extern int envA, envD, envS, envR;
extern int cguiGetKeyDown(int key);
extern int cguiGetKeyUp(int key);

int currentWave = WAVE_SAW;
int isPlaying = 0;

int chordMode = 0;
int chordRoot = 0;
int chordAcc = 0;
int chord3rd = 4;
int chordExt[12] = {0};
int chordVoices = 8;
char chordName[32] = "C MAJOR [DEFAULT]";

float activeChordFreqs[MIX_CHANNELS] = {

  NOTE_C3,
  NOTE_E3,
  NOTE_G3,
  NOTE_C4,
  NOTE_E4,
  NOTE_G4,
  NOTE_C5,
  NOTE_E5
  
};

float baseFreqs[12] = {

  130.81, 138.59, 146.83, 155.56, 164.81, 174.61,
  185.00, 196.00, 207.65, 220.00, 233.08, 246.94
  
}

void tmuseTriggerChord() {

  int i;

  for (i = 0; i < MIX_CHANNELS; i++) {

    if (i < chordVoices) {

      mixPlayTone(i, currentWave, activeChordFreqs[i]);
      
    } else {

      mixKeyOff(i);
      
    }
    
  }
  
}

void tmuseBuildChord() {

  int intervals[16];
  
}

/* INPUTS ! */

int tmuseProcessControls() {

  int redraw = 0;
  int oldWave = currentWave;
  int i;
    
  if (cguiGetKeyDown(KEY_Q)) { envA += 2; redraw = 1; }
  if (cguiGetKeyDown(KEY_A)) { envA -= 2; if (envA < 1) envA = 1; redraw = 1; }

  if (cguiGetKeyDown(KEY_W)) { envD += 2; redraw = 1; }
  if (cguiGetKeyDown(KEY_S)) { envD -= 2; if (envD < 1) envD = 1; redraw = 1; }

  if (cguiGetKeyDown(KEY_E)) { envS += 2000; redraw = 1; }
  if (cguiGetKeyDown(KEY_D)) { envS -= 2000; if (envS < 1) envS = 1; redraw = 1; }

  if (cguiGetKeyDown(KEY_R)) { envR += 5; redraw = 1; }
  if (cguiGetKeyDown(KEY_F)) { envR  -= 5; if (envR < 1) envR = 1; redraw = 1; }

  if (cguiGetKeyDown(KEY_1)) { currentWave = WAVE_SINE; redraw = 1; }
  if (cguiGetKeyDown(KEY_2)) { currentWave = WAVE_SQUARE; redraw = 1; }
  if (cguiGetKeyDown(KEY_3)) { currentWave = WAVE_SAW; redraw = 1; }
  if (cguiGetKeyDown(KEY_4)) { currentWave = WAVE_TRIANGLE; redraw = 1; }
  if (cguiGetKeyDown(KEY_5)) { currentWave = WAVE_NOISE; redraw = 1; }

  /* PLAYBACK */

  if (cguiGetKeyDown(KEY_SPACE)) {

    isPlaying = !isPlaying;

    redraw = 1;

    tmuseTriggerChord();

  }

  if (cguiGetKeyUp(KEY_SPACE)) {

    isPlaying = 0;
    redraw = 1;
    for (i = 0; i < MIX_CHANNELS; i++) mixKeyOff(i);
    
  }

  if (currentWave != oldWave && isPlaying) {

    tmuseTriggerChord();
    
  }

  return redraw;
  
}
    
