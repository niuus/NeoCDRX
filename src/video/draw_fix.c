/******************************************
**** Fixed Text Layer Drawing Routines ****
******************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "neocdrx.h"

/* Draw Single FIX character */
INLINE void
draw_fix (u16 code, u16 colour, u16 sx, u16 sy, u16 * palette,
	  unsigned char *fix_memory, int opaque)
{
  u8 y;
  u32 mydword;
  u32 *fix = (u32 *) & (fix_memory[code << 5]);
  u16 *dest;
  u16 *paldata = (unsigned short *) &palette[colour];
  u16 col;

  for (y = 0; y < 8; y++)
    {
      dest = video_line_ptr[sy + y] + sx;
      mydword = *fix++;

      if (!opaque)
	{
	  col = (mydword >> 0) & 0x0f;
	  if (col)
	    dest[0] = paldata[col];
	  col = (mydword >> 4) & 0x0f;
	  if (col)
	    dest[1] = paldata[col];
	  col = (mydword >> 8) & 0x0f;
	  if (col)
	    dest[2] = paldata[col];
	  col = (mydword >> 12) & 0x0f;
	  if (col)
	    dest[3] = paldata[col];
	  col = (mydword >> 16) & 0x0f;
	  if (col)
	    dest[4] = paldata[col];
	  col = (mydword >> 20) & 0x0f;
	  if (col)
	    dest[5] = paldata[col];
	  col = (mydword >> 24) & 0x0f;
	  if (col)
	    dest[6] = paldata[col];
	  col = (mydword >> 28) & 0x0f;
	  if (col)
	    dest[7] = paldata[col];
	}
      else
	{
	  col = (mydword >> 0) & 0x0f;
	  dest[0] = paldata[col];
	  col = (mydword >> 4) & 0x0f;
	  dest[1] = paldata[col];
	  col = (mydword >> 8) & 0x0f;
	  dest[2] = paldata[col];
	  col = (mydword >> 12) & 0x0f;
	  dest[3] = paldata[col];
	  col = (mydword >> 16) & 0x0f;
	  dest[4] = paldata[col];
	  col = (mydword >> 20) & 0x0f;
	  dest[5] = paldata[col];
	  col = (mydword >> 24) & 0x0f;
	  dest[6] = paldata[col];
	  col = (mydword >> 28) & 0x0f;
	  dest[7] = paldata[col];
	}
    }
}


/* Draw entire Character Foreground */
void
video_draw_fix (void)
{
  u16 x, y;
  u16 code, colour;
  u16 *fixarea = (u16 *)(video_vidram + 0xE004);

  for (y = 0; y < 28; y++)
    {
      for (x = 0; x < 40; x++)
	{
	  code = fixarea[x << 5];

	  colour = (code & 0xf000) >> 8;
	  code &= 0xfff;

	  if (video_fix_usage[code])
	    draw_fix (code, colour, (x << 3), (y << 3), video_paletteram_pc,
		      neogeo_fix_memory, video_fix_usage[code] & 1);
	}
      fixarea++;
    }
}

/* FIX palette for fixputs*/
u16 palette[16] = { 0x0000, 0xffff, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000,
  0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff
};

void
fixputs (u16 x, u16 y, const char *string)
{
  u8 i;
  int length = strlen (string);

  if (y > 27)
    return;

  if (x + length > 40)
    {
      length = 40 - x;
    }

  if (length < 0)
    return;

  y <<= 3;

  for (i = 0; i < length; i++)
    {
      draw_fix (toupper ((u8)(string[i])) + 0x300, 0, (x + i) << 3, y, palette,
		neogeo_rom_memory + 0x7C000, 0);
    }

  return;
}
