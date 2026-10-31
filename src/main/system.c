#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "scratch.h"


/* system.c's data, defined in original order (docs/data_migration.md). */
void PLAYERObjDispatch(SysMsg *msg), NPCObjDispatch(SysMsg *msg), COMPUTERObjDispatch(SysMsg *msg);
void DOORObjDispatch(SysMsg *msg), CAMERAObjDispatch(SysMsg *msg), ELEVATORObjDispatch(SysMsg *msg);
void SPECIALObjDispatch(SysMsg *msg), FIREObjDispatch(SysMsg *msg), GENObjDispatch(SysMsg *msg);
void LASERObjDispatch(SysMsg *msg), LIGHTObjDispatch(SysMsg *msg), INTERACTObjDispatch(SysMsg *msg);
void SWITCHObjDispatch(SysMsg *msg), GLASSObjDispatch(SysMsg *msg), TGTObjDispatch(SysMsg *msg);
void ANIMATEDObjDispatch(SysMsg *msg), NPCGenDispatch(SysMsg *msg), BOMBObjDispatch(SysMsg *msg);
void EFFECTS2ObjDispatch(SysMsg *msg), EFFECTSObjDispatch(SysMsg *msg);
void ROTATINGObjDispatch(SysMsg *msg), PICKUPObjDispatch(SysMsg *msg);
void AUTOPICKUPObjDispatch(SysMsg *msg);
void func_80015DDC(void);

/* .data */
SysMsgHandler gDispatchFunctions[125] = {
    PLAYERObjDispatch, NPCObjDispatch, NPCObjDispatch, (SysMsgHandler)0x80144FA0,
    COMPUTERObjDispatch, COMPUTERObjDispatch, DOORObjDispatch, DOORObjDispatch, CAMERAObjDispatch,
    CAMERAObjDispatch, DOORObjDispatch, ELEVATORObjDispatch, ELEVATORObjDispatch,
    ELEVATORObjDispatch, SPECIALObjDispatch, (SysMsgHandler)0x801435DC, FIREObjDispatch,
    GENObjDispatch, LASERObjDispatch, LIGHTObjDispatch, LIGHTObjDispatch, LIGHTObjDispatch,
    (SysMsgHandler)0x801443E8, (SysMsgHandler)0x8014216C, SPECIALObjDispatch, GENObjDispatch,
    INTERACTObjDispatch, SWITCHObjDispatch, SWITCHObjDispatch, SWITCHObjDispatch,
    (SysMsgHandler)0x80144AF4, (SysMsgHandler)0x80144AF4, GLASSObjDispatch, TGTObjDispatch,
    SPECIALObjDispatch, LIGHTObjDispatch, SWITCHObjDispatch, FIREObjDispatch, NULL, FIREObjDispatch,
    LIGHTObjDispatch, ANIMATEDObjDispatch, NPCGenDispatch, NULL, TGTObjDispatch, GLASSObjDispatch,
    BOMBObjDispatch, EFFECTS2ObjDispatch, FIREObjDispatch, LIGHTObjDispatch, NULL, LIGHTObjDispatch,
    LIGHTObjDispatch, NPCObjDispatch, EFFECTSObjDispatch, GLASSObjDispatch, TGTObjDispatch,
    EFFECTS2ObjDispatch, SWITCHObjDispatch, DOORObjDispatch, (SysMsgHandler)0x80141998,
    TGTObjDispatch, DOORObjDispatch, ROTATINGObjDispatch, ELEVATORObjDispatch, SWITCHObjDispatch,
    TGTObjDispatch, (SysMsgHandler)0x8014365C, TGTObjDispatch, TGTObjDispatch, LIGHTObjDispatch,
    LIGHTObjDispatch, LIGHTObjDispatch, DOORObjDispatch, TGTObjDispatch, TGTObjDispatch,
    NPCObjDispatch, GLASSObjDispatch, GENObjDispatch, PICKUPObjDispatch, PICKUPObjDispatch,
    GLASSObjDispatch, GLASSObjDispatch, EFFECTSObjDispatch, DOORObjDispatch, GLASSObjDispatch,
    GLASSObjDispatch, INTERACTObjDispatch, INTERACTObjDispatch, INTERACTObjDispatch,
    FIREObjDispatch, EFFECTS2ObjDispatch, NPCObjDispatch, SPECIALObjDispatch, LIGHTObjDispatch,
    SPECIALObjDispatch, DOORObjDispatch, DOORObjDispatch, TGTObjDispatch, AUTOPICKUPObjDispatch,
    INTERACTObjDispatch, NPCObjDispatch, TGTObjDispatch, TGTObjDispatch, SPECIALObjDispatch,
    SPECIALObjDispatch, SWITCHObjDispatch, LIGHTObjDispatch, INTERACTObjDispatch, LIGHTObjDispatch,
    (SysMsgHandler)0x80144114, LIGHTObjDispatch, GLASSObjDispatch, DOORObjDispatch,
    (SysMsgHandler)0x80143B9C, SPECIALObjDispatch, SPECIALObjDispatch, EFFECTSObjDispatch,
    DOORObjDispatch, CAMERAObjDispatch, ELEVATORObjDispatch, SWITCHObjDispatch, SWITCHObjDispatch,
    SWITCHObjDispatch, SWITCHObjDispatch,
};
int mSystemStateStack[10] = { 0 };
char tcOverlay[16] = { 0 };
static SysMsgDesc gSysMsgTable[2] = { { 0, NULL, NULL }, { 1, (SysMsgHandler)func_80015DDC, NULL } };
static u8 D_800FCB28[SYS_MOVIE_VRAM_PAGES] = { /* texture pages the movie decoder overwrites */
    0x00, 0x01, 0x02, 0x06, 0x07, 0x08, 0x10, 0x11, 0x12, 0x16, 0x17, 0x18,
};

