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

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>
#include <zlib.h>
#include <fat.h>
#include "neocdrx.h"
#include "gcaudio.h"
#include "ncdr_rom.h"


#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#endif

/*** Bios Checksums ***/
#define LEBIOS 0xdf9de490
#define BEBIOS 0x33697892

#define REGION_JAPAN  0
#define REGION_USA    1
#define REGION_EUROPE 2

#define REGION REGION_USA

#define PRG_MEM ( 2 * 1024 * 1024 ) /* 2mb */
#define SPR_MEM ( 4 * 1024 * 1024 ) /* 4mb */
#define PCM_MEM ( 1024 * 1024 )     /* 1mb */
#define ROM_MEM ( 512 * 1024 )      /* 512kb */
#define FIX_MEM ( 128 * 1024 )      /* 128kb */
#define IPL_MEM 0x7000
/*** 0.1.46 - Code reduction ***/
#define MEM_BUCKET (PRG_MEM + SPR_MEM + PCM_MEM + ROM_MEM + FIX_MEM + IPL_MEM) /* ~9mb */

/*** 2D Video Globals ***/
GXRModeObj *vmode;		/*** Graphics Mode Object ***/
u32 *xfb[2] = { NULL, NULL };	/*** Framebuffers ***/
int whichfb = 0;		/*** Frame buffer toggle ***/

//Console Version Type Helpers
#define GC_CPU_VERSION01 0x00083214
#define GC_CPU_VERSION02 0x00083410
#ifndef mfpvr
#define mfpvr()  ({unsigned int rval; asm volatile("mfpvr %0" : "=r" (rval)); rval;})
#endif
#define is_gamecube() (((mfpvr() == GC_CPU_VERSION01)||((mfpvr() == GC_CPU_VERSION02))))

typedef struct {
  u8   mid;         //Manufacturer ID
  char oid[2];      //OEM/Application ID
  char pnm[5];      //Product Name
  u8   prv;         //product version 0001 0001 = 1.1
  u32  psn;         //product serial number
  u16  mdt;         //bottom 4 bits are month, 8 bits above is year since 2000
  u8   unk;
} CIDdata __attribute__((aligned(32)));

void __SYS_ReadROM(void *buf,u32 len,u32 offset);
char IPLInfo[256] __attribute__((aligned(32)));


/*** Globals ***/
unsigned char *neogeo_rom_memory = NULL;
unsigned char *neogeo_prg_memory = NULL;
unsigned char *neogeo_fix_memory = NULL;
unsigned char *neogeo_ipl_memory = NULL;
unsigned char *neogeo_spr_memory = NULL;
unsigned char *neogeo_pcm_memory = NULL;
unsigned char *neogeo_all_memory = NULL;

unsigned char neogeo_memorycard[8192];
char neogeo_game_vectors[0x100];
int FrameTicker = 0;

/*** locals ***/
char neogeo_region = REGION;
static char config_game_name[80];

/*** Patches ***/
static int patch_rbff2 = 0;
static int patch_adkworld = 0;
static int patch_crsword2 = 0;
static int patch_aof2 = 0;
int patch_ssrpg = 0;

/*** Forward prototypes ***/
static void neogeo_do_cdda(int command, int track_number_bcd);
static void neogeo_read_gamename(void);
static void neogeo_cdda_check(void);
static void neogeo_run(void);
static void neogeo_run_bios(void);

/*** 68K core ***/
int mame_debug = 0;
int previouspc = 0;
int ophw = 0;
int cur_mrhard = 0;
static int restart = 0;

/****************************************************************************
* Frameticker
****************************************************************************/
static void framestart(u32 arg)
{
	FrameTicker++;
}

#define MEMDEBUG 0
#if MEMDEBUG

