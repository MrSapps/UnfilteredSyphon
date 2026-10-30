#include "common.h"
#include "level.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

/* Object types specific to this level */
#define MUSEUM_TYPE_SWITCH_41 0x41
#define MUSEUM_TYPE_KEYPAD 0x1C
#define MUSEUM_TYPE_PATH_SWITCH 0x22
#define MUSEUM_TYPE_PHAGAN 0x4C
#define MUSEUM_TYPE_56 0x56
#define MUSEUM_TYPE_PLANE_ANCHOR 0x44

/* museum.ovl statics, still defined in the overlay's asm .data/.bss */
extern u8 D_80143E18[]; /* texture animation sources/targets */
extern u8 D_80143E1C[];
extern u8 D_80143E20[];
extern u8 D_80143E24[];
extern u8 D_80143E28[];
extern u8 D_80143E2C[];
extern u8 D_80143E30[];
extern u8 D_80143E54[];
extern u8 D_80143E78[];
extern u8 D_80143E9C[];
extern u8 D_80143EC0[];
extern u8 D_80143EE4[];
extern void *D_80143E10; /* "Benton" */
extern void *D_80143E14; /* "Gate Control Panel" */
extern u8 D_80143F08[];  /* museum saved objective state */
extern u8 D_80143F30[];  /* museum2 saved objective state */
extern s32 D_80143F58[]; /* XA tracks of the Aramov taunts */
extern u32 D_80143F80;   /* next taunt */
extern s16 D_8014405C;   /* object id of the path mover */
extern u8 D_80144060;    /* lock code hint played */
extern u8 D_80144064;    /* hostage shot */

extern u32 D_80105F58[][8]; /* per-weapon info, 0x20 bytes each */
extern void *gElevatorCallSwitchMsg;
extern void *gAramovMsg;
extern void *gHitNPCCallback;

typedef struct TexAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 flags;
} TexAnim;

/* Path/physics state hanging off GameObjectRuntime+0x0C */
typedef struct PathState {
    /* 0x000 */ VECTOR32 cur; /* cur.vy is the progress along the path */
    /* 0x010 */ u8 unk010[0x10];
    /* 0x020 */ s32 velX;
    /* 0x024 */ s32 velY;
    /* 0x028 */ s32 velZ;
    /* 0x02C */ u8 unk02C[0xC4];
    /* 0x0F0 */ VECTOR32 push;
    /* 0x100 */ u8 unk100;
    /* 0x101 */ u8 unk101;
    /* 0x102 */ u8 unk102[6];
    /* 0x108 */ u8 unk108;
    /* 0x109 */ u8 unk109[0x23];
    /* 0x12C */ s32 endProgress;
    /* 0x130 */ u8 unk130[0x44];
    /* 0x174 */ s32 point;
    /* 0x178 */ u8 unk178[4];
    /* 0x17C */ s32 active;
    /* 0x180 */ u8 unk180[0x24];
    /* 0x1A4 */ u32 flags;
} PathState;

typedef struct MuseumAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ s32 unk18;
} MuseumAnim;

/* GameObjectRuntime as seen by the moving objects of this level */
typedef struct MuseumMover {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1; /* low 3 bits: state */
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 sector;
    /* 0x08 */ MuseumAnim *anim;
    /* 0x0C */ PathState *path;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ struct NPCInfo *npc;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 flags23;
} MuseumMover;

/* Objects per display sector */
typedef struct ActivationList {
    /* 0x00 */ s32 count;
    /* 0x04 */ GameObjectInfo **infos;
} ActivationList;

#define MOVER_STATE(m) ((m)->flags1 & 7)
#define MOVER_SET_STATE(m, s) ((m)->flags1 = ((m)->flags1 & 0xF8) | (s))
#define IN_RANGE(x) ((u32)((x) + 0x2000) <= 0x4000)

static inline u8 PathIsStopped(PathState *path) {
    if (((path->unk108 && (path->flags & 0x100000)) || !path->unk101) && IN_RANGE(path->velX) &&
        IN_RANGE(path->velY) && IN_RANGE(path->velZ)) {
        return 1;
    }
    return 0;
}

