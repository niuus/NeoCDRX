/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* Generic File I/O
*
* This module attempts to provide a single interface for file I/O.
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include "fileio.h"

static GENHANDLER genhandler;

/****************************************************************************
* GEN_SetHandler
*
* Call BEFORE using any of the other functions
****************************************************************************/
void
GEN_SetHandler (GENHANDLER * g)
{
  memcpy (&genhandler, g, sizeof (GENHANDLER));
}

/****************************************************************************
* GEN_fopen
*
* Passthrough fopen
****************************************************************************/
u32
GEN_fopen (const char *filename, const char *mode)
{
  if (genhandler.gen_fopen == NULL)
    return 0;			/*** NULL - no file or handler ***/

  return (genhandler.gen_fopen) (filename, mode);
}

/****************************************************************************
* GEN_fread
*
* Passthrough fread
****************************************************************************/
u32
GEN_fread (char *buffer, int block, int length, u32 fp)
{
  if (genhandler.gen_fread == NULL)
    return 0;

  return (genhandler.gen_fread) (buffer, block, length, fp);
}

/****************************************************************************
* GEN_fwrite
*
* Passthrough fwrite
****************************************************************************/
u32
GEN_fwrite (char *buffer, int block, int length, u32 fp)
{
  if (genhandler.gen_fwrite == NULL)
    return 0;

  return (genhandler.gen_fwrite) (buffer, block, length, fp);
}

/****************************************************************************
* GEN_fclose
*
* Passthrough fclose
****************************************************************************/
int
GEN_fclose (u32 fp)
{
  if (genhandler.gen_fclose == NULL)
    return 0;

  return (genhandler.gen_fclose) (fp);
}

/****************************************************************************
* GEN_fseek
*
* Passthrough fseek
****************************************************************************/
int
GEN_fseek (u32 fp, int where, int whence)
{
  if (genhandler.gen_fseek == NULL)
    return 0;

  return (genhandler.gen_fseek) (fp, where, whence);
}

/****************************************************************************
* GEN_ftell
*
* Passthrough ftell
****************************************************************************/
int
GEN_ftell (u32 fp)
{
  if (genhandler.gen_ftell == NULL)
    return -1;

  return (genhandler.gen_ftell) (fp);
}

/****************************************************************************
* GEN_fcloseall
***************************************************************************/
void
GEN_fcloseall (void)
{
  if (genhandler.gen_fcloseall == NULL)
    return;

  (genhandler.gen_fcloseall) ();
}


/****************************************************************************
* GEN_fcloseall
***************************************************************************/
int
GEN_getdir (char *dir)
{
  if (genhandler.gen_getdir == NULL)
    return 0;

  return (genhandler.gen_getdir) (dir);
}

/****************************************************************************
* GEN_mount
***************************************************************************/
void
GEN_mount (void)
{
  if (genhandler.gen_mount == NULL)
    return;

  (genhandler.gen_mount) ();
}

