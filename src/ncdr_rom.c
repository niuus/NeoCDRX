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
#include "ncdr_rom.h"

/*** M68K Illegal Op Exceptions ***/
#define NEOGEO_END_UPLOAD 	0xFABF
#define CDROM_LOAD_FILES		0xFAC0
#define NEOGEO_EXIT_CDPLAYER	0xFAC4
#define NEOGEO_PROGRESS_SHOW 0xFAC5
#define NEOGEO_START_UPLOAD	0xFAC6
#define NEOGEO_IPL			0xFAC7
#define NEOGEO_IPL_END		0xFAC8
#define NEOGEO_TRACE		0xFACE

typedef struct {
    unsigned short offset;
    unsigned short patch;
} ROMPATCH;

static ROMPATCH rompatch[] = {
   /*** Patch BIOS load files with/without image to use the BIOS routines to decode
           the actual filenames needed. This makes more sense to me and safeguards against
	 unknown filetypes. ***/
    {0xDBFC, 0x4E71},
    {0xDBFE, 0x4E71},
    {0xDC24, 0x4E71},
    {0xDC26, 0x4E71},
    {0xDD42, CDROM_LOAD_FILES},
    {0xDD44, 0x4E71},
    {0xDD46, 0x4E71},

	/*** Hook for loading animations ***/
    {0xDE48, NEOGEO_PROGRESS_SHOW},
    {0xDE4A, 0x4E71},
    {0xDE4C, 0x4E71},
    {0xDE4E, 0x4E71},
    {0xDE72, 0x4E71},
    {0xDE74, 0x4E71},

	/*** Reuse FABF for end of file loading cleanups ***/
    {0xDC54, NEOGEO_END_UPLOAD},

	/*** Patch start of loading with image ***/
    {0xDB80, NEOGEO_START_UPLOAD},

	/*** Patch BIOS CDROM Check ***/
    {0xB040, 0x4E71},
    {0xB042, 0x4E71},

	/*** Patch BIOS upload command ***/
    {0x546, 0xFAC1},
    {0x548, 0x4E75},

	/*** Patch BIOS CDDA check ***/
    {0x56A, 0xFAC3},
    {0x56C, 0x4E75},

	/*** Patch CD Player Exit ***/
    {0x55E, NEOGEO_EXIT_CDPLAYER},
    {0x560, 0x4E75},

	/*** Full reset, please ***/
    {0xA87A, 0x4239},
    {0xA87C, 0x0010},
    {0xA87E, 0xFDAE},

	/*** Patch CD Player ***/
    {0x120, 0x55E},

	/*** IPL.TXT Processing patches ***/
    {0xD85A, NEOGEO_IPL},
    {0xD85C, 0x4E71},
    {0xD864, 0x4E71},
    {0xD866, 0x4E71},
    {0xD868, 0x4E71},
    {0xD86A, 0x4E71},
    {0xD700, 0x4E71},
    {0xD702, 0x4E71},
    {0xD736, 0x4E71},
    {0xD738, 0x4E71},
    {0xD766, NEOGEO_IPL_END},

    /*** Removed - BIOS needs to perform this task! ***/
    //{0xD77E, 0x4E71},

    //{0xD9DA, NEOGEO_TRACE},

	/*** Terminator ***/
    {0xFFFF, 0xFFFF},

	/*** System checks ***/
    {0xAD36, 5}
};

/****************************************************************************
* neogeo_patch_rom
****************************************************************************/
void neogeo_patch_rom(void)
{
    int i = 0;

    while (rompatch[i].offset != 0xFFFF) {
	*(unsigned short *) (neogeo_rom_memory + rompatch[i].offset) =
	    rompatch[i].patch;
	i++;
    }
}