extern ActivationList *objActivationList;
extern VECTOR32 gExploDir;
extern u8 D_80143F84[]; /* target info (bss) */

#define RT_PATH(rt) (*(PathState **)&(rt)->move)

s32 PathGetNumPoints(GameObjectRuntime *rt);
void PathSetPositionToPoint(GameObjectRuntime *rt, s32 point);
void ObjectSetAmbientSndIdxCallback(s32 (*cb)());
TexAnim *EGSDisplayInitTextureAnimation(void *a, void *b, s32 c, s32 d, s32 e);
void NPCCombatModeCallback(void (*cb)());
void NPCSetAwarenessLevel(GameObjectRuntime *rt, s32 level, s32 arg2, s32 arg3);
void SysSoundQueueXA(s32 arg0, s32 track);
void JoystickDeactivate(void);
void NPCInitHostageTaker(s16 id);
void NPCShootHostage(s16 id);
void NPCDropGun(s16 id);
void SndSysCheckXADoneCallback(s32 arg);
void MovementAddGobj(MuseumMover *m);
s32 EGSDisplayGetAnimSector(MuseumAnim *anim);
void CobjAddBulletCollisionBoxGobj(MuseumMover *m, s32 a, s32 b, s32 c);
void CobjAddCollisionBoxGobj(MuseumMover *m, s32 a, s32 b, s32 c, s32 d);
void CobjRemoveBulletCollisionBoxGobj(MuseumMover *m);
void CobjRemoveCollisionBoxGobj(MuseumMover *m);
void PathGetPoint(MuseumMover *m, s32 arg1, VECTOR32 *out);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void DamageGOBJ(s32 arg0, s16 from, s16 to, s32 amount, s32 type);
void ParticleSmoke(s32 arg0, VECTOR32 *pos, VECTOR32 *dir, s32 arg3, s32 arg4);
void MovementRemoveGobj(MuseumMover *m);
void MovementGobjInit(MuseumMover *m, s32 a, s32 b, s32 c, s32 d);
void PhysicsGobjBasicInit(MuseumMover *m, s32 a, s32 b);
void CollisionGobjBasicInit(MuseumMover *m, s32 a, s32 b, s32 c, s32 d, s32 e);
void TargetInitGobj(MuseumMover *m, void *info, s32 a, s32 b);
void func_801463A4(s16 id, s32 arg1); /* LevelCreateGOBJ (init.ovl) */
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos, VECTOR32 *move);
/* museum passes only the first three arguments */
#define EGSFrameSetPosition3 ((void (*)(EGSFrame *, s32, VECTOR32 *))EGSFrameSetPosition)
void PlaneFallSetup(MuseumMover *m, s16 id);
void PathHandleEx(MuseumMover *m, VECTOR32 *scale, s32 a, s32 b, s32 c, s32 d, s32 *out);
void CobjAddWindowGobj(MuseumMover *m, s32 a, s32 b, s32 c);
void CobjRemoveWindowGobj(MuseumMover *m);
void func_80147AAC(MuseumMover *m, s32 a, s32 b, s32 c); /* init.ovl */
extern VECTOR32 gObjPathScaleVector;
void WeaponDrop(GameObjectRuntime *rt, s32 arg1);
void WeaponSetTaserHitNpcCallback(void (*cb)());
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);

static void func_80141744(void);
static void func_80142650(s32 arg0);
static void func_80142A84(void);
static void func_801417CC(s16 reason);

static void func_80141608(void) {
    EGSDisplayInitTextureAnimation(D_80143E18, D_80143E30, 9, 0x3C, 0)->flags = 0x20;
}

static void func_80141644(void) {
    EGSDisplayInitTextureAnimation(D_80143E2C, D_80143EE4, 9, 0x3C, 5)->flags = 0x20;
}

static void func_80141684(void) {
    EGSDisplayInitTextureAnimation(D_80143E1C, D_80143E54, 9, 0x3C, 1)->flags = 0x20;
}

static void func_801416C4(void) {
    EGSDisplayInitTextureAnimation(D_80143E24, D_80143E9C, 9, 0x3C, 3)->flags = 0x20;
}

