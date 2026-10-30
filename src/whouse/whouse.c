#include "common.h"
#include "level.h"
#include "system.h"

/* Object types specific to this level */
#define WHOUSE_TYPE_SNIFFER 0x59      /* sniffer box (interact objective) */
#define WHOUSE_TYPE_SWITCH_A 0x24     /* electric fence power switch */
#define WHOUSE_TYPE_SWITCH_B 0x7B     /* electric fence power switch */
#define WHOUSE_TYPE_FENCE 0x60        /* electric fence segment */

/* whouse.ovl statics, still defined in the overlay's asm .data/.bss */
extern void *D_801426FC;      /* "sniffers remaining" text */
extern void *D_80142700;      /* "N more sniffers" format */
extern void *D_80142704;      /* name/use string for the special NPC */
extern u8 D_80142708[];       /* texture animation tables */
extern u8 D_8014270C[];
extern u8 D_8014272C[];
extern u8 D_80142730[];
extern u8 D_80142750[];       /* inwhouse objectives/timers save info */
extern u8 D_80142778[];       /* whouse objectives/timers save info */
extern void **D_8014277C;     /* -> objective message table */
extern char D_801427A0[];     /* objective text buffer (0x40) */
extern s32 D_801427E0;        /* only reset here */
extern s16 D_801427E4;        /* teleport target for the meeting movie */
extern s16 D_801427E8;        /* number of sniffers */
extern s16 D_801427EC;        /* sniffers used */
extern u8 D_801427F0;         /* special NPC hit already reported */
extern u8 D_80140FE4[];       /* objective format (rodata) */

/* main.exe */
extern u8 gElectricFenceFlag;
extern void *gPowerSwitchMsg;
extern u8 gExploShakeVector[];
extern s32 gExeTick;
void EGSDisplayInitTextureAnimation(void *a, void *b, s32 c, s32 d, s32 e);
void InterfaceStartCountdownAfterGameCam(s32 seconds, s32 arg1, void (*done)());
void GameCameraShakeStart(s32 a, s32 b, s32 c, void *vec, s32 e, s32 f);
void EGSDisplaySetFlashTime(s32 t);
void ParticleFireWaveDeath(void);
void DamageKillAllPeople(s32 by, s32 kind);
void SysSoundQueueXA(s32 a, s32 track);
void JoystickDeactivate(void);
void EGSDisplayRegisterSnifferAnim(void *anim);

void InwhouseTimerEndCallback(void);
static void func_801415F4(void);
static void func_80141760(void);
static void func_801417B4(s16 id, s16 by);
static void func_80141820(s16 id, s16 by);
static s32 func_801418D4(void);
static void func_80141974(s16 id);
static s32 func_80141AEC(s16 id);
static void func_80141C44(s16 id);
static void func_80141CCC(void);
static char *func_80141D5C(void);
static void *func_80141D6C(s16 id, s16 kind);

/* Inwhouse countdown expired: the warehouse burns down. */
void InwhouseTimerEndCallback(void) {
    if (((GameObjectRuntime *)gObjPlayer)->healthInfo->maxHealth > 0) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        SysPlayXA(0xA8, 0, 1);
    }
    GameCameraShakeStart(0, 5, 1, gExploShakeVector, 0, 0);
    EGSDisplaySetFlashTime(10);
    ParticleFireWaveDeath();
    DamageKillAllPeople(gPlayerOffset, 0x11);
    if (!gMissionFailed) {
        LevelMissionFailed(0, -1, 0);
    }
}

static void func_801415F4(void) {
    SysPlaySong(0, 7, 0, 0);
}

void iInwhouseInit(s32 mode) {
    int i;
    s32 count;

    EGSDisplayInitTextureAnimation(D_80142708, D_8014270C, 8, 3, 0);
    EGSDisplayInitTextureAnimation(D_8014272C, D_80142730, 8, 3, 0);
    InterfaceStartCountdownAfterGameCam(900, 0, InwhouseTimerEndCallback);
    gLevelAlerted = 1;
    npcSetLevelAlert();
    SaveObjectivesNTimersReinit(D_80142750, 0);
    LevelObjectiveAdd(4, -1);
    if (gObjectives != NULL) {
        count = gObjectives->count;
        for (i = 0; i < count; i++) {
            if (gObjectives != NULL) {
                gObjectives->unk24 |= 1 << i;
            }
        }
    }
    if (mode == 3) {
        func_801415F4();
    } else {
        gSysMovieDoneCallback = func_801415F4;
    }
    gUsingSndBigFire = 1;
    gDefaultLevelDmgFX = 20;
}

