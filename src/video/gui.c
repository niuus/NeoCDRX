/****************************************************************************
* NeoCDRX
*
* GUI File Selector
****************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
#include "neocdrx.h"
#include "backdrop.h"
#include "banner.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#endif

/*** GC 2D Video ***/
extern unsigned int *xfb[2];
extern int whichfb;
extern GXRModeObj *vmode;

/*** libOGC Default Font ***/
extern u8 console_font_8x16[];

static u32 fgcolour = COLOR_WHITE;
static u32 bgcolour = COLOR_BLACK;
static unsigned char background[1280 * 480] ATTRIBUTE_ALIGN (32);
static unsigned char bannerunc[banner_WIDTH * banner_HEIGHT * 2] ATTRIBUTE_ALIGN (32);
static void unpack (void);

unsigned short SaveDevice = 1;           // default save location to SD card
int use_SD  = 0;
int use_USB = 0;
int use_IDE = 0;
int use_WKF = 0;
int use_DVD = 0;

int mega = 0;

/****************************************************************************
* plotpixel
****************************************************************************/
static void
plotpixel (int x, int y)
{
  u32 pixel;

  pixel = xfb[whichfb][(y * 320) + (x >> 1)];

  if (x & 1)
    xfb[whichfb][(y * 320) + (x >> 1)] =
      (pixel & 0xffff00ff) | (COLOR_WHITE & 0xff00);
  else
    xfb[whichfb][(y * 320) + (x >> 1)] =
      (COLOR_WHITE & 0xffff00ff) | (pixel & 0xff00);
}

/****************************************************************************
* roughcircle
****************************************************************************/
static void
roughcircle (int cx, int cy, int x, int y)
{
  if (x == 0)
    {
      plotpixel (cx, cy + y);			/*** Anti ***/
      plotpixel (cx, cy - y);
      plotpixel (cx + y, cy);
      plotpixel (cx - y, cy);
    }
  else
    {
      if (x == y)
	{
	  plotpixel (cx + x, cy + y);		/*** Anti ***/
	  plotpixel (cx - x, cy + y);
	  plotpixel (cx + x, cy - y);
	  plotpixel (cx - x, cy - y);
	}
  else
	{
	  if (x < y)
		{
		  plotpixel (cx + x, cy + y);	/*** Anti ***/
		  plotpixel (cx - x, cy + y);
		  plotpixel (cx + x, cy - y);
		  plotpixel (cx - x, cy - y);
		  plotpixel (cx + y, cy + x);
		  plotpixel (cx - y, cy + x);
		  plotpixel (cx + y, cy - x);
		  plotpixel (cx - y, cy - x);
		}

	}
    }
}

/****************************************************************************
* circle
****************************************************************************/
void
circle (int cx, int cy, int radius)
{
  int x = 0;
  int y = radius;
  int p = (5 - radius * 4) / 4;

  roughcircle (cx, cy, x, y);

  while (x < y)
    {
      x++;
      if (p < 0)
	p += (x << 1) + 1;
      else
	{
	  y--;
	  p += ((x - y) << 1) + 1;
	}
      roughcircle (cx, cy, x, y);
    }
}

/****************************************************************************
* drawchar
****************************************************************************/
static void
drawchar (int x, int y, char c)
{
  int yy, xx;
  u32 colour[2];
  int offset;
  u8 bits;

  offset = (y * 320) + (x >> 1);

  for (yy = 0; yy < 16; yy++)
    {
      bits = console_font_8x16[((c << 4) + yy)];

      for (xx = 0; xx < 4; xx++)
	{
	  if (bits & 0x80)
		colour[0] = fgcolour;
	  else
		colour[0] = bgcolour;

	  if (bits & 0x40)
		colour[1] = fgcolour;
	  else
		colour[1] = bgcolour;

	  xfb[whichfb][offset + xx] =
		(colour[0] & 0xffff00ff) | (colour[1] & 0xff00);

	  bits <<= 2;
	}

      offset += 320;
    }
}

