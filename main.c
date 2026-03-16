#include <time.h>
#include <string.h>

#include "defs.h"

#define TICKS_PER_SECOND 35
#define TICK_INTERVAL (UCLOCKS_PER_SEC / TICKS_PER_SECOND)

int showFPS = 0;
int showPos = 0;
int showTics = 0;

int currentFPS = 0;
unsigned long frameCount = 0;
unsigned long ticCount = 0;

char mdpFilename[32] = "game.mdp";

int main(int argc, char *argv[]) {

  if (argc > 1) {

    strcpy(mdpFilename, argv[1]);
    
  }
  
  Player player;

  /* TIME VARS */

  uclock_t lastTime, startTime, currentTime, nextTick;
  int frames = 0;
  int catchUpLoops = 0;
  int needsRender = 0;

  initMapSystem();
  loadMap(1);

  initPlayer(&player);
  initVideo();
  initKeyboard();

  startTime = uclock();
  lastTime = startTime;
  nextTick = startTime;
  
  while(1) {

      /* TIME/FPS/TICS CALC */

      currentTime = uclock();

      if (currentTime - lastTime >= UCLOCKS_PER_SEC) {

          currentFPS = frames;
          frames = 0;
          lastTime = currentTime;

      }

    if (uclock() > nextTick + UCLOCKS_PER_SEC) {

        nextTick = uclock();

    }

    needsRender = 0;

    catchUpLoops = 0;

    while (uclock() >= nextTick && catchUpLoops < 10) {

        processInput(&player);
        updatePlayer(&player);

        ticCount++;
        nextTick += TICK_INTERVAL;
        catchUpLoops++;
        needsRender = 1;

    }

    /* ENG PIPELINE */

    if (needsRender) {

        renderScene(&player);
        frames++;
        frameCount++;

    }
    
  }

  return 0;

}
