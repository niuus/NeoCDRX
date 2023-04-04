/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* SD FileIO
*
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ogc/mutex.h>
#include <sys/dir.h>
#include <fat.h>
#include "neocdrx.h"
#include "fileio.h"
#include <dirent.h>

/* Generic File I/O */
#define MAXFILES	32
static FILE *sdfsfiles[MAXFILES];
static GENHANDLER sdhandler;
static u32 sdmutex = 0;

#define MAXDIRENTRIES 0x4000
static char *direntries[MAXDIRENTRIES];

char msg[128];

#ifndef HW_RVL
extern const DISC_INTERFACE* WKF_slot;
#endif
extern const DISC_INTERFACE* IDEA_slot;
extern const DISC_INTERFACE* IDEB_slot;

int have_ROM;
/****************************************************************************
* SDFindFree
****************************************************************************/
int SDFindFree( void )
{
  int i;

  for( i = 0; i < MAXFILES; i++ )
    {
      if ( sdfsfiles[i] == NULL )
        return i;
    }

  return -1;
}

/****************************************************************************
* SDfopen
****************************************************************************/
static u32
SDfopen (const char *filename, const char *mode)
{
  /* No writing allowed */
  if ( strstr(mode,"w") )
    {
      return 0;
    }

  /* Open for reading */
  int handle = SDFindFree();
  if ( handle == -1 )
    {
      sprintf(msg,"OUT OF HANDLES!");
      ActionScreen(msg);
      return 0;
    }

  while ( LWP_MutexLock( sdmutex ) );

  sdfsfiles[handle] =  fopen(filename, mode);
  if ( sdfsfiles[handle] == NULL )
    {
      LWP_MutexUnlock( sdmutex );
      return 0;
    }

  LWP_MutexUnlock( sdmutex );
  return handle | 0x8000;
}

/****************************************************************************
* SDfclose
****************************************************************************/
static int
SDfclose (u32 fp)
{
  while ( LWP_MutexLock( sdmutex ) );

  if( sdfsfiles[fp & 0x7FFF] != NULL )
    {
      fclose(sdfsfiles[fp & 0x7FFF]);
      sdfsfiles[fp & 0x7FFF] = NULL;
      LWP_MutexUnlock( sdmutex );
      return 1;
    }

  LWP_MutexUnlock( sdmutex );
  return 0;
}

/****************************************************************************
* SDfread
****************************************************************************/
static u32
SDfread (char *buf, int block, int len, u32 fp)
{
  int handle = ( fp & 0x7FFF );

  if( sdfsfiles[handle] == NULL )
    return 0;

  while ( LWP_MutexLock ( sdmutex ) );

  int bytesdone = fread(buf, block, len, sdfsfiles[handle]);

  LWP_MutexUnlock( sdmutex );

  return bytesdone;

}

/****************************************************************************
* SDSeek
****************************************************************************/
static int
SDfseek (u32 fp, int where, int whence)
{
  int handle = ( fp & 0x7FFF );

  if ( sdfsfiles[handle] == NULL )
    {
      sprintf(msg,"SEEK : Invalid Handle %d", handle);
      ActionScreen(msg);
      return -1;
    }

  return fseek(sdfsfiles[handle], where, whence );

}

/****************************************************************************
* SDftell
*
* Return current position
****************************************************************************/
static int
SDftell (u32 fp)
{
  int handle = ( fp & 0x7FFF );

  if ( sdfsfiles[handle] == NULL )
    {
      sprintf(msg,"FTELL : Invalid Handle %d", handle);
      ActionScreen(msg);
      return -1;
    }

  return ftell(sdfsfiles[handle]);
}

static void
SDfcloseall (void)
{
  int i;

  while ( LWP_MutexLock( sdmutex ) );

  for( i = 0; i < MAXFILES; i++ )
    {
      if ( sdfsfiles[i] != NULL )
        fclose(sdfsfiles[i]);
    }

  LWP_MutexUnlock( sdmutex );
}

