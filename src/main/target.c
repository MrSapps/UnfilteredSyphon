#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"
#include "egsmath.h"

/* the original was built with gcc's builtin abs() */
#define abs(x) __builtin_abs(x)

/* target.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_8001149C[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */


/* .sdata (0x8010F820/4 unreferenced; joystick.c's gButtonStrings literals follow) */
static u8 D_8010F820 = 0;
static u8 D_8010F821 = 0; /* target release pending */
static u8 D_8010F822 = 0; /* re-acquire requested while already in target mode */
static s32 D_8010F824 = 0;
/* .sbss */
static u16 D_8010FFDC; /* "no target" status message id */
/* .data: targeting profiles for kinds 1..3, 0x36 bytes each */
static s16 D_801055DC[3][0x1B] = {
    {
        0xA00, 1, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0x64,
        0, 0, 0, 0, 0, 0, 0, 0, -0x3E8, -1, 0x4B, 0,
        0, 0, 0,
    },
    {
        0xA00, 1, 1, -0x32, 0, 0, 0, 0, 0, 0, 0, 0,
        3, 0xC8, 0, 0, -0x32, -0x64, 0, 0, -0x3E8, 0, -0xA, 0,
        0, 0, 0,
    },
    {
        0xB40, 1, 1, -0x1E, 5, 0x50, 0x28, 0xA, 0, -0xA, 0xA, 0x14,
        0x10, 0x50, 0xA, 0, -0x14, -0x28, 0x28, 0, -0x3E8, -0xA, -0xA, 0,
        -0xA, 0, 0x14,
    },
};
#define D_801055A6 ((u8 *)D_801055DC - 0x36) /* profile of kind 0 would be here */
#define D_8010562A (D_801055DC[1][12])       /* kind 2's field 0x18, set from the level data */
extern u8 gTargetModeFlag;
extern s32 gSniperMode;
extern void *gNoTgtMsg;
typedef struct TargetTimer {
    s16 id;
    u8 unk02[2];
    s32 tick;
} TargetTimer;
extern TargetTimer mTargetTimerList[8];
extern u8 *gLevData;
extern s32 gCurrentWeapon;

/* GameObjectInfo+0x24 as a word: low byte pickup/weapon id, bits 0x3000 weapon kind */
#define INFO_WORD24(info) (*(u32 *)&(info)->flags24)

typedef struct TargetWeapon {
    /* 0x00 */ u32 unk00_0 : 3;
    /* 0x00 */ u32 kind : 3;
    /* 0x00 */ u32 unk00_6 : 26;
    /* 0x04 */ u32 unk04;
    /* 0x08 */ u32 accuracy : 6;
    /* 0x08 */ u32 damageNear : 10; /* within 0x3C0 */
    /* 0x08 */ u32 damageFar : 10;
    /* 0x08 */ u32 unk08_26 : 6;
    /* 0x0C */ u8 unk0C[0x14];
} TargetWeapon;
extern TargetWeapon D_80105F58[]; /* per-weapon info */
#define WEAPON_KIND(w) (D_80105F58[w].kind)

/* Weapon a gobj carries: its pickup id, else 19/20 by kind bits, the player's current one */
#define GOBJ_WEAPON(id)                                                              \
    ((id) != gPlayerOffset                                                           \
         ? (*(u8 *)&gameObjectsInfo[id].flags24 != 0                                 \
                ? *(u8 *)&gameObjectsInfo[id].flags24                                \
                : ((INFO_WORD24(&gameObjectsInfo[id]) & 0x3000) == 0x1000             \
                       ? 19                                                          \
                       : ((INFO_WORD24(&gameObjectsInfo[id]) & 0x3000) == 0x2000 ? 20 : 0))) \
         : gCurrentWeapon)

void EGSGeometry_GetFuzzyDistancePoint2Point(void *a, void *b, s32 *dist);
void EGSGeometryINT_VectorGetAngleFromXZPlane(void *v, s32 *angle);
void EGSGeometry_VectorGetXZAngleFromZAxis(VECTOR32 *v, s32 *angle);
s32 GameSysFuzzyMathXZDistance(VECTOR32 *a, VECTOR32 *b);
void EGSFrameGetOrientation(struct EGSFrame *frame, s32 arg1, VECTOR32 *out);
extern void **gCamera;

extern s32 gCASJoystickGobjID;
void AnimateGobjRequestAim(GameObjectRuntime *rt, VECTOR32 *target, u8 arg2, u8 mode);
u8 GameCameraCheckMode(s32 mode);
GameObjectRuntime *GameCameraGetPrimaryGobj();

