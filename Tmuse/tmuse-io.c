#include <string.h>
#include <stdio.h>
#include "tmuse-mix.h"
#include "tmuse-defs.h"
#include "../defs.h"

extern int envA, envD, envS, envR;
extern int cguiGetKeyUp(int key);
extern int cguiGetKeyDown(int key);
extern int cguiGetKey(int key);

int currentWave = WAVE_SAW;
int isPlaying = 0;

int selectedMode = 0;
int activeMode = 0;
int chordSubState = 0;

int pRoot = 0;
int pAcc = 0;
int pThird = 4;
int pVoices = 8;

int pExt[16] = {0};

void tmuseTriggerChord() {

  int i;

  for (i = 0; i < MIX_CHANNELS; i++) {

    if (i < chordVoices) {

      mixPlayTone(i, currentWave, activeChordFreqs[i], 0.6 - (i * 0.05));
      
    } else {

      mixKeyOff(i);

    }
    
  }
  
}


/* INPUTS ! */

int tmuseProcessControls() {

  int redraw  = 0;
  int oldWave = currentWave;
  int i;
  int isShift = cguiGetKey(KEY_LSHIFT) || cguiGetKey(KEY_RSHIFT);
  int isCtrl =  cguiGetKey(KEY_CTRL);
  int newRoot;

  /* CHORD MODE TRIGGER */

  if (!isCtrl && !activeMode) {

    if (cguiGetKeyDown(KEY_LEFT)) { selectedMode = (selectedMode == 0) ? 1 : 0; redraw = 1; }
    if (cguiGetKeyDown(KEY_RIGHT)) { selectedMode = (selectedMode == 1) ? 0 : 1; redraw = 1; }

  }

  if (cguiGetKeyDown(KEY_CTRL)) {

    activeMode = 1;
    chordSubState = 0;
    
    pRoot = chordRoot; pAcc = chordAcc; pThird = chordThird;
    pVoices = chordVoices;

    for (i = 0; i < 16; i++) pExt[i] = chordExt[i];

    redraw = 1;

  }

  if (cguiGetKeyDown(KEY_BSPACE)) {

    chordRoot = pRoot; chordAcc = pAcc; chordThird = pThird;
    chordVoices = pVoices;

    for (i = 0; i < 16; i++)
      chordExt[i] = pExt[i];

    mBuildChord();
    mUpdateChordName();

    if (isPlaying)
      tmuseTriggerChord();

    redraw = 1;

  }

  if (cguiGetKeyUp(KEY_CTRL)) {

    activeMode = 0;
    mBuildChord();
    mUpdateChordName();

    if (isPlaying)
      tmuseTriggerChord();

    redraw = 1;

  }

  /* ================ */
  /* CHORD BUILD MODE */
  /* ================ */

  if (activeMode) {

    if (chordSubState == 0) {

    /* EDIT ROOT */

      if (isShift) {

	chordSubState = 1;
	redraw = 1;

      } else {

	newRoot = -1;

	if (cguiGetKeyDown(KEY_C)) newRoot = 0;
	if (cguiGetKeyDown(KEY_D)) newRoot = 2;
	if (cguiGetKeyDown(KEY_E)) newRoot = 4;
	if (cguiGetKeyDown(KEY_F)) newRoot = 5;
	if (cguiGetKeyDown(KEY_G)) newRoot = 7;
	if (cguiGetKeyDown(KEY_A)) newRoot = 9;
	if (cguiGetKeyDown(KEY_B)) newRoot = 11;

	if (newRoot != -1) {

	  chordRoot = newRoot;
	  chordAcc = 0;
	  chordThird = 4;

	  for (i = 0; i < 16; i++) chordExt[i] = 0;

	  chordSubState = 1; 
	  redraw = 1;

	}

      }

    }

    /* EDIT CHORD */

    else if (chordSubState == 1) {

      if (isShift && cguiGetKeyDown(KEY_EQUALS)) { chordAcc =  1; redraw = 1; }
      if (isShift && cguiGetKeyDown(KEY_MINUS))  { chordAcc = -1; redraw = 1; }

      /* CHORD QUALITY */

      if (cguiGetKeyDown(KEY_M)) {

	chordThird = isShift ? 4 : 3;
	redraw = 1;

      }

      if (cguiGetKeyDown(KEY_1)) { if (!isShift) chordVoices = 1; redraw = 1; }
      if (cguiGetKeyDown(KEY_2)) { if (!isShift) chordVoices = 2; else chordExt[2] = !chordExt[2]; redraw = 1; }
      if (cguiGetKeyDown(KEY_3)) { if (!isShift) chordVoices = 3; redraw = 1; }
      if (cguiGetKeyDown(KEY_4)) { if (!isShift) chordVoices = 4; else chordExt[5] = !chordExt[5]; redraw = 1; }
      if (cguiGetKeyDown(KEY_5)) { if (!isShift) chordVoices = 5; else chordExt[7] = !chordExt[7]; redraw = 1; }
      if (cguiGetKeyDown(KEY_6)) { if (!isShift) chordVoices = 6; else chordExt[9] = !chordExt[9]; redraw = 1; }
      if (cguiGetKeyDown(KEY_7)) {

	if (!isShift) chordVoices = 7;

	else {

	  chordExt[chordThird == 3 ? 10 : 11] = !chordExt[chordThird == 3 ? 10 : 11];

	}

	redraw = 1;

      }
      
      if (cguiGetKeyDown(KEY_8)) { if (!isShift) chordVoices = 8; else chordExt[12] = !chordExt[12]; redraw = 1; } 

      if (cguiGetKeyDown(KEY_9)) { if (isShift) chordExt[14] = !chordExt[14];
	redraw = 1; }

    }

    if (redraw) mUpdateChordName();

  }

  else {
   
    /* TUNING MODE */

    if (selectedMode == 0) {
 
      if (cguiGetKeyDown(KEY_Q)) { envA += 2; redraw = 1; }
      if (cguiGetKeyDown(KEY_A)) { envA -= 2; if (envA < 1) envA = 1; redraw = 1; }
      if (cguiGetKeyDown(KEY_W)) { envD += 2; redraw = 1; }
      if (cguiGetKeyDown(KEY_S)) { envD -= 2; if (envD < 1) envD = 1; redraw = 1; }
      if (cguiGetKeyDown(KEY_E)) { envS += 2000; redraw = 1; }
      if (cguiGetKeyDown(KEY_D)) { envS -= 2000; if (envS < 1) envS = 1; redraw = 1; }
      if (cguiGetKeyDown(KEY_R)) { envR  += 5; redraw = 1; }
      if (cguiGetKeyDown(KEY_F)) { envR  -= 5; if (envR < 1) envR = 1; redraw = 1; }

      mixSetEnv(envA, envD, envS, envR);

    }

    else if (selectedMode == 1) {

      if (cguiGetKeyDown(KEY_1)) { currentWave = WAVE_SINE; redraw = 1; }
      if (cguiGetKeyDown(KEY_2)) { currentWave = WAVE_SQUARE; redraw = 1; }
      if (cguiGetKeyDown(KEY_3)) { currentWave = WAVE_SAW; redraw = 1; }
      if (cguiGetKeyDown(KEY_4)) { currentWave = WAVE_TRIANGLE; redraw = 1; }
      if (cguiGetKeyDown(KEY_5)) { currentWave = WAVE_NOISE; redraw = 1; }

    }

    if (cguiGetKeyDown(KEY_SPACE)) {

      isPlaying = 1;
      redraw = 1;
      tmuseTriggerChord();

    }    

  }

  /* PLAYBACK */

  if (cguiGetKeyDown(KEY_SPACE)) { isPlaying = 1; redraw = 1; 
    tmuseTriggerChord(); }

  if (cguiGetKeyUp(KEY_SPACE))   { isPlaying = 0; redraw = 1; 
    for (i = 0; i < MIX_CHANNELS; i++) mixKeyOff(i); }

  if (currentWave != oldWave && isPlaying) 
    tmuseTriggerChord();

  return redraw;
  
}
    
