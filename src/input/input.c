/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

#include "neocdrx.h"
#include <math.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif


#define P1UP    0x00000001
#define P1DOWN  0x00000002
#define P1LEFT  0x00000004
#define P1RIGHT 0x00000008
#define P1A     0x00000010
#define P1B     0x00000020
#define P1C     0x00000040
#define P1D     0x00000080

#define P2UP    0x00000100
#define P2DOWN  0x00000200
#define P2LEFT  0x00000400
#define P2RIGHT 0x00000800
#define P2A     0x00001000
#define P2B     0x00002000
#define P2C     0x00004000
#define P2D     0x00008000

#define P1START 0x00010000
#define P1SEL   0x00020000
#define P2START 0x00040000
#define P2SEL   0x00080000

#define SPECIAL 0x01000000
static u32 keys = 0;
static int padcal = 80;
int accept_input = 0;

static unsigned int neopadmap[] =
  { P1UP,
	P1DOWN,
	P1LEFT,
	P1RIGHT,
	P1B,
	P1A,
	P1D,
	P1C
};

unsigned short gcpadmap[] =
  { PAD_BUTTON_UP,
    PAD_BUTTON_DOWN,
    PAD_BUTTON_LEFT,
    PAD_BUTTON_RIGHT,
    PAD_BUTTON_A,		// A
    PAD_BUTTON_B,		// B
    PAD_BUTTON_X,		// D
    PAD_BUTTON_Y		// C
};

#ifdef HW_RVL

#define MAX_HELD_CNT 15
static u32 held_cnt = 0;

static u32 wpadmap[3][8] =
{
  {
    WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_2, WPAD_BUTTON_1,
    WPAD_BUTTON_A, WPAD_BUTTON_B
  },
  {
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
    WPAD_BUTTON_B, WPAD_BUTTON_A,
    WPAD_BUTTON_1, WPAD_BUTTON_PLUS
  },
  {
    WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
    WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT,
    WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B,
    WPAD_CLASSIC_BUTTON_X, WPAD_CLASSIC_BUTTON_Y
  }
};

#define PI 3.14159265f

