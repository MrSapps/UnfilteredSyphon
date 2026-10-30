#include "common.h"
#include "level.h"
#include "system.h"

/* Object types specific to this level */
#define PARK_TYPE_HOSTAGE 0x01
#define PARK_TYPE_MEI_XING 0x04
#define PARK_TYPE_EXIT 0x0E

/* park.ovl statics, still defined in the overlay's asm .data/.bss */
extern void *D_80142880;          /* objective format string */
extern void *D_80142884;          /* "bombs" (plural) */
extern void *D_80142888;          /* "bomb" (last one) */
extern void *D_8014288C;          /* bombs-remaining status format */
extern void *D_80142890;          /* computer already used message */
extern void *D_80142894;          /* Mei Xing's name */
extern void **D_8014289C;         /* objective text slot */
extern u8 parkObjectives[];       /* saved objective/timer state */
extern u8 D_8014308C[];           /* fog scrim */
extern char D_801430A4[];         /* objective text buffer */
extern GameObjectRuntime *D_80143104; /* hostage */
extern u8 D_80143108;             /* bombs total */
extern u8 D_8014310C;             /* bombs defused */
extern u8 D_80143110;
extern u8 D_80143114;             /* Mei Xing hit warning played */
extern u8 D_80143118;             /* hostage lost */

extern s32 xaLevelStartIndex[];
extern void *gCommArrayMsg;
extern void *gHitNPCCallback;

typedef struct ParkCamera {
    /* 0x00 */ u8 unk00[0x8C];
    /* 0x8C */ EGSListNode *objects;
} ParkCamera;

typedef struct ParkDisplayObj {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* low 10 bits object id, 0x2000 = game object */
} ParkDisplayObj;

void LevelSetDirectionPointerFlag(s32 flag);
void LevelSetPointer(s32 id);
void ObjectDetonateBomb(GameObjectRuntime *rt);
void NPCCombatModeCallback(void (*cb)());
void NPCInitHostageTaker(s16 id);
void NPCShootHostage(s16 id);
void SysSoundQueueXA(s32 arg0, s32 track);
void JoystickDeactivate(void);
void StringContactMeiXingMsg(void);
void InterfaceCountdownReset(s32 arg0);
s16 InterfaceCountdownGet(void);
void InterfaceStartCountdownAfterGameCam(s16 seconds, s32 arg1, void (*done)());
void EGSDisplayRegisterFogScrim(void *scrim);
void ParticleStartRain(void);
GameObjectRuntime *ObjectMatchToRefIDByCatID(s16 id, s32 cat);
void SysSoundPlayPeriodicSong(s32 arg0, s32 song);

/* Written as separate tests so cc1 doesn't merge the bit checks into one mask. */
#define OBJECTIVE_DONE(n) (gObjectives != NULL && (gObjectives->completed & (1 << (n))))
#define OBJECTIVE_UNK20(n) (gObjectives != NULL ? (gObjectives->unk20 & (1 << (n))) : 0)

/* park calls ObjectBombDeath with only three arguments ($a3 is left unset). */
#define ObjectBombDeath3 ((void (*)(s32, s32, s32))ObjectBombDeath)

static void func_801413B8(void);
static void func_80141434(void);
static void func_801417C4(void);
static void func_801420B8(void);

/* Rebuild the "N bombs remaining" objective text. */
static void func_801413B8(void) {
    StringObjectiveObjectsRemaining2(D_801430A4, D_80142880, gRemainingMsg,
                                     D_8014310C != D_80143108 - 1 ? D_80142884 : D_80142888, D_8014310C,
                                     D_80143108);
}

/* Once all objectives are done, point the player at the exit. */
static void func_80141434(void) {
    int i;

    if (!gMissionFailed && OBJECTIVE_DONE(0) && OBJECTIVE_DONE(1) && OBJECTIVE_DONE(3)) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            if (i != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type == PARK_TYPE_EXIT) {
                LevelSetDirectionPointerFlag(1);
                LevelSetPointer(i);
                return;
            }
        }
    }
}

/* Countdown expired: every visible armed bomb goes off. */
static void func_8014152C(void) {
    EGSListNode *node;
    int id;
    GameObjectRuntime *rt;

    if (gObjectives == NULL || !(gObjectives->unk20 & 2)) {
        for (node = ((ParkCamera *)*gCamera)->objects; node != NULL; node = node->next) {
            id = ((ParkDisplayObj *)node->data)->id;
            if ((id & 0x2000) && (id &= 0x3FF) != GOBJ_ID_666) {
                if (gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == GOBJ_TYPE_BOMB) {
                    rt = gameObjectsInfo[id].runtime;
                    if (!(rt->flags1 & 0x80) && rt->healthInfo->health > 0) {
                        ObjectDetonateBomb(rt);
                    }
                }
            }
        }
        ObjectBombDeath3(2, 0x54, 0);
    }
}

static void func_8014165C(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == PARK_TYPE_MEI_XING) {
            if (!D_80143114 && (gObjectives == NULL || !(gObjectives->completed & 4))) {
                D_80143114 = 1;
                SysPlayXA(0x5E, 0, 1);
            }
        } else {
            NPCCopDamagedCallback(id, by);
            ObjectBombDamagedCallback(id, by);
        }
    }
}

