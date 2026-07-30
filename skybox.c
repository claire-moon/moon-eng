#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

extern unsigned char *VIR_SCR;

void drawSkyboxRow(Player *p, int y, int p_row,
		   float idleYaw, int skyboxID) {

  int shade, skyPanX1, skyPanX2, x, hash, starColor;

  if (skyboxID == 1 || skyboxID == 2) {

    /* BASE GRADIENT */

    shade = 10 + (abs(p_row) * 32) / (SCR_H / 2);

    if (shade > 31) shade = 31;

    /* SKYBOX ID CHECK */

    if (skyboxID == 1) {

      memset(VIR_SCR + (y * SCR_W), shade, SCR_W);

    } else {

      memset(VIR_SCR + (y * SCR_W), 224 + shade, SCR_W);

    }

    /* PARALLAX STARS */

    skyPanX1 = (int)((p->a + idleYaw) * 150.0 + (p->idleTimer * 15.0));
    skyPanX2 = (int)((p->a + idleYaw) *  80.0 + (p->idleTimer * 5.0));

    for (x = 0; x < SCR_W; x++) {

      hash = ((x + skyPanX1) * 139 + y * 5939) & 2047;

      if (hash < 4) {

	starColor = (sin((x + skyPanX1) +
			 p->idleTimer * 4.0) > 0) ? 0 : 8;

	VIR_SCR[(y * SCR_W) + x] = starColor;

      } else {

	hash = ((x + skyPanX2) * 113 + y * 4177) & 2047;

	if (hash < 8) {

	  VIR_SCR[(y * SCR_W) + x] = 12;

	}

      }

    }

    /* IF ID IS 2 RENDER PLANET (zeus specific) */

  }

  else {

    /* FALLBACK SKY (BLUE) */

    memset(VIR_SCR + (y *SCR_W), 3, SCR_W);

  }

}
