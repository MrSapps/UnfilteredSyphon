#include "common.h"
#include "level.h"
#include "system.h"
#include "gobj.h"

/*
 * cave.ovl holds the level code for CAVE, CAVE2 and SILO (iCaveInit, iCave2Init,
 * iSiloInit), plus the falling-object dispatcher used by those levels.
 */

/* Object types specific to this overlay */
#define CAVE_TYPE_EXPLOSIVE 0x04 /* counted at init; flags0 0x20 = already set */

/* Path/movement state hanging off GameObjectRuntime+0x0C. */
typedef struct CaveMoveInfo {
    /* 0x000 */ VECTOR32 pos;
    /* 0x010 */ u8 unk10[0x10];
    /* 0x020 */ VECTOR32 vel;
    /* 0x030 */ u8 unk30[0x40];
    /* 0x070 */ VECTOR32 unk70;
    /* 0x080 */ u8 unk80[0x81];
    /* 0x101 */ u8 unk101;
    /* 0x102 */ u8 unk102[6];
    /* 0x108 */ u8 unk108;
    /* 0x109 */ u8 unk109[0x23];
    /* 0x12C */ s32 unk12C;
    /* 0x130 */ u8 unk130[0x74];
    /* 0x1A4 */ u32 unk1A4;
} CaveMoveInfo;

typedef struct CaveAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 flagsA;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ GameObjectModel *model;
} CaveAnim;

typedef struct CaveNPCInfo {
    /* 0x00 */ u8 unk00[0x2C];
    /* 0x2C */ s16 unk2C;
    /* 0x2E */ u8 unk2E[0xA];
    /* 0x38 */ s16 unk38;
    /* 0x3A */ u8 unk3A[7];
    /* 0x41 */ u8 unk41;
} CaveNPCInfo;

/* GameObjectRuntime as seen by the falling objects, the silo missile and Rhoemer. */
typedef struct CaveRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1; /* falling objects: low 3 bits state, 0x80 has a path */
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 unk04; /* falling object: tick it starts to fall; missile: top of its path */
    /* 0x08 */ CaveAnim *anim;
    /* 0x0C */ CaveMoveInfo *move;
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ CaveNPCInfo *npc;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 unk22; /* 2 for Rhoemer */
    /* 0x23 */ u8 flags23;
} CaveRuntime;

#define FALLING_STATE(rt) ((rt)->flags1 & 7)
#define FALLING_SET_STATE(rt, s) ((rt)->flags1 = ((rt)->flags1 & 0xF8) | (s))

/* cave.ovl statics, still defined in the overlay's asm .data/.bss */
extern void *D_80143C6C;   /* locked-door string (cave) */
extern void *D_80143C70;   /* use string (default switch) */
extern void *D_80143C74;   /* switch thrown status message */
extern void *D_80143C78;   /* use string (types 0x1C/0x7A) */
extern void *D_80143C7C;   /* use string (kind 1) */
extern void *D_80143C80;   /* computer status message */
extern void *D_80143C84;   /* locked-door string (silo) */
extern void *D_80143C88;   /* use string (silo, kind 3) */
extern void *D_80143C8C;   /* use string (silo, kind 1) */
extern u8 D_80143C90[];    /* cave objectives/timers save info */
extern u8 D_80143CB8[];    /* silo objectives/timers save info */
extern void **D_80143CBC;  /* silo objective message table */
extern u8 D_80143CE0[];    /* camera shake vectors */
extern u8 D_80143CF0[];
extern u8 D_80143D00[];
extern u8 D_80143D10[];    /* missile launch camera shake vector */
extern u8 D_80143D20[];    /* computer camera shake state */
extern u8 D_80143D64[];    /* falling objects camera shake state */
extern s16 D_80143DA8;     /* falling objects triggered */
extern s16 D_80143DAC;     /* falling objects landed */
extern u8 D_80143DB0;      /* explosives total */
extern u8 D_80143DB4;      /* explosives set / computer uses */
extern u8 D_80143DB8;      /* switch thrown */
extern u8 D_80143DBC;      /* locked message played */
extern s32 D_80143DC0;     /* height below which the missile exhaust kills the player */
extern s16 D_80143DC8;     /* silo missile object */
extern s16 D_80143DCC;     /* Rhoemer (boss fight movie / message target) */
extern s16 D_80143DD0;     /* Rhoemer at the missile console */
extern u8 D_80143DD4;      /* hit-the-device warning played */
extern u8 D_80143DD8;      /* missile launched */
extern u8 D_80143DDC;
extern u8 D_80143DE0;      /* special NPC hit warning played */
extern s16 museumBriefings; /* (misnamed .bss) game-cam script played at the special NPC */

