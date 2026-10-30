#include "common.h"
#include "system.h"
#include "gameobj.h"

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010028);

void SysChangeLevel(u32 level) {
#line 329
    ASSERT(level < SYS_LEVEL_COUNT);
    gContinueLevel[0] = 0;
    SysHalt(4);
    gSysInfo.level = level;
}

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010044);

INCLUDE_ASM("asm/main/nonmatchings/system", SysInit);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysLoadTextureCacheOverlay);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStreamTextureCacheOverlay);

INCLUDE_ASM("asm/main/nonmatchings/system", func_8001593C);

INCLUDE_ASM("asm/main/nonmatchings/system", StallOverlayCallback);

INCLUDE_ASM("asm/main/nonmatchings/system", StallFadeCallback);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStall);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStallCheck);

INCLUDE_RODATA("asm/main/nonmatchings/system", D_80010078);

INCLUDE_ASM("asm/main/nonmatchings/system", func_80015A64);

/* Matches as C (see SysStatePush below), but its ASSERT's __FILE__ string must be
 * emitted at the first assert in this file (in SysInit), so it waits until the
 * earlier asserting functions are decompiled:
 *
 * #line 1361
 * void SysStatePush(int state) {
 *     ASSERT(mSystemStateDepth < SYS_STATE_STACK_MAX);
 *     mSystemStateStack[++mSystemStateDepth] = state;
 *     func_80015A64(state, 1);
 * }
 */
INCLUDE_ASM("asm/main/nonmatchings/system", SysStatePush);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStatePop);

INCLUDE_ASM("asm/main/nonmatchings/system", SysSetState);

INCLUDE_ASM("asm/main/nonmatchings/system", SysSaveScreen);

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

INCLUDE_ASM("asm/main/nonmatchings/system", SysGetMovieStatus);

INCLUDE_ASM("asm/main/nonmatchings/system", func_80015DDC);

INCLUDE_ASM("asm/main/nonmatchings/system", func_80015DFC);

INCLUDE_ASM("asm/main/nonmatchings/system", SysStopMovie);

INCLUDE_ASM("asm/main/nonmatchings/system", SysPlayMovieCB);

INCLUDE_ASM("asm/main/nonmatchings/system", SysPlayMovie);

INCLUDE_ASM("asm/main/nonmatchings/system", SysExeTickQueueInit);

INCLUDE_ASM("asm/main/nonmatchings/system", SysExeTickQueueTimer);

INCLUDE_ASM("asm/main/nonmatchings/system", SysExeTickGetTimerTicks);

INCLUDE_ASM("asm/main/nonmatchings/system", func_800164D0);

INCLUDE_RODATA("asm/main/nonmatchings/system", jtbl_800100A8);
