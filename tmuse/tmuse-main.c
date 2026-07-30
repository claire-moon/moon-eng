#include <stdlib.h>
#include <stdio.h>

#include "tmuse-dsp.h"
#include "tmuse-mix.h"
#include "tmuse-defs.h"

int tmuseInit() {

  char *blaster = getenv("BLASTER");

  int irq = 5, dma = 1;
  int ports[] = {0x220, 0x240, 0x260, 0x280};
  int detectedPort = 0;
  int p;

  if (blaster) {

    char *ptr = blaster;

    while (*ptr) {

      if (*ptr == 'A' || *ptr == 'a') ports[0] = strtol(ptr + 1, NULL, 16);
      if (*ptr == 'I' || *ptr == 'i') irq  = strtol(ptr + 1, NULL, 10);
      if (*ptr == 'D' || *ptr == 'd') dma  = strtol(ptr + 1, NULL, 10);

      ptr++;

    }

  }

  for (p = 0; p < 4; p++) {

    if (dspInit(ports[p], irq, dma)) {

      detectedPort = ports[p];
      break;

    }

  }

  if (!detectedPort) {

    return 0;

  }

  mixInit();

  return 1;

}

void tmuseCleanup() {

  dspCleanup();

}
