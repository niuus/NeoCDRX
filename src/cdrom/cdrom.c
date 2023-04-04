/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* CDROM Data interface
*
* Simulate all CDROM reading using standard files.
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "neocdrx.h"

#define PSEP '\\'

/*** Look for UPPER case files only ***/
#define IPL_TXT  "IPL.TXT"
#define PRG      "PRG"
#define FIX      "FIX"
#define SPR      "SPR"
#define Z80      "Z80"
#define PAT      "PAT"
#define PCM      "PCM"
#define JUE      "JUE"

/*** Definitions ***/
#define BUFFER_SIZE    131072
#define PRG_TYPE    0
#define FIX_TYPE    1
#define SPR_TYPE    2
#define Z80_TYPE    3
#define PAT_TYPE    4
#define PCM_TYPE    5
#define TITLE_X_SYS "TITLE_X.SYS"
#define LOGO_X_PRG "LOGO_X.PRG"

#define    min(a, b) ((a) < (b)) ? (a) : (b)

/*** Globals ***/
char cdpath[1024];
int img_display = 0;

/*** Locals ***/
static unsigned char cdrom_buffer[BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

/*** Prototypes ***/
int recon_filetype(char *);
static void cdrom_apply_patch(short *source, int offset, int bank);
static int sectorstodo = 0;

/*** Debug ***/
static int totalbytes;
static char cddebug[128];
int ipl_in_progress = 0;

/****************************************************************************
* getfilelen
*
* Safeguard against over-reading and exploding memory
****************************************************************************/
static int getfilelen(GENFILE fp)
{
  int len;

  GEN_fseek(fp, 0, SEEK_END);
  len = GEN_ftell(fp);
  GEN_fseek(fp, 0, SEEK_SET);

  return len;
}

/****************************************************************************
* recon_filetype
****************************************************************************/
int recon_filetype(char *ext)
{
  /*** Art of Fighting fixes ***/
  if (stricmp(ext, "OBJ") == 0)
    strcpy(ext, "SPR");

  if (stricmp(ext, "AAT") == 0)
    strcpy(ext, "PAT");

  if (stricmp(ext, "ACM") == 0)
    strcpy(ext, "PCM");

  /*** Neo Turf Master ***/
  if (stricmp(ext, "A80") == 0)
    return PRG_TYPE;

  if (stricmp(ext, PRG) == 0)
    return PRG_TYPE;

  if (stricmp(ext, FIX) == 0)
    return FIX_TYPE;

  if (stricmp(ext, SPR) == 0)
    return SPR_TYPE;

  if (stricmp(ext, Z80) == 0)
    return Z80_TYPE;

  if (stricmp(ext, PAT) == 0)
    return PAT_TYPE;

  if (stricmp(ext, PCM) == 0)
    return PCM_TYPE;

  return -1;
}

/****************************************************************************
* cdrom_inc_progess
****************************************************************************/
void cdrom_inc_progress(int done)
{
  unsigned int sectors;

  sectors = (done >> 11);
  if (done & 0x7ff)
    sectors++;

  sectorstodo = sectors >> 2;

}

/****************************************************************************
* cdrom_mount
*
* Use this function to point all subsequent reads to a directory
****************************************************************************/
int cdrom_mount(char *mount)
{
  char Path[256];
  char tmp[1024];
  GENFILE fp;

  strcpy(tmp, mount);

  if (tmp[strlen(tmp) - 1] != '/')
    strcat(tmp, "/");

  strcpy(cdpath, tmp);
  strcpy(Path, tmp);
  strcat(Path, IPL_TXT);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    return 0;

  GEN_fclose(fp);

  return 1;
}

/****************************************************************************
* cdrom_load_prg_file
****************************************************************************/
static int cdrom_load_prg_file(char *FileName, unsigned int Offset)
{
  GENFILE fp;
  char Path[256];
  unsigned char *Ptr;
  int Readed;
  int flen;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  /*** Safeguard against over-reads ***/
  flen = getfilelen(fp);

  //sprintf(cddebug,"%s : %d", FileName, flen);
  //ActionScreen(cddebug);

  if ((Offset + flen) > 0x200000)
    {
      sprintf(cddebug, "PRG : %08x %d", Offset, flen);
      ActionScreen(cddebug);
      return 1;
    }

  Ptr = neogeo_prg_memory + Offset;
  totalbytes = 0;

  do
    {
      /*** PRG01
           Readed = GEN_fread (cdrom_buffer, 1, BUFFER_SIZE, fp);
           neogeo_swab (cdrom_buffer, Ptr, Readed);
      ***/
      Readed = GEN_fread((char *)Ptr, 1, BUFFER_SIZE, fp);
      Ptr += Readed;
      totalbytes += Readed;
    }
  while (Readed == BUFFER_SIZE);	//Readed==BUFFER_SIZE &&

  cdrom_inc_progress(totalbytes);

  GEN_fclose(fp);

  if (Offset == 0 && ipl_in_progress)
    memcpy(neogeo_game_vectors, neogeo_prg_memory, 0x100);

  return 1;
}

/****************************************************************************
* cdrom_load_z80_file
****************************************************************************/
static int cdrom_load_z80_file(char *FileName, unsigned int Offset)
{
  GENFILE fp;
  char Path[256];
  int flen;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  /*** Safeguard against over-reads ***/
  flen = getfilelen(fp);

  //sprintf(cddebug,"%s : %d", FileName, flen);
  //ActionScreen(cddebug);

  if ((Offset + flen) > 0x10000)
    {
      sprintf(cddebug, "Z80 : %08x %d", Offset, flen);
      ActionScreen(cddebug);
      return 1;
    }

  totalbytes = GEN_fread((char *)subcpu_memspace + Offset, 1, 0x10000, fp);
  GEN_fclose(fp);

  cdrom_inc_progress(totalbytes);

  if ( Offset == 0 )
    mz80_reset();

  return 1;
}

/****************************************************************************
* cdrom_load_fix_file
*
* Note: FIX memory can be overwritten by an incoming file.
* To overcome this while the loading screens are on, the original FIX memory
* is copied to a temporary buffer and restored. The new data is then sent into
* program memory offset 0x115E06 for retrieval at the end of uploading.
****************************************************************************/
static int cdrom_load_fix_file(char *FileName, unsigned int Offset)
{
  GENFILE fp;
  char Path[256];
  unsigned char *Ptr, *Src;
  int Readed;
  int flen;
  int restore = 0;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  /*** Safeguard against over-reads ***/
  flen = getfilelen(fp);
  //sprintf(cddebug,"%s : %d", FileName, flen);
  //ActionScreen(cddebug);
  if ((Offset + flen) > 0x20000)
    {
      sprintf(cddebug, "FIX : %08x %d", Offset, flen);
      ActionScreen(cddebug);
      return 1;
    }

  if (m68k_read_memory_8(0x10FDDC) )
    {
      memcpy(neogeo_ipl_memory, neogeo_fix_memory, 0x6000);
      memcpy(neogeo_ipl_memory + 0x6000, video_fix_usage, 0x300);
      restore = 1;
    }

  Ptr = neogeo_fix_memory + Offset;

  totalbytes = 0;
  do
    {
      memset(cdrom_buffer, 0, BUFFER_SIZE);
      Readed = GEN_fread((char *)cdrom_buffer, 1, BUFFER_SIZE, fp);
      Src = cdrom_buffer;
      if (Src == NULL) { }
      if (Readed > 0)
        {
          memcpy(Ptr, cdrom_buffer, Readed);
          if ((Ptr == neogeo_fix_memory) && restore)
            memcpy(neogeo_prg_memory + 0x115E06, Ptr,
                   0x6000);
          neogeo_decode_fix(neogeo_fix_memory, Offset, Readed);
        }

      Ptr += Readed;
      Offset += Readed;
      totalbytes += Readed;
    }
  while (Readed == BUFFER_SIZE);

  GEN_fclose(fp);

  /*** Reinstate low memory ***/
  if (restore)
    {
      memcpy(neogeo_fix_memory, neogeo_ipl_memory, 0x6000);
      memcpy(video_fix_usage, neogeo_ipl_memory + 0x6000, 0x300);
    }

  cdrom_inc_progress(totalbytes);

  return 1;

}

/****************************************************************************
* cdrom_load_spr_file
****************************************************************************/
static int cdrom_load_spr_file(char *FileName, unsigned int Offset)
{
  GENFILE fp;
  char Path[256];
  unsigned char *Ptr;
  int Readed;
  int flen;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  /*** Safeguard against over-reads ***/
  flen = getfilelen(fp);
  //sprintf(cddebug,"%s : %d", FileName, flen);
  //ActionScreen(cddebug);
  if ((Offset + flen) > 0x400000)
    {
      ActionScreen(Path);
      sprintf(cddebug, "SPR : %08x %d", Offset, flen);
      ActionScreen(cddebug);
      return 1;
    }

  Ptr = neogeo_spr_memory + Offset;
  totalbytes = 0;
  do
    {
      memset(cdrom_buffer, 0, BUFFER_SIZE);
      Readed = GEN_fread((char *)cdrom_buffer, 1, BUFFER_SIZE, fp);
      if (Readed > 0)
        {
          memcpy(Ptr, cdrom_buffer, Readed);
          neogeo_decode_spr(neogeo_spr_memory, Offset, Readed);
          Offset += Readed;
          Ptr += Readed;
          totalbytes += Readed;
        }
    }
  while (Readed == BUFFER_SIZE);

  GEN_fclose(fp);
  cdrom_inc_progress(totalbytes);

  return 1;

}

/****************************************************************************
* cdrom_load_pcm_file
****************************************************************************/
static int cdrom_load_pcm_file(char *FileName, unsigned int Offset)
{
  GENFILE fp;
  char Path[256];
  unsigned char *Ptr;
  int bread = 0;
  int flen;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  if (Offset > 0x100000)
    return 1;

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  /*** Safeguard against over-reads ***/
  flen = getfilelen(fp);

  //sprintf(cddebug,"%s : %d", FileName, flen);
  //ActionScreen(cddebug);

  if ((Offset + flen) > 0x100000)
    {
      sprintf(cddebug, "PCM : %08x %d", Offset, flen);
      ActionScreen(cddebug);
      return 1;
    }

  Ptr = neogeo_pcm_memory + Offset;
  bread = GEN_fread((char *)Ptr, 1, flen, fp);
  totalbytes = bread;
  GEN_fclose(fp);

  cdrom_inc_progress(totalbytes);

  return 1;
}

/****************************************************************************
* cdrom_load_pat_file
****************************************************************************/
static int
cdrom_load_pat_file(char *FileName, unsigned int Offset, unsigned int Bank)
{
  GENFILE fp;
  char Path[256];
  int Readed;

  strcpy(Path, cdpath);
  strcat(Path, FileName);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      return 0;
    }

  Readed = GEN_fread((char *)cdrom_buffer, 1, BUFFER_SIZE, fp);
  cdrom_apply_patch((short *) cdrom_buffer, Offset, Bank);

  totalbytes = Readed;

  GEN_fclose(fp);
  cdrom_inc_progress(totalbytes);

  return 1;
}

