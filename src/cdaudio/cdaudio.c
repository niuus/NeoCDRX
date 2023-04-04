/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/*** NeoCD/SDL CDaudio - modified for MP3 playback ***/

//-- Include files -----------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mad.h>
#include "neocdrx.h"
#include "madfilter.h"

//--- Madplay filter ---------------------------------------------------------
static struct audio_dither audio_left, audio_right;
static int mp3status;
static int mp3_init(void);
static int mp3sample_rate = 0;

//-- Private Variables -------------------------------------------------------
static int cdda_min_track;
static int cdda_max_track;
static int cdda_track_end;
static int cdda_loop_counter;

//-- Public Variables --------------------------------------------------------
int cdda_first_drive = 0;
int cdda_current_drive = 0;
int cdda_current_track = 0;
int cdda_current_frame = 0;
int cdda_playing = 0;
int cdda_autoloop = 0;
int cdda_volume = 0;
int cdda_disabled = 0;

//-- Function Prototypes -----------------------------------------------------
int cdda_init(void);
int cdda_play(int);
void cdda_stop(void);
void cdda_resume(void);
void cdda_shutdown(void);
void cdda_loop_check(void);
int cdda_get_disk_info(void);

static GENFILE mp3file = 0;

/*** libmad structures ***/
static struct mad_stream madStream;
static struct mad_frame madFrame;
static struct mad_synth madSynth;
static mad_timer_t madTimer;
static unsigned char *OutputPtr;
static unsigned char *OutBufferEnd;
static unsigned int FrameCounter;

/*** mp3 buffers ***/
#if use_SD || use_USB || use_IDE
#define MAD_INPUT_BUFFER (0x8000)
#else
#define MAD_INPUT_BUFFER (2*MAD_OUTPUT_BUFFER)
#endif
#define MAD_OUTPUT_BUFFER (8192*64)
//#define MAD_INPUT_BUFFER (8*MAD_OUTPUT_BUFFER)
//#endif
//#define MAD_OUTPUT_BUFFER (1024*16)
static unsigned char madInBuffer[MAD_INPUT_BUFFER + MAD_BUFFER_GUARD];
static unsigned char madOutBuffer[MAD_INPUT_BUFFER];

/*** Prototypes ***/
static void MAD_Destroy(void);
static int DecodeNextFrame(int samples);
static void MAD_Destroy(void);
static void MAD_Init(void);
static int needframe = 1;
static int madSamples = 0;
static int mp3end = 0;

//----------------------------------------------------------------------------
int cdda_init(void)
{
  if ( mp3file )
    GEN_fclose(mp3file);

  mp3file = 0;
  mp3_init();

  return 1;
}

//----------------------------------------------------------------------------
int cdda_get_disk_info(void)
{
  return 1;
}

//----------------------------------------------------------------------------
int cdda_play(int track)
{
  char Path[1024];

  if (cdda_disabled)
    return 1;
  if (cdda_playing && cdda_current_track == track)
    return 1;

  if (mp3file)
    GEN_fclose(mp3file);

  sprintf(Path, "%smp3/track%02d.mp3", cdpath, track);
  MAD_Init();

  mp3file = GEN_fopen(Path, (char *) "rb");
  if (mp3file)
    {
      mp3status = MP3PLAYING;
      mp3sample_rate = 0;
    }
  else
    {
      mp3status = MP3NOTPLAYING;
      cdda_disabled = 1;
      return 1;
    }

  cdda_current_track = track;
  cdda_loop_counter = 0;
  cdda_playing = 1;
  cdda_track_end = 2000000;
  mp3end = 0;
  return 1;
}

//----------------------------------------------------------------------------
void cdda_pause(void)
{
  if (cdda_disabled)
    return;

  mp3status = MP3PAUSED;
  cdda_playing = 0;
}


void cdda_stop(void)
{
  if (cdda_disabled)
    return;

  mp3status = MP3NOTPLAYING;

  cdda_playing = 0;
}

//----------------------------------------------------------------------------
void cdda_resume(void)
{
  if (cdda_disabled || cdda_playing)
    return;

  if (mp3status == MP3PAUSED)
    mp3status = MP3PLAYING;

  cdda_playing = 1;
}

//----------------------------------------------------------------------------
void cdda_shutdown(void)
{
  if (cdda_disabled)
    return;
}

//----------------------------------------------------------------------------
void cdda_loop_check(void)
{
  if (cdda_disabled)
    return;
  if (cdda_playing == 1)
    {
      cdda_loop_counter++;

      if (mp3end)
        {
          if (cdda_autoloop)
            cdda_play(cdda_current_track);
          else
            cdda_stop();
        }
    }
}

//---------------------------------------------------------------------------
static int mp3_init(void)
{
  int i;
  char Path[1024];

  cdda_min_track = cdda_max_track = 0;
  cdda_current_track = 0;
  cdda_playing = 0;
  cdda_loop_counter = 0;

  mp3status = MP3NOTPLAYING;

  for (i = 1; i < 60; i++)
    {
      sprintf(Path, "%smp3/track%02d.mp3", cdpath, i);
      mp3file = GEN_fopen(Path, (char *) "rb");
      if (mp3file)
        {
          if (!cdda_min_track)
            cdda_min_track = i;

          cdda_max_track = i;
          GEN_fclose(mp3file);
        }
    }

  mp3file = 0;

  if (!(cdda_min_track))
    cdda_disabled = 1;
  else
    cdda_disabled = 0;

  return 1;
}