static void func_80141788(void) {
    D_80143104->flags23 |= 8;
    LevelObjectiveComplete(1);
    func_80141434();
}

/* Hostage rescued. */
static void func_801417C4(void) {
    NPCCombatModeCallback(NULL);
    gHitNPCCallback = NULL;
    NPCInitHostageTaker(D_80143104->id);
    if (!D_80143118 && (gObjectives == NULL || !(gObjectives->completed & 2))) {
        SysSoundQueueXA(-1, 0x56);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
        func_80141788();
    }
}

static void func_80141870(s16 id, s16 by) {
    int type;
    s32 ref;

    type = GOBJ_TYPE((s16)id);
    if (type == PARK_TYPE_HOSTAGE) {
        if (!gDeviceDetonated) {
            if ((s16)id == gLevelSpecificNpcID) {
                SysSoundQueueXA(-1, 0x62);
                StringContactMeiXingMsg();
                LevelObjectiveComplete(3);
                func_80141434();
            } else if ((s16)id == D_80143104->id) {
                func_801417C4();
            }
        }
    } else if (type == GOBJ_TYPE_COP) {
        if (!gDeviceDetonated) {
            ref = gameObjectsInfo[(s16)id].refId;
            if (ref != -1 && ref != GOBJ_ID_666 &&
                gameObjectsModelInfo[gameObjectsInfo[ref].modelIndex].type == GOBJ_TYPE_BOMB) {
                NPCCopKilledWashington(id, by, ref, 0, 0);
            } else if (!D_80143118) {
                D_80143118 = 1;
                LevelMissionFailed(1, 0x58, 1);
            }
        }
    } else if (type == GOBJ_TYPE_BOMB) {
        if (!OBJECTIVE_UNK20(1) && !OBJECTIVE_UNK20(2) &&
            !(gameObjectsInfo[(s16)id].runtime->flags1 & 0x80)) {
            InterfaceCountdownReset(0);
            ObjectBombDeath3(1, 0x82, 0);
        }
    } else if (type == PARK_TYPE_MEI_XING) {
        if (gObjectives == NULL || (!(gObjectives->completed & 4) && !(gObjectives->unk20 & 8))) {
            SysSoundQueueXA(-1, 0x5F);
            StringContactMeiXingMsg();
            LevelMissionFailed(3, -1, 0);
        }
    }
}

static s32 func_80141B58(void) {
    if (!gMissionFailed && OBJECTIVE_DONE(0) && OBJECTIVE_DONE(1) && OBJECTIVE_DONE(3)) {
        SysPlayXA(0x63, 0, 1);
        return 1;
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        SysPlayXA(0x64, 0, 1);
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void func_80141C0C(s16 id) {
    if (!D_80143118 && id == D_80143104->id) {
        NPCCombatModeCallback(NULL);
        NPCShootHostage(D_80143104->id);
    }
}

/* Bomb defused. */
static void func_80141C6C(s16 id, s16 by) {
    GameObjectRuntime *cop;
    s16 linked;
    u8 defused;

    cop = ObjectMatchToRefID(id, GOBJ_TYPE_COP);
    if (by == gPlayerOffset) {
        linked = cop->id;
        if (linked != id && linked != GOBJ_ID_666 &&
            gameObjectsModelInfo[gameObjectsInfo[linked].modelIndex].type == GOBJ_TYPE_COP) {
            SysPlayXA(0x59, 0, 1);
            NPCHazmatStart(cop->id);
            return;
        }
    }
    defused = ++D_8014310C;
    if (gObjectives == NULL || !(gObjectives->unk20 & 4)) {
        if (defused < D_80143108) {
            StringInteractObjectsRemaining(D_8014288C, D_80143108 - defused);
            func_801413B8();
            SysPlayXA(0x5A, 0, 1);
            if (!gMissionFailed) {
                LevelMissionCtrMsg(gCheckpointMsg, 0);
                SaveCreateContinue();
            }
        } else {
            InterfaceCountdownAbort();
            func_801413B8();
            SysPlayXA(0x6B, 0, 1);
            LevelObjectiveComplete(0);
            func_80141434();
        }
    }
}

static s32 func_80141E30(void) {
    if (gObjectives == NULL || !(gObjectives->unk20 & 8)) {
        LevelObjectiveAdd(3, -1);
        gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 &= ~2;
        SysSoundQueueXA(-1, 0x60);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
        LevelObjectiveComplete(2);
    } else {
        TextStatusMessage(D_80142890, 0);
    }
    return 1;
}

static s32 func_80141EFC(s16 id) {
    s32 xa;

    xa = *(s32 *)((u8 *)&gameObjectsInfo[id] + 0x28);
    if (xa >= 100) {
        xa -= 100;
    }
    if (0x5D - xaLevelStartIndex[gSysInfo.level] == xa && gObjectives != NULL) {
        gObjectives->unk1C |= 4;
    }
    return 1;
}

void iMuseumInit(s16 id) {
    s16 ref;
    s16 type;

    ref = gameObjectsInfo[id].refId;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == PARK_TYPE_MEI_XING) {
            LevelObjectiveAdd(2, id);
            if (gObjectives != NULL) {
                gObjectives->unk24 |= 8;
                gObjectives->unk1C &= ~4;
            }
        }
    } else {
        LevelObjectiveAdd(1, id);
    }
}

