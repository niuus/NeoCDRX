/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* DVD File I/O
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neocdrx.h"
#include "fileio.h"
#include "iso9660.h"

#ifdef HW_RVL
#include <di/di.h>
#endif

// Generic file system
#define MAXFILES 32
#ifdef HW_RVL
#define BUFFSIZE 0x800   /* LIBDI requires 2048 bytes sectors */
#else
#define BUFFSIZE 0x8000
#endif

GENFILEINFO fileinfo[MAXFILES];
static GENHANDLER dvdhandler;
static u8 dvdfixup[BUFFSIZE] ATTRIBUTE_ALIGN (32);
extern int usleep();

/****************************************************************************
* DVDFindFree
****************************************************************************/
static int
DVDFindFree (void)
{
  int i = 0;

  while (i < MAXFILES)
    {
      if (fileinfo[i].handle == -1)
        return i;
      i++;
    }

  return -1;
}

/****************************************************************************
* DVD_fopen
*
* Open a file on the DVD. This will only apply to the current DVD directory.
****************************************************************************/
static u32
DVDfopen (const char *filename, const char *mode)
{
  DIRENTRY *fdir;
  int handle;

  //  No write mode available
  if (strstr (mode, "w"))
    return 0;

  handle = DVDFindFree ();
  if (handle == -1)
    return 0;

  fdir = FindFile ((char *)filename);

  if (fdir == NULL)
    {
      return 0;
    }

  //  Make a copy in the file info structure
  fileinfo[handle].handle = handle;
  fileinfo[handle].mode = GEN_MODE_READ;
  fileinfo[handle].length = fdir->lengthbe;
  fileinfo[handle].offset_on_media64 = (long long int)fdir->offsetbe * 2048;
  fileinfo[handle].currpos = 0;

  return handle | 0x8000;
}

/****************************************************************************
* DVD_fclose
*
* Close a previously opened file
****************************************************************************/
static int
DVDfclose (u32 fp)
{
  if (fileinfo[fp & 0x7fff].handle != -1)
    {
      fileinfo[fp & 0x7fff].handle = -1;
      return 1;
    }
  return 0;
}

/****************************************************************************
* DVDfread
****************************************************************************/
static u32
DVDfread (char *buf, int block, int len, u32 fp)
{
  int handle = fp & 0x7fff;
  int bytesrequested = 0;
  int bytesavailable = 0;
  int bytestoread = 0;
  int bytesdone = 0;
  int blocks = 0;
  int i, remain;

  //  Is this handle valid?
  if (fileinfo[handle].handle == -1)
    return 0;

  bytesrequested = (block * len);
  bytesavailable = fileinfo[handle].length - fileinfo[handle].currpos;

  if (bytesrequested <= bytesavailable)
    bytestoread = bytesrequested;
  else
    bytestoread = bytesavailable;

  if (bytestoread == 0)
    return 0;

  memset (dvdfixup, 0, BUFFSIZE);

  //  How many blocks
  blocks = bytestoread / BUFFSIZE;

  if (blocks)
    {
      for (i = 0; i < blocks; i++)
        {
          gcsim64 (dvdfixup,
                   fileinfo[handle].offset_on_media64 + (long long int)fileinfo[handle].currpos,
                   BUFFSIZE);
          memcpy (buf + bytesdone, dvdfixup, BUFFSIZE);
          fileinfo[handle].currpos += BUFFSIZE;
          bytesdone += BUFFSIZE;
        }
    }

  remain = (bytestoread & (BUFFSIZE - 1));

  //  Get remaining bytes
  if (remain)
    {
      gcsim64 (dvdfixup,
               fileinfo[handle].offset_on_media64 + (long long int)fileinfo[handle].currpos,
               BUFFSIZE);
      memcpy (buf + bytesdone, dvdfixup, remain);
      fileinfo[handle].currpos += remain;
      bytesdone += remain;
    }

  return bytesdone;
}

/****************************************************************************
* DVD_fseek
*
* Seek to a position in a file
****************************************************************************/
static int
DVDfseek (u32 fp, int where, int whence)
{
  int handle;

  handle = fp & 0x7fff;

  if (fileinfo[handle].handle == -1)
    return -1;

  switch (whence)
    {
    case SEEK_END:
      if (where > 0)
        return 0;       //  Fail

      fileinfo[handle].currpos = fileinfo[handle].length + where;
      return 1;

    case SEEK_CUR:
      if ((where + fileinfo[handle].currpos) > fileinfo[handle].length)
        return 0;

      fileinfo[handle].currpos += where;
      return 1;

    case SEEK_SET:
      if (where < 0)
        return 0;

      fileinfo[handle].currpos = where;
      return 1;
    }

  return 0;
}

/****************************************************************************
* DVD_ftell
*
* Return current position
****************************************************************************/
static int
DVDftell (u32 fp)
{
  int handle = fp & 0x7fff;

  if (fileinfo[handle].handle != -1)
    {
      return fileinfo[handle].currpos;
    }

  return 0;
}

/****************************************************************************
* DVDfcloseall
****************************************************************************/
static void
DVDfcloseall (void)
{
  memset (&fileinfo, 0xff, sizeof (GENFILEINFO) * MAXFILES);
}

/****************************************************************************
* DVDgetdir
****************************************************************************/
static int
DVDgetdir(char *dir)
{
  int *p = (int *) dirbuffer;

  GetSubDirectories (dir, dirbuffer, 0x10000);
  return p[0];
}

/****************************************************************************
* DVDmount
****************************************************************************/
static void
DVDmount ( void )
{
  memset (basedir, 0, 1024);
  memset (scratchdir, 0, 1024);
//  memset (dirbuffer, 0, 0x10000);

  if (!mount_image ())
  {
#ifdef HW_RVL
    DI_Init();

    u32 val;
    DI_GetCoverRegister(&val);
    while(val & 0x1)
    {
      InfoScreen((char *) "Please insert disc !");
      DI_GetCoverRegister(&val);
    }
    DI_Mount();
    while(DI_GetStatus() & DVD_INIT) usleep(10);
    if (!(DI_GetStatus() & DVD_READY))
    {
      char msg[50];
      sprintf(msg, "DI Status Error: 0x%08X !\n",DI_GetStatus());
      ActionScreen(msg);
    }
#else
    DVD_Init();
    DVD_Mount();
#endif

    if (!mount_image())
    {
	ActionScreen((char *) "Error reading disc !");
    }
  }
  
  //  Always start at the root
  strcpy (basedir, "/");

  //  Build sub-directory tree
  DVDgetdir (basedir);

  //  Do dir selection
  DirSelector ();
  
  bannerscreen();
}


/****************************************************************************
* DVD_SetHandler
****************************************************************************/
void
DVD_SetHandler (void)
{
  /*** Clear ***/
  memset (&dvdhandler, 0, sizeof (GENHANDLER));
  memset (&fileinfo, 0xff, sizeof (GENFILEINFO) * MAXFILES);

  /*** Set generic handlers ***/
  dvdhandler.gen_fopen = DVDfopen;
  dvdhandler.gen_fclose = DVDfclose;
  dvdhandler.gen_fread = DVDfread;
  dvdhandler.gen_fseek = DVDfseek;
  dvdhandler.gen_ftell = DVDftell;
  dvdhandler.gen_fcloseall = DVDfcloseall;
  dvdhandler.gen_getdir = DVDgetdir;
  dvdhandler.gen_mount = DVDmount;

  GEN_SetHandler (&dvdhandler);
}

