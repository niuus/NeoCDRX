/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

#ifndef __NEOINPUT__
#define __NEOINPUT__
void update_input (void);
unsigned char read_player1 (void);
unsigned char read_player2 (void);
unsigned char read_pl12_startsel (void);
extern int accept_input;
extern u16 getMenuButtons(void);
#endif
