#include <stdio.h>
#include <conio.h>

#include "sound.h"

int main() {

  char input;

  int isPlaying = 1;

  clrscr();

  cprintf("\r\n");
  cprintf("MOON ENG\r\n");
  cprintf("SOUND TEST\r\n");
  cprintf("\r\n");

  mseInit();

  cprintf("press 1 for SINE\r\n");
  cprintf("press 2 for SQUARE\r\n");
  cprintf("press 3 for SAW\r\n");
  cprintf("press 4 for TRIANGLE\r\n");
  cprintf("press 5 for NOISE\r\n");
  cprintf("press SPACE to toggle playback\r\n");
  cprintf("press ESC to exit to DOS\r\n\r\n");

  while (1) {

    mseUpdate();

    if (kbhit()) {

      input = getch();

      if (input == 27) break;

      /* WAVEFORM CHANGER */
      
      if (input >= '1' && input <= '5') {

	currentWave = input - '1';

	if (isPlaying) {

	  msePlayTone(0, currentWave, 220.0, 0.8);
	  msePlayTone(1, currentWave, 440.0, 0.5);
	  
	}

	cprintf("[SYNTH] WAVE SELECTED: %d \r\n", currentWave);
	
      }

      /* TOGGLE SOUND */
      
      if (input == ' ') {

	isPlaying = !isPlaying;

	if (isPlaying) {

	  msePlayTone(0, currentWave, 220.0, 0.8);
	  msePlayTone(1, currentWave, 440.0, 0.5);       
	  
	} else {

	  msePlayTone(0, 0.0, 0.0);
	  msePlayTone(1, 0.0, 0.0);
	  
	}
	
      }
      
    }
    
  }

  cprintf("\r\n\r\n SHUTTING DOWN HARDWARE TIMER...\r\n");

  mseCleanup();

  textbackground(0);
  textcolor(7);
  clrscr();

  return 0;
  
}
