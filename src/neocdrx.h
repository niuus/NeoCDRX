/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
*
* Acknowledgements
*
*	NGCD 0.8 "The Kick Ass NeoGeo CD Emulator" - Martinez Fabrice
*	NeoCD/SDL 0.3.1                            - Fosters
*	NeogeoCDZ                                  - NJ
*	Musashi M68000 C Core                      - Karl Stenerud
*	Mame Z80 C Core                            - Juergen Buchmueller
*	Sound core and miscellaneous               - Mame Team
*
****************************************************************************/

#ifndef __NEOGEOREDUX__
#define __NEOGEOREDUX__

#define APPTITLE "NeoCD-RX"
#define VERSION "0.1.55"
#define SAMPLE_RATE 48000

/*** SDL Replacement Types ***/
typedef signed char Sint8;
typedef unsigned char Uint8;
typedef signed short Sint16;
typedef unsigned short Uint16;
typedef signed int Sint32;
typedef unsigned int Uint32;

/*** Header files ***/
#include <gccore.h>
#include "m68k.h"
#include "z80intrf.h"
#include "fileio.h"
#include "memory.h"
#include "cpuintf.h"
#include "cdrom.h"
#include "cdaudio.h"
#include "patches.h"
#include "video.h"
#include "gxvideo.h"
#include "pd4990a.h"
#include "input.h"
#include "timer.h"
#include "streams.h"
#include "ay8910.h"
#include "2610intf.h"
#include "sound.h"
#include "gcaudio.h"
#include "mcard.h"
#include "gui.h"
#include "iso9660.h"
#include "dirsel.h"
#include "dvdfileio.h"
#include "sdfileio.h"
#include "wkf.h"
#include "ata.h"

/*** Functions ***/
void neogeo_swab(const void *src1, const void *src2, int isize);
int neogeo_redux(void);
void neogeo_decode_spr(unsigned char *mem, unsigned int offset,
		       unsigned int length);
void neogeo_decode_fix(unsigned char *mem, unsigned int offset,
		       unsigned int length);
void neogeo_cdda_control(void);
void neogeo_prio_switch(void);
void neogeo_exit(void);
void neogeo_exit_cdplayer(void);
void neogeo_new_game(void);
void neogeo_trace(void);
void neogeo_reset(void);

/*** Globals ***/
extern unsigned char *neogeo_rom_memory;
extern unsigned char *neogeo_prg_memory;
extern unsigned char *neogeo_fix_memory;
extern unsigned char *neogeo_ipl_memory;
extern unsigned char *neogeo_spr_memory;
extern unsigned char *neogeo_pcm_memory;
extern unsigned char neogeo_memorycard[8192];
extern unsigned short SaveDevice;
extern int use_SD;
extern int use_USB;
extern int use_IDE;
extern int use_WKF;
extern int use_DVD;

extern int have_ROM;

extern char neogeo_game_vectors[0x100];
extern char neogeo_region;
extern int patch_ssrpg;

#endif
