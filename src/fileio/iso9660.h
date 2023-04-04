/****************************************************************************
* UFS ISO9660
*
* Minimal ISO9660 implementation for Nintendo Gamecube
****************************************************************************/

#ifndef __UFS_ISO9660__
#define __UFS_ISO9660__

/*** Minimal PVD ***/
#define RECLEN 0
#define EXTENT 6
#define FILE_LENGTH 14
#define FILE_FLAGS 25
#define FILE_NAMELEN 32
#define FILENAME 33
#define PVDROOT 0x9c
#define MAX_LEVELS 256
#define MAX_FILENAME 128
#define SYSID 8
#define VOLID 40
#define VOLSETID 190
#define PUBSETID 318
#define DATASETID 446
#define APPSETID 574
#define PTABLELENGTH 0x88
#define PTABLEOFFSET 0x94

#define ISDIR 2
#define ISHIDDEN 1

typedef struct
  {
    int pvdoffset;
    int rootoffset;
    int rootlength;
    int pathtablelength;
    int pathtableoffset;
    char system_id[34];
    char volume_id[34];
    char volset_id[130];
    char pubset_id[130];
    char dataset_id[130];
    char appset_id[130];
  }
PVD;

typedef struct
  {
    unsigned char nlength;
    char extended;
    int record;
    short parent;
  }
__attribute__ ((__packed__)) PATHHDR;

typedef struct
  {
    unsigned char nlength;
    char extended;
    int offsetle;
    int offsetbe;
    int lengthle;
    int lengthbe;
    unsigned char years;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
    unsigned char utcoffset;
    unsigned char flags;
    unsigned char interleaved;
    unsigned char intergap;
    short seqnole;
    short seqnobe;
    unsigned char identifier;
  }
__attribute__ ((__packed__)) DIRENTRY;

typedef struct
  {
    long long int offset64;
    int parent;
    char *path;
  }
PATHDECODE;

typedef struct
  {
    long long int offset64;
    char *path;
  }
FILEDECODE;

typedef struct
  {
    char fname[1024];
    DIRENTRY fdir;
  }
CACHEFILE;

/*** Prototypes ***/
extern int mount_image (void);
extern void unmount_image (void);
extern void GetSubDirectories (char *dir, char *buf, int len);
extern DIRENTRY *FindFile (char *filename);
extern int gcsim64( void *buffer, long long int offset, int length );
extern void dvd_motor_off(void);

#endif
