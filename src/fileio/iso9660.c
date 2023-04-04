/****************************************************************************
* ISO 9660 Parser
****************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <ogc/mutex.h>

#include "neocdrx.h"

#ifdef HW_RVL
#include <di/di.h>
#endif

#define MAXCACHED 128
#define SECTORSIZE 2048

#define GCMAXCAPACITY   0x57057C00ULL
#define WIIMAXCAPACITY  4699979776ULL

/** DVD I/O Address base **/
volatile unsigned long *dvd = (volatile unsigned long *) 0xCC006000;

#ifndef HW_RVL
static u32 dvdmutex = 0;
#endif

static char dvdbuffer[SECTORSIZE] ATTRIBUTE_ALIGN (32);
static PVD pvd;
static char *pathtable;
static char *pathdecs;
static int pdeclength;
static int pathentries;
static int joliet = 0;

static void dumppathtable (void);
static void DecodePathTable (void);
static CACHEFILE cachedfiles[MAXCACHED];
static int cached = 0;

#ifndef HW_RVL
/* GC/Wii integration */
typedef struct
  {
    u16 rev;
    u16 dev;
    u32 reldate;
    u8 pad[24];
  }
DVDDriveInfo;

static DVDDriveInfo dinfo ATTRIBUTE_ALIGN(32);
#endif

static long long int DVDMaxCapacity = 0;
extern int usleep();


/****************************************************************************
* SetDriveCapacity
****************************************************************************/
static void
SetDriveCapacity( void )
{
#ifndef HW_RVL
  /* Get DVD Info */
  DCInvalidateRange((void *)&dinfo, 32);

  dvd[0] = 0x2E;
  dvd[1] = dvd[3] = 0;
  dvd[2] = 0x12000000;
  dvd[4] = dvd[6] = 0x20;
  dvd[5] = (u32)&dinfo;
  dvd[7] = 3;

  while( dvd[7] & 1 );

   InfoScreen((char *) " ");  

// WKF dinfo.reldate is detected as 537330195
   if ((dinfo.reldate == 537330195) && (__wkfSpiReadId() != 0 && __wkfSpiReadId() != 0xFFFFFFFF) && (wkfIsInserted(0) == true) ){
     ActionScreen ((char *) "Remove WKF SDcard to enable Flatmode");
     __wkfReset();
     SetDriveCapacity();
  }



  switch( dinfo.reldate )
    {
      /* Known GC Drives */
    case 0x20020402:	/* Model 04 */
    case 0x20010608:	/* Model 06 */
    case 0x20020823:	/* Model 08 */
    case 0x20010831:	/* Panasonic */
//     DVD_Reset(DVD_RESETHARD);
      DVDMaxCapacity = GCMAXCAPACITY;
      break;

    default:
      DVDMaxCapacity = WIIMAXCAPACITY;	/* Assume Wii */
    }
#else
  DVDMaxCapacity = WIIMAXCAPACITY;
#endif
}

/****************************************************************************
 * dvd_motor_off
 ****************************************************************************/
void dvd_motor_off( void )
{
#ifndef HW_RVL
  dvd[0] = 0x2e;
  dvd[1] = 0;
  dvd[2] = 0xe3000000;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = 0;
  dvd[6] = 0;
  dvd[7] = 1; // Do immediate
  while (dvd[7] & 1) usleep(10);

  /*** PSO Stops blackscreen at reload ***/
  dvd[0] = 0x14;
  dvd[1] = 0;

#else
  DI_StopMotor();
#endif
}

/****************************************************************************
* GCSim64
****************************************************************************/
int gcsim64( void *buffer, long long int offset, int length )
{

#ifndef HW_RVL
  while ( LWP_MutexLock( dvdmutex ) != 0 )
    usleep(50);
#endif

  DCInvalidateRange ((void *) buffer, length);

  if ( offset < DVDMaxCapacity )
    {
#ifndef HW_RVL
      dvd[0] = 0x2E;
      dvd[1] = 0;
      dvd[2] = 0xA8000000;

      u32 ofs = offset >> 2;
      dvd[3] = ofs;

      dvd[4] = length;
      dvd[5] = (unsigned long) buffer;
      dvd[6] = length;
      dvd[7] = 3;			/*** Enable reading with DMA ***/
      while (dvd[7] & 1);

      if (dvd[0] & 0x4)		/* Ensure it has completed */
        {
          LWP_MutexUnlock( dvdmutex );
          return 0;
        }

#else
    if (DI_ReadDVD(buffer, length >> 11, (u32)(offset >> 11)))
        return 0;
#endif
    }
  else				// Let's not read past end of DVD
    {
      ActionScreen ((char *) "Offset exceeds Capacity!");
#ifndef HW_RVL
      LWP_MutexUnlock( dvdmutex );
#endif
  return 0;
    }

#ifndef HW_RVL
  LWP_MutexUnlock( dvdmutex );
#endif
  return 1;

}

