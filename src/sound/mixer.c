/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neocdrx.h"
#include "streams.h"
#include "eq.h"

#define MIXBUFFER 16384
#define MIXMASK ((MIXBUFFER >> 2) -1)

static u8 mixbuffer[MIXBUFFER];	/*** 16k mixing buffer ***/
char mp3buffer[8192];		/*** Filled on each call by streamupdate ***/

static double mp3volume = 1.0f;
static double fxvolume = 1.0f;
static EQSTATE eqs;
static MIXER mixer;

/****************************************************************************
 * Nintendo GameCube SDL_MixAudio replacement function.
 *
 * This is adapted from the source of libsdl-1.2.9
 * As such it only contains the mixing function for 16BITMSB samples.
 ****************************************************************************/
void
ngcMixAudio (u8 * dst, u8 * src, int len, int volume)
//ngcMixAudio (Uint8 * dst, Uint8 * src, int len, int volume)
{
	
  s16 src1, src2;
  s16 *dsts = (s16 *) dst;
  s16 *srcs = (s16 *) src;

  int dst_sample;
  const int max_audioval = 32767;
  const int min_audioval = -32768;

  //len >>= 1;

  while (len--)
    {
      src1 = srcs[0];
      src2 = dsts[0];
      srcs++;

      dst_sample = (src1 + src2);

      if (dst_sample > max_audioval)
	{
	  dst_sample = max_audioval;
	}
      else if (dst_sample < min_audioval)
	{
	  dst_sample = min_audioval;
	}

      *dsts++ = (s16) dst_sample;
    }
}

static void
MP3MixAudio (char * dst, u8 * src, int len)
{
  s16 *s, *d;
  int i;
  double lsample;
  double rsample;

  s = (s16 *) src;
  d = (s16 *) dst;

  for (i = 0; i < len >> 1; i += 2)
    {
      lsample = ((int) ((double) d[i] * mp3volume) + (int)((double) s[i] * fxvolume));
      rsample = ((int) ((double) d[i + 1] * mp3volume) + (int)((double) s[i + 1] * fxvolume));

//     lsample = (int)((double) s[i] * fxvolume);
//      rsample = (int)((double) s[i + 1] * fxvolume);

      lsample = do_3band (&eqs, lsample);
      rsample = do_3band (&eqs, rsample);

      if (lsample < -32768)
	lsample = -32768;
      else
	{
	  if (lsample > 32767)
	    lsample = 32767;
	}

      if (rsample < -32768)
	rsample = -32768;
      else
	{
	  if (rsample > 32767)
	    rsample = 32767;
	}

      d[i] = (s16) lsample;
      d[i + 1] = (s16) rsample;

    }
}

/****************************************************************************
* audio_update
*
* Called from NeoCD every frame.
****************************************************************************/
void
mixer_update_audio (void)
{
  int i;
  int *src = (int *) mp3buffer;
  int *dst = (int *) mixbuffer;

  /*** Update from sound core ***/
  streamupdate (3200);
  MP3MixAudio (mp3buffer, (u8 *) play_buffer, 3200);

  /*** Update the mixbuffer ***/
  for (i = 0; i < 800; i++)
    {
      dst[mixer.head] = *src++;
      mixer.head++;
      mixer.head &= MIXMASK;
    }
}

/****************************************************************************
* mixer_init
****************************************************************************/
void
mixer_init (void)
{
  memset (&mixer, 0, sizeof (MIXER));
  memset (mp3buffer, 0, 8192);
  memset (mixbuffer, 0, MIXBUFFER);
  init_3band_state (&eqs, 880, 5000, 48000);
}

/****************************************************************************
* mixer_getaudio
****************************************************************************/
int
mixer_getaudio (u8 * outbuffer, int length)
{
  int *dst = (int *) outbuffer;
  int *src = (int *) mixbuffer;
  int donebytes = 0;

  /*** Always clear the buffer ***/
  memset (outbuffer, 0, length);

  while ((mixer.tail != mixer.head) && (donebytes < length))
    {
      *dst++ = src[mixer.tail];
      mixer.tail++;
      mixer.tail &= MIXMASK;
      donebytes += 4;
    }

  donebytes &= ~0x1f;

  if (donebytes == 0)
    return length >> 1;

  return donebytes;
}

/****************************************************************************
* mixer_set
****************************************************************************/
void mixer_set( float fx, float mp3, float l, float m, float h )
{
	mp3volume = (double)mp3;
	fxvolume = (double)fx;
	eqs.lg = (double)l;
	eqs.mg = (double)m;
	eqs.hg = (double)h;
}