/* .sdata */
static char D_8010F608[] = "";
int mSystemStateDepth = 1;
int gSystemState = 0;
u8 gSysHaltState = 1;
void (*gSysMovieDoneCallback)(void) = NULL;
s32 gSysFrameCounter = 0;
u8 gUsePCFiles = 0;
u8 gSystemStopDebugDisplay = 0; /* debug overlay (top-right status window) */
static u8 D_8010F622 = 0xFF;          /* its text window */
static s32 D_8010F624 = 1;      /* last shown "no damage" state */
static char D_8010F628[] = "FR "; /* used by the asm SysExecute */
static char D_8010F62C[] = "INV";
static char D_8010F630[] = "XX";
/* then the literals "%s.OVL" and "MENU" */

/* .sbss */
static u16 D_8010FF68; /* text ids */
static u16 D_8010FF6A;
static u16 D_8010FF6C;
/* Only [0] is used, but the next item is at +8 and the section ends at 0x8010FF78:
 * an 8-byte object (aspsx 8-aligns 8-byte .lcomm). */
static u16 D_8010FF70[4];

/* .bss. aspsx 16-aligns every .lcomm larger than 8 bytes, but the original has
 * mSysMsgQueueNext and the sprite buffer at 4-aligned offsets, so the two queues and
 * the buffer must have been one object.  (The asm SysExecute reaches the three by
 * name through config/main.syms.ld.) */
static ExeTickTimer mExeTickTimers[SYS_EXE_TICK_TIMER_MAX];
static struct {
    SysMsgQueue queue;
    SysMsgQueue next;
    u8 textSprites[0x370];
} D_80110560;
#define mSysMsgQueue D_80110560.queue
#define mSysMsgQueueNext D_80110560.next
#define D_80111B48 D_80110560.textSprites /* text sprite buffer */

/* commons this TU reaches through $gp */
EGSListNode *exeTickTimerList;
void (*gStallCallback)(void);
u8 gStallFaded;
u8 gStallLoaded;

