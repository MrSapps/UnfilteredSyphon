#include "common.h"
#include "game.h"
#include "system.h"

/* stream.c's small data, still assembled from asm */
SDATA_LOCAL(gCurrentStreamReq)
SDATA_LOCAL(gStreamReqListRun)
SDATA_LOCAL(gStreamReqListPause)
SDATA_LOCAL(gTerrainStreamPause)
SDATA_LOCAL(gHogHeader)
SDATA_LOCAL(gHogPos)
SDATA_LOCAL(D_8010FA14)
SDATA_LOCAL(D_8010FA18)
SDATA_LOCAL(D_8010FA1C)
SDATA_LOCAL(D_8010FA28)
SDATA_LOCAL(D_8010FA2C)
SDATA_LOCAL(D_8010FA30)
SDATA_LOCAL(D_8010FA34)

/* CD position / file descriptor (libcd) */
typedef struct CdlLOC {
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} CdlLOC;

typedef struct CdlFILE {
    /* 0x00 */ CdlLOC pos;
    /* 0x04 */ u32 size;
    /* 0x08 */ char name[16];
} CdlFILE; /* size 0x18 */

typedef struct StreamReq {
    /* 0x00 */ void *addr;       /* destination buffer */
    /* 0x04 */ s32 sector;       /* absolute CD sector */
    /* 0x08 */ s32 numSectors;
    /* 0x0C */ u8 sync;          /* keep the CD busy / chain requests */
    /* 0x0D */ u8 unk0D[3];
    /* 0x10 */ void (*callback)(struct StreamReq *req);
    /* 0x14 */ void (*doneCallback)(s32 arg);
    /* 0x18 */ s32 arg;          /* tag passed to doneCallback */
    /* 0x1C */ s32 free;         /* nonzero when the slot is unused */
    /* 0x20 */ s32 index;
} StreamReq; /* size 0x24 */

#define STREAM_REQ_MAX 30
#define CDL_PAUSE 9
#define CDL_SETLOC 2
#define CDL_DISK_ERROR 5

extern StreamReq gStreamReqs[STREAM_REQ_MAX];
extern EGSListNode *gCurrentStreamReq;
extern EGSListNode *gStreamReqListRun;   /* requests issued while running the game */
extern EGSListNode *gStreamReqListPause; /* requests issued from other system states */
extern u8 gTerrainStreamPause;
extern void *gHogHeader;
extern s32 gHogPos;
extern s32 D_8010FA14; /* sector of the last read started */
extern s32 D_8010FA18; /* stalled tick counter */
extern s32 D_8010FA1C; /* system state at the last check */
extern char D_8010FA20[]; /* "WAIT %d" (.sdata) */
extern s32 D_8010FA28[]; /* "CDERR" (.sdata) */
extern s32 D_8010FA30[]; /* "CD DATA" (.sdata) */
extern char D_8010FA38[]; /* "V" (.sdata) */
extern char D_8010FA3C[]; /* "T" (.sdata) */
extern s32 cdError[]; /* status text shown while streaming */
extern u8 cdResult[];
extern void *gTextureCache;

int CdControlF(u8 com, u8 *param);
int CdControl(u8 com, u8 *param, u8 *result);
int CdReadSync(int mode, u8 *result);
int CdSync(int mode, u8 *result);
int CdReset(int mode);
int CdPosToInt(CdlLOC *p);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdRead(int sectors, void *buf, int mode);
void EGSListAddToEnd(EGSListNode **list, EGSListNode *after, void *data);
u8 EGSHogGetBlockInfoFromHeader(void *header, s32 id, u32 *offset, u32 *size);
void VramSwap(StreamReq *req);
void AckVram(StreamReq *req);
void FlushedStreamCallback(StreamReq *req);
void CheckStreamRequests(u8 noStart);
StreamReq *StreamRequest(void *addr, s32 sector, s32 numSectors, u8 sync,
                         void (*callback)(StreamReq *), s32 arg);
void DataStreamCallback(StreamReq *req);
void DataStream(void *addr, s32 sector, s32 numSectors, s32 arg, void (*done)(s32));
void FlushStreamReqList(EGSListNode **list);

/* the active request list depends on whether the game is running (state 0) */
#define STREAM_LIST() (gSystemState == 0 ? &gStreamReqListRun : &gStreamReqListPause)
#define STREAM_LIST_HEAD() (gSystemState != 0 ? gStreamReqListPause : gStreamReqListRun)

INCLUDE_RODATA("asm/main/nonmatchings/stream", D_80012380);

