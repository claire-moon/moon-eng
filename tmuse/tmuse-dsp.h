#ifndef TMUSE_DSP_H
#define TMUSE_DSP_H

#define DSP_SAMPLE_RATE      22050
#define DSP_BUFFER_SIZE      4096

/* SB16 PORTS */

#define SB_RESET             0x226
#define SB_READ              0x22A
#define SB_WRITE             0x22C
#define SB_DATA_AVAIL        0x22E
#define SB_INTR_ACK          0x22F

/* SB16 DSP CMDS */

#define CMD_SET_TIME_CONSTANT 0x40
#define CMD_TURN_ON_SPEAKER   0xD1
#define CMD_TURN_OFF_SPEAKER  0xD3
#define CMD_HALT_DMA          0xD0
#define CMD_CONTINUE_DMA      0xD4
#define CMD_8BIT_AUTO_INIT    0x1C

/* DMA DOUBLE BUFFER */

extern unsigned char dspBuffer[DSP_BUFFER_SIZE];
extern volatile int dspHalfReady;
extern unsigned int dmaLinAddr;

/* FUNCS */

int dspInit(int basePort, int irq, int dma);

void dspCleanup();

#endif