static void func_80141704(void) {
    EGSDisplayInitTextureAnimation(D_80143E20, D_80143E78, 9, 0x3C, 2)->flags = 0x20;
}

/* The level goes on alert: stop watching for the player being spotted. */
static void func_80141744(void) {
    s32 ref;

    ref = gameObjectsInfo[D_8014405C].refId;
    if (ref != -1) {
        SysPostMessage(0x14, 1, D_8014405C, ref, 0, 0, 0, 0);
    }
    DamageSetDamagedCallback(NULL);
    NPCCombatModeCallback(NULL);
    WeaponSetWeaponFiredCallback(NULL);
    npcSetLevelAlert();
}

/* Player was spotted: Phagan flees and the mission fails. */
static void func_801417CC(s16 reason) {
    GameObjectRuntime *phagan;

    func_80141744();
    if (!gMissionFailed && (gObjectives == NULL || !(gObjectives->completed & 1))) {
        phagan = gameObjectsInfo[gLevelSpecificNpcID].runtime;
        if (phagan->healthInfo->maxHealth > 0) {
            *(u8 *)&gameObjectsInfo[phagan->id].flags24 = 4;
            CASRequestAction(phagan->id, 0x25);
            phagan->npc->nameMsgId = 0x50;
            NPCSetAwarenessLevel(phagan, 2, 1, 0);
        }
        if (gObjectives != NULL) {
            gObjectives->unk14 |= 1;
        }
        LevelMissionFailed(reason, 0x47, 0);
    }
}

static void func_80141908(s16 id, s16 by) {
    if (by == gPlayerOffset && !gMissionFailed && id == gLevelSpecificNpcID) {
        func_801417CC(0);
    }
}

static void func_80141964(s16 id, s16 by) {
    if (!gMissionFailed && id == gLevelSpecificNpcID) {
        func_801417CC(0);
    } else if (id == gLevelSpecificNpcID2) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysSoundQueueXA(-1, 0x35);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
    }
}

static s32 func_80141A14(void) {
    if (gMissionFailed) {
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
        return 0;
    }
    return 1;
}

static void func_80141A54(s16 id) {
    if (!gMissionFailed && id == gLevelSpecificNpcID) {
        SysExeTickQueueTimer((void (*)(s32))func_801417CC, 2, 0);
    }
}

static s32 func_80141AA0(s16 id) {
    s32 ref;

    if (id != GOBJ_ID_666 &&
        gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == MUSEUM_TYPE_SWITCH_41) {
        ref = gameObjectsInfo[gameObjectsInfo[id].runtime->id].refId;
        if (ref != -1 && !(gameObjectsInfo[ref].runtime->flags0 & 8)) {
            SysPostMessage(0x23, 3, id, ref, 0, 0, 0, 0);
        }
        return 2;
    }
    return 3;
}

static s32 func_80141BA8(s16 id) {
    s32 ref;
    u8 ret;

    ref = gameObjectsInfo[id].refId;
    ret = 1;
    if (ref != -1) {
        ret = (*(s32 *)gameObjectsInfo[ref].runtime & 0x22) != 0x20;
    }
    return ret;
}

INCLUDE_RODATA("asm/museum/nonmatchings/museum", D_80140FE4);

static void func_80141C20(s16 id) {
    if (!gMissionFailed) {
        gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 |= 2;
        ObjectDoMovieEx("MUSEUM", 0x2D, id, id);
    }
}

/* Meeting movie finished: back to the player, Benton is now in play. */
static void func_80141C98(s16 id) {
    gameObjectsInfo[gLevelSpecificNpcID2].runtime->flags23 &= ~2;
    ObjectTeleportPlayer(id, 0xC);
    GameCameraRequestCutToBehind();
    func_80141744();
    SysPlayXA(0x49, 0, 1);
    LevelObjectiveAdd(2, -1);
    LevelObjectiveComplete(0);
}

static s32 func_80141D2C(void) {
    return 0xC;
}

static void *func_80141D34(s16 id) {
    if (id == gameObjectsInfo[gLevelSpecificNpcID2].refId && !D_80144060) {
        D_80144060 = 1;
        SysPlayXA(0x36, 0, 1);
    }
    return gLockCodeRequiredMsg;
}