extern char cdError[];
extern s16 gGabeSector;
extern char **gSectorMap; /* 0x10 bytes per sector, name first */
s32 NPCDebugGetNoDamageFlag(void);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextSetWindowJustification(u8 window, s32 just);
void TextSpriteInitialize(void *buf, s32 count);
u16 TextAddString(u8 window, char *str, s32 ticks, void *sprites);
void TextAddStringMulti(u8 window, s32 n, char **strs, s32 ticks, void *sprites, u16 *ids);
void TextSetStringIntensity(u16 str, s32 r, s32 g, s32 b);
u16 TextReplaceString(u16 str, char *text);
void TextRemoveWindow(u8 window);
void EGSDisplayGetLoad(s16 *a, s16 *b, s32 *c, char **text);
void FriendlyPreparePlayerAndCameraFaceListsBeforeUse(s32);
void FriendlyRevertPlayerAndCameraFaceListsAfterUse(void);
void GameCameraUpdateLeadAndWatchOffsets(void);
void CameraUpdateAfterDisplay(void);
void CameraUpdateBeforeDisplay(void);
void MovementUpdateAfterDisplay(void);
void MovementUpdateBeforeDisplay(void);
void GameSysProcessMoveRequests(void);
void GameSysClearMoveRequests(void);
void CASProcess(void);
void ShadowProcess(void);
void func_800164D0(void);
static void func_800153BC(SysMsgQueue *src, SysMsgQueue *dst);

/* Functions missing from siphon.map were static in system.c. */
static void SysEnterState(u32 state, int push);

static const s32 D_80010028[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

void SysChangeLevel(u32 level) {
#line 329
    ASSERT(level < SYS_LEVEL_COUNT);
    gContinueLevel[0] = 0;
    SysHalt(4);
    gSysInfo.level = level;
}

void SysInit(u8 mode) {
    int i;
    GameObjectRuntime *rt;
    GameObjectModel *model;

    gSystemTick = 0;
    EGSDebugInit(0);
    EGSMemInit(0x801FF000);
    EGSDebugInit(0);

    if (mode == 3) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            rt = gameObjectsInfo[i].runtime;
            model = NULL;
            if (rt != NULL) {
                if (rt->anim != NULL) {
                    model = rt->anim->model;
                }
                if (model != NULL && (model->flags & GOBJ_MODEL_FLAG_400000)) {
                    model->display->unk10 = 0;
                }
            }
        }
    }

    if (mode == 1) {
#line 471
        ASSERT(EGSDeviceGraphicsInit(384, 240, 0) == 0);
        EGSDeviceCounterInitSystemClock();
        EGSDeviceCdInit();
        SysLoadTextureCacheOverlay("MOVIE.OVL", 0);
        EGSDeviceControllerInit();
        EGSDisplayInit();
        if (mSystemStateDepth != mode) {
            mSystemStateDepth = mode;
        }
    } else {
        EGSDisplayInit();
        EGSDisplayReinitCameras();
        EGSDeviceRender();
        EGSDeviceRender();
        PauseTerrainStream();
        UnpauseTerrainStream();
        BreakDraw();
    }

    SysLoadOverlay("INIT.OVL", D_80145BF0, 0);
    SYSCasInit();
    SYSCameraInit();
    SYSMovementInit();
    SYSGameInit();
    ShadowInit();

    for (i = 0; i < SYS_MSG_QUEUE_MAX; i++) {
        mSysMsgQueue.msgs[i].id = 0;
    }
    mSysMsgQueue.count = 0;

    GameSysTableHook();
    gLevelReady = 0;
    SysPostMessage(1, SYS_MSG_IMMEDIATE, SYS_MSG_GLOBAL, SYS_MSG_GLOBAL, mode, 0, 0, 0);
    if (mode != 1) {
        TerrainUpdateActivation(1);
    } else {
        SysStatePush(4);
    }
    D_8010F622 = 0xFF;
}

void CopyMsgToSysQueue(SysMsg *msg) {
    EGSMiscMemcpy(&mSysMsgQueue.msgs[mSysMsgQueue.count], msg, sizeof(SysMsg) / 4);
    mSysMsgQueue.count++;
}

#ifdef NON_MATCHING
/*
 * Structure and the scratchpad-stack sequence match (SPAD_ENTER/LEAVE around plain calls
 * reproduce the original exactly); remaining differences are register allocation in the
 * dispatch loop (original: s0 = queue then msg, s3 = table, s4 = msgs) and which delay
 * slot the loop counter's zeroing lands in.
 */