extern s16 gDefaultLevelDmgFX;
extern s32 gPlayerNumConsecutiveHits;
void AnimateGobjRequestShotRecoil(GameObjectRuntime *rt, s32 player);
void DamageGOBJ(s16 from, s16 by, s16 target, s16 damage, s16 action);
void DamageTerrain(s32 id, VECTOR32 *pos, VECTOR32 *normal, VECTOR32 *dir, s16 fx);
s32 BulletTextureGet(s16 fx);
void EGSDisplayRegisterDamage(VECTOR32 *pos, VECTOR32 *normal, VECTOR32 *dir, s32 tex, s32 arg4);
s16 WeaponGetDeathAction(s16 weapon);
s32 NPCDoNoDamage();

/* TargetInfo fields past the shared definition */
typedef struct TargetShot {
    /* 0x00 */ u8 unk00[0x5C];
    /* 0x5C */ s16 victim; /* gobj the last shot actually hit, -1 for none */
    /* 0x5E */ u8 unk5E[0x16];
    /* 0x74 */ VECTOR32 hitPos;    /* where the last shot landed */
    /* 0x84 */ u8 unk84[0x10];
    /* 0x94 */ VECTOR32 hitNormal;
    /* 0xA4 */ u8 unkA4[0x20];
    /* 0xC4 */ s32 prevTick;
    /* 0xC8 */ s8 hits; /* consecutive shots at the same target, max 100 */
} TargetShot;
#define TARGET_SHOT(target) ((TargetShot *)(target))

/* View of a camera handle */
typedef struct TargetCamera {
    /* 0x00 */ s16 (*rot)[3]; /* rotation matrix rows */
    /* 0x04 */ u8 unk04[0x10];
    /* 0x14 */ s32 index; /* slot in each gobj's per-camera display table */
} TargetCamera;
int ratan2(int y, int x);

extern u16 gActivationDistance;
extern s16 gTaserGobjID;
extern s16 getNPCIndex;
extern s32 npcList[6];
int NPCPickNPCTarget(s16 id);

/* NPCInfo fields past the shared definition */
typedef struct TargetNPC {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ s16 dist; /* distance to the player */
} TargetNPC;
#define TARGET_NPC(rt) ((TargetNPC *)(rt)->npc)

/* A polygon of a gobj's hit model */
typedef struct TargetPoly {
    /* 0x00 */ s32 numVerts;
    /* 0x04 */ u32 *data; /* usable when word-aligned and data[-1] & 0x7FFFFFFF */
    /* 0x08 */ SVECTOR *normal;
    /* 0x0C */ u8 unk0C[8];
    /* 0x14 */ SVECTOR *verts[1];
} TargetPoly;

typedef struct TargetPolyNode {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ struct TargetPolyNode *next;
    /* 0x08 */ TargetPoly poly;
} TargetPolyNode;

/* GameObjectAnim fields past the shared definition */
typedef struct TargetLight {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 r, g, b;
} TargetLight;

typedef struct TargetAnim {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ TargetLight *light; /* ambient light at the gobj */
    /* 0x20 */ u8 unk20[8];
    /* 0x28 */ TargetPolyNode *polys;
} TargetAnim;
#define TARGET_ANIM(rt) ((TargetAnim *)(rt)->anim)

void GameCameraSetModeTarget(s32 on, s32 arg1, GameObjectRuntime *target, s32 arg3);
s32 TextIsStringShowing(u16 str);
void TextChangeStringDuration(u16 str, s32 ticks);
void TextRemoveString(u16 str);
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
u8 func_80032988(GameObjectRuntime *rt, s32 targetId, u8 wide);
s32 TargetViewConeCheck(s32 id, VECTOR32 *from, VECTOR32 *dir, s32 range, s32 fov, VECTOR32 *to);
int func_800333F8(GameObjectRuntime *rt);
void func_800323C4(TargetInfo *target);
void TextRemoveString(u16 str);
static u8 func_80034D24(GameObjectRuntime *rt, VECTOR32 *eye, VECTOR32 *point, VECTOR32 *normal);

/* Recompute the chance (target->unk58) that a gobj hits its target. */
void TargetCalcProb(s32 id) {
    GameObjectRuntime *rt;
    TargetInfo *target;
    TargetLight *light;
    s32 penalty;
    s16 i;

    rt = gameObjectsInfo[id].runtime;
    target = rt->target;
    penalty = 0;
    if (rt->healthInfo->health <= 0 || gTaserGobjID == id) {
        func_800323C4(target);
        target->targetId = -1;
        return;
    }
    if (target->targetId < 0) {
        func_800323C4(target);
        return;
    }
    for (i = 0; i < 15; i++) {
        target->unk34[i] = 0;
    }
    for (i = 0; i < 10; i++) {
        if (target->actions[i] == NULL) {
            break;
        }
        target->actions[i](rt);
    }
    light = TARGET_ANIM(gameObjectsInfo[target->targetId].runtime)->light;
    if (light->r + light->g + light->b < 0x400) {
        if (rt->id == gPlayerOffset) {
            if (!gPlayerFlashlight) {
                penalty = -20;
            }
        } else {
            penalty = -40;
        }
    }
    target->unk58 = D_80105F58[GOBJ_WEAPON(rt->id)].accuracy + penalty;
    for (i = 0; i < 15; i++) {
        target->unk58 += target->unk34[i];
    }
    if (target->unk58 < 0) {
        target->unk58 = 0;
    }
    if (id == gPlayerOffset && target->unk58 < 15 && (target->flags & 2) && gTargetModeFlag
        && WEAPON_KIND(GOBJ_WEAPON(rt->id)) == 1) {
        target->unk58 = 15;
    }
}