#ifndef HW_RVL
/****************************************************************************
* FlushXenoCache
*
* Flush the read cache on Xeno to ensure that any later data is correct.
****************************************************************************/
static void
FlushXenoCache( void )
{
  u8 *cache;

  cache = memalign(32,0x10000);

  /* Step 1 - read 64K from offset zero */
  gcsim64(cache, 0LL, 0x10000);

  /* Step 2 - read from > 4MB */
  gcsim64(cache, 0x400800, 0x10000);

  free(cache);
}
#endif

/****************************************************************************
* Mount Image
****************************************************************************/
int
mount_image (void)
{
  int sector = 15;
  int iso9660 = 0;
  int i;

#ifndef HW_RVL
  static int have_mutex = 0;
  if ( !have_mutex )
    {
      LWP_MutexInit( &dvdmutex, FALSE );
      have_mutex = 1;
    }
#endif

  /* Set drive capacity */
  SetDriveCapacity();

#ifndef HW_RVL
  /* Xeno */
  FlushXenoCache();
#endif

  memset (&pvd, 0, sizeof (PVD));
  memset (&cachedfiles, 0, sizeof (CACHEFILE) * MAXCACHED);

  /*** Find PVD ***/
  while (sector < 64)
    {
      if( !gcsim64 (dvdbuffer, (long long int)sector * SECTORSIZE, SECTORSIZE) )
      {
        return 0;
      }

      if ((memcmp (dvdbuffer, "\2CD001\1", 8)) == 0)
        {
          joliet = 1;

          /*** Fixup for text strings ***/
          for (i = 0; i < 32; i++)
            dvdbuffer[i + SYSID] = dvdbuffer[((i << 1) + 1) + SYSID];
          for (i = 0; i < 32; i++)
            dvdbuffer[i + VOLID] = dvdbuffer[((i << 1) + 1) + VOLID];
          for (i = 0; i < 128; i++)
            dvdbuffer[i + VOLSETID] = dvdbuffer[((i << 1) + 1) + VOLSETID];
          for (i = 0; i < 128; i++)
            dvdbuffer[i + PUBSETID] = dvdbuffer[((i << 1) + 1) + PUBSETID];
          for (i = 0; i < 128; i++)
            dvdbuffer[i + APPSETID] = dvdbuffer[((i << 1) + 1) + APPSETID];
          for (i = 0; i < 128; i++)
            dvdbuffer[i + DATASETID] = dvdbuffer[((i << 1) + 1) + DATASETID];

          break;
        }
      sector++;
    }

  if (!joliet)
    {
      sector = 15;
      while (sector < 64)
        {
          if ( !gcsim64 (dvdbuffer, (long long int)sector * SECTORSIZE, SECTORSIZE) );
          {
            return 0;
          }

          if ((memcmp (dvdbuffer, "\1CD001\1", 8)) == 0)
            {
              iso9660 = 1;
              break;
            }
          sector++;
        }
    }

  if (!(joliet + iso9660))
    {
      ActionScreen((char *) "Unsupported disc format !");
      return 0;
    }

  pvd.pvdoffset = sector * SECTORSIZE;
  memcpy (&pvd.rootoffset, dvdbuffer + PVDROOT + EXTENT, 4);
  memcpy (&pvd.rootlength, dvdbuffer + PVDROOT + FILE_LENGTH, 4);
  memcpy (&pvd.pathtablelength, dvdbuffer + PTABLELENGTH, 4);
  memcpy (&pvd.pathtableoffset, dvdbuffer + PTABLEOFFSET, 4);

  memcpy (pvd.system_id, dvdbuffer + SYSID, 32);
  memcpy (pvd.volume_id, dvdbuffer + VOLID, 32);
  memcpy (pvd.volset_id, dvdbuffer + VOLSETID, 128);
  memcpy (pvd.pubset_id, dvdbuffer + PUBSETID, 128);
  memcpy (pvd.appset_id, dvdbuffer + APPSETID, 128);
  memcpy (pvd.dataset_id, dvdbuffer + DATASETID, 128);

  pvd.rootoffset = pvd.rootoffset * SECTORSIZE;
  pvd.pathtableoffset = pvd.pathtableoffset * SECTORSIZE;

  pvd.pathtablelength &= ~0x1f;
  pvd.pathtablelength += 64;

  if (!pathtable) pathtable = memalign (32, pvd.pathtablelength);
  if (!pathtable) return 0;
  memset (pathtable, 0, pvd.pathtablelength);

  /*** How many blocks ***/
  int blocks = pvd.pathtablelength / SECTORSIZE;
  int bytesdone = 0;

  if (blocks)
    {
      for (i = 0; i < blocks; i++)
        {
          if ( !gcsim64 (dvdbuffer,
                   (long long int)pvd.pathtableoffset + bytesdone,
                   SECTORSIZE))
          {
            ActionScreen((char *) "Error reading pathtable sector !");
            return 0;
          }
          memcpy (pathtable + bytesdone, dvdbuffer, SECTORSIZE);
          bytesdone += SECTORSIZE;
        }
    }

  /*** Get remaining bytes ***/
  int remain = pvd.pathtablelength % SECTORSIZE;
  if (remain)
    {
      if ( !gcsim64 (dvdbuffer,
              (long long int)pvd.pathtableoffset + bytesdone,
              SECTORSIZE))
      {
        ActionScreen((char *) "Error reading pathtable last sector !");
        return 0;
      }
      memcpy (pathtable + bytesdone, dvdbuffer, remain);
    }

  dumppathtable ();
  DecodePathTable ();

  return 1;

}

