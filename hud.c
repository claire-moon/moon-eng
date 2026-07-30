#include <stdio.h>
#include <sys/farptr.h>
#include <go32.h>

#include "defs.h"

extern unsigned char *VIR_SCR;

/*
 * drawChar()
 * reads the raw BIOS 8x8 font directly from
 * hardware ROM, like a boss
 */

void drawChar(int x, int y, char c, int color) {

	int row, col;
	unsigned char bits;

	for (row = 0; row < 8; row++) {

		bits = _farpeekb(_dos_ds, 0xFFA6E + (c * 8) + row);

		for (col = 0; col < 8; col++) {

			if (bits & (1 << (7 - col))) {

				VIR_SCR[(y + row) * SCR_W + (x + col)] = color;

			}

		}

	}

}

void drawString(int x, int y, char *str, int color) {

	while (*str) {

		drawChar(x, y, *str, color);
		x += 8;
		str++;

	}

}

void drawHUD(Player *p) {

	char buffer[64];

	if (showFPS) {

		sprintf(buffer, "FPS: %d", currentFPS);
		drawString(5, 5, buffer, 15);

	}

	if (showPos) {

		sprintf(buffer, "X:%1.f Y:%.1f ANG:%.1f", p->x, p->y, p->a);
		drawString(5, 15, buffer, 14);

	}

	if (showTics) {

		sprintf(buffer, "TICS: %lu FRAMES: %lu", ticCount, frameCount);
		drawString(5, 25, buffer, 11);

	}

}