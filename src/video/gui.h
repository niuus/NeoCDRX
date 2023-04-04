/****************************************************************************
* NeoCDRX
*
* GUI File Selector
****************************************************************************/

#ifndef __NEOGUI__
#define __NEOGUI__

#define TXT_NORMAL 0
#define TXT_INVERSE 1
#define TXT_DOUBLE 2

#define BGCOLOUR 0x74b3745a
#define BGBANNER 0x962a96ca
#define BGPANE 0x63b3635b
#define BMPANE 0xa685a67b
#define INVTEXT 0x628a6277

void gprint (int x, int y, char *text, int mode);
void ShowScreen (void);
void DrawScreen (void);
void setbgcolour (u32 colour);
void setfgcolour (u32 colour);
void WaitButtonA (void);
void ActionScreen (char *msg);
void InfoScreen (char *msg);
void LoadingScreen (char *msg);
void bannerscreen (void);
int load_mainmenu (void);

#endif