/****************************************************************************
* unmount_image
****************************************************************************/
void
unmount_image (void)
{
  if (pathdecs) free (pathdecs);
  if (pathtable) free (pathtable);
#ifndef HW_RVL
  if (dvdmutex) LWP_MutexDestroy(dvdmutex);
#endif
}

/****************************************************************************
* Dump Path Table
****************************************************************************/
static void
dumppathtable (void)
{
  PATHHDR *phdr;
  char *pname;
  int pos;
  int ofs = 0;

  phdr = (PATHHDR *) pathtable;
  pathentries = pdeclength = 0;

  while (phdr->nlength)
    {
      pname = (char *) phdr + (sizeof (PATHHDR));
      if (pname == NULL) { } 
      
      pos = sizeof (PATHHDR) + phdr->nlength;
      if (phdr->nlength & 1)
        pos++;

      ofs += pos;
      pathentries++;
      pdeclength += (phdr->nlength + 1);

      phdr = (PATHHDR *) (pathtable + ofs);
    }
}

/****************************************************************************
* Decode Path Table
*
* *WARNING* - This reuses the pathtable block as a string holder.
****************************************************************************/
static void
DecodePathTable (void)
{
  PATHHDR *phdr = (PATHHDR *) pathtable;
  PATHDECODE *pdec;
  int i, j, ph;
  int ofs, pos;
  char *pname;
  int strofs = 0;

  if (!pathdecs) pathdecs = memalign (32, pathentries * sizeof (PATHDECODE));
  memset (pathdecs, 0, (pathentries * sizeof (PATHDECODE)));
  pdec = (PATHDECODE *) pathdecs;
  ofs = pos = strofs = 0;

  /*** Decode path information ***/
  for (i = 0; i < pathentries; i++)
    {
      pname = (char *) phdr + sizeof (PATHHDR);
      pos = sizeof (PATHHDR) + phdr->nlength;
      if (phdr->nlength & 1)
        pos++;

      pdec[i].offset64 = (long long int)phdr->record * 2048;
      pdec[i].parent = phdr->parent;
      pdec[i].path = (char *) (pathtable + strofs);
      ph = phdr->nlength;

      if (!joliet)
        {
          for (j = 0; j < ph; j++)
            pathtable[strofs++] = pname[j];
        }
      else
        {
          for (j = 0; j < ph >> 1; j++)
            pathtable[strofs++] = pname[(j << 1) + 1];
        }

      pathtable[strofs++] = 0;

      ofs += pos;
      phdr = (PATHHDR *) (pathtable + ofs);

    }

}

/****************************************************************************
* FindDirectory
****************************************************************************/
static int
FindDirectory (char *dir)
{
  char work[2048];
  char *p;
  int parent = 1;
  int i, found;
  PATHDECODE *pdec = (PATHDECODE *) pathdecs;

  strcpy (work, dir);

  p = strtok (work, "/");
  if (p == NULL)
    return -1;

  while (p != NULL)
    {
      found = 0;

      for (i = 0; i < pathentries; i++)
        {
          if (pdec[i].parent == parent)
            {
              if (stricmp (p, pdec[i].path) == 0)
                {
                  parent = i + 1;
                  found = 1;
                  break;
                }
            }
        }

      if (found)
        p = strtok (NULL, "/");
      else
        return -1;
    }

  return parent;

}

