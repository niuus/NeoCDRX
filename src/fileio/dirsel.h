/****************************************************************************
* NeoGeo Directory Selector
*
* As there is no 'ROM' to speak of, use the directory as the starting point
****************************************************************************/

#ifndef __NEODIRSEL__
#define __NEODIRSEL__

extern char basedir[1024];
extern char dirbuffer[0x10000];
extern char scratchdir[1024];

extern char root_dir[10];
extern void DirSelector (void);

#endif
