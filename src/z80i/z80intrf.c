/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/************************
*** Z80 CPU Interface ***
************************/

//-- Include Files ----------------------------------------------------------
#include <gccore.h>
#include "neocdrx.h"
#include <stdio.h>
#include <stdlib.h>

//-- Exported Functions -----------------------------------------------------
unsigned char subcpu_memspace[65536];
int sound_code = 0;
int pending_command = 0;
int result_code = 0;
int nmi_over = 0;

//---------------------------------------------------------------------------
/*** 
* Whilst tracing the Z80, it appears that PORT0/8/C are always written
* with the same value after NMI.
*
* At this point, the M68K reads from address 0x32000 to determine the
* success or failure of the previous NMI.
*
* On NMI, sound_code == 3 is total reset. 
* sound_code == 1 is a partial reset.
* 
* So really, it should not be called sound_code, as it does this
* very important task too.
***/

void
PortWrite (UINT16 PortNo, UINT8 data)
{

  switch (PortNo & 0xff)
    {

    case 0x4:
      YM2610_control_port_0_A_w (0, data);
      break;

    case 0x5:
      YM2610_data_port_0_A_w (0, data);
      break;

    case 0x6:
      YM2610_control_port_0_B_w (0, data);
      break;

    case 0x7:
      YM2610_data_port_0_B_w (0, data);
      break;

    case 0x8:
      /* NMI enable / acknowledge? (the data written doesn't matter) */
      break;

    case 0xc:
      result_code = data;
      break;

    case 0x18:
      /* NMI disable? (the data written doesn't matter) */
      break;

    case 0x80:
      cdda_stop ();
      break;

    default:
      //printf("Unimplemented Z80 Write Port: %x data: %x\n",PortNo&0xff,data);
      break;
    }
}

//---------------------------------------------------------------------------
UINT8
PortRead (UINT16 PortNo)
{
  switch (PortNo & 0xff)
    {
    case 0x0:
      pending_command = 0;
      return sound_code;
      break;

    case 0x4:
      return YM2610_status_port_0_A_r (0);
      break;

    case 0x5:
      return YM2610_read_port_0_r (0);
      break;

    case 0x6:
      return YM2610_status_port_0_B_r (0);
      break;

    default:
      //printf("Unimplemented Z80 Read Port: %d\n",PortNo&0xff);
      break;
    };
  return 0;
}
