/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* Generic File I/O
*
* This module attempts to provide a single interface for file I/O.
****************************************************************************/
#ifndef __GENFILEIO__
#define __GENFILEIO__

#define GEN_MODE_READ 1
#define GEN_MODE_WRITE 2

typedef struct
  {
    u32 handle;
    int length;
    int currpos;
    long long int offset_on_media64;
    int mode;
  }
GENFILEINFO;

typedef struct
  {
    u32 (*gen_fopen) (const char *filename, const char *mode);
    u32 (*gen_fread) (char *buffer, int block, int length, u32 fp);
    u32 (*gen_fwrite) (char *buffer, int block, int length, u32 fp);
    int (*gen_fclose) (u32 fp);
    int (*gen_fseek) (u32 fp, int where, int whence);
    int (*gen_ftell) (u32 fp);
    int (*gen_getdir)(char *dir);
    void (*gen_mount) (void);
    void (*gen_fcloseall) (void);
  }
GENHANDLER;

typedef u32 GENFILE;

extern u32 GEN_fopen (const char *filename, const char *mode);
extern u32 GEN_fread (char *buffer, int block, int length, u32 fp);
extern u32 GEN_fwrite (char *buffer, int block, int length, u32 fp);
extern int GEN_fclose (u32 fp);
extern int GEN_fseek (u32 fp, int where, int whence);
extern int GEN_ftell (u32 fp);
extern int GEN_getdir(char *dir);
extern void GEN_mount ( void );
extern void GEN_fcloseall (void);

extern void GEN_SetHandler (GENHANDLER * g);

#endif