/* main.exe */
extern s32 gExeTick;
extern void *gElevatorCallSwitchMsg;
extern u8 gExploDir[];
extern u8 gEliminateRhoemerMsg[];
extern void *gRhoemerMsg;
extern void (*gGameCamScriptEndCallback)(void);
void GameCameraShakeStart(void *state, s32 ticks, s32 arg2, void *vec, s32 arg4, s32 arg5);
void GameCameraShakeModifyMagnitude(void *state, void *vec);
void GameCameraShakeStop(void *state);
void GameCamCheckScriptCamDoneCallback(s32 arg);
s32 LevelDoEOL(u8 force);
s32 LevelGetPathInfo(s32 id, s32 *start, s32 *path);
void SysSoundQueueXA(s32 arg0, s32 track);
void SysSetSongVolPan(s32 arg0, s32 song, void *from, s32 vol, s32 pan);
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);
s32 SysExeTickGetTimerTicks(void (*cb)(s32));
void SndSysCheckXADoneCallback(s32 arg);
void SaveInternalTimer(s32 ticks);
void StringContactMeiXingMsg(void);
void UseSetComputerCallback(void (*cb)());
void JoystickDeactivate(void);
void DamageGOBJ(s32 arg0, s16 id, s16 by, s16 amount, s32 kind);
void GSActivateGameObject(GameObjectRuntime *rt, void *camera, s32 arg2);
void ParticleExplosion(s16 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void ParticleSmoke(s32 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
s32 ParticleFireFade(s16 id);
s32 ParticleSmokeFade(s16 id);
void ParticleFireWaveDeath(void);
s32 PathGetVelocity(CaveRuntime *rt);
s32 PathGetNumPoints(CaveRuntime *rt);
void PathGetPoint(CaveRuntime *rt, s32 index, VECTOR32 *out);
void PathSetPositionToPoint(CaveRuntime *rt, s32 index);
void PathSetupEX(CaveRuntime *rt, s32 arg1);
void MovementAddGobj(CaveRuntime *rt);
void MovementRemoveGobj(CaveRuntime *rt);
void MovementGobjInit(CaveRuntime *rt, s32 a, s32 b, s32 c, s32 d);
void PhysicsGobjBasicInit(CaveRuntime *rt, s32 a, s32 b);
void CobjAddBulletCollisionBoxGobj(CaveRuntime *rt, s32 a, s32 b, s32 c);
void CobjAddCollisionBoxGobj(CaveRuntime *rt, s32 a, s32 b, s32 c, s32 d);
void CobjRemoveBulletCollisionBoxGobj(CaveRuntime *rt);
void CobjRemoveCollisionBoxGobj(CaveRuntime *rt);
void CollisionGobjBasicInit(CaveRuntime *rt, s32 a, s32 b, s32 c, s32 d, s32 e);
/* cave passes no movement vector ($a3 is left unset) */
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void func_801463A4(s16 id, s32 arg1); /* LevelCreateGOBJ (init.ovl) */

/* Start a chain of falling objects, each one later than the previous. */
static void func_801417E8(CaveRuntime *rt) {
    CaveRuntime *first;
    s32 next;
    s32 tick;
    s32 delay;

    tick = gExeTick;
    rt->unk04 = tick;
    SysPlaySong(0, 0x18, (s32)rt, 0);
    SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
    first = rt;
    while ((next = gameObjectsInfo[rt->id].refId) != -1) {
        rt = (CaveRuntime *)gameObjectsInfo[next].runtime;
        if (rt->flags1 & 0x80) {
            delay = PathGetVelocity(rt);
        } else {
            delay = 10;
        }
        tick += delay;
        rt->unk04 = tick;
        D_80143DA8++;
        SysPostMessage(10, 4, first->id, next, 0, 0, 0, 0);
    }
    GameCameraShakeStart(0, tick - gExeTick + D_80143DA8, 1, D_80143D00, 0, 0);
}

static s32 func_80141958(void) {
    switch (++D_80143DB4) {
    case 1:
        TextStatusMessage(D_80143C80, 0);
        GameCameraShakeStart(D_80143D20, -1, 1, D_80143CE0, 0, 0);
        SysPlaySong(0, 0x18, 0, 0);
        break;
    case 2:
        TextStatusMessage(D_80143C80, 0);
        GameCameraShakeModifyMagnitude(D_80143D20, D_80143CF0);
        break;
    case 3:
        LevelDoEOL(0);
        break;
    }
    return 0;
}

static s32 func_80141A48(s16 id) {
    GameObjectRuntime *rt;
    int type;
    s16 target;

    rt = gameObjectsInfo[id].runtime;
    if (id != GOBJ_ID_666) {
        type = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type;
    } else {
        type = GOBJ_ID_666;
    }
    if (type == 0x1D || type == 0x7C) {
        target = rt->id;
        SysPostMessage(0x1B, 3, target, gameObjectsInfo[gameObjectsInfo[target].refId].runtime->id, 0, 0, 0, 0);
        D_80143DB8 = 1;
        TextStatusMessage(D_80143C74, 0);
        return 0;
    }
    return 3;
}

static s32 func_80141B6C(s16 id) {
    s32 ref;
    u8 alive;

    ref = gameObjectsInfo[id].refId;
    alive = 1;
    if (ref != -1) {
        alive = gameObjectsInfo[ref].runtime->healthInfo->maxHealth > 0;
    }
    return alive;
}

static void func_80141BE4(s16 id) {
    LevelObjectiveAdd(0, id);
}

static void *func_80141C0C(void) {
    if (!D_80143DB8 && !D_80143DBC) {
        D_80143DBC = 1;
        SysSoundQueueXA(-1, 0xC);
        StringContactMeiXingMsg();
    }
    return D_80143C6C;
}

static void *func_80141C68(s16 id, s16 kind) {
    int type;

    if (kind == 1) {
        return D_80143C7C;
    }
    if (kind == 4) {
        type = GOBJ_TYPE(id);
        if (type == 0x1B || type == 0x79) {
            return gElevatorCallSwitchMsg;
        }
        if (type == 0x1C || type == 0x7A) {
            return D_80143C78;
        }
        return D_80143C70;
    }
    return NULL;
}

void iCaveInit(void) {
    int i;
    int type;

    D_80143DB0 = 0;
    D_80143DB4 = 0;
    D_80143DA8 = 1;
    D_80143DAC = 0;
    D_80143DB8 = 0;
    D_80143DBC = 0;
    SaveObjectivesNTimersReinit(D_80143C90, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(1, -1);
    }
    UseSetComputerCallback(func_80141958);
    UseSetSwitchCallback(func_80141A48);
    ObjectSetQuipObjectCallback(func_80141B6C);
    ObjectSetSpecialObjectCallback(func_80141BE4);
    SetLockedStringCallback(func_80141C0C);
    SetUseStringCallback(func_80141C68);
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == CAVE_TYPE_EXPLOSIVE) {
            D_80143DB0++;
            if (gameObjectsInfo[i].runtime->flags0 & 0x20) {
                D_80143DB4++;
            }
        }
    }
    gDefaultLevelDmgFX = 11;
}

