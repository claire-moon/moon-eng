#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"

#define TICKS_PER_SECOND 35
#define TICK_INTERVAL (UCLOCKS_PER_SEC / TICKS_PER_SECOND)
#define SMOKE_TICKS 1

int showFPS = 0;
int showPos = 0;
int showTics = 0;

int currentFPS = 0;
unsigned long frameCount = 0;
unsigned long ticCount = 0;

char mdpFilename[32] = "game.mdp";
int moonSmokeMode = 0;

static void smokeTrace(const char *phase) {

  FILE *trace;

  if (!moonSmokeMode) return;

  trace = fopen("SMOKE.LOG", "ab");
  if (!trace) return;

  fprintf(trace, "%s\r\n", phase);
  fclose(trace);

}

static int optionEquals(const char *left, const char *right) {

  while (*left != '\0' && *right != '\0') {

    if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) {

      return 0;

    }

    left++;
    right++;

  }

  return *left == '\0' && *right == '\0';

}

static int writeSmokeResult(unsigned long ticks) {

  FILE *result = fopen("SMOKE.OUT", "wb");

  if (!result) return 0;

  fprintf(result, "PASS\tZEUS.STARTUP\tTICKS\t%lu\r\n", ticks);

  return fclose(result) == 0;

}

int main(int argc, char *argv[]) {

  Player player;
  int argIndex;
  int mdpArgumentSeen = 0;
  unsigned long smokeTicks = 0;

  for (argIndex = 1; argIndex < argc; argIndex++) {

    if (optionEquals(argv[argIndex], "/SMOKE") ||
        optionEquals(argv[argIndex], "-SMOKE") ||
        optionEquals(argv[argIndex], "--SMOKE")) {

      moonSmokeMode = 1;
      continue;

    }

    if (mdpArgumentSeen) {

      fprintf(stderr, "Usage: ZEUS.EXE [PACKAGE.MDP] [/SMOKE]\n");
      return 2;

    }

    if (snprintf(mdpFilename, sizeof(mdpFilename), "%s", argv[argIndex]) >=
        (int)sizeof(mdpFilename)) {

      fprintf(stderr, "MDP path is too long (maximum %u characters).\n",
              (unsigned int)(sizeof(mdpFilename) - 1));
      return 1;

    }

    mdpArgumentSeen = 1;

  }

  smokeTrace("ARGS");

  /* TIME VARS */

  uclock_t lastTime, startTime, currentTime, nextTick;
  int frames = 0;
  int catchUpLoops = 0;
  int needsRender = 0;

  initMapSystem();
  smokeTrace("MAP.INIT");
  loadMap(1);
  smokeTrace("MAP.LOAD");

  initPlayer(&player);
  smokeTrace("PLAYER.INIT");
  initVideo();
  smokeTrace("VIDEO.INIT");
  initKeyboard();
  smokeTrace("INPUT.INIT");

  startTime = uclock();
  lastTime = startTime;
  nextTick = startTime;

  if (moonSmokeMode) {

    for (smokeTicks = 0; smokeTicks < SMOKE_TICKS; smokeTicks++) {

      processInput(&player);
      updatePlayer(&player);
      ticCount++;
      renderScene(&player);
      frameCount++;

      smokeTrace("FRAME.RENDERED");

    }

  } else while(1) {

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

  cleanupKeyboard();
  smokeTrace("INPUT.DONE");
  cleanupVideo();
  smokeTrace("VIDEO.DONE");

  if (moonSmokeMode && !writeSmokeResult(smokeTicks)) {

    fprintf(stderr, "Unable to write SMOKE.OUT.\n");
    return 1;

  }

  return 0;

}