/****************************************************************************
* memdebug
*
* Used to compare memory between GC load and PC
****************************************************************************/
void memdebug()
{
	FILE *fp;

	printf("Dumping Memory ... Wait\n");
	fp = fopen("program.bin", "wb");
	fwrite(neogeo_prg_memory, 1, PRG_MEM, fp);
	fclose(fp);

	fp = fopen("rom.bin", "wb");
	fwrite(neogeo_rom_memory, 1, ROM_MEM, fp);
	fclose(fp);

	fp = fopen("sprite.bin", "wb");
	fwrite(neogeo_spr_memory, 1, SPR_MEM, fp);
	fclose(fp);

	fp = fopen("pcm.bin", "wb");
	fwrite(neogeo_pcm_memory, 1, PCM_MEM, fp);
	fclose(fp);

	fp = fopen("fix.bin", "wb");
	fwrite(neogeo_fix_memory, 1, FIX_MEM, fp);
	fclose(fp);

	fp = fopen("z80.bin", "wb");
	fwrite(subcpu_memspace, 1, 0x10000, fp);
	fclose(fp);

	fp = fopen("fix_usage.bin", "wb");
	fwrite(video_fix_usage, 1, 4096, fp);
	fclose(fp);

	printf("Done\n");
}
#endif

/****************************************************************************
* neogeo_swab
****************************************************************************/
void neogeo_swab(const void *src1, const void *src2, int isize)
{
	char *ptr1;
	char *ptr2;
	char tmp;
	int ic1;

	ptr1 = (char *) src1;
	ptr2 = (char *) src2;
	for (ic1 = 0; ic1 < isize; ic1 += 2) {
	tmp = ptr1[ic1 + 0];
	ptr2[ic1 + 0] = ptr1[ic1 + 1];
	ptr2[ic1 + 1] = tmp;
	}
}

/****************************************************************************
* neogeo_free_memory
****************************************************************************/
static void neogeo_free_memory(void)
{
	if ( neogeo_all_memory == NULL )
		free(neogeo_all_memory);
}