/* Threat (0..0x1000) a gobj poses to its target; zero when it has none or is unarmed. */
void TargetSetThreatLevel(s32 id) {
    GameObjectRuntime *rt;
    TargetInfo *target;
    s32 kind;

    rt = gameObjectsInfo[id].runtime;
    target = rt->target;
    if (target->targetId < 0
        || (rt->id != gPlayerOffset
                ? *(u8 *)&gameObjectsInfo[rt->id].flags24 == 0
                      && (kind = INFO_WORD24(&gameObjectsInfo[rt->id]) & 0x3000) != 0x1000 && kind != 0x2000
                : gCurrentWeapon == 0)) {
        target->unkB8 = 0;
        target->threat = 0;
        target->flags &= ~2;
        target->unkB0 = target->unkAC;
    } else if (rt->npcState != 2 || rt->npc->unk48 == 2) {
        if (target->threat < 0xAB8) {
            target->unkB8 = 0;
        }
        target->threat = (target->unk58 << 12) / 100;
        if (target->threat <= 0) {
            target->threat = 1;
        }
    } else {
        target->threat = 1;
    }
    if (target->threat > 0x1000) {
        target->threat = 0x1000;
    }
}

void TargetCameraUpdate(void) {
    s16 id;

    if (gTargetModeFlag && (id = gObjPlayer->target->targetId) >= 0) {
        GameCameraSetModeTarget(1, 0, gameObjectsInfo[id].runtime, 0);
    } else {
        GameCameraSetModeTarget(0, 0, NULL, 0);
    }
}

/* Can rt see gobj `targetId`? 0: no; else TargetViewConeCheck's result, 3 when only in
 * target mode. `wide` selects the narrow aiming cone instead of all around. */
u8 func_80032988(GameObjectRuntime *rt, s32 targetId, u8 wide) {
    VECTOR32 tpos;
    VECTOR32 me;
    VECTOR32 dir;
    VECTOR32 side;
    MATRIX m;
    s32 angle;
    GameObjectRuntime *t;
    s32 fov;
    s32 dist;
    u8 vis;

    t = gameObjectsInfo[targetId].runtime;
    vis = 0;
    if (!(t->target->flags & 1)) {
        goto invisible;
    }
    if (t->flags23 & 2) {
        goto invisible;
    }
    me = rt->move->pos;
    tpos = t->move->pos;
    if (((void **)t->anim)[((TargetCamera *)*gCamera)->index] == NULL) {
        goto invisible;
    }
    dist = GameSysFuzzyMathXZDistance(&me, &tpos);
    if (GOBJ_TYPE(t->id) == 3 || dist <= 0x7D00) {
        goto visible;
    }
invisible:
    return 0;
visible:
    if (rt->id == gPlayerOffset) {
        m.m[0][0] = ((TargetCamera *)*gCamera)->rot[0][0];
        m.m[0][1] = -((TargetCamera *)*gCamera)->rot[0][1];
        m.m[0][2] = ((TargetCamera *)*gCamera)->rot[0][2];
        m.m[1][0] = -((TargetCamera *)*gCamera)->rot[1][0];
        m.m[1][1] = ((TargetCamera *)*gCamera)->rot[1][1];
        m.m[1][2] = -((TargetCamera *)*gCamera)->rot[1][2];
        m.m[2][0] = ((TargetCamera *)*gCamera)->rot[2][0];
        m.m[2][1] = -((TargetCamera *)*gCamera)->rot[2][1];
        m.m[2][2] = ((TargetCamera *)*gCamera)->rot[2][2];
        side.vx = m.m[0][0];
        side.vy = m.m[1][0];
        side.vz = m.m[2][0];
        dir.vx = -side.vz;
        dir.vy = 0;
        dir.vz = side.vx;
    } else if (rt->cas->flags & 0x100000) {
        dir.vx = rt->anim->frame->m[0][2];
        dir.vy = rt->anim->frame->m[1][2];
        dir.vz = rt->anim->frame->m[2][2];
        dir.vy = -dir.vy;
    } else {
        EGSFrameGetOrientation(rt->anim->frames[14], 0, &dir);
        dir.vx = -dir.vx;
        dir.vy = -dir.vy;
        dir.vz = -dir.vz;
    }
    dir.vy = 0;
    EGSGeometry_VectorGetXZAngleFromZAxis(&dir, &angle);
    if (rt->id == gPlayerOffset) {
        if (t->healthInfo->health <= 0) {
            vis = 0;
            if (!gTargetModeFlag) {
                t->target->flags &= ~1;
                goto out;
            }
        } else if (!gTargetModeFlag) {
            vis = 0;
            if (!(t->target->flags & 0x10)) {
                goto out;
            }
        }
        fov = gTargetModeFlag ? 0x800 : 0x271;
        if (!wide) {
            fov = 0x1000;
        }
        vis = TargetViewConeCheck(gPlayerOffset, &me, &dir, 0x7D00, fov, &tpos);
        if (gTargetModeFlag && !vis) {
            vis = 3;
        }
    }
out:
    return vis;
}

