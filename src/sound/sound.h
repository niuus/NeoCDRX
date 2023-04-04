#ifndef SOUND_H
#define SOUND_H

#include "neocdrx.h"
#define NB_SEGMENT 20

int init_sdl_audio (void);
void sound_shutdown (void);
void sound_toggle (void);

#endif
