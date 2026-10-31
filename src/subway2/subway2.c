#include "common.h"
#include "level.h"

/* Object types specific to this level */
#define SUBWAY2_TYPE_BOMB_SWITCH 0x57
#define SUBWAY2_TYPE_BOMB 0x58
#define SUBWAY2_TYPE_DEVICE 0x76

/*
 * The bss items sit 4 bytes apart, as aspsx 2.56's .lcomm rule lays them out
 * (docs/data_migration.md).  aspsx 2.77 and maspsx pack them by size instead, so
 * both builds get explicit padding.
 */
#define BSS_PAD2(n) static s16 n;
#define BSS_PAD3(n) static u8 n; static s16 n##_2;

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0};

static char *D_80142040[2] = {
    "INCOMING FROM BENTON:\n\nYou're lucky to be alive. Ten square blocks of downtown D.C. "
    "just sank 20 feet into the ground.  It looks like Rhoemer had the entire subway wired to "
    "explode as part of his escape plan.  \n",
    "It's likely you've cut his time table short, and that he's still down there with you.  "
    "Aramov and Girduex also remain unaccounted for.  Don't let Rhoemer escape that terminal, "
    "and be aware that you're now completely cut off from all reinforcements.",
};
Briefing subway2Briefings = {
    {"Washington DC", "Georgia Street Subway", "08:23 23:45\n"}, 2, D_80142040,
};
static char *D_8014205C[3] = {
    "Locate explosives cache",
    "Blow open passage to street and protect CBDC agent",
    ".", /* replaced by gEliminateRhoemerMsg once the bomb is armed */
};
static char *D_80142068[2] = {"Do not kill any CBDC agent", "Do not damage any bomb"};
static char *D_80142070 = "C4 taken"; /* status messages */
static char *D_80142074 = "Need Explosives";
static char *D_80142078 = "Blocked Passage";
static char *D_8014207C = "Gas Mains";
static char *D_80142080 = "Gas Mains Closed";
/* the original .rdata ends in 3 stray bytes of linker padding */
static const char D_801412CD = 'r', D_801412CE = 'a', D_801412CF = 'm';
static LevelObjectives D_80142084 = {3, D_8014205C, 2, D_80142068}; /* saved objective/timer state */

static GameObjectRuntime *D_801420AC; /* cop linked to the bomb */
static s16 D_801420B0; BSS_PAD2(D_801420B0_pad) /* bomb object id */
static s16 D_801420B4; BSS_PAD2(D_801420B4_pad) /* device object id */
static u8 D_801420B8; BSS_PAD3(D_801420B8_pad)  /* countdown running */
static u8 D_801420BC; BSS_PAD3(D_801420BC_pad)  /* bomb armed */
extern s32 D_801077FC;
extern u8 gEliminateRhoemerMsg[];

static void func_801412D0();
static void func_80141400(s16 id, s16 by);
static void func_8014145C(s32 id, s16 by);
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

static s32 func_801417B0(s16 id) {
    int type;
    s32 ref;
    int found;
    GameObjectInfo *info;

    if (id != GOBJ_ID_666) {
        type = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type;
        if (type == 0x24 || type == 0x7B) {
            ref = gameObjectsInfo[id].refId;
            if (ref != -1) {
                found = ObjectInvisibleMatchToRefID(ref, 0x5D);
                if (found != ref) {
                    info = &gameObjectsInfo[(s16)found];
                    if (info->flags24 & 0x20) {
                        SysPlayXA(0x92, 0, 1);
                    }
                }
            }
            TextStatusMessage(D_80142080, 0);
            return 1;
        }
    }
    return 3;
}

static s32 func_801418BC(s16 id) {
    s32 ref;
    s32 ret;
    GameObjectInfo *info;
    int type;
    int found;

    ref = gameObjectsInfo[id].refId;
    ret = 1;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == 0x5D) {
            info = &gameObjectsInfo[(s16)ref];
            if (info->flags24 & 0x20) {
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
            if (found != ref) {
                info = &gameObjectsInfo[(s16)found];
                if (info->flags24 & 0x20) {
                    ret = 0;
                }
            }
        }
    }
    return ret;
}

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
    return (s32)D_80142074;
}

static s32 func_80141B88(s16 id, s16 kind) {
    if (kind == 3) {
        if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == SUBWAY2_TYPE_BOMB_SWITCH) {
            return D_801077FC;
        }
        return (s32)D_80142078;
    }
    if (kind == 4) {
        return (s32)D_8014207C;
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
    SaveObjectivesNTimersReinit(&D_80142084, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(2, -1);
    }
    if (mode == 3) {
        func_80141C34();
    } else {
        gSysMovieDoneCallback = func_80141C34;
    }
    gUsingSndBigFire = 1;
    D_80142084.primaryTexts[2] = (char *)gEliminateRhoemerMsg;
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

