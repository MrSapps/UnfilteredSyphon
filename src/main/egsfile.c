#include "common.h"

#include "game.h"

/* egsfile.c's small data, still assembled from asm/main/data/sdata.sdata.s */
SDATA_LOCAL(D_8010FF40)
SDATA_LOCAL(gIsStreamingFlag)
SDATA_LOCAL(D_8010FF45)
SDATA_LOCAL(D_8010FF46)
SDATA_LOCAL(D_8010FF48)
SDATA_LOCAL(D_8010FF4C)
SDATA_LOCAL(D_8010FF50)
SDATA_LOCAL(D_8010FF54)
SDATA_LOCAL(gFogFilePtr)
SDATA_LOCAL(gFogHeader)
SDATA_LOCAL(D_8010FF60)

/* libcd */
typedef struct {
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} CdlLOC;

typedef struct {
    CdlLOC pos;
    u32 size;
    char name[16];
} CdlFILE;

int CdControl(u8 com, u8 *param, u8 *result);
int CdPosToInt(CdlLOC *p);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
CdlFILE *CdSearchFile(CdlFILE *fp, char *name);

char *strcpy(char *, const char *);
char *strcat(char *, const char *);
char *strchr(const char *, int);
char *strrchr(const char *, int);
int strcmp(const char *, const char *);

void *EGSMemMalloc(u32 size);
void EGSMemFree(void *ptr);
void EGSMiscMemcpy(void *dst, void *src, s32 words);

#define EGS_FILE_FREE 0xCACACACA
#define EGS_FILE_MAX 5

/* An open file (0x14 bytes). */
typedef struct EGSFile {
    /* 0x00 */ CdlLOC pos; /* first sector */
    /* 0x04 */ u32 size; /* EGS_FILE_FREE when the slot is unused */
    /* 0x08 */ s32 last; /* last sector on CD; <= 0: -(PC file handle) */
    /* 0x0C */ CdlLOC cur; /* read position */
    /* 0x10 */ u32 remaining; /* bytes left to read */
} EGSFile;

/* FOG: a directory of files packed together on the CD (0x190 bytes). */
typedef struct FogEntry {
    /* 0x00 */ char name[16];
    /* 0x10 */ s32 sector; /* relative to FogHeader.sector */
    /* 0x14 */ s32 sectors; /* size in sectors */
} FogEntry;

typedef struct FogHeader {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 sector; /* absolute sector of the fog file */
    /* 0x0C */ u8 unk0C[4];
    /* 0x10 */ FogEntry entries[16];
} FogHeader;

typedef void (*EGSFileErrorCallback)(int err);

extern EGSFile *D_8010FF40; /* file being streamed */
extern u8 gIsStreamingFlag;
extern u8 D_8010FF45;
extern u8 D_8010FF46;
extern void (*D_8010FF48)(void); /* called when the stream stops */
extern s32 D_8010FF4C;
extern s32 D_8010FF50;
extern EGSFileErrorCallback D_8010FF54;
extern EGSFile *gFogFilePtr;
extern FogHeader *gFogHeader;
extern u8 D_8010FF60; /* file table initialised */
extern FogHeader fogHeader;
extern EGSFile D_80125210[EGS_FILE_MAX];

int EGSFileClose(EGSFile **file);
int EGSFileStreamPause(u8 stop);
static void EGSFileStreamTick(void);
static void EGSFileStreamCheckEnd(void);
int CdSync(int mode, u8 *result);
int CdRead2(long mode);
int EGSDeviceCounterSetPeriodicCallback(void (*func)(void), int period, s32 *handle);
void EGSDeviceCounterUnsetPeriodicCallback(s32 handle);
int EGSFileOpen(char *name, EGSFile **out);
int EGSFileRead(EGSFile *file, void *buf, u32 size, u32 *nread);
static void EGSFileError(int err);
static int EGSFileSeek(EGSFile *file);
EGSFile *FindFreeFile(void);
int PClseek(int fd, int offset, int whence);
int CdRead(int sectors, void *buf, int mode);
int CdReadSync(int mode, u8 *result);

CdlFILE *EGSFileSearchCD(CdlFILE *fp, char *name)
{
    char base[16];
    char *p;
    FogEntry *entry;
    int i;

    if (gFogHeader != NULL) {
        p = strrchr(name, '\\');
        if (p != NULL) {
            strcpy(base, p + 1);
        }
        p = strchr(base, ';');
        if (p != NULL) {
            *p = '\0';
        }
        for (i = 0; i < 16; i++) {
            entry = &gFogHeader->entries[i];
            if (strcmp(entry->name, base) == 0) {
                CdIntToPos(gFogHeader->sector + entry->sector, &fp->pos);
                if (entry->sectors == 0) {
                    return NULL;
                }
                fp->size = entry->sectors * 0x800;
                return fp;
            }
        }
    }
    return CdSearchFile(fp, name);
}

void FileCloseFog(void)
{
    if (gFogHeader != NULL) {
        EGSFileClose(&gFogFilePtr);
        gFogHeader = NULL;
    }
}

