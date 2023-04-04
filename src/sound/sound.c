#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "neocdrx.h"
#include "streams.h"
extern int YM2610_sh_start (void);
extern void YM2610_sh_stop (void);
int sound = 1;
#include "sound.h"

#define MIXER_MAX_CHANNELS 16
#define BUFFER_LEN 16384

extern Uint16 play_buffer[BUFFER_LEN];

void
update_sdl_stream (void *userdata, Uint8 * stream, int len)
{
  streamupdate (len);
  memcpy (stream, (Uint8 *) play_buffer, len);
}

int
init_sdl_audio (void)
{
  streams_sh_start ();
  YM2610_sh_start ();

  return 1;
}

void
sound_toggle (void)
{
  sound ^= 1;
}

void
sound_shutdown (void)
{
  streams_sh_stop ();
  YM2610_sh_stop ();
}