void SysExecute(void) {
    char *noDmgText[2];
    s16 loadA;
    s16 loadB;
    s32 loadC;
    char *loadText;
    SysMsg *msg;
    SysMsgHandler fn;
    SysMsgHandler *dispatch;
    SysMsgHandler *table;
    SysMsgQueue *queue;
    int i;
    int n;
    s32 noDamage;

    gExeTick++;
    FriendlyPreparePlayerAndCameraFaceListsBeforeUse(1);
    SPAD_ENTER();
    GameCameraUpdateLeadAndWatchOffsets();
    SPAD_LEAVE();
    CameraUpdateAfterDisplay();
    SPAD_ENTER();
    SysPostMessage(4, SYS_MSG_IMMEDIATE, SYS_MSG_LEVEL, SYS_MSG_LEVEL, 0, 0, 0, 0);
    SPAD_LEAVE();
    MovementUpdateAfterDisplay();
    SPAD_ENTER();
    GameSysProcessMoveRequests();
    SPAD_LEAVE();
    GameSysClearMoveRequests();
    FriendlyRevertPlayerAndCameraFaceListsAfterUse();
    CASProcess();
    func_800164D0();

    queue = &mSysMsgQueueNext;
    func_800153BC(&mSysMsgQueue, queue);
    n = queue->count;
    table = gDispatchFunctions;
    for (i = 0, msg = queue->msgs; i < n; i++, msg++) {
        if (queue->count == 0) {
            break;
        }
        if (msg->to == SYS_MSG_GLOBAL) {
            fn = gSysMsgTable[msg->id].handle;
        } else if (msg->to == SYS_MSG_LEVEL || msg->to == SYS_MSG_LEVEL_RAW) {
            fn = gSysInfo.levelMsgTable[msg->id].handle;
        } else {
            if (msg->to != SYS_MSG_OBJ_666) {
                dispatch = &table[gameObjectsModelInfo[gameObjectsInfo[msg->to].modelIndex].type];
            } else {
                dispatch = &table[SYS_MSG_OBJ_666];
            }
            fn = *dispatch;
        }
        if (fn != NULL) {
            fn(msg);
        }
    }

    if (!gSystemStopDebugDisplay) {
        if (!(gSystemTick & 3)) {
            noDamage = NPCDebugGetNoDamageFlag();
            if (D_8010F622 == 0xFF) {
                noDmgText[0] = D_8010F628;
                noDmgText[1] = D_8010F62C;
                D_8010F624 = 1;
                D_8010F622 = TextAddWindow(70, -85, 92, 75);
                TextSetWindowJustification(D_8010F622, 1);
                TextSpriteInitialize(D_80111B48, 20);
                D_8010FF6C = TextAddString(D_8010F622, D_8010F630, -1, D_80111B48);
                TextAddStringMulti(D_8010F622, 2, noDmgText, -1, D_80111B48 + 0x2C0, &D_8010FF68);
                TextSetStringIntensity(D_8010FF6C, 0xA0, 0x60, 0xA0);
                TextSetStringIntensity(D_8010FF6A, 0x30, 0x60, 0xA0);
                D_8010FF70[0] = TextAddString(D_8010F622, D_8010F630, -1, D_80111B48 + 0x39C);
                D_8010FF68 = TextReplaceString(D_8010FF68, D_8010F608);
                TextSetStringIntensity(D_8010FF70[0], 0xA0, 0x50, 0x30);
            }
            D_8010FF6C = TextReplaceString(D_8010FF6C, cdError[0] ? cdError : gSectorMap[gGabeSector * 4]);
            EGSDisplayGetLoad(&loadA, &loadB, &loadC, &loadText);
            D_8010FF70[0] = TextReplaceString(D_8010FF70[0], loadText);
            if (noDamage != D_8010F624) {
                D_8010FF6A = TextReplaceString(D_8010FF6A, noDamage ? D_8010F62C : D_8010F608);
                D_8010F624 = noDamage;
            }
        }
    } else if (D_8010F622 != 0xFF) {
        TextRemoveWindow(D_8010F622);
        D_8010F622 = 0xFF;
    }

    MovementUpdateBeforeDisplay();
    SPAD_ENTER();
    ShadowProcess();
    SPAD_LEAVE();
    SPAD_ENTER();
    CameraUpdateBeforeDisplay();
    SPAD_LEAVE();
}
#else
INCLUDE_ASM("asm/main/nonmatchings/system", SysExecute);
#endif