static s32 func_80141EFC(void) {
    SysPlayXA(0x12, 0, 1);
    return 1;
}

static void *func_80141F24(s16 id, s16 kind) {
    void *msg;

    msg = NULL;
    if (kind == 4) {
        msg = gElevatorCallSwitchMsg;
    }
    return msg;
}

void iCave2Init(void) {
    D_80143DA8 = 1;
    D_80143DAC = 0;
    SaveObjectivesNTimersReinit(D_80143C90, 0);
    LevelObjectiveAdd(1, -1);
    LevelSetEOLCallback(func_80141EFC);
    SetUseStringCallback(func_80141F24);
    gDefaultLevelDmgFX = 11;
}

static void func_80141FBC(void) {
    if (gObjectives == NULL || !(gObjectives->completed & 8)) {
        LevelMissionFailed(2, 0x73, 0);
    }
}


static void func_80142004(void) {
    int i;
    int type;

    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == 0x30) {
            gameObjectsInfo[i].runtime->flags23 &= ~2;
            gameObjectsInfo[i].runtime->flags23 |= 1;
            GSActivateGameObject(gameObjectsInfo[i].runtime, *gCamera, 0);
        }
        if (type == 0x36) {
            gameObjectsInfo[i].runtime->flags23 |= 2;
        }
    }
    GameCameraShakeStart(0, 0x140, 1, D_80143D10, 0, 0);
}

