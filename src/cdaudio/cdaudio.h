/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

#ifndef	CDAUDIO_H
#define CDAUDIO_H

/*** NeoCD/SDL CDaudio - modified for MP3 playback ***/

//-- Exported Variables ------------------------------------------------------
extern int cdda_first_drive;
extern int cdda_nb_of_drives;
extern int cdda_current_drive;
extern int cdda_current_track;
extern int cdda_playing;
extern char drive_list[32];
extern int nb_of_drives;
extern int cdda_autoloop;
extern char cddapath[1024];

//-- Exported Functions ------------------------------------------------------
int cdda_init(void);
int cdda_play(int);
void cdda_pause(void);
void cdda_stop(void);
void cdda_resume(void);
void cdda_shutdown(void);
void cdda_loop_check(void);
int cdda_get_disk_info(void);
void cdda_build_drive_list(void);
int cdda_get_volume(void);
void cdda_set_volume(int volume);
void audio_setup(void);

//-- libMP3 -----------------------------------------------------------------
int mp3_decoder(int len, char *outbuffer);

#define MP3PLAYING 1
#define MP3NOTPLAYING 2
#define MP3PAUSED 3

#endif				/* CDAUDIO_H */
