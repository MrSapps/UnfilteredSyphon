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
    u8 unk02[2];
    s32 tick;
} TargetTimer;
extern TargetTimer mTargetTimerList[8];
extern s16 D_8010562A;
extern u8 *gLevData;

void GameCameraSetModeTarget(s32 on, s32 arg1, GameObjectRuntime *target, s32 arg3);
s32 TextIsStringShowing(u16 str);
void TextChangeStringDuration(u16 str, s32 ticks);
void TextRemoveString(u16 str);
extern u8 D_801055A6[];   /* targeting profiles, 0x36 bytes per kind */
extern s32 D_8012C8AC[];  /* mTargetTimerList[i] start tick (aliases its +4) */
extern s32 gExeTick;
void func_80031AEC();
void func_80031BDC();
void func_80031C70();
void func_80031E10();
void func_80031F6C();
void func_80032304();
void TargetAcquire(GameObjectRuntime *rt, s32 mode);
int func_80032E24();
u8 func_80032988(GameObjectRuntime *rt, s16 target, s32 arg2);
int func_800333F8(GameObjectRuntime *rt);
void func_800323C4(TargetInfo *target);
void TextRemoveString(u16 str);

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

void TargetInitGobj(GameObjectRuntime *rt, TargetInfo *target, s32 kind, u8 arg3) {
    int i;

    if (target == NULL) {
        target = EGSMemMalloc(sizeof(TargetInfo));
#line 1263
        ASSERT(target != NULL);
    }
    rt->target = target;
    for (i = 0; i < 15; i++) {
        target->unk34[i] = 0;
    }
    target->profile = &D_801055A6[kind * 0x36];
    if (kind == 1) {
        target->actions[0] = func_80031AEC;
        target->actions[1] = func_80031BDC;
        target->actions[2] = func_80031C70;
        target->actions[3] = func_80031E10;
        target->actions[4] = func_80031F6C;
        target->unk1C = NULL;
    } else if (kind == 2) {
        target->actions[0] = func_80031AEC;
        target->actions[1] = func_80031BDC;
        target->actions[2] = func_80031C70;
        target->actions[3] = func_80031E10;
        target->actions[4] = func_80031F6C;
        target->unk1C = NULL;
    } else if (kind == 3) {
        target->actions[0] = func_80031AEC;
        target->actions[1] = func_80031C70;
        target->actions[2] = func_80031E10;
        target->actions[3] = func_80031F6C;
        target->actions[4] = func_80032304;
        target->unk1C = NULL;
    } else if (kind == 0) {
        target->actions[0] = NULL;
    }
    target->unkAC = arg3;
    target->unkB0 = arg3;
    target->unkA8 = -1;
    target->unk52 = 0;
    target->unk54 = 0;
    target->unk56 = 0;
    target->unkA4 = 0;
    target->unk60 = 0;
    target->unk02 = 0;
    target->threat = 0;
    target->unkC0 = 0;
    target->flags &= ~8;
    target->flags &= ~2;
    target->targetId = -1;
    target->unkBC = -1;
    target->unk5A = -1;
    target->unk5E = -1;
    target->startTick = gExeTick;
    target->lastTick = gExeTick;
    target->flags |= 1;
}

INCLUDE_ASM("asm/main/nonmatchings/target", func_800333F8);

static void func_800337C4(void) {
    if (D_8010F821) {
        TargetAcquire(gameObjectsInfo[gPlayerOffset].runtime, 2);
    }
    D_8010F821 = 0;
}

void TargetAcquireCAS(s16 id, s32 action) {
    GameObjectRuntime *rt;
    u8 wasTargeting;

    rt = gameObjectsInfo[id].runtime;
    if (action == 2) {
        if (D_8010F822) {
            gTargetModeFlag = 1;
            TargetAcquire(rt, 1);
            D_8010F822 = 0;
            return;
        }
        wasTargeting = gTargetModeFlag;
        TargetAcquire(rt, 0);
        if (wasTargeting) {
            if (TextIsStringShowing(D_8010FFDC)) {
                if (rt->target->targetId < 0) {
                    TextChangeStringDuration(D_8010FFDC, 40);
                    return;
                }
                TextRemoveString(D_8010FFDC);
                return;
            }
            D_8010FFDC = TextStatusMessage(gNoTgtMsg, 0);
        }
    } else if (action == 3) {
        D_8010F821 = 0;
        if (gTargetModeFlag) {
            D_8010F822 = 1;
            return;
        }
        if (gSniperMode) {
            CASRequestAction(id, 13);
        }
        gTargetModeFlag = 1;
        rt->target->targetId = -1;
    }
}

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

void TargetSetCAS(s16 id, s32 action) {
    int i;
    int freeSlot;
    u8 found;

    freeSlot = -1;
    found = 0;
    for (i = 0; i < 8; i++) {
        if (mTargetTimerList[i].id == id) {
            if (action == 0x2B) {
                mTargetTimerList[i].tick = gExeTick;
            } else if (action == 0x2C) {
                mTargetTimerList[i].id = -1;
            } else if (action == 0x2D && (u32)(gExeTick - mTargetTimerList[i].tick) >= 61 &&
                       (id != gPlayerOffset || !gPlayerFlashlight)) {
                CASRequestAction(id, 0x2C);
            }
            found = 1;
            break;
        }
        if (mTargetTimerList[i].id == -1) {
            freeSlot = i;
        }
    }
    if (!found && freeSlot != -1 && action == 0x2B) {
        mTargetTimerList[freeSlot].id = id;
        mTargetTimerList[freeSlot].tick = gExeTick;
    }
}

/* mode 0: keep/auto-pick, 1: pick next, 2: drop target */
void TargetAcquire(GameObjectRuntime *rt, s32 mode) {
    TargetInfo *target;
    int prev;
    s32 id;
    u8 targeting;

    target = rt->target;
    prev = target->targetId;
    if (rt->npcState == 2) {
        id = -1;
        if (rt->npc->flags & 0x20000000) {
            s32 ref = gameObjectsInfo[rt->linkedId].refId;
            if (gameObjectsInfo[ref].runtime->healthInfo->maxHealth > 0) {
                id = ref;
            }
        }
        if (id < 0) {
            id = func_80032E24(rt, id);
        }
        target->targetId = id;
    } else {
        targeting = gTargetModeFlag;
        if (mode == 0 || (prev < 0 && mode != 2)) {
            if (targeting && prev >= 0) {
                if (!func_80032988(rt, prev, 0)) {
                    target->targetId = -1;
                }
            } else {
                target->targetId = func_80032E24(rt);
            }
        } else if (mode == 1) {
            target->targetId = func_800333F8(rt);
            target->flags |= 8;
        } else if (mode == 2) {
            target->targetId = -1;
            gTargetModeFlag = 0;
        }
        if (!(*rt->flagsPtr & 0x02000000) && !gPlayerFlashlight) {
            if (rt->target->targetId >= 0) {
                CASRequestAction(rt->linkedId, 0x2B);
            } else {
                if (mode != 2) {
                    CASRequestAction(rt->linkedId, 0x2D);
                }
                gTargetModeFlag = 0;
            }
        }
    }
    if (target->targetId != prev) {
        func_800323C4(target);
        TargetCameraUpdate();
    }
}

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