static void func_8014217C(s32 unused) {
    int i;
    int type;
    s32 start;
    s32 path;
    s32 burning;

    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == 0x36) {
            LevelGetPathInfo(i, &start, &path);
            if (start == 1) {
                burning = ParticleFireFade(i);
            } else {
                burning = ParticleSmokeFade(i);
            }
            if (burning == 0) {
                gameObjectsInfo[i].runtime->flags23 |= 2;
            }
        }
    }
}

void SiloMissileStart(s16 id) {
    CaveRuntime *rt;
    VECTOR32 pos;

    rt = (CaveRuntime *)gameObjectsInfo[id].runtime;
    rt->flags0 |= 8;
    MovementAddGobj(rt);
    PathGetPoint(rt, PathGetNumPoints(rt) - 1, &pos);
    rt->unk04 = pos.vy;
    SysPlaySong(0, 0x18, (s32)rt, 0);
    func_80142004();
    SysPostMessage(10, 4, id, id, 0, 0, 0, 0);
}

INCLUDE_RODATA("asm/cave/nonmatchings/cave", D_80140FE4);


static const VECTOR32 D_801416E4 = { 0, 0x100, 0, 0 }; /* missile climb per tick */

void SiloMissileAction(CaveRuntime *rt) {
    VECTOR32 pos;

    pos.vx = rt->anim->frame->pos.vx;
    pos.vy = rt->anim->frame->pos.vy;
    pos.vz = rt->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    if (!(gExeTick & 3)) {
        pos.vy -= 0x20;
        ParticleExplosion(rt->id, &pos, gExploDir, 0x1000, 0);
        pos.vy -= 0x120;
        ParticleExplosion(rt->id, &pos, gExploDir, 0x800, 0);
    }
    if (rt->unk04 > pos.vy) {
        rt->move->unk70.vx += D_801416E4.vx;
        rt->move->unk70.vy += D_801416E4.vy;
        rt->move->unk70.vz += D_801416E4.vz;
        SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
        SysSetSongVolPan(0, 0x18, rt, -1, -1);
    } else {
        rt->flags23 = (rt->flags23 & ~4) | 2;
        rt->flags0 &= ~8;
        MovementRemoveGobj(rt);
        SysStopSong(0, 0x18);
    }
}

