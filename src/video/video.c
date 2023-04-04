/*******************************************
**** VIDEO.C - Video Hardware Emulation ****
*******************************************/

//-- Include Files -----------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "neocdrx.h"

//-- Defines -----------------------------------------------------------------
#define VIDEO_TEXT		0
#define VIDEO_NORMAL	1
#define VIDEO_SCANLINES	2

#define LINE_BEGIN mydword = (*((unsigned int *)fspr))
#define LINE_MID	mydword = (*((unsigned int *)fspr+1))
#define PIXEL_LAST	col = (mydword&0x0F); if (col) *bm = paldata[col]
#define PIXEL_R		col = (mydword&0x0F); if (col) *bm = paldata[col]; bm--
#define PIXEL_F		col = (mydword&0x0F); if (col) *bm = paldata[col]; bm++
#define PIXEL_LAST_OPAQUE col = (mydword&0x0F); *bm = paldata[col]
#define PIXEL_R_OPAQUE col = (mydword&0x0F); *bm = paldata[col]; bm--
#define PIXEL_F_OPAQUE col = (mydword&0x0F); *bm = paldata[col]; bm++
#define SHIFT7		mydword >>= 28
#define SHIFT6		mydword >>= 24
#define SHIFT5		mydword >>= 20
#define SHIFT4		mydword >>= 16
#define SHIFT3		mydword >>= 12
#define SHIFT2		mydword >>= 8
#define SHIFT1		mydword >>= 4

//-- Global Variables --------------------------------------------------------
char video_vidram[0x20000];
unsigned short *video_paletteram_ng;
unsigned short video_palette_bank0_ng[4096];
unsigned short video_palette_bank1_ng[4096];
unsigned short *video_paletteram_pc;
unsigned short video_palette_bank0_pc[4096];
unsigned short video_palette_bank1_pc[4096];
unsigned short video_color_lut[32768];
u32 video_color_long[0x10000];
u32 video_scan_long[0x10000];

int video_modulo;
int video_pointer;
int video_dos_cx;
int video_dos_cy;
int video_mode = 0;
unsigned short *video_line_ptr[224];
unsigned char video_fix_usage[4096];
unsigned char video_spr_usage[0x10000];
static char videobuffer[(NEOSCR_WIDTH * (NEOSCR_HEIGHT + 16)) * 2];
static char *video_buffer = videobuffer;
u8 *SrcPtr;
u8 *DestPtr;
unsigned char video_shrinky[17];
unsigned char full_y_skip[16] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

unsigned int neogeo_frame_counter = 0;
unsigned int neogeo_frame_counter_speed = 4;

unsigned int video_hide_fps = 1;
int video_scanlines_capable = 1;
double gamma_correction = 1.0;

int fullscreen_flag = 0;
int display_mode = 1;
int snap_no;
int frameskip = 0;

//-- Function Prototypes -----------------------------------------------------
int video_init (void);
void video_shutdown (void);
void video_draw_line (int);
int video_set_mode (int);
void video_mode_toggle (void);
void video_fullscreen_toggle (void);
void incframeskip (void);
void video_precalc_lut (void);
void video_flip_pages (void);
void video_draw_spr (unsigned int code, unsigned int color, int flipx,
		     int flipy, int sx, int sy, int zx, int zy);
void video_draw_screen1 (void);
void video_draw_screen2 (void);
void snapshot_init (void);
void video_save_snapshot (void);
void video_setup (void);
void video_draw_spr_opaque (unsigned int code, unsigned int color, int flipx,
			    int flipy, int sx, int sy, int zx, int zy);

static inline u32
VFLIP32 (unsigned int b)
{
  unsigned int c;
  c = (b & 0xff000000) >> 24;
  c |= (b & 0xff0000) >> 8;
  c |= (b & 0xff00) << 8;
  c |= (b & 0xff) << 24;
  return c;
}
static inline unsigned short
VFLIP16 (unsigned short b)
{
  return ((b & 0xff00) >> 8) | ((b & 0xff) << 8);
}