static void *func_80141DBC(s16 id, s16 kind) {
    if (kind != 4) {
        return NULL;
    }
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == MUSEUM_TYPE_KEYPAD) {
        return D_80143E14;
    }
    return gElevatorCallSwitchMsg;
}

static void func_80141E50(s16 id) {
    if (gameObjectsInfo[id].refId == gameObjectsInfo[gLevelSpecificNpcID2].refId) {
        SysPlayXA(0x37, 0, 1);
        D_80144060 = 1;
        LevelObjectiveAdd(1, -1);
    }
    if (gLevelKeycards != gLevelKeycardsTaken) {
        if (!gMissionFailed) {
            LevelMissionCtrMsg(gCheckpointMsg, 0);
            SaveCreateContinue();
        }
    } else {
        LevelObjectiveComplete(1);
    }
}

/* Firing a loud weapon alerts Phagan. */
static void func_80141F40(s32 arg0, s16 weapon) {
    if (!gMissionFailed && !((D_80105F58[weapon][0] >> 14) & 1)) {
        func_801417CC(0);
    }
}

/* iMuseumInit */
static void func_80141F94(void) {
    int i;
    s16 type;
    s16 ref;
    s32 bit;
    s32 count;
    s32 point;
    GameObjectRuntime *rt;

    D_8014405C = -1;
    D_80144060 = 0;
    SaveObjectivesNTimersReinit(D_80143F08, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        if (gObjectives != NULL) {
            count = gObjectives->count;
            for (bit = 0; bit < count; bit++) {
                if (gObjectives != NULL) {
                    gObjectives->unk24 |= 1 << bit;
                }
            }
        }
    }
    SysStopSong(0, 0xC);
    DamageSetDestroyedCallback(func_80141964);
    if (gObjectives == NULL || !(gObjectives->completed & 1)) {
        DamageSetDamagedCallback(func_80141908);
        NPCCombatModeCallback(func_80141A54);
        WeaponSetWeaponFiredCallback(func_80141F40);
    } else {
        npcSetLevelAlert();
    }
    LevelSetEOLCallback(func_80141A14);
    UseSetSwitchCallback(func_80141AA0);
    ObjectSetQuipObjectCallback(func_80141BA8);
    ObjectSetSpecialObjectCallback(func_80141C20);
    ObjectSetMovieDoneCallback(func_80141C98);
    ObjectSetAmbientSndIdxCallback(func_80141D2C);
    SetLockedStringCallback(func_80141D34);
    SetUseStringCallback(func_80141DBC);
    PickupSetKeyCallback(func_80141E50);
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == MUSEUM_TYPE_PATH_SWITCH) {
            ref = gameObjectsInfo[i].refId;
            if (ref != -1 && GOBJ_TYPE(ref) != 0x77 && (rt = gameObjectsInfo[ref].runtime) != NULL &&
                rt->unk20[2] == 9) {
                if (gObjectives == NULL || !(gObjectives->completed & 1)) {
                    rt->flags0 |= 5;
                    point = PathGetNumPoints(rt) - 1;
                    if (RT_PATH(rt) != NULL) {
                        RT_PATH(rt)->active = 1;
                        RT_PATH(rt)->point = point;
                    }
                    PathSetPositionToPoint(rt, point);
                } else {
                    rt->flags0 &= ~5;
                    if (RT_PATH(rt) != NULL) {
                        RT_PATH(rt)->active = 0;
                        RT_PATH(rt)->point = 0;
                    }
                    PathSetPositionToPoint(rt, 0);
                }
                D_8014405C = i;
            }
        } else if (type == 0x63 && !D_80144060 && (gameObjectsInfo[i].runtime->flags0 & 0x20)) {
            D_80144060 = 1;
        }
    }
    if (gLevelSpecificNpcID != -1) {
        gLevelSpecificNpcName = gPhaganMsg;
        gameObjectsInfo[gLevelSpecificNpcID].runtime->npc->nameMsgId = 9;
        if (gObjectives != NULL && (gObjectives->completed & 1)) {
            gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 |= 2;
        }
    }
    if (gLevelSpecificNpcID2 != -1) {
        gLevelSpecificNpc2Name = D_80143E10;
        gameObjectsInfo[gLevelSpecificNpcID2].runtime->npc->nameMsgId = 0x32;
        if (gObjectives == NULL || !(gObjectives->completed & 1)) {
            gameObjectsInfo[gLevelSpecificNpcID2].runtime->flags23 |= 2;
        }
    }
    gDefaultLevelDmgFX = 11;
}