/* Missile launch: anyone still under it burns, then the countdown starts. */
static void func_80142528(void) {
    VECTOR32 pos;
    s32 burned;
    GameObjectHealth *health;

    if (!D_80143DD8 && !D_80143DDC && gameObjectsInfo[D_80143DC8].runtime->healthInfo->maxHealth > 0) {
        pos = ((CaveRuntime *)gObjPlayer)->move->pos;
        burned = pos.vy < D_80143DC0;
        if (burned) {
            health = ((CaveRuntime *)gObjPlayer)->healthInfo;
            DamageGOBJ(-1, gPlayerOffset, gPlayerOffset, health->health + health->maxHealth, 0x11);
            ParticleFireWaveDeath();
        }
        D_80143DD8 = 1;
        if (gObjectives == NULL || !(gObjectives->completed & 4)) {
            LevelMissionFailed(1, 0x72, 0);
        } else if (!burned) {
            InterfaceStartCountdown(0xB4, 0, func_80141FBC);
        }
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        func_8014217C(0);
        SiloMissileStart(D_80143DC8);
    }
}

static void func_801426D4(s32 unused) {
    func_8014217C(0);
    SysPlayXA(0x75, 0, 1);
    SysExeTickQueueTimer(SndSysCheckXADoneCallback, 0x14, (s32)func_80142528);
}

static void func_8014271C(void) {
    SysExeTickQueueTimer(func_801426D4, 0x4B0, 0);
}


static void func_80142748(s16 id) {
    if (!D_80143DE0 && id == gLevelSpecificNpcID) {
        D_80143DE0 = 1;
        SysPlayXA(0x71, (s32)gameObjectsInfo[id].runtime, 0);
    }
}

static void func_801427BC(void) {
    gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 &= ~2;
}

static void func_80142804(s16 id, s16 by) {
    if (by == gPlayerOffset && !D_80143DD4 && id != GOBJ_ID_666 &&
        gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == 0x6C) {
        D_80143DD4 = 1;
        SysPlayXA(0x30, 0, 1);
    }
}

static void func_801428B8(void) {
    gGameCamScriptEndCallback = NULL;
    LevelDoEOL(0);
}

static void func_801428E0(s16 id) {
    VECTOR32 pos; /* never filled in */
    GameObjectHealth *health;

    if (id == gLevelSpecificNpcID2 && !gDeviceDetonated) {
        gGameCamScriptEndCallback = func_801428B8;
        SysPostMessage(0x12, SYS_MSG_IMMEDIATE, gPlayerOffset, museumBriefings, 0, 0, 0, 0);
        return;
    }
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == 0x6C) {
        gDeviceDetonated = 1;
        LevelMissionFailed(0, -1, 0);
        ParticleFireWaveDeath();
        ObjectBigExplosionFX(id, &pos, 0x1000, 5, 10);
        health = ((CaveRuntime *)gObjPlayer)->healthInfo;
        DamageGOBJ(-1, gPlayerOffset, gPlayerOffset, health->health + health->maxHealth, 0x11);
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysExeTickQueueTimer(func_801426D4, 0, 0);
        SysExeTickQueueTimer((void (*)(s32))func_80142528, 0, 0);
    }
}

static void func_80142A8C(void) {
    ObjectDoMovieEx("TEMP", 0x2D, D_80143DCC, D_80143DCC);
    LevelMissionCtrMsg("MOVIE FOR FINAL RHOEMER BOSS FIGHT", 0);
}

static void func_80142AD0(void) {
    SysExeTickQueueTimer((void (*)(s32))func_80142A8C, 2, 0);
}

static s32 func_80142AFC(void) {
    if (!gMissionFailed) {
        InterfaceCountdownAbort();
        SysSoundQueueXA(-1, 0x74);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
        SysExeTickQueueTimer(SndSysCheckXADoneCallback, 0x14, (s32)func_80142AD0);
    }
    return 0;
}

static void func_80142B6C(s16 id) {
    GameObjectInfo *info;

    if (!gMissionFailed) {
        SysPostMessage(0x1B, SYS_MSG_IMMEDIATE, id, D_80143DCC, 0, 0, 0, 0);
        info = &gameObjectsInfo[D_80143DD0];
        info->flags24 &= ~0x20;
        SaveInternalTimer(SysExeTickGetTimerTicks(func_801426D4));
        LevelObjectiveComplete(2);
    }
}