static s8 WPAD_StickX(int chan, int right)
{
  float mag = 0.0;
  float ang = 0.0;

  WPADData *data = WPAD_Data(chan);
  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* Calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * sin(M_PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}


static s8 WPAD_StickY(int chan, int right)
{
  float mag = 0.0;
  float ang = 0.0;

  WPADData *data = WPAD_Data(chan);

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* Calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * cos(M_PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}
#endif

u16 getMenuButtons(void)
{
  
  /* Slowdown input updates */
  VIDEO_WaitVSync();
  
  /* Get gamepad inputs */
  PAD_ScanPads();
  u16 p = PAD_ButtonsDown(0);
  s8 x  = PAD_StickX(0);
  s8 y  = PAD_StickY(0);
  if (x > 70) p |= PAD_BUTTON_RIGHT;
  else if (x < -70) p |= PAD_BUTTON_LEFT;
  if (y > 60) p |= PAD_BUTTON_UP;
  else if (y < -60) p |= PAD_BUTTON_DOWN;

#ifdef HW_RVL
  /* Update WPAD status */
  WPAD_ScanPads();
  u32 q = WPAD_ButtonsDown(0);
  u32 h = WPAD_ButtonsHeld(0);
  x = WPAD_StickX(0, 0);
  y = WPAD_StickY(0, 0);
  
     /* Is Wiimote directed toward screen? (horizontal/vertical orientation) */
     struct ir_t ir;
     WPAD_IR(0, &ir);

     /* Wiimote directions */
     if (q & WPAD_BUTTON_UP)         p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
     else if (q & WPAD_BUTTON_DOWN)  p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
     else if (q & WPAD_BUTTON_LEFT)  p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
     else if (q & WPAD_BUTTON_RIGHT) p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;


     if (h & WPAD_BUTTON_UP)
     {
        held_cnt ++;
        if (held_cnt == MAX_HELD_CNT)
        {
           held_cnt = MAX_HELD_CNT - 2;
           p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
        }
     }
     else if (h & WPAD_BUTTON_DOWN)
     {
        held_cnt ++;
        if (held_cnt == MAX_HELD_CNT)
        {
           held_cnt = MAX_HELD_CNT - 2;
           p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
        }
     }
     else if (h & WPAD_BUTTON_LEFT)
     {
        held_cnt ++;
        if (held_cnt == MAX_HELD_CNT)
        {
           held_cnt = MAX_HELD_CNT - 2;
           p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
        }
     }
     else if (h & WPAD_BUTTON_RIGHT)
     {
        held_cnt ++;
        if (held_cnt == MAX_HELD_CNT)
        {
           held_cnt = MAX_HELD_CNT - 2;
           p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;
        }
     }
     else held_cnt = 0;


     /* Analog sticks */
     if (y > 70)       p |= PAD_BUTTON_UP;
     else if (y < -70) p |= PAD_BUTTON_DOWN;
     if (x < -60)      p |= PAD_BUTTON_LEFT;
     else if (x > 60)  p |= PAD_BUTTON_RIGHT;

     /* Wii Classic Controller directions */
     if (q & WPAD_CLASSIC_BUTTON_UP)         p |= PAD_BUTTON_UP;
     else if (q & WPAD_CLASSIC_BUTTON_DOWN)  p |= PAD_BUTTON_DOWN;
     if (q & WPAD_CLASSIC_BUTTON_LEFT)       p |= PAD_BUTTON_LEFT;
     else if (q & WPAD_CLASSIC_BUTTON_RIGHT) p |= PAD_BUTTON_RIGHT;

     /* Wiimote keys */
     if (q & WPAD_BUTTON_MINUS)  p |= PAD_TRIGGER_L;
     if (q & WPAD_BUTTON_PLUS)   p |= PAD_TRIGGER_R;
     if (q & WPAD_BUTTON_A)      p |= PAD_BUTTON_X;
     if (q & WPAD_BUTTON_2)      p |= PAD_BUTTON_A;
     if (q & WPAD_BUTTON_1)      p |= PAD_BUTTON_B;
     if (q & WPAD_BUTTON_HOME)   p |= PAD_TRIGGER_Z;
  
     /* Wii Classic Controller keys */
     if (q & WPAD_CLASSIC_BUTTON_FULL_L) p |= PAD_TRIGGER_L;
     if (q & WPAD_CLASSIC_BUTTON_FULL_R) p |= PAD_TRIGGER_R;
     if (q & WPAD_CLASSIC_BUTTON_X)      p |= PAD_BUTTON_X;
     if (q & WPAD_CLASSIC_BUTTON_A)      p |= PAD_BUTTON_A;
     if (q & WPAD_CLASSIC_BUTTON_B)      p |= PAD_BUTTON_B;
     if (q & WPAD_CLASSIC_BUTTON_HOME)   p |= PAD_TRIGGER_Z;
#endif
  return p;
}


#ifdef HW_RVL
/****************************************************************************
 * DecodeJoy WII
 ****************************************************************************/
unsigned int
DecodeJoyWii (unsigned short p)
{
  unsigned int J = 0;
  int i;

  u32 exp;
  if (WPAD_Probe(0, &exp) == WPAD_ERR_NONE)
  {
     for (i = 0; i < 8; i++)
     {
        if (p & wpadmap[exp][i])
        J |= neopadmap[i];
     }
  }
  return J;
}
#endif

/****************************************************************************
 * DecodeJoy
 ****************************************************************************/
static unsigned int
DecodeJoy (unsigned short p)
{
  unsigned int J = 0;
  int i;

  for (i = 0; i < 8; i++)
	{
	  if (p & gcpadmap[i])
        J |= neopadmap[i];
	}
  return J;
}

/****************************************************************************
 * GetAnalog
 ****************************************************************************/
static unsigned int
GetAnalog (int Joy)
{
  float t;
  unsigned int i = 0;
  s8 x = PAD_StickX (Joy);
  s8 y = PAD_StickY (Joy);

#ifdef HW_RVL
  x +=WPAD_StickX(Joy,0);
  y +=WPAD_StickY(Joy,0);
#endif

  if ((x * x + y * y) > (padcal * padcal))
    {
      if (x > 0 && y == 0)
	i |= P1RIGHT;
      if (x < 0 && y == 0)
	i |= P1LEFT;
      if (x == 0 && y > 0)
	i |= P1UP;
      if (x == 0 && y < 0)
	i |= P1DOWN;

      if (x != 0 && y != 0)
	{

      /*** Recalc left / right ***/
	t = (float) y / x;
	if (t >= -2.41421356237 && t < 2.41421356237)
	  {
		if (x >= 0)
	i |= P1RIGHT;
		else
	i |= P1LEFT;
	  }

	/*** Recalc up / down ***/
	t = (float) x / y;
	if (t >= -2.41421356237 && t < 2.41421356237)
	  {
		if (y >= 0)
	i |= P1UP;
		else
	i |= P1DOWN;
	  }
	}
	}

  return i;
}

/****************************************************************************
 * StartSel
 ****************************************************************************/
static unsigned int
startsel (unsigned short p)
{
  int J = 0;
  if (p & PAD_BUTTON_START)	// Button START
    J |= 1;
  if (p & PAD_TRIGGER_Z)	// Button SELECT or PAUSE
    J |= 2;

#ifdef HW_RVL
  if ((p & WPAD_BUTTON_PLUS) || (p & WPAD_CLASSIC_BUTTON_PLUS))    // Button START
    J |= 1;
  if ((p & WPAD_BUTTON_MINUS) || (p & WPAD_CLASSIC_BUTTON_MINUS))  // Button SELECT or PAUSE
    J |= 2;
#endif

  return J;
}

/****************************************************************************
 * update_input
 ****************************************************************************/
void
update_input (void)
{
  unsigned short p;
  unsigned int t;

  if (!accept_input)
    return;

	// Player One
  p = PAD_ButtonsHeld (0);
  
	//  GO BACK TO MENU
  if (p & PAD_TRIGGER_L) neogeo_new_game ();

	//  MEMORY CARD SAVE
  if (p & PAD_TRIGGER_R){
     if (mcard_written) {
        if (neogeo_set_memorycard ()) mcard_written = 0;
     }
  }

#ifdef HW_RVL
  u32 exp;

  // Update WPAD status
  WPAD_ScanPads();
  if (WPAD_Probe(0, &exp) == WPAD_ERR_NONE)
  {
	p = WPAD_ButtonsHeld(0);

	//  GO BACK TO MENU
	if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
		neogeo_new_game ();

	//  MEMORY CARD SAVE 
	if (((p & WPAD_BUTTON_PLUS) && (p & WPAD_BUTTON_MINUS)) || (p & WPAD_CLASSIC_BUTTON_FULL_R))
	{
	if (mcard_written)
		{
			if (neogeo_set_memorycard ())
				mcard_written = 0;
		}
	}

	// PAD Buttons
	// if (P1_controller == 1)
        keys = DecodeJoyWii (p);
  }
  else keys = DecodeJoy (p);
#else
  keys = DecodeJoy (p);
#endif

  keys |= GetAnalog (0);
  t = startsel (p);
  keys |= (t << 16);


    // Player Two
  p = PAD_ButtonsHeld (1);

	//  GO BACK TO MENU
	if (p & PAD_TRIGGER_L)
		neogeo_new_game ();

	//  MEMORY CARD SAVE
	if (p & PAD_TRIGGER_R)
	{
    if (mcard_written)
		{
			if (neogeo_set_memorycard ())
				mcard_written = 0;
		}
	}

#ifdef HW_RVL
  if (WPAD_Probe(1, &exp) == WPAD_ERR_NONE)
  {
     p = WPAD_ButtonsHeld(1);
     
	//  GO BACK TO MENU
	if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
		neogeo_new_game ();

	//  MEMORY CARD SAVE
	if (((p & WPAD_BUTTON_PLUS) && (p & WPAD_BUTTON_MINUS)) || (p & WPAD_CLASSIC_BUTTON_FULL_R))
	{
		if (mcard_written)
		{
			if (neogeo_set_memorycard ())
				mcard_written = 0;
		}
	}

     keys |= (DecodeJoyWii (p) << 8);
  }
  else keys |= (DecodeJoy (p) << 8);
#else
  keys |= (DecodeJoy (p) << 8);
#endif 
  keys |= (GetAnalog (1) << 8);
  t = startsel (p);
  keys |= (t << 18);
}

/*--------------------------------------------------------------------------*/
unsigned char
read_player1 (void)
{
  return ~keys & 0xff;
}

/*--------------------------------------------------------------------------*/
unsigned char
read_player2 (void)
{
  return ~(keys >> 8) & 0xff;
}

/*--------------------------------------------------------------------------*/
unsigned char
read_pl12_startsel (void)
{
  return ~(keys >> 16) & 0x0f;
}
