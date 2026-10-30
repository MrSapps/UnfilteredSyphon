#include "common.h"
#include "level.h"

/* Object types specific to this level */
#define SUBWAY2_TYPE_BOMB_SWITCH 0x57
#define SUBWAY2_TYPE_BOMB 0x58
#define SUBWAY2_TYPE_DEVICE 0x76

/* subway2.ovl statics, still defined in the overlay's asm .data/.bss */
extern GameObjectRuntime *D_801420AC; /* cop linked to the bomb */
extern s16 D_801420B0;                /* bomb object id */
extern s16 D_801420B4;                /* device object id */
extern u8 D_801420B8;                 /* countdown running */
extern u8 D_801420BC;                 /* bomb armed */
extern void *D_80142070;              /* status messages */
extern void *D_80142080;
extern s32 D_80142074;
extern s32 D_80142078;
extern s32 D_8014207C;
extern s32 D_801077FC;
extern u8 D_80142084[];           /* saved objective/timer state */
typedef struct { u8 unk00[8]; void *msg; } Subway2Objective;
extern Subway2Objective *D_80142088;
extern u8 gEliminateRhoemerMsg[];

static void func_801412D0();
static void func_80141400(s16 id, s16 by);
static void func_8014145C();
static s32 func_801415A0();
static void func_80141624(void);
static void func_8014165C(s16 id);
static s32 func_801417B0(s16 id);
static s32 func_801418BC(s16 id);
static void func_80141AC0(s16 id);
static s32 func_80141B78(void);
static s32 func_80141B88(s16 id, s16 kind);
static void func_80141C34(void);

#ifdef NON_MATCHING
/* Same instructions; the original schedules both index loads up front. */
/* Countdown expired: the bomb goes off. */
static void func_801412D0(void) {
    s32 pos[4];
    s16 bomb;
    GameObjectRuntime *bombRt;
    GameObjectHealth *health;

    bomb = D_801420B0;
    bombRt = gameObjectsInfo[bomb].runtime;
    health = gameObjectsInfo[D_801420B4].runtime->healthInfo;
    health->armor = 1;
    health->health = 1;
    ObjectBigExplosionFX(bomb, pos, 0x1800, 5, 10);
    DamageRadius(bomb, pos, 0x1C2, 0x1E0, 0x60, 0x11, 0);
    health = bombRt->healthInfo;
    health->armor = -1;
    health->health = -1;
    bombRt->flags23 |= 2;
    NPCHazmatStart(D_801420AC->id);
    SysPlayXA(0x8F, 0, 1);
    InterfaceCountdownAbort();
    D_801420B8 = 0;
    D_801420BC = 1;
}
#else
INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801412D0);
#endif

static void func_80141400(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        NPCCopDamagedCallback(id, by);
        ObjectBombDamagedCallback(id, by);
    }
}

#ifdef NON_MATCHING
/* Right length and logic; register allocation of the id copy differs. */
static void func_8014145C(s32 id, s16 by) {
    GameObjectRuntime *rt;
    s16 type;

    rt = gameObjectsInfo[(s16)id].runtime;
    type = GOBJ_TYPE((s16)id);
    if (type == GOBJ_TYPE_COP) {
        if (!gDeviceDetonated) {
            NPCCopKilledWashington(id, by, (s16)gameObjectsInfo[(s16)id].refId, 0, 1);
        }
    } else if (type == GOBJ_TYPE_BOMB) {
        if (!(rt->flags1 & 0x80)) {
            ObjectBombDeath(1, 0x82, 0, id);
        }
    } else if (type == SUBWAY2_TYPE_BOMB) {
        if (rt->flags0 & 0x20) {
            func_801412D0();
        }
    }
}
#else
INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_8014145C);
#endif

static s32 func_801415A0(s32 arg0, s32 arg1, s32 arg2, s32 id) {
    s32 ret;

    if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 2)) {
        ret = 1;
        SysPlayXA(0x126, 0, 1);
    } else {
        ObjectBombDeath(1, 0x97, 1, id);
        ret = 0;
    }
    return ret;
}

static void func_80141624(void) {
    D_801420BC = 0;
    SysPlayXA(0x5A, 0, 1);
    LevelObjectiveComplete(1);
}

static void func_8014165C(s16 id) {
    GameObjectRuntime *rt;
    int type;

    type = GOBJ_TYPE(id);
    if (type == SUBWAY2_TYPE_BOMB_SWITCH) {
        rt = gameObjectsInfo[D_801420B0].runtime;
        rt->flags0 &= ~0x10;
        WeaponAdd(0x18, 1);
        TextStatusMessage(D_80142070, 0);
        SysPlayXA(0x99, 0, 1);
        LevelObjectiveComplete(0);
    } else if (type == SUBWAY2_TYPE_BOMB) {
        if (gObjectives != NULL && (gObjectives->completed & 1)) {
            WeaponRemove(0x18);
            D_801420B8 = 1;
            InterfaceStartCountdown(5, 0, func_801412D0);
        }
    }
}