static s32 func_80142C14(s16 id) {
    s32 ok;

    ok = 1;
    if (gameObjectsInfo[id].refId != -1 && (gObjectives == NULL || !(gObjectives->completed & 4))) {
        ok = 0;
    }
    return ok;
}

static void func_80142C80(void) {
    ObjectDoMovieEx("TEMP", 0x2D, D_80143DD0, D_80143DD0);
    LevelMissionCtrMsg("MOVIE OF RHOEMER FIRING MISSILE", 0);
}

static void func_80142CC4(s16 id) {
    if (id == D_80143DD0 && !gMissionFailed && !D_80143DD8) {
        D_80143DDC = 1;
        SysExeTickQueueTimer(func_801426D4, 0, 0);
        SysExeTickQueueTimer(SndSysCheckXADoneCallback, 0, 0);
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysExeTickQueueTimer((void (*)(s32))func_80142C80, 2, 0);
    }
}

static void func_80142D78(s16 id) {
    int type;

    type = GOBJ_TYPE(id);
    if (type == 0x5F) {
        D_80143DDC = 0;
        SysExeTickQueueTimer((void (*)(s32))func_80142528, 1, 0);
        SysExeTickQueueTimer((void (*)(s32))func_801427BC, 2, 0);
        return;
    }
    gameObjectsInfo[gLevelSpecificNpcID2].runtime->flags23 &= ~2;
    SaveInternalTimer(0);
    LevelObjectiveAdd(4, -1);
    LevelObjectiveComplete(3);
}

/* Misnamed in the map: the silo's use-string callback. */
void *iSubwayInit(s16 id, s16 kind) {
    void *msg;

    if (kind == 4) {
        return gElevatorCallSwitchMsg;
    }
    if (kind == 1) {
        return D_80143C8C;
    }
    msg = NULL;
    if (kind == 3) {
        msg = D_80143C88;
    }
    return msg;
}

static void *func_80142EEC(void) {
    return D_80143C84;
}

