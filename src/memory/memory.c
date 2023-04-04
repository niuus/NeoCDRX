/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* NeoGeo CD Memory Interface
*
* This comes almost entirely from NeoGeoCDZ by NJ
* Modification is mostly integration with the NeoCD/SDL layer
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "neocdrx.h"

/*** For debugging ***/
#define TRACE 0

extern CPU CPU_Z80;

#define PRG_TYPE		0x00
#define FIX_TYPE		0x01
#define SPR_TYPE		0x02
#define Z80_TYPE		0x03
#define PCM_TYPE		0x04
#define PAT_TYPE		0x05
#define PAL_TYPE		0x06
#define OBJ_TYPE		0x07
#define AXX_TYPE		0x08
#define BACKUP_RAM		0x09
#define UNKNOWN_TYPE	0x0f

static char hwcontrol[512];
static int sample_rate = 1;
static int scanline_read = 0;
static int current_rastercounter = 0;
static int current_rasterline = 0;
static int irq1control = 0;
static unsigned int irq1pos_value = 0;
static int irq1start = 1000;
static int vblank_int = 0;
static int scanline_int = 0;
int nowait_irqack = 0;
int rldivisor = 0x180;
static unsigned int frame_counter = 0;
static unsigned int z80_cdda_offset = 0;

int watchdog_counter = 0;
int mcard_written = 0;

static unsigned char exmem[0x100];
static unsigned char exmem_latch[0x100];
static unsigned char exmem_bank[4];
static unsigned char exmem_counter;

static unsigned char upload_mode;
static unsigned char upload_type;
static unsigned int upload_offset1;
static unsigned int upload_offset2;
static unsigned int upload_length;
static unsigned short upload_pattern;

/*** Investigate further ***/
int spr_disable = 0;
int fix_disable = 0;
int video_enable = 1;

/*** Prototypes ***/
static READ16_HANDLER (neogeo_controller1_16_r);
static READ16_HANDLER (neogeo_controller2_16_r);
static READ16_HANDLER (neogeo_controller3_16_r);
static READ16_HANDLER (neogeo_z80_r);
static READ16_HANDLER (neogeo_video_16_r);
static READ16_HANDLER (neogeo_paletteram16_r);
static READ16_HANDLER (neogeo_memcard16_r);
static WRITE16_HANDLER (neogeo_memcard16_w);
static WRITE16_HANDLER (watchdog_reset_16_w);
static WRITE16_HANDLER (neogeo_z80_w);
static WRITE16_HANDLER (neogeo_syscontrol1_16_w);
static WRITE16_HANDLER (neogeo_syscontrol2_16_w);
static WRITE16_HANDLER (neogeo_video_16_w);
static WRITE16_HANDLER (neogeo_paletteram16_w);
static READ16_HANDLER (neogeo_hardcontrol_16_r);
static WRITE16_HANDLER (neogeo_hardcontrol_16_w);
static READ16_HANDLER (neogeo_externalmem_16_r);
static WRITE16_HANDLER (neogeo_externalmem_16_w);

static void update_interrupts (void);

static READMEM neogeo_readmem[] = {
  {MEM_RAM, 0x000000, 0x1fffff, NULL},
  {MEM_MAP, 0x300000, 0x31ffff, neogeo_controller1_16_r},
  {MEM_MAP, 0x320000, 0x33ffff, neogeo_z80_r},
  {MEM_MAP, 0x340000, 0x35ffff, neogeo_controller2_16_r},
  {MEM_MAP, 0x380000, 0x39ffff, neogeo_controller3_16_r},
  {MEM_MAP, 0x3c0000, 0x3dffff, neogeo_video_16_r},
  {MEM_MAP, 0x400000, 0x7fffff, neogeo_paletteram16_r},
  {MEM_MAP, 0x800000, 0x803fff, neogeo_memcard16_r},
  {MEM_ROM, 0xc00000, 0xc7ffff, NULL},
  {MEM_MAP, 0xe00000, 0xefffff, neogeo_externalmem_16_r},
  {MEM_MAP, 0xff0000, 0xffffff, neogeo_hardcontrol_16_r},
  {MEM_END,}
};

static WRITEMEM neogeo_writemem[] = {
  {MEM_RAM, 0x000000, 0x1fffff, NULL},
  {MEM_MAP, 0x300000, 0x31ffff, watchdog_reset_16_w},
  {MEM_MAP, 0x320000, 0x33ffff, neogeo_z80_w},
  {MEM_MAP, 0x380000, 0x39ffff, neogeo_syscontrol1_16_w},
  {MEM_MAP, 0x3a0000, 0x3affff, neogeo_syscontrol2_16_w},
  {MEM_MAP, 0x3c0000, 0x3dffff, neogeo_video_16_w},
  {MEM_MAP, 0x400000, 0x7fffff, neogeo_paletteram16_w},
  {MEM_MAP, 0x800000, 0x803fff, neogeo_memcard16_w},
  {MEM_MAP, 0xe00000, 0xefffff, neogeo_externalmem_16_w},
  {MEM_MAP, 0xff0000, 0xffffff, neogeo_hardcontrol_16_w},
  {MEM_END,}
};

static READMAP read_map[0x100];
static WRITEMAP write_map[0x100];

static inline u32
FLIP32 (u32 b)
{
  unsigned int c;
  c = (b & 0xff000000) >> 24;
  c |= (b & 0xff0000) >> 8;
  c |= (b & 0xff00) << 8;
  c |= (b & 0xff) << 24;
  return c;
}

static inline unsigned short
FLIP16 (unsigned short b)
{
  return ((b & 0xff00) >> 8) | ((b & 0xff) << 8);
}