void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3) {
    SysMsg *msg;
    SysMsg local;
    SysMsgHandler fn;
    SysMsgHandler *dispatch;
    SysMsgHandler *table;
    int i;

    if (id == 0 || mSysMsgQueue.count >= SYS_MSG_QUEUE_MAX) {
        return;
    }
    msg = &mSysMsgQueue.msgs[mSysMsgQueue.count];

    if (from == SYS_MSG_GLOBAL) {
        if (gSysMsgTable[id].onPost != NULL) {
            gSysMsgTable[id].onPost(msg, d0, d1, d2, d3);
        }
    } else if (from == SYS_MSG_LEVEL) {
        if (gSysInfo.levelMsgTable[id].onPost != NULL) {
            gSysInfo.levelMsgTable[id].onPost(msg, d0, d1, d2, d3);
        }
    }

    if (priority == SYS_MSG_IMMEDIATE) {
        if (to == SYS_MSG_GLOBAL) {
            msg->data[0] = d0;
            gSysMsgTable[id].handle(msg);
        } else if (to == SYS_MSG_LEVEL) {
            gSysInfo.levelMsgTable[id].handle(msg);
        } else if (to == SYS_MSG_LEVEL_RAW) {
            local.id = id;
            local.priority = priority;
            local.from = from;
            local.to = to;
            gSysInfo.levelMsgTable[id].handle(&local);
        } else {
            table = gDispatchFunctions;
            if (to != SYS_MSG_OBJ_666) {
                dispatch = &table[gameObjectsModelInfo[gameObjectsInfo[to].modelIndex].type];
            } else {
                dispatch = &table[SYS_MSG_OBJ_666];
            }
            fn = *dispatch;
            local.id = id;
            local.priority = priority;
            local.from = from;
            local.to = to;
            local.data[0] = d0;
            local.data[1] = d1;
            local.data[2] = d2;
            local.data[3] = d3;
            if (fn != NULL) {
                fn(&local);
            }
        }
        return;
    }

    for (i = 0; i < mSysMsgQueue.count; i++) {
        if (mSysMsgQueue.msgs[i].id == id && mSysMsgQueue.msgs[i].from == from &&
            mSysMsgQueue.msgs[i].to == to && to != SYS_MSG_LEVEL) {
            return;
        }
    }
    msg->id = id;
    msg->priority = priority;
    msg->from = from;
    msg->to = to;
    mSysMsgQueue.count++;
}

/* Move queued messages from src to dst, highest priority (0) first. */
void func_800153BC(SysMsgQueue *src, SysMsgQueue *dst) {
    int count;
    int priority;
    int n;
    int i;
    u16 id;

    count = 0;
    n = src->count;
    for (priority = 0; priority < SYS_MSG_PRIORITY_COUNT; priority++) {
        if (n == 0) {
            break;
        }
        for (i = 0; i < n; i++) {
            if (src->msgs[i].priority == priority) {
                id = src->msgs[i].id;
                if (id != 0 && (gInEOLState == 0 || (id != 10 && id != 17))) {
                    EGSMiscMemcpy(&dst->msgs[count++], &src->msgs[i], sizeof(SysMsg) / 4);
                    src->msgs[i].id = 0;
                }
            }
        }
    }
    src->count = 0;
    dst->count = count;
}

void SysFlushMsgQueue(void) {
    int i;

    for (i = 0; i < SYS_MSG_QUEUE_MAX; i++) {
        mSysMsgQueue.msgs[i].id = 0;
        mSysMsgQueueNext.msgs[i].id = 0;
    }
    mSysMsgQueue.count = 0;
    mSysMsgQueueNext.count = 0;
}

s32 SysLoadData(char *name, void **buf, s32 size) {
    char path[56];
    s32 file;
    u32 len;
    s32 nread;
    s32 err;

    strcpy(path, name);
    if (EGSFileOpen(path, &file) != 0) {
        sprintf(path, "\\COMMON\\%s;1", name);
        if (EGSFileOpen(path, &file) != 0) {
            sprintf(path, "\\%s\\%s;1", gLevelNames[gSysInfo.level], name);
            if (EGSFileOpen(path, &file) != 0) {
                SysIsPaused();
                return 0;
            }
        }
    }

    if (size < 0) {
        EGSFileGetBufferSize(file, &len);
        if (len < ABS(size)) {
            len = ABS(size);
        }
        *buf = EGSMemMalloc(len);
    } else if (size != 0) {
        len = size;
    } else {
        EGSFileGetBufferSize(file, &len);
        *buf = EGSMemMalloc(len);
    }

    if (*buf == NULL) {
        EGSFileClose(&file);
        return 0;
    }
    err = EGSFileRead(file, *buf, len, &nread);
    EGSFileClose(&file);
    if (err != 0) {
        *buf = NULL;
        return 0;
    }
    return len;
}

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010078);