//----------------------------------------------------------------------------
int
video_init (void)
{
  int y;
  unsigned short *ptr;

  video_precalc_lut ();

  memset (video_palette_bank0_ng, 0, 8192);
  memset (video_palette_bank1_ng, 0, 8192);
  memset (video_palette_bank0_pc, 0, 8192);
  memset (video_palette_bank1_pc, 0, 8192);
  memset (video_spr_usage, 0, 0x10000);
  memset (video_vidram, 0, 0x20000);
  memset (video_buffer, 0, (NEOSCR_WIDTH * NEOSCR_HEIGHT) * 2);

  video_paletteram_ng = video_palette_bank0_ng;
  video_paletteram_pc = video_palette_bank0_pc;
  video_modulo = 0;
  video_pointer = 0;

  ptr = (unsigned short *) (video_buffer);

  for (y = 0; y < 224; y++)
    {
      video_line_ptr[y] = ptr;
      ptr += 320;
    }

  return 1;
}

//----------------------------------------------------------------------------
void
video_shutdown (void)
{
}

//----------------------------------------------------------------------------
int
video_set_mode (int mode)
{
  return 1;
}

//----------------------------------------------------------------------------
void
video_precalc_lut (void)
{
  int ndx, rr, rg, rb;

  for (rr = 0; rr < 32; rr++)
    {
      for (rg = 0; rg < 32; rg++)
	{
	  for (rb = 0; rb < 32; rb++)
	    {
	      ndx =
		((rr & 1) << 14) | ((rg & 1) << 13) | ((rb & 1) << 12) |
		((rr & 30) << 7) | ((rg & 30) << 3) | ((rb & 30) >> 1);
	      video_color_lut[ndx] =
		((int) (31 * pow ((double) rb / 31, 1 / gamma_correction)) <<
		 0) | ((int) (63 *
			      pow ((double) rg / 31,
				   1 / gamma_correction)) << 5) | ((int) (31 *
									  pow
									  ((double) rr / 31, 1 / gamma_correction)) << 11);
	    }
	}
    }

  for (ndx = 0; ndx < 0x10000; ndx++)
    {
      video_color_long[ndx] = ndx | (ndx << 16);
      video_scan_long[ndx] = (video_color_long[ndx] >> 1) & 0x7bef7bef;
    }

}