#if 0
#define logaccess(...) printf(__VA_ARGS__)
#else
#define logaccess(...)
#endif

#if TRACE
/****************************************************************************
* Debug tracing only
****************************************************************************/
void
tracethis (char *buf)
{
  static FILE *fp;
  char crlf[2] = { 0x0d, 0x0a };

  if (!fp)
    {
      fp = fopen ("trace.txt", "ab");
    }

  if (fp)
    {
      fwrite (buf, 1, strlen (buf), fp);
      fwrite (crlf, 1, 2, fp);
    }
}

#endif

/****************************************************************************
* memreset
****************************************************************************/
void
memreset (void)
{
  sample_rate = 1;
  scanline_read = 0;
  current_rastercounter = 0;
  current_rasterline = 0;
  irq1control = 0;
  irq1pos_value = 0;
  irq1start = 1000;
  vblank_int = 0;
  scanline_int = 0;
  nowait_irqack = 0;
  frame_counter = 0;
  z80_cdda_offset = 0;
  rldivisor = 0x180;
  watchdog_counter = 0;
  mcard_written = 0;

  memset (hwcontrol, 0, 0x200);
  memset (exmem, 0xff, 0x100);
  memset (exmem_latch, 0xff, 0x100);
  memset (exmem_bank, 0, 4);
  exmem_counter = 0;
}

/****************************************************************************
* initialise_memmap
****************************************************************************/
void
initialise_memmap ()
{
  int i;
  unsigned int start, end;
  time_t ltime;
  struct tm *today;
  READMEM *neoread;
  WRITEMEM *neowrite;

	/*** Set all map regions to bad ***/
  for (i = 0; i < 0xff; i++)
    {
      start = (i << 16);
      end = ((i + 1) << 16) - 1;

      read_map[i].type = MEM_BAD;
      read_map[i].start = start;
      read_map[i].end = end;
      read_map[i].func = NULL;

      write_map[i].type = MEM_BAD;
      write_map[i].start = start;
      write_map[i].end = end;
      write_map[i].func = NULL;
    }

	/*** Set read mapping ***/
  neoread = neogeo_readmem;

  while (neoread->type != MEM_END)
    {
      start = (neoread->start >> 16) & 0xff;
      end = (neoread->end >> 16) & 0xff;

      for (i = start; i <= end; i++)
	{
	  read_map[i].type = neoread->type;
	  read_map[i].base = neoread->start;
	  read_map[i].func = neoread->func;
	  if (i == end)
	    read_map[i].end = neoread->end;
	}

      neoread++;
    }

	/*** Set write map ***/
  neowrite = neogeo_writemem;

  while (neowrite->type != MEM_END)
    {
      start = (neowrite->start >> 16) & 0xff;
      end = (neowrite->end >> 16) & 0xff;

      for (i = start; i <= end; i++)
	{
	  write_map[i].type = neowrite->type;
	  write_map[i].base = neowrite->start;
	  write_map[i].func = neowrite->func;
	  if (i == end)
	    write_map[i].end = neowrite->end;
	}

      neowrite++;
    }

  memreset ();

  time (&ltime);
  today = localtime (&ltime);
  pd4990a.seconds = ((today->tm_sec / 10) << 4) + (today->tm_sec % 10);
  pd4990a.minutes = ((today->tm_min / 10) << 4) + (today->tm_min % 10);
  pd4990a.hours = ((today->tm_hour / 10) << 4) + (today->tm_hour % 10);
  pd4990a.days = ((today->tm_mday / 10) << 4) + (today->tm_mday % 10);
  pd4990a.month = (today->tm_mon + 1);
  pd4990a.year = (((today->tm_year % 100) / 10) << 4) + (today->tm_year % 10);
  pd4990a.weekday = today->tm_wday;

}

/****************************************************************************
* M68000 Read Byte
****************************************************************************/
unsigned int
m68k_read_memory_8 (unsigned int address)
{
  READMAP *neoread;
  int shift;

  address &= MEM_AMASK;
  neoread = &read_map[address >> 16];

  if (address <= neoread->end)
    {
      address -= neoread->base;

      switch (neoread->type)
	{
	case MEM_ROM:
	  return neogeo_rom_memory[address];

	case MEM_RAM:
		/*** PRG 01
	  return neogeo_prg_memory[address ^ 1];
		***/
	  return neogeo_prg_memory[address];

	case MEM_MAP:
	  shift = (~address & 1) << 3;
	  return (((neoread->func) (address >> 1,
				    ~(0xff << shift)) >> shift));
	}
    }

  return 0xff;
}

/****************************************************************************
* M68000 Read Word
****************************************************************************/
unsigned int
m68k_read_memory_16 (unsigned int address)
{
  READMAP *neoread;

  address &= MEM_AMASK;
  neoread = &read_map[address >> 16];

  if (address <= neoread->end)
    {
      address -= neoread->base;

      switch (neoread->type)
	{
	case MEM_ROM:
	  return *(unsigned short *) (neogeo_rom_memory + address);

	case MEM_RAM:
	  /*** PRG 01
	  return FLIP16 (*(unsigned short *) (neogeo_prg_memory + address));
	   ***/
	  return *(unsigned short *) (neogeo_prg_memory + address);

	case MEM_MAP:
	  return ((neoread->func) (address >> 1, 0));
	}
    }

  return 0xffff;
}

