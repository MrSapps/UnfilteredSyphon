#include "common.h"
#include "level.h"
#include "system.h"
#include "weapon.h"

typedef struct Vec4 {
    s32 vx, vy, vz, pad;
} Vec4;

/* Hierarchy/transform block hanging off the runtime (+0x0C). */
typedef struct ChopperXform {
    /* 0x000 */ Vec4 curPos;
    /* 0x010 */ u8 unk10[0x30];
    /* 0x040 */ Vec4 pos;
    /* 0x050 */ s32 vel[3]; /* 20.12 */
    /* 0x05C */ u8 unk5C[0x64];
    /* 0x0C0 */ s32 rot[3];
    /* 0x0CC */ u8 unkCC[4];
    /* 0x0D0 */ s32 rotVel[3]; /* 20.12 */
    /* 0x0DC */ u8 unkDC[4];
    /* 0x0E0 */ Vec4 rotDelta;
    /* 0x0F0 */ u8 unkF0[0xA0];
    /* 0x190 */ s32 unk190;
} ChopperXform;

typedef struct ChopperModel {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
} ChopperModel;

typedef struct ChopperAnimParams {
    s32 p0, p1, p2, p3, p4;
} ChopperAnimParams;

typedef struct ChopperAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags8;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 flagsA;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ void *hierarchy;
    /* 0x10 */ ChopperModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ ChopperAnimParams *params;
} ChopperAnim;

typedef struct ChopperTarget {
    /* 0x00 */ s16 id;
    /* 0x02 */ s16 unk02;
    /* 0x04 */ u32 flags; /* bit 1: line of sight clear */
    /* 0x08 */ u8 unk08[0x50];
    /* 0x58 */ s16 unk58;
    /* 0x5A */ s16 hitId;
    /* 0x5C */ u8 unk5C[4];
    /* 0x60 */ s16 material;
    /* 0x62 */ s16 unk62;
    /* 0x64 */ Vec4 hitPos;
    /* 0x74 */ u8 unk74[0x10];
    /* 0x84 */ Vec4 normal;
} ChopperTarget;

/* GameObjectRuntime as used by this overlay */
typedef struct ChopperGobj {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ s16 id;
    /* 0x04 */ u32 state; /* damage level 0..3, 4 = destroyed */
    /* 0x08 */ ChopperAnim *anim;
    /* 0x0C */ ChopperXform *xform;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ ChopperTarget *target;
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ NPCInfo *npc;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 flags23;
} ChopperGobj;

/* gameObjectsInfo[] entry seen as its placement transform */
typedef struct ObjInfoXform {
    /* 0x00 */ s32 modelIndex;
    /* 0x04 */ s16 m[3][3];
    /* 0x16 */ s16 pad16;
    /* 0x18 */ s32 t[3];
    /* 0x24 */ u8 unk24;
    /* 0x25 */ u8 unk25[0x27];
} ObjInfoXform; /* size 0x4C */
#define OBJ_XFORM(id) (&((ObjInfoXform *)gameObjectsInfo)[id])
/* the same entry with the scaled index added first */
#define OBJ_XFORM_AT(id) ((ObjInfoXform *)((id) * sizeof(ObjInfoXform) + (u32)gameObjectsInfo))

/* Player world position as kept by the anim hierarchy */
typedef struct PlayerFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 x, y, z;
} PlayerFrame;
#define PLAYER_XFORM() (((PlayerFrame **)(((ChopperGobj *)gObjPlayer)->anim))[3])

/* Player transform block (gObjPlayer +0x0C) */
typedef struct PlayerXform {
    /* 0x000 */ Vec4 pos;
    /* 0x010 */ u8 unk10[0xFC];
    /* 0x10C */ Vec4 eyePos;
} PlayerXform;
#define PLAYER_XFORM2() ((PlayerXform *)((ChopperGobj *)gObjPlayer)->xform)

/* Line of sight query for _LOSPoint2Point */
typedef struct LOSQuery {
    /* 0x00 */ Vec4 *from;
    /* 0x04 */ Vec4 *to;
    /* 0x08 */ ChopperGobj *ignore;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10[3];
    /* 0x1C */ u8 *result;
    /* 0x20 */ Vec4 *hitPos;
    /* 0x24 */ Vec4 *normal;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
} LOSQuery;

typedef struct LOSBlockerObj {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id;
} LOSBlockerObj;
typedef struct LOSBlockerPoly {
    /* 0x00 */ s16 unk00;
    /* 0x02 */ s16 material;
} LOSBlockerPoly;
typedef struct LOSBlocker {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 kind; /* 1: object */
    /* 0x0C */ u8 unk0C[0x3C];
    /* 0x48 */ LOSBlockerObj *obj;
    /* 0x4C */ LOSBlockerPoly *poly;
} LOSBlocker;

/* gObjPlayer +0x0C points at the player's position */
#define PLAYER_POS() (*(Vec4 **)((u8 *)gObjPlayer + 0xC))

void MovementGobjLookAtPoint(ChopperGobj *gobj, Vec4 *point, s32 arg2);
void GTEMath_VectorNormal(Vec4 *in, Vec4 *out);
void EGSDisplayFlashlightOn(void *light);
void EGSDisplayFlashlightOff(void *light);
void CobjAddBulletCollisionBoxGobj(ChopperGobj *gobj, s32 arg1, s32 arg2, s32 arg3);
void CobjRemoveBulletCollisionBoxGobj(ChopperGobj *gobj);
void SysExeTickQueueTimer(void (*callback)(s32), s32 ticks, s32 arg);
void NPCEnableGenerator(s32 id);
s32 NPCCheckGenerators(void);
void NPCDisableGenerator(s32 id);

/* chopper.ovl statics, still defined in the overlay's asm .data/.bss */
/*
 * chopper.ovl's own data.  bss items 4 bytes apart are laid out as aspsx 2.56's .lcomm
 * rule does (docs/data_migration.md); aspsx 2.77 and maspsx pack them by size, so both
 * builds get explicit padding.  0x80145578..0x8014571F holds Vec4s and arrays at 4- or
 * 8-aligned offsets, which cannot be separate statics (a > 8-byte .lcomm is 16-aligned):
 * it is two storage blocks, and the code keeps addressing the parts through the original
 * labels (extern + .set below; under _PSYQ psylink's equates give them).
 */
#define BSS_PAD3(n) static u8 n; static s16 n##_2;

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0};

static char *D_801454D0[2] = {
    "INCOMING FROM LIAN:\n\nGabe, they've found me, and I'm under heavy fire.  These woods are "
    "swarming with troops.  But I can't take off until you disable the base's radar tracking "
    "system, or I'll be shot out of the sky.\n",
    "Hurry and disable that radar - there's a panel on the assembly itself.  Destroy it if you "
    "have to.  As soon as you do, I'll be able to dust off and get you out of there.  Hurry, I "
    "can't hold them off much longer...",
};
Briefing chopperPreSwitchBriefings = {
    {"Rozovka, Kazakhstan", "Rhoemer's Base", "09:01 21:30\n"}, 2, D_801454D0,
};
static char *D_801454EC[2] = {
    "INCOMING FROM MARKINSON:\n\nStay focused on the mission, Logan.  Yes, Lian is dead, but "
    "you've got more pressing concerns.  You've got to get out of there on foot now and still arm "
    "those explosives, or her death will have meant nothing.  The time for revenge is later.  \n",
    "The attack chopper they've sent after you is an old Russian prototype Ki-33, built primarily "
    "for troop transport.  It was never put into production because of it's weak armor plating, "
    "necessary for high speeds and heavy payloads.  You should be able to take it out if you're "
    "careful... and fast.  ",
};
/* briefings once the generator is down */
static Briefing D_801454F4 = {
    {"Rozovka, Kazakhstan", "Rhoemer's Base", "09:01 21:31\n"}, 2, D_801454EC,
};
static char *D_80145508[2] = {"Disable radar tracking", "Shoot down attack helicopter"};
static char *D_80145510 = "Radar Tracking Override."; /* use string */
static u32 D_80145514 = 0x01000240;                  /* texture animation */
static u32 D_80145518[4] = {0x01000250, 0x01000260, 0x01000270, 0x01400240}; /* its frames */
static LevelObjectives D_80145528 = {2, D_80145508}; /* saved objective/timer state */
static s32 D_80145550 = 0;
static s32 D_80145554 = 0;
static s32 D_80145558 = 0;
static s32 D_8014555C = 0;
static s32 D_80145560 = 0;
static u8 D_80145564 = 0;
static u8 D_80145565 = 0;
static u8 D_80145566 = 0;
static u8 D_80145567 = 0;
static u8 D_80145568 = 0;
static u8 D_80145569 = 0;
static u8 D_8014556A = 0;
static u8 D_8014556B = 0;
static u8 D_8014556C = 0;
static u8 D_8014556D = 0;
static u8 D_8014556E = 0;
static u8 D_8014556F = 0;
static u8 D_80145570 = 0;
static u8 D_80145571 = 0;
static u8 D_80145572 = 0;
/* the original .data ends in stray linker padding bytes ("...lisio") */
static u8 D_80145573 = 'l', D_80145574 = 'i', D_80145575 = 's', D_80145576 = 'i', D_80145577 = 'o';

