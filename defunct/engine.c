
/*
 *
 *   E N G I N E . C
 *    M O O N  E N G
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <sys/nearptr.h>
#include <math.h>

/* CONSTS */

#define VGA_Physics_Address  0xA0000
#define VID_INT              0x10
#define VGA_320x200x256      0x13
#define TXT_MODE             0x03
#define SCR_W                320
#define SCR_H                200
#define SCR_SIZE             64000

#define PI                   3.1415926535
#define DEG2RAD              0.01745329

#define MAP_SIZE             8
#define TIL_SIZE             64

/* GLOBALS */

volatile unsigned char *VGA = 0;
unsigned char *VIR_SCR = 0;

float px = 300;
float py = 300;
float pa = 0;
  
/* FUNCS */

/*
 * setMode()
 * switches vid mode using BIOS interrupt 0x10
 */

void setMode(unsigned char mode) {

  union REGS regs;

  regs.h.ah = 0x00;
  regs.h.al = mode;
  int86(VID_INT, &regs, &regs);
  
}

/*
 * flipBuffer()
 * blits entire virt screen to vid mem
 */

void flipBuffer() {

  memcpy((void *)VGA, VIR_SCR, SCR_SIZE);

}

/*
 * plotPixel
 * writes directly to video mem
 */

void plotPixel(int x, int y, unsigned char color) {

  if (x >= 0 && x < SCR_W && y >= 0 && y <SCR_H) {
  
    VIR_SCR[(y * SCR_W) + x] = color;

  }
  
}

/* MAIN */

int main(void) {

  int r, i;
  int mapX, mapY, lineH, lineO;
  float ra, rx, ry, dx, dy, dist, ca;
  char key;
  
  union REGS regs;

  /* DPMI SETUP */

  if (__djgpp_nearptr_enable() == 0) {

    printf("ERROR: DPMI NEAR POINTER FAILED..!\n");

    return 1;
    
  }

  VGA = (unsigned char *)(__djgpp_conventional_base + VGA_Physics_Address);
  VIR_SCR = (unsigned char *)malloc(SCR_SIZE);

  if (VIR_SCR == NULL) {

    printf("ERROR: NOT ENOUGH RAM FOR DOUBLE BUFFER..!");

    return 1;
    
  }
  
  /* GFX START */

  setMode(VGA_320x200x256);

  /* GAME LOOP */

  while (1) {

        memset(VIR_SCR, 7, SCR_SIZE / 2);
        memset(VIR_SCR + (SCR_SIZE / 2), 8, SCR_SIZE / 2);
      
        for (r = 0; r < SCR_W; r++) {

        ra = pa - 0.52359 + ((float)r * 0.003272);
    
        rx = px;
        ry = py;
        dx = cos(ra);
        dy = sin(ra);
        dist = 0;
     
	while(1) {

	  rx += dx;
	  ry += dy;
	  dist += 1;
	  
          mapX = (int)(rx / TIL_SIZE);
          mapY = (int)(ry / TIL_SIZE);

          if (mapX < 0 || mapX >= MAP_SIZE || mapY < 0 || mapY >= MAP_SIZE) {

	    break;
	
          }

          if (map01[mapY][mapX] != 0) {

	    ca = pa - ra;

	    if (ca < 0) ca += 2*PI;
	    if (ca > 2*PI) ca -= 2*PI;
	
	    dist = dist * cos(ca);

	    if (dist < 1) dist = 1;
	
	    lineH = (TIL_SIZE * 277) / dist;

	    if (lineH > SCR_H) lineH = SCR_H;

	    lineO = (SCR_H / 2) - (lineH / 2);

	    for (i = 0; i < lineH; i++) {

	      plotPixel(r, i + lineO, 4);
	  
	    }

	    break;
	
	  }
      }
  }    
      
  flipBuffer();
  
  }

  setMode(TXT_MODE);
  free(VIR_SCR);
  __djgpp_nearptr_disable();

  return 0;

}