void InitStreamRequests(void) {
    s32 i;

    gTextureCache = D_801377E0;
    gCurrentStreamReq = NULL;
    gStreamReqListRun = NULL;
    gStreamReqListPause = NULL;
    for (i = 0; i < STREAM_REQ_MAX; i++) {
        gStreamReqs[i].free = -1;
        gStreamReqs[i].index = i;
    }
}

StreamReq *StreamRequest(void *addr, s32 sector, s32 numSectors, u8 sync,
                         void (*callback)(StreamReq *), s32 arg) {
    EGSListNode *node;
    EGSListNode *last;
    StreamReq *req;
    s32 i;

    node = gSystemState == 0 ? gStreamReqListRun : gStreamReqListPause;
    last = NULL;
    for (; node != NULL; node = node->next) {
        StreamReq *other = node->data;
        last = node;
        if (other->addr == addr && other->arg == arg) {
            return NULL;
        }
    }

    req = NULL;
    for (i = 0; i < STREAM_REQ_MAX; i++) {
        if (gStreamReqs[i].free != 0) {
            req = &gStreamReqs[i];
            req->addr = addr;
            req->sector = sector;
            req->numSectors = numSectors;
            req->arg = arg;
            req->callback = callback;
            req->doneCallback = NULL;
            req->free = 0;
            req->sync = sync;
            break;
        }
    }
#line 106
    ASSERT(req != NULL);
    EGSListAddToEnd(STREAM_LIST(), last, req);
    if (req->sync) {
        CheckStreamRequests(0);
    }
    return req;
}

void DataStreamCallback(StreamReq *req) {
    if (req->doneCallback != NULL) {
        req->doneCallback(req->arg);
    }
    if ((u32)req->arg >> 24 == 0xFF) {
        tcOverlay[0] = req->arg;
    }
}

void DataStream(void *addr, s32 sector, s32 numSectors, s32 arg, void (*done)(s32)) {
#line 132
    ASSERT(gCurrentStreamReq != (EGSListNode *)0xCACACACA);
    StreamRequest(addr, sector, numSectors, 0, DataStreamCallback, arg)->doneCallback = done;
}

s32 StreamOverlay(char *path, void *dst, s32 size, u32 tag, void (*done)(s32)) {
    CdlFILE file;
    u32 fileSize;

    if (EGSFileSearchCD(&file, path) == 0) {
        return 0;
    }
    fileSize = file.size;
    fileSize = ((fileSize + 0x7FF) >> 11) << 11;
    if ((u32)size < fileSize) {
        return 0;
    }
    DataStream(dst, CdPosToInt(&file.pos), fileSize >> 11, tag, done);
    return 1;
}

u8 StreamLock(void) {
    if (gTerrainStreamPause) {
        return 0;
    }
    if (STREAM_LIST_HEAD() == NULL) {
        PauseTerrainStream();
        return 1;
    }
    return 0;
}

void StreamSynchronous(void) {
    while (STREAM_LIST_HEAD() != NULL) {
        CheckStreamRequests(0);
        CheckStreamRequests(0);
    }
}

s32 OpenHogForStreaming(char *path, void *header) {
    CdlFILE file;

    if (EGSFileSearchCD(&file, path) == 0) {
        return 5;
    }
    gHogHeader = header;
    gHogPos = CdPosToInt(&file.pos);
    return 0;
}

s32 StreamHogBlock(s32 id, void *dst, u32 size, s32 tag, void (*done)(s32)) {
    u32 offset;
    u32 blockSize;

    if (EGSHogGetBlockInfoFromHeader(gHogHeader, id, &offset, &blockSize) == 0) {
        return 5;
    }
    if (size < blockSize) {
        return 0x1F;
    }
    DataStream(dst, gHogPos + (offset >> 11), blockSize >> 11, tag, done);
    return 0;
}

void FlushStreamReqList(EGSListNode **list) {
    EGSListNode *node;
    EGSListNode *next;

    node = *list;
    while (node != NULL) {
        next = node->next;
        EGSListRemove(list, node);
        node = next;
    }
}

void FlushAllStreamRequests(void) {
    CdControlF(CDL_PAUSE, 0);
    FlushStreamReqList(STREAM_LIST());
    gCurrentStreamReq = NULL;
    gTerrainStreamPause = 0;
}