/****************************************************************************
* M68000 Read Double Word
****************************************************************************/
unsigned int
m68k_read_memory_32 (unsigned int address)
{
  READMAP *neoread;
	/*** PRG 01
  unsigned int data;
	***/

  address &= MEM_AMASK;
  neoread = &read_map[address >> 16];

  if (address <= neoread->end)
    {
      address -= neoread->base;

      switch (neoread->type)
	{
	case MEM_ROM:
	  return *(unsigned int *) (neogeo_rom_memory + address);

	case MEM_RAM:
	  /*** PRG 01
	  data = FLIP16(*(unsigned short *)(neogeo_prg_memory + address)) << 16;
	  data |= (FLIP16(*(unsigned short *)(neogeo_prg_memory + address + 2)));
	  return data;
	  ***/
	  return *(unsigned int *) (neogeo_prg_memory + address);

	case MEM_MAP:
	  address >>= 1;
	  return (((neoread->func) (address,
				    0) << 16) | (neoread->func) (address + 1,
								 0));
	}
    }

  return 0xffffffff;
}

/****************************************************************************
* M68000 Write Byte
****************************************************************************/
void
m68k_write_memory_8 (unsigned int address, unsigned int value)
{
  WRITEMAP *neowrite;
  int shift;

  address &= MEM_AMASK;
  value &= 0xff;
  neowrite = &write_map[address >> 16];

  if (address <= neowrite->end)
    {
      address -= neowrite->base;

      switch (neowrite->type)
	{
	case MEM_RAM:
		/*** PRG 01
	  neogeo_prg_memory[address ^ 1] = value;
		***/
	  neogeo_prg_memory[address] = value;
	  break;

	case MEM_MAP:
	  shift = (~address & 1) << 3;
	  (neowrite->func) (address >> 1, (value << shift), ~(0xff << shift));
	  break;
	}
    }
}

/****************************************************************************
* M68000 Write Word
****************************************************************************/
void
m68k_write_memory_16 (unsigned int address, unsigned int value)
{
  WRITEMAP *neowrite;

  address &= MEM_AMASK;
  value &= 0xffff;
  neowrite = &write_map[address >> 16];

  if (address <= neowrite->end)
    {
      address -= neowrite->base;

      switch (neowrite->type)
	{
	case MEM_RAM:
		/*** PRG 01
	  *(unsigned short *) (neogeo_prg_memory + address) = FLIP16 (value);
		***/
	  *(unsigned short *) (neogeo_prg_memory + address) = value;
	  break;

	case MEM_MAP:
	  (neowrite->func) (address >> 1, value, 0);
	  break;
	}
    }
}

/****************************************************************************
* M68000 Write Double Word
****************************************************************************/
void
m68k_write_memory_32 (unsigned int address, unsigned int value)
{
  WRITEMAP *neowrite;

  address &= MEM_AMASK;
  neowrite = &write_map[address >> 16];

  if (address <= neowrite->end)
    {
      address -= neowrite->base;

      switch (neowrite->type)
	{
	case MEM_RAM:
		/*** PRG 01
	  *(unsigned short *)(neogeo_prg_memory + address) = FLIP16((value >> 16));
	  *(unsigned short *)(neogeo_prg_memory + address + 2) = FLIP16(value & 0xffff);
		***/
	  *(unsigned int *) (neogeo_prg_memory + address) = value;
	  break;

	case MEM_MAP:
	  address >>= 1;
	  (neowrite->func) (address, value >> 16, 0);
	  (neowrite->func) (address + 1, value & 0xffff, 0);
	  break;
	}
    }
}

/****************************************************************************
* Read Player 1
****************************************************************************/
static
READ16_HANDLER (neogeo_controller1_16_r)
{
  return (u16) (read_player1 () << 8);
}

/****************************************************************************
* Read Player 2
****************************************************************************/
static
READ16_HANDLER (neogeo_controller2_16_r)
{
  return (u16) (read_player2 () << 8);
}

/****************************************************************************
* Read Player Start / Select
****************************************************************************/
static
READ16_HANDLER (neogeo_controller3_16_r)
{
  return (u16) ((read_pl12_startsel () << 8) & 0x0f00);
}

/****************************************************************************
* Read coin, Z80 etc
****************************************************************************/
static
READ16_HANDLER (neogeo_z80_r)
{
  unsigned short res;
  int coinflip = pd4990a_testbit_r (0);
  int databit = pd4990a_databit_r (0);

  res = 0 ^ (coinflip << 6) ^ (databit << 7);

  if (sample_rate)
    {
      res |= result_code << 8;
      if (pending_command)
	res &= 0x7fff;
    }
  else
    res = 0x0100;

  return res;
}

/****************************************************************************
* Read video RAM
****************************************************************************/
static
READ16_HANDLER (neogeo_vidram16_data_r)
{
  unsigned short *v = (unsigned short *) video_vidram;
  return v[video_pointer];
}

/****************************************************************************
* Read video modulo
****************************************************************************/
static
READ16_HANDLER (neogeo_vidram16_modulo_r)
{
  return video_modulo;
}

static
READ16_HANDLER (neogeo_control_16_r)
{
  int res;

  scanline_read = 1;		/* needed for raster_busy optimization */

  res = ((current_rastercounter << 7) & 0xff80) |	/* raster counter */
    (neogeo_frame_counter & 0x0007);	/* frame counter */

  return res;
}

/****************************************************************************
* Read video memory
****************************************************************************/
static
READ16_HANDLER (neogeo_video_16_r)
{
  unsigned short retdata = 0xffff;

  if (!ACCESSING_MSB)
    {
      return 0xff;
    }

  offset &= 0x3;

  switch (offset << 1)
    {
    case 0:
    case 2:
      retdata = neogeo_vidram16_data_r (0, mem_mask);
      break;
    case 4:
      retdata = neogeo_vidram16_modulo_r (0, mem_mask);
      break;
    case 6:
      retdata = neogeo_control_16_r (0, mem_mask);
      break;
    }

  return retdata;
}

