#ifndef TMUSE_MIX_H
#define TMUSE_MIX_H

/*
 *  CHANNELS
 */

#define MIX_CHANNELS 8

/*
 *  WAVEFORMS
 */

#define WAVE_SINE      0
#define WAVE_SQUARE    1
#define WAVE_SAW       2
#define WAVE_TRIANGLE  3
#define WAVE_NOISE     4

/*
 *  ASDR ENV STATES
 */

#define ENV_OFF        0
#define ENV_ATTACK     1
#define ENV_DECAY      2
#define ENV_SUSTAIN    3
#define ENV_RELEASE    4

/* STRUCTS */

typedef struct {

  int waveType;
  float frequency;
  float volume;
  int active;
  unsigned int phaseAcc;
  unsigned int phaseStep;

  int envState;
  int envVol;
  int attackRate;
  int decayRate;
  int sustainLevel;
  int releaseRate;

} Voice;

extern Voice channels[MIX_CHANNELS];

void mixInit();
void mixUpdate();
void mixPlayTone(int channel, int waveType, float freq, float vol);
void mixKeyOff(int channel);
void mixSetEnv(int a, int d, int s, int r);

#endif