static u8 D_80145578_s[0xC0]; /* 0x80145578..0x80145637, see above */
static u8 D_80145638_s[0xE8]; /* 0x80145638..0x8014571F */
static s32 D_80145720; /* gun mode */
static s32 D_80145724; /* previous gun mode */
static s32 D_80145728;
static s32 D_8014572C; /* altitude table */
static s32 D_80145730;
static s32 D_80145734;
static s32 D_80145738;
static s32 D_8014573C;
static s32 D_80145740;
static s32 D_80145744;
static s32 D_80145748;
static s32 D_8014574C; /* path info */
static s32 D_80145750;
static s32 D_80145754;
static s32 D_80145758;
static s32 D_8014575C;
static s32 D_80145760;
static s32 D_80145764; /* full health */
static s32 D_80145768[4]; /* health thresholds per damage level */
static s32 D_80145778;
static s32 D_8014577C; /* radar slot */
static s32 D_80145780; /* spotlight count */
static s32 D_80145784; /* chopper object id, -1 if none */
static s32 D_80145788;
static s32 D_8014578C; /* current song, -1 if none */
static s32 D_80145790;
static u8 D_80145794; BSS_PAD3(D_80145794_pad)
static u8 D_80145798; BSS_PAD3(D_80145798_pad) /* position set by message 0x2A */
static s32 D_8014579C; /* padding */

extern ChopperGobj *D_8014561C; /* the chopper */
extern u8 D_80145620[];         /* chopper searchlight */
extern Vec4 D_80145660;         /* chopper start position */
extern Vec4 D_80145670;         /* player position */
extern Vec4 D_80145680;         /* chopper facing direction */
Vec4 chopperPos; /* common: psylink lays it out after the .bss */

void func_801463A4(s32 id, s32 arg1);
void LevelGetPathInfo(s32 id, void *info, s32 *arg2);
s32 NPCAddGobjToRadar(s32 id);
void TargetInitGobj(ChopperGobj *gobj, ChopperTarget *target, s32 arg2, s32 arg3);
void MovementGobjInit(ChopperGobj *gobj, s32 arg1, Vec4 *pos, Vec4 *dir, s32 arg4);
void MovementAddGobj(ChopperGobj *gobj);
void EGSAnimSetBox(ChopperAnim *anim, s32 arg1, Vec4 *box);

extern NPCInfo D_80145578; /* the chopper's NPC block */
extern s16 D_801455A8;
extern s16 D_801455AE;
extern Vec4 D_801456E0; /* gun turret rotation */
extern Vec4 D_80145710; /* searchlight rotation */

/* Object types used by this level */
#define CHOPPER_TYPE_CHOPPER 3
#define CHOPPER_TYPE_GENERATOR 4
#define CHOPPER_TYPE_SPOTLIGHT 0x22
#define CHOPPER_TYPE_LIGHT_TARGET 0x5F

/* gPlayerOffset read as a halfword */
#define PLAYER_ID (*(s16 *)&gPlayerOffset)

int rand(void);
extern s32 gExeTick;
extern Vec4 gExploDir;
extern void *gBriefings;
void ParticleSmoke(s32 id, Vec4 *pos, Vec4 *dir, s32 scale, s32 arg4);
void ParticleExplosion(s32 id, Vec4 *pos, Vec4 *dir, s32 scale, s32 arg4);
void DamageGOBJ(s16 from, s16 by, s16 id, s16 amount, s16 type);


void InterfaceSetRadarWorldRadius(s32 arg0, s32 inner, s32 outer);
void EGSGeometryINT_GetDistancePointToPoint(Vec4 *a, Vec4 *b, s32 *dist);
void EGSGeometryFIX_VectorGetAngleFromXZPlane(Vec4 *dir, s32 *angles);
s32 ratan2(s32 y, s32 x);
s32 EGSMathFIXMul(s32 a, s32 b);
void WeaponReplace(ChopperGobj *gobj);
void EGSFrameUpdateHierarchy(void *hierarchy);
void EGSDisplayFlashlightInit(void *light);
void EGSDisplayFlashlightParams(void *light, s32 arg1, s32 arg2, s32 arg3);
void EGSDisplayFlashlightFrame(void *light, s32 arg1, s32 arg2);

extern Vec4 D_80145690;
extern Vec4 D_801456A0;
extern Vec4 D_801456B0;
extern Vec4 D_801456C0;
extern s32 D_80145638; /* searchlight field */

void EGSDisplayInitTextureAnimation(void *anim, void *frames, s32 arg2, s32 arg3, s32 arg4);
void ParticleInitBreath(void);

extern Vec4 D_801455CC;    /* patrol waypoints */
extern Vec4 D_801455DC;
extern Vec4 D_801455EC;
extern Vec4 D_801455FC;
extern s32 D_8014560C[4];  /* spotlights */

typedef struct SVec3 {
    s16 vx, vy, vz;
} SVec3;

void NPCRemoveDead(void);
void SysSetSongVolPan(s32 arg0, s32 song, ChopperGobj *gobj, s32 vol, s32 pan);
void LevelDoEOL(s32 arg0);

static void func_801421D8(u8 *facing);
static void func_80143B88(u8 visible);
static void func_801447EC(void);
static void func_80144958(void);

extern LOSBlocker *losBlocker;
extern u32 D_80106220; /* main-exe settings word; bits 6..15: chopper gun damage */
extern Vec4 D_801456D0;  /* gun sweep angles */
extern u8 D_801455C0;
extern Vec4 D_801456F0; /* spotlight the chopper is heading for */
extern Vec4 D_80145700; /* its facing */
void *memset(void *, int, unsigned int);
void EGSGeometry_IsIntersectXZPoint2UnrotBox(Vec4 *p, Vec4 *box, s32 arg2, u8 *inside);
void EGSGeometryINT_GetXZDistancePoint2Point(Vec4 *a, Vec4 *b, s32 *dist);
void EGSGeometry_VectorRotateYXZ(Vec4 *in, Vec4 *angles, Vec4 *out);

/* 20.12 fixed point shift that rounds toward zero */
#define SHR_TRUNC(x, n) ((x) < 0 ? -((-(x)) >> (n)) : (x) >> (n))
#define WRAP_ANGLE(a)          \
    if ((a) > 0x800) {         \
        (a) -= 0x1000;         \
    } else if ((a) < -0x800) { \
        (a) += 0x1000;         \
    }
#define FRAME_POS(frame) ((PlayerFrame *)(frame))
#define ANIM_MATRIX(anim) ((s16(*)[3])(anim)->hierarchy)
void EGSGeometry_AngleToVectorYX(Vec4 *angles, Vec4 *out);
void _LOSPoint2Point(LOSQuery *q);
void EGSGeometry_GetFuzzyDistancePoint2Point(Vec4 *a, Vec4 *b, s32 *dist);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(Vec4 *a, Vec4 *b, s32 *dist);
void ParticleGunFlash(s32 id, Vec4 *pos);
void EGSAnimGunFlash(ChopperAnim *anim, s32 arg1);
s32 WeaponGetDeathAction(s32 weapon);
s32 BulletTextureGet(s32 weapon);
void EGSDisplayRegisterDamage(Vec4 *pos, Vec4 *normal, Vec4 *dir, s32 tex, s32 arg4);
void DamageTerrain(s32 id, Vec4 *pos, Vec4 *normal, Vec4 *dir, s32 weapon);
void WeaponSubRound(ChopperGobj *gobj);
void EGSFrameSetRotationParentYXZ(s32 frame, Vec4 *rot);
void EGSMath_VectorScaleDownByPowerOf2(Vec4 *in, s32 shift, Vec4 *out);
void EGSFrameLookAtPoint(s32 frame, s32 arg1, Vec4 *point, s32 arg3);

static void func_80141538(Vec4 *target);
static void func_801415CC(s32 song);
static void func_8014162C(void);
static void func_8014175C(void);
static void func_801417A0(void);
static void func_8014185C(void);

/* Turn the chopper to face a point. */
static void func_80141538(Vec4 *target) {
    MovementGobjLookAtPoint(D_8014561C, target, 0);
    D_80145680.vx = target->vx - chopperPos.vx;
    D_80145680.vy = target->vy - chopperPos.vy;
    D_80145680.vz = target->vz - chopperPos.vz;
    GTEMath_VectorNormal(&D_80145680, &D_80145680);
}

