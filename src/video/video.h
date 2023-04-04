/*******************************************
**** VIDEO.H - Video Hardware Emulation ****
****            Header File             ****
*******************************************/

#ifndef	VIDEO_H
#define VIDEO_H

/*-- Defines ---------------------------------------------------------------*/
#define VIDEO_TEXT	0
#define VIDEO_NORMAL	1
#define	VIDEO_SCANLINES	2
#define NEOSCR_WIDTH 320
#define NEOSCR_HEIGHT 224

/*-- Global Variables ------------------------------------------------------*/
extern char video_vidram[0x20000];
extern unsigned short *video_paletteram_ng;
extern unsigned short video_palette_bank0_ng[4096];
extern unsigned short video_palette_bank1_ng[4096];
extern unsigned short *video_paletteram_pc;
extern unsigned short video_palette_bank0_pc[4096];
extern unsigned short video_palette_bank1_pc[4096];
extern int video_modulo;
extern int video_pointer;
extern unsigned short *video_paletteram;
extern unsigned short *video_paletteram_pc;
extern unsigned short video_palette_bank0[4096];
extern unsigned short video_palette_bank1[4096];
extern unsigned short *video_line_ptr[224];
extern unsigned char video_fix_usage[4096];
extern unsigned char video_spr_usage[0x10000];
extern unsigned int video_hide_fps;
extern unsigned short video_color_lut[32768];
extern int spr_disable;
extern int fix_disable;
extern int video_mode;
extern double gamma_correction;
extern int frameskip;

extern unsigned int neogeo_frame_counter;
extern unsigned int neogeo_frame_counter_speed;

/*-- video.c functions ----------------------------------------------------*/
int video_init (void);
void video_shutdown (void);
int video_set_mode (int);
void video_draw_screen1 (void);
void video_save_snapshot (void);
void video_draw_spr (unsigned int code, unsigned int color, int flipx,
		     int flipy, int sx, int sy, int zx, int zy);
void video_setup (void);
void video_fullscreen_toggle (void);
void video_mode_toggle (void);
void video_clear (void);
void blitter (void);
void savescreen (char *buffer);

/*-- draw_fix.c functions -------------------------------------------------*/
void video_draw_fix (void);
void fixputs (u16 x, u16 y, const char *string);

#endif /* VIDEO_H */