void SortListing( int max )
{
  int top,seek;
  char *t;

  for( top = 0; top < max - 1; top++ )
    {
      for( seek = top + 1; seek < max; seek++ )
        {
          if ( stricmp(direntries[top], direntries[seek]) > 0 )
            {
              t = direntries[top];
              direntries[top] = direntries[seek];
              direntries[seek] = t;
            }
        }
    }
}

/****************************************************************************
 * SDgetdir
 ****************************************************************************/
static int SDgetdir( char *thisdir )
{
  DIR *dirs = NULL;
  static int count = 0;
  int i;
  unsigned int *p;
  struct dirent *entry;

  for( i = 0; i < count; i++ )
    free(direntries[i]);

  count = 0;

  dirs = opendir(thisdir);
  if ( dirs != NULL )
    {
      entry = readdir(dirs);
      while ( entry != NULL )
        {
          /* Only get subdirectories */
         if (entry->d_type == DT_DIR)
		   if (strcmp(entry->d_name,".") &&
             strcmp(entry->d_name,".."))
		   {
			 direntries[count++] = strdup(entry->d_name);
		   }

		  if ( count == MAXDIRENTRIES ) break;
          entry = readdir(dirs);
        }
    }

  if ( count > 1 )
    SortListing(count);

  memcpy(dirbuffer, &count, 4);
  p = (unsigned int *)(dirbuffer + 4);
  for ( i = 0; i < count; i++ )
    {
      memcpy(&p[i], &direntries[i], 4);
    }

  return count;
}


/****************************************************************************
* SDmount
****************************************************************************/
static void SDmount()
{
  memset (basedir, 0, 1024);
  memset (scratchdir, 0, 1024);
  memset (dirbuffer, 0, 0x10000);

  memset (root_dir, 0, 10);

  // Define DIR search location by Device type
  if (use_IDE) {
     if ( IDEA_slot->startup() && fatMountSimple("IDEA", IDEA_slot) ) sprintf(root_dir,"IDEA:");
     else if ( IDEB_slot->startup() && fatMountSimple("IDEB", IDEB_slot) ) sprintf(root_dir,"IDEB:");
     else { ActionScreen ("IDE-EXI not initialized"); return; }
  }
#ifdef HW_RVL
  else if (use_SD)  sprintf(root_dir,"sd:");               //  search this DIR first
  else if (use_USB) sprintf(root_dir,"usb:");
#else
  else if (use_WKF) {
     if ( WKF_slot->startup() && fatMountSimple("WKF", WKF_slot) )  sprintf(root_dir,"WKF:");
     else { ActionScreen ("WKF not initialized"); return; }
  }
#endif

  // Test if DIR exists, else default to root DIR by Device
  sprintf(basedir,"%s/NeoCDRX/games/",root_dir);
  DIR *dir = opendir(basedir);
  if (!dir) sprintf(basedir,"%s/",root_dir);
  else closedir(dir);

  if ( !SDgetdir(basedir) )
    return;

  DirSelector();

  // Do not show the banner if there is no title selected
  if (have_ROM == 1) bannerscreen();
}

void
SD_SetHandler ()
{
  /* Clear */
  memset(&sdhandler, 0, sizeof(GENHANDLER));
  memset(&sdfsfiles, 0, MAXFILES * 4);
  memset(&direntries, 0, MAXDIRENTRIES * 4);

  sdhandler.gen_fopen = SDfopen;
  sdhandler.gen_fclose = SDfclose;
  sdhandler.gen_fread = SDfread;
  sdhandler.gen_fseek = SDfseek;
  sdhandler.gen_ftell = SDftell;
  sdhandler.gen_fcloseall = SDfcloseall;
  sdhandler.gen_getdir = SDgetdir;
  sdhandler.gen_mount = SDmount;
 
  GEN_SetHandler (&sdhandler);

  /* Set mutex */
  LWP_MutexInit(&sdmutex, FALSE);

}