//----------------------------------------------------------------------------
void
video_draw_screen1 ()
{
  //static unsigned int fc;
  int sx = 0, sy = 0, oy = 0, my = 0, zx = 1, rzy = 1;
  int offs, i, count, y;
  int tileno, tileatr, t1, t2, t3;
  char fullmode = 0;
  int ddax = 0, dday = 0, rzx = 15, yskip = 0;
//  int pass1 = 0;
//  int t;

  if (!video_enable)
    {
      video_clear ();
      return;
    }

  if (!spr_disable)
    {

//      if ( patch_ssrpg )
//      {
//	t = *(u16*)(video_vidram + 0x10404);

//	if ( ( t & 0x40 ) == 0 )
//	{
//		for ( pass1 = 6; pass1 < 0x300; pass1 += 2 )
//		{
//			t = *(u16*)(video_vidram + 0x10400 + pass1);
//			if ( ( t & 0x40 ) == 0 )
//				break;
//		}
		
//		if ( pass1 == 6 )
//			pass1 = 0;
//	}
//      }
//      else
//        pass1 = 0;

      //fill background colour
      int f;
      unsigned short *vxfb = (unsigned short *) video_buffer;
      for (f = 0; f < (320 * 224); f++)
	vxfb[f] = video_paletteram_pc[4095];

//      for (count = pass1; count < 0x300; count += 2)
      for (count = 0; count < 0x300; count += 2)
	{
	  t3 = *((unsigned short *) (&video_vidram[0x10000 + count]));
	  t1 = *((unsigned short *) (&video_vidram[0x10400 + count]));
	  t2 = *((unsigned short *) (&video_vidram[0x10800 + count]));

		// If this bit is set this new column is placed next to last one
		if (t1 & 0x40) {
		//sx += rzx;            /* new x */
		sx += (rzx + 1);
		  // if (sx >= 0x1F0)    /* x>496 => x-=512 */
		  //      sx -= 0x200;
		// Wiimpathy : Wii right screen and black screen fix
		// FIXME : without this workaround the Wii crashes, reason unknown!
		if ( sx >= 512 )    /* x>496 => x-=512 */
                sx -= 512;
		/* Get new zoom for this column */
			zx = (t3 >> 8) & 0x0F;
			sy = oy;                /* y pos = old y pos */
		} else {	/* nope it is a new block */
		/* Sprite scaling */
			zx = (t3 >> 8) & 0x0F;    /* zoom x */
			rzy = t3 & 0xff;          /* zoom y */
			if (rzy==0) continue;
			sx = (t2 >> 7);
			//if (sx >= 0x1F0)
			//sx -= 0x200;

	      // Number of tiles in this strip
	      my = t1 & 0x3f;
	      if (my == 0x20)
		fullmode = 1;
	      else if (my >= 0x21)
		fullmode = 2;	// most games use 0x21, but
	      else
		fullmode = 0;	// Alpha Mission II uses 0x3f

	      sy = 0x1F0 - (t1 >> 7);
	      if (sy > 0x100)
		sy -= 0x200;

	     if (fullmode == 2 || (fullmode == 1 && rzy == 0xff))
		{
			//while (sy < -16) sy += (rzy + 1) << 1; //2 * (rzy + 1);
			while (sy < -16)
			sy += 2 * (rzy + 1);
		}
		oy = sy;

		 if(my==0x21) my=0x20;

		 else if(rzy!=0xff && my!=0)
		 //my = ((my<<4<<8)/(rzy+1)+15)>>4; //my = ((my*16*256)/(rzy+1)+15)/16;
		 my = ((my * 16 * 256) / (rzy + 1) + 15) / 16;

		 if(my>0x20) my=0x20;
		  ddax = 0;		// setup x zoom
		  if (ddax == 0) { }
	    }

	  rzx = zx;

	 // No point doing anything if tile strip is 0
	 //if ((my == 0) || (sx > 311))
      if ((my==0)||(sx>=320)||(sx <-16)) // 320
		continue;

	  // Setup y zoom
	  if(rzy==255)
		yskip=16;
	  else
		dday=0;   // =256; NS990105 mslug fix

	  offs = count<<6;

	  // my holds the number of tiles in each vertical multisprite block
	  for (y = 0; y < my; y++)
	    {
	      tileno = *((unsigned short *) (&video_vidram[offs]));
	      offs += 2;
	      tileatr = *((unsigned short *) (&video_vidram[offs]));
	      offs += 2;

	      if (tileatr & 0x8)
		tileno = (tileno & ~7) | (neogeo_frame_counter & 7);
	      else if (tileatr & 0x4)
		tileno = (tileno & ~3) | (neogeo_frame_counter & 3);

//                      tileno &= 0x7FFF;
	      if (tileno > 0x7FFF)	/*** Fatal Fury 3 uses tiles up to 34000 ***/
		continue;

	      if (rzy != 255)
		{
		  yskip = 0;
		  video_shrinky[0] = 0;
		  for (i = 0; i < 16; i++)
		    {
		      video_shrinky[i + 1] = 0;
		      dday -= rzy + 1;
		      if (dday <= 0)
			{
			  dday += 256;
			  yskip++;
			  video_shrinky[yskip]++;
			}
		      else
			video_shrinky[yskip]++;
		    }
		}

	    if (fullmode == 2 || (fullmode == 1 && rzy == 0xff)) 
         { 
            if (sy >= 248) sy -= (rzy + 1) << 1; //2 * (rzy + 1); 
         } 
         else if (fullmode == 1) 
         { 
            if (y == 0x10) sy -= (rzy + 1) << 1; //2 * (rzy + 1); 
         } 
         else if (sy > 0x110) sy -= 0x200;// NS990105 mslug2 fix*/

/*if (fullmode == 2 || (fullmode == 1 && rzy == 0xff))
         {
            if (sy >= 248) sy -= 2 * (rzy + 1);
         }
         else if (fullmode == 1)
         {
            if (y == 0x10) sy -= 2 * (rzy + 1);
         }
         else if (sy > 0x110) sy -= 0x200; */


	      if ((tileatr >> 8) && (sy < 224) && video_spr_usage[tileno])
		{
		  if (video_spr_usage[tileno] & 1)
		    video_draw_spr_opaque (tileno,
					   tileatr >> 8,
					   tileatr & 0x01, tileatr & 0x02,
					   sx, sy, rzx, yskip);
		  else
		    video_draw_spr (tileno,
				    tileatr >> 8,
				    tileatr & 0x01, tileatr & 0x02,
				    sx, sy, rzx, yskip);
		}

	      sy += yskip;
	    }			// for y
	}			// for count
    }

  if (!fix_disable)
    video_draw_fix ();

    /*** Do clipping ***/
  for (count = 0; count < 224; count++)
    {
      for (sx = 0; sx < 8; sx++)
	video_line_ptr[count][sx] = video_line_ptr[count][sx + 311] = 0;
    }

  update_video (320, 224, video_buffer);

}

