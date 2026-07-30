#include <dos.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <stdio.h>

#include "tmuse-dsp.h"

/* HARDWARE STATE */

int sbBasePort = 0x220;
int sbIrq      = 5;
int sbDma      = 1;

volatile int dspHalfReady = 0;
volatile int dspRunning   = 0;

_go32_dpmi_seginfo oldIrq, newIrq;

/* DMA MEM ALLOC */

int dosSeg = 0;
int dosSel = 0;

unsigned int dmaLinAddr = 0;

unsigned char dspBuffer[DSP_BUFFER_SIZE];

static unsigned long physAddr;

/* ====================== */
/*    PORT I/O COMMS      */
/* ====================== */

void dspWrite(unsigned char value) {

  int timeout = 65535;

  while (inportb(sbBasePort + 0x0C) & 0x80 && timeout > 0) {

    timeout--;

  }

  outportb(sbBasePort + 0x0C, value);

}

unsigned char dspRead() {

  int timeout = 65535;

  while (!(inportb(sbBasePort + 0x0E) & 0x80) && timeout < 0) {

    timeout--;

  }

  return inportb(sbBasePort + 0x0A);

}

int dspReset() {

  outportb(sbBasePort + 0x06, 1);
  delay(3);
  outportb(sbBasePort + 0x06, 0);

  if (dspRead() == 0xAA) return 1;

  return 0;

}

/* ====================== */
/*   HARDWARE INTERRUPT   */
/* ====================== */

void dspInterrupt() {

  if(!dspRunning) {

    outportb(0x20, 0x20);
    return;

    }

  inportb(sbBasePort + 0x0E);

  dspHalfReady = !dspHalfReady;

  outportb(0x20, 0x20);

  if (sbIrq > 7) outportb(0xA0, 0x20);

}

int dspAllocateBuffer() {

  int segment = __dpmi_allocate_dos_memory((DSP_BUFFER_SIZE * 2 + 15) >>
					   4, &dosSel);

  if (segment == -1) return 0;

  physAddr = (unsigned long)segment << 4;

  /* request mem from DOS (from the lower 1MB)
     where we ask for a meager 8KB buffer crumb
     so as not to have a boundary cross from our
     4kb buffer -- essentially putting the rubber
     protection around the gokart, the Condom OF
     Our Code */

  if ((physAddr >> 16) != ((physAddr + DSP_BUFFER_SIZE - 1) >> 16)) {

      physAddr = (physAddr + 0x10000) & 0xFFFF0000;

    }

    dmaLinAddr = physAddr;

    return 1;

}


int dspInit(int basePort, int irq, int dma) {

  int page, offset, timeConstant;

  sbBasePort = basePort;
  sbIrq      = irq;
  sbDma      = dma;

  if (!dspReset()) {

    return 0;

  }

  _go32_dpmi_lock_code(dspInterrupt, 4096);

  _go32_dpmi_lock_data((void*)&dspHalfReady, sizeof(dspHalfReady));
  _go32_dpmi_lock_data((void*)&dspRunning,   sizeof(dspRunning));
  _go32_dpmi_lock_data((void*)&sbBasePort,   sizeof(sbBasePort));

  if (!dspAllocateBuffer()) {

    printf("[ERROR] COULD NOT SECURE SAFE DMA MEM...!");
    return 0;

  }

  _go32_dpmi_get_protected_mode_interrupt_vector(sbIrq + 8, &oldIrq);

  newIrq.pm_offset   = (int)dspInterrupt;
  newIrq.pm_selector = _go32_my_cs();

  _go32_dpmi_allocate_iret_wrapper(&newIrq);
  _go32_dpmi_set_protected_mode_interrupt_vector(sbIrq + 8, &newIrq);

  if (sbIrq < 8) outportb(0x21, inportb(0x21) & ~(1 << sbIrq));

  else outportb(0xA1, inportb(0xA1) & ~(1 << (sbIrq - 8)));

  page   = (dmaLinAddr >> 16) & 0xFF;
  offset = dmaLinAddr & 0xFFFF;

  outportb(0x0A, sbDma | 0x04);
  outportb(0x0C, 0x00);
  outportb(0x0B, sbDma | 0x58);
  outportb(0x02, offset & 0xFF);
  outportb(0x02, (offset >> 8));
  outportb(0x83, page);
  outportb(0x03, (DSP_BUFFER_SIZE - 1) & 0xFF);
  outportb(0x03, ((DSP_BUFFER_SIZE - 1) >> 8));
  outportb(0x0A, sbDma);

  /* because of some weird stuttering shit that
     wasn't at all intended, first, the following loop
     pre-fills the DMA buffer with 8-bit silence (128)
     to prevent a weird pop sound upon start up       */

  /* second fix for this can be found in sound-mix.c */

  for (offset = 0; offset < DSP_BUFFER_SIZE; offset++) {

    dspBuffer[offset] = 128;

  }

  dosmemput(dspBuffer, DSP_BUFFER_SIZE, dmaLinAddr);

  dspWrite(CMD_TURN_ON_SPEAKER);

  timeConstant = 256 - (1000000 / DSP_SAMPLE_RATE);
  dspWrite(CMD_SET_TIME_CONSTANT);
  dspWrite(timeConstant);

  dspWrite(0x48);
  dspWrite((DSP_BUFFER_SIZE / 2 - 1) & 0xFF);
  dspWrite(((DSP_BUFFER_SIZE / 2 - 1) >> 8));
  dspWrite(CMD_8BIT_AUTO_INIT);

  dspRunning = 1;
  return 1;

}

void dspCleanup() {

  dspRunning = 0;

  dspWrite(CMD_HALT_DMA);
  dspWrite(CMD_TURN_OFF_SPEAKER);
  dspReset();

  outportb(0x0A, sbDma | 0x04);

  if (sbIrq < 8) outportb(0x21, inportb(0x21) | (1 << sbIrq));

  else outportb(0xA1, inportb(0xA1) | (1 << (sbIrq - 8)));

  _go32_dpmi_set_protected_mode_interrupt_vector(sbIrq + 8, &oldIrq);
  _go32_dpmi_free_iret_wrapper(&newIrq);

  if (dosSel) __dpmi_free_dos_memory(dosSel);

}