void iSiloInit(void) {
    s32 timer;
    GameObjectInfo *silo;
    GameObjectInfo *info;
    CaveRuntime *rt;
    CaveRuntime *npc;
    int i;
    int type;
    s32 ref;

    D_80143DC0 = 0;
    D_80143DD0 = -1;
    D_80143DC8 = -1;
    D_80143DD4 = 0;
    D_80143DCC = -1;
    museumBriefings = -1;
    SaveObjectivesNTimersReinit(D_80143CB8, (s32)&timer);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(2, -1);
        LevelObjectiveAdd(3, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 7;
        }
    }
    D_80143CBC[4] = gEliminateRhoemerMsg;
    silo = NULL;
    DamageSetDamagedCallback(func_80142804);
    DamageSetDestroyedCallback(func_801428E0);
    UseSetComputerCallback(func_80142AFC);
    UseSetLevSpecObjCallback(func_80142B6C);
    ObjectSetQuipObjectCallback(func_80142C14);
    ObjectSetSpecialObjectCallback(func_80142CC4);
    ObjectSetMovieDoneCallback(func_80142D78);
    SetLockedStringCallback(func_80142EEC);
    SetUseStringCallback(iSubwayInit);
    D_80143DD8 = gObjectives != NULL ? gObjectives->completed & 8 : 0;
    D_80143DDC = 0;
    D_80143DE0 = gObjectives != NULL ? gObjectives->completed & 8 : 0;
    InterfaceSetAddLevelSpecificNPCCallback(func_80142748);
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == 6) {
            if (!D_80143DD8) {
                SysPostMessage(0x18, 3, i, i, 0, 0, 0, 0);
            }
        } else if (type == 0x5F) {
            ref = gameObjectsInfo[i].refId;
            if (ref != -1) {
                if (GOBJ_TYPE(ref) == 0x6C) {
                    D_80143DD0 = i;
                    if (!D_80143DD8 && (gObjectives == NULL || !(gObjectives->completed & 4))) {
                        info = &gameObjectsInfo[D_80143DD0];
                        info->flags24 |= 0x20;
                    }
                    rt = (CaveRuntime *)gameObjectsInfo[ref].runtime;
                    D_80143DC8 = ref;
                    rt->anim->flagsA |= 0x20;
                    rt->anim->model->flags |= 0x8000;
                    rt->unk21 = 0xFA;
                    rt->flags23 |= 4;
                    MovementGobjInit(rt, 0, 0, 0, 0);
                    PhysicsGobjBasicInit(rt, 0x1000, 0);
                    ref = gameObjectsInfo[ref].refId;
                    if (ref != -1) {
                        D_80143DCC = ref;
                        ref = gameObjectsInfo[ref].refId;
                        if (ref != -1) {
                            museumBriefings = ref;
                        }
                    }
                }
            } else {
                silo = &gameObjectsInfo[(s16)i];
            }
        } else if (type == 0xB) {
            VECTOR32 pos;

            rt = (CaveRuntime *)gameObjectsInfo[i].runtime;
            PathGetPoint(rt, PathGetNumPoints(rt) - 1, &pos);
            D_80143DC0 = pos.vy;
        } else if (type == 0x30 || (type == 0x36 && D_80143DD8)) {
            gameObjectsInfo[i].runtime->flags23 |= 2;
        }
    }
    if (gLevelSpecificNpcID != -1) {
        npc = (CaveRuntime *)gameObjectsInfo[gLevelSpecificNpcID].runtime;
        if (npc->unk22 == 2) {
            gLevelSpecificNpcName = gRhoemerMsg;
            npc->healthInfo->maxHealth = 0x7FFF;
            npc->flags23 |= 2;
        }
    }
    if (gLevelSpecificNpcID2 != -1) {
        npc = (CaveRuntime *)gameObjectsInfo[gLevelSpecificNpcID2].runtime;
        if (npc->unk22 == 2) {
            gLevelSpecificNpc2Name = gRhoemerMsg;
            if (gObjectives == NULL || !(gObjectives->completed & 8)) {
                npc->flags23 |= 2;
            }
            npc->npc->unk38 = 100;
            npc->npc->unk2C = 100;
            npc->npc->unk41 = 0;
        }
    }
    if (silo != NULL && !D_80143DD8) {
        if (timer == 0) {
            if (!(silo->flags24 & 0x20)) {
                SysExeTickQueueTimer(GameCamCheckScriptCamDoneCallback, 20, (s32)func_8014271C);
            } else {
                func_801426D4(0);
            }
        } else {
            SysExeTickQueueTimer(func_801426D4, timer, 0);
        }
    }
    gDefaultLevelDmgFX = 15;
}

/* On the ground (or not on anything) and no longer moving. */
static inline u8 FallingLanded(CaveMoveInfo *move) {
    if ((move->unk108 && (move->unk1A4 & 0x100000)) || !move->unk101) {
        if (move->vel.vx >= -0x2000 && move->vel.vx <= 0x2000 &&
            move->vel.vy >= -0x2000 && move->vel.vy <= 0x2000 &&
            move->vel.vz >= -0x2000 && move->vel.vz <= 0x2000) {
            return 1;
        }
    }
    return 0;
}