/* npc.c's NPC list iteration (npcList, getNPCIndex) */
#define FIRST_NPC(id)                       \
    {                                       \
        s32 i_;                             \
        getNPCIndex = 0;                    \
        for (i_ = 0; i_ < 6; i_++) {        \
            (id) = npcList[i_];             \
            if ((id) >= 0) {                \
                getNPCIndex = i_ + 1;       \
                break;                      \
            }                               \
        }                                   \
    }
#define NEXT_NPC(id)                                \
    {                                               \
        s32 i_;                                     \
        (id) = -1;                                  \
        for (i_ = getNPCIndex; i_ < 6; i_++) {      \
            (id) = npcList[i_];                     \
            if ((id) >= 0) {                        \
                getNPCIndex = i_ + 1;               \
                break;                              \
            }                                       \
        }                                           \
    }

/* Pick the nearest targetable NPC (fully visible first, else partly visible in target mode). */
/* old-style definition: TargetAcquire passes a stray second argument */
int func_80032E24(rt)
    GameObjectRuntime *rt;
{
    GameObjectRuntime *other;
    s32 best;
    s32 alt;
    u16 bestDist;
    u16 altDist;
    s32 id;
    s32 type;
    u8 ok;
    s32 vis;

    best = -1;
    alt = -1;
    altDist = gActivationDistance;
    bestDist = altDist;
    if (rt->npcState == 2) {
        best = gPlayerOffset;
        if (rt->npc != NULL && (rt->npc->flags & 1)) {
            best = NPCPickNPCTarget(rt->id);
        }
    } else {
        if (gTaserGobjID >= 0) {
            return gTaserGobjID;
        }
        FIRST_NPC(id);
        while (id >= 0) {
            other = gameObjectsInfo[id].runtime;
            if (other != NULL && other->healthInfo != NULL) {
                type = GOBJ_TYPE(id);
                if (rt->id == gPlayerOffset) {
                    ok = type != GOBJ_TYPE_COP && type != 0x4C && type != 0x5C;
                } else {
                    ok = 1;
                }
                if (other->healthInfo->health <= 0 || !ok) {
                    other->target->flags &= ~1;
                }
                if ((other->target->flags & 8) && other->id != rt->id) {
                    vis = func_80032988(rt, other->id, 1);
                    if (vis == 1 && gTargetModeFlag && !(other->target->flags & 0x10)) {
                        vis = 2;
                    }
                    if (vis == 1) {
                        if (TARGET_NPC(other)->dist < (s16)bestDist) {
                            best = other->id;
                            bestDist = TARGET_NPC(other)->dist;
                        }
                    } else if (gTargetModeFlag && vis != 0) {
                        if (TARGET_NPC(other)->dist < (s16)altDist) {
                            alt = other->id;
                            altDist = TARGET_NPC(other)->dist;
                        }
                    }
                }
            }
            NEXT_NPC(id);
        }
    }
    if (alt != -1 && best == -1) {
        best = alt;
    }
    return best;
}

void TargetInit(void) {
    int i;

    D_8010FFDC = 0;
    D_8010F821 = 0;
    for (i = 0; i < 8; i++) {
        mTargetTimerList[i].id = -1;
    }
    D_8010562A = *(u16 *)(gLevData + 0x4C);
}


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