/****************************************************************************
* Read video palette RAM
****************************************************************************/
static
READ16_HANDLER (neogeo_paletteram16_r)
{
  unsigned short *v = (unsigned short *) video_paletteram_ng;
  offset &= 0xfff;
  return v[offset];
}

/****************************************************************************
* Memory card handlers
****************************************************************************/
static
READ16_HANDLER (neogeo_memcard16_r)
{
  return neogeo_memorycard[offset] | 0xff00;
}

static
WRITE16_HANDLER (neogeo_memcard16_w)
{
  if (ACCESSING_LSB)
    {
      neogeo_memorycard[offset] = data & 0xff;
      mcard_written = 1;
    }
}

/****************************************************************************
* Read video palette RAM
****************************************************************************/
static
WRITE16_HANDLER (watchdog_reset_16_w)
{
  watchdog_counter = 3 * 60;	/* 3s * 60 fps */
}

/****************************************************************************
* Z80 NMI
****************************************************************************/
static
WRITE16_HANDLER (neogeo_z80_w)
{
  pending_command = 1;
  sound_code = (data >> 8) & 0xff;
  mz80nmi ();
}

/****************************************************************************
* PD4990A 
****************************************************************************/
static
WRITE16_HANDLER (neogeo_syscontrol1_16_w)
{
  offset &= 0x7f;

  switch (offset << 1)
    {
    case 0x00:
      break;
    case 0x10:
      break;
    case 0x50:
      pd4990a_control_16_w (0, data, mem_mask);
      break;
    case 0xd0:
      pd4990a_control_16_w (0, data, mem_mask);
      break;
    default:
      break;
    }
}

/****************************************************************************
* Select BIOS vectors
****************************************************************************/
static void
neogeo_select_bios_vectors (void)
{
  memcpy (neogeo_prg_memory, neogeo_rom_memory, 0x100);
	/*** PRG 01
  neogeo_swab(neogeo_prg_memory, neogeo_prg_memory, 0x100);
	***/
}

/****************************************************************************
* Select Game vectors
****************************************************************************/
static void
neogeo_select_game_vectors (void)
{
  memcpy (neogeo_prg_memory, neogeo_game_vectors, 0x100);
}

/****************************************************************************
* Select Pallete Bank 0
****************************************************************************/
static void
neogeo_setpalbank0 (void)
{
  video_paletteram_ng = video_palette_bank0_ng;
  video_paletteram_pc = video_palette_bank0_pc;
}

/****************************************************************************
* Select Pallete Bank 1
****************************************************************************/
static void
neogeo_setpalbank1 (void)
{
  video_paletteram_ng = video_palette_bank1_ng;
  video_paletteram_pc = video_palette_bank1_pc;
}

/****************************************************************************
* Vectors and palette banks
****************************************************************************/
static
WRITE16_HANDLER (neogeo_syscontrol2_16_w)
{
  offset &= 0xf;

  switch (offset << 1)
    {
    case 0x00:
      break;			// unknown
    case 0x02:
      neogeo_select_bios_vectors ();
      break;
    case 0x04:
      break;			// unknown
    case 0x06:
      break;			// unknown
    case 0x08:
      break;			// unknown
    case 0x0a:
      break;			// unknown
    case 0x0c:
      break;			// unknown
    case 0x0e:
      neogeo_setpalbank0 ();
      break;
    case 0x10:
      break;			// unknown
    case 0x12:
      neogeo_select_game_vectors ();
      break;
    case 0x14:
      break;			// unknown
    case 0x16:
      break;			// unknown
    case 0x18:
      break;			// unknown
    case 0x1a:
      break;			// unknown
    case 0x1c:
      break;			// unknown
    case 0x1e:
      neogeo_setpalbank1 ();
      break;
    }
}

/****************************************************************************
* Update palette
****************************************************************************/
static
WRITE16_HANDLER (neogeo_paletteram16_w)
{
  unsigned short newword;

  offset &= 0xfff;
  newword = video_paletteram_ng[offset];
  COMBINE_DATA (&newword);
  video_paletteram_ng[offset] = newword;
  video_paletteram_pc[offset] = video_color_lut[newword & 0x7fff];

}

/****************************************************************************
* Neogeo Video Pointer
****************************************************************************/
static
WRITE16_HANDLER (neogeo_vidram16_offset_w)
{
  COMBINE_DATA (&video_pointer);
}

/****************************************************************************
* Neogeo Video Modulo
****************************************************************************/
static
WRITE16_HANDLER (neogeo_vidram16_modulo_w)
{
  COMBINE_DATA (&video_modulo);
}

/****************************************************************************
* Neogeo Video Control
****************************************************************************/
static
WRITE16_HANDLER (neogeo_control_16_w)
{
  /* Auto-Anim Speed Control */
  neogeo_frame_counter_speed = (data >> 8) & 0xff;
  irq1control = data & 0xff;
}

/****************************************************************************
* Write video ram
****************************************************************************/
static
WRITE16_HANDLER (neogeo_vidram16_data_w)
{
  unsigned short *v = (unsigned short *) video_vidram;
  COMBINE_DATA (&v[video_pointer]);

  video_pointer = (video_pointer & 0x8000)	/* gururin fix */
    | ((video_pointer + video_modulo) & 0x7fff);
}

/****************************************************************************
* Hardware control read
****************************************************************************/
static
READ16_HANDLER (neogeo_hardcontrol_16_r)
{
  unsigned short *mem = (unsigned short *) hwcontrol;
  offset &= 0xff;
  return mem[offset];
}