//       Without  flip              With Flip
// 01: X0000000 00000000        00000000 0000000X
// 02: X0000000 X0000000        0000000X 0000000X
// 03: X0000X00 00X00000        00000X00 00X0000X
// 04: X000X000 X000X000        000X000X 000X000X
// 05: X00X00X0 0X00X000        000X00X0 0X00X00X
// 06: X0X00X00 X0X00X00        00X00X0X 00X00X0X
// 07: X0X0X0X0 0X0X0X00        00X0X0X0 0X0X0X0X
// 08: X0X0X0X0 X0X0X0X0        0X0X0X0X 0X0X0X0X
// 09: XX0X0X0X X0X0X0X0        0X0X0X0X X0X0X0XX
// 10: XX0XX0X0 XX0XX0X0        0X0XX0XX 0X0XX0XX
// 11: XXX0XX0X X0XX0XX0        0XX0XX0X X0XX0XXX
// 12: XXX0XXX0 XXX0XXX0        0XXX0XXX 0XXX0XXX
// 13: XXXXX0XX XX0XXXX0        0XXXX0XX XX0XXXXX
// 14: XXXXXXX0 XXXXXXX0        0XXXXXXX 0XXXXXXX
// 15: XXXXXXXX XXXXXXX0        0XXXXXXX XXXXXXXX
// 16: XXXXXXXX XXXXXXXX        XXXXXXXX XXXXXXXX

void
video_draw_spr (unsigned int code, unsigned int color, int flipx,
		int flipy, int sx, int sy, int zx, int zy)
{
  int oy, ey, y, dy;
  unsigned short *bm;
  int col;
  int l;
  unsigned int mydword;
  unsigned char *fspr = 0;
  unsigned char *l_y_skip;
  const unsigned short *paldata;


  // Check for total transparency, no need to draw
  //if (video_spr_usage[code] == 0)
  //      return;

  if (sx <= -8)
    return;

  if (zy == 16)
    l_y_skip = full_y_skip;
  else
    l_y_skip = video_shrinky;

  fspr = neogeo_spr_memory;

  // Mish/AJP - Most clipping is done in main loop
  oy = sy;
  ey = sy + zy - 1;		// Clip for size of zoomed object

  if (sy < 0)
    sy = 0;
  if (ey >= 224)
    ey = 223;

  if (flipy)			// Y flip
    {
      dy = -8;
      fspr += (code + 1) * 128 - 8 - (sy - oy) * 8;
    }
  else				// normal
    {
      dy = 8;
      fspr += code * 128 + (sy - oy) * 8;
    }

  paldata = (unsigned short *) &video_paletteram_pc[color * 16];

  if (flipx)			// X flip
    {
      l = 0;
      switch (zx)
	{
	case 0:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_MID;
	      SHIFT7;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 1:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 1;
	      LINE_BEGIN;
	      SHIFT7;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT7;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 2:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 2;
	      LINE_BEGIN;
	      SHIFT5;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT5;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 3:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 3;
	      LINE_BEGIN;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT4;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT4;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 4:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 4;
	      LINE_BEGIN;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT3;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT3;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 5:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 5;
	      LINE_BEGIN;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT3;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 6:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 6;
	      LINE_BEGIN;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 7:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 7;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 8:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 8;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      LINE_MID;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 9:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 9;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 10:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 10;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      LINE_MID;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 11:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 11;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 12:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 12;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT2;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 13:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 13;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 14:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 14;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 15:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 15;
	      LINE_BEGIN;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      LINE_MID;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_R;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	}
    }
  else				// normal
    {
      l = 0;
      switch (zx)
	{
	case 0:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 1:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 2:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT5;
	      PIXEL_F;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 3:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT4;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT4;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 4:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT3;
	      PIXEL_F;
	      SHIFT3;
	      PIXEL_F;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT3;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 5:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT3;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT3;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 6:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 7:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 8:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 9:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 10:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 11:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 12:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT2;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 13:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 14:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	case 15:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      LINE_MID;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_F;
	      SHIFT1;
	      PIXEL_LAST;
	      l++;
	    }
	  break;
	}
    }
}


