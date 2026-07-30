#include "tmuse-dsp.h"
#include "tmuse-mix.h"

#include <math.h>
#include <stdlib.h>

#include <sys/movedata.h>

#ifndef PI
#define PI 3.1415926535
#endif

Voice channels[MIX_CHANNELS];
unsigned char sineLUT[256];

int lastHalf   = -1;
int envA       = 200;
int envD       = 30;
int envS       = 32000;
int envR       = 10;

void mixInit() {

  int i;

  for (i = 0; i < 256; i++) {

    sineLUT[i] = (unsigned char)((sin((i / 256.0) * 2.0 * PI) + 1.0) * 127.5);

  }

  for (i = 0; i < MIX_CHANNELS; i++) {

    channels[i].active = 0;
    channels[i].phaseAcc = 0;
    channels[i].envState = ENV_OFF;

  }

}

void mixSetEnv(int a, int d, int s, int r) {

  envA = a;
  envD = d;
  envS = s;
  envR = r;

}

void mixPlayTone(int ch, int waveType, float freq, float vol) {

  if (ch < 0 || ch >= MIX_CHANNELS) return;

  channels[ch].waveType  = waveType;
  channels[ch].frequency = freq;
  channels[ch].volume    = vol;

  channels[ch].phaseStep = (unsigned int)((freq / DSP_SAMPLE_RATE) * 65536.0);

  channels[ch].phaseAcc = 0;

  /* ASDR ENV TRIGGER */

  channels[ch].active   = 1;
  channels[ch].envState = ENV_ATTACK;

  /* TODO: REMOVE THIS LATER */

  /* for now leaving in this hardcoded pluck/pad env
     for testing*/

  channels[ch].attackRate   = envA;
  channels[ch].decayRate    = envD;
  channels[ch].sustainLevel = envS;
  channels[ch].releaseRate  = envR;

}

void mixKeyOff(int ch) {

  if (ch < 0 || ch >= MIX_CHANNELS) return;

  if (channels[ch].envState != ENV_OFF) {

    channels[ch].envState = ENV_RELEASE;

  }

}

void mixUpdate() {

  int c, i, lutIndex;
  int mixedSample, rawSample, intVol;
  int halfSize = DSP_BUFFER_SIZE / 2;
  int startIdx;

  if (dspHalfReady == lastHalf) return;

  lastHalf = dspHalfReady;
  startIdx = lastHalf ? 0 : halfSize;

  for (i = startIdx; i < startIdx + halfSize; i++) {

    mixedSample = 0.0;

    for (c = 0; c < MIX_CHANNELS; c++) {

      if (!channels[c].active) continue;

      /* ASDR STATE MACHINE */

      switch(channels[c].envState) {

      case ENV_ATTACK:
	channels[c].envVol += channels[c].attackRate;

	if (channels[c].envVol >= 65535) {

	  channels[c].envVol   = 65535;
	  channels[c].envState = ENV_DECAY;

	}

	break;

      case ENV_DECAY:

	channels[c].envVol -= channels[c].decayRate;

	if (channels[c].envVol <= channels[c].sustainLevel) {

	  channels[c].envVol   = channels[c].sustainLevel;
	  channels[c].envState = ENV_SUSTAIN;

	}

	break;

      case ENV_SUSTAIN:

	channels[c].envVol = channels[c].sustainLevel;

	break;

      case ENV_RELEASE:

	channels[c].envVol -= channels[c].releaseRate;

	if (channels[c].envVol <= 0) {

	  channels[c].envVol = 0;
	  channels[c].envState = ENV_OFF;
	  channels[c].active = 0;

	}

	break;

      }

      intVol = (int)(((float)channels[c].envVol / 65535.0f) *
		     (channels[c].volume * 256.0f));

      switch(channels[c].waveType) {

      case WAVE_SINE:

	lutIndex = (channels[c].phaseAcc >> 8) & 0xFF;
	rawSample = (sineLUT[lutIndex] - 128);
	break;

      case WAVE_SAW:

	rawSample = ((channels[c].phaseAcc >> 8) & 0xFF) - 128;
	break;

      case WAVE_SQUARE:

	rawSample = (channels[c].phaseAcc & 0x8000) ? 127 : -128;
	break;

      case WAVE_TRIANGLE:

	lutIndex = (channels[c].phaseAcc >> 8) & 0xFF;
	if (lutIndex > 127) lutIndex = 255 - lutIndex;
	rawSample = (float)(lutIndex * 2) - 128;
	break;

      case WAVE_NOISE:

	rawSample = (float)(rand() % 256) - 128;
	break;

      default:

	rawSample = 0.0;
	break;

      }


      mixedSample += (rawSample * intVol) >> 8;

      channels[c].phaseAcc += channels[c].phaseStep;
      channels[c].phaseAcc &= 0xFFFF;

    }

    mixedSample = (mixedSample / MIX_CHANNELS) + 128;

    if (mixedSample > 255) mixedSample = 255;
    if (mixedSample < 0) mixedSample = 0;

    dspBuffer[i] = (unsigned char)mixedSample;

  }

  dosmemput(&dspBuffer[startIdx], halfSize, dmaLinAddr + startIdx);

}