#define decode_spr(n, s)						\
{												\
	*dst |= (*(s) & 0x01) << ((n +  0) - 0);	\
	*dst |= (*(s) & 0x02) << ((n +  4) - 1);	\
	*dst |= (*(s) & 0x04) << ((n +  8) - 2);	\
	*dst |= (*(s) & 0x08) << ((n + 12) - 3);	\
	*dst |= (*(s) & 0x10) << ((n + 16) - 4);	\
	*dst |= (*(s) & 0x20) << ((n + 20) - 5);	\
	*dst |= (*(s) & 0x40) << ((n + 24) - 6);	\
	*dst |= (*(s) & 0x80) << ((n + 28) - 7);	\
	(s)++;										\
}

void
neogeo_decode_spr (unsigned char *mem, unsigned int offset,
		   unsigned int length)
{
  unsigned char buf[128], *usage;
  unsigned int *dst;
  int i, j, k;

  dst = (unsigned int *) (mem + offset);

  usage = video_spr_usage + (offset >> 7);

  for (i = 0; i < length; i += 128)
    {
      int opaque = 0;
      unsigned char *src, *src2;

      memcpy (buf, dst, 128);

      src = buf;
      src2 = buf + 64;

      for (j = 0; j < 16; j++)
	{
	  *dst = 0;
	  decode_spr (1, src2);
	  decode_spr (0, src2);
	  decode_spr (3, src2);
	  decode_spr (2, src2);
	  for (k = 0; k < 32; k += 4)
	    opaque += (*dst & (0x0f << k)) != 0;
	  dst++;

	  *dst = 0;
	  decode_spr (1, src);
	  decode_spr (0, src);
	  decode_spr (3, src);
	  decode_spr (2, src);
	  for (k = 0; k < 32; k += 4)
	    opaque += (*dst & (0x0f << k)) != 0;
	  dst++;
	}

      if (opaque)
	*usage = (opaque == 256) ? 1 : 2;
      else
	*usage = 0;
      usage++;
    }
}

#define undecode_fix(n)				\
{									\
	tile = *(mem2 + (ofs++));		\
	buf[n] = tile;					\
}

void
neogeo_undecode_fix (unsigned char *mem, int offset, unsigned int length)
{
  int i, j, ofs;
  unsigned char tile;
  unsigned char buf[32];
  unsigned char *mem2 = mem + offset;

  for (i = 0; i < length; i += 32)
    {
      ofs = 0;

      for (j = 0; j < 8; j++)
	{
	  undecode_fix (j + 8);
	  undecode_fix (j + 0);
	  undecode_fix (j + 24);
	  undecode_fix (j + 16);
	}

      memcpy (mem2, buf, 32);

      mem2 += 32;
    }
}

#define decode_fix(n)				\
{									\
	tile = buf[n];					\
	*mem++ = tile;					\
	opaque += (tile & 0x0f) != 0;	\
	opaque += (tile >> 4) != 0;		\
}

void
neogeo_decode_fix (unsigned char *mem, unsigned int offset,
		   unsigned int length)
{
  int i, j;
  unsigned char tile, opaque;
  unsigned char buf[32], *usage;

  mem += offset;
  usage = video_fix_usage + (offset >> 5);

  for (i = 0; i < length; i += 32)
    {
      opaque = 0;

      memcpy (buf, mem, 32);

      for (j = 0; j < 8; j++)
	{
	  decode_fix (j + 8);
	  decode_fix (j + 0);
	  decode_fix (j + 24);
	  decode_fix (j + 16);
	}

      if (opaque)
	*usage = (opaque == 64) ? 1 : 2;
      else
	*usage = 0;
      *usage++;
    }
}

static
WRITE16_HANDLER (upload_offset1_16_w)
{
  if (offset)
    upload_offset1 = (upload_offset1 & 0xffff0000) | (unsigned int) data;
  else
    upload_offset1 =
      (upload_offset1 & 0x0000ffff) | ((unsigned int) data << 16);
}

static
WRITE16_HANDLER (upload_offset2_16_w)
{
  if (offset)
    upload_offset2 = (upload_offset2 & 0xffff0000) | (unsigned int) data;
  else
    upload_offset2 =
      (upload_offset2 & 0x0000ffff) | ((unsigned int) data << 16);

  upload_mode = UPLOAD_MEMORY;
}

static
WRITE16_HANDLER (upload_pattern_16_w)
{
  upload_pattern = data;
  upload_mode = UPLOAD_PATTERN;
}

static
WRITE16_HANDLER (upload_length_16_w)
{
  if (offset)
    upload_length = (upload_length & 0xffff0000) | (unsigned int) data;
  else
    upload_length =
      (upload_length & 0x0000ffff) | ((unsigned int) data << 16);
}