/****************************************************************************
* neogeo_redux - main function
****************************************************************************/
int main(void)
{
    GENFILE fp;
    unsigned int crc;

    VIDEO_Init();
    PAD_Init();

#ifdef HW_RVL
   WPAD_Init();
#else
   int retPAD = 0;
   while(retPAD <= 0) { retPAD = PAD_ScanPads(); usleep(100); }
#endif
    
   __SYS_ReadROM(IPLInfo,256,0);                           // Read IPL tag
    
   // Get the IPL TAG to set video mode then :
   // This is always set to 60hz Whether your running PAL or NTSC
   //   - Wii has no IPL tags for "PAL" so let libOGC figure out the video mode
   if(!is_gamecube()) {
     vmode = VIDEO_GetPreferredMode(NULL);                //Last mode used
   }
   else {
      // Gamecube, determine based on IPL
      // If Trigger L detected during bootup, force 480i safemode
      // for Digital Component cable for SDTV compatibility.
      if(VIDEO_HaveComponentCable() && !(PAD_ButtonsDown(0) & PAD_TRIGGER_L)) {
        if((strstr(IPLInfo,"PAL")!=NULL)) {
          vmode = &TVEurgb60Hz480Prog;                    //Progressive 60hz
        }
        else {
          vmode = &TVNtsc480Prog;                         //Progressive 480p
        }
     }
     else {
        //try to use the IPL region
        if(strstr(IPLInfo,"PAL")!=NULL) {
          vmode = &TVEurgb60Hz480IntDf;                   //Interlaced 60hz
        }
        else if(strstr(IPLInfo,"NTSC")!=NULL) {
          vmode = &TVNtsc480IntDf;                        //Interlaced 480i
        }
        else {
          vmode = VIDEO_GetPreferredMode(NULL);           //Last mode used
        }
     }  
   }

    VIDEO_Configure(vmode);

    xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

    console_init(xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight,
		 vmode->fbWidth * 2);

    VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

    VIDEO_SetNextFramebuffer(xfb[0]);

    VIDEO_SetPreRetraceCallback(framestart);

    VIDEO_SetPostRetraceCallback((VIRetraceCallback)PAD_ScanPads);
    VIDEO_SetBlack(0);

    VIDEO_Flush();

    VIDEO_WaitVSync();
    if (vmode->viTVMode & VI_NON_INTERLACE)
	VIDEO_WaitVSync();

#ifdef HW_RVL
  WPAD_Init();
  WPAD_SetIdleTimeout(60);
  WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#endif
    
    fatInitDefault();
//  load_settings_default(); // eventually add settings ???
    
	//  0.1.46 - All memory allocated in one chunk
	neogeo_all_memory = memalign(32, MEM_BUCKET);
	if ( neogeo_all_memory == NULL )
		return 0;

	//  Clear memory
	memset(neogeo_all_memory, 0, MEM_BUCKET);

	//  Create pointers
	neogeo_prg_memory = neogeo_all_memory;
	neogeo_spr_memory = neogeo_prg_memory + PRG_MEM;
	neogeo_fix_memory = neogeo_spr_memory + SPR_MEM;
	neogeo_pcm_memory = neogeo_fix_memory + FIX_MEM;
	neogeo_rom_memory = neogeo_pcm_memory + PCM_MEM;
	neogeo_ipl_memory = neogeo_rom_memory + ROM_MEM;

	//  Initialise Mame memory map etc
	initialise_memmap();

	//  Go to main menu
	// Select Device
	// Select title
	//SET DEVICE HANDLER and START DEVICE
	while (!have_ROM) load_mainmenu();

	//  Find BIOS 
	char bios_dir[25];
	char bios_dir1[25];

#ifdef HW_RVL
	if (use_DVD == 1) {
		sprintf(bios_dir, "NeoCDRX/bios/NeoCD.bin");        // search DVD root for bios - Wii
		sprintf(bios_dir1,"bios/NeoCD.bin");
	}
	else if (use_SD == 1)  {
		sprintf(bios_dir, "sd:/NeoCDRX/bios/NeoCD.bin");    // search internal SD slot root for bios - Wii
		sprintf(bios_dir1,"sd:/bios/NeoCD.bin");
	}
	else if (use_USB == 1)   {
		sprintf(bios_dir, "usb:/NeoCDRX/bios/NeoCD.bin");    // search USB root for bios - Wii
		sprintf(bios_dir1,"usb:/bios/NeoCD.bin");
	}
#else
		sprintf(bios_dir, "NeoCDRX/bios/NeoCD.bin");        // always use SD Gecko slot for bios - GC
		sprintf(bios_dir1,"bios/NeoCD.bin");
#endif

	/*** Retrieving NEOGEOCD backup memory ***/
	memset(neogeo_memorycard, 0, 8192);
	neogeo_get_memorycard();

    /*** Loading BIOS ***/
	fp = GEN_fopen(bios_dir, "rb");
    if (!fp) {
		fp = GEN_fopen(bios_dir1, "rb");
		if (!fp) {
			ActionScreen((char *) "BIOS not found!");
			neogeocd_exit();
		}
	}

	GEN_fread((char *)neogeo_rom_memory, 1, ROM_MEM, fp);
	GEN_fclose(fp);

	//  Check BIOS
	crc = crc32(0, neogeo_rom_memory, ROM_MEM);

	if (crc == LEBIOS)
		neogeo_swab(neogeo_rom_memory, neogeo_rom_memory, ROM_MEM);
	else {
			if (crc != BEBIOS) {
			ActionScreen((char *) "Invalid BIOS!");
			neogeocd_exit();
		}
    }

	//  Swap the Sprite data - required for BE bios
	neogeo_swab(neogeo_rom_memory + 0x50000, neogeo_rom_memory + 0x50000, 0x20000);

	//  Patch ROM
	neogeo_patch_rom();

	//  Initialise local video
	video_init();

	//  Start CDROM system
	cdrom_mount(basedir);

	cdda_init();

	init_sdl_audio();

	neogeo_run();

	neogeo_free_memory();

	while (1);

	return 0; // Keep gcc happy
}

/****************************************************************************
* neogeo_exit
****************************************************************************/
void neogeo_exit(void)
{
	puts("NEOGEO: Exit requested by software...");
	while (1);
}

/****************************************************************************
* neogeo_exception
****************************************************************************/
void neogeo_exception(void)
{
	puts("NEOGEO: Exception!");
	while (1);
}