static void func_801415CC(s32 song) {
    if (D_8014578C != song) {
        if (D_8014578C != -1) {
            SysStopSong(0, D_8014578C);
        }
        D_8014578C = song;
        SysPlaySong(0, song, 0, 0);
    }
}

static void func_8014162C(void) {
    Vec4 pos;

    EGSDisplayFlashlightOn(D_80145620);
    D_8014561C->anim->flags8 &= ~8;
    chopperPos = D_80145660;
    D_80145565 = 1;
    D_8014561C->xform->pos = chopperPos;
    D_80145670 = *PLAYER_POS();
    pos.vx = D_80145670.vx;
    pos.vy = chopperPos.vy;
    pos.vz = D_80145670.vz;
    func_80141538(&pos);
    D_80145790 = 2;
    D_8014556B = 1;
}

static void func_8014175C(void) {
    SysPostMessage(10, 4, D_8014561C->id, D_8014561C->id, 0, 0, 0, 0);
}

static void func_801417A0(void) {
    if (gObjectives != NULL && (gObjectives->completed & 1)) {
        func_8014162C();
        func_801415CC(0x13);
    } else {
        EGSDisplayFlashlightOff(D_80145620);
    }
    D_8014561C->flags23 &= ~2;
    D_8014561C->flags23 |= 1;
    CobjAddBulletCollisionBoxGobj(D_8014561C, 1, 0, 0);
    SysExeTickQueueTimer((void (*)(s32))func_8014175C, 1, 0);
}

static void func_8014185C(void) {
    if (gPlayerRadioFlag) {
        SysExeTickQueueTimer((void (*)(s32))func_8014185C, 1, 0);
    } else {
        D_80145572 = 0;
        func_801417A0();
    }
}

static void func_801418AC(void) {
    D_8014561C->flags23 |= 2;
    D_8014561C->flags23 &= ~1;
    CobjRemoveBulletCollisionBoxGobj(D_8014561C);
}

static u8 func_80141908(s32 id) {
    s32 refId;
    s32 ret;

    refId = gameObjectsInfo[id].refId;
    NPCEnableGenerator(refId);
    ret = NPCCheckGenerators();
    NPCDisableGenerator(refId);
    return ret;
}


/* The NPC tick/rate fields are written as struct members (ChopNPCTick) so they stay put;
 * the rate reads gWeaponInfo[22] as struct fields, which (unlike plain scalar externs)
 * conflict with the npc pointer store and so are not hoisted above it. The params are
 * copied in field order (the stores sink, so only local-alloc's tie order showed it). */
typedef struct {
    u8 pad[0x30];
    s16 tick;
    s16 pad32[2];
    s16 rate;
} ChopNPCTick;
/* Chopper object init (message 2). */
static void func_8014196C(s32 id) {
    Vec4 box[2];
    s32 pathInfo = 0;

    D_8014561C = (ChopperGobj *)gameObjectsInfo[id].runtime;
    D_8014561C->unk22 = 8;
    func_801463A4(D_8014561C->id, 0);
    D_8014561C->unk10 = 0;
    D_8014561C = (ChopperGobj *)gameObjectsInfo[id].runtime;
    D_8014561C->flags1 &= 0x80;
    D_8014561C->flags1 |= 0;
    D_8014561C->anim->flagsA |= 0x20;
    D_8014561C->anim->model->flags |= 0x8000;
    D_8014561C->unk21 = 0xFF;
    D_8014561C->anim->flagsA |= 0x40;
    pathInfo = 0;
    LevelGetPathInfo(D_8014561C->id, &D_8014574C, &pathInfo);
    D_8014577C = NPCAddGobjToRadar(D_8014561C->id);
    TargetInitGobj(D_8014561C, D_8014561C->target, 3, 2);
    D_8014561C->target->unk02 = 1;
    MovementGobjInit(D_8014561C, 0, &chopperPos, &D_80145680, 0);
    MovementAddGobj(D_8014561C);
    D_80145550 = D_8014561C->anim->params->p0;
    D_8014555C = D_8014561C->anim->params->p1;
    D_80145560 = D_8014561C->anim->params->p2;
    D_80145554 = D_8014561C->anim->params->p3;
    D_80145558 = D_8014561C->anim->params->p4;
    D_80145710.vy = 0x1E9;
    D_801456E0.vx = 0;
    D_801456E0.vy = 0;
    D_801456E0.vz = 0;
    D_80145710.vx = 0;
    D_80145710.vz = 0;
    D_8014561C->npc = &D_80145578;
    ((ChopNPCTick *)&D_80145578)->tick = gSystemTick;
    ((ChopNPCTick *)&D_80145578)->rate = gWeaponInfo[22].clipSize * ((gWeaponInfo[22].flags & 7) + 1);
    box[0].vx = -0xE0;
    box[0].vy = -0x1A0;
    box[0].vz = -0x6E0;
    box[1].vx = 0xE0;
    box[1].vy = 0x60;
    box[1].vz = 0x300;
    EGSAnimSetBox(D_8014561C->anim, 0, box);
}

#ifdef NON_MATCHING
/* scheduling only: the original loads pos.vy before D_8014574C and stores D_80145734 after D_80145730/D_80145758.
 * sched2 ties: both loads priority 1, luid order decides (ours has 282=D_8014574C before 249=pos.vy
 * after sched1). 60 random orders of the store statements, no-r, r-first and a y local: none better.
 * Not fixed by: moving any one statement of the block, 300 random statement orders, a twice-set h temp,
 * reading D_8014574C directly instead of r, D_80145750 = D_80145754 - (r >> 3).
 * r5C: root cause is sched1 luid order: a1 = r>>3 (insn 307) is placed right above the low-priority a1 arg copy
 * (birthing_insn_p boost), near the top, so sched2's luid tie-break puts sra a1 / lw 574C early. The original's
 * sched1 had 307 after the r>>1 sra. Twice-set temps for r>>3 (h = r; h >>= 3) don't change it. */
/* Level start: place the chopper and set up its flight envelope. */
static void func_80141C64(void) {
    Vec4 ofs;
    Vec4 pos;
    s32 dist;
    GameObjectHealth *health;
    s32 h;
    s32 i;
    s32 *p;
    s32 r;

    D_80145670.vx = PLAYER_XFORM()->x;
    D_80145670.vy = PLAYER_XFORM()->y;
    D_80145670.vz = PLAYER_XFORM()->z;
    D_80145670.vy = -D_80145670.vy;
    {
        ObjInfoXform *info = OBJ_XFORM_AT(D_8014561C->id);

        pos.vx = info->t[0];
        pos.vy = info->t[1];
        pos.vz = info->t[2];
    }
    if (!D_80145798) {
        ObjInfoXform *info;

        chopperPos = pos;
        info = OBJ_XFORM_AT(D_8014561C->id);
        D_80145680.vx = info->m[0][2];
        D_80145680.vy = info->m[1][2];
        D_80145680.vz = info->m[2][2];
    }
    D_8014561C->state = 0;
    health = D_8014561C->healthInfo;
    health->armor = 0xFA0;
    health->health = 0xFA0;
    D_80145764 = health->armor;
    D_80145768[0] = D_80145764;
    for (i = 3; i > 0; i--) {
        D_80145768[i] = D_80145764 * (4 - i) / 4;
    }
    D_80145748 = 0x1D40;
    D_80145738 = pos.vy - 0xB40;
    D_80145744 = pos.vy - 0x2D0;
    D_8014573C = pos.vy - 0x5A0;
    D_80145740 = pos.vy - 0x870;
    D_8014572C = pos.vy;
    r = D_8014574C;
    D_80145730 = pos.vy - 0x680;
    D_80145758 = r >> 4;
    D_80145734 = pos.vy + 0x3C;
    D_80145754 = r >> 1;
    D_80145750 = (r >> 1) - (r >> 3);
    D_8014561C->xform->unk190 = r;
    InterfaceSetRadarWorldRadius(0x14F, r >> 3, r);
    if (!D_80145798) {
        if (D_80145790 == 2) {
            chopperPos.vy = D_80145738;
            D_8014561C->anim->flags8 |= 8;
        } else {
            EGSGeometryINT_GetDistancePointToPoint(&chopperPos, &D_80145670, &dist);
            chopperPos.vy = D_8014572C + dist / 5;
        }
        D_8014561C->xform->pos = chopperPos;
    }
    EGSGeometryFIX_VectorGetAngleFromXZPlane(&D_80145680, D_8014561C->xform->rot);
    D_8014561C->xform->rot[1] = ratan2(D_80145680.vx, D_80145680.vz);
    D_8014561C->xform->rot[2] = 0;
    D_80145690 = chopperPos;
    D_80145690.vy = D_80145670.vy;
    ofs.vx = EGSMathFIXMul(D_801456C0.vx, 0x1900);
    ofs.vy = EGSMathFIXMul(D_801456C0.vy, 0x1900);
    ofs.vz = EGSMathFIXMul(D_801456C0.vz, 0x1900);
    D_801456B0.vx = D_801456A0.vx + ofs.vx;
    D_801456B0.vy = D_801456A0.vy + ofs.vy;
    D_801456B0.vz = D_801456A0.vz + ofs.vz;
    OBJ_XFORM(D_8014561C->id)->unk24 = 0x16;
    WeaponReplace(D_8014561C);
    EGSFrameUpdateHierarchy(D_8014561C->anim->hierarchy);
    EGSDisplayFlashlightInit(D_80145620);
    EGSDisplayFlashlightParams(D_80145620, 0x96, 0xC, 6);
    D_80145638 = 0x28;
    EGSDisplayFlashlightFrame(D_80145620, D_80145554, 0);
    if (gObjectives != NULL && (gObjectives->completed & 1)) {
        SysExeTickQueueTimer((void (*)(s32))func_8014185C, 0x78, 0);
        D_80145572 = 1;
    } else {
        func_801417A0();
    }
}
#else
INCLUDE_ASM("asm/chopper/nonmatchings/chopper", func_80141C64);
#endif