#ifdef NON_MATCHING
/* a0/a1 of the ASSERT call get swapped by the scheduler */
int EGSFileOpenFog(char *name)
{
    u8 *buf;
    u32 nread;

    FileCloseFog();
    if (EGSFileOpen(name, &gFogFilePtr) != 0) {
        return 0;
    }
    EGSFileRead(gFogFilePtr, buf = EGSMemMalloc(0x800), 0x800, &nread);
#line 131
    ASSERT(nread == sizeof(buf[0]) * 0x800);
    EGSMiscMemcpy(&fogHeader, buf, sizeof(FogHeader) / 4);
    EGSMemFree(buf);
    gFogHeader = &fogHeader;
    gFogHeader->sector = CdPosToInt(&gFogFilePtr->pos);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsfile", EGSFileOpenFog);
#endif

EGSFile *FindFreeFile(void)
{
    int i;

    if (!D_8010FF60) {
        D_8010FF60 = 1;
        for (i = 0; i < EGS_FILE_MAX; i++) {
            D_80125210[i].size = EGS_FILE_FREE;
        }
    }
    for (i = 0; i < EGS_FILE_MAX; i++) {
        if (D_80125210[i].size == EGS_FILE_FREE) {
            D_80125210[i].size = 0;
            return &D_80125210[i];
        }
    }
    return NULL;
}

static int EGSFileSeek(EGSFile *file)
{
    int tries = 0;
    int ret = 0;

    while (ret == 0 && tries++ < 10) {
        ret = CdControl(2, (u8 *)&file->cur, NULL);
        if (ret == 1) {
            ret = CdControl(0x15, NULL, NULL);
        }
    }
    if (ret == 0) {
        EGSFileError(0x25);
        return 0x25;
    }
    return 0;
}

int EGSFileAccess(EGSFile *file, u32 offset)
{
    int start;

    if (file->last <= 0) {
        PClseek(-file->last, offset, 0);
    } else {
        start = CdPosToInt(&file->pos);
#line 203
        ASSERT((offset & 0x7FF) == 0);
        CdIntToPos(start + offset / 0x800, &file->cur);
        file->remaining = file->size - offset;
    }
    return 0;
}

int EGSFileOpen(char *name, EGSFile **out)
{
    CdlFILE fp;
    CdlFILE *found;
    int tries;
    int ret;
    int start;

    if (name == NULL || out == NULL) {
        return 1;
    }
    if (gIsStreamingFlag == 1) {
        ret = EGSFileStreamPause(0);
        if (ret != 0) {
            return ret;
        }
    }
    *out = FindFreeFile();
    if (*out == NULL) {
        return 3;
    }
    tries = 0;
    found = NULL;
    while (found == NULL && tries++ < 5) {
        found = EGSFileSearchCD(&fp, name);
    }
    if (found == NULL) {
        (*out)->size = EGS_FILE_FREE;
        *out = NULL;
        EGSFileError(5);
        return 5;
    }
    if (fp.size == 0) {
        (*out)->size = EGS_FILE_FREE;
        *out = NULL;
        EGSFileError(4);
        return 4;
    }
    start = CdPosToInt(&fp.pos);
    (*out)->pos = fp.pos;
    (*out)->size = fp.size;
    (*out)->last = start + (fp.size + 0x7FF) / 0x800 - 1;
    (*out)->cur = fp.pos;
    (*out)->remaining = fp.size;
    return EGSFileSeek(*out);
}

int EGSFileOpenPC(char *dir, char *name, EGSFile **out)
{
    char path[80];
    char *p;
    int ret;

    strcpy(path, dir);
    strcat(path, name);
    p = strchr(path, ';');
#line 297
    ASSERT(p != NULL);
    *p = '\0';
    D_8010FF46 = 0;
    ret = EGSFileOpen(path, out);
    D_8010FF46 = 1;
    return ret;
}

int EGSFileGetFileSize(EGSFile *file, u32 *size)
{
    if (file == NULL || size == NULL) {
        return 1;
    }
    *size = file->size;
    return 0;
}

int EGSFileGetBufferSize(EGSFile *file, u32 *size)
{
    if (file == NULL || size == NULL) {
        return 1;
    }
    if (file->last < 0) {
        *size = file->size;
    } else {
        *size = (file->size + 0x7FF) / 0x800 * 0x800;
    }
    return 0;
}

int EGSFileRead(EGSFile *file, void *buf, u32 size, u32 *nread)
{
    u32 whole;
    u32 sectors;
    int err;
    int tries;
    int ret;

    if (file == NULL || buf == NULL || (file->last >= 0 && size < 0x800) || nread == NULL) {
        *nread = 0;
        return 1;
    }
    if (file->remaining == 0) {
        *nread = 0;
        return 0;
    }
    if (gIsStreamingFlag == 1) {
        err = EGSFileStreamPause(0);
        if (err != 0) {
            *nread = 0;
            return err;
        }
    }
    whole = size / 0x800;
    if (file->remaining >= whole * 0x800) {
        sectors = whole;
        *nread = whole * 0x800;
    } else {
        sectors = (file->remaining + 0x7FF) / 0x800;
        *nread = file->remaining;
    }
    tries = 0;
    ret = 0;
    while (ret == 0 && tries++ < 10) {
        ret = CdControl(2, (u8 *)&file->cur, NULL);
        if (ret != 0) {
            CdRead(sectors, buf, 0x80);
            ret = CdReadSync(0, NULL) != -1;
        }
    }
    if (ret == 0) {
        *nread = 0;
        EGSFileError(0x24);
        return 0x24;
    }
    CdIntToPos(CdPosToInt(&file->cur) + sectors, &file->cur);
    file->remaining -= *nread;
    return 0;
}

int EGSFileRewind(EGSFile *file)
{
    if (file == NULL) {
        return 1;
    }
    if (file->last < 0) {
        return 4;
    }
    if (gIsStreamingFlag == 1) {
        EGSFileStreamPause(0);
    }
    file->cur = file->pos;
    file->remaining = file->size;
    return EGSFileSeek(file);
}

int EGSFileClose(EGSFile **file)
{
    if (file == NULL || *file == NULL) {
        return 1;
    }
    if (gIsStreamingFlag == 1 && *file == D_8010FF40) {
        EGSFileStreamPause(1);
    }
    (*file)->size = EGS_FILE_FREE;
    *file = NULL;
    return 0;
}

int EGSFileSetErrorCallback(EGSFileErrorCallback cb)
{
    D_8010FF54 = cb;
    return 0;
}

static void EGSFileError(int err)
{
    if (D_8010FF54 != NULL) {
        D_8010FF54(err);
    }
}

/* Periodic callback while streaming: (re)starts the CD read once the drive is idle. */
static void EGSFileStreamTick(void)
{
    u8 result[8];
    int ret = 0;
    int tries = 0;

    CdSync(1, result);
    if (result[0] != 2) {
        return;
    }
    if (D_8010FF50 != 0) {
        EGSDeviceCounterUnsetPeriodicCallback(D_8010FF50);
    }
    D_8010FF50 = 0;
    while (ret == 0 && ++tries < 11) {
        ret = CdControl(2, (u8 *)&D_8010FF40->cur, NULL);
        if (ret == 1) {
            ret = CdRead2(D_8010FF45 == 1 ? 0xC0 : 0x1C0);
        }
    }
    if (ret == 0) {
        EGSFileError(0x25);
        EGSFileStreamPause(1);
        return;
    }
    if (D_8010FF45 == 1 && D_8010FF4C == 0) {
        EGSDeviceCounterSetPeriodicCallback(EGSFileStreamCheckEnd, 3, &D_8010FF4C);
    }
}

/* Periodic callback: pauses the stream when it gets near the end of the file. */
static void EGSFileStreamCheckEnd(void)
{
    u8 result[8];
    CdlLOC loc;

    if (CdControl(0x10, NULL, result) == 1) {
        loc.minute = result[0];
        loc.second = result[1];
        loc.sector = result[2];
        if (CdPosToInt(&loc) > D_8010FF40->last - 12) {
            EGSFileStreamPause(1);
        }
    }
}

int EGSFileStreamPlay(EGSFile *file, u8 mode, void (*done)(void))
{
    int ret;

    if (file == NULL) {
        return 1;
    }
#line 539
    ASSERT(file->last >= 0);
    if (gIsStreamingFlag == 1) {
        ret = EGSFileStreamPause(0);
        if (ret != 0) {
            return ret;
        }
    }
    D_8010FF40 = file;
    D_8010FF48 = done;
    D_8010FF45 = mode;
    gIsStreamingFlag = 1;
#line 554
    ASSERT(D_8010FF50 == 0);
    ret = EGSDeviceCounterSetPeriodicCallback(EGSFileStreamTick, 1, &D_8010FF50);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

int EGSFileStreamPause(u8 stop)
{
    u8 result[8];
    u8 param[8];
    int ret = 0;
    int tries;

    param[0] = 0x80;
    if (gIsStreamingFlag) {
        if (D_8010FF50 != 0) {
            EGSDeviceCounterUnsetPeriodicCallback(D_8010FF50);
        }
        D_8010FF50 = 0;
        if (D_8010FF4C != 0) {
            EGSDeviceCounterUnsetPeriodicCallback(D_8010FF4C);
        }
        D_8010FF4C = 0;
    }
    if (!gIsStreamingFlag) {
        return 0;
    }
    gIsStreamingFlag = 0;
    if (D_8010FF48 != NULL) {
        D_8010FF48();
    }
    D_8010FF48 = NULL;
    tries = 0;
    while (ret == 0 && ++tries < 11) {
        if (stop == 1) {
            ret = CdControl(9, NULL, NULL);
        } else {
            ret = CdControl(0xE, param, NULL);
        }
    }
    if (ret == 0) {
        EGSFileError(0x25);
        return 0x25;
    }
    D_8010FF40->cur.minute = result[0];
    D_8010FF40->cur.second = result[1];
    D_8010FF40->cur.sector = result[2];
    D_8010FF40 = NULL;
    return 0;
}