static void func_80142524(s32 track) {
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
    SysPlayXA(track, 0, 1);
}

static void func_80142570(s16 id, s16 by) {
    if (!gMissionFailed) {
        if (id == gLevelSpecificNpcID) {
            func_80142524(0x42);
            if (gObjectives != NULL) {
                gObjectives->unk14 |= 8;
            }
            LevelMissionFailed(0, -1, 0);
        }
        if (!gMissionFailed && id == gLevelSpecificNpcID2) {
            func_80142524(0x41);
            if (gObjectives != NULL) {
                gObjectives->unk14 |= 8;
            }
            LevelMissionFailed(1, -1, 0);
        }
    }
}

/* Aramov shoots Phagan. */
static void func_80142650(s32 arg0) {
    GameObjectRuntime *aramov;

    if (!D_80144064 && (gObjectives == NULL || !(gObjectives->completed & 8))) {
        aramov = gameObjectsInfo[gLevelSpecificNpcID2].runtime;
        D_80144064 = 1;
        NPCSetAwarenessLevel(aramov, 2, 1, 0);
        NPCShootHostage(aramov->id);
        gHitNPCCallback = NULL;
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysExeTickQueueTimer(func_80142650, 0, 0);
    }
}

static void func_80142730(void) {
    NPCInitHostageTaker(gLevelSpecificNpcID2);
    gameObjectsInfo[gLevelSpecificNpcID].runtime->npc->nameMsgId = 9;
    SysExeTickQueueTimer(func_80142650, 0, 0);
    gHitNPCCallback = NULL;
}

/* Aramov gives up. */
static void func_801427AC(void) {
    NPCDropGun(gLevelSpecificNpcID2);
    if (!D_80144064 && (gObjectives == NULL || !(gObjectives->completed & 8))) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysPlayXA(0x45, 0, 1);
        gameObjectsInfo[gLevelSpecificNpcID2].runtime->npc->flags |= 0x1000000;
        CASRequestAction(gLevelSpecificNpcID2, 0x60);
        LevelObjectiveComplete(3);
    }
}

/* Aramov tasered. */
static void func_80142890(s16 id) {
    if (id == gLevelSpecificNpcID2 && gameObjectsInfo[gLevelSpecificNpcID].runtime->healthInfo->maxHealth > 0 &&
        (gObjectives == NULL || !(gObjectives->completed & 8))) {
        func_801427AC();
        func_80142730();
    }
}

static s32 func_80142940(void) {
    if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 8)) {
        SysPlayXA(0x43, 0, 1);
        return 1;
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void func_801429D8(s16 id) {
    if (id == gLevelSpecificNpcID2) {
        func_80142650(0);
    }
}

/* Play the next Aramov taunt. */
static void func_80142A0C(void) {
    SysPlayXA(D_80143F58[D_80143F80], 0, 1);
    D_80143F80++;
    if (D_80143F80 < 10) {
        SysExeTickQueueTimer(SndSysCheckXADoneCallback, 0x14, (s32)func_80142A84);
    }
}

static void func_80142A84(void) {
    if (!gMissionFailed && (gObjectives == NULL || !(gObjectives->completed & 8))) {
        SysExeTickQueueTimer((void (*)(s32))func_80142A0C, 1, 0);
    }
}

static void func_80142AE4(s16 id) {
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == MUSEUM_TYPE_PATH_SWITCH) {
        ObjectDoMovieEx("MUSEUM2", 0x2D, id, id);
        return;
    }
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
    D_80143F80 = 0;
    func_80142A84();
    SysExeTickQueueTimer(func_80142650, 0x17C, 0);
}