static void func_80141760(void) {
    StringObjectiveObjectsRemaining2(D_801427A0, D_801426FC, gRemainingMsg, D_80140FE4, D_801427EC, D_801427E8);
}

static void func_801417B4(s16 id, s16 by) {
    if (by == gPlayerOffset && !D_801427F0 && id == gLevelSpecificNpcID) {
        D_801427F0 = 1;
        SysPlayXA(0xA1, 0, 1);
    }
}

static void func_80141820(s16 id, s16 by) {
    if (id == gLevelSpecificNpcID && !(gameObjectsInfo[id].runtime->flags0 & 0x20)) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        if (gObjectives != NULL) {
            gObjectives->unk14 |= 1;
        }
        LevelMissionFailed(0, 0xA2, 0);
    }
}

static s32 func_801418D4(void) {
    if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 4)) {
        if (gObjectives->completed & 1) {
            SysPlayXA(0xA5, 0, 1);
            return 1;
        }
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void func_80141974(s16 id) {
    int type;

    type = GOBJ_TYPE(id);
    if (type == WHOUSE_TYPE_SNIFFER) {
        D_801427EC++;
        if (D_801427EC < D_801427E8) {
            StringInteractObjectsRemaining(D_80142700, D_801427E8 - D_801427EC);
            func_80141760();
            if (D_801427EC == 1) {
                SysSoundQueueXA(-1, 0x9F);
                GameSysSetLetterBoxMode(1);
                JoystickDeactivate();
                CASRequestAction(gPlayerOffset, 0x5B);
            }
            if (!gMissionFailed) {
                LevelMissionCtrMsg(gCheckpointMsg, 0);
                SaveCreateContinue();
            }
        } else {
            func_80141760();
            SysSoundQueueXA(-1, 0xA4);
            GameSysSetLetterBoxMode(1);
            JoystickDeactivate();
            CASRequestAction(gPlayerOffset, 0x5B);
            LevelObjectiveComplete(2);
        }
    }
}

/* Power switch: turns off every electric fence segment. */
static s32 func_80141AEC(s16 id) {
    int type;
    int i;

    type = GOBJ_TYPE(id);
    if (type == WHOUSE_TYPE_SWITCH_A || type == WHOUSE_TYPE_SWITCH_B) {
        gElectricFenceFlag = 0;
        for (i = 0; i < gLevelGameObjCount; i++) {
            if (GOBJ_TYPE(i) == WHOUSE_TYPE_FENCE) {
                SysPostMessage(0x1B, 4, id, i, 0, 0, 0, 0);
            }
        }
        LevelObjectiveComplete(1);
        return 0;
    }
    return 3;
}

INCLUDE_RODATA("asm/whouse/nonmatchings/whouse", D_80140FE4);

static void func_80141C44(s16 id) {
    s32 ref;

    ref = gameObjectsInfo[id].refId;
    if (!gMissionFailed && ref == gLevelSpecificNpcID) {
        ObjectDoMovieEx("TEMP", 0x2D, id, id);
        LevelMissionCtrMsg("MOVIE OF MEETING", 0);
    }
}

static void func_80141CCC(void) {
    GameObjectRuntime *rt;

    ObjectTeleportPlayer(D_801427E4, 0x51);
    GameCameraRequestCutToBehind();
    CASRequestAction(gLevelSpecificNpcID, 0x55);
    rt = gameObjectsInfo[gLevelSpecificNpcID].runtime;
    rt->flags0 |= 0x20;
    rt->flags23 |= 8;
    LevelObjectiveComplete(0);
}

static char *func_80141D5C(void) {
    return " ";
}

static void *func_80141D6C(s16 id, s16 kind) {
    if (kind == 3) {
        if (GOBJ_TYPE(id) != WHOUSE_TYPE_SNIFFER) {
            return D_80142704;
        }
        return " ";
    }
    if (kind == 4) {
        return gPowerSwitchMsg;
    }
    return NULL;
}

void iWhouseInit(void) {
    int i;
    int bit;
    int type;
    GameObjectRuntime *rt;
    s32 count;

    D_801427E4 = -1;
    D_801427E0 = 0;
    D_801427E8 = 0;
    D_801427EC = 0;
    D_801427F0 = 0;
    gLevelAlerted = 1;
    npcSetLevelAlert();
    D_8014277C[2] = D_801427A0;
    SaveObjectivesNTimersReinit(D_80142778, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(1, -1);
        LevelObjectiveAdd(2, -1);
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
    DamageSetDamagedCallback(func_801417B4);
    DamageSetDestroyedCallback(func_80141820);
    LevelSetEOLCallback(func_801418D4);
    UseSetLevSpecObjCallback(func_80141974);
    UseSetSwitchCallback(func_80141AEC);
    ObjectSetSpecialObjectCallback(func_80141C44);
    ObjectSetMovieDoneCallback(func_80141CCC);
    SetLockedStringCallback(func_80141D5C);
    SetUseStringCallback(func_80141D6C);
    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (type == WHOUSE_TYPE_SNIFFER) {
            D_801427E8++;
            if (D_801427E8 == 1) {
                EGSDisplayRegisterSnifferAnim(rt->anim);
            }
            if (rt->flags0 & 0x20) {
                D_801427EC++;
            }
        } else if (type == WHOUSE_TYPE_SWITCH_A || type == WHOUSE_TYPE_SWITCH_B) {
            if (rt->flags0 & 0x20) {
                gElectricFenceFlag = 0;
            }
        }
    }
    func_80141760();
    if (gLevelSpecificNpcID != -1) {
        if (gameObjectsInfo[gLevelSpecificNpcID].runtime->unk20[2] == 2) {
            gLevelSpecificNpcName = D_80142704;
            D_801427E4 = gameObjectsInfo[gLevelSpecificNpcID].refId;
        }
    }
    gDefaultLevelDmgFX = 11;
}

/* Path-driven mover attached to a runtime object (runtime + 0x0C). */
typedef struct WhouseVec {
    s32 vx, vy, vz, pad;
} WhouseVec;

typedef struct WhousePath {
    /* 0x000 */ u8 unk000[4];
    /* 0x004 */ s32 progress;
    /* 0x008 */ u8 unk008[0x18];
    /* 0x020 */ s32 velX;
    /* 0x024 */ s32 velY;
    /* 0x028 */ s32 velZ;
    /* 0x02C */ u8 unk02C[4];
    /* 0x030 */ WhouseVec pos;
    /* 0x040 */ u8 unk040[0x70];
    /* 0x0B0 */ WhouseVec target;
    /* 0x0C0 */ u8 unk0C0[0x30];
    /* 0x0F0 */ WhouseVec push;
    /* 0x100 */ u8 unk100;
    /* 0x101 */ u8 unk101;
    /* 0x102 */ u8 unk102[6];
    /* 0x108 */ u8 unk108;
    /* 0x109 */ u8 unk109[0x23];
    /* 0x12C */ s32 endProgress;
    /* 0x130 */ u8 unk130[0x74];
    /* 0x1A4 */ u32 flags;
} WhousePath;

typedef struct WhouseAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s16 *matrix;
} WhouseAnim;

