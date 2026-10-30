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

INCLUDE_ASM("asm/main/nonmatchings/system", SysPostMessage);

INCLUDE_ASM("asm/main/nonmatchings/system", func_800153BC);

void SysFlushMsgQueue(void) {
    int i;

    for (i = 0; i < SYS_MSG_QUEUE_MAX; i++) {
        mSysMsgQueue.msgs[i].id = 0;
        mSysMsgQueueNext.msgs[i].id = 0;
    }
    mSysMsgQueue.count = 0;
    mSysMsgQueueNext.count = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/system", SysLoadData);

INCLUDE_ASM("asm/main/nonmatchings/system", SysLoadOverlay);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysStreamTextureCacheOverlay);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysStall);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStallCheck);

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010078);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysPlayMovieCB);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysExeTickQueueTimer);

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