static const s32 D_80141480 = 0xB;

#ifndef _PSYQ
/* labels inside the bss storage blocks (see the top of the file) */
__asm__(".set D_80145578, D_80145578_s+0x0\n"
        ".set D_801455A8, D_80145578_s+0x30\n"
        ".set D_801455AE, D_80145578_s+0x36\n"
        ".set D_801455C0, D_80145578_s+0x48\n"
        ".set D_801455CC, D_80145578_s+0x54\n"
        ".set D_801455D0, D_80145578_s+0x58\n"
        ".set D_801455D4, D_80145578_s+0x5C\n"
        ".set D_801455DC, D_80145578_s+0x64\n"
        ".set D_801455E0, D_80145578_s+0x68\n"
        ".set D_801455E4, D_80145578_s+0x6C\n"
        ".set D_801455EC, D_80145578_s+0x74\n"
        ".set D_801455F0, D_80145578_s+0x78\n"
        ".set D_801455F4, D_80145578_s+0x7C\n"
        ".set D_801455FC, D_80145578_s+0x84\n"
        ".set D_80145600, D_80145578_s+0x88\n"
        ".set D_80145604, D_80145578_s+0x8C\n"
        ".set D_8014560C, D_80145578_s+0x94\n"
        ".set D_8014561C, D_80145578_s+0xA4\n"
        ".set D_80145620, D_80145578_s+0xA8\n"
        ".set D_80145638, D_80145638_s+0x0\n"
        ".set D_80145660, D_80145638_s+0x28\n"
        ".set D_80145670, D_80145638_s+0x38\n"
        ".set D_80145674, D_80145638_s+0x3C\n"
        ".set D_80145678, D_80145638_s+0x40\n"
        ".set D_80145680, D_80145638_s+0x48\n"
        ".set D_80145684, D_80145638_s+0x4C\n"
        ".set D_80145688, D_80145638_s+0x50\n"
        ".set D_80145690, D_80145638_s+0x58\n"
        ".set D_80145694, D_80145638_s+0x5C\n"
        ".set D_80145698, D_80145638_s+0x60\n"
        ".set D_801456A0, D_80145638_s+0x68\n"
        ".set D_801456A4, D_80145638_s+0x6C\n"
        ".set D_801456A8, D_80145638_s+0x70\n"
        ".set D_801456B0, D_80145638_s+0x78\n"
        ".set D_801456B4, D_80145638_s+0x7C\n"
        ".set D_801456B8, D_80145638_s+0x80\n"
        ".set D_801456C0, D_80145638_s+0x88\n"
        ".set D_801456C4, D_80145638_s+0x8C\n"
        ".set D_801456C8, D_80145638_s+0x90\n"
        ".set D_801456D0, D_80145638_s+0x98\n"
        ".set D_801456E0, D_80145638_s+0xA8\n"
        ".set D_801456E4, D_80145638_s+0xAC\n"
        ".set D_801456E8, D_80145638_s+0xB0\n"
        ".set D_801456F0, D_80145638_s+0xB8\n"
        ".set D_801456F4, D_80145638_s+0xBC\n"
        ".set D_801456F8, D_80145638_s+0xC0\n"
        ".set D_80145700, D_80145638_s+0xC8\n"
        ".set D_80145704, D_80145638_s+0xCC\n"
        ".set D_80145708, D_80145638_s+0xD0\n"
        ".set D_80145710, D_80145638_s+0xD8\n"
        ".set D_80145714, D_80145638_s+0xDC\n"
        ".set D_80145718, D_80145638_s+0xE0\n");
#endif

#ifdef NON_MATCHING
/*
 * Written from the m2c output; control flow follows the original but
 * scheduling, register allocation and some cross-jumped tails still differ.
 * The original also keeps its zero-initialised Vec4s in a .rodata constant
 * (D_80141484), which this version would emit itself.
 */
