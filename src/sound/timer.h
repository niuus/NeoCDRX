/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 */

#ifndef _TIMER_H_
#define _TIMER_H_


typedef struct timer_struct
{
  double time;
  unsigned int odo_debut;
  unsigned int nb_cycle;
  int param;
  unsigned int del_it;
  void (*func) (int param);
  struct timer_struct *next;
} timer_struct;

extern double timer_count;

timer_struct *insert_timer (double duration, int param, void (*func) (int));
void del_timer (timer_struct * ts);
void my_timer (void);
double timer_get_time (void);
void free_all_timer (void);

#endif
