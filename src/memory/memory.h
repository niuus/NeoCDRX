/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* NeoGeo CD Memory Interface
*
* Based on the standard Mame memory interface.
****************************************************************************/
#ifndef __MEMINTRF__
#define __MEMINTRF__

#define READ8_HANDLER(name) 	unsigned char  name(unsigned int offset)
#define WRITE8_HANDLER(name) 	void   name(unsigned int offset, unsigned char data)
#define READ16_HANDLER(name)	unsigned short name(unsigned int offset, unsigned short mem_mask)
#define WRITE16_HANDLER(name)	void   name(unsigned int offset, unsigned short data, unsigned short mem_mask)
#define READ32_HANDLER(name)	unsigned int name(unsigned int offset)
#define WRITE32_HANDLER(name)	void   name(unsigned int offset, unsigned int data)

#define ACCESSING_LSB16				((mem_mask & 0x00ff) == 0)
#define ACCESSING_MSB16				((mem_mask & 0xff00) == 0)
#define ACCESSING_LSB				ACCESSING_LSB16
#define ACCESSING_MSB				ACCESSING_MSB16

#define COMBINE_DATA(varptr)		(*(varptr) = (*(varptr) & mem_mask) | (data & ~mem_mask))
#define COMBINE_SWABDATA(varptr)	(*(varptr) = (*(varptr) & (mem_mask << 8)) | (data & ~(mem_mask >> 8)))

#define MEM_AMASK 0x00ffffff

#define MEM_END    -1
#define MEM_BAD     0
#define MEM_ROM     1
#define MEM_RAM     2
#define MEM_NOP     3
#define MEM_MAP     4

#define IRQ1CTRL_AUTOANIM_STOP		0x08
#define IRQ1CTRL_ENABLE				0x10
#define IRQ1CTRL_LOAD_RELATIVE		0x20
#define IRQ1CTRL_AUTOLOAD_VBLANK	0x40
#define IRQ1CTRL_AUTOLOAD_REPEAT	0x80

#define RASTER_LINES		264
#define RASTER_COUNTER_START 0x1f0	/* value assumed right after vblank */
#define RASTER_COUNTER_RELOAD 0x0f8	/* value assumed after 0x1ff */
#define RASTER_LINE_RELOAD (0x200 - RASTER_COUNTER_START)

#define UPLOAD_IMMEDIATE		0x00
#define UPLOAD_PATTERN		0x01
#define UPLOAD_MEMORY		0x02
#define UPLOAD_FILE			0x03

#define	EXMEM_OBJ		0x00
#define	EXMEM_PCMA		0x01
#define	EXMEM_Z80		0x04
#define	EXMEM_FIX			0x05
#define	EXMEM_UNKNOWN	0xff

#define upload_get_type()		m68k_read_memory_8(0x108000 + 0x7eda)
#define upload_get_bank()		m68k_read_memory_8(0x108000 + 0x7edb)
#define upload_get_dst()		m68k_read_memory_32(0x108000 + 0x7ef4)
#define upload_get_src()		m68k_read_memory_32(0x108000 + 0x7ef8)
#define upload_get_length()		m68k_read_memory_32(0x108000 + 0x7efc)

typedef struct
{
  int type;
  unsigned int start;
  unsigned int end;
  unsigned short (*func) (unsigned int offset, unsigned short mask);
} READMEM;

typedef struct
{
  int type;
  unsigned int start;
  unsigned int end;
  void (*func) (unsigned int offset, unsigned short data,
		unsigned short mask);
} WRITEMEM;

typedef struct
{
  int type;
  unsigned int start;
  unsigned int end;
  unsigned int base;
  unsigned short (*func) (unsigned int offset, unsigned short mask);
} READMAP;

typedef struct
{
  int type;
  unsigned int start;
  unsigned int end;
  unsigned int base;
  void (*func) (unsigned int offset, unsigned short data,
		unsigned short mask);
} WRITEMAP;

void initialise_memmap (void);
unsigned int m68k_read_memory_8 (unsigned int address);
unsigned int m68k_read_memory_16 (unsigned int address);
unsigned int m68k_read_memory_32 (unsigned int address);
void m68k_write_memory_8 (unsigned int address, unsigned int value);
void m68k_write_memory_16 (unsigned int address, unsigned int value);
void m68k_write_memory_32 (unsigned int address, unsigned int value);
void neogeo_sound_irq (int irq);
void neogeo_interrupt (void);
void neogeo_raster_interrupt (void);
void neogeo_raster_interrupt_busy (void);
void neogeo_decode_spr (unsigned char *mem, unsigned int offset,
			unsigned int length);
void neogeo_decode_fix (unsigned char *mem, unsigned int offset,
			unsigned int length);
void neogeo_undecode_fix (unsigned char *mem, int offset,
			  unsigned int length);
void memreset (void);

extern int watchdog_counter;
extern int scanline;
extern unsigned short *video_paletteram_ng;
extern char neogeo_game_vectors[0x100];
extern int nowait_irqack;
extern int rastercounter_adjust;
extern int mcard_written;
extern int spr_disable;
extern int fix_disable;
extern int video_enable;
extern int rldivisor;

#endif