/* Per-frame flight logic: patrol, spotlight runs and strafing passes. */
static void func_801421D8(u8 *facingOut) {
    Vec4 savedPos;
    Vec4 move;
    Vec4 savedDir;
    Vec4 ang = { 0, 0, 0, 0 };
    Vec4 rotDelta = { 0, 0, 0, 0 };
    Vec4 tmp;
    Vec4 pt;
    Vec4 v;
    s32 d[2]; /* distance, d[1] */
    u8 inBoxA;
    u8 inBoxB;
    Vec4 rot;
    s32 dist2;
    u8 facing;
    u8 far;
    u8 approach;
    u8 diving;
    u8 prevLight;
    u8 light;
    u8 wasUp;
    s32 speed;
    s32 bank;
    s32 best;
    s32 best2;
    s32 spot;
    s32 i;
    s32 lim;
    s32 mode;
    s32 x;
    ObjInfoXform *info;
    ChopperXform *xf;

    D_801455C0 = 0;
    D_80145720 = 1;
    D_80145670 = *PLAYER_POS();
    chopperPos = D_8014561C->xform->curPos;
    D_80145680.vx = ANIM_MATRIX(D_8014561C->anim)[0][2];
    D_80145680.vy = ANIM_MATRIX(D_8014561C->anim)[1][2];
    D_80145680.vz = ANIM_MATRIX(D_8014561C->anim)[2][2];
    D_80145680.vy = -D_80145680.vy;
    v.vx = SHR_TRUNC(D_8014561C->xform->rotVel[0], 12);
    v.vy = SHR_TRUNC(D_8014561C->xform->rotVel[1], 12);
    v.vz = SHR_TRUNC(D_8014561C->xform->rotVel[2], 12);
    ang.vx = D_8014561C->xform->rot[0] + v.vx;
    WRAP_ANGLE(ang.vx);
    ang.vy = D_8014561C->xform->rot[1] + v.vy;
    WRAP_ANGLE(ang.vy);
    ang.vz = D_8014561C->xform->rot[2] + v.vz;
    WRAP_ANGLE(ang.vz);
    D_801456A0.vx = FRAME_POS(D_80145558)->x;
    D_801456A0.vy = FRAME_POS(D_80145558)->y;
    D_801456A0.vz = FRAME_POS(D_80145558)->z;
    D_801456A0.vy = -D_801456A0.vy;
    EGSGeometry_GetFuzzyXZDistancePoint2Point(&chopperPos, &D_80145670, &d[0]);
    facing = (D_80145670.vx - chopperPos.vx) * D_80145680.vx + (D_80145670.vz - chopperPos.vz) * D_80145680.vz >= 0;
    far = D_80145748 < d[0];

    if (D_8014556A != facing) {
        if (facing) {
            if (!D_8014556C && D_80145790 == 1) {
                /* pick the nearest spotlight to make a run on */
                best2 = 0x7FFFFFFF;
                best = 0x7FFFFFFF;
                spot = D_8014560C[0];
                D_8014556C = 1;
                D_8014556D = 0;
                D_8014556E = 0;
                D_8014556F = 0;
                D_80145788 = 0;
                for (i = 0; i < D_80145780; i++) {
                    s32 id = D_8014560C[i];

                    info = OBJ_XFORM((s16)id);
                    pt.vx = info->t[0];
                    pt.vy = info->t[1];
                    pt.vz = info->t[2];
                    EGSGeometry_GetFuzzyXZDistancePoint2Point(&pt, &chopperPos, &d[0]);
                    if (d[0] < best) {
                        spot = id;
                        best2 = best;
                        best = d[0];
                    } else if (d[0] < best2) {
                        best2 = d[0];
                    }
                }
                info = OBJ_XFORM((s16)spot);
                D_801456F0.vx = info->t[0];
                D_801456F0.vy = info->t[1];
                D_801456F0.vz = info->t[2];
                D_80145700.vx = info->m[0][2];
                D_80145700.vy = info->m[1][2];
                D_801456F0.vy = D_80145738;
                D_80145700.vz = info->m[2][2];
                func_80141538(&D_801456F0);
                D_80145784 = gameObjectsInfo[spot].refId;
            } else if (!D_8014556B && D_80145790 == 2) {
                D_8014561C->flags1 &= 0x80;
                D_8014561C->flags1 |= 0x5A;
                D_8014556B = 1;
                D_80145794 = rand() & 1;
                D_80145728 = d[0];
                D_80145570 = D_80145778 != 0;
            }
        }
        D_8014556A = facing;
    }
    approach = D_8014556C && !D_8014556D;
    diving = D_80145570 && D_80145790 == 1;

    if (!(D_8014561C->flags1 & 0x7F)) {
        rotDelta.vy = 0;
        if ((facing || far) && !approach) {
            D_801455C0 = 1;
            if (facing) {
                mode = 0x547;
                if (!far) {
                    mode = 0xAB8;
                }
            } else {
                mode = 0x547;
            }
            D_80145720 = mode;
            if (!D_8014556C) {
                if (D_8014578C == 0x13) {
                    if (D_80145720 == 0x547) {
                        func_801415CC(0x12);
                    }
                } else if (D_80145565 && D_80145720 == 0xAB8) {
                    func_801415CC(0x13);
                }
                tmp.vx = D_80145670.vx - chopperPos.vx;
                tmp.vz = D_80145670.vz - chopperPos.vz;
                tmp.vy = D_80145670.vy - chopperPos.vy;
                d[1] = ratan2(tmp.vx, tmp.vz);
                d[1] -= ang.vy;
                WRAP_ANGLE(d[1]);
                x = d[1] << 12;
                rotDelta.vy = SHR_TRUNC(x, 3);
                if (rotDelta.vy > 0x38E38) {
                    rotDelta.vy = 0x38E38;
                } else if (rotDelta.vy < -0x38E38) {
                    rotDelta.vy = -0x38E38;
                }
            }
        }
        x = rotDelta.vy * -8 - (ang.vz << 12);
        rotDelta.vz = SHR_TRUNC(x, 3);
    }

    savedPos = chopperPos;
    savedDir = D_80145680;
    if (D_80145680.vy != 0) {
        D_80145680.vy = 0;
        GTEMath_VectorNormal(&D_80145680, &D_80145680);
    }

    if (D_80145790 == 0) {
        move.vx = EGSMathFIXMul(D_80145680.vx, D_8014574C);
        move.vy = EGSMathFIXMul(D_80145680.vy, D_8014574C);
        move.vz = EGSMathFIXMul(D_80145680.vz, D_8014574C);
    } else {
        if (far) {
            speed = D_8014574C;
            inBoxB = 0;
            inBoxA = 0;
            D_8014575C = D_80145758;
        } else if (approach) {
            speed = D_8014574C;
            D_8014575C = speed;
        } else {
            EGSGeometry_IsIntersectXZPoint2UnrotBox(&chopperPos, &D_801455CC, 0, &inBoxA);
            if (!facing) {
                inBoxB = 0;
                speed = D_8014574C;
                D_8014575C = speed;
            } else if (inBoxA) {
                inBoxB = 0;
                if (diving) {
                    if (D_8014556E && !D_80145788) {
                        if (D_8014575C > 2) {
                            D_8014575C -= 2;
                            if (D_8014575C < 2) {
                                D_8014575C = 2;
                            }
                        }
                    } else if (D_8014556F) {
                        D_8014575C += 10;
                        if (D_8014575C >= D_80145750) {
                            D_80145570 = 0;
                        }
                        if (D_8014575C >= D_8014574C) {
                            D_8014575C = D_8014574C;
                        }
                    }
                    speed = D_8014575C;
                } else {
                    speed = D_8014574C;
                    D_8014575C = speed;
                }
            } else {
                if (!diving) {
                    EGSGeometry_IsIntersectXZPoint2UnrotBox(&chopperPos, &D_801455EC, 0, &inBoxB);
                } else {
                    inBoxB = 1;
                }
                if (!inBoxB) {
                    speed = D_8014574C;
                    D_8014575C = 0;
                } else {
                    if (D_80145744 < chopperPos.vy) {
                        if (!diving) {
                            lim = D_8014574C;
                        } else {
                            lim = D_80145754;
                        }
                        D_8014575C += 2;
                        if (lim < D_8014575C) {
                            D_8014575C = lim;
                        }
                    }
                    speed = D_8014575C;
                }
            }
        }

        if (!(D_8014561C->flags1 & 0x7F)) {
            if (!approach && (!D_8014556E || !inBoxA)) {
                move.vx = EGSMathFIXMul(D_80145680.vx, speed);
                move.vy = EGSMathFIXMul(D_80145680.vy, speed);
                move.vz = EGSMathFIXMul(D_80145680.vz, speed);
            } else {
                if (speed != 0) {
                    func_80141538(&D_801456F0);
                    EGSGeometry_GetFuzzyXZDistancePoint2Point(&D_801456F0, &chopperPos, &d[0]);
                } else {
                    d[0] = 0;
                }
                if (speed < d[0]) {
                    move.vx = EGSMathFIXMul(D_80145680.vx, speed);
                    move.vy = EGSMathFIXMul(D_80145680.vy, speed);
                    move.vz = EGSMathFIXMul(D_80145680.vz, speed);
                } else if (approach) {
                    /* arrived over the spotlight: line up with it */
                    D_80145680 = D_80145700;
                    D_8014575C = 0;
                    D_8014556F = 0;
                    D_80145788 = 0;
                    move.vx = D_801456F0.vx - chopperPos.vx;
                    move.vy = D_801456F0.vy - chopperPos.vy;
                    move.vz = D_801456F0.vz - chopperPos.vz;
                    D_8014556D = 1;
                    D_8014556E = 1;
                    D_80145570 = 1;
                    EGSGeometryFIX_VectorGetAngleFromXZPlane(&D_80145680, D_8014561C->xform->rot);
                    D_8014561C->xform->rot[1] = ratan2(D_80145680.vx, D_80145680.vz);
                    D_8014561C->xform->rot[2] = 0;
                    info = OBJ_XFORM((s16)D_80145784);
                    D_801456F0.vx = info->t[0];
                    D_801456F0.vy = info->t[1];
                    D_801456F0.vy = D_80145734;
                    D_801456F0.vz = info->t[2];
                    func_80141538(&D_801456F0);
                } else if (D_8014556E) {
                    if (D_8014575C != 0) {
                        D_8014575C = 0;
                        if (func_80141908(D_80145784)) {
                            D_80145788 = (rand() % 2 + 1) * 40;
                            D_80145569 = 1;
                        } else {
                            D_80145788 = 1;
                        }
                        move.vx = D_801456F0.vx - chopperPos.vx;
                        move.vy = D_801456F0.vy - chopperPos.vy;
                        move.vz = D_801456F0.vz - chopperPos.vz;
                    } else {
                        move.vx = 0;
                        move.vy = 0;
                        move.vz = 0;
                        if (--D_80145788 != 0) {
                            if (D_80145788 % 40 == 0 && !func_80141908(D_80145784)) {
                                D_80145788 = 1;
                            }
                        } else {
                            D_8014556E = 0;
                            D_8014556F = 1;
                        }
                    }
                }
                if (approach) {
                    rotDelta.vy = 0;
                } else if (D_8014556E || D_8014556F) {
                    D_80145680 = D_80145700;
                    EGSGeometryFIX_VectorGetAngleFromXZPlane(&savedDir, D_8014561C->xform->rot);
                    D_8014561C->xform->rot[1] = ratan2(savedDir.vx, savedDir.vz);
                    D_8014561C->xform->rot[2] = 0;
                }
            }
        } else {
            /* strafing pass: circle around the player */
            Vec4 step = { 0, D_80145794 ? -0xB : 0xB, 0 };

            rot = step;
            D_80145720 = 0x547;
            D_8014561C->flags1 = (D_8014561C->flags1 & 0x80) | ((D_8014561C->flags1 - 1) & 0x7F);
            tmp.vx = chopperPos.vx - D_80145670.vx;
            pt.vx = D_80145670.vx;
            pt.vy = D_80145738;
            pt.vz = D_80145670.vz;
            tmp.vy = chopperPos.vy - D_80145738;
            tmp.vz = chopperPos.vz - D_80145670.vz;
            tmp.vy = 0;
            GTEMath_VectorNormal(&tmp, &tmp);
            EGSGeometry_VectorRotateYXZ(&tmp, &rot, &tmp);
            v.vx = EGSMathFIXMul(tmp.vx, D_80145728);
            v.vy = EGSMathFIXMul(tmp.vy, D_80145728);
            v.vz = EGSMathFIXMul(tmp.vz, D_80145728);
            rotDelta.vy = 0;
            v.vx = pt.vx + v.vx;
            v.vy = pt.vy + v.vy;
            v.vz = pt.vz + v.vz;
            move.vx = v.vx - chopperPos.vx;
            move.vy = v.vy - chopperPos.vy;
            move.vz = v.vz - chopperPos.vz;
            MovementGobjLookAtPoint(D_8014561C, &pt, 0);
        }
    }

    chopperPos.vx += move.vx;
    chopperPos.vy += move.vy;
    chopperPos.vz += move.vz;
    EGSGeometryINT_GetXZDistancePoint2Point(&chopperPos, &D_80145670, &dist2);

    if (D_80145790 == 0) {
        chopperPos.vy = D_8014572C + dist2 / 5;
        if (facing) {
            x = 0x1C71C7 - (ang.vx << 12);
        } else {
            x = -0x71C71 - (ang.vx << 12);
        }
        rotDelta.vx = SHR_TRUNC(x, 4);
    } else {
        bank = 0;
        if (inBoxA) {
            if (facing) {
                if (D_80145738 < chopperPos.vy && !D_80145565) {
                    D_80145565 = 1;
                    D_8014561C->anim->flags8 &= ~8;
                }
                if (chopperPos.vy >= D_80145734 || (chopperPos.vy += D_80145760, D_80145734 < chopperPos.vy)) {
                    chopperPos.vy = D_80145734;
                    D_80145760 = 0;
                }
                if (diving && D_8014575C != 0) {
                    bank = 0x38E38;
                    if (!D_8014556E) {
                        bank = 0x1C71C7;
                    }
                }
            } else {
                D_80145760 = 0;
                chopperPos.vy -= 10;
            }
        } else {
            prevLight = D_80145566;
            if (far || (!facing && !D_80145778)) {
                D_80145760 = 0;
                chopperPos.vy = D_80145738;
            } else if (facing && inBoxB) {
                if (D_8014573C < chopperPos.vy) {
                    if (D_80145744 < chopperPos.vy) {
                        bank = 0x71C71;
                        if (D_80145790 != 1) {
                            bank = 0x1C71C7;
                        }
                    }
                    D_80145760 -= 2;
                    if (D_80145760 < 2) {
                        D_80145760 = 2;
                    }
                    chopperPos.vy += D_80145760 > 0x14 ? 0x14 : D_80145760;
                    if (D_80145734 < chopperPos.vy) {
                        chopperPos.vy = D_80145734;
                    }
                } else {
                    D_80145760 += 2;
                    chopperPos.vy += D_80145760 > 0x14 ? 0x14 : D_80145760;
                }
            } else {
                chopperPos.vy -= 0x1E;
                bank = 0xE38E3;
                if (chopperPos.vy < D_80145738) {
                    chopperPos.vy = D_80145738;
                }
            }
            wasUp = D_80145565;
            D_80145565 = D_80145738 < chopperPos.vy;
            if (wasUp != D_80145565) {
                if (D_80145778) {
                    if (D_80145565) {
                        D_8014561C->anim->flags8 &= ~8;
                        func_801415CC(0x13);
                    } else {
                        D_8014561C->anim->flags8 |= 8;
                    }
                } else if (D_80145572 || gObjectives == NULL || !(gObjectives->completed & 1)) {
                    D_80145565 = 0;
                    func_801418AC();
                    return;
                } else if (chopperPos.vy == D_80145738) {
                    chopperPos.vy++;
                    D_80145565 = 1;
                    D_8014561C->xform->pos = chopperPos;
                }
            }
            if (!D_80145565 && D_80145738 < chopperPos.vy) {
                D_80145565 = 1;
                D_8014561C->anim->flags8 &= ~8;
            }
            v.vx = FRAME_POS(D_80145554)->x;
            v.vy = FRAME_POS(D_80145554)->y;
            v.vz = FRAME_POS(D_80145554)->z;
            v.vy = -v.vy;
            light = 0;
            if (D_8014561C->state < 3) {
                light = v.vy >= D_80145730;
            }
            D_80145566 = light;
            if (prevLight != light) {
                if (D_80145565) {
                    EGSDisplayFlashlightOn(D_80145620);
                } else {
                    EGSDisplayFlashlightOff(D_80145620);
                }
            }
        }
        x = bank - (ang.vx << 12);
        rotDelta.vx = SHR_TRUNC(x, 4);
    }

    move.vx = chopperPos.vx - savedPos.vx;
    move.vz = chopperPos.vz - savedPos.vz;
    move.vx <<= 12;
    move.vz <<= 12;
    move.vy = chopperPos.vy - savedPos.vy;
    move.vy <<= 12;
    D_8014561C->xform->vel[0] += move.vx;
    D_8014561C->xform->vel[1] += move.vy;
    D_8014561C->xform->vel[2] += move.vz;
    D_8014561C->xform->rotDelta = rotDelta;
    D_801456C0 = savedDir;
    D_801456D0 = ang;
    *facingOut = facing;
}
#else
INCLUDE_ASM("asm/chopper/nonmatchings/chopper", func_801421D8);
#endif

