/****************************************************************************
*
*  2610intf.c
*
*  The YM2610 emulator supports up to 2 chips.
*  Each chip has the following connections:
*  - Status Read / Control Write A
*  - Port Read / Data Write A
*  - Control Write B
*  - Data Write B
*
****************************************************************************/

#include <stdio.h>
#include "neocdrx.h"
#include "ay8910.h"
#include "fm.h"
#include "2610intf.h"
#include "timer.h"
#include "streams.h"

/* use FM.C with stream system */

static int stream;
static timer_struct *Timer[2];

/*------------------------- TM2610 -------------------------------*/
/* IRQ Handler */
static void IRQHandler(int n, int irq)
{
    //printf("IRQ!!!\n");
    neogeo_sound_irq(irq);
}

/* Timer overflow callback from timer.c */
void timer_callback_2610(int param)
{
    int c = param;

    Timer[c] = 0;
    YM2610TimerOver(c);
}

/* TimerHandler from fm.c */
static void TimerHandler(int c, int count, double stepTime)
{

    if (count == 0) {		/* Reset FM Timer */
	if (Timer[c]) {
	    del_timer(Timer[c]);
	    Timer[c] = 0;
	}
    } else {			/* Start FM Timer */
	double timeSec = (double) count * stepTime;

	if (Timer[c] == 0) {
	    Timer[c] =
		(timer_struct *) insert_timer(timeSec, c,
					      timer_callback_2610);
	}
    }
}

void FMTimerInit(void)
{
    Timer[0] = Timer[1] = 0;
    free_all_timer();
}

/* update request from fm.c */
void YM2610UpdateRequest(void)
{
    static double old_tc;
    double tc = timer_count - old_tc;
    int len = (int) (SAMPLE_RATE * tc) << 2;
    if (len > 4) {
	old_tc = timer_count;
	streamupdate(len);
    }
}

int YM2610_sh_start(void)
{
    int j;
    int rate = SAMPLE_RATE;
    char buf[YM2610_NUMBUF][40];
    void *pcmbufa, *pcmbufb;
    int pcmsizea, pcmsizeb;

    if (AY8910_sh_start())
	return 1;

    /* Timer Handler set */
    FMTimerInit();
    for (j = 0; j < YM2610_NUMBUF; j++) {
	buf[j][0] = 0;
    }
    if (buf == NULL) { }

    stream = stream_init_multi(YM2610_NUMBUF, 0, YM2610UpdateOne);
    pcmbufa = (void *) neogeo_pcm_memory;
    pcmsizea = 0x100000;	//1048576;
    pcmbufb = NULL;		/*(void *) memory.sound2; */
    pcmsizeb = 0;		/*memory.sound2_size; */

    //}

  /**** initialize YM2610 ****/
    /*
     * if (YM2610Init(8000000,rate,
     * pcmbufa,pcmsizea,pcmbufb,pcmsizeb,
     * TimerHandler,IRQHandler) == 0)
     */
    if (YM2610Init(8000000, rate,
		   pcmbufa, pcmsizea, pcmbufb, pcmsizeb,
		   TimerHandler, IRQHandler) == 0)
	return 0;

    /* error */
    return 1;
}

/************************************************/
/* Sound Hardware Stop				*/
/************************************************/
void YM2610_sh_stop(void)
{
    YM2610Shutdown();
}

/* reset */
void YM2610_sh_reset(void)
{
    YM2610ResetChip();
}

/************************************************/
/* Status Read for YM2610 - Chip 0		*/
/************************************************/
Uint32 YM2610_status_port_0_A_r(Uint32 offset)
{
    return YM2610Read(0);
}

Uint32 YM2610_status_port_0_B_r(Uint32 offset)
{
    return YM2610Read(2);
}

/************************************************/
/* Port Read for YM2610 - Chip 0		*/
/************************************************/
Uint32 YM2610_read_port_0_r(Uint32 offset)
{
    return YM2610Read(1);
}


/************************************************/
/* Control Write for YM2610 - Chip 0		*/
/* Consists of 2 addresses			*/
/************************************************/
void YM2610_control_port_0_A_w(Uint32 offset, Uint32 data)
{
    YM2610Write(0, data);
}

void YM2610_control_port_0_B_w(Uint32 offset, Uint32 data)
{
    YM2610Write(2, data);
}

/************************************************/
/* Data Write for YM2610 - Chip 0		*/
/* Consists of 2 addresses			*/
/************************************************/
void YM2610_data_port_0_A_w(Uint32 offset, Uint32 data)
{
    YM2610Write(1, data);
}

void YM2610_data_port_0_B_w(Uint32 offset, Uint32 data)
{
    YM2610Write(3, data);
}
