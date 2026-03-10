#include <dos.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>

#include "sound.h"

volatile int mseRunning = 0;
volatile int sampleIndex = 0;

_go32_dpmi_seginfo oldTimer, newTimer;
unsigned char originalPort;

void mseInterrupt() {

  unsigned char pulseWidth;

  if (!mseRunning) {

    outportb(0x20, 0x20);

    return;
    
  }

  pulseWidth = (unsigned char)((masterBuffer[sampleIndex] * 149) / 255);

  if (pulseWidth == 0) pulseWidth = 1;

  outportb(0x42, pulseWidth);

  sampleIndex++;

  if (sampleIndex >= MSE_BUFFER_SIZE) sampleIndex = 0;

  outportb(0x20, 0x20);
  
}

void mseInit() {

  int divisor;

  _go32_dpmi_lock_code(mseInterrupt, 4096);
  _go32_dpmi_lock_data((void*)masterBuffer, MSE_BUFFER_SIZE);
  _go32_dpmi_lock_data((void*)&sampleIndex, sizeof(sampleIndex));
  _go32_dpmi_lock_data((void*)&mseRunning, sizeof(mseRunning));

  originalPort = inportb(0x61);
  outportb(0x61, originalPort | 3);
  outportb(0x43, 0x90);

  _go32_dpmi_get_protected_mode_interrupt_vector(8, &oldTimer);

  newTimer.pm_offset = (int)mseInterrupt;
  newTimer.pm_selector = _go32_my_cs();

  _go32_dpmi_allocate_iret_wrapper(&newTimer);
  _go32_dpmi_set_protected_mode_interrupt_vector(8, &newTimer);

  divisor = 1193180 / MSE_SAMPLE_RATE;

  outportb(0x43, 0x36);
  outportb(0x40, divisor & 0xFF);
  outportb(0x40, divisor >> 8);

  mseRunning = 1;
  
}

void mseCleanup() {

  mseRunning = 0;

  outportb(0x43, 0x36);
  outportb(0x40, 0);
  outportb(0x40, 0);

  _go32_dpmi_set_protected_mode_interrupt_vector(8, &oldTimer);
  _go32_dpmi_free_iret_wrapper(&newTimer);

  outportb(0x61, originalPort);
  
}
