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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neocdrx.h"

CPU CPU_Z80;
static CPU CPU_M68K;

#define TIMESLICE 264
#define M68SEC 12000000
#define Z80SEC   4000000
#define M68FRAME ( M68SEC / 60 )
#define Z80FRAME ( Z80SEC / 60 )
#define Z80SCANLINE ( Z80FRAME / TIMESLICE )
#define M68SCANLINE ( M68FRAME / TIMESLICE )

static int inited = 0;
static int raster_interrupt_enabled = 0;
int scanline = 0;

/****************************************************************************
* Individual Game Configs
*
* List contains games which do not match the standard configuration
****************************************************************************/
static GAMECONFIG gameconfig[] = {
  {"THE LAST BLADE 2", 1, 1},	/* 1 */
  {"ART OF FIGHTING2", 0, 1},	/* 2 */
  {"FATAL FURY 3", 0, 1},	/* 3 */
  {"F. HISTORY", 0, 1},		/* 5 */
  {"NEO TURF MASTERS", 0, 1},	/* 6 */
  {"BREAKERS", 0, 1},		/* 7 */
  {"THE LAST BLADE", 0, 1},	/* 8 */
  {"K.O.F.'98", 0, 1},		/* 9 */
  {"DARK KOMBAT", 0, 1},	/* 10 */
  {"OVER  TOP", 0, 1},		/* 11 */
  {"NINJA COMBAT", 0, 1},	/* 12 */
  {"RIDING HERO", 0, 2},	/* 13 */
  {"SENGOKU", 0, 1},		/* 14 */
  {"SENGOKU 2", 0, 1},		/* 15 */
  {"Top Hunter", 0, 1},		/* 16 */
  {"TOP PLAYERS GOLF", 0, 1},	/* 17 */
  {"SUPER SIDEKICKS2", 0, 1},	/* 18 */
  {"POWER SPIKES II", 0, 1},	/* 19 */
  {"SUPER SIDEKICKS3", 0, 1},	/* 20 */
  {"SamuraiShodown 3", 0, 1},	/* 21 */
  {"PULSTAR", 0, 1},		/* 22 */
  {"THRASH RALLY CD", 0, 1},	/* 23 */
  {"VIEW-POINT", 0, 1},		/* 27 */
  {"WINDJAMMERS", 0, 1},	/* 28 */
  {"STREET SLAM", 0, 1},	/* 29 */
  {"",0,0}
};

/****************************************************************************
* NeoGeo Configure Game
* Game name is always passed as ENGLISH name
****************************************************************************/
void
neogeo_configure_game (char *gamename)
{
  int i = 0;
	
  nowait_irqack = 0;
  raster_interrupt_enabled = 0;

  while( strlen(gameconfig[i].gamename) )
    {
      if (stricmp (gamename, gameconfig[i].gamename) == 0)
	{
	  nowait_irqack = gameconfig[i].nowait_irqack;
	  raster_interrupt_enabled = gameconfig[i].raster_interrupt_enabled;

	  /*** be verbose ***/
	  /*
	  ActionScreen(gamename);
	  */
	  break;
	}
	i++;
    }

  CPU_Z80.boost = CPU_M68K.boost = 0;
}

/****************************************************************************
* neogeo_runframe
*
* This function will run a complete video frame for NeoCD.
* CPU initialization etc is performed in neocd.c as usual.
****************************************************************************/
void
neogeo_runframe (void)
{
  int cycles;

  if (!inited)
    {
      memset (&CPU_Z80, 0, sizeof (CPU));
      memset (&CPU_M68K, 0, sizeof (CPU));

      CPU_Z80.total_cycles = 0.0;
      CPU_M68K.total_cycles = 0.0;

      inited = 1;
    }

	/*** Set CPU cycles for this frame ***/
  CPU_Z80.cycles_frame = Z80FRAME + CPU_Z80.boost;
  CPU_M68K.cycles_frame = M68FRAME + CPU_M68K.boost;

	/*** Set CPU cycles for scanline ***/
  CPU_Z80.cycles_scanline = ((CPU_Z80.cycles_frame) / TIMESLICE);
  CPU_M68K.cycles_scanline = (CPU_M68K.cycles_frame) / TIMESLICE;

	/*** Clear done cycles ***/
  CPU_Z80.cycles_done = CPU_M68K.cycles_done = 0;

  for (scanline = TIMESLICE - 1; scanline >= 0; scanline--)
    {
		/*** Run M68K ***/
      if (CPU_M68K.cycles_done < CPU_M68K.cycles_frame)
	{
	  if (scanline)
	    cycles = m68k_execute (CPU_M68K.cycles_scanline);
	  else
	    cycles =
	      m68k_execute (CPU_M68K.cycles_frame - CPU_M68K.cycles_done);

	  CPU_M68K.cycles_done += cycles;
	  CPU_M68K.total_cycles += cycles;
	}

		/*** Run Z80 ***/
      if ((CPU_Z80.cycles_done < CPU_Z80.cycles_frame) && ( cpu_enabled))
	{
	  if (scanline)
	    cycles = mz80exec (CPU_Z80.cycles_scanline);
	  else
	    cycles = mz80exec (CPU_Z80.cycles_frame - CPU_Z80.cycles_done);

	  CPU_Z80.cycles_done += cycles;
	  CPU_Z80.total_cycles += cycles;
	  my_timer ();
	}

      switch (raster_interrupt_enabled)
	{
	case 0:
	  neogeo_interrupt ();
	  break;

	case 1:
	  neogeo_raster_interrupt ();
	  break;

	case 2:
	  neogeo_raster_interrupt_busy ();
	  break;
	}
    }

	/*** Adjust processors ***/
  CPU_Z80.total_time_us = (CPU_Z80.total_cycles * Z80_USEC);
  CPU_M68K.total_time_us = (CPU_M68K.total_cycles * M68K_USEC);
  CPU_Z80.boost = CPU_M68K.boost = 0;

  if (CPU_Z80.total_time_us > CPU_M68K.total_time_us)
    CPU_M68K.boost =
      (int) ((double) M68SEC *
	     (CPU_Z80.total_time_us - CPU_M68K.total_time_us));
  else
    CPU_Z80.boost =
      (int) ((double) Z80SEC *
	     (CPU_M68K.total_time_us - CPU_Z80.total_time_us));

}
