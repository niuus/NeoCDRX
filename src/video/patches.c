/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* NeoGeo CD
*
* VRAM patches from NeoGeoCDZ
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neocdrx.h"

/****************************************************************************
* Patch Fatal Fury 2
****************************************************************************/
void
patch_vram_rbff2 (void)
{
  UINT16 offs;
  UINT16 *neogeo_vidram16 = (UINT16 *) video_vidram;

  for (offs = 0; offs < ((0x300 >> 1) << 6); offs += 2)
    {
      UINT16 tileno = neogeo_vidram16[offs];
      UINT16 tileatr = neogeo_vidram16[offs + 1];

      if (tileno == 0x7a00 && (tileatr == 0x4b00 || tileatr == 0x1400))
	{
	  neogeo_vidram16[offs] = 0x7ae9;
	  return;
	}
    }
}

/****************************************************************************
* Patch ADK world
****************************************************************************/
void
patch_vram_adkworld (void)
{
  UINT16 offs;
  UINT16 *neogeo_vidram16 = (UINT16 *) video_vidram;

  for (offs = 0; offs < ((0x300 >> 1) << 6); offs += 2)
    {
      UINT16 tileno = neogeo_vidram16[offs];
      UINT16 tileatr = neogeo_vidram16[offs + 1];

      if ((tileno == 0x14c0 || tileno == 0x1520) && tileatr == 0x0000)
	neogeo_vidram16[offs] = 0x0000;
    }
}

/****************************************************************************
* Patch CrossSwords 2
****************************************************************************/
void
patch_vram_crsword2 (void)
{
  UINT16 offs;
  UINT16 *neogeo_vidram16 = (UINT16 *) video_vidram;

  for (offs = 0; offs < ((0x300 >> 1) << 6); offs += 2)
    {
      UINT16 tileno = neogeo_vidram16[offs];
      UINT16 tileatr = neogeo_vidram16[offs + 1];

      if (tileno == 0x52a0 && tileatr == 0x0000)
	neogeo_vidram16[offs] = 0x0000;
    }
}