static void func_80142BB4(s16 id) {
    s32 ref;
    GameObjectRuntime *rt;

    ObjectTeleportPlayer(id, 6);
    ref = gameObjectsInfo[id].refId;
    if (ref != -1) {
        rt = gameObjectsInfo[ref].runtime;
        rt->healthInfo->maxHealth = -1;
        rt->flags23 |= 2;
    }
    GameCameraRequestCutToBehind();
}

static void *func_80142C58(void) {
    return gLockCodeRequiredMsg;
}

static void *func_80142C68(s16 id, s16 kind) {
    void *msg;

    msg = NULL;
    if (kind == 4 && id != GOBJ_ID_666 &&
        gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == MUSEUM_TYPE_KEYPAD) {
        msg = D_80143E14;
    }
    return msg;
}

static void func_80142CF0(void) {
    if (gLevelKeycards != gLevelKeycardsTaken) {
        if (!gMissionFailed) {
            LevelMissionCtrMsg(gCheckpointMsg, 0);
            SaveCreateContinue();
        }
    } else {
        LevelObjectiveComplete(1);
    }
}

/* Hit callback for Aramov. */
static void func_80142D58(s16 id, s32 flags) {
    if (id == gLevelSpecificNpcID2) {
        if (flags & 0x8100) {
            if (gameObjectsInfo[gLevelSpecificNpcID].runtime->healthInfo->maxHealth > 0 &&
                (gObjectives == NULL || !(gObjectives->completed & 8))) {
                func_801427AC();
                func_80142730();
            }
        } else if (flags & 1) {
            if (gameObjectsInfo[gLevelSpecificNpcID].runtime->healthInfo->maxHealth > 0) {
                func_80142730();
            }
        } else {
            func_80142650(0);
        }
    }
}

void iMuseum2Init(void) {
    int i;
    s16 type;
    s32 bit;
    s32 count;
    GameObjectRuntime *phagan;
    GameObjectRuntime *aramov;
    s16 id;

    EGSDisplayInitTextureAnimation(D_80143E28, D_80143EC0, 9, 0x3C, 4)->flags = 0x20;
    EGSDisplayQueueInterrupt(func_80141608, 10, 0);
    EGSDisplayQueueInterrupt(func_80141644, 20, 0);
    EGSDisplayQueueInterrupt(func_80141684, 30, 0);
    EGSDisplayQueueInterrupt(func_801416C4, 40, 0);
    EGSDisplayQueueInterrupt(func_80141704, 50, 0);
    D_80144064 = 0;
    SaveObjectivesNTimersReinit(D_80143F30, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(1, -1);
        LevelObjectiveAdd(3, -1);
        if (gObjectives != NULL) {
            count = gObjectives->count;
            for (bit = 0; bit < count; bit++) {
                if (gObjectives != NULL) {
                    gObjectives->unk24 |= 1 << bit;
                }
            }
        }
    }
    if (gObjectives == NULL || !(gObjectives->completed & 8)) {
        WeaponSetTaserHitNpcCallback(func_80142890);
        NPCCombatModeCallback(func_801429D8);
        gHitNPCCallback = func_80142D58;
    }
    DamageSetDestroyedCallback(func_80142570);
    LevelSetEOLCallback(func_80142940);
    ObjectSetSpecialObjectCallback(func_80142AE4);
    ObjectSetMovieDoneCallback(func_80142BB4);
    SetLockedStringCallback(func_80142C58);
    SetUseStringCallback(func_80142C68);
    PickupSetKeyCallback(func_80142CF0);
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == MUSEUM_TYPE_PHAGAN) {
            if (gLevelSpecificNpcID == -1) {
                gLevelSpecificNpcID = i;
            }
        } else if (type == MUSEUM_TYPE_56) {
            gameObjectsInfo[i].runtime->anim->unk00[0xA] |= 0x20;
            gameObjectsInfo[i].runtime->anim->model->flags |= 0x8000;
        }
    }
    if (gLevelSpecificNpcID != -1) {
        gLevelSpecificNpcName = gPhaganMsg;
        phagan = gameObjectsInfo[gLevelSpecificNpcID].runtime;
        phagan->flags23 |= 1;
        aramov = ObjectMatchToRefID(phagan->id, 1);
        id = aramov->id;
        if (id != phagan->id) {
            gLevelSpecificNpcID2 = id;
            gLevelSpecificNpc2Name = gAramovMsg;
            aramov->flags23 |= 1;
            if (gObjectives == NULL || !(gObjectives->completed & 8)) {
                NPCInitHostageTaker(gLevelSpecificNpcID2);
            } else if (aramov->healthInfo->maxHealth > 0) {
                CASRequestAction(gLevelSpecificNpcID2, 0x60);
                aramov->npc->flags |= 0x1000000;
                WeaponDrop(aramov, 0);
                *(u8 *)&gameObjectsInfo[aramov->id].flags24 = 0;
            }
        }
    }
    gDefaultLevelDmgFX = 11;
}