static void *func_80142094(s16 id, s16 kind) {
    if (kind == 1) {
        return gCommArrayMsg;
    }
    return NULL;
}

static void func_801420B8(void) {
    SysPlaySong(0, 0x16, 0, 0);
    SysSoundPlayPeriodicSong(0, 0x17);
}

#ifdef NON_MATCHING
/*
 * One instruction short: the original copies ObjectMatchToRefIDByCatID's result
 * into $v1 before the healthInfo load, and keeps `type` in $v1 rather than $a1.
 */
void iParkInit(s32 mode) {
    int i;
    int type;
    int refType;
    s32 ref;
    GameObjectRuntime *npc; /* hostage, or the cop holding one */
    GameObjectRuntime *rt;
    GameObjectRuntime *cop;

    EGSDisplayRegisterFogScrim(D_8014308C);
    D_80143104 = NULL;
    D_80143108 = 0;
    D_8014310C = 0;
    D_80143110 = 0;
    D_80143114 = 0;
    D_80143118 = 0;
    InterfaceViralRadarInit();
    ParticleStartRain();
    *D_8014289C = D_801430A4;
    SaveObjectivesNTimersReinit(parkObjectives, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(4, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 7;
        }
        InterfaceStartCountdownAfterGameCam(1200, 0, func_8014152C);
    } else if (gObjectives == NULL || !(gObjectives->completed & 1)) {
        InterfaceStartCountdownAfterGameCam(InterfaceCountdownGet() / 20, 0, func_8014152C);
    }
    if (mode == 3) {
        func_801420B8();
    } else {
        gSysMovieDoneCallback = func_801420B8;
    }
    DamageSetDamagedCallback(func_8014165C);
    DamageSetDestroyedCallback((void (*)(s16, s16))func_80141870);
    LevelSetEOLCallback(func_80141B58);
    if (gObjectives == NULL || !(gObjectives->completed & 2)) {
        NPCCombatModeCallback(func_80141C0C);
    }
    UseSetBombCallback(func_80141C6C);
    UseSetComputerCallback(func_80141E30);
    ObjectSetQuipObjectCallback(func_80141EFC);
    ObjectSetSpecialObjectCallback(iMuseumInit);
    SetUseStringCallback(func_80142094);

    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == PARK_TYPE_HOSTAGE) {
            ref = gameObjectsInfo[i].refId;
            if (ref != -1 && GOBJ_TYPE(ref) == GOBJ_TYPE_COP) {
                npc = gameObjectsInfo[i].runtime;
                D_80143104 = npc;
                if (gObjectives == NULL || !(gObjectives->completed & 2)) {
                    NPCInitHostageTaker(npc->id);
                } else {
                    do {
                        type = GOBJ_TYPE(ref);
                        if (type == GOBJ_TYPE_COP) {
                            gameObjectsInfo[ref].runtime->flags23 |= 2;
                        }
                        ref = gameObjectsInfo[ref].refId;
                    } while (ref != -1);
                }
            }
        } else if (type == GOBJ_TYPE_COP) {
            ref = gameObjectsInfo[i].refId;
            if (ref == -1 || GOBJ_TYPE(ref) == GOBJ_TYPE_COP) {
                npc = ObjectMatchToRefIDByCatID(i, 2);
                if (npc->healthInfo->health <= 0) {
                    D_80143118 = 1;
                }
            }
        } else if (type == GOBJ_TYPE_BOMB) {
            rt = gameObjectsInfo[i].runtime;
            D_80143108++;
            if (rt->flags1 & 0x80) {
                D_8014310C++;
            }
            cop = ObjectMatchToRefID(i, GOBJ_TYPE_COP);
            type = GOBJ_TYPE(cop->id);
            if (cop->id != i && type == GOBJ_TYPE_COP && (*(u32 *)rt & 0x8020) != 0x20) {
                cop->flags23 |= 2;
            }
            if (cop->healthInfo->health <= 0) {
                gLevelInnocentsKilled++;
            }
        }
    }
    D_80143108--;
    func_801413B8();
    if (gLevelSpecificNpcID != -1) {
        if (gameObjectsInfo[gLevelSpecificNpcID].runtime->unk20[2] == 2) {
            gLevelSpecificNpcName = D_80142894;
            gameObjectsInfo[gLevelSpecificNpcID].runtime->npc->nameMsgId = 0xB;
            if (gObjectives == NULL || !(gObjectives->completed & 4)) {
                gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 |= 2;
            }
        }
    }
    gDefaultLevelDmgFX = 0xB;
}
#else
INCLUDE_ASM("asm/park/nonmatchings/park", iParkInit);
#endif

INCLUDE_RODATA("asm/park/nonmatchings/park", D_80140FE4);