/****************************************************************************
* cdrom_set_sectors
*
* Routine to set the number of sectors to be read
****************************************************************************/
void cdrom_set_sectors(void)
{
  LOADFILE *lfile = (LOADFILE *) (neogeo_prg_memory + 0x115A06);
  GENFILE fp;
  int i = 0;
  int flen;
  int total = 0;
  int Type;
  char *p;
  char fname[16];
  char path[256];

  if (m68k_read_memory_8(0x10FDDC))
    {
      while (lfile[i].fname[0] != 0)
        {
          strcpy(fname, lfile[i].fname);
          p = strstr(fname, ".");
          if (p)
            p++;

          Type = recon_filetype(p);
	  if ( Type == 0) { }

          p = strstr(fname, ";");
          if (p)
            *p = 0;

          strcpy(path, cdpath);
          strcat(path, fname);
          fp = GEN_fopen(path, "rb");
          if (fp)
            {
              GEN_fseek(fp, 0, SEEK_END);
              flen = GEN_ftell(fp);
              total += (flen >> 11);
              if (flen & 0x7ff)
                total++;
              GEN_fclose(fp);
            }

          i++;

        }

      m68k_write_memory_32(0x10F694, total);
      total = ((0x8000 / total) << 8) | 0x80;

      m68k_write_memory_32(0x10F68C, total);
      m68k_write_memory_32(0x10F690, 0);
      m68k_write_memory_8(0x10F793, 0);

    }
}