#ifdef NON_MATCHING
/* Register allocation differs around the size clamp (limit lands in $a3 directly). */
void SysLoadOverlay(char *name, u8 *addr, u8 viaBuffer) {
    char path[56];
    s32 file;
    u32 len;
    s32 nread;
    u8 *dst;
    u32 max;

    DrawSync(0);
    sprintf(path, "\\BIN\\%s;1", name);
#line 1135
    ASSERT(EGSFileOpen(path, &file) == 0);
    EGSFileGetBufferSize(file, &len);
    dst = D_801377E0;
    if (!viaBuffer) {
        dst = addr;
    }
#line 1139
    ASSERT(EGSFileRead(file, dst, len, &nread) == 0);
    if (viaBuffer) {
        max = D_80145BF0 - D_80140FE0;
        len = (len + 3) & ~3;
        if (len < max) {
            max = len;
        }
        len = max;
        EGSMiscMemcpy(addr, D_801377E0, max >> 2);
        EGSMemRegisterVariableOverlayEnd(D_80140FE0 + len);
        tcOverlay[0] = 0;
    }
    EGSFileClose(&file);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/system", SysLoadOverlay);
#endif

void SysLoadTextureCacheOverlay(char *name, u8 arg1) {
    if (name == NULL) {
        tcOverlay[0] = 0;
        return;
    }
    if (strcmp(name, tcOverlay) != 0) {
        SysLoadOverlay(name, D_801377E0, arg1);
        strcpy(tcOverlay, name);
    }
}

void SysStreamTextureCacheOverlay(char *name, void (*done)(s32)) {
    char path[56];

    DrawSync(0);
    sprintf(path, D_80010078, name);
    if (strcmp(name, tcOverlay) == 0) {
        done(0);
        return;
    }
    StreamOverlay(path, D_801377E0, 0x9800, (u8)name[0] | 0xFF000000, done);
    /* first byte stays cleared until the stream finishes */
    strcpy(tcOverlay + 1, name + 1);
    tcOverlay[0] = 0;
}

void func_8001593C(void) {
    InterfaceForceOff();
    EGSDisplayFadeOff();
    SysStatePop();
}

void StallOverlayCallback(void) {
    gStallLoaded = 1;
}

void StallFadeCallback(void) {
    gStallFaded = 1;
}

void SysStall(char *overlay, s32 fadeSpeed, void (*callback)(void)) {
    char name[24];

    SysStatePush(9);
    gStallLoaded = 0;
    gStallFaded = 0;
    gStallCallback = callback;
    GameSysSetLetterBoxMode(3);
    EGSDisplayFade(1, fadeSpeed, StallFadeCallback);
    SysStopXA();
    sprintf(name, "%s.OVL", overlay);
    SysStreamTextureCacheOverlay(name, (void (*)(s32))StallOverlayCallback);
}

void SysStallCheck(void) {
    if (gStallLoaded != 0 && gStallFaded != 0) {
        gStallCallback();
    }
}

static void SysEnterState(u32 state, int push) {
    gSystemState = state;
    switch (state) {
    case SYS_STATE_GAME:
        SysPauseSound(0);
        UnpauseTerrainStream();
        break;
    case 1:
    case 2:
    case 5:
    case 6:
    case 8:
    case SYS_STATE_STALL:
        break;
    case SYS_STATE_MOVIE:
        PauseTerrainStream();
        break;
    case SYS_STATE_TITLE:
        StreamSynchronous();
        if (gLevelRestartHeap != NULL) {
            gObjPlayer = NULL;
            EGSDisplayStopLevel();
            DrawSync(0);
        }
        SysLoadOverlay("TITLE.OVL", D_80140FE0, 1);
        SysLoadTextureCacheOverlay("MOVIE.OVL", 0);
        if (gLevelRestartHeap != NULL) {
            EGSMemSetHeap(gLevelRestartHeap);
            EGSDisplayResetOTs();
            EGSDisplayMalloc(EGSMemAvail());
        }
        break;
    case SYS_STATE_MENU:
        if (push) {
            SysStall("MENU", 14, func_8001593C);
        }
        break;
    case SYS_STATE_COUNT:
#line 1350
        ASSERT(state < SYS_STATE_COUNT);
        break;
    default:
#line 1355
        ASSERT(0);
        break;
    }
}

void SysStatePush(int state) {
#line 1364
    ASSERT(mSystemStateDepth < SYS_STATE_STACK_MAX);
    mSystemStateStack[++mSystemStateDepth] = state;
    SysEnterState(state, 1);
}

void SysStatePop(void) {
#line 1379
    ASSERT(mSystemStateDepth > 0);
    mSystemStateDepth--;
    SysEnterState(mSystemStateStack[mSystemStateDepth], 0);
}

void SysSetState(int state) {
    mSystemStateStack[mSystemStateDepth] = state;
    SysEnterState(state, 1);
}

void SysSaveScreen(void) {
    GameSysSetLetterBoxMode(3);
    PauseTerrainStream();
    SysStatePush(4);
    func_801445A4();
}

int SysIsPaused(void) {
    return gSystemState != 0 && gSystemState != 5 && gSystemState != 6 && gSystemState != 9;
}

void SysHalt(u8 mode) {
    EGSDeviceControllerVibrate(2, 0, 0);
    if (mode == 3) {
        SysStopAllSongs();
    } else {
        SysStopSound();
    }
    SysStatePush(2);
    gSysHaltState = mode;
}

void SysUnhalt(void) {
    UnpauseTerrainStream();
    SysStatePop();
    gSysHaltState = 0;
}

u8 SysGetMovieStatus(void) {
    return gMovieInfo.playing;
}

void func_80015DDC(void) {
    func_801470B0();
}

void func_80015DFC(void) {
    if (gPlayerRadioFlag == 0) {
        GameSysSetLetterBoxMode(0);
    }
}

void SysStopMovie(void) {
    s32 volume;
    int i;
    u8 page;
    void *camera;

    volume = SysSoundGetMasterVolume(2);
    SysSoundSetMasterVolume(2, 0);
    camera = *gCamera;
    gLevelReady = 1;
    EGSDisplayDeleteMovie(camera);
    EGSDeviceGraphicsSetResolution(384, 240);
    EGSDeviceGraphicsSetColorMode(0);
    EGSDisplayMovieMalloc(NULL, 0);
    MovieClose();
    UnpauseTerrainStream();
    UnpauseTerrainStream();

    if (gMovieInfo.playing) {
        EGSDisplayFlush();
        if (gSystemState != SYS_STATE_TITLE && gMovieInfo.doneMsgId != SYS_MSG_END_OF_LEVEL) {
            /* the decoder used these pages; stream their textures back in */
            for (i = 0; i < SYS_MOVIE_VRAM_PAGES; i++) {
                page = D_800FCB28[i];
                if (gTexturePageMap[page]) {
                    StreamVramPage(page, 0);
                }
            }
            StreamSynchronous();
        }
    }
    if (gSystemState != SYS_STATE_TITLE) {
        GameSysSetLetterBoxMode(2);
        EGSDisplayFade(-1, 7, func_80015DFC);
    }

    gMovieInfo.playing = 0;
    if (gMovieInfo.doneMsgId == SYS_MSG_END_OF_LEVEL) {
        EGSDisplayQueueInterrupt(GameSysEOL, 2, 0);
    } else if (gMovieInfo.doneMsgId != 0) {
        SysPostMessage(gMovieInfo.doneMsgId, 3, gMovieInfo.doneMsgFrom, gMovieInfo.doneMsgTo, 0, 0, 0, 0);
    }
    if (gSystemState == SYS_STATE_MOVIE) {
        SysStatePop();
    }
    SysSoundSetMasterVolume(2, volume);
    if (gSysMovieDoneCallback != NULL) {
        gSysMovieDoneCallback();
        gSysMovieDoneCallback = NULL;
    }
}

void SysPlayMovieCB(void) {
    EGSDisplayMovieMalloc(D_80145BF0, gMovieInfo.bufferSize + 0x11000);
    if (gSystemState != 4) {
        SysStatePush(3);
        SysSetMusicExciteLevel(1);
    }
    gMovieInfo.playing = 1;
    EGSDisplayFlush();
    EGSDeviceGraphicsSetResolution(gMovieInfo.width, gMovieInfo.height);
    EGSDisplayAddMovie(*gCamera, 1);
    PrepareMovieSound();
}

void SysPlayMovie(MovieInfo *info) {
    s32 pos[6];
    char path[56];
    u8 *buf;
    s32 err;

    buf = D_80145BF0;
    err = 5;
    if (!StreamLock()) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        } else {
            PauseTerrainStream();
            UnpauseTerrainStream();
            StreamLock();
        }
    }
    SysLoadTextureCacheOverlay("MOVIE.OVL", 0);
    MovieSetStreamBufferNumSectors(25);
    if (gMovieInfo.playing) {
        SysStopMovie();
        gMovieInfo.playing = 0;
    }

    /* absolute paths are used as-is, bare names live in \\MOVIES\\ */
    sprintf(path, "%s%s.STR;1",
            (info->name[0] != '\\' && info->name[0] != '/') ? "\\MOVIES\\" : D_8010F608, info->name);
    gMovieInfo = *info;

    if (gUsePCFiles) {
        err = 5;
    } else if (EGSFileSearchCD(pos, path) != 0) {
        MovieSetDoubleDecodeBuffers(0);
        if (info->trueColor) {
            EGSDeviceGraphicsSetColorMode(1);
        }
        if (info->halfSize) {
            gMovieInfo.bufferSize = 0x20F58;
        } else {
            gMovieInfo.bufferSize = 0x25D78;
        }
        err = MovieOpen(path, info->unk84, info->halfSize, -1, SysStopMovie, buf, gMovieInfo.bufferSize);
    }

    if (err == 0) {
        if (gSystemState != SYS_STATE_TITLE) {
            EGSDisplayFadeFull();
        }
        SysPlayMovieCB();
        return;
    }
    gMovieInfo.playing = 0;
    if (gSystemState != SYS_STATE_TITLE) {
        EGSDisplayFadeFull();
    }
    if (gSystemState == SYS_STATE_MOVIE) {
        SysStatePop();
    }
    SysStopMovie();
}