/****************************************************************************
* drawcharw
****************************************************************************/
static void
drawcharw (int x, int y, char c)
{
  int yy, xx;
  int offset;
  int bits;

  offset = (y * 320) + (x >> 1);

  for (yy = 0; yy < 16; yy++)
    {
      bits = console_font_8x16[((c << 4) + yy)];

      for (xx = 0; xx < 8; xx++)
	{
	  if (bits & 0x80)
		xfb[whichfb][offset + xx] = xfb[whichfb][offset + 320 + xx] =
		  fgcolour;
	  else
		xfb[whichfb][offset + xx] = xfb[whichfb][offset + 320 + xx] =
		  bgcolour;

	  bits <<= 1;
	}

      offset += 640;
    }
}

/****************************************************************************
* gprint
****************************************************************************/
void
gprint (int x, int y, char *text, int mode)
{
  int n;
  int i;

  n = strlen (text);
  if (!n)
    return;

  if (mode != TXT_DOUBLE)
    {
      for (i = 0; i < n; i++, x += 8)
	  drawchar (x, y, text[i]);
    }
  else
    {
      for (i = 0; i < n; i++, x += 16)
	  drawcharw (x, y, text[i]);
    }
}

/****************************************************************************
* DrawScreen
****************************************************************************/
void
DrawScreen (void)
{
  static int inited = 0;

  if (!inited)
    {
      unpack ();
      inited = 1;
    }

  VIDEO_WaitVSync ();

  whichfb ^= 1;
  memcpy (xfb[whichfb], background, 1280 * 480);
}

/****************************************************************************
* ShowScreen
****************************************************************************/
void
ShowScreen (void)
{
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();
}

/****************************************************************************
* setfgcolour
****************************************************************************/
void
setfgcolour (u32 colour)
{
  fgcolour = colour;
}

/****************************************************************************
* setbgcolour
****************************************************************************/
void
setbgcolour (u32 colour)
{
  bgcolour = colour;
}

/****************************************************************************
* WaitButtonA
****************************************************************************/
void
WaitButtonA (void)
{
  short joy;
  joy = getMenuButtons();

  while (!(joy & PAD_BUTTON_A)) joy = getMenuButtons();
    VIDEO_WaitVSync ();

  while (joy & PAD_BUTTON_A) joy = getMenuButtons();
    VIDEO_WaitVSync ();
}

/****************************************************************************
* ActionScreen
****************************************************************************/
void
ActionScreen (char *msg)
{
  int n;
  char pressa[] = "Press A to continue";

  DrawScreen ();

  n = strlen (msg);
  fgcolour = COLOR_WHITE;
  bgcolour = BMPANE;

  gprint ((640 - (n * 16)) >> 1, 248, msg, TXT_DOUBLE);

  gprint (168, 288, pressa, TXT_DOUBLE);

  ShowScreen ();

  WaitButtonA ();
}

/****************************************************************************
* InfoScreen
****************************************************************************/
void
InfoScreen (char *msg)
{
  int n;

  DrawScreen ();

  n = strlen (msg);
  fgcolour = COLOR_WHITE;
  bgcolour = BMPANE;

  gprint ((640 - (n * 16)) >> 1, 264, msg, TXT_DOUBLE);

  ShowScreen ();
}