#ifdef NON_MATCHING
/*
 * Same logic; gcc keeps &D_801456B0 / &D_80145690 in callee-saved registers
 * where the original rematerialises them, which shifts register allocation.
 * r4D: it is cse: the block copy's address pseudo (aim = D_801456B0, look = D_80145690)
 * replaces the symbol in one later `D_801456B0.vx = ...` store reached on the same cse path,
 * so the pseudo lives across calls. Moving `aim = D_801456B0;` after `track = 1;` gives 22
 * (fixes the D_80145565 load order) but the cse reuse remains.
 * r5C: the original's copy is movstrsi with a CONSTANT source (output_block_move does `la a2,sym` itself and
 * has only 3 temps left: 3 lw/sw + 1 lw/sw), i.e. combine folded the address pseudo into the movstr because cse
 * never reused it. The reused store is the else-branch one (D_80145565 == 0), reached by cse follow-jumps
 * (taken beqz, label preceded by BARRIER); a goto/if(0) layout of the else body is followed the same way.
 */
/* Swing the gun and searchlight toward their targets. */
static void func_80143B88(u8 visible) {
    Vec4 dir;
    Vec4 ofs;
    Vec4 aim;
    Vec4 look;
    s32 dist;
    u8 track;
    u8 fire;

    aim = D_801456B0;
    D_80145571 = 0;
    track = 1;
    if (D_80145565) {
        D_801456E0.vy += 0x11C;
        if (D_801456E0.vy > 0x800) {
            D_801456E0.vy -= 0x1000;
        } else if (D_801456E0.vy < -0x800) {
            D_801456E0.vy += 0x1000;
        }
        D_80145710.vy -= 0x11C;
        if (D_80145710.vy > 0x800) {
            D_80145710.vy -= 0x1000;
        } else if (D_80145710.vy < -0x800) {
            D_80145710.vy += 0x1000;
        }
        EGSFrameSetRotationParentYXZ(D_8014555C, &D_801456E0);
        EGSFrameSetRotationParentYXZ(D_80145560, &D_80145710);
        EGSGeometry_GetFuzzyXZDistancePoint2Point(&aim, &D_80145670, &dist);
        if (!D_80145570 || !visible) {
            ofs.vx = EGSMathFIXMul(D_801456C0.vx, 0x1900);
            ofs.vy = EGSMathFIXMul(D_801456C0.vy, 0x1900);
            ofs.vz = EGSMathFIXMul(D_801456C0.vz, 0x1900);
            D_801456B0.vx = D_801456A0.vx + ofs.vx;
            D_801456B0.vy = D_801456A0.vy + ofs.vy;
            D_801456B0.vz = D_801456A0.vz + ofs.vz;
        } else {
            dir.vx = D_80145670.vx - aim.vx;
            dir.vy = D_80145670.vy - aim.vy;
            dir.vz = D_80145670.vz - aim.vz;
            if (dist > 0xE0) {
                EGSMath_VectorScaleDownByPowerOf2(&dir, 5, &ofs);
            } else {
                GTEMath_VectorNormal(&dir, &dir);
                ofs.vx = EGSMathFIXMul(dir.vx, dist >= 0x17 ? 0x17 : dist);
                ofs.vy = EGSMathFIXMul(dir.vy, dist >= 0x17 ? 0x17 : dist);
                ofs.vz = EGSMathFIXMul(dir.vz, dist >= 0x17 ? 0x17 : dist);
                D_80145571 = dist < 0x20;
            }
            D_801456B0.vx = aim.vx + ofs.vx;
            D_801456B0.vy = aim.vy + ofs.vy;
            D_801456B0.vz = aim.vz + ofs.vz;
            D_801456C0.vx = D_801456B0.vx - D_801456A0.vx;
            D_801456C0.vy = D_801456B0.vy - D_801456A0.vy;
            D_801456C0.vz = D_801456B0.vz - D_801456A0.vz;
            GTEMath_VectorNormal(&D_801456C0, &D_801456C0);
        }
        fire = 0;
        if (visible && D_8014561C->healthInfo->health > 0 && D_801456A0.vy >= D_80145730 &&
            (dist <= 0x1900 || D_80145790 != 0)) {
            fire = 1;
        }
        D_80145564 = fire;
        if (fire) {
            D_801455C0 = 2;
            D_80145720 = 0x1000;
        }
    } else {
        D_80145564 = 0;
        ofs.vx = EGSMathFIXMul(D_801456C0.vx, 0x1900);
        ofs.vy = EGSMathFIXMul(D_801456C0.vy, 0x1900);
        ofs.vz = EGSMathFIXMul(D_801456C0.vz, 0x1900);
        D_801456B0.vx = D_801456A0.vx + ofs.vx;
        D_801456B0.vy = D_801456A0.vy + ofs.vy;
        D_801456B0.vz = D_801456A0.vz + ofs.vz;
    }
    EGSFrameLookAtPoint(D_80145558, 0, &D_801456B0, 0);
    if (D_80145566 && visible) {
        look = D_80145690;
        EGSGeometry_GetFuzzyXZDistancePoint2Point(&look, &D_80145670, &dist);
        if (dist <= 0x640 || D_80145790 != 0) {
            dir.vx = D_80145670.vx - look.vx;
            dir.vy = D_80145670.vy - look.vy;
            dir.vz = D_80145670.vz - look.vz;
            if (dist > 0xA0) {
                EGSMath_VectorScaleDownByPowerOf2(&dir, 2, &ofs);
            } else {
                GTEMath_VectorNormal(&dir, &dir);
                ofs.vx = EGSMathFIXMul(dir.vx, dist >= 0x40 ? 0x40 : dist);
                ofs.vy = EGSMathFIXMul(dir.vy, dist >= 0x40 ? 0x40 : dist);
                ofs.vz = EGSMathFIXMul(dir.vz, dist >= 0x40 ? 0x40 : dist);
            }
            D_80145690.vx = look.vx + ofs.vx;
            D_80145690.vy = look.vy + ofs.vy;
            D_80145690.vz = look.vz + ofs.vz;
        } else {
            ofs.vx = EGSMathFIXMul(D_80145680.vx, 0xC80);
            ofs.vy = EGSMathFIXMul(D_80145680.vy, 0xC80);
            ofs.vz = EGSMathFIXMul(D_80145680.vz, 0xC80);
            D_80145690.vx = chopperPos.vx + ofs.vx;
            D_80145690.vy = chopperPos.vy + ofs.vy;
            D_80145690.vz = chopperPos.vz + ofs.vz;
            D_80145690.vy = D_80145670.vy;
        }
        track = 0;
    }
    if (track) {
        D_80145690 = chopperPos;
        D_80145690.vy = D_80145670.vy;
    }
    EGSFrameLookAtPoint(D_80145554, 0, &D_80145690, 0);
}
#else
INCLUDE_ASM("asm/chopper/nonmatchings/chopper", func_80143B88);
#endif