typedef struct WhouseMover {
    /* 0x00 */ u8 timer;
    /* 0x01 */ u8 flags1; /* low 3 bits: state, 0x80: has path */
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ WhouseAnim *anim;
    /* 0x0C */ WhousePath *path;
    /* 0x10 */ u8 unk10[0x12];
    /* 0x22 */ u8 unk22;
} WhouseMover;

typedef struct WhouseMsg {
    /* 0x00 */ u16 id;
    /* 0x02 */ u8 unk02[6];
    /* 0x08 */ s32 to;
} WhouseMsg;

s32 PathGetVelocity(WhouseMover *m);
void PathGetPoint(WhouseMover *m, s32 arg1, WhouseVec *out);
void MatrixNormal_0(s16 *in, s16 *out);
void MovementRemoveGobj(WhouseMover *m);
void MovementAddGobj(WhouseMover *m);
void MovementGobjInit(WhouseMover *m, s32 a, s32 b, s32 c, s32 d);
void CobjAddCollisionBoxGobj(WhouseMover *m, s32 a, s32 b, s32 c, s32 d);
void CobjRemoveCollisionBoxGobj(WhouseMover *m);
void func_801463A4(s16 id, s32 arg1); /* LevelCreateGOBJ (init.ovl) */
void PathSetupEX(WhouseMover *m, s32 arg1);
void PhysicsGobjBasicInit(WhouseMover *m, s32 a, s32 b);
void CollisionGobjBasicInit(WhouseMover *m, s32 a, s32 b, s32 c, s32 d, s32 e);