/****************************************************************************
* Credits
****************************************************************************/
int credits()
{
int quit = 0;
int ret = 0;
short joy;

char Title[]   = "CREDITS";
char Coder[]   = "Coding: NiuuS";
char Thanks[]  = "Thanks to:";
char Softdev[] = "Softdev and his Neo-CD Redux (GCN) (2007)"; 
char Coders1[] = "Wiimpathy / Jacobeian for NeoCD-Wii (2011)";
char Coders2[] = "infact for Neo-CD Redux (2011)";
char Coders3[] = "megalomaniac for Neo-CD Redux Unofficial (2013-2016)";
char Fun[]     = "GIGA POWER!";
char iosVersion[20];
char appVersion[20]= "NeoCD-RX v1.0.02";

#ifdef HW_RVL
	sprintf(iosVersion, "IOS : %d", IOS_GetVersion());
#endif

  DrawScreen ();
  
  fgcolour = COLOR_BLACK;
  bgcolour = BMPANE;

  gprint (250, 160, Title, TXT_DOUBLE);
  gprint (60, 210, Coder, 3);
  gprint (60, 250, Thanks, 3);
  gprint (60, 280, Softdev, 3);
  gprint (60, 300, Coders1, 3);
  gprint (60, 320, Coders2, 3);
  gprint (60, 340, Coders3, 3);
  gprint (60, 360, Fun, 3);
  gprint (510, 390, iosVersion, 0);
  gprint (60, 390, appVersion, 0);

  ShowScreen ();

  while (quit == 0)
  {
    joy = getMenuButtons();

    if (joy & PAD_BUTTON_A)
    { 
      quit = 1;
      ret = -1;
    }

    if (joy & PAD_BUTTON_B)
    {
      quit = 1;
      ret = -1;
    }
  }
  return ret;
}

/****************************************************************************
* unpack
****************************************************************************/
static void
unpack (void)
{
  unsigned long inbytes, outbytes;

  inbytes = backdrop_COMPRESSED;
  outbytes = backdrop_RAW;

  uncompress (background, &outbytes, backdrop, inbytes);

  inbytes = banner_COMPRESSED;
  outbytes = banner_RAW;

  uncompress (bannerunc, &outbytes, banner, inbytes);
}

/****************************************************************************
* LoadingScreen
****************************************************************************/
void
LoadingScreen (char *msg)
{
  int n;

  VIDEO_WaitVSync ();

  whichfb ^= 1;

  n = strlen (msg);
  fgcolour = COLOR_WHITE;
  bgcolour = COLOR_BLACK;

  gprint ((640 - (n * 16)) >> 1, 410, msg, TXT_DOUBLE);

  ShowScreen ();
}

/****************************************************************************
* draw_menu
****************************************************************************/
char menutitle[60] = { "" };
int menu = 0;

static void draw_menu(char items[][22], int maxitems, int selected)//(  int currsel )
{
   int i;
   int j;
   if (mega == 1)  j = 162; 
   else j = 225;
   int n;
   char msg[] = "";
   n = strlen (msg);


   DrawScreen ();
   
   for( i = 0; i < maxitems; i++ )
   {
      if ( i == selected )
      {
         setfgcolour (BMPANE);
         setbgcolour (INVTEXT);
         gprint( ( 640 - ( strlen(items[i]) << 4 )) >> 1, j, items[i], TXT_DOUBLE);
      }
      else
      {
         setfgcolour (COLOR_BLACK);
         setbgcolour (BMPANE);
         gprint( ( 640 - ( strlen(items[i]) << 4 )) >> 1, j, items[i], TXT_DOUBLE); 
      }
      j += 32;
   }
   
   if (mega == 0) {
      setfgcolour (COLOR_WHITE);
      setbgcolour (BMPANE);//COLOR_BLACK);
      gprint ((640 - (n * 16)) >> 1, 162/*432*/, msg, TXT_DOUBLE);
   }

   ShowScreen();
}

/****************************************************************************
* do_menu
****************************************************************************/
int DoMenu (char items[][22], int maxitems)
{
  int redraw = 1;
  int quit = 0;
  int ret = 0;
  short joy;

  while (quit == 0)
  {
    if (redraw)
    {
      draw_menu (&items[0], maxitems, menu);
      redraw = 0;
    }


    joy = getMenuButtons();

    if (joy & PAD_BUTTON_UP)
    {
      redraw = 1;
      menu--;
      if (menu < 0) menu = maxitems - 1;
    }

    if (joy & PAD_BUTTON_DOWN)
    {
      redraw = 1;
      menu++;
      if (menu == maxitems) menu = 0;
    }

    if (joy & PAD_BUTTON_A)
    {
      quit = 1;
      ret = menu;
    }

    if (joy & PAD_BUTTON_B)
    {
      quit = 1;
      ret = -1;
    }
  }
  return ret;
}

