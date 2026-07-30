#include <math.h>

#include "defs.h"

void applyPhysics(Player *p) {

  int mapX, mapY;

  p->a += p->va;

  if (p->a < 0) p->a += 2 * PI;
  if (p->a > 2 * PI) p->a -= 2 * PI;

  p->x += p->vx;

  mapX = (int)(p->x / TIL_SIZE);
  mapY = (int)(p->y / TIL_SIZE);

  if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight || currentMap[(mapY * mapWidth) + mapX] != 0) {

    p->x -= p->vx;
    p->vx = 0;

  }

  p->y += p->vy;

  mapX = (int)(p->x / TIL_SIZE);
  mapY = (int)(p->y / TIL_SIZE);

  if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight || currentMap[(mapY * mapWidth) + mapX] != 0) {

    p->y -= p->vy;
    p->vy = 0;

  }

  p->vx *= 0.85;
  p->vy *= 0.85;
  p->va *= 0.70;

}