void SysExeTickQueueInit(void) {
    int i;

    exeTickTimerList = NULL;
    for (i = 0; i < SYS_EXE_TICK_TIMER_MAX; i++) {
        mExeTickTimers[i].callback = NULL;
        mExeTickTimers[i].ticks = 0;
        mExeTickTimers[i].arg = 0;
    }
}

void SysExeTickQueueTimer(void (*callback)(s32), s32 ticks, s32 arg) {
    EGSListNode *node;
    ExeTickTimer *timer;
    int i;

    for (node = exeTickTimerList; node != NULL; node = node->next) {
        timer = node->data;
        if (timer->callback == callback && timer->arg == arg) {
            if (ticks == 0) {
                EGSListRemove(&exeTickTimerList, node);
                timer->callback = NULL;
            } else {
                timer->ticks = ticks;
            }
            return;
        }
    }
    if (ticks != 0) {
        for (i = 0; i < SYS_EXE_TICK_TIMER_MAX; i++) {
            timer = &mExeTickTimers[i];
            if (timer->callback == NULL) {
                break;
            }
        }
#line 1780
        ASSERT(i < SYS_EXE_TICK_TIMER_MAX);
        timer->callback = callback;
        timer->ticks = ticks;
        timer->arg = arg;
        EGSListAdd(&exeTickTimerList, timer);
    }
}

s32 SysExeTickGetTimerTicks(void (*callback)(s32)) {
    int i;
    ExeTickTimer *timer;

    for (i = 0, timer = mExeTickTimers; i < SYS_EXE_TICK_TIMER_MAX; i++, timer++) {
        if (timer->callback == callback) {
            return timer->ticks;
        }
    }
    return 0;
}

void func_800164D0(void) {
    EGSListNode *node;
    EGSListNode *next;
    ExeTickTimer *timer;
    void (*callback)(s32);

    for (node = exeTickTimerList; node != NULL; node = next) {
        timer = node->data;
        next = node->next;
        if (--timer->ticks == 0) {
            callback = timer->callback;
            timer->callback = NULL;
            EGSListRemove(&exeTickTimerList, node);
            callback(timer->arg);
        }
    }
}

