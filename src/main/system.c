#include "common.h"
#include "system.h"
#include "gameobj.h"

/* system.c's small data, still assembled from asm/main/data/sdata.sdata.s */
SDATA_LOCAL(mSystemStateDepth)
SDATA_LOCAL(gSystemState)
SDATA_LOCAL(gSysHaltState)
SDATA_LOCAL(D_8010F622)
SDATA_LOCAL(gSysMovieDoneCallback)
SDATA_LOCAL(gStallLoaded)
SDATA_LOCAL(gStallFaded)
SDATA_LOCAL(exeTickTimerList)
SDATA_LOCAL(gStallCallback)

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010028);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysExecute);

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

void SysStall(s32 level, s32 fadeSpeed, void (*callback)(void)) {
    char name[24];

    SysStatePush(9);
    gStallLoaded = 0;
    gStallFaded = 0;
    gStallCallback = callback;
    GameSysSetLetterBoxMode(3);
    EGSDisplayFade(1, fadeSpeed, StallFadeCallback);
    SysStopXA();
    sprintf(name, D_8010F634, level);
    SysStreamTextureCacheOverlay(name, (void (*)(s32))StallOverlayCallback);
}

void SysStallCheck(void) {
    if (gStallLoaded != 0 && gStallFaded != 0) {
        gStallCallback();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/system", func_80015A64);

void SysStatePush(int state) {
#line 1364
    ASSERT(mSystemStateDepth < SYS_STATE_STACK_MAX);
    mSystemStateStack[++mSystemStateDepth] = state;
    func_80015A64(state, 1);
}

void SysStatePop(void) {
#line 1379
    ASSERT(mSystemStateDepth > 0);
    mSystemStateDepth--;
    func_80015A64(mSystemStateStack[mSystemStateDepth], 0);
}

void SysSetState(int state) {
    mSystemStateStack[mSystemStateDepth] = state;
    func_80015A64(state, 1);
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
    return mSysMovie.playing;
}

void func_80015DDC(void) {
    func_801470B0();
}

void func_80015DFC(void) {
    if (gPlayerRadioFlag == 0) {
        GameSysSetLetterBoxMode(0);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/system", SysStopMovie);

void SysPlayMovieCB(void) {
    EGSDisplayMovieMalloc(D_80145BF0, mSysMovie.bufferSize + 0x11000);
    if (gSystemState != 4) {
        SysStatePush(3);
        SysSetMusicExciteLevel(1);
    }
    mSysMovie.playing = 1;
    EGSDisplayFlush();
    EGSDeviceGraphicsSetResolution(mSysMovie.width, mSysMovie.height);
    EGSDisplayAddMovie(*gCamera, 1);
    PrepareMovieSound();
}

INCLUDE_ASM("asm/main/nonmatchings/system", SysPlayMovie);

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

INCLUDE_RODATA("asm/main/nonmatchings/system", jtbl_800100A8);