void CheckStreamRequests(u8 noStart) {
    s32 state;
    u8 more;
    s32 readResult;
    s32 syncResult;
    StreamReq *req;
    CdlLOC loc;

    state = gSystemState;
    more = 1;
    if (state != D_8010FA1C) {
        CdControlF(CDL_PAUSE, 0);
        if (state == 0) {
            FlushStreamReqList(&gStreamReqListPause);
        }
        /* volatile keeps the scheduler from hoisting the gSystemState reload above the stores */
        *(EGSListNode * volatile *)&gCurrentStreamReq = NULL;
        *(volatile u8 *)&gTerrainStreamPause = 0;
        D_8010FA1C = *(volatile int *)&gSystemState;
    }
    if (gTerrainStreamPause) {
        if (STREAM_LIST_HEAD() == NULL) {
            return;
        }
        if (gPlayerRadioFlag) {
            return;
        }
        sprintf((char *)cdError, D_8010FA20, (D_8010FA18 >> 3) + 1);
        if (++D_8010FA18 == 61) {
            SysStopXA();
            D_8010FA18 = 0;
        }
        return;
    }
    D_8010FA18 = 0;
    while (more) {
        if (gCurrentStreamReq != NULL) {
            readResult = CdReadSync(1, cdResult);
            req = gCurrentStreamReq->data;
            if (readResult == 0) {
                *(char *)cdError = 0;
                CdControlF(CDL_PAUSE, 0);
                if (req->sector == D_8010FA14) {
                    if (req->callback != NULL) {
                        req->callback(req);
                    }
                } else {
                    req->sync = 0;
                    req->free = -1;
                    EGSListRemove(STREAM_LIST(), gCurrentStreamReq);
                    gCurrentStreamReq = NULL;
                    return;
                }
                EGSListRemove(STREAM_LIST(), gCurrentStreamReq);
                gCurrentStreamReq = NULL;
                req->free = -1;
                req->sync = 0;
            } else if (readResult < 0) {
                gCurrentStreamReq = NULL;
                return;
            } else {
                if (req->sync) {
                    continue;
                }
                return;
            }
        }
        syncResult = CdSync(1, cdResult);
        *(char *)cdError = 0;
        if (syncResult == 0) {
            return;
        }
        if (syncResult == CDL_DISK_ERROR) {
            __builtin_memcpy(cdError, D_8010FA28, 6); /* strcpy(cdError, "CDERR") */
            CdReset(0);
        }
        if (gCurrentStreamReq != NULL && ((StreamReq *)gCurrentStreamReq->data)->sync == 0) {
            return;
        }
        if (STREAM_LIST_HEAD() == NULL) {
            return;
        }
        if (noStart) {
            return;
        }
        if (gCurrentStreamReq == NULL) {
            gCurrentStreamReq = gSystemState == 0 ? gStreamReqListRun : gStreamReqListPause;
#line 474
            ASSERT(gCurrentStreamReq != NULL);
        }
        req = gCurrentStreamReq->data;
        if (req->callback == DataStreamCallback) {
            __builtin_memcpy(cdError, D_8010FA30, 8); /* strcpy(cdError, "CD DATA") */
        } else {
            sprintf((char *)cdError, "CD %s%ld", req->callback == VramSwap ? D_8010FA38 : D_8010FA3C,
                    req->arg & 0xFFF);
        }
        CdIntToPos(req->sector, &loc);
        CdControl(CDL_SETLOC, (u8 *)&loc, 0);
        CdRead(req->numSectors, req->addr, 0x80);
        D_8010FA14 = req->sector;
        if (!req->sync) {
            more = 0;
        }
    }
}

void PauseTerrainStream(void) {
    EGSListNode *cur;

    cur = gCurrentStreamReq;
    if (cur == (EGSListNode *)0xCACACACA) {
        return;
    }
    if (cur != NULL) {
        do {
            CheckStreamRequests(1);
        } while (gCurrentStreamReq == cur);
    }
    gTerrainStreamPause = 1;
    SysLoadTextureCacheOverlay(NULL, 0);
}

void UnpauseTerrainStream(void) {
    gTerrainStreamPause = 0;
    *(char *)cdError = 0;
}

void FlushStreamRequests(s32 tag) {
    EGSListNode *node;
    EGSListNode *next;
    StreamReq *req;

    node = gSystemState == 0 ? gStreamReqListRun : gStreamReqListPause;
    if (gCurrentStreamReq != NULL) {
        StreamReq *cur = gCurrentStreamReq->data;
        if (cur->arg == tag && cur->callback != AckVram) {
            cur->callback = FlushedStreamCallback;
            cur->free = -1;
            gCurrentStreamReq = NULL;
        }
    }
    while (node != NULL) {
        req = node->data;
        next = node->next;
        if (req->arg == tag) {
            EGSListRemove(STREAM_LIST(), node);
            req->free = -1;
        }
        node = next;
    }
}
