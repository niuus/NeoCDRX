#ifndef __NGCMIXER__
#define __NGCMIXER__

typedef struct
{
  int head;
  int tail;
} MIXER;

void mixer_update_audio (void);
void mixer_init (void);
//void ngcMixAudio (Uint8 * dst, Uint8 * src, int len, int volume);
void ngcMixAudio (u8 * dst, u8 * src, int len, int volume);
int mixer_getaudio (u8 * outbuffer, int length);
void mixer_set( float fx, float mp3, float l, float m, float h );

extern char mp3buffer[8192];
#endif
