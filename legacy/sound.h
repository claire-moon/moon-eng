#ifndef SOUND_H
#define SOUND_H

#define MSE_SAMPLE_RATE 8000
#define MSE_BUFFER_SIZE 2048

#define WAVE_SINE 0
#define WAVE_SQUARE 1
#define WAVE_SAW 2
#define WAVE_TRIANGLE 3
#define WAVE_NOISE 4

#define MSE_CHANNELS 4

typedef struct {

  float frequency;
  float phase;
  float volume;

  int active;  
  
} Oscillator;

extern unsigned char masterBuffer[MSE_BUFFER_SIZE];

extern volatile int sampleIndex;

/* HARDWARE INTERRUPT DRIVER */

void mseInit();
void mseCleanup();

/* SYNTHESIZER MIXER */

void mseUpdate();
void msePlayTone(int channel, int waveType, float freq, float vol);

#endif