/****************************************************************************
* GetSubdirectories
****************************************************************************/
void
GetSubDirectories (char *dir, char *buf, int len)
{
  int parent;
  int i;
  int count = 0;
  int ofs = 4;

  PATHDECODE *pdec = (PATHDECODE *) pathdecs;

  memset (buf, 0, len);

  if (stricmp (dir, "/") == 0)
    parent = 1;
  else
    parent = FindDirectory (dir);

  if (parent < 0)
    return;

  /*** Look for children of this directory ***/
  for (i = 0; i < pathentries; i++)
    {
      if ((pdec[i].parent == parent) && (strlen (pdec[i].path)))
        {
          memcpy (buf + ofs, &pdec[i].path, 4);
          ofs += 4;
          if (ofs > len)
            break;
#if 0
          strcat (buf, pdec[i].path);
          strcat (buf, "|");
#endif
          count++;
        }
    }

  memcpy (buf, &count, 4);

}

/****************************************************************************
* Search Cache
****************************************************************************/
static int
SearchCache (char *filename)
{
  int i;

  for (i = 0; i < MAXCACHED; i++)
    {
      if (stricmp (cachedfiles[i].fname, filename) == 0)
        return i;
    }

  return -1;
}

/****************************************************************************
* FindFile
*
* Attempt to locate a file and return it's structure
****************************************************************************/
DIRENTRY *
FindFile (char *filename)
{
  int d, i;
  char work[1024];
  char dir[1024];
  char file[1024];
  char cfile[1024];
  char *pname;
  int ofs = 0;
  PATHDECODE *pdec = (PATHDECODE *) pathdecs;
  DIRENTRY *fdir = (DIRENTRY *) dvdbuffer;

  static long long int prevoffset64 = 0;
  long long int dvdoffset64 = 0;

  /*** Search cache ***/
  d = SearchCache (filename);
  if (d >= 0)
    {
      return (DIRENTRY *) & cachedfiles[d].fdir;
    }

  strcpy (work, filename);

  /*** Find the directory ***/
  for (i = strlen (work) - 1; i > 0; i--)
    {
      if (work[i] == '/')
        {
          work[i] = 0;
          strcpy (dir, work);
          strcpy (file, &work[i + 1]);
          break;
        }
    }

  d = FindDirectory (dir);

  if (d >= 0)
    {
      /*** Ok - now have the directory base
             Go find the file ***/

      if (prevoffset64 != pdec[d - 1].offset64)
        {
          memset (dvdbuffer, 0, SECTORSIZE);
          if( !gcsim64 (dvdbuffer, pdec[d - 1].offset64, SECTORSIZE) ) return NULL;
          prevoffset64 = pdec[d - 1].offset64;
        }

      while (fdir->nlength)
        {
          if (!(fdir->flags & ISDIR))
            {
              pname = (char *) (dvdbuffer + ofs + sizeof (DIRENTRY));

              if (!joliet)
                {
                  for (i = 0; i < fdir->identifier; i++)
                    {
                      if (pname[i] == ';')
                        break;

                      cfile[i] = pname[i];
                    }
                }
              else
                {
                  for (i = 0; i < fdir->identifier >> 1; i++)
                    {
                      if (pname[(i << 1) + 1] == ';')
                        break;

                      cfile[i] = pname[(i << 1) + 1];
                    }
                }

              cfile[i] = 0;

              if (stricmp (cfile, file) == 0)
                {
                  strcpy (cachedfiles[cached].fname, filename);
                  memcpy (&cachedfiles[cached].fdir, fdir, sizeof (DIRENTRY));
                  cached++;
                  if (cached >= MAXCACHED)
                    cached = 0;

                  return fdir;
                }
            }

          ofs += fdir->nlength;
          fdir = (DIRENTRY *) (dvdbuffer + ofs);

          if (fdir->nlength == 0)
            {
              /*** Read next sector ***/
              dvdoffset64 += SECTORSIZE;
              if( !gcsim64 (dvdbuffer, pdec[d-1].offset64 + dvdoffset64, SECTORSIZE ) ) return NULL;
              ofs = 0;

              prevoffset64 = -1;

              fdir = (DIRENTRY *) (dvdbuffer + ofs);
              if ((fdir->identifier == 1) && (fdir->flags & ISDIR))
                {
                  pname = (char *) (dvdbuffer + ofs + sizeof (DIRENTRY));
                  if (pname[0] == 0)
                    {
                      return NULL;
                    }
                }
            }
        }
    }

  return NULL;

}