/****************************************************************************
* write upload
*
* Notes for PPC
* Both ROM and PRG are stored in BIG endian format.
* Transfers from PRG no longer need to 'swabbed'
****************************************************************************/
static
WRITE16_HANDLER (hardware_upload_16_w)
{
  if (data == 0x00)
    {
      upload_mode = UPLOAD_IMMEDIATE;
      upload_offset1 = 0;
      upload_offset2 = 0;
      upload_length = 0;
    }
  else if (data == 0x40)
    {
      int i;
      unsigned char *src, *dst;

      if (upload_mode == UPLOAD_MEMORY)
	{
	  upload_type = upload_get_type () & 0x0f;
	}
      else
	{
	  if (upload_offset1 >= 0x000000 && upload_offset1 < 0x200000)
	    {
	      upload_type = PRG_TYPE;
	    }
	  else if (upload_offset1 >= 0x400000 && upload_offset1 < 0x500000)
	    {
	      upload_type = PAL_TYPE;
	    }
	  else if (upload_offset1 >= 0x800000 && upload_offset1 < 0x804000)
	    {
	      upload_type = BACKUP_RAM;
	    }
	  else			// 0xe00000
	    {
	      switch (exmem[exmem_counter])
		{
		case EXMEM_OBJ:
		  upload_type = SPR_TYPE;
		  break;
		case EXMEM_PCMA:
		  upload_type = PCM_TYPE;
		  break;
		case EXMEM_Z80:
		  upload_type = Z80_TYPE;
		  break;
		case EXMEM_FIX:
		  upload_type = FIX_TYPE;
		  break;
		}
	    }
	}

      switch (upload_mode)
	{
	case UPLOAD_PATTERN:
	  {
	    switch (upload_type)
	      {
	      case FIX_TYPE:
	      case Z80_TYPE:
	      case PCM_TYPE:
	      case BACKUP_RAM:
		for (i = 0; i < upload_length; i++)
		  {
		    offset = upload_offset1 + (i * 2);
		    m68k_write_memory_16 (offset,
					  (upload_pattern & 0xff) | 0xff00);
		  }
		break;

	      case PRG_TYPE:
	      case PAL_TYPE:
	      case SPR_TYPE:
		for (i = 0; i < upload_length; i++)
		  {
		    offset = upload_offset1 + (i * 2);
		    m68k_write_memory_16 (offset, upload_pattern);
		  }
		break;
	      }
	  }
	  break;

	case UPLOAD_MEMORY:
	  {
	    unsigned int length;

	    length = upload_length << 1;
	    src = neogeo_prg_memory + upload_offset1;

	    switch (upload_type & 0x0f)
	      {
	      case PRG_TYPE:
		dst = neogeo_prg_memory;
		memcpy (dst + upload_offset2, src, length);
		break;

	      case FIX_TYPE:
		dst = neogeo_fix_memory;
		offset = upload_offset2 - 0xe00000;
			/*** PRG 02
		neogeo_swab (src, dst + (offset >> 1), length);
	                     ***/
		memcpy (dst + (offset >> 1), src, length);
		neogeo_decode_fix (dst, offset >> 1, length);
		break;

	      case SPR_TYPE:
		dst = neogeo_spr_memory;
		offset =
		  (upload_offset2 - 0xe00000) + (upload_get_bank () << 20);
			/*** PRG 02
		neogeo_swab (src, dst + offset, length);
			***/
		memcpy (dst + offset, src, length);
		neogeo_decode_spr (dst, offset, length);
		exmem_bank[EXMEM_OBJ] =
		  ((offset + upload_get_length ()) >> 20) & 0x03;
		break;

	      case Z80_TYPE:
		dst = subcpu_memspace;
		offset = upload_offset2 - 0xe00000;
			/*** PRG 02
		neogeo_swab (src, dst + (offset >> 1), length);
			***/
		memcpy (dst + (offset >> 1), src, length);
		break;

	      case PAL_TYPE:
		for (i = 0; i < length; i++)
		  {
		    data = m68k_read_memory_16 (upload_offset1 + i * 2);
		    m68k_write_memory_16 (upload_offset2 + i * 2, data);
		  }
		break;
	      }
	  }
	  break;

	case UPLOAD_FILE:
			/*** cdrom_load_files ??? ***/
	  break;
	}
    }
}

static
WRITE16_HANDLER (exmem_type_16_w)
{
  exmem[exmem_counter] = data;

  if (exmem[exmem_counter] == EXMEM_Z80)
    {
      z80_cdda_offset = m68k_read_memory_32 (0x108000 + 0x76ea);
      if (z80_cdda_offset)
	z80_cdda_offset = (z80_cdda_offset - 0xe00000) >> 1;
    }

  if (exmem[exmem_counter] == EXMEM_OBJ)
    exmem_bank[EXMEM_OBJ] = 0;
  if (exmem[exmem_counter] == EXMEM_PCMA)
    exmem_bank[EXMEM_PCMA] = 0;
}

static
WRITE16_HANDLER (spr_disable_16_w)
{
  spr_disable = data & 0xff;
}

static
WRITE16_HANDLER (fix_disable_16_w)
{
  fix_disable = data & 0xff;
}

static
WRITE16_HANDLER (video_enable_16_w)
{
  video_enable = data & 0xff;
}

static
WRITE16_HANDLER (exmem_latch_16_w)
{
  exmem_counter++;
  exmem_counter &= 0xff;
  exmem_latch[exmem_counter] = offset & 0x0f;
}

static
WRITE16_HANDLER (exmem_latch_clear_16_w)
{
  offset &= 0x0f;

  if (exmem_latch[exmem_counter] == offset)
    {
      exmem_latch[exmem_counter] = EXMEM_UNKNOWN;
      exmem_counter--;
      exmem_counter &= 0xff;
    }
}

static
WRITE16_HANDLER (z80_enable_16_w)
{
  if (data & 0xff)
    cpu_enabled = 1;
  else
    {
      mz80_reset ();
      cpu_enabled = 0;
    }
}

static
WRITE16_HANDLER (exmem_bank_16_w)
{
  offset &= 0x0f;
  exmem_bank[offset] = data & 0x03;
}