/* Next target: the targetable NPC with the smallest clockwise angle from the current one. */
int func_800333F8(GameObjectRuntime *rt) {
    VECTOR32 tpos;
    VECTOR32 me;
    VECTOR32 d;
    s32 base;
    GameObjectRuntime *cur;
    GameObjectRuntime *other;
    s32 best;
    s32 bestAngle;
    s32 id;
    s32 type;
    u8 ok;

    best = -1;
    bestAngle = 0x1000;
    cur = gameObjectsInfo[rt->target->targetId].runtime;
    me = rt->move->pos;
    tpos = cur->move->pos;
    d.vx = tpos.vx - me.vx;
    d.vy = tpos.vy - me.vy;
    d.vz = tpos.vz - me.vz;
    EGSGeometry_VectorGetXZAngleFromZAxis(&d, &base);
    FIRST_NPC(id);
    while (id >= 0) {
        other = gameObjectsInfo[id].runtime;
        if (other != NULL) {
            if (other->healthInfo->health <= 0) {
                other->target->flags &= ~1;
            }
            type = GOBJ_TYPE(other->id);
            if (rt->id == gPlayerOffset) {
                ok = type != GOBJ_TYPE_COP && type != 0x4C;
            } else {
                ok = 1;
            }
            if ((other->target->flags & 8) && other->npcState == 2 && ok && id != rt->id && id != cur->id
                && func_80032988(rt, other->id, 0)) {
                VECTOR32 opos;
                s32 angle;

                opos = other->move->pos;
                d.vx = opos.vx - me.vx;
                d.vy = opos.vy - me.vy;
                d.vz = opos.vz - me.vz;
                EGSGeometry_VectorGetXZAngleFromZAxis(&d, &angle);
                angle -= base;
                if (angle < 0) {
                    angle += 0x1000;
                }
                if (angle >= 0 && angle <= bestAngle) {
                    best = id;
                    bestAngle = angle;
                }
            }
        }
        NEXT_NPC(id);
    }
    if (best < 0) {
        return rt->target->targetId;
    }
    return best;
}

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

