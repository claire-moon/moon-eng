#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

extern unsigned char *VIR_SCR;

/*
 * loadSprite()
 * reads a raw pixel lump from the moon-data-package (MDP)
 */

Sprite *loadSprite(const char *name) {

  FILE *f;
  MoonHeader head;
  MoonEntry entry;
  Sprite *s;
  int i;
  int found = 0;

  f = fopen(mdpFilename, "rb");

  if (!f) return NULL;

  if (fread(&head, sizeof(MoonHeader), 1, f) != 1 ||
      memcmp(head.magic, "MOON", 4) != 0 ||
      head.numLumps < 0 || head.dirOffset < (int)sizeof(MoonHeader) ||
      fseek(f, head.dirOffset, SEEK_SET) != 0) {

    fclose(f);
    return NULL;

  }

  /* FIND LUMP */

  for (i = 0; i < head.numLumps; i++) {

    if (fread(&entry, sizeof(MoonEntry), 1, f) != 1) break;

    entry.name[sizeof(entry.name) - 1] = '\0';

    if (strcmp(entry.name, name) == 0) {

      found = 1;
      break;

    }

  }

  if (!found) {

    fclose(f);
    return NULL;

  }

  /* ALLOCATE & READ ! */

  if (entry.offset < (int)sizeof(MoonHeader) || entry.size < 128 * 128) {

    fclose(f);
    return NULL;

  }

  s = (Sprite *)malloc(sizeof(Sprite));

  if (!s) {

    fclose(f);
    return NULL;

  }

  /* TODO : CHANGE FROM HARDCODED 128x128 TO
            HAVING IT READ DIMS FROM HEADER ! */

  s->width   = 128;
  s->height  = 128;

  s->data = (unsigned char *)malloc(s->width * s->height);

  if (!s->data || fseek(f, entry.offset, SEEK_SET) != 0 ||
      fread(s->data, s->width * s->height, 1, f) != 1) {

    free(s->data);
    free(s);
    fclose(f);
    return NULL;

  }

  fclose(f);
  return s;

}

void freeSprite(Sprite *sprite) {

  if (!sprite) return;

  free(sprite->data);
  free(sprite);

}

/*
 * drawSprite()
 * draws a sprite with transparency (index 255)
 */

void drawSprite(int x, int y, Sprite *s) {

  int u, v;
  int screenOffset, spriteOffset;
  unsigned char pixel;

  for (v = 0; v < s->height; v++) {

    for (u = 0; u < s->width; u++) {

      /* SAFETY CHECK */

      if (x + u < 0 || x + u >= SCR_W || y + v < 0 || y + v >= SCR_H)

	continue;

      spriteOffset = (v * s->width) + u;
      pixel = s->data[spriteOffset];

      /* TRANS CHECK (index 255 is PINK !!) */

      if (pixel != 255) {

	screenOffset = ((y + v) * SCR_W) + (x + u);
	VIR_SCR[screenOffset] = pixel;

      }

    }

  }

}
