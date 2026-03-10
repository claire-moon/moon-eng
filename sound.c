#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <dos.h>
#include <pc.h>

#define SAMPLE_RATE 8000
#define PI 3.1415926535

/*
 *  PWM ENGINE
 */

int i;
unsigned char oPort, pWidth;
uclock_t nTime;
int interval = UCLOCKS_PER_SEC / SAMPLE_RATE;

oPort = inportb(0x61);

outportb(0x61, oPort | 3);

outportb(0x43, 0x90);

nTime =  uClock();

for (i = 0; i < length; i++) {

    pWidth = (unsigned char)((data[i] * 149) / 255);

    if (pWidth == 0) pWidth = 1;

    outportb(0x42, pWidth);

    nTime += interval;

    while (uclock() < nTime) {

        /*busy*/

    }

}