#ifdef NON_MATCHING
/* two addu operand orders differ */
static s32 func_801417B0(s16 id) {
    int type;
    s32 ref;
    int found;

    if (id != GOBJ_ID_666) {
        type = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type;
        if (type == 0x24 || type == 0x7B) {
            ref = gameObjectsInfo[id].refId;
            if (ref != -1) {
                found = ObjectInvisibleMatchToRefID(ref, 0x5D);
                if (found != ref && (gameObjectsInfo[(s16)found].flags24 & 0x20)) {
                    SysPlayXA(0x92, 0, 1);
                }
            }
            TextStatusMessage(D_80142080, 0);
            return 1;
        }
    }
    return 3;
}
#else
INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801417B0);
#endif

#ifdef NON_MATCHING
/* addu operand order of the flags24 lookups differs (as in func_801417B0). */
static s32 func_801418BC(s16 id) {
    s32 ref;
    s32 ret;
    int type;
    int found;

    ref = gameObjectsInfo[id].refId;
    ret = 1;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == 0x5D) {
            if (gameObjectsInfo[(s16)ref].flags24 & 0x20) {
                ret = 0;
            } else if (!gPlayerFlashlight) {
                ret = 0;
            }
        } else if (type == GOBJ_TYPE_BOMB) {
            if (gameObjectsInfo[ref].runtime->flags1 & 0x80) {
                ret = 0;
            }
        } else if (D_801420B8 || D_801420BC || gameObjectsInfo[ref].runtime->healthInfo->health == -1) {
            ret = 0;
        } else {
            found = ObjectInvisibleMatchToRefID(ref, 0x5D);
            if (found != ref && (gameObjectsInfo[(s16)found].flags24 & 0x20)) {
                ret = 0;
            }
        }
    }
    return ret;
}
#else
INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801418BC);
#endif

static void func_80141AC0(s16 id) {
    int i;
    s32 count;

    if ((s16)gameObjectsInfo[id].refId != -1) {
        LevelObjectiveAdd(0, id);
        LevelObjectiveAdd(1, -1);
        if (gObjectives != NULL) {
            count = gObjectives->count;
            for (i = 0; i < count; i++) {
                if (gObjectives != NULL) {
                    gObjectives->unk24 |= 1 << i;
                }
            }
        }
    }
}

static s32 func_80141B78(void) {
    return D_80142074;
}

static s32 func_80141B88(s16 id, s16 kind) {
    if (kind == 3) {
        if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == SUBWAY2_TYPE_BOMB_SWITCH) {
            return D_801077FC;
        }
        return D_80142078;
    }
    if (kind == 4) {
        return D_8014207C;
    }
    return 0;
}

static void func_80141C34(void) {
    SysPlaySong(0, 7, 0, 0);
    SysPlaySong(0, 13, 0, 0);
}

void iSubway2Init(s32 mode) {
    int i;
    GameObjectRuntime *rt;
    s16 type;
    s16 linkedType;
    GameObjectHealth *health;

    D_801420AC = NULL;
    D_801420B0 = -1;
    D_801420B4 = -1;
    D_801420B8 = 0;
    D_801420BC = 0;
    InterfaceViralRadarInit();
    SaveObjectivesNTimersReinit(D_80142084, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(2, -1);
    }
    if (mode == 3) {
        func_80141C34();
    } else {
        gSysMovieDoneCallback = func_80141C34;
    }
    gUsingSndBigFire = 1;
    D_80142088->msg = gEliminateRhoemerMsg;
    SysStopSong(0, 0x18);

    DamageSetDamagedCallback(func_80141400);
    DamageSetDestroyedCallback((void (*)(s16, s16))func_8014145C);
    LevelSetEOLCallback(func_801415A0);
    UseSetBombCallback(func_80141624);
    UseSetLevSpecObjCallback(func_8014165C);
    UseSetSwitchCallback(func_801417B0);
    ObjectSetQuipObjectCallback(func_801418BC);
    ObjectSetSpecialObjectCallback(func_80141AC0);
    SetLockedStringCallback(func_80141B78);
    SetUseStringCallback(func_80141B88);

    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (type == SUBWAY2_TYPE_BOMB) {
            D_801420B0 = i;
            if (gObjectives == NULL || !(gObjectives->completed & 1)) {
                rt->flags0 |= 0x10;
            }
        } else if (type == SUBWAY2_TYPE_DEVICE) {
            D_801420B4 = i;
            if (gObjectives == NULL || !(gObjectives->completed & 2)) {
                health = rt->healthInfo;
                health->armor = 0x7FFF;
                health->health = 0x7FFF;
            }
        } else if (type == GOBJ_TYPE_BOMB) {
            D_801420AC = ObjectMatchToRefID(i, GOBJ_TYPE_COP);
            linkedType = GOBJ_TYPE(D_801420AC->id);
            rt->flags0 |= 0x20;
            if (D_801420AC->id == i || linkedType != GOBJ_TYPE_COP) {
                D_801420AC = NULL;
            } else {
                D_801420AC->flags23 |= 2;
            }
            /* reads through D_801420AC even when it was just cleared */
            if (D_801420AC->healthInfo->health <= 0) {
                gLevelInnocentsKilled++;
            }
        }
    }
    gDefaultLevelDmgFX = 11;
}

INCLUDE_RODATA("asm/subway2/nonmatchings/subway2", D_80140FE4);
