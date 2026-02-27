#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

extern unsigned char *VIR_SCR;

/*
 * loadSprite()
 * reads a raw pixel lump from the moon-data-package (MDP)
 */

Sprite* loadSprite(char* name) {

  FILE *f;
  MoonHeader head;
  MoonEntry entry;
  Sprite *s;
  int i;
  int found = 0;

  f = fopen("game.mdp", "rb");

  if (!f) return NULL;

  fread(&head, sizeof(MoonHeader), 1, f);
  fseek(f, head.dirOffset, SEEK_SET);

  /* FIND LUMP */

  for (i = 0; i < head.numLumps; i++) {

    fread(&entry, sizeof(MoonEntry), 1, f);

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

  s = (Sprite *)malloc(sizeof(Sprite));

  /* TODO : CHANGE FROM HARDCODED 128x128 TO
            HAVING IT READ DIMS FROM HEADER ! */

  s->width   = 128;
  s->height  = 128;

  s->data = (unsigned char *)malloc(s->width * s->height);

  fseek(f, entry.offset, SEEK_SET);
  fread(s->data, s->width * s->height, 1, f);

  fclose(f);
  return s;
  
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
