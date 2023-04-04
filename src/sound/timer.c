/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 */

#include <stdlib.h>
#include <stdio.h>
#include "neocdrx.h"

extern CPU CPU_Z80;

double timer_count;
timer_struct *timer_list;
#define MAX_TIMER 3
timer_struct timers[MAX_TIMER];
int nb_interlace = 264 * 4;
//int nb_timer=0;

double
timer_get_time (void)
{
  return timer_count;
}

timer_struct *
insert_timer (double duration, int param, void (*func) (int))
{
  int i;
  for (i = 0; i < MAX_TIMER; i++)
    {
      if (timers[i].del_it)
	{
	  timers[i].time = timer_count + duration;
	  timers[i].param = param;
	  timers[i].func = func;
	  //printf("Insert_timer %d duration=%f param=%d\n",i,timers[i].time,timers[i].param);
	  timers[i].del_it = 0;
	  return &timers[i];
	}
    }
  printf ("YM2610: No timer free!\n");
  return NULL;			/* No timer free */
}

void
free_all_timer (void)
{
  int i;
  for (i = 0; i < MAX_TIMER; i++)
    {
      timers[i].del_it = 1;
    }
}

void
del_timer (timer_struct * ts)
{
  ts->del_it = 1;
}

static double inc;

void
my_timer (void)
{

  static int init = 1;
  int i;

  if (init)
    {
      //timer_init_save_state();
      init = 0;
      if (0)
	{
	  inc = (double) (0.02) / nb_interlace;
	  //(conf.sound ? (double) nb_interlace : 1.0);
	}
      else
	{
	  inc = (double) (0.01666) / nb_interlace;
	  //(conf.sound ? (double) nb_interlace : 1.0);
	}
      for (i = 0; i < MAX_TIMER; i++)
	timers[i].del_it = 1;

    }

  //timer_count += inc;               /* 16ms par frame */

	/*** This keeps the YM2610 in sync with the Z80 ***/
  timer_count = (CPU_Z80.total_cycles * Z80_USEC);

  for (i = 0; i < MAX_TIMER; i++)
    {
      if (timer_count >= timers[i].time && timers[i].del_it == 0)
	{
	  //printf("Timer_expire %d duration=%f param=%d\n",i,timers[i].time,timers[i].param);
	  if (timers[i].func)
	    timers[i].func (timers[i].param);
	  timers[i].del_it = 1;
	}
    }
}
