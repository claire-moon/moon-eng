#include <string.h>
#include <stdio.h>

#include "tmuse-defs.h"
#include "tmuse-mix.h"

int chordRoot = 0;
int chordAcc = 0;
int chordThird = 4;
int chordExt[16] = {0};
int chordVoices = 8;

char chordName[64] = "C MAJOR [DEFAULT]";

float activeChordFreqs[MIX_CHANNELS] = {0};

float baseFreqs[12] = {

  130.81, 138.59, 146.83, 155.56, 164.81, 174.61,
  185.00, 196.00, 207.65, 220.00, 233.08, 246.94

};

void mBuildChord() {

  int intervals[16];
  int num = 0, i, semitone, oct, final;

  intervals[num++] = 0;

  if (chordThird > 0 ) intervals[num++] = chordThird;

  for (i = 1; i < 16; i++) {

    if (chordExt[i]) intervals[num++] = i;

 }

  for (i = 0; i < MIX_CHANNELS; i++) {

    if (i >= chordVoices) {

      activeChordFreqs[i] = 0.0f;
      continue;

    }
    
    semitone = intervals[i % num];
    oct = i / num;
    final = chordRoot + chordAcc + semitone + (oct * 12);;

    while(final < 0) final += 12;

    activeChordFreqs[i] = baseFreqs[final % 12];
    activeChordFreqs[i] *= (1 << (final / 12));

  }

}

void mUpdateChordName() {

  char rootStr[4] = "";
  char accStr[4] = "";
  char qualStr[8] = "";
  char extStr[16] = "";

  /* ROOT NOTE */

  switch(chordRoot) {

  case 0:    strcpy(rootStr, "C");  break;
  case 2:    strcpy(rootStr, "D");  break;
  case 4:    strcpy(rootStr, "E");  break;
  case 5:    strcpy(rootStr, "F");  break;
  case 7:    strcpy(rootStr, "G");  break;
  case 9:    strcpy(rootStr, "A");  break;
  case 11:   strcpy(rootStr, "B");  break;

  }

  /* ACCIDENTAL */

  if (chordAcc == 1) strcpy(accStr, "#");
  if (chordAcc == -1) strcpy(accStr, "b");
  
  /* QUALITY */
  
  strcpy(qualStr, (chordThird == 3) ? "m" : "M");

  /* EXTENSIONS */

  if (chordExt[14]) strcpy(extStr, "9");
  else if (chordExt[11] || chordExt[10]) strcpy(extStr, "7");
  else if (chordExt[9]) strcpy(extStr, "6");
  else if (chordExt[5]) strcpy(extStr, "sus4");
  else if (chordExt[2]) strcpy(extStr, "sus2");

  /* STRING ASSEMBLY */

  sprintf(chordName, "%s%s%s%s (%d VOICES)",
          rootStr, accStr, qualStr, extStr, chordVoices);


}