/* `full` gets a3 through global.c expand_preferences: dmg = full >> k shares dmg's a3 preference
 * (from the DamageGOBJ arg). The terrain path's id must be its own variable, or dmg also
 * prefers a0 (DamageTerrain arg copy) and full takes a0 first in allocation order. */
/* Aim the chopper's gun at the player and fire a burst. */
void ChopperTargeting(s32 *mode, u8 *active) {
    Vec4 ofs;
    Vec4 aim;
    Vec4 hitPos;
    Vec4 playerPos;
    LOSQuery los;
    u8 clear;
    s32 distPlayer;
    s32 distTarget;
    s32 hitId;
    s32 dmg;
    ChopperTarget *tgt;
    u8 fire;
    ChopperGobj *hit;
    s32 action;


    *active = D_80145565;
    *mode = D_80145720;
    D_8014561C->target->id = gPlayerOffset;
    if (D_80145564) {
        hitId = -1;
        dmg = 0;
        tgt = D_8014561C->target;
        fire = 1;
        tgt->unk58 = 100;
        D_801455A8 = gSystemTick;
        if (!D_80145570) {
            D_801456D0.vx += 0x38;
            EGSGeometry_AngleToVectorYX(&D_801456D0, &D_801456C0);
            GTEMath_VectorNormal(&D_801456C0, &D_801456C0);
            ofs.vx = EGSMathFIXMul(D_801456C0.vx, 0x1900);
            ofs.vy = EGSMathFIXMul(D_801456C0.vy, 0x1900);
            ofs.vz = EGSMathFIXMul(D_801456C0.vz, 0x1900);
        } else {
            ofs.vx = EGSMathFIXMul(D_801456C0.vx, 0x1900);
            ofs.vy = EGSMathFIXMul(D_801456C0.vy, 0x1900);
            ofs.vz = EGSMathFIXMul(D_801456C0.vz, 0x1900);
        }
        if (!D_80145571) {
            aim.vx = D_801456A0.vx + ofs.vx;
            aim.vy = D_801456A0.vy + ofs.vy;
            aim.vz = D_801456A0.vz + ofs.vz;
        } else {
            aim = D_80145670;
        }
        los.from = &D_801456A0;
        los.to = &aim;
        los.ignore = D_8014561C;
        los.unk0C = 0;
        los.result = &clear;
        los.hitPos = &hitPos;
        los.normal = &tgt->normal;
        los.unk28 = 0;
        los.unk2C = 0;
        los.unk30 = 0;
        _LOSPoint2Point(&los);
        {
            LOSBlocker *blk = losBlocker;

            tgt->flags = (tgt->flags & ~2) | ((clear & 1) << 1);
            tgt->hitPos = hitPos;
            if (blk != NULL) {
                if (blk->kind == 1) {
                    hitId = blk->obj->id & 0x3FF;
                } else {
                    hitId = -1;
                }
                {
                    s32 m = losBlocker->poly->material;
                    tgt->material = m & 0x3F;
                }
            }
        }
        if (hitId >= 0) {
            hit = (ChopperGobj *)gameObjectsInfo[hitId].runtime;
            if (hit->unk22 != 2 || hit->healthInfo->health <= 0) {
                dmg = D_80106220 >> 6;
                dmg &= 0x3FF;
            } else {
                fire = 0;
            }
        } else {
            playerPos = PLAYER_XFORM2()->eyePos;
            EGSGeometry_GetFuzzyDistancePoint2Point(&hitPos, &playerPos, &distPlayer);
            EGSGeometry_GetFuzzyDistancePoint2Point(&hitPos, &D_80145670, &distTarget);
            if (!D_80145570 && (distPlayer < 0x8C || distTarget < 0x8C)) {
                s32 full = D_80106220 >> 6;

                full &= 0x3FF;

                hit = (ChopperGobj *)gObjPlayer;
                dmg = (distPlayer >= 0x46 && distTarget >= 0x46) ? full >> 2 : full >> 1;
            } else {
                fire = 0;
                if (distPlayer < 0x514 || distTarget < 0x514) {
                    fire = 1;
                }
                hit = NULL;
            }
        }
        if (fire) {
            ParticleGunFlash(D_8014561C->id, &hitPos);
            EGSAnimGunFlash(D_8014561C->anim, 3);
            if (hit != NULL) {
                D_8014561C->target->hitId = hit->id;
                action = WeaponGetDeathAction(0x16);
                DamageGOBJ(D_8014561C->id, D_8014561C->id, hit->id, dmg, action);
            } else {
                s32 tex;
                Vec4 *normal;
                s32 weapon;
                s32 id;

                id = D_8014561C->id;
                tex = BulletTextureGet(9);
                normal = &tgt->normal;
                weapon = 9;
                if (tex) {
                    EGSDisplayRegisterDamage(&hitPos, normal, &D_80145680, tex, 0);
                }
                DamageTerrain(id, &hitPos, normal, &D_80145680, weapon);
            }
            SysPlaySong(1, 9, 0, 0);
            WeaponSubRound(D_8014561C);
        }
    } else {
        D_8014561C->target->unk58 = 0;
    }
}

/* Pick the chopper's next attack pattern. */
static void func_801447EC(void) {
    s32 mask;
    u8 flag;

    if (D_80145778 == 0) {
        D_80145778 = 1;
    }
    if (D_8014561C->state != 0) {
        mask = 1;
        if (D_8014561C->state < 3) {
            flag = D_80145568 && D_80145569;
        } else {
            mask = 3;
            flag = D_80145567;
        }
        if (!flag || (rand() & mask) == mask) {
            if (D_8014561C->state < 3) {
                D_80145790 = 1;
                D_80145568 = 1;
            } else {
                D_80145790 = 2;
                D_80145567 = 1;
            }
            goto end;
        }
    }
    D_80145790 = 0;
end:
    if (D_8014556E) {
        D_8014556E = 0;
    }
    D_8014556B = 0;
    D_8014556C = 0;
    D_8014556D = 0;
    D_8014556E = 0;
    D_8014556F = 0;
    D_80145570 = 0;
    D_80145788 = 0;
}