#define MOVER_STATE(m) ((m)->flags1 & 7)
#define MOVER_SET_STATE(m, s) ((m)->flags1 = ((m)->flags1 & 0xF8) | (s))
#define IN_RANGE(x) ((u32)((x) + 0x2000) <= 0x4000)

static inline u8 PathIsStopped(WhousePath *path) {
    if (((path->unk108 && (path->flags & 0x100000)) || !path->unk101) && IN_RANGE(path->velX) &&
        IN_RANGE(path->velY) && IN_RANGE(path->velZ)) {
        return 1;
    }
    return 0;
}

/* Message handler of the moving platform object. */
void func_8014216C(WhouseMsg *msg) {
    WhouseMover *m;
    WhousePath *path;
    WhouseVec v;
    WhouseVec w;
    WhouseVec d;
    s32 limit;
    s32 z;
    u8 done;
    s16 *mat;

    m = (WhouseMover *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 10:
        path = m->path;
        if (path == NULL) {
            break;
        }
        done = 0;
        if (MOVER_STATE(m) == 1) {
            m->timer++;
            if (m->flags1 & 0x80) {
                limit = (double)PathGetVelocity(m);
            } else {
                limit = 13;
            }
            if (m->timer >= limit) {
                MOVER_SET_STATE(m, 2);
                v.vx = v.vy = 0;
                v.vz = 0x16C16;
                m->path->push.vx += v.vx;
                m->path->push.vy += v.vy;
                m->path->push.vz += v.vz;
                PathGetPoint(m, 1, &v);
                m->path->endProgress = m->unk04 = v.vy;
            } else if (m->timer & 1) {
                z = 0x4FA4F;
                if (m->timer & 2) {
                    z = -0x4FA4F;
                }
                w.vz = z;
                w.vx = w.vy = 0;
                m->path->push.vx += w.vx;
                m->path->push.vy += w.vy;
                m->path->push.vz += w.vz;
            }
        } else if (MOVER_STATE(m) == 2 && path->progress < path->endProgress + 75) {
            v = path->pos;
            w = m->path->target;
            if ((u32)v.vy <= 10000 && w.vz == 0) {
                MOVER_SET_STATE(m, 3);
                d.vx = d.vy = 0;
                d.vz = -0xE38E3;
                m->path->push.vx += d.vx;
                m->path->push.vy += d.vy;
                m->path->push.vz += d.vz;
            }
        } else if (MOVER_STATE(m) == 3) {
            if (PathIsStopped(m->path)) {
                mat = m->anim->matrix;
                done = 1;
                mat[1] = 0;
                mat[7] = 0;
                mat[3] = 0;
                mat[5] = 0;
                MatrixNormal_0(mat, mat);
                MOVER_SET_STATE(m, 4);
                MovementRemoveGobj(m);
            }
        }
        if (!done) {
            SysPostMessage(10, 4, m->id, m->id, 0, 0, 0, 0);
        }
        break;
    case 26:
        if (MOVER_STATE(m) == 0) {
            m->timer = 0;
            MOVER_SET_STATE(m, 1);
            MovementAddGobj(m);
            SysPostMessage(10, 4, m->id, m->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        if (MOVER_STATE(m) < 2) {
            m->timer = 0;
            m->unk04 = gExeTick;
        }
        CobjAddCollisionBoxGobj(m, 1, 0, 1, 1);
        break;
    case 5:
        CobjRemoveCollisionBoxGobj(m);
        break;
    case 2:
        m->unk22 = 0;
        func_801463A4(m->id, 0);
        PathSetupEX(m, 1);
        if (m->path != NULL) {
            m->flags1 |= 0x80;
        } else {
            m->flags1 &= 0x7F;
        }
        m->timer = 0;
        m->flags1 &= 0xF8;
        m->anim->unk09 = 0x40;
        MovementGobjInit(m, 0, 0, 0, 0);
        PhysicsGobjBasicInit(m, 0x2000, 1);
        CollisionGobjBasicInit(m, m->path->progress, 0x666, 0x800, 1, 0xC00);
        break;
    }
}