void
video_draw_spr_opaque (unsigned int code, unsigned int color, int flipx,
		       int flipy, int sx, int sy, int zx, int zy)
{
  int oy, ey, y, dy;
  unsigned short *bm;
  int col;
  int l;
  unsigned int mydword;
  unsigned char *fspr = 0;
  unsigned char *l_y_skip;
  const unsigned short *paldata;


  // Check for total transparency, no need to draw
  //if (video_spr_usage[code] == 0)
  //      return;

  if (sx <= -8)
    return;

  if (zy == 16)
    l_y_skip = full_y_skip;
  else
    l_y_skip = video_shrinky;

  fspr = neogeo_spr_memory;

  // Mish/AJP - Most clipping is done in main loop
  oy = sy;
  ey = sy + zy - 1;		// Clip for size of zoomed object

  if (sy < 0)
    sy = 0;
  if (ey >= 224)
    ey = 223;

  if (flipy)			// Y flip
    {
      dy = -8;
      fspr += (code + 1) * 128 - 8 - (sy - oy) * 8;
    }
  else				// normal
    {
      dy = 8;
      fspr += code * 128 + (sy - oy) * 8;
    }

  paldata = (unsigned short *) &video_paletteram_pc[color * 16];

  if (flipx)			// X flip
    {
      l = 0;
      switch (zx)
	{
	case 0:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_MID;
	      SHIFT7;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 1:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 1;
	      LINE_BEGIN;
	      SHIFT7;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT7;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 2:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 2;
	      LINE_BEGIN;
	      SHIFT5;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT5;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 3:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 3;
	      LINE_BEGIN;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT4;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT4;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 4:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 4;
	      LINE_BEGIN;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT3;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 5:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 5;
	      LINE_BEGIN;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT3;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 6:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 6;
	      LINE_BEGIN;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 7:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 7;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 8:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 8;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 9:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 9;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 10:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 10;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 11:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 11;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 12:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 12;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT2;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 13:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 13;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 14:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 14;
	      LINE_BEGIN;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 15:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx + 15;
	      LINE_BEGIN;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      LINE_MID;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_R_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	}
    }
  else				// normal
    {
      l = 0;
      switch (zx)
	{
	case 0:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 1:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 2:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT5;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 3:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT4;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT4;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 4:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT3;
	      PIXEL_F_OPAQUE;
	      SHIFT3;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT3;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 5:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT3;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT3;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 6:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 7:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 8:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 9:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 10:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 11:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 12:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT2;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 13:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 14:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	case 15:
	  for (y = sy; y <= ey; y++)
	    {
	      fspr += l_y_skip[l] * dy;
	      bm = (video_line_ptr[y]) + sx;
	      LINE_BEGIN;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      LINE_MID;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_F_OPAQUE;
	      SHIFT1;
	      PIXEL_LAST_OPAQUE;
	      l++;
	    }
	  break;
	}
    }
}

/****************************************************************************
* video_clear
****************************************************************************/
void
video_clear (void)
{
  memset (video_buffer, 0, (NEOSCR_WIDTH * (NEOSCR_HEIGHT + 16)) * 2);
}

/****************************************************************************
* blitter
****************************************************************************/
void
blitter (void)
{
  update_video (320, 224, video_buffer);
}

/****************************************************************************
* savescreen
****************************************************************************/
void
savescreen (char *buffer)
{
  memcpy (buffer, video_buffer, (640 * 224));
}