/****************************************************************************
* neogeo_run
*
* This is the main NeoGeo CD Emulation loop.
****************************************************************************/
static void neogeo_run(void)
{
	/*** Let's Go !!! ***/
	StartGX();
	InitGCAudio();
	FrameTicker = 0;

	/*** (re)start emulation ***/
	for (;;) {
	neogeo_run_bios();
	restart = 0;

	/*** emulation loop ***/
	for (;;) {
		/*** Run CPUS ***/
		neogeo_runframe();

		/*** Check watchdog ***/
		if (watchdog_counter > 0) {
		if (--watchdog_counter == 0)
			neogeo_reset();
		}
		// Apply patches
		if (patch_aof2)
		m68k_write_memory_8(0x108000 + 0x280, 0);

		if (patch_rbff2)
		patch_vram_rbff2();

		if (patch_adkworld)
		patch_vram_adkworld();

		if (patch_crsword2)
		patch_vram_crsword2();

		neogeo_cdda_check();
		cdda_loop_check();

		/*** Decode MP3 ***/
			mp3_decoder(3200, (char*)mp3buffer);

		/*** Update Audio ***/
		update_audio();

		/*** Allow for 5 frames, user menus etc ***/
		if (FrameTicker > 5)
		FrameTicker = 1;

		while (FrameTicker == 0) //wait until vbl
		usleep(50);

		FrameTicker--;

		/*** Update video ***/
		video_draw_screen1();

		/*** Update input ***/
		update_input();

		if (restart)
		break;
		}
	}
}

/****************************************************************************
* neogeo_reset
****************************************************************************/
void neogeo_reset(void)
{
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_PC, 0x122);
    m68k_set_reg(M68K_REG_SR, 0x2700);
    m68k_set_reg(M68K_REG_A7, 0x10F300);
    m68k_set_reg(M68K_REG_ISP, 0x10F300);
    m68k_set_reg(M68K_REG_USP, 0x10F300);
    m68k_write_memory_8(0x10FD80, 0x82);
    m68k_write_memory_8(0x10FDAF, 0x01);
    m68k_write_memory_8(0x10FEE1, 0x0A);
    m68k_write_memory_8(0x10F675, 0x01);
    m68k_write_memory_8(0x10FEBF, 0x00);
    m68k_write_memory_32(0x10FDB6, 0);
    m68k_write_memory_32(0x10FDBA, 0);

    /* System Region */
    m68k_write_memory_8(0x10FD83, neogeo_region);
    m68k_write_memory_16(0xff011c, ~(neogeo_region << 8));

    cdda_current_track = 0;
    mz80_reset();
}

/****************************************************************************
* neogeo_cdda_check
***************************************************************************/
static void neogeo_cdda_check(void)
{
    int Offset;

    Offset = m68k_read_memory_32(0x10F6EA);
	if (Offset < 0xE00000)	// Invalid addr
	return;

	Offset -= 0xE00000;
	Offset >>= 1;

	neogeo_do_cdda(subcpu_memspace[Offset], subcpu_memspace[Offset + 1]);
}

/****************************************************************************
* neogeo_cdda_control
***************************************************************************/
void neogeo_cdda_control(void)
{
	neogeo_do_cdda((m68k_get_reg(NULL, M68K_REG_D0) >> 8) & 0xFF,
			m68k_get_reg(NULL, M68K_REG_D0) & 0xFF);
}

/****************************************************************************
* neogeo_do_cdda
***************************************************************************/
static void neogeo_do_cdda(int command, int track_number_bcd)
{
    int track_number;
    int offset;
    int loop;

    if ((command == 0) && (track_number_bcd == 0))
	return;

  /*** Confirmed with BIOS ***/
    if ((command & 2) == 0) {
	m68k_write_memory_8(0x10F64B, track_number_bcd);
	m68k_write_memory_8(0x10F6F8, track_number_bcd);
	m68k_write_memory_8(0x10F6F7, command);
    }

    m68k_write_memory_8(0x10F6F6, command);

    offset = m68k_read_memory_32(0x10F6EA);

    if (offset) {

	m68k_write_memory_8(0xFF0127, offset & 0xff);
	m68k_write_memory_8(0xFF0105, 4);

	offset -= 0xE00000;
	offset >>= 1;

	m68k_write_memory_8(0x10F678, 1);

	subcpu_memspace[offset] = 0;
	subcpu_memspace[offset + 1] = 0;
    }

    loop = !(command & 1);
    track_number = ((track_number_bcd >> 4) * 10) + (track_number_bcd & 0x0f);

    switch (command) {
		case 0:
		case 1:
		case 4:
		case 5:
			if ( ( track_number > 1 ) && ( track_number < 99 ) )
			{
			cdda_play(track_number);
			cdda_autoloop = loop;
			}
			break;

		case 2:
		case 6:
			if ( cdda_playing )
			{
			cdda_autoloop = 0;
			cdda_pause();
			}
			break;

		case 3:
		case 7:
			cdda_autoloop = loop;
			cdda_resume();
			break;
	}
}