/* CAS callback: aim at the current target, or straight ahead (camera or body facing). */
void TargetAimCAS(s16 id) {
    VECTOR32 dir;
    VECTOR32 aim;
    VECTOR32 side;
    MATRIX m;
    GameObjectRuntime *rt;
    GameObjectRuntime *t;

    rt = gameObjectsInfo[id].runtime;
    t = NULL;
    if (((rt->target->targetId >= 0
          && (t = gameObjectsInfo[rt->target->targetId].runtime, !(rt->cas->flags & 0x200)) && !gPlayerFlashlight)
         || (gTargetModeFlag && rt->target->targetId >= 0 && (rt->target->flags & 8)))
        && (!(t->npcState == 1 || t->npcState == 2)
            || ((t->anim->model->flags & GOBJ_MODEL_FLAG_400000) && !(t->anim->unk00[8] & 0x10) && t->move != NULL))
        && t->target != NULL && t->move != NULL) {
        AnimateGobjRequestAim(rt, NULL, 0, 0);
        return;
    }
    if ((rt->cas->flags & 0x300) == 0x100) {
        if (rt->id == gCASJoystickGobjID && rt == GameCameraGetPrimaryGobj()
            && (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) || GameCameraCheckMode(3)
                || GameCameraCheckMode(8) || GameCameraCheckMode(9))) {
            m.m[0][0] = ((TargetCamera *)*gCamera)->rot[0][0];
            m.m[0][1] = -((TargetCamera *)*gCamera)->rot[0][1];
            m.m[0][2] = ((TargetCamera *)*gCamera)->rot[0][2];
            m.m[1][0] = -((TargetCamera *)*gCamera)->rot[1][0];
            m.m[1][1] = ((TargetCamera *)*gCamera)->rot[1][1];
            m.m[1][2] = -((TargetCamera *)*gCamera)->rot[1][2];
            m.m[2][0] = ((TargetCamera *)*gCamera)->rot[2][0];
            m.m[2][1] = -((TargetCamera *)*gCamera)->rot[2][1];
            m.m[2][2] = ((TargetCamera *)*gCamera)->rot[2][2];
            side.vx = m.m[0][0];
            side.vy = m.m[1][0];
            side.vz = m.m[2][0];
            dir.vx = -side.vz;
            dir.vy = 0;
            dir.vz = side.vx;
        } else {
            VECTOR32 *v = &dir;

            v->vx = rt->anim->frame->m[0][2];
            v->vy = rt->anim->frame->m[1][2];
            v->vz = rt->anim->frame->m[2][2];
            v->vy = -v->vy;
            if (v->vy != 0) {
                v->vy = 0;
                GTEMath_VectorNormal((VECTOR *)v, (VECTOR *)v);
            }
        }
        aim.vx = EGSMathFIXMul(dir.vx, 0xA0000);
        aim.vy = EGSMathFIXMul(dir.vy, 0xA0000);
        aim.vz = EGSMathFIXMul(dir.vz, 0xA0000);
        aim.vx = rt->move->pos.vx + aim.vx;
        aim.vy = rt->move->pos.vy + aim.vy;
        aim.vz = rt->move->pos.vz + aim.vz;
        AnimateGobjRequestAim(rt, &aim, 1, 0);
    }
}

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
            s32 ref = gameObjectsInfo[rt->id].refId;
            if (gameObjectsInfo[ref].runtime->healthInfo->health > 0) {
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
        if (!(rt->cas->flags & 0x02000000) && !gPlayerFlashlight) {
            if (rt->target->targetId >= 0) {
                CASRequestAction(rt->id, 0x2B);
            } else {
                if (mode != 2) {
                    CASRequestAction(rt->id, 0x2D);
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

/* Fire rt's weapon at its target. Returns the hit chance on a hit, 0 on a miss. */
s32 TargetShoot(GameObjectRuntime *rt, u8 canHit) {
    VECTOR32 muzzle;
    VECTOR32 me;
    VECTOR32 tpos;
    VECTOR32 dir;
    VECTOR32 tdir;
    s32 dist;
    TargetInfo *target;
    GameObjectRuntime *t;
    GameObjectRuntime *r;
    s32 value;
    u16 fx;
    s16 weapon;
    s32 ref;
    s32 type;
    u8 visible;

    t = NULL;
    visible = 1;
    target = rt->target;
    value = target->unk58;
    if (rt->id == gPlayerOffset) {
        AnimateGobjRequestShotRecoil(rt, 0);
    } else {
        AnimateGobjRequestShotRecoil(rt, 1);
    }
    fx = target->unk60;
    if (target->unk60 == 0) {
        fx = gDefaultLevelDmgFX;
    }
    weapon = GOBJ_WEAPON(rt->id);
    dir.vx = rt->anim->frame->m[0][2];
    dir.vy = rt->anim->frame->m[1][2];
    dir.vz = rt->anim->frame->m[2][2];
    dir.vy = -dir.vy;
    me = rt->move->pos;
    if (rt->target->targetId >= 0) {
        t = gameObjectsInfo[rt->target->targetId].runtime;
        tdir.vx = t->anim->frame->m[0][2];
        tdir.vy = t->anim->frame->m[1][2];
        tdir.vz = t->anim->frame->m[2][2];
        tdir.vy = -tdir.vy;
        tpos = t->move->pos;
        if (WEAPON_KIND(weapon) == 2) {
            visible = TargetViewConeCheck(rt->id, &me, &dir, 0x7D00, 0x800, &tpos);
        }
    } else {
        CASRequestAction(rt->id, 0x2B);
    }
    target->lastTick = gExeTick;
    if (target->unk5E >= 0) {
        DamageGOBJ(rt->id, rt->id, target->unk5E, D_80105F58[weapon].damageFar, 0x892);
    }
    ref = gameObjectsInfo[rt->id].refId;
    if (ref >= 0 && rt->npcState == 2 && ref != target->unk5E && (rt->npc->flags & 0x20000000)) {
        r = gameObjectsInfo[ref].runtime;
        type = GOBJ_TYPE(r->id);
        if ((type == 0x37 || type == 0x2C) && r->healthInfo->health > 0) {
            DamageGOBJ(rt->id, rt->id, ref, D_80105F58[weapon].damageNear, 0x892);
        }
    }
    if (rt->target->targetId < 0 || !visible) {
        muzzle.vx = rt->anim->frame->pos.vx;
        muzzle.vy = rt->anim->frame->pos.vy;
        muzzle.vz = rt->anim->frame->pos.vz;
        muzzle.vy = -muzzle.vy;
        EGSGeometry_GetFuzzyDistancePoint2Point(&muzzle, &TARGET_SHOT(target)->hitPos, &dist);
        if (dist < 0x3C1) {
            value = D_80105F58[weapon].damageNear;
        } else {
            value = D_80105F58[weapon].damageFar;
        }
        if (TARGET_SHOT(target)->victim >= 0) {
            if (TARGET_SHOT(target)->victim != gPlayerOffset) {
                DamageGOBJ(rt->id, rt->id, TARGET_SHOT(target)->victim, value, WeaponGetDeathAction(weapon));
            }
            goto done;
        }
        if ((u16)(weapon - 6) < 2) {
            DamageRadius(rt->id, &TARGET_SHOT(target)->hitPos, value, 0x60, 0x20, 0x11, 0);
        }
        {
            s16 fx16;
            s32 id;
            s32 tex;
            VECTOR32 *hit;
            VECTOR32 *normal;

            fx16 = fx;
            id = rt->id;
            tex = BulletTextureGet(fx16);
            hit = &TARGET_SHOT(target)->hitPos;
            normal = &TARGET_SHOT(target)->hitNormal;
            if (tex != 0) {
                EGSDisplayRegisterDamage(hit, normal, &dir, tex, 0);
            }
            DamageTerrain(id, hit, normal, &dir, fx16);
        }
        if (rt->id == gPlayerOffset) {
            goto missed;
        }
        goto done;
    }
    if (target->unkBC != target->targetId) {
        target->unkBC = target->targetId;
        target->unkC0 = TARGET_SHOT(target)->prevTick = gExeTick;
        TARGET_SHOT(target)->hits = 0;
    } else if (rt->target->flags & 2) {
        TARGET_SHOT(target)->prevTick = target->unkC0;
        target->unkC0 = gExeTick;
        if (++TARGET_SHOT(target)->hits > 100) {
            TARGET_SHOT(target)->hits = 100;
        }
    }
    if (rt->npcState == 2) {
        if (t->id == gPlayerOffset && NPCDoNoDamage(rt->id)) {
            value = 0;
        }
        if (value >= 120) {
            return value;
        }
    } else if (value >= 100 || weapon == 16) {
        return 100;
    }
    if (!canHit || value <= 0 || value < rand() % 100) {
        VECTOR32 v;

        if (rt->npcState == 2) {
            GameObjectRuntime *vic;

            vic = NULL;
            if (TARGET_SHOT(rt->target)->victim >= 0) {
                vic = gameObjectsInfo[TARGET_SHOT(rt->target)->victim].runtime;
            }
            if (vic != NULL && !(vic->npcState == 1 || vic->npcState == 2) && vic->healthInfo->health > 0) {
                s32 saved = target->targetId;

                target->targetId = -1;
                DamageGOBJ(rt->id, rt->id, TARGET_SHOT(target)->victim, D_80105F58[weapon].damageFar, WeaponGetDeathAction(weapon));
                target->targetId = saved;
                goto done;
            }
            if ((rt->target->flags & 2) && target->targetId == gPlayerOffset) {
                if (func_80034D24(t, &me, &me, &v) == 1) {
                    s16 fx2 = gDefaultLevelDmgFX;
                    s32 id = rt->id;
                    s32 tex = BulletTextureGet(fx2);

                    if (tex != 0) {
                        EGSDisplayRegisterDamage(&me, &v, &v, tex, 0);
                    }
                    DamageTerrain(id, &me, &v, &v, fx2);
                }
            }
            return 0;
        }
        if (rt->target->flags & 2) {
            v.vx = me.vx - TARGET_SHOT(target)->hitPos.vx;
            v.vy = me.vy - TARGET_SHOT(target)->hitPos.vy;
            v.vz = me.vz - TARGET_SHOT(target)->hitPos.vz;
            TARGET_SHOT(target)->hitPos.vx += (v.vx >> 4) + (rand() & 0xFF) - 0x7F;
            TARGET_SHOT(target)->hitPos.vy += (v.vy >> 4) + (rand() & 0xFF) - 0x7F;
            TARGET_SHOT(target)->hitPos.vz += (v.vz >> 4) + (rand() & 0xFF) - 0x7F;
            DamageTerrain(rt->id, &TARGET_SHOT(target)->hitPos, &TARGET_SHOT(target)->hitNormal, &v, (s16)fx);
        } else if (TARGET_SHOT(target)->victim >= 0) {
            if (TARGET_SHOT(target)->victim != gPlayerOffset) {
                muzzle.vx = rt->anim->frame->pos.vx;
                muzzle.vy = rt->anim->frame->pos.vy;
                muzzle.vz = rt->anim->frame->pos.vz;
                muzzle.vy = -muzzle.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&muzzle, &TARGET_SHOT(target)->hitPos, &dist);
                DamageGOBJ(rt->id, rt->id, TARGET_SHOT(target)->victim,
                           dist < 0x3C1 ? D_80105F58[weapon].damageNear : D_80105F58[weapon].damageFar,
                           WeaponGetDeathAction(weapon));
            }
        } else {
            DamageTerrain(rt->id, &TARGET_SHOT(target)->hitPos, &TARGET_SHOT(target)->hitNormal,
                          &TARGET_SHOT(target)->hitNormal, (s16)fx);
        }
missed:
        gPlayerNumConsecutiveHits = gPlayerNumConsecutiveHits > 0 ? -1 : gPlayerNumConsecutiveHits - 1;
    done:
        return 0;
    }
    gPlayerNumConsecutiveHits = gPlayerNumConsecutiveHits < 0 ? 1 : gPlayerNumConsecutiveHits + 1;
    return value;
}

/* Pick a random polygon of rt's hit model facing `eye` (any if eye is NULL) and a random
 * point on it; returns 1 with the point and the polygon normal (y up), else 0. */
static u8 func_80034D24(GameObjectRuntime *rt, VECTOR32 *eye, VECTOR32 *point, VECTOR32 *normal) {
    VECTOR32 step;
    TargetPolyNode *node;
    TargetPolyNode *pick;
    TargetPoly *poly;
    s32 count;
    s32 r;
    s32 t;
    u32 size;
    u8 found;

    count = 0;
    for (node = TARGET_ANIM(rt)->polys; node != NULL; node = node->next) {
        count++;
    }
    if (count == 0) {
        return 0;
    }
    r = rand() % count;
    node = TARGET_ANIM(rt)->polys;
    for (count = 0; count < r;) {
        count++;
        node = node->next;
    }
    pick = node;
    if (eye != NULL) {
        found = 0;
        do {
            VECTOR32 d;
            TargetPoly *p;
            SVECTOR *v;
            SVECTOR *n;

            p = &node->poly;
            if ((u32)p->data & 3) {
                size = 0;
            } else {
                size = p->data[-1] & 0x7FFFFFFF;
            }
            if (size != 0) {
                v = p->verts[0];
                n = p->normal;
                d.vx = v->vx - eye->vx;
                d.vy = -v->vy - eye->vy;
                d.vz = v->vz - eye->vz;
                if (n->vx * d.vx - n->vy * d.vy + n->vz * d.vz <= 0) {
                    found = 1;
                    pick = node;
                }
            }
            node = node->next;
            if (node == NULL) {
                node = TARGET_ANIM(rt)->polys;
            }
        } while (!found && node != NULL && node != pick);
    } else {
        found = 1;
    }
    if (found == 1) {
        poly = &pick->poly;
        point->vx = poly->verts[0]->vx;
        point->vy = poly->verts[0]->vy;
        point->vz = poly->verts[0]->vz;
        for (count = 1; count < poly->numVerts; count++) {
            step.vx = poly->verts[count]->vx - point->vx;
            step.vy = poly->verts[count]->vy - point->vy;
            step.vz = poly->verts[count]->vz - point->vz;
            t = rand() & 0xFFF;
            step.vx = EGSMathFIXMul(step.vx, t);
            step.vy = EGSMathFIXMul(step.vy, t);
            step.vz = EGSMathFIXMul(step.vz, t);
            point->vx += step.vx;
            point->vy += step.vy;
            point->vz += step.vz;
        }
        point->vy = -point->vy;
        normal->vx = poly->normal->vx;
        normal->vy = -poly->normal->vy;
        normal->vz = poly->normal->vz;
    }
    return found;
}

#ifdef NON_MATCHING
/* scheduling: the first ratan2 result is stored right after the call; the original stores it after diff.vx */
/* Is `to` within `range` and the `fov` cone around `dir` seen from `from`?
 * 0: no, 1: inside the cone, 2: player only, close to the cone. */
s32 TargetViewConeCheck(s32 id, VECTOR32 *from, VECTOR32 *dir, s32 range, s32 fov, VECTOR32 *to) {
    s32 dist;
    s32 result;
    u8 visible;
    s32 d;

    result = 0;
    EGSGeometry_GetFuzzyDistancePoint2Point(from, to, &dist);
    visible = 1;
    if (dist < range) {
        VECTOR32 diff;
        struct {
            s32 dirYaw;
            s32 toYaw;
            s32 dYaw;
            s32 dirPitch;
            s32 toPitch;
            s32 dPitch;
        } a;

        a.dirYaw = ratan2(dir->vx, dir->vz);
        diff.vx = to->vx - from->vx;
        diff.vy = to->vy - from->vy;
        diff.vz = to->vz - from->vz;
        a.toYaw = ratan2(diff.vx, diff.vz);
        a.dYaw = a.toYaw - a.dirYaw;
        if (a.dYaw > 0x800) {
            a.dYaw -= 0x1000;
        } else if (a.dYaw < -0x800) {
            a.dYaw += 0x1000;
        }
        if (id == gPlayerOffset && WEAPON_KIND(gCurrentWeapon) == 2 && !gTargetModeFlag) {
            EGSGeometryINT_VectorGetAngleFromXZPlane(dir, &a.dirPitch);
            EGSGeometryINT_VectorGetAngleFromXZPlane(&diff, &a.toPitch);
            a.dPitch = a.toPitch - a.dirPitch;
            if (a.dPitch > 0x800) {
                a.dPitch -= 0x1000;
            } else if (a.dPitch < -0x800) {
                a.dPitch += 0x1000;
            }
            if (abs(a.dPitch) > fov >> 1) {
                visible = 0;
            }
        }
        d = abs(a.dYaw);
        if (fov >> 1 >= d) {
            result = 1;
        } else if (id == gPlayerOffset && (WEAPON_KIND(gCurrentWeapon) == 1 || dist < 0x281) && d < 0x401) {
            result = 2;
        }
    }
    if (!visible) {
        result = 0;
    }
    return result;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/target", TargetViewConeCheck);
#endif

