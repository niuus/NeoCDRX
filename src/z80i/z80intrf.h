/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/************************
*** Z80 CPU Interface ***
***    Header File    ***
************************/

#ifndef Z80INTRF_H
#define Z80INRTF_H

#include "z80.h"

void z80_init (void);
int mz80exec (int cycles);
void mz80nmi (void);
void mz80int (int irq);
void z80_exit (void);

extern UINT8 subcpu_memspace[65536];
extern int sound_code;
extern int pending_command;
extern int result_code;
extern int z80_cycles;

#endif /* Z80INTRF_H */