/****************************************************************************
* cdrom_load_files
*
* Now using BIOS prepared filenames
****************************************************************************/
void cdrom_load_files(void)
{
  LOADFILE *lfile;
  char FileName[16];
  int Bnk, Off, Type;
  char *p;
  int showscreen = 0;
  int address;

  /*** Don't let it go near the CDROM ***/
  m68k_write_memory_8(0x10F6C2, 7);
  m68k_write_memory_8(0x10FEDB, m68k_read_memory_8(0x10FEDE));

  /*** Get new pointer ***/
  address = m68k_read_memory_32(0x10F6A0);
  lfile = (LOADFILE *) (neogeo_prg_memory + address);
  if (lfile->fname[0] == 0)
    {
      return;
    }
  /*** Set img_display according to BIOS ***/
  img_display = 0;
  showscreen = m68k_read_memory_8(0x10FDDC);

#if 0
  /*** Stop any playing tracks ***/
  cdda_stop();
  cdda_current_track = 0;
#endif

  if (showscreen)
    {
      if (address == 0x115A06)
        {
          cdrom_set_sectors();
        }

      fix_disable = 0;
      video_clear();
      video_draw_fix();
      blitter();
    }

  /*** Decode file to load ***/
  /*** Fix filename ***/
  strcpy(FileName, lfile->fname);

  if (ipl_in_progress)
    lfile->image = 0xFFFF;

  p = strstr(FileName, ";");
  if (p)
    *p = 0;

  p = strstr(FileName, ".");
  if (p)
    p++;

  Type = recon_filetype(p);

  Bnk = lfile->bank >> 8;

  Off = lfile->offset;

  switch (Type)
    {
    case PRG_TYPE:
      cdrom_load_prg_file(FileName, Off);
      break;
    case FIX_TYPE:
      cdrom_load_fix_file(FileName, Off >> 1);
      break;
    case SPR_TYPE:
      /*** King of Fighters 99 ***/
      Bnk &= 3;
      cdrom_load_spr_file(FileName, (Bnk * 0x100000) + Off);
      break;
    case Z80_TYPE:
      cdrom_load_z80_file(FileName, Off >> 1);
      break;
    case PAT_TYPE:
      cdrom_load_pat_file(FileName, Off, Bnk);
      break;
    case PCM_TYPE:
      cdrom_load_pcm_file(FileName, (Bnk * 0x80000) + (Off >> 1));
      break;
    }

}