static
READ16_HANDLER (neogeo_externalmem_16_r)
{
  unsigned char *src;
  unsigned short retdata = 0xffff;
  //char tbuf[32];
	
  switch (exmem[exmem_counter])
    {
    case EXMEM_OBJ:
      src = neogeo_spr_memory;
      offset = (offset << 1) + (exmem_bank[EXMEM_OBJ] << 20);
      retdata = (src[offset] << 8) | src[offset + 1];
      break;

    case EXMEM_PCMA:
      src = neogeo_pcm_memory;
      offset = offset + (exmem_bank[EXMEM_PCMA] << 19);
      retdata = src[offset] | 0xff00;
      break;

    case EXMEM_Z80:
      if (z80_cdda_offset)
	{
	  if (offset == z80_cdda_offset || offset == z80_cdda_offset + 1)
	    {
	      return 0xff;			/** Kills Metal Slug and Ghost Pilots!!! **/
	    }
	}

      src = subcpu_memspace;
      retdata = src[offset] | 0xff00;
      break;

    case EXMEM_FIX:
      src = neogeo_fix_memory;
      //memcpy(tbuf, src + (offset & ~0x1f), 32);
      //neogeo_undecode_fix(tbuf, 0, 32);
      retdata = src[offset] | 0xff00;
      //retdata = tbuf[offset & 0x1f] | 0xff00;
      break;
    }

  return retdata;
}

WRITE16_HANDLER (neogeo_externalmem_16_w)
{
  unsigned char *dst;

  switch (exmem[exmem_counter])
    {
    case EXMEM_OBJ:
      offset = (offset << 1) + (exmem_bank[EXMEM_OBJ] << 20);
      data = (data << 8) | (data >> 8);
      dst = neogeo_spr_memory;
      COMBINE_SWABDATA ((unsigned short *) (dst + offset));
      if ((offset & 0x7f) == 0x7e)
	neogeo_decode_spr (dst, (offset & ~0x7f), 128);
      return;

    case EXMEM_PCMA:
      dst = neogeo_pcm_memory;
      offset += exmem_bank[EXMEM_PCMA] << 19;
      dst[offset] = data & 0xff;
      return;

    case EXMEM_Z80:
      if (z80_cdda_offset)
	{
	  if (offset == z80_cdda_offset || offset == z80_cdda_offset + 1)
	    {
	      return;
	    }
	}
      dst = subcpu_memspace;
      dst[offset] = data & 0xff;
      return;

    case EXMEM_FIX:
      dst = neogeo_fix_memory;
      dst[offset] = data & 0xff;
      if ((offset & 0x1f) == 0x1f)
	neogeo_decode_fix (dst, (offset & ~0x1f), 32);
      return;
    }
}

/****************************************************************************
* Hardware control write
****************************************************************************/
static
WRITE16_HANDLER (neogeo_hardcontrol_16_w)
{
  unsigned short *mem = (unsigned short *) hwcontrol;
  offset &= 0xff;

  switch (offset << 1)
    {
    case 0x00:
      break;			// unknown
    case 0x02:
      break;			// unknown
    case 0x04:
      break;			// unknown
    case 0x06:
      break;			// unknown

    case 0x60:
      hardware_upload_16_w (0, data, mem_mask);
      break;
    case 0x64:
      upload_offset1_16_w (0, data, mem_mask);
      break;
    case 0x66:
      upload_offset1_16_w (1, data, mem_mask);
      break;
    case 0x68:
      upload_offset2_16_w (0, data, mem_mask);
      break;
    case 0x6a:
      upload_offset2_16_w (1, data, mem_mask);
      break;
    case 0x6c:
      upload_pattern_16_w (0, data, mem_mask);
      break;
    case 0x70:
      upload_length_16_w (0, data, mem_mask);
      break;
    case 0x72:
      upload_length_16_w (1, data, mem_mask);
      break;

    case 0x7e:
      break;			// unknown
    case 0x80:
      break;			// unknown
    case 0x82:
      break;			// unknown
    case 0x84:
      break;			// unknown
    case 0x86:
      break;			// unknown
    case 0x88:
      break;			// unknown
    case 0x8a:
      break;			// unknown
    case 0x8c:
      break;			// unknown
    case 0x8e:
      break;			// unknown


    case 0x104:
      exmem_type_16_w (0, data, mem_mask);
      break;
    case 0x110:
      spr_disable_16_w (0, data, mem_mask);
      break;
    case 0x114:
      fix_disable_16_w (0, data, mem_mask);
      break;
    case 0x118:
      video_enable_16_w (0, data, mem_mask);
      break;
    case 0x11c:
      break;			// machine settings (read only)

    case 0x120:		// OBJ RAM latch
    case 0x122:		// PCM-A RAM latch
    case 0x126:		// Z80 RAM latch
    case 0x128:		// FIX RAM latch
      exmem_latch_16_w (offset, data, mem_mask);
      break;

    case 0x140:		// OBJ RAM latch clear
    case 0x142:		// PCM-A RAM latch clear
    case 0x146:		// Z80 RAM latch clear
    case 0x148:		// FIX RAM latch clear
      exmem_latch_clear_16_w (offset, data, mem_mask);
      break;

    case 0x180:
      break;			// unknown
    case 0x182:
      z80_enable_16_w (0, data, mem_mask);
      break;

    case 0x1a0:		// OBJ RAM bank
    case 0x1a2:		// PCM-A RAM bank
    case 0x1a6:		// unknown (PAT bank?)
      exmem_bank_16_w (offset, data, mem_mask);
      break;

    default:
      //printf("Write upload %08x %02x\n", offset << 1, data);
      break;
    }

  COMBINE_DATA (&mem[offset]);
}

