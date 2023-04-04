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
#include "icon.h"

#define MC_SIZE 0x4000
#define MC_MAKER "NCRX"
#define MC_CODE  "00"
#define MC_FILENAME "NeoCDRX.sav"
#define MC_COMMENT1 "NeoCD RX 1.5"
#define MC_COMMENT2 "CD Memory Card Save"

static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);
static u8 mcardbuffer[MC_SIZE] ATTRIBUTE_ALIGN (32);
static card_dir CardDir;
static card_file CardFile;
static card_stat CardStatus;
static int cardslot;

/****************************************************************************
* MountMemCard
*
* Try to mount a memory card
****************************************************************************/
static int
MountMemCard (void)
{
  int CardError;
  int retries = 0;

  memset (SysArea, 0, CARD_WORKAREA);

  while (retries < 10)
    {
	  CardError = CARD_Mount (CARD_SLOTA, SysArea, NULL);
	  if (CardError)
	    {
	      CardError = CARD_Mount (CARD_SLOTB, SysArea, NULL);
	      if (!CardError)
			cardslot = CARD_SLOTB;
	    }
	  else
	    cardslot = CARD_SLOTA;

    if (CardError)
	{
	  retries++;
	}
    else
	  retries = 10;
    }

  return (CardError == 0);
}

/****************************************************************************
* CardFileExists
*
* Look for an existing memory card save
****************************************************************************/
static int
CardFileExists (char *filename)
{
  int CardError;

  memset (&CardDir, 0, sizeof (card_dir));

  CardError = CARD_FindFirst (cardslot, &CardDir, TRUE);
  while (CardError != CARD_ERROR_NOFILE)
    {
      if (strcmp ((char *) CardDir.filename, filename) == 0)
	return 1;

      CardError = CARD_FindNext (&CardDir);
    }

  return 0;
}

/****************************************************************************
* neogeo_get_memorycard
*
* Function to retrieve or create a memory card save
****************************************************************************/
int
neogeo_get_memorycard (void)
{
    
if (SaveDevice == 0)
{
  int carderror;
  int blocks;
  u32 sectorsize;
  int ofs;
  char mcfilename[] = MC_FILENAME;

	/*** Initialise the card lib ***/
  CARD_Init (MC_MAKER, MC_CODE);

  if (MountMemCard ())
    {
		/*** Get sector size ***/
      carderror = CARD_GetSectorSize (cardslot, &sectorsize);
      blocks = (MC_SIZE / sectorsize) * sectorsize;
      if (MC_SIZE % sectorsize)
	blocks += sectorsize;

      if (CardFileExists (mcfilename))
	{
	  carderror = CARD_Open (cardslot, MC_FILENAME, &CardFile);
	  if (carderror)
	    {
	      CARD_Unmount (cardslot);
	      return 0;
	    }

			/*** Read in the data ***/
	  ofs = 0;
	  DCInvalidateRange (mcardbuffer, MC_SIZE);
	  while (ofs < blocks)
	    {
	      CARD_Read (&CardFile, mcardbuffer + ofs, sectorsize, ofs);
	      ofs += sectorsize;
	    }

	  memcpy (neogeo_memorycard, mcardbuffer + 2112, 8192);
	  CARD_Close (&CardFile);
	  CARD_Unmount (cardslot);

	  return 1;
	}
      else
	{
	  CARD_Unmount (cardslot);
	  return neogeo_set_memorycard ();
	}
    }
}
else //SD Card
{
  FILE *fp;
  int Readed;

  fp = fopen("/NeoCDRX/NeoCDRXsave.bin", "rb");
  if (!fp)
    {
      // file does not exist, try to create it
      neogeo_set_memorycard();

      return 0;
    }

    Readed = fread(neogeo_memorycard, 1, 8192, fp);

    fclose(fp);

    if (Readed == 8192)
      {
	return 1;
      }
}

  return 0;
}

/****************************************************************************
* neogeo_set_memorycard
****************************************************************************/
int
neogeo_set_memorycard (void)
{
if (SaveDevice == 0)
{
  int carderror;
  int blocks;
  u32 sectorsize;
  int ofs;
  char mcfilename[] = MC_FILENAME;

  CARD_Init (MC_MAKER, MC_CODE);

  if (MountMemCard ())
    {
      carderror = CARD_GetSectorSize (cardslot, &sectorsize);
      blocks = (MC_SIZE / sectorsize) * sectorsize;
      if (blocks % sectorsize)
	blocks += sectorsize;

      if (CardFileExists (mcfilename))
	{
	  carderror = CARD_Open (cardslot, MC_FILENAME, &CardFile);
	  if (carderror)
	    {
	      CARD_Unmount (cardslot);
	      return 0;
	    }
	}
      else
	{
	  carderror = CARD_Create (cardslot, MC_FILENAME, blocks, &CardFile);
	  if (carderror)
	    {
	      CARD_Unmount (cardslot);
	      return 0;
	    }
	}

      memset (mcardbuffer, 0, MC_SIZE);

		/*** 1. Copy the icon ***/
      memcpy (mcardbuffer, saveicon, 2048);

		/*** 2. Set comments - 32 bytes each***/
      memcpy (mcardbuffer + 2048, MC_COMMENT1, strlen (MC_COMMENT1));
      memcpy (mcardbuffer + 2080, MC_COMMENT2, strlen (MC_COMMENT2));

		/*** 3. Copy neogeo_memorycard ***/
      memcpy (mcardbuffer + 2112, neogeo_memorycard, 8192);

		/*** Set defaults in card status ***/
      CARD_GetStatus (cardslot, CardFile.filenum, &CardStatus);
      CardStatus.icon_addr = 0;
      CardStatus.icon_fmt = 2;
      CardStatus.icon_speed = 1;
      CardStatus.comment_addr = 2048;
      CARD_SetStatus (cardslot, CardFile.filenum, &CardStatus);

		/*** Write the file ***/
      ofs = 0;
      DCFlushRange (mcardbuffer, MC_SIZE);
      while (ofs < blocks)
	{
	  carderror =
	    CARD_Write (&CardFile, mcardbuffer + ofs, sectorsize, ofs);
	  ofs += sectorsize;
	}

      CARD_Close (&CardFile);
      CARD_Unmount (cardslot);

      return 1;
    }
}
else //SD Card
{

  FILE *fp;
  int Written;

  fp = fopen("/NeoCDRX/NeoCDRXsave.bin", "wb");
  if (!fp)
    {
      return 0;
    }

    Written = fwrite(neogeo_memorycard, 1, 8192, fp);

    fclose(fp);

    if (Written == 8192)
      {
	return 1;
      }
}
  return 0;
}