/* Damage smoke / fire trailing from the chopper. */
static void func_80144958(void) {
    if (D_80145565 && (gExeTick & 4) && D_8014561C->state != 0) {
        if (D_8014561C->state < 3) {
            ParticleSmoke(-1, &chopperPos, &gExploDir, 0x1000, 1);
        } else {
            ParticleExplosion(-1, &chopperPos, &gExploDir, 0x1000, 0);
        }
    }
}

/* Destroyed callback */
static void func_80144A00(s16 id) {
    GameObjectRuntime *rt;

    if (GOBJ_TYPE(id) == CHOPPER_TYPE_GENERATOR) {
        rt = gameObjectsInfo[id].runtime;
        if (gObjectives == NULL || !(gObjectives->completed & 1)) {
            rt->flags0 |= 0x20;
            SysPlayXA(0x2E, 0, 1);
            SysExeTickQueueTimer((void (*)(s32))func_8014185C, 0x1CC, 0);
            D_80145572 = 1;
            if (gObjectives != NULL) {
                if (!(gObjectives->unk18 & 1)) {
                    LevelObjectiveAdd(0, -1);
                }
                gObjectives->completed |= 1;
            }
            gBriefings = &D_801454F4;
            LevelObjectiveAdd(1, -1);
            LevelObjectiveComplete(0);
        }
    }
}

/* Computer use callback */
static s32 func_80144B4C(s16 id) {
    s32 hp;

    hp = gameObjectsInfo[id].runtime->healthInfo->health;
    if (hp > 0) {
        DamageGOBJ(-1, PLAYER_ID, id, hp, 0xF);
    }
    return 0;
}

static void *func_80144BC0(void) {
    return D_80145510;
}

void iChopperLevelInit(void) {
    s32 i;
    s32 generator;
    s32 type;
    s32 ref;

    EGSDisplayInitTextureAnimation(&D_80145514, D_80145518, 4, 4, 0);
    D_801455CC.vx = 0x541;
    D_801455EC.vx = -0x37;
    D_801455CC.vy = 0x320;
    D_801455EC.vy = 0x320;
    D_801455CC.vz = -0x1708;
    D_801455EC.vz = -0x1C80;
    D_801455DC.vx = 0x1549;
    D_801455FC.vx = 0x1AC1;
    D_801455DC.vy = 0x320;
    D_801455FC.vy = 0x320;
    D_801455DC.vz = -0xC7F;
    D_801455FC.vz = -0x707;
    D_80145720 = 1;
    D_80145724 = 1;
    D_8014578C = -1;
    D_80145728 = 0;
    D_80145744 = 0;
    D_80145740 = 0;
    D_8014573C = 0;
    D_80145738 = 0;
    D_8014572C = 0;
    D_80145748 = 0;
    D_80145760 = 0;
    D_8014575C = 0;
    D_80145758 = 0;
    D_80145764 = 0;
    D_80145778 = 0;
    D_8014577C = 0;
    D_80145780 = 0;
    D_80145788 = 0;
    D_80145790 = 2;
    D_80145564 = 0;
    D_80145565 = 0;
    D_80145566 = 0;
    D_80145567 = 0;
    D_80145568 = 0;
    D_80145569 = 0;
    D_8014556A = 0;
    D_80145794 = 0;
    D_8014556C = 0;
    D_8014556D = 0;
    D_8014556E = 0;
    D_8014556F = 0;
    D_80145570 = 0;
    D_80145571 = 0;
    D_80145572 = 0;
    generator = -1;
    ParticleInitBreath();
    gBriefings = &chopperPreSwitchBriefings;
    SaveObjectivesNTimersReinit(&D_80145528, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
    }
    if (gObjectives != NULL && (gObjectives->completed & 1)) {
        gBriefings = &D_801454F4;
    }
    DamageSetDestroyedCallback((void (*)(s16, s16))func_80144A00);
    UseSetComputerCallback(func_80144B4C);
    SetUseStringCallback((void (*)())func_80144BC0);
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == CHOPPER_TYPE_SPOTLIGHT) {
            ref = gameObjectsInfo[i].refId;
            if (GOBJ_TYPE(ref) == CHOPPER_TYPE_LIGHT_TARGET && D_80145780 < 4) {
                D_8014560C[D_80145780] = i;
                D_80145780++;
            }
        } else if (type == CHOPPER_TYPE_CHOPPER) {
            generator = i;
        }
    }
    D_80145784 = generator;
    if (generator != -1) {
        func_80141C64();
    }
    gDefaultLevelDmgFX = 3;
}

/* Message handler for the chopper object. */
void CHOPPERObjDispatch(SysMsg *msg) {
    switch (msg->id) {
    case 10: {
        u8 visible;

        func_801421D8(&visible);
        if (D_8014561C->flags23 & 2) {
            break;
        }
        func_80143B88(visible);
        if (D_80145724 != D_80145720) {
            if (D_80145720 == 1 && (D_80145724 == 0xAB8 || D_80145724 == 0x1000)) {
                func_801447EC();
            }
            D_80145724 = D_80145720;
        }
        func_80144958();
        if (D_8014561C->state < 3) {
            NPCRemoveDead();
        }
        if (D_8014578C != -1) {
            SysSetSongVolPan(0, D_8014578C, D_8014561C, 0x7F, -1);
        }
        SysPostMessage(10, 4, D_8014561C->id, D_8014561C->id, 0, 0, 0, 0);
        break;
    }
    case 13: {
        s32 hp;
        u32 level;
        s32 i;
        s32 *t;

        if (D_8014561C->healthInfo->health > 0) {
            level = 0;
            hp = D_8014561C->healthInfo->health;
            for (i = 3, t = &D_80145768[3]; i > 0; i--, t--) {
                if (hp < *t) {
                    level = i;
                    break;
                }
            }
            if (D_8014561C->state < level) {
                Vec4 v;
                Vec4 p;

                D_8014561C->state = level;
                if (level >= 3) {
                    EGSDisplayFlashlightOff(D_80145620);
                }
                ParticleSmoke(-1, &chopperPos, &gExploDir, 0x1000, 1);
                v.vx = EGSMathFIXMul(D_80145680.vx, 0xC0);
                v.vy = EGSMathFIXMul(D_80145680.vy, 0xC0);
                v.vz = EGSMathFIXMul(D_80145680.vz, 0xC0);
                p.vx = chopperPos.vx - v.vx;
                p.vy = chopperPos.vy - v.vy;
                p.vz = chopperPos.vz - v.vz;
                ParticleSmoke(-1, &p, &gExploDir, 0x1000, 1);
            }
        } else if (!(D_8014561C->flags0 & 0x80)) {
            D_8014561C->flags0 |= 0x80;
            D_8014561C->state = 4;
            if (gObjectives != NULL) {
                if (!(gObjectives->unk18 & 2)) {
                    LevelObjectiveAdd(1, -1);
                }
                gObjectives->completed |= 2;
            }
            LevelDoEOL(0);
        }
        break;
    }
    case 0x29: {
        SVec3 sv;

        sv.vx = chopperPos.vx;
        sv.vy = chopperPos.vy;
        D_80145660 = chopperPos;
        sv.vz = chopperPos.vz;
        *(s32 *)msg->data[0] = (u16)sv.vx | ((u16)sv.vy << 16);
        *(s16 *)msg->data[1] = sv.vz;
        break;
    }
    case 0x2A: {
        Vec4 p;
        SVec3 sv;

        sv.vx = ((s16 *)msg->data[0])[0];
        sv.vy = ((s16 *)msg->data[0])[1];
        sv.vz = *(s16 *)msg->data[1];
        chopperPos.vx = sv.vx;
        chopperPos.vy = sv.vy;
        chopperPos.vz = sv.vz;
        D_80145660 = chopperPos;
        D_8014561C->xform->pos = chopperPos;
        D_80145670.vx = PLAYER_XFORM()->x;
        D_80145670.vy = PLAYER_XFORM()->y;
        D_80145670.vz = PLAYER_XFORM()->z;
        D_80145670.vy = -D_80145670.vy;
        p.vx = D_80145670.vx;
        p.vy = chopperPos.vy;
        p.vz = D_80145670.vz;
        func_80141538(&p);
        D_80145790 = 2;
        D_8014556B = 1;
        D_80145798 = 1;
        break;
    }
    case 2:
        D_8014556B = 0;
        D_80145798 = 0;
        func_8014196C(msg->to);
        break;
    }
}