/****************************************************************************
* Save_menu - a friendly way to allow user to select location for RXsave.bin
****************************************************************************/
int ChooseMemCard (void)
{
  char titles[5][25] = { {"RXsave.bin not found\0"}, {"choose a save location\0"}, {"\0"}, {"A - SD Gecko   \0"}, {"B - Memory Card\0"} };
  int i;
  int quit = 0;
  short joy;
  
   DrawScreen ();

   fgcolour = COLOR_WHITE;
   bgcolour = BMPANE;

   for (i = 0; i < 5; i++) gprint ((640 - (strlen (titles[i]) * 16)) >> 1, 192 + (i * 32), titles[i], TXT_DOUBLE);

   ShowScreen ();

   while (!quit)
   {
      joy = getMenuButtons();
      if (joy & PAD_BUTTON_A)
        SaveDevice = 1;
        quit = 1;

      if (joy & PAD_BUTTON_B)
        SaveDevice = 0;
        quit = 1;
   }

  return 1;
}

/****************************************************************************
* Audio menu
****************************************************************************/

int audiomenu()
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  char buf[22];
  int count = 6;
  static char items[6][22] = {
    { "SFX Volume:       1.0" },
    { "MP3 Volume:       1.0" }, 
    { "Low Gain:         1.0" }, 
    { "Mid Gain:         1.0" },
    { "High Gain:        1.0" },
    { "Go Back" }

  };
  static float opts[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

  menu = 0;

  while (quit == 0)
  {
    sprintf(items[0],"SFX Volume       %1.1f",opts[0]);
    sprintf(items[1],"MP3 Volume       %1.1f",opts[1]);
    sprintf(items[2],"Low Gain         %1.1f",opts[2]);
    sprintf(items[3],"Mid Gain         %1.1f",opts[3]);
    sprintf(items[4],"High Gain        %1.1f",opts[4]);
    
    ret = DoMenu (&items[0], count);
    switch (ret)
    {
      case -1:
      case 5:
         quit = 1;
         break;
      default:
          opts[menu-0] += 0.1f;
          if ( opts[menu-0] > 2.0f ) opts[menu-0] = 1.0f;
          strcpy(buf, items[menu]);
          buf[18]=0;
          sprintf(items[menu],"%s%1.1f", buf, opts[menu-0]);
         break;
	}
  }
  mixer_set( opts[0], opts[1], opts[2], opts[3], opts[4]);
  menu = prevmenu;
  return 0;
}

/****************************************************************************
* Options menu
****************************************************************************/

int optionmenu()
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  //mega = 1;
  char buf[22];
  int count = 4;
  static char items[4][22] = 
  {
    { "Region:           USA" },
    { "Save Device:   SD/USB" },
    { "FX / Music Equalizer" },
    { "Go Back" }
  };

  menu = 0;

  while (quit == 0)
  {
    if (neogeo_region == 0) sprintf(items[0], "Region:         JAPAN");
      else if (neogeo_region == 1) sprintf(items[0], "Region:           USA");
      else sprintf(items[0], "Region:        EUROPE");

    if (SaveDevice == 1) sprintf(items[1], "Save Device:   SD/USB");
    else sprintf(items[1], "Save Device: MEM Card");

    ret = DoMenu (&items[0], count);
    switch (ret)
    {
      case 0:	// BIOS Region
         neogeo_region++;
         if ( neogeo_region > 2 ) neogeo_region = 0;
        break;

      case 1:	// Save Device location
        SaveDevice ^= 1;
        break;

      case 2:
        audiomenu();
        break;

      case -1:	// Go Back
      case 3:
         quit = 1;
         break;
	}
  }
  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Load menu
 *
 ****************************************************************************/
static u8 load_menu = 0;