//---------------------------------------------------------------------------
int mp3_decoder(int len, char *outbuffer)
{
  int bread;
  int j;
  int *src, *dst;
  double ratio;
  static int fixincr = 0;
  static int readlen;
  int fixofs = 0;

  memset(outbuffer, 0, len);

  if (mp3status != MP3PLAYING)
    return 0;

  if (!mp3sample_rate)
    {
      FrameCounter = 0;
      while (mp3sample_rate == 0)
        bread = DecodeNextFrame(4);		/*** Get a few bytes to see how much is really needed ***/

      /*** Now setup the samplerate values ***/
      ratio = (double) mp3sample_rate / (double) 48000.0;
      fixincr = (int) 65536.0 *ratio;
      readlen = (int) ((double) 3200.0 * ratio);
      readlen &= ~3;
    }

  bread = DecodeNextFrame(readlen);
  if (bread == 0) { }
  src = (int *) madOutBuffer;
  dst = (int *) outbuffer;
  for (j = 0; j < 1600; j++)
    {
      *dst++ = src[fixofs >> 16];
      fixofs += fixincr;
    }

  return readlen;
}

/****************************************************************************
* libMAD Stuff follows ...
****************************************************************************/
static int mp3_read(char *buffer, int len)
{
  int ret;

  if (mp3file == 0)
    return 0;

  ret = GEN_fread(buffer, 1, len, mp3file);

  if (ret <= 0)
    {
      cdda_track_end = cdda_loop_counter;
      mp3end = 1;
      ret = 0;
    }

  return ret;
}

static void MAD_Destroy(void)
{
  mad_timer_reset(&madTimer);
  mad_synth_finish(&madSynth);
  mad_frame_finish(&madFrame);
  mad_stream_finish(&madStream);
}

static void MAD_Init(void)
{

  static int madinited = 0;

  if (madinited)
    MAD_Destroy();

  mad_stream_init(&madStream);
  mad_frame_init(&madFrame);
  mad_synth_init(&madSynth);
  mad_timer_reset(&madTimer);
  memset(&madInBuffer, 0, MAD_INPUT_BUFFER + MAD_BUFFER_GUARD);
  memset(&audio_left, 0, sizeof(audio_left));
  memset(&audio_right, 0, sizeof(audio_right));
  OutputPtr = (unsigned char *) &madOutBuffer;
  OutBufferEnd = OutputPtr + MAD_OUTPUT_BUFFER;
  FrameCounter = 0;
  needframe = 1;
  madSamples = 0;
  madinited = 1;
  mp3sample_rate = 0;

}

static int MAD_DecodeFrame(void)
{
  size_t ReadSize, madRemaining;
  unsigned char *ReadStart = NULL;
  unsigned char *GuardPtr = NULL;
  if ((madStream.buffer == NULL) || (madStream.error == MAD_ERROR_BUFLEN))
    {

      /*** Determine buffer read ***/
      if (madStream.next_frame != NULL)
        {
          madRemaining = madStream.bufend - madStream.next_frame;
          memmove(madInBuffer, madStream.next_frame, madRemaining);
          ReadStart = (unsigned char *) (madInBuffer + madRemaining);
          ReadSize = MAD_INPUT_BUFFER - madRemaining;
        }

      else
        {
          ReadSize = MAD_INPUT_BUFFER;
          ReadStart = (unsigned char *) madInBuffer;
          madRemaining = 0;
        }
      /*** Read from buffer ***/
      ReadSize &= ~0x1f;	/*** For DVD must be 32byte aligned ***/
      ReadSize = mp3_read((char *)ReadStart, ReadSize);
      if (ReadSize == 0)
        {
          /*** End of file ***/
          GuardPtr = ReadStart + ReadSize;
          memset(GuardPtr, 0, MAD_BUFFER_GUARD);
          ReadSize += MAD_BUFFER_GUARD;
        }

      /*** Submit to mad stream decoder ***/
      mad_stream_buffer(&madStream, madInBuffer, ReadSize + madRemaining);
      madStream.error = 0;
    }
  if (mad_frame_decode(&madFrame, &madStream))
    {
      if (MAD_RECOVERABLE(madStream.error))
        {
          if (madStream.error != MAD_ERROR_LOSTSYNC
              || madStream.this_frame != GuardPtr)
            {
              return -1;
            }
        }

      else
        {
          if (madStream.error != MAD_ERROR_BUFLEN)
            return -1;
        }
    }

  if (FrameCounter == 0)
    mp3sample_rate = madFrame.header.samplerate;

  mad_timer_add(&madTimer, madFrame.header.duration);
  mad_synth_frame(&madSynth, &madFrame);

  FrameCounter++;
  return 0;
}

static int DecodeNextFrame(int samples)
{
  s32 Sample;
  signed short *p;
  OutputPtr = (unsigned char *) madOutBuffer;
  OutBufferEnd = OutputPtr + samples;

  p = (signed short *) madOutBuffer;
  memset(&madOutBuffer, 0, MAD_OUTPUT_BUFFER);

  while (OutputPtr != OutBufferEnd)
    {
      if (needframe)
        {
          if (MAD_DecodeFrame() != 0)
            {
              return (OutputPtr - madOutBuffer);
            }
          needframe = 0;
          madSamples = 0;
        }

      Sample =
        audio_linear_dither(madSynth.pcm.samples[0][madSamples],
                            &audio_left);
      *(p++) = Sample & 0xffff;

      if (MAD_NCHANNELS(&madFrame.header) == 2)
        Sample =
          audio_linear_dither(madSynth.pcm.samples[1][madSamples],
                              &audio_right);

      *(p++) = Sample & 0xffff;
      OutputPtr += 4;
      madSamples++;
      if (madSamples == madSynth.pcm.length)
        needframe = 1;
    }
  return OutputPtr - madOutBuffer;		  /*** Signal end ***/
}
