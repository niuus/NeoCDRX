/*
 *	Emulation for the NEC PD4990A.
 *
 *	The PD4990A is a serial I/O Calendar & Clock IC used in the
 *		NEO GEO and probably a couple of other machines.
 */  
  
#include <gccore.h>
#include "neocdrx.h"
  
/* Set the data in the chip to Monday 09/09/73 00:00:00 	*/ 
/* If you ever read this Leejanne, you know what I mean :-) */ 
struct pd4990a_s pd4990a = 
{ 
0x00, /* seconds BCD */ 
    0x00, /* minutes BCD */ 
    0x00, /* hours   BCD */ 
    0x09, /* days    BCD */ 
    9, /* month   Hexadecimal form */ 
    0x73, /* year    BCD */ 
    1 /* weekday BCD */  
};


static int retraces = 0;	/* Assumes 60 retraces a second */

static int coinflip = 0;	/* Pulses a bit in order to simulate */

  /* test output */ 
static int outputbit = 0;

static int bitno = 0;


void
pd4990a_addretrace (void) 
{
  
coinflip ^= 1;
  
retraces++;
  
if (retraces < 60)
    
return;
  
retraces = 0;
  
pd4990a.seconds++;
  
if ((pd4990a.seconds & 0x0f) < 10)
    
return;
  
pd4990a.seconds &= 0xf0;
  
pd4990a.seconds += 0x10;
  
if (pd4990a.seconds < 0x60)
    
return;
  
pd4990a.seconds = 0;
  
pd4990a.minutes++;
  
if ((pd4990a.minutes & 0x0f) < 10)
    
return;
  
pd4990a.minutes &= 0xf0;
  
pd4990a.minutes += 0x10;
  
if (pd4990a.minutes < 0x60)
    
return;
  
pd4990a.minutes = 0;
  
pd4990a.hours++;
  
if ((pd4990a.hours & 0x0f) < 10)
    
return;
  
pd4990a.hours &= 0xf0;
  
pd4990a.hours += 0x10;
  
if (pd4990a.hours < 0x24)
    
return;
  
pd4990a.hours = 0;
  
pd4990a_increment_day ();

}


void
pd4990a_increment_day (void) 
{
  
int real_year;
  

pd4990a.days++;
  
if ((pd4990a.days & 0x0f) >= 10)
    
    {
      
pd4990a.days &= 0xf0;
      
pd4990a.days += 0x10;
    
}
  

pd4990a.weekday++;
  
if (pd4990a.weekday == 7)
    
pd4990a.weekday = 0;
  

switch (pd4990a.month)
    
    {
    
case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      
if (pd4990a.days == 0x32)
	
	{
	  
pd4990a.days = 1;
	  
pd4990a_increment_month ();
	
}
      
break;
    
case 2:
      
real_year = (pd4990a.year >> 4) * 10 + (pd4990a.year & 0xf);
      
if ((real_year % 4) && (!(real_year % 100) || (real_year % 400)))
	
	{
	  
if (pd4990a.days == 0x29)
	    
	    {
	      
pd4990a.days = 1;
	      
pd4990a_increment_month ();
	    
}
	
}
      
      else
	
	{
	  
if (pd4990a.days == 0x30)
	    
	    {
	      
pd4990a.days = 1;
	      
pd4990a_increment_month ();
	    
}
	
}
      
break;
    
case 4:
    case 6:
    case 9:
    case 11:
      
if (pd4990a.days == 0x31)
	
	{
	  
pd4990a.days = 1;
	  
pd4990a_increment_month ();
	
}
      
break;
    
}

}


void
pd4990a_increment_month (void) 
{
  
pd4990a.month++;
  
if (pd4990a.month == 13)
    
    {
      
pd4990a.month = 1;
      
pd4990a.year++;
      
if ((pd4990a.year & 0x0f) >= 10)
	
	{
	  
pd4990a.year &= 0xf0;
	  
pd4990a.year += 0x10;
	
}
      
if (pd4990a.year == 0xA0)
	
pd4990a.year = 0;
    
}

}



READ8_HANDLER (pd4990a_testbit_r) 
{
  
return (coinflip);

}



READ8_HANDLER (pd4990a_databit_r) 
{
  
return (outputbit);

}



WRITE8_HANDLER (pd4990a_control_w) 
{
  
switch (data)
    
    {
    
case 0x00:		/* Load Register */
      
switch (bitno)
	
	{
	
case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	
case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	  
outputbit = (pd4990a.seconds >> bitno) & 0x01;
	  
break;
	
case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	
case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
	  
outputbit = (pd4990a.minutes >> (bitno - 0x08)) & 0x01;
	  
break;
	
case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	
case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	  
outputbit = (pd4990a.hours >> (bitno - 0x10)) & 0x01;
	  
break;
	
case 0x18:
	case 0x19:
	case 0x1a:
	case 0x1b:
	
case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	  
outputbit = (pd4990a.days >> (bitno - 0x18)) & 0x01;
	  
break;
	
case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	  
outputbit = (pd4990a.weekday >> (bitno - 0x20)) & 0x01;
	  
break;
	
case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
	  
outputbit = (pd4990a.month >> (bitno - 0x24)) & 0x01;
	  
break;
	
case 0x28:
	case 0x29:
	case 0x2a:
	case 0x2b:
	
case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
	  
outputbit = (pd4990a.year >> (bitno - 0x28)) & 0x01;
	  
break;
	
}
      
break;
    

case 0x02:		/* shift one position */
      
bitno++;
      
break;
    

case 0x04:		/* Start afresh with shifting */
      
bitno = 0;
      
break;
    

default:			/* Unhandled value */
      
break;
    
}

}



WRITE16_HANDLER (pd4990a_control_16_w) 
{
  
pd4990a_control_w (offset, data & 0xff);

}