#ifdef NON_MATCHING
/* Only the prologue differs: cc1 schedules the $ra save after the id sign extension. */
#define HALF(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))

/* Start the plane falling: it drops at the midpoint of the two anchor objects on either side. */
void PlaneFallSetup(MuseumMover *m, s16 id) {
    VECTOR32 vel;
    VECTOR32 pos;
    s16 ends[2];
    int i;
    s16 tmp;
    GameObjectInfo *a;
    GameObjectInfo *b;
    s32 *ta;
    s32 *tb;

    ends[0] = -1;
    ends[1] = -1;
    if (m->id != id) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            if (i != id && i != GOBJ_ID_666 &&
                gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type == MUSEUM_TYPE_PLANE_ANCHOR) {
                if (ends[0] == -1) {
                    ends[0] = i;
                } else if (ends[1] == -1) {
                    ends[1] = i;
                }
            }
        }
        if (ends[0] < id && id < ends[1]) {
            tmp = ends[0];
            ends[0] = ends[1];
            ends[1] = tmp;
        }
        a = &gameObjectsInfo[ends[0]];
        b = &gameObjectsInfo[ends[1]];
        ta = a->matrix.t;
        tb = b->matrix.t;
        pos.vx = a->matrix.t[0] + b->matrix.t[0];
        pos.vy = a->matrix.t[1] + b->matrix.t[1];
        pos.vz = a->matrix.t[2] + b->matrix.t[2];
        pos.vx = HALF(pos.vx);
        pos.vy = HALF(pos.vy);
        pos.vz = HALF(pos.vz);
        vel.vx = ta[0] - tb[0];
        vel.vy = ta[1] - tb[1];
        vel.vz = ta[2] - tb[2];
        vel.vx *= 1000;
        vel.vy *= 1000;
        vel.vz *= 1000;
    } else {
        vel.vx = (u16)vel.vx;
        vel.vy = (u16)vel.vy;
        vel.vz = (u16)vel.vz;
        pos.vx = (u16)pos.vx;
        pos.vy = (u16)pos.vy;
        pos.vz = (u16)pos.vz;
    }
    SysPlaySong(0, 0x18, 0, (s32)&pos);
    MovementAddGobj(m);
    m->path->push.vx += vel.vx;
    m->path->push.vy += vel.vy;
    m->path->push.vz += vel.vz;
    m->sector = EGSDisplayGetAnimSector(m->anim);
}
#else
INCLUDE_ASM("asm/museum/nonmatchings/museum", PlaneFallSetup);
#endif