void FALLINGObjDispatch(SysMsg *msg) {
    VECTOR32 pt;
    VECTOR32 pos;
    CaveRuntime *rt;
    CaveRuntime *other;
    CaveMoveInfo *move;
    GameObjectHealth *health;
    s32 ref;
    s16 maxHealth;
    u8 done;
    u8 landed;

    rt = (CaveRuntime *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 10:
        move = rt->move;
        if (move == NULL) {
            break;
        }
        done = 0;
        if ((u32)gExeTick > (u32)rt->unk04) {
            if (FALLING_STATE(rt) == 0) {
                FALLING_SET_STATE(rt, 2);
                PathGetPoint(rt, 1, &pt);
                MovementAddGobj(rt);
                rt->move->unk12C = pt.vy;
            } else if (FALLING_STATE(rt) == 2) {
                landed = FallingLanded(move);
                if (!landed) {
                    SysSetSongVolPan(0, 0x18, rt, -1, -1);
                } else {
                    FALLING_SET_STATE(rt, 4);
                    MovementRemoveGobj(rt);
                    PathSetPositionToPoint(rt, 1);
                    done = 1;
                    if (++D_80143DAC & 4) {
                        pos.vx = rt->anim->frame->pos.vx;
                        pos.vy = rt->anim->frame->pos.vy;
                        pos.vz = rt->anim->frame->pos.vz;
                        pos.vy = -pos.vy;
                        ParticleSmoke(-1, &pos, gExploDir, 0x1000, 1);
                    }
                    if (D_80143DA8 == D_80143DAC) {
                        pos.vx = rt->anim->frame->pos.vx;
                        pos.vy = rt->anim->frame->pos.vy;
                        D_80143DAC = 0;
                        D_80143DA8 = 1;
                        pos.vz = rt->anim->frame->pos.vz;
                        pos.vy = -pos.vy;
                        ParticleSmoke(-1, &pos, gExploDir, 0x1000, 1);
                        GameCameraShakeStop(D_80143D64);
                        SysStopSong(0, 0x18);
                    }
                }
            }
        }
        if (!done) {
            SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
        }
        break;
    case 18:
    case 20:
        health = rt->healthInfo;
        if (health->maxHealth > 0) {
            health->maxHealth = -1;
            ref = gameObjectsInfo[rt->id].refId;
            if (ref != -1) {
                do {
                    other = (CaveRuntime *)gameObjectsInfo[ref].runtime;
                    other->healthInfo->maxHealth = -1;
                    ref = gameObjectsInfo[other->id].refId;
                } while (ref != -1);
            }
            GameCameraShakeStart(D_80143D64, -1, 1, D_80143CF0, 0, 0);
            SysExeTickQueueTimer((void (*)(s32))func_801417E8, (rt->flags1 & 0x80) ? PathGetVelocity(rt) : 10, (s32)rt);
        }
        break;
    case 26:
        if (FALLING_STATE(rt) == 2) {
            health = ((CaveRuntime *)gObjPlayer)->healthInfo;
            maxHealth = health->maxHealth;
            if (maxHealth > 0) {
                DamageGOBJ(-1, rt->id, gPlayerOffset, health->health + maxHealth, 0xF);
            }
        }
        break;
    case 6:
        CobjAddBulletCollisionBoxGobj(rt, 1, 0, 0);
        CobjAddCollisionBoxGobj(rt, 1, 0, 1, 1);
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(rt);
        CobjRemoveCollisionBoxGobj(rt);
        break;
    case 2:
        rt->unk22 = 0;
        func_801463A4(rt->id, 0);
        PathSetupEX(rt, 1);
        if (rt->move != NULL) {
            rt->flags1 |= 0x80;
        } else {
            rt->flags1 &= 0x7F;
        }
        rt->unk22 = 6;
        rt->anim->unk09 = 0x40;
        rt->unk04 = 0;
        rt->flags1 &= 0xF8;
        PathGetPoint(rt, 0, &pt);
        MovementGobjInit(rt, 0, (s32)&pt, 0, 0);
        PhysicsGobjBasicInit(rt, 0x2000, 1);
        CollisionGobjBasicInit(rt, rt->move->pos.vy, 0x333, 0x800, 0x800, 0x333);
        break;
    case 42:
        if (rt->healthInfo->maxHealth <= 0) {
            PathGetPoint(rt, PathGetNumPoints(rt) - 1, &pos);
            EGSFrameSetPosition(rt->anim->frame, 0, &pos);
        }
        break;
    }
}