/****************************************************************************
* neogeo_start_upload
****************************************************************************/
void neogeo_start_upload(void)
{
  m68k_write_memory_8(0x10FE88, 0);

  if (m68k_read_memory_8(0x10FDDC))
    {
      neogeo_undecode_fix(neogeo_fix_memory, 0, 0x6000);
    }

  cpu_enabled = 0;
}

/****************************************************************************
* neogeo_progress_show
****************************************************************************/
void neogeo_progress_show(void)
{
  int progress;

  sectorstodo--;
  if (sectorstodo < 0)
    sectorstodo = 0;

  m68k_write_memory_16(0x10F688, sectorstodo--);

  progress = m68k_read_memory_32(0x10F690);
  progress += (m68k_read_memory_32(0x10F68C) << 4);
  m68k_write_memory_32(0x10F690, progress);
  if (progress >= 0x800000)
    m68k_write_memory_32(0x10F690, 0x800000);

  if ( m68k_read_memory_8(0x10FDDC) )
    {
      fix_disable = 0;
      video_clear();
      video_draw_fix();
      blitter();
    }
}

/****************************************************************************
* neogeo_end_upload
****************************************************************************/
void neogeo_end_upload(void)
{
  video_clear();
  video_draw_fix();
  blitter();

  m68k_write_memory_32(0x10F68C, 0x00000000);
  m68k_write_memory_8(0x10F6C3, 0x00);
  m68k_write_memory_8(0x10F6D9, 0x01);
  m68k_write_memory_8(0x10F6DB, 0x01);
  m68k_write_memory_32(0x10F742, 0x00000000);
  m68k_write_memory_32(0x10F746, 0x00000000);
  m68k_write_memory_8(0x10FDC2, 0x01);
  m68k_write_memory_8(0x10FDDC, 0x00);
  m68k_write_memory_8(0x10FDDD, 0x00);
  m68k_write_memory_8(0x10FE85, 0x01);
  m68k_write_memory_8(0x10FE88, 0x00);
  m68k_write_memory_8(0x10FEC4, 0x01);

  fix_disable = 0;
  img_display = 0;
  spr_disable = 0;
  video_enable = 1;
  cpu_enabled = 1;
}