/****************************************************************************
* neogeo_read_gamename
****************************************************************************/
static void neogeo_read_gamename(void)
{
    unsigned char *Ptr;
    int temp;
    char tname[80] = "\0";
    char clean[80];
    char *p;
    int i;

	/*** Always take english game name ***/
	Ptr = neogeo_prg_memory + m68k_read_memory_32(0x11A);
	memcpy(config_game_name, Ptr, 80);

	for (temp = 0; temp < 80; temp++) {
	if (!isprint((u8)config_game_name[temp])) {
		config_game_name[temp] = 0;
		break;
	}
	  }

	/*** Clean up leading and trailing spaces ***/
	strcpy(clean, config_game_name);

	for (i = strlen(clean) - 1; i > 0; i--) {
	if (clean[i] != 32)
		break;
	else
		clean[i] = 0;
    }

    p = (char *) clean;
    while (*p == 32)
	p++;

    strcpy(config_game_name, p);

	/*** ADK World is DARK KOMBAT - same as the Aggressor of Dark Combat! ***/
    if (strcmp(config_game_name, "DARK KOMBAT") == 0) {
	strcpy(tname, config_game_name);
	config_game_name[0] = 0;
    }

    if (strlen(config_game_name) == 0) {
		/*** Try again on japanese name ***/
	Ptr = neogeo_prg_memory + m68k_read_memory_32(0x116);
		/*** PRG 01
		neogeo_swab (Ptr, config_game_name, 80);
		***/
	memcpy(config_game_name, Ptr, 80);

	for (temp = 0; temp < 80; temp++) {
		if (!isprint((u8)config_game_name[temp])) {
		config_game_name[temp] = 0;
		break;
		}
	}

	strcpy(clean, config_game_name);

	for (i = strlen(clean) - 1; i > 0; i--) {
		if (clean[i] != 32)
		break;
		else
		clean[i] = 0;
	}

	p = (char *) clean;
	while (*p == 32)
		p++;

	strcpy(config_game_name, p);
    }

	/*** NOTE - NEED TO FIX AGGRESSORS OF DARK KOMBAT ***/
	if (strlen(config_game_name) == 0) {
	if (strlen(tname))
		strcpy(config_game_name, tname);
	}

	patch_ssrpg = 0;
	patch_adkworld = (strcmp(config_game_name, "ADK WORLD") == 0);
	patch_rbff2 = (strcmp(config_game_name, "REAL BOUT 2") == 0);
	patch_crsword2 = (strcmp(config_game_name, "CROSSED SWORDS 2") == 0);
	patch_aof2 = (strcmp(config_game_name, "ART OF FIGHTING2") == 0);

	/* Special patch for Samurai Spirits RPG */
	if (strcmp(config_game_name, "TEST PROGRAM USA") == 0) {
	strcpy(config_game_name, "SAMURAI SPIRITS RPG");

	m68k_write_memory_16(0x132008, 0x4E75);
	m68k_write_memory_16(0x13200E, 0x4E75);
	m68k_write_memory_16(0x132014, 0x4E75);
	m68k_write_memory_16(0x13201A, 0x4E75);

	m68k_write_memory_16(0x132020, 0x4EF9);
	m68k_write_memory_16(0x132022, 0x00C0);
	m68k_write_memory_16(0x132024, 0xDB60);

	m68k_write_memory_16(0x132026, 0x4EF9);
	m68k_write_memory_16(0x132028, 0x00C0);
	m68k_write_memory_16(0x13202A, 0xDB6A);

	patch_ssrpg = 1;
    }
}

/****************************************************************************
* neogeo_exit_cdplayer
*
* This function resets the "patched" bios to the original values.
* Called from M68k core
****************************************************************************/
void neogeo_exit_cdplayer(void)
{
    if (accept_input)
	m68k_set_reg(M68K_REG_PC, 0xC0AD10);
    else {
	m68k_set_reg(M68K_REG_PC, 0xC0D69C);
	m68k_set_reg(M68K_REG_A5, 0x108000);
    }
}