/****************************************************************************
* IRQ 1 Position
****************************************************************************/
static
WRITE16_HANDLER (neogeo_irq1pos_16_w)
{
  if (offset)
    irq1pos_value = (irq1pos_value & 0xffff0000) | (unsigned int) data;
  else
    irq1pos_value =
      (irq1pos_value & 0x0000ffff) | ((unsigned int) data << 16);

  if (irq1control & IRQ1CTRL_LOAD_RELATIVE)
    {
      int line = (irq1pos_value + 0x3b) / rldivisor;

      irq1start = current_rasterline + line;
    }
}

/****************************************************************************
* IRQ Acknowledge
****************************************************************************/
static
WRITE16_HANDLER (neogeo_irqack_16_w)
{
  if (ACCESSING_LSB)
    {
      if (data & 4)
	vblank_int = 0;
      if (data & 2)
	scanline_int = 0;
      update_interrupts ();
    }
}

/****************************************************************************
* Miscellaneous Video
****************************************************************************/
static
WRITE16_HANDLER (neogeo_video_16_w)
{
  offset &= 0x7;

  switch (offset << 1)
    {
    case 0x0:
      neogeo_vidram16_offset_w (0, data, mem_mask);
      break;
    case 0x2:
      neogeo_vidram16_data_w (0, data, mem_mask);
      break;
    case 0x4:
      neogeo_vidram16_modulo_w (0, data, mem_mask);
      break;
    case 0x6:
      neogeo_control_16_w (0, data, mem_mask);
      break;
    case 0x8:
      neogeo_irq1pos_16_w (0, data, mem_mask);
      break;
    case 0xa:
      neogeo_irq1pos_16_w (1, data, mem_mask);
      break;
    case 0xc:
      neogeo_irqack_16_w (0, data, mem_mask);
      break;
    case 0xe:
      break;			/* Unknown, see control_r */
    }
}

/****************************************************************************
* Ma,me Z80 sound irq
****************************************************************************/
void
neogeo_sound_irq (int irq)
{
  if (irq)
    {
      if (CPU_Z80.irq_state != ASSERT_LINE)
	mz80int (irq);
    }
  else
    {
      mz80ClearPendingInterrupt (0);
    }
}

/****************************************************************************
* Video Interrupt Handlers
****************************************************************************/
static void
update_interrupts (void)
{
  int level = 0;

  /* determine which interrupt is active */
  if (vblank_int)
    level = 2;
  if (scanline_int)
    level = 1;

  /* either set or clear the appropriate lines */
  if (level)
    {
      m68k_set_irq (level);

      if (nowait_irqack)
	{
	  if (level == 2)
	    vblank_int = 0;
	  if (level == 1)
	    scanline_int = 0;
	}
    }
  else
    m68k_set_irq (0);
}

/****************************************************************************
* Video Interrupt Handlers
****************************************************************************/
void
neogeo_interrupt (void)
{
  int line = RASTER_LINES - scanline;

  current_rasterline = line;

  int l = line;

  if (l == RASTER_LINES)
    l = 0;			/* vblank */
  if (l < RASTER_LINE_RELOAD)
    current_rastercounter = RASTER_COUNTER_START + l;
  else
    current_rastercounter = RASTER_COUNTER_RELOAD + l - RASTER_LINE_RELOAD;

  if (line == RASTER_LINES)	/* vblank */
    {
      current_rasterline = 0;

      /* Add a timer tick to the pd4990a */
      pd4990a_addretrace ();

      if (!(irq1control & IRQ1CTRL_AUTOANIM_STOP))
	{
	  /* Animation counter */
	  if (frame_counter++ > neogeo_frame_counter_speed)	/* fixed animation speed */
	    {
	      frame_counter = 0;
	      neogeo_frame_counter++;
	    }
	}

      /* return a standard vblank interrupt */
      vblank_int = 1;		/* vertical blank */
    }

  update_interrupts ();
}

/****************************************************************************
* Raster Interrupt
****************************************************************************/
static void
raster_interrupt (int busy)
{
  int line = RASTER_LINES - scanline;

  current_rasterline = line;

  {
    int l = line;

    if (l == RASTER_LINES)
      l = 0;			/* vblank */
    if (l < RASTER_LINE_RELOAD)
      current_rastercounter = RASTER_COUNTER_START + l;
    else
      current_rastercounter = RASTER_COUNTER_RELOAD + l - RASTER_LINE_RELOAD;
  }

  if (busy)
    {
      if (scanline_read)
	{
	  scanline_read = 0;
	}
    }

  if (irq1control & IRQ1CTRL_ENABLE)
    {
      if (line == irq1start)
	{
	  if (irq1control & IRQ1CTRL_AUTOLOAD_REPEAT)
	    irq1start += (irq1pos_value + 3) / rldivisor;	/* ridhero gives 0x17d */

	  scanline_int = 1;
	}
    }

  if (line == RASTER_LINES)	/* vblank */
    {
      current_rasterline = 0;

      if (irq1control & IRQ1CTRL_AUTOLOAD_VBLANK)
	irq1start = (irq1pos_value + 3) / rldivisor;	/* ridhero gives 0x17d */
      else
	irq1start = 1000;

      /* Add a timer tick to the pd4990a */
      pd4990a_addretrace ();

      /* Animation counter */
      if (!(irq1control & IRQ1CTRL_AUTOANIM_STOP))
	{
	  if (frame_counter++ > neogeo_frame_counter_speed)	/* fixed animation speed */
	    {
	      frame_counter = 0;
	      neogeo_frame_counter++;
	    }
	}

      vblank_int = 1;
    }

  update_interrupts ();
}

void
neogeo_raster_interrupt (void)
{
  raster_interrupt (0);
}

void
neogeo_raster_interrupt_busy (void)
{
  raster_interrupt (1);
}