/****************************************************************************
* neogeo_upload
*
* Called from M68000 core
****************************************************************************/
void neogeo_upload(void)
{
  int Zone;
  int Taille;
  int Banque;
  int Offset = 0;
  unsigned char *Source;
  unsigned char *Dest;

  Zone = m68k_read_memory_8(0x10FEDA);

  switch (Zone & 0x0F)
    {
    case 0:			// PRG

      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      Dest = neogeo_prg_memory + m68k_read_memory_32(0x10FEF4);
      Taille = m68k_read_memory_32(0x10FEFC);

      memcpy(Dest, Source, Taille);

      m68k_write_memory_32(0x10FEF4,
                           m68k_read_memory_32(0x10FEF4) + Taille);

      break;

    case 2:			// SPR
      Banque = m68k_read_memory_8(0x10FEDB);
      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      Offset = m68k_read_memory_32(0x10FEF4) + (Banque << 20);
      Dest = neogeo_spr_memory + Offset;
      Taille = m68k_read_memory_32(0x10FEFC);

      memcpy(Dest, Source, Taille);
      neogeo_decode_spr(neogeo_spr_memory, Offset, Taille);

      // Mise à jour des valeurs
      Offset = m68k_read_memory_32(0x10FEF4);
      Banque = m68k_read_memory_8(0x10FEDB);
      Taille = m68k_read_memory_8(0x10FEFC);

      Offset += Taille;

      while (Offset > 0x100000)
        {
          Banque++;
          Offset -= 0x100000;
        }

      m68k_write_memory_32(0x10FEF4, Offset);
      m68k_write_memory_16(0x10FEDB, Banque);

      break;

    case 1:			// FIX
      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      Offset = m68k_read_memory_32(0x10FEF4) >> 1;
      Dest = neogeo_fix_memory + Offset;
      Taille = m68k_read_memory_32(0x10FEFC);

      memcpy( Dest, Source, Taille );
      neogeo_decode_fix(neogeo_fix_memory, Offset, Taille);

      Offset = m68k_read_memory_32(0x10FEF4);
      Taille = m68k_read_memory_32(0x10FEFC);

      Offset += (Taille << 1);

      m68k_write_memory_32(0x10FEF4, Offset);

      break;

    case 3:			// Z80

      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      Dest = subcpu_memspace + (m68k_read_memory_32(0x10FEF4) >> 1);
      Taille = m68k_read_memory_32(0x10FEFC);
      m68k_write_memory_32(0x10FEF4,
                           m68k_read_memory_32(0x10FEF4) + (Taille << 1));

      break;

    case 5:			// Z80 patch

      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      cdrom_apply_patch((short *) Source, m68k_read_memory_32(0x10FEF4),
                        m68k_read_memory_8(0x10FEDB));

      break;

    case 4:			// PCM
      Banque = m68k_read_memory_8(0x10FEDB);
      Source = neogeo_prg_memory + m68k_read_memory_32(0x10FEF8);
      Offset = (m68k_read_memory_32(0x10FEF4) >> 1) + (Banque << 19);
      Dest = neogeo_pcm_memory + Offset;
      Taille = m68k_read_memory_32(0x10FEFC);

      memcpy(Dest, Source, Taille);

      // Mise à jour des valeurs
      Offset = m68k_read_memory_32(0x10FEF4);
      Banque = m68k_read_memory_8(0x10FEDB);
      Taille = m68k_read_memory_8(0x10FEFC);

      Offset += (Taille << 1);

      while (Offset > 0x100000)
        {
          Banque++;
          Offset -= 0x100000;
        }

      m68k_write_memory_32(0x10FEF4, Offset);
      m68k_write_memory_16(0x10FEDB, Banque);

      break;

    }
}

