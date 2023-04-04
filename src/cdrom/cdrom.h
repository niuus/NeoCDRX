/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* CDROM Data interface
*
* Simulate all CDROM reading using standard files.
****************************************************************************/
#ifndef __NEOCDROM__
#define __NEOCDROM__

/*** Globals ***/
extern char cdpath[1024];
extern int img_display;
extern int ipl_in_progress;

/*** Prototypes ***/
int cdrom_process_ipl(void);
int recon_filetype(char *ext);
int cdrom_mount(char *mount);
void cdrom_load_files(void);
void neogeo_upload(void);
void cdrom_load_title(void);
int cdrom_process_ipl(void);
void neogeo_end_upload(void);
void neogeo_start_upload(void);
void neogeo_ipl(void);

typedef struct
  {
    char fname[16];
    short bank;
    int offset;
    int unk1;
    short unk2;
    short image;
    short unk3;
  }
__attribute__ ((__packed__)) LOADFILE;

#endif