/****************************************************************************
* neogeo_new_game
*
* Reset the entire system ready for a new CD
****************************************************************************/
void neogeo_new_game(void)
{
	/*** Prevent scratching noises in menu ***/
	AUDIO_StopDMA();
	if (!load_mainmenu() /* !load_options() */)
	{
	AUDIO_StartDMA();
	return;
	}

	GEN_fcloseall();

	/*** Clear memory, except the BIOS ROM ***/
	memset(neogeo_prg_memory, 0, PRG_MEM);
	memset(neogeo_spr_memory, 0, SPR_MEM);
	memset(neogeo_fix_memory, 0, FIX_MEM);
	memset(neogeo_pcm_memory, 0, PCM_MEM);
	memset(subcpu_memspace, 0, 0x10000);

	/*** Set video defaults ***/
	memset(video_palette_bank0_ng, 0, 8192);
    memset(video_palette_bank1_ng, 0, 8192);
    memset(video_palette_bank0_pc, 0, 8192);
    memset(video_palette_bank1_pc, 0, 8192);
    memset(video_spr_usage, 0, 0x10000);
    memset(video_vidram, 0, 0x20000);

	/*** Set memory system defaults ***/
	memreset();
	video_clear();
	mz80_reset();
	YM2610_sh_reset();

	/*** Load the new title ***/
	cdrom_mount(basedir);
	cdda_init();

	restart = 1;
	AUDIO_StartDMA();
}

/****************************************************************************
* neogeo_run_bios
****************************************************************************/
static void neogeo_run_bios(void)
{
	static int z80_inited = 0;

	accept_input = 0;

	/*** Set region ***/
	m68k_write_memory_8(0x10FD83, neogeo_region);
	m68k_write_memory_16(0xff011c, ~(neogeo_region << 8));

	/*** Copy ROM Vectors ***/
	memcpy(neogeo_prg_memory, neogeo_rom_memory, 0x100);

#if 0
	memcpy(neogeo_fix_memory, neogeo_rom_memory + 0x7C000, 0x4000);
	neogeo_decode_fix(neogeo_fix_memory, 0, 0x4000);
#endif

	/*** Reset M68k ***/
	m68k_pulse_reset();

	/*** Reset Z80 ***/
	if ( z80_inited )
		mz80_reset();
	else
	{
	mz80_init();
	z80_inited = 1;
	}
}

/****************************************************************************
* neogeo_end_ipl
****************************************************************************/
void neogeo_ipl_end(void)
{
	memcpy(neogeo_prg_memory, neogeo_game_vectors, 0x100);

	m68k_write_memory_8(0x10FD83, neogeo_region);
	m68k_write_memory_16(0xff011c, ~(neogeo_region << 8));
	m68k_write_memory_32(0x10F6EE, m68k_read_memory_32(0x68L)); // $68 *must* be copied at 10F6EE
	m68k_write_memory_32(0x10F6F2, m68k_read_memory_32(0x64L)); // VBLANK

	neogeo_end_upload();
	neogeo_read_gamename();
	neogeo_configure_game(config_game_name);

	ipl_in_progress = fix_disable = spr_disable = 0;
	accept_input = cpu_enabled = 1;
}

/****************************************************************************
* neogeo_trace
****************************************************************************/
void neogeo_trace(void)
{
    ActionScreen((char *) "Tracepoint Reached");
}

/****************************************************************************
* neogeo_exit
****************************************************************************/
void neogeocd_exit(void)
{
  /* clean memory */
  if (mcard_written) neogeo_set_memorycard ();
  neogeo_free_memory();
  unmount_image();
  sound_shutdown();

#ifdef HW_RVL
  DI_Close();

  char * sig = (char *)0x80001804;
  if(sig[0] == 'S' && sig[1] == 'T' && sig[2] == 'U' && sig[3] == 'B' &&
     sig[4] == 'H' && sig[5] == 'A' && sig[6] == 'X' && sig[7] == 'X')
  {
    /* return to HBC */
    exit(0);
  }
  else
  {
    /* return to System Menu */
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
  }

#else
  int *psoid = (int *) 0x80001800;
  if (psoid[0] == 0x7c6000a6)
  {
    /* return to SDLOAD */
    void (*PSOReload) () = (void (*)()) 0x80001800;
    PSOReload();
  }
  else
  {
    /* hot reset */
    SYS_ResetSystem(SYS_HOTRESET,0,0);
  }
#endif
}