int loadmenu ()
{
  int prevmenu = menu;
  int ret,count;
  int quit = 0;

#ifdef HW_RVL
  count = 6;
  char item[6][22] = {
    {"Load from SD"},
    {"Load from USB"},
    {"Load from IDE-EXI"},
    {"Load from DVD"},
    {"Stop DVD Motor"},
    {"Go Back"}
  };
#else
  count = 6;
  char item[6][22] = {
    {"Load from SD"},
    {"Load from IDE-EXI"},
    {"Load from WKF"},
    {"Load from DVD"},
    {"Stop DVD Motor"},
    {"Go Back"}
  };
#endif

  menu = load_menu;
  
  while (quit == 0)
  {
     use_SD  = 0;
     use_USB = 0;
     use_IDE = 0;
     use_WKF = 0;
     use_DVD = 0;
     ret = DoMenu (&item[0], count);
     switch (ret)
     {
        case -1:               // Button B - Exit
        case 5:
           quit = 1;
           break;

#ifdef HW_RVL
        case 1:                // Load from USB
           use_USB = 1;
           InfoScreen((char *) "Mounting media");
           SD_SetHandler();
           GEN_mount();
           if (have_ROM == 1) return 1;// quit = 1;
           break;

        case 2:                // Load from IDE-EXI
#else
        case 2:                // Load from WKF
           use_WKF = 1;
           InfoScreen((char *) "Mounting media");
           SD_SetHandler();
           GEN_mount();
           if (have_ROM == 1) return 1;// quit = 1;
           break;

        case 1:                // Load from IDE-EXI
#endif
           use_IDE = 1;
           InfoScreen((char *) "Mounting media");
           SD_SetHandler();
           GEN_mount();
           if (have_ROM == 1) return 1;// quit = 1;
           break;

        case 3:                // Load from DVD
           use_DVD = 1;
           InfoScreen((char *) "Mounting media");
           DVD_SetHandler();
           GEN_mount();  
           if (have_ROM == 1) return 1;// quit = 1;
           break;

        case 4:                // Stop DVD
           InfoScreen((char *) "Stopping DVD drive...");
           dvd_motor_off();
           break;

        default:               // Load from FAT device
           use_SD  = 1;
           InfoScreen((char *) "Mounting media");
           SD_SetHandler();
           GEN_mount();
           if (have_ROM == 1) return 1;
        break;

    }
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Main Menu
 *
 ****************************************************************************/

int load_mainmenu()
{
  s8 ret;
  u8 quit = 0;
  menu = 0;
#ifdef HW_RVL
  u8 count = 6;
  char items[6][22] =
#else
  u8 count = 6;
  char items[6][22] =
#endif
  {
    {"Play Game"},
    {"Reset Game"},
    {"Load New Game"},
    {"Settings"},
    {"Exit"},
	{"Credits"}
  };


  // Switch to menu default rendering mode (auto detect)
  VIDEO_Configure (vmode);
  VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  VIDEO_WaitVSync();



	while (quit == 0)
	{
		ret = DoMenu (&items[0], count);

	switch (ret)
	{
	  case -1:
      case  0: /*** Return to game ***/
        ret = 0;
        quit = 1;
        break;

      case 1:  /*** Reset game ***/
        neogeo_reset();
        YM2610_sh_reset();
        ret = 0;
        quit = 1;
        break;

      case 2:  /*** Load device menu ***/
        quit = loadmenu();
        break;

      case 3:  /*** Settings ***/
        optionmenu();
        break;

      case 4:  /*** Exit ***/
        VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        neogeocd_exit();
        break;

      case 5:  /*** Credits ***/
		credits();
        break;
	}
  }

  // Remove any still held buttons 
  while(PAD_ButtonsHeld(0)) PAD_ScanPads();
#ifdef HW_RVL
  while(WPAD_ButtonsHeld(0)) WPAD_ScanPads();
#endif

  return ret;
}

/****************************************************************************
* bannerscreen
****************************************************************************/
void
bannerscreen (void)
{
  int y, x, j;
  int offset;
  int *bb = (int *) bannerunc;

  whichfb ^= 1;
  offset = (200 * 320) + 40;
  VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);

  for (y = 0, j = 0; y < banner_HEIGHT; y++)
    {
      for (x = 0; x < (banner_WIDTH >> 1); x++)
		xfb[whichfb][offset + x] = bb[j++];

      offset += 320;
    }

  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();
}
