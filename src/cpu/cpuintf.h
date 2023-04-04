/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* NeoCD/SDL CPU Timer Interface
*
* Module to schedule CPU, sound timers and interrupts
****************************************************************************/
#ifndef __CPUINTF__
#define __CPUINTF__

#define Z80_USEC   ((1.0 / 4000000.0))
#define M68K_USEC ((1.0 / 12000000.0 ))

typedef struct
{
  int cycles_done;
  int cycles_frame;
  int cycles_scanline;
  int cycles_overrun;
  int irq_state;
  double total_cycles;
  double total_time_us;
  int boost;
} CPU;

typedef struct
{
  char gamename[20];
  int nowait_irqack;
  int raster_interrupt_enabled;
} GAMECONFIG;

void neogeo_runframe (void);
void neogeo_configure_game (char *gamename);

#endif
