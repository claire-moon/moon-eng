#include "sound.h"

#include <math.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.1415926535
#endif

Oscillator synth[MSE_CHANNELS];

unsigned char masterBuffer[MSE_BUFFER_SIZE];

int writeIndex = 0;

/* LOOKUP TABLE */

unsigned char sineLUT[256];
int lutInit = 0;

/* INTEGER PHASE ACCUMULATOR */

unsigned int phaseAcc[MSE_CHANNELS];
unsigned int phaseStep[MSE_CHANNELS];

void msePlayTone(int channel, float freq, float vol) {

  int i;

  if (channel < 0 || channel >= MSE_CHANNELS) return;

  /* PRE-CALC WAVE */

  if (!lutInit) {

    for (i = 0; i < 256; i++) {

      sineLUT[i] = (unsigned char)((sin((i / 256.0) * 2.0 * PI) + 1.0) * 127.5);
      
    }

    lutInit = 1;
    
  }

  synth[channel].waveType  = waveType;
  synth[channel].frequency = freq;
  synth[channel].volume    = vol;
  synth[channel].active    = (vol > 0.0) ? 1 : 0;  

  /* CALC ISR */

  phaseStep[channel] = (unsigned int)((freq / MSE_SAMPLE_RATE) * 65536.0);
  
}

void mseUpdate() {

  int i;
  int lutIndex;
  float mixedSample, rawSample;

  while(((writeIndex + 1) % MSE_BUFFER_SIZE) != sampleIndex) {

    mixedSample = 0.0;

    for (c = 0; c < MSE_CHANNELS; c++) {

      if (!synth[c].active) continue;

      switch (synth[c].waveType) {

      case WAVE_SINE:
	
	lutIndex = (phaseAcc[c] >> 8) & 0xFF;
	rawSample = (sineLUT[lutIndex] - 127.5);
	break;

      case WAVE_SQUARE:

	rawSample = (phaseAcc[c] & 0x8000) ? 127.0 : -128.0;
	break;

      case WAVE_SAW:

	rawSample = (float)(phaseAcc[c] >> 8) - 128.0;
	break;

      case WAVE_TRIANGLE:

	lutIndex = (phaseAcc[c] >> 8) & 0xFF;
	if (lutIndex > 127) lutIndex = 255 - lutIndex;
	rawSample = (float)(lutIndex * 2) - 128.0;
	break;
	
      case WAVE_NOISE:

	rawSample = 0.0;
	break;

      default:
	
	rawSample = 0.0;
	break;
	
      }

      mixedSample += (rawSample * synth[c].volume);

      phaseAcc[c] += phaseStep[c];
    
    }

    mixedSample = (mixedSample / MSE_CHANNELS) + 127.5;

    if (mixedSample > 255.0) mixedSample = 255.0;
    if (mixedSample < 0.0) mixedSample = 0.0;

    masterBuffer[writeIndex] = (unsigned char)mixedSample;

    writeIndex++;

    if (writeIndex >= MSE_BUFFER_SIZE) writeIndex = 0;
    
  }

}