/* Message handler of the falling plane. */
void TGTMUSEUMObjDispatch(SysMsg *msg) {
    MuseumMover *m;
    MuseumMover *other;
    PathState *path;
    ActivationList *list;
    VECTOR32 v;
    VECTOR32 cur;
    VECTOR32 t;
    s32 dist[2];
    VECTOR32 p;
    s32 i;
    u8 done;

    m = (MuseumMover *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(m, 1, 0, 0);
        CobjAddCollisionBoxGobj(m, 1, 0, 1, 0);
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(m);
        CobjRemoveCollisionBoxGobj(m);
        break;
    case 10:
        path = m->path;
        if (path == NULL) {
            break;
        }
        done = 0;
        if (MOVER_STATE(m) == 1) {
            MOVER_SET_STATE(m, 2);
            PathGetPoint(m, 1, &v);
            m->path->endProgress = v.vy;
            PlaneFallSetup(m, msg->from);
        } else if (MOVER_STATE(m) == 2) {
            cur = path->cur;
            list = &objActivationList[m->sector];
            for (i = 0; i < list->count; i++) {
                other = (MuseumMover *)list->infos[i]->runtime;
                if (((GameObjectRuntime *)other)->healthInfo->maxHealth > 0) {
                    t.vx = other->anim->frame->pos.vx;
                    t.vy = other->anim->frame->pos.vy;
                    t.vz = other->anim->frame->pos.vz;
                    t.vy = -t.vy;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&cur, &t, dist);
                    if (dist[0] < 0x180) {
                        DamageGOBJ(-1, m->id, other->id, 100, 0x11);
                    }
                }
            }
        } else if (PathIsStopped(m->path)) {
            p.vx = m->anim->frame->pos.vx;
            p.vy = m->anim->frame->pos.vy;
            p.vz = m->anim->frame->pos.vz;
            p.vy = -p.vy;
            ParticleSmoke(-1, &p, &gExploDir, 0x1000, 1);
            done = 1;
            MOVER_SET_STATE(m, 4);
            MovementRemoveGobj(m);
        }
        if (!done) {
            SysPostMessage(10, 4, m->id, m->id, 0, 0, 0, 0);
        }
        break;
    case 42:
        p.vx = m->anim->frame->pos.vx;
        p.vy = m->anim->frame->pos.vy;
        p.vz = m->anim->frame->pos.vz;
        p.vy = -p.vy;
        p.vy = *(s32 *)msg->data[0];
        EGSFrameSetPosition3(m->anim->frame, 0, &p);
        break;
    case 41:
        v.vx = m->anim->frame->pos.vx;
        v.vy = m->anim->frame->pos.vy;
        v.vz = m->anim->frame->pos.vz;
        v.vy = -v.vy;
        *(s32 *)msg->data[0] = v.vy;
        break;
    case 2:
        func_801463A4(m->id, 0);
        m->unk22 = 3;
        TargetInitGobj(m, D_80143F84, 0, 0);
        m->flags0 &= 0x77;
        m->unk10 = 0;
        m->anim->unk09 = 0x40;
        m->flags1 &= 0xF8;
        MovementGobjInit(m, 0, 0, 0, 0);
        PhysicsGobjBasicInit(m, 0x10000, 1);
        CollisionGobjBasicInit(m, m->path->cur.vy, 0x666, 0x1000, 0x800, 0xC00);
        break;
    }
}

/* Message handler of the breakable windows. */
void TGTMUSEUM2ObjDispatch(SysMsg *msg) {
    MuseumMover *m;
    s32 out;

    m = (MuseumMover *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 10:
        if (m->path != NULL && (m->flags0 & 0x40)) {
            PathHandleEx(m, &gObjPathScaleVector, 1, 1, -1, -1, &out);
            SysPostMessage(10, 4, m->id, m->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        m->flags0 |= 0x40;
        CobjAddWindowGobj(m, 1, 0, 0);
        if (m->path != NULL) {
            SysPostMessage(10, 4, m->id, m->id, 0, 0, 0, 0);
        }
        break;
    case 5:
        m->flags0 &= ~0x40;
        CobjRemoveWindowGobj(m);
        break;
    case 42:
        if (m->healthInfo->maxHealth > 0) {
            break;
        }
        m->flags1 |= 0x80;
    case 13:
        if (m->healthInfo->maxHealth <= 0 && !(m->flags0 & 0x80)) {
            m->flags0 |= 0x80;
            m->flags23 |= 2;
        }
        break;
    case 2:
        func_801463A4(m->id, 0);
        m->unk22 = 10;
        TargetInitGobj(m, D_80143F84, 0, 0);
        m->flags0 &= 0x3D;
        m->unk10 = 0;
        m->flags1 &= 0x7F;
        m->anim->unk18 = 0;
        m->anim->unk09 = 0x17;
        m->unk20 |= 0x20;
        func_80147AAC(m, 0x20, 0x10, 0x20);
        break;
    }
}
