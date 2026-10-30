#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"

/* target.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F821)
SDATA_LOCAL(D_8010F822)
SDATA_LOCAL(D_8010FFDC)

extern u8 D_8010F821;  /* target release pending */
extern u8 D_8010F822;  /* re-acquire requested while already in target mode */
extern u16 D_8010FFDC; /* "no target" status message id */
extern u8 gTargetModeFlag;
extern s32 gSniperMode;
extern void *gNoTgtMsg;
typedef struct TargetTimer {
    s16 id;
    u8 unk02[6];
} TargetTimer;
extern TargetTimer mTargetTimerList[8];
extern s16 D_8010562A;
extern u8 *gLevData;

void GameCameraSetModeTarget(s32 on, s32 arg1, GameObjectRuntime *target, s32 arg3);
s32 TextIsStringShowing(u16 str);
void TextChangeStringDuration(u16 str, s32 ticks);
void TextRemoveString(u16 str);
void TargetAcquire(GameObjectRuntime *rt, s32 mode);
s16 func_80032E24();
u8 func_80032988(GameObjectRuntime *rt, s16 target, s32 arg2);
s16 func_800333F8(GameObjectRuntime *rt);
void func_800323C4(TargetInfo *target);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetCalcProb);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetSetThreatLevel);

void TargetCameraUpdate(void) {
    s16 id;

    if (gTargetModeFlag && (id = gObjPlayer->target->targetId) >= 0) {
        GameCameraSetModeTarget(1, 0, gameObjectsInfo[id].runtime, 0);
    } else {
        GameCameraSetModeTarget(0, 0, NULL, 0);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/target", func_80032988);

INCLUDE_ASM("asm/main/nonmatchings/target", func_80032E24);

void TargetInit(void) {
    int i;

    D_8010FFDC = 0;
    D_8010F821 = 0;
    for (i = 0; i < 8; i++) {
        mTargetTimerList[i].id = -1;
    }
    D_8010562A = *(u16 *)(gLevData + 0x4C);
}

INCLUDE_RODATA("asm/main/nonmatchings/target", D_8001149C);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetInitGobj);

INCLUDE_ASM("asm/main/nonmatchings/target", func_800333F8);

static void func_800337C4(void) {
    if (D_8010F821) {
        TargetAcquire(gameObjectsInfo[gPlayerOffset].runtime, 2);
    }
    D_8010F821 = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/target", TargetAcquireCAS);

void TargetAcquireNoneCAS(s16 id, s32 action) {
    GameObjectRuntime *rt;

    rt = gameObjectsInfo[id].runtime;
    if (action == 0x41) {
        D_8010F821 = 1;
        EGSDisplayQueueInterrupt(func_800337C4, 5, (s32)&D_8010F821);
        return;
    }
    TargetAcquire(rt, 2);
}

INCLUDE_ASM("asm/main/nonmatchings/target", TargetAimCAS);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetSetCAS);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetAcquire);

void TargetClear(GameObjectRuntime *rt) {
    TargetInfo *target;

    target = rt->target;
    target->targetId = -1;
    target->unkBC = -1;
    target->unk5E = -1;
    target->threat = 0;
    target->unkB8 = 0;
    target->unkC0 = 0;
    target->flags &= ~2;
    target->unkB0 = target->unkAC;
}

INCLUDE_ASM("asm/main/nonmatchings/target", TargetShoot);

INCLUDE_ASM("asm/main/nonmatchings/target", func_80034D24);

INCLUDE_ASM("asm/main/nonmatchings/target", TargetViewConeCheck);

INCLUDE_ASM("asm/main/nonmatchings/target", func_80035328);