/****************************************************************************
* cdrom_load_logo
*
* This is required for the funky loading screens
***************************************************************************/
static void cdrom_load_logo(void)
{
  char logo[12] = LOGO_X_PRG;
  char jue[4] = JUE;
  char Path[256];
  GENFILE fp;

  logo[5] = jue[m68k_read_memory_8(0x10FD83) & 3];

  strcpy(Path, cdpath);
  strcat(Path, logo);

  fp = GEN_fopen(Path, "rb");
  if (fp)
    {
      GEN_fread(((char *)neogeo_prg_memory + 0x120000), 1, 0x20000, fp);
      GEN_fclose(fp);
    }
}

#define PATCH_Z80(a, b) { \
			    subcpu_memspace[(a) & 0xffff] = (b) & 0xff; \
			    subcpu_memspace[(a + 1) & 0xffff] = ((b)>>8) & 0xff; \
			}

void cdrom_apply_patch(short *source, int offset, int bank)
{
  int master_offset;

  master_offset = (((bank * 0x100000) + offset) >> 8) & 0xFFFF;

  while (*source != 0)
    {
      PATCH_Z80(source[0], ((source[1] + master_offset) >> 1));
      PATCH_Z80(source[0] + 2, (((source[2] + master_offset) >> 1) - 1));

      if ((source[3]) && (source[4]))
        {
          PATCH_Z80(source[0] + 5, ((source[3] + master_offset) >> 1));
          PATCH_Z80(source[0] + 7,
                    (((source[4] + master_offset) >> 1) - 1));
        }

      source += 5;
    }
}

/****************************************************************************
* neogeo_ipl
*
* This is an attempt at processing the IPL through BIOS
****************************************************************************/
void neogeo_ipl(void)
{
  GENFILE fp;
  char Path[256];
  char buf[2048];
  int len;

  cdrom_load_logo();

  strcpy(Path, cdpath);
  strcat(Path, IPL_TXT);

  fp = GEN_fopen(Path, "rb");
  if (!fp)
    {
      ActionScreen((char *) "NO IPL!");
      neogeocd_exit();
    }

  /*** IPL.TXT is limited to 1k
         It looks for the EOF terminator - so make sure they exist
         If you want to be mean to the warez monkey - comment the
         next line out ;)
  ****/
  memset(buf, 26, 2048);
  len = GEN_fread(buf, 1, 2048, fp);
  memcpy(neogeo_prg_memory + 0x111204, buf, len + 2);
  GEN_fclose(fp);

  ipl_in_progress = 1;
  cpu_enabled = 0;

}
