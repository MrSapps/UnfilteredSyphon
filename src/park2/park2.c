#include "common.h"
#include "level.h"
#include "system.h"

/* Object types specific to this level */
#define PARK2_TYPE_HANS 0x3C   /* Hans (the boss): killing him ends the level */
#define PARK2_TYPE_42 0x42

extern u8 D_80145930;
extern s32 D_8014593C;
extern s32 D_80145940;
extern const char D_801412B0[]; /* "bin/park2.c", defined after iBossLoadHog (used by BOSSObjDispatch too) */

void SysSoundPlayRandXA(void *list);
void LevelDoEOL(s32 arg0);
s32 EGSHogOpen(char *path, s32 arg1, s32 *hog);
void EGSHogGetBlockByName(s32 hog, s32 name, void *dst);
void CASInit(s16 id, void *actions, void *anims, s32 arg3);
void CASReset(s16 id);
void func_80149E88(void *anims, s32 arg1);

/* Hans' runtime object (a GameObjectRuntime with the fields the boss code uses). */
typedef struct BossTarget {
    /* 0x00 */ s16 id;
    /* 0x02 */ u8 unk02[2];
    /* 0x04 */ s32 flags;
    /* 0x08 */ u8 unk08[0x5C];
    /* 0x64 */ s32 aim[3];    /* where Hans aims when the target is not in sight */
    /* 0x70 */ u8 unk70[0x14];
    /* 0x84 */ s32 normal[3]; /* surface normal at the aim point */
    /* 0x90 */ u8 unk90[0x44];
    /* 0xD4 */ s32 unkD4;
} BossTarget;

typedef struct BossNPC {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ u32 flags;
    /* 0x24 */ u8 unk24[0xA];
    /* 0x2E */ s16 unk2E;
    /* 0x30 */ u8 unk30[0x11];
    /* 0x41 */ u8 unk41;
} BossNPC;

typedef struct AnimNode {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s16 *matrix; /* 3x3 rotation */
    /* 0x24 */ u8 unk24[0x10];
    /* 0x34 */ s32 unk34;
} AnimNode;

typedef struct ObjAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ void *unk18;
} ObjAnim;

typedef struct BossRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ s16 linkedId;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ ObjAnim *anim;
    /* 0x0C */ u8 unk0C[8];
    /* 0x14 */ BossTarget *target;
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ BossNPC *npc;
    /* 0x20 */ u8 flags20;
    /* 0x21 */ u8 unk21[2];
    /* 0x23 */ u8 flags23;
} BossRuntime;

typedef struct ObjMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} ObjMatrix;

/*
 * Call fn on the scratchpad stack (0x1F8003E0), with the caller's $sp saved at
 * 0x1F800000. The whole sequence is one asm block, so the argument is held in a
 * callee-saved register across it.
 */
#define SCRATCH_CLOBBERS                                                               \
    "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", \
        "$15", "$24", "$25", "$31", "memory"
#define SCRATCH_ENTER                                                                  \
    ".set noat\n\t"                                                                   \
    "lui $1, 0x1F80\n\tsw $29, 0($1)\n\tlui $29, 0x1F80\n\tori $29, $29, 0x3E0\n\t"
#define SCRATCH_LEAVE "lui $29, 0x1F80\n\tlw $29, 0($29)\n\t.set at"
#define SCRATCH_CALL0(fn) \
    __asm__ volatile(SCRATCH_ENTER "jal " #fn "\n\t" SCRATCH_LEAVE : : : SCRATCH_CLOBBERS)
#define SCRATCH_CALL1(fn, arg)                                                         \
    __asm__ volatile(SCRATCH_ENTER "move $4, %0\n\tjal " #fn "\n\t" SCRATCH_LEAVE \
                     :                                                                 \
                     : "r"(arg)                                                        \
                     : SCRATCH_CLOBBERS)


/* Flamethrower effect state */
typedef struct EGSFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 pos[3];
    /* 0x20 */ void *parent;
} EGSFrame;

typedef struct PointSource {
    /* 0x00 */ s32 on;
    /* 0x04 */ EGSFrame *frame;
    /* 0x08 */ u8 unk08[0x24];
} PointSource; /* size 0x2C */

typedef struct FlameJetPos {
    /* 0x00 */ EGSFrame *frame;
    /* 0x04 */ s16 vel[3];
} FlameJetPos; /* size 0xC */

typedef struct FlamePoly {
    /* 0x00 */ s32 tag;
    /* 0x04 */ u32 otz;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ union {
        s32 word;
        struct {
            u8 r, g, b, code;
        } c;
    } rgbc;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ s32 xy1;
    /* 0x1C */ u8 unk1C[4];
    /* 0x20 */ s32 xy2;
    /* 0x24 */ u8 unk24[4];
    /* 0x28 */ s32 xy3;
    /* 0x2C */ u8 unk2C[4];
} FlamePoly; /* size 0x30 */

typedef struct Vec4 {
    s32 x, y, z, pad;
} Vec4;

typedef struct PlayerMove {
    /* 0x000 */ Vec4 pos;
    /* 0x010 */ u8 unk10[0x11C];
    /* 0x12C */ s32 groundY;
} PlayerMove;

typedef struct PlayerObj {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ PlayerMove *move;
} PlayerObj;

typedef struct SpriteInfo {
    u8 unk00[0x2C];
} SpriteInfo;

typedef struct SpriteBank {
    /* 0x00 */ u8 unk00[0x58];
    /* 0x58 */ SpriteInfo sprites[4];
} SpriteBank;

#define FLAME_JET_COUNT 48

extern SpriteBank *D_80105E28;
extern u8 gPlayerOnFireFlag;

void CameraGetDisplay(void **camera, s32 *display);
void EGSDisplaySetupTransformWorldToScreen(s32 display);
void EGSDisplayTransformWorldToScreen(Vec4 *in, Vec4 *out);
void EGSGeometry_VectorReflectAboutUnitNormal(Vec4 *v, Vec4 *normal, Vec4 *out);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(void *a, Vec4 *b, s32 *dist);
void EGSDisplayPointSourceParams(PointSource *ps, s32 arg1, s32 color);
void EGSDisplayAddPoly(s32 display, void *poly);
void WeaponGetTip(BossRuntime *obj, Vec4 *tip);
void AnimateGobjRequestAim(BossRuntime *obj, Vec4 *at, s32 arg2, s32 arg3);
void GTEMath_VectorNormal(Vec4 *in, Vec4 *out);


/* 12-bit fixed point to integer, rounding toward zero */
#define FIX12(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

void EGSDisplayBuildPoly4FT(void *poly, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, void *sprite, s32 arg7);
void EGSDisplayDeletePoly(s32 arg0, void *poly);
void EGSFrameCreate(EGSFrame **frame);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, s32 *pos);
void EGSDisplayPointSourceInit(PointSource *ps);
void EGSDisplayPointSourceFrame(PointSource *ps, EGSFrame *frame);
void EGSDisplayPointSourceOn();
void ParticleFireFade(s16 handle);
void ParticleFireShow(s16 handle, s32 *pos, s32 arg2, s32 arg3);
extern s32 gPlayerNumConsecutiveHits;

int rand(void);
void NPCDoAction(BossRuntime *rt);
void NPCDoDamage(BossRuntime *rt);
void NPCActivate(u16 id);
void NPCDeactivate(s32 id);
void NPCGobjInit(BossRuntime *rt);
void EGSDisplayHMDCache(void *anim, s32 arg1);
void EGSDisplayPointSourceOff(PointSource *ps);
void LevelGetGameObjectMatrix(s16 id, ObjMatrix *mat);
void func_801463A4(s16 id, void *blocks);

static void func_80141360(s16 id, s16 by);
static s32 func_801418C8(void);
static void func_80141F5C(void);
static void func_801422DC(void);
#ifdef NON_MATCHING
static s32 func_80142BF4(Vec4 *target);
#else
s32 func_80142BF4(Vec4 *target);
#endif
#ifdef NON_MATCHING
static void func_8014210C(s32 seg, s32 *pos, s32 arg2);
#else
void func_8014210C(s32 seg, s32 *pos, s32 arg2);
#endif
static void func_80141398(s16 id, s16 by, s32 arg2, s32 arg3);
static void func_80141528(s32 arg0, s16 weapon, s32 arg2, s32 arg3);
static void func_801416E4(void);

void DamageDeathCAS(), DamageGobjCAS(), MovementGobjMoveCAS();
extern u32 gBoxStand[];

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0};

static char *D_8014334C[2] = {
    "INCOMING FROM BENTON:\n\nLogan, it should be pretty simple now - take out Girdeux.  But check "
    "your fire.  If any explosives go off in that room, that viral bomb will blow and take out half "
    "the eastern seaboard.  Also, Girdeux is likely to be wearing full body armor.  You'll have to "
    "find a weakpoint in his defense.\n",
    "Don't let your history with Girdeux cloud your judgement, Logan.  If you fail, thousands will "
    "die tonight.  I don't want to see a repeat of Costa Rica.",
};
Briefing park2Briefings = {
    {"Washington DC", "Freedom Memorial", "08:24 01:15\n"}, 2, D_8014334C,
};
static char *D_80143368[1] = {"Kill Girdeux"};
static char *D_8014336C[1] = {"Do not use grenades or Girdeux's bomb may be damaged"};
static LevelObjectives D_80143370 = {1, D_80143368, 1, D_8014336C}; /* objectives/timers info */
static const s32 D_80141248 = 2; /* unreferenced */
/* random XA lists */
static s16 D_80143398[34] = {0, 0, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xFD, 0xFF, 0xFA, 0x101, -1};
static s16 D_801433DC[34] = {0, 0, 0xEB, 0xEC, 0xF6, 0xFC, 0xF4, 0xFB, 0xF5, 0xED, -1};
static s16 D_80143420[34] = {0, 0, 0xE8, 0xE9, 0xEA, -1};
static s16 D_80143464[34] = {0, 0, 0xE6, 0xE7, -1};
static s16 D_801434A8[34] = {0, 0, 0xF7, 0xF8, 0xF9, 0xFE, 0x100, -1};
/* 0-terminated list of anim file names */
s32 gHansAnimFiles[8] = {
    (s32)"B1DEATH.HAN", (s32)"B1IDLE.HAN", (s32)"B1IDLE2.HAN", (s32)"B1STHIT.HAN",
    (s32)"B1STHIT2.HAN", (s32)"B1STTAL.HAN", (s32)"WK0.HAN", 0,
};
/* Hans' action sequences and action table (raw words, like playcas.c's) */
u32 seq_hans_stand[12] = {
    3, 0xFFFF0002, 1, 0xFF, 0, 0, 0xFFFF, 0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
u32 seq_hans_walk[12] = {
    3, 0xFFFF0002, 6, 0xFF, 0, 0, 0xFFFF, 0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
static u32 D_8014356C[12] = {
    3, 0xFFFF0000, 0, 0x3106FF, 0, 0, 0xFFFF, 0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
u32 seq_hans_hit[12] = {
    3, 0xFFFF0000, 3, 0xFF, 0, 0, 0xFFFF, 0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
u32 seq_hans_fall[24] = {
    0x10003, 0xFFFF0000, 0x96, 0x230AFF, 0, 0, 0x20003, 0xFFFF0000, 0x96, 0x230AFF, 0, 0, 0x30003,
    0xFFFF0000, 0x64, 0x230AFF, 0, 0, 0xFFFF, 0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
u32 seq_hans_land[30] = {
    0x10023, 0xFFFF0000, 0x97, 0x212B0CFF, 0, 0, 0x20023, 0xFFFF0000, 0x97, 0x212B0CFF, 0, 0,
    0x30023, 0xFFFF0000, 0x65, 0x212B0CFF, 0, 0, 0x2B, 0xFFFF0000, 0xFF, 0x301FF, 0, 0, 0xFFFF,
    0xFFFF0000, 0xFF, 0xFF, 0, 0,
};
u32 mHansActionInfo[1089] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 2, 1, 0, 0, 0x100, 0, 0, 0, 0, (u32)gBoxStand, 0,
    0, 2, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0,
    0x316, 4, 0, 0xC0280, 0, 0, 0, (u32)MovementGobjMoveCAS, 0, 0, 0, 6, 5, 0, 0x83400, 2, 0xC,
    (u32)seq_hans_stand, 0, 0, 0, 0, 6, 6, 0, 0x83400, 4, 0xA, (u32)seq_hans_walk, 0, 0, 0, 0, 6, 7,
    0, 0x1400, 8, 6, (u32)seq_hans_walk, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0,
    0, 9, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0xA, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0,
    0xB, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0xC, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0,
    0xD, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0xE, 0, 0x40000, 0, 0, (u32)seq_hans_hit,
    (u32)DamageGobjCAS, 0xD, 0, 0, 0, 0xF, 0, 0, 0x40000, 0, (u32)D_8014356C, (u32)DamageDeathCAS,
    0x20, 0, 0, 0, 0x10, 0, 0, 0x40000, 0, (u32)D_8014356C, (u32)DamageDeathCAS, 0x20, 0, 0, 0,
    0x11, 0, 0, 0x40000, 0, (u32)D_8014356C, (u32)DamageDeathCAS, 0x20, 0, 0, 0, 0x12, 0, 0,
    0x40000, 0, (u32)D_8014356C, (u32)DamageDeathCAS, 0x20, 0, 0, 0, 0x13, 0, 0, 0, 0, 0, 0, 0,
    (u32)gBoxStand, 0, 0, 0x14, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 6, 0x15, 0, 0x1002400, 0,
    0x80000, 0, 0, 7, 0, 0, 6, 0x16, 0, 0x1002400, 0, 0x80000, 0, 0, 8, 0, 0, 0, 0x17, 0, 0, 0, 0,
    0, 0, 0, (u32)gBoxStand, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x1B, 0, 0, 0,
    0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x1C, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x1D, 0, 0,
    0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x1E, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x1F, 0,
    0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x20, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x21,
    0, 0x40000, 0, 0x18000, (u32)seq_hans_land, 0, 0, 0, 0, 0, 0x22, 0, 0, 0, 0, 0, 0, 0,
    (u32)gBoxStand, 0, 0, 0x23, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x24, 0, 0, 0, 0, 0, 0,
    0, (u32)gBoxStand, 0, 0, 0x25, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x26, 0, 0, 0, 0, 0,
    0, 0, (u32)gBoxStand, 0, 0, 0x27, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x28, 0, 0, 0, 0,
    0, 0, 0, (u32)gBoxStand, 0, 0, 0x29, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2A, 0, 0, 0,
    0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2B, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2C, 0, 0,
    0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2D, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2E, 0,
    0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x2F, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x30,
    0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x31, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0x2012,
    0x33, 0, 0x1008000, 0x8000, 0x800000, (u32)seq_hans_fall, 0, 0, 0, 0, 0, 0x34, 0, 0, 0, 0, 0, 0,
    0, (u32)gBoxStand, 0, 0, 0x35, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x36, 0, 0, 0, 0, 0,
    0, 0, (u32)gBoxStand, 0, 0, 0x37, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x38, 0, 0, 0, 0,
    0, 0, 0, (u32)gBoxStand, 0, 0, 0x39, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x3A, 0, 0, 0,
    0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x3B, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x3C, 0, 0,
    0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x3D, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x3E, 0,
    0x40000, 0, 0x18000, (u32)seq_hans_land, 0, 0, 0, 0, 0, 0x3F, 0, 0, 0, 0, 0, 0, 0,
    (u32)gBoxStand, 0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x41, 0, 0, 0, 0, 0, 0,
    0, (u32)gBoxStand, 0, 0, 0x42, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x43, 0, 0, 0, 0, 0,
    0, 0, (u32)gBoxStand, 0, 0, 0x44, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x45, 0, 0, 0, 0,
    0, 0, 0, (u32)gBoxStand, 0, 0, 0x46, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x47, 0, 0, 0,
    0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x48, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x49, 0, 0,
    0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x4A, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x4B, 0,
    0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x4C, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x4D,
    0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x4E, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0,
    0x4F, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x50, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0,
    0, 0x51, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0x52, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand,
    0, 0, 0x53, 0, 0, 0, 0, 0, 0, 0, (u32)gBoxStand, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static s32 D_801447A8 = 0; /* Hans' weapon attached */
static s32 D_801447AC = 0; /* next jet segment to fire */

/*
 * .bss.  The original has these 4-aligned (an aspsx with 2.56's .lcomm rule);
 * aspsx 2.77 and maspsx would 16-align the light arrays, and as one C struct the
 * code would share its base address.  So it is laid out here (aspsx would put
 * any .lcomm in front of it).
 */
extern s32 D_801447B0;              /* ticks the player has been in Hans' sight */
extern s32 D_801447B4;
extern s32 D_801447B8;              /* last seen gPlayerNumConsecutiveHits */
extern u8 D_801447BC;               /* bomb destroyed */
extern BossRuntime *D_801447C0;     /* Hans */
extern PointSource D_801447C4[6];   /* flame lights (one per 8 jet segments) */
extern PointSource D_801448CC[3];   /* jet lights (one per 16 jet segments) */
extern PointSource D_80144950;      /* Hans' muzzle light */
extern s32 D_8014497C;              /* BOSS.HOG handle */
extern s32 D_80144980[8];           /* blocks loaded from BOSS.HOG */
__asm__(".bss\n"
        "\t.align 2\n"
        "D_801447B0:\n\t.space 4\n"
        "D_801447B4:\n\t.space 4\n"
        "D_801447B8:\n\t.space 4\n"
        "D_801447BC:\n\t.space 4\n"
        "D_801447C0:\n\t.space 4\n"
        "D_801447C4:\n\t.space 0x108\n"
        "D_801448CC:\n\t.space 0x84\n"
        "D_80144950:\n\t.space 0x2C\n"
        "D_8014497C:\n\t.space 4\n"
        "D_80144980:\n\t.space 0x20\n"
        ".text\n");

/* commons: psylink lays them out after the .bss */
EGSFrame *mFlamePools[6];
Vec4 mFlamethrowerEnd[FLAME_JET_COUNT]; /* where each jet segment is aimed */
FlamePoly mFlamethrowerJet[FLAME_JET_COUNT];
s32 mFlameHandle[6];                    /* fire particle handles */
Vec4 mFlamethrowerEndNormal[FLAME_JET_COUNT]; /* surface normal hit by each segment */
u8 mHansAnimInfo[0xE0];
FlameJetPos mFlamethrowerJetPos[FLAME_JET_COUNT];

static void func_80141360(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        ObjectBombDamagedCallback(id, by);
    }
}

static void func_80141398(s16 id, s16 by, s32 arg2, s32 arg3) {
    int type;

    if (id == gPlayerOffset) {
        if (!gMissionFailed) {
            SysPlayXA(0x69, 0, 1);
        }
    } else if (id != GOBJ_ID_666) {
        type = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type;
        if (type == GOBJ_TYPE_BOMB) {
            if (!gMissionFailed) {
                D_801447BC = 1;
                ObjectBombDeath(0, 0x82, 0, arg3);
            }
        } else if (type == PARK2_TYPE_HANS) {
            if (!gMissionFailed && SysSoundIsXAPlaying()) {
                SysStopXA();
            }
            SysSoundPlayRandXA(D_80143464);
            if (!gMissionFailed) {
                if (gObjectives != NULL) {
                    if (!(gObjectives->unk18 & 1)) {
                        LevelObjectiveAdd(0, -1);
                    }
                    gObjectives->completed |= 1;
                }
                LevelDoEOL(0);
            }
        }
    }
}

static void func_80141528(s32 arg0, s16 weapon, s32 arg2, s32 arg3) {
    if (!gMissionFailed && (weapon == 0x13 || weapon == 0x10)) {
        ObjectBombDeath(0, 0x82, 0, arg3);
    }
}

void iPark2Init(void) {
    LevelObjectives *obj;
    GameObjectInfo *info;
    GameObjectModel *model;
    s32 count;
    s32 i;
    s32 j;

    D_801447BC = 0;
    SaveObjectivesNTimersReinit(&D_80143370, 0);
    LevelObjectiveAdd(0, -1);
    if (gObjectives != NULL) {
        count = gObjectives->count;
        for (i = 0; i < count; i++) {
            if (gObjectives != NULL) {
                gObjectives->unk24 |= 1 << i;
            }
        }
    }
    DamageSetDamagedCallback(func_80141360);
    DamageSetDestroyedCallback((void (*)(s16, s16))func_80141398);
    WeaponSetWeaponFiredCallback((void (*)())func_80141528);
    for (j = 0; j < gLevelGameObjCount; j++) {
        if (GOBJ_TYPE(j) == PARK2_TYPE_42) {
            model = gameObjectsInfo[j].runtime->anim->model;
            model->flags |= 0x10000;
            break;
        }
    }
    gDefaultLevelDmgFX = 11;
}

static void func_801416E4(void) {
    func_80149E88(mHansAnimInfo, 7);
    D_80145930 = 1;
    D_8014593C = 2;
    D_80145940 = 2;
}

typedef struct BossCAS {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 unk09;
} BossCAS;

typedef struct BossObj {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 id;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ BossCAS *cas;
} BossObj;

void BossCASInit(BossObj *obj, s32 mode, s32 arg2) {
    func_801416E4();
    if (obj->cas == NULL) {
        CASInit(obj->id, mHansActionInfo, mHansAnimInfo, 5);
    } else {
        CASReset(obj->id);
    }
    obj->cas->unk08 = 1;
    if (mode == 2) {
        obj->cas->unk09 = 3;
    } else if (mode == 1) {
        obj->cas->unk09 = 2;
    } else if (mode == 0) {
        obj->cas->unk09 = 1;
    } else if (mode == 4) {
        obj->cas->unk09 = 4;
    }
}

void iBossLoadHog(void) {
    char path[0x20];
    s32 i;

    sprintf(path, "\\%s\\BOSS.HOG;1", gLevelNames[gSysInfo.level]);
    if (EGSHogOpen(path, 0, &D_8014497C) != 0) {
        EGSDebugAssert(1, 0, D_801412B0, 539);
    }
    for (i = 0; gHansAnimFiles[i] != 0; i++) {
        EGSHogGetBlockByName(D_8014497C, gHansAnimFiles[i], &D_80144980[i]);
    }
}


const char D_801412B0[] = "bin/park2.c";


static s32 func_801418C8(void) {
    BossNPC *npc;

    npc = D_801447C0->npc;
    if (((GameObjectRuntime *)gObjPlayer)->healthInfo->health > 0 && (npc->flags & 0x100)) {
        if (npc->unk41 != 0) {
            return npc->unk41 != 0;
        }
        if (D_801447C0->target->unkD4 > 0x4CC) {
            npc->unk41 = rand() & 0x1F;
            SysPlaySong(0, 0x12, 0, 0);
        }
    } else if (npc->unk41 != 0) {
        npc->unk41--;
    }
    return npc->unk41 != 0;
}

void BOSSObjDispatch(SysMsg *msg) {
    ObjMatrix mat;
    BossRuntime *boss;
    BossRuntime *rt;
    BossNPC *npc;
    BossTarget *target;
    PointSource *light;
    s32 id;
    s32 i;

    switch (msg->id) {
    case 10:
        boss = D_801447C0;
        if (!(boss->flags23 & 2)) {
            SCRATCH_CALL1(NPCDoAction, boss);
        }
        boss->npc->flags &= ~0x200000;
        SCRATCH_CALL0(func_801422DC);
        if (func_801418C8() & 0xFF) {
            SCRATCH_CALL1(func_80142BF4, D_801447C0->npc->unk10);
            break;
        }
        target = D_801447C0->target;
        if (target->id == gPlayerOffset && ((GameObjectRuntime *)gObjPlayer)->healthInfo->health > 0) {
            if (!(target->flags & 2)) {
                if (++D_801447B0 > 100) {
                    D_801447B0 = 0;
                    SysSoundPlayRandXA(D_80143398);
                }
            } else {
                if (!(rand() & 0xFF)) {
                    SysSoundPlayRandXA(D_801434A8);
                }
                D_801447B0 = 0;
            }
        }
        light = &D_80144950;
        if (light->on != 0) {
            EGSDisplayPointSourceOff(light);
            SysStopSong(0, 0x12);
            SysPlaySong(0, 0x13, 0, 0);
        }
        break;
    case 2:
        iBossLoadHog();
        func_801463A4(msg->to, D_80144980);
        D_801447C0 = (BossRuntime *)gameObjectsInfo[msg->to].runtime;
        EGSDisplayHMDCache(D_801447C0->anim, 1);
        if ((D_801447C0->npc = EGSMemMalloc(0x54)) == NULL) {
            EGSDebugAssert(1, 0, D_801412B0, 668);
        }
        NPCGobjInit(D_801447C0);
        BossCASInit((BossObj *)D_801447C0, 2, 0);
        LevelGetGameObjectMatrix(D_801447C0->linkedId, &mat);
        npc = D_801447C0->npc;
        npc->unk2E = mat.t[1];
        D_801447C0->flags20 |= 0x40;
        D_801447B0 = 0;
        D_801447B4 = 180;
        D_801447B8 = 0;
        func_80141F5C();
        D_801447C0->healthInfo->health = 0x7FFF;
        D_801447A8 = 0;
        CASRequestAction(D_801447C0->linkedId, 5);
        break;
    case 13:
    case 32:
        SCRATCH_CALL1(NPCDoDamage, D_801447C0);
        if (msg->id != 32) {
            if (!gMissionFailed) {
                SysStopXA();
            }
            SysSoundPlayRandXA(D_80143420);
        }
        break;
    case 6:
        SCRATCH_CALL1(NPCActivate, (u16)D_801447C0->linkedId);
        if (D_801447A8 == 0) {
            id = msg->to;
            for (i = id - 1; i >= 0; i--) {
                rt = (BossRuntime *)gameObjectsInfo[i].runtime;
                if (gameObjectsInfo[rt->linkedId].refId == id) {
                    rt->anim->unk0C = ((AnimNode *)D_801447C0->anim->unk18)->unk34;
                    rt->anim->unk18 = NULL;
                    rt->anim->unk09 = 0x11;
                    rt->anim->unk18 = D_801447C0->anim;
                    break;
                }
            }
            D_801447A8 = 1;
        }
        break;
    case 5:
        SCRATCH_CALL1(NPCDeactivate, (u16)D_801447C0->linkedId);
        break;
    case 41:
    case 42:
        return;
    }
    if (gPlayerNumConsecutiveHits != D_801447B8) {
        D_801447B8 = gPlayerNumConsecutiveHits;
        if (gPlayerNumConsecutiveHits % 10 == 0) {
            SysSoundPlayRandXA(D_801433DC);
        }
    }
}

static void func_80141F5C(void) {
    FlamePoly *poly;
    s32 i;

    poly = mFlamethrowerJet;
    for (i = 0; i < FLAME_JET_COUNT; i++, poly++) {
        EGSDisplayBuildPoly4FT(poly, 0, 0, 0, 0, 0, &D_80105E28->sprites[i & 3], 1);
        poly->tag = 0;
    }
    for (i = 0; i < FLAME_JET_COUNT; i++) {
        EGSFrameCreate(&mFlamethrowerJetPos[i].frame);
        if ((i & 0xF) == 3) {
            mFlameHandle[i >> 4] = 0;
            EGSDisplayPointSourceInit(&D_801448CC[i >> 4]);
            EGSDisplayPointSourceFrame(&D_801448CC[i >> 4], mFlamethrowerJetPos[i].frame);
        }
    }
    for (i = 0; i < 6; i++) {
        EGSFrameCreate(&mFlamePools[i]);
        D_801447C4[i].frame = mFlamePools[i];
        EGSDisplayPointSourceInit(&D_801447C4[i]);
    }
    EGSDisplayPointSourceInit(&D_80144950);
    D_80144950.frame = (EGSFrame *)D_801447C0->anim->unk0C;
}

static void func_8014210C(s32 seg, s32 *pos, s32 arg2) {
    PointSource *ps;
    s32 n;
    s32 handle;

    if (!(seg & 7)) {
        n = seg >> 3;
        if (mFlameHandle[n] != 0) {
            ParticleFireFade(mFlameHandle[n]);
        } else {
            PointSource *lights = D_801447C4;

            ps = lights + n;
            if (ps->on == 0) {
                EGSDisplayPointSourceOn(ps);
            }
        }
        handle = *(u16 *)&gSystemTick | 0x7FFF0000;
        mFlameHandle[n] = handle;
        ParticleFireShow(handle, pos, 0x800, 0);
        if (D_801447C4[n].frame->parent == NULL) {
            D_801447C4[n].frame->pos[0] = pos[0];
            D_801447C4[n].frame->pos[1] = -pos[1];
            D_801447C4[n].frame->pos[2] = pos[2];
        } else {
            EGSFrameSetPosition(D_801447C4[n].frame, 0, pos);
        }
    }
    n = seg >> 4;
    if ((seg & 0xF) == 3) {
        PointSource *lights = D_801448CC;

        ps = lights + n;
        if (ps->on != 0) {
            EGSDisplayPointSourceOff(ps);
        }
    }
    EGSDisplayDeletePoly(arg2, &mFlamethrowerJet[seg]);
}

/* |a - b| is within half of the per-frame step tol */
#define WITHIN_HALF(a, b, tol) (ABS((tol) / 2) > ABS((a) - (b)))

/* Move the flamethrower jet segments and rebuild their polygons. */
static void func_801422DC(void) {
    Vec4 playerPos;
    s32 display[2];
    Vec4 cur;
    Vec4 nxt;
    s32 dist;
    FlamePoly *poly;
    FlamePoly *nextPoly;
    FlameJetPos *jet;
    FlameJetPos *next;
    u8 onFire;
    s32 i;
    s32 dx;
    s32 dy;
    s32 dz;
    s16 w;
    s16 h;
    s32 z;
    u32 r;

    onFire = gPlayerOnFireFlag;
    CameraGetDisplay(gCamera, display);
    EGSDisplaySetupTransformWorldToScreen(display[0]);
    playerPos = ((PlayerObj *)gObjPlayer)->move->pos;
    for (i = 0; i < FLAME_JET_COUNT; i++) {
        poly = &mFlamethrowerJet[i];
        if (i != FLAME_JET_COUNT - 1) {
            nextPoly = &mFlamethrowerJet[i + 1];
        } else {
            nextPoly = mFlamethrowerJet;
        }
        if (poly->tag == 0) {
            continue;
        }
        jet = &mFlamethrowerJetPos[i];
        jet->vel[1] -= 2;
        cur.x = jet->frame->pos[0];
        cur.y = jet->frame->pos[1];
        cur.z = jet->frame->pos[2];
        cur.y = -cur.y;
        if (WITHIN_HALF(cur.y, mFlamethrowerEnd[i].y, jet->vel[1])) {
            func_8014210C(i, &cur.x, display[0]);
            continue;
        }
        if (WITHIN_HALF(jet->frame->pos[0], mFlamethrowerEnd[i].x, jet->vel[0]) ||
            WITHIN_HALF(jet->frame->pos[2], mFlamethrowerEnd[i].z, jet->vel[2])) {
            cur.x = jet->vel[0];
            cur.y = jet->vel[1];
            cur.z = jet->vel[2];
            EGSGeometry_VectorReflectAboutUnitNormal(&cur, &mFlamethrowerEndNormal[i], &cur);
            mFlamethrowerEndNormal[i].x = 0;
            mFlamethrowerEndNormal[i].y = 0;
            mFlamethrowerEndNormal[i].z = 0;
            jet->vel[0] = cur.x >> 2;
            jet->vel[2] = cur.z >> 2;
        }
        cur.x = jet->frame->pos[0];
        cur.y = jet->frame->pos[1];
        cur.z = jet->frame->pos[2];
        cur.y = -cur.y;
        if (nextPoly->tag != 0) {
            if (i == FLAME_JET_COUNT - 1) {
                next = mFlamethrowerJetPos;
                nxt.x = next->frame->pos[0];
                nxt.y = next->frame->pos[1];
                nxt.z = next->frame->pos[2];
                nxt.y = -nxt.y;
                nxt.x += next->vel[0];
                nxt.y += next->vel[1] - 2;
                nxt.z += next->vel[2];
            } else {
                next = &mFlamethrowerJetPos[i + 1];
                nxt.x = next->frame->pos[0];
                nxt.y = next->frame->pos[1];
                nxt.z = next->frame->pos[2];
                nxt.y = -nxt.y;
            }
            dx = ABS(nxt.x - cur.x) >> 3;
            dy = ABS(nxt.y - cur.y) >> 3;
            dz = ABS(nxt.z - cur.z) >> 3;
            if (ABS(jet->vel[0]) < dx || ABS(jet->vel[1]) < dy || ABS(jet->vel[2]) < dz) {
                nxt.x -= cur.x;
                nxt.y -= cur.y;
                nxt.z -= cur.z;
                nxt.x /= 4;
                nxt.y /= 4;
                nxt.z /= 4;
                nxt.x = cur.x + nxt.x;
                nxt.y = cur.y + nxt.y;
                nxt.z = cur.z + nxt.z;
            }
        } else {
            nxt.x = cur.x;
            nxt.y = cur.y;
            nxt.z = cur.z;
        }
        EGSDisplayTransformWorldToScreen(&nxt, &nxt);
        cur.x += jet->vel[0];
        cur.y += jet->vel[1];
        cur.z += jet->vel[2];
        if (jet->frame->parent == NULL) {
            jet->frame->pos[0] = cur.x;
            jet->frame->pos[1] = -cur.y;
            jet->frame->pos[2] = cur.z;
        } else {
            EGSFrameSetPosition(jet->frame, 0, &cur.x);
        }
        if (nxt.z <= 0) {
            poly->xy3 = 0x04000400;
            poly->xy2 = 0x04000400;
            poly->xy1 = 0x04000400;
            poly->xy0 = 0x04000400;
            continue;
        }
        cur.x += jet->vel[0] >> 1;
        cur.y += jet->vel[1] >> 1;
        cur.z += jet->vel[2] >> 1;
        if (!onFire) {
            EGSGeometry_GetFuzzyXZDistancePoint2Point(&cur, &playerPos, &dist);
            if (dist < 0x30) {
                SysPostMessage(0xD, 5, 0x29A, gPlayerOffset, 0, 0, 0, 0);
                onFire = 1;
            }
        }
        EGSDisplayTransformWorldToScreen(&cur, &cur);
        h = cur.x - nxt.x;
        w = cur.y - nxt.y;
        if (mFlamethrowerEndNormal[i].x == 0 && mFlamethrowerEndNormal[i].y == 0 && mFlamethrowerEndNormal[i].z == 0) {
            w >>= 1;
            h >>= 1;
        } else {
            w >>= 2;
            h >>= 2;
        }
        z = nxt.z;
        if (cur.z < nxt.z) {
            z = cur.z;
        }
        z >>= 2;
        if (z < 0) {
            z = 0;
        }
        poly->otz = (u32)(z * 3) >> 2;
        poly->xy0 = ((nxt.x - w) & 0xFFFF) | ((-nxt.y - h) << 16);
        poly->xy1 = ((nxt.x + w) & 0xFFFF) | ((h - nxt.y) << 16);
        poly->xy2 = ((cur.x - w) & 0xFFFF) | ((-cur.y - h) << 16);
        poly->xy3 = ((cur.x + w) & 0xFFFF) | ((h - cur.y) << 16);
    }
    for (i = 0; i < 6; i++) {
        if (mFlameHandle[i] != 0) {
            r = (rand() & 0x1F) + 0x80;
            EGSDisplayPointSourceParams(&D_801447C4[i], 100, (((r >> 1) + (rand() & 0x1F)) << 8) + r);
        }
    }
    if (!onFire) {
        for (i = 0; i < 6; i++) {
            if (mFlameHandle[i] != 0) {
                EGSGeometry_GetFuzzyXZDistancePoint2Point(&mFlamePools[i]->pos, &playerPos, &dist);
                if (dist < 0x30) {
                    SysPostMessage(0xD, 5, 0x29A, gPlayerOffset, 0, 0, 0, 0);
                    return;
                }
            }
        }
    }
}

#ifdef NON_MATCHING
/*
 * Same length; only the register choice for D_801447C0->target vs. the
 * mFlamethrowerEnd base in the two branches below is swapped.
 */
/* Fire the next flamethrower jet segment from Hans' weapon towards target. */
static s32 func_80142BF4(Vec4 *target) {
    Vec4 tip;
    Vec4 dir;
    s32 display;
    s32 dist;
    FlamePoly *poly;
    FlameJetPos *jet;
    Vec4 *nrm;
    PointSource *ps;
    s32 steps;
    s32 vy;
    s32 r;
    s32 g;
    s32 n;
    s32 rnd;
    s32 c0;
    s32 c1;
    s32 c2;
    s32 rnd2;
    s32 b;
    s32 gr;
    s32 rd;

    if (target != NULL) {
        jet = &mFlamethrowerJetPos[D_801447AC];
        poly = &mFlamethrowerJet[D_801447AC];
        if (poly->tag == 0) {
            CameraGetDisplay(gCamera, &display);
            EGSDisplayAddPoly(display, poly);
        }
        WeaponGetTip(D_801447C0, &tip);
        dir.x = target->x - tip.x;
        dir.y = target->y - tip.y;
        dir.z = target->z - tip.z;
        EGSGeometry_GetFuzzyXZDistancePoint2Point(target, &tip, &dist);
        if (D_801447C0->target->flags & 2) {
            Vec4 *end = &mFlamethrowerEnd[D_801447AC];

            end->x = tip.x;
            end->y = tip.y;
            end->z = tip.z;
            end->x -= dir.x;
            end->y -= dir.y;
            end->z -= dir.z;
        } else {
            Vec4 *end = &mFlamethrowerEnd[D_801447AC];

            end->x = D_801447C0->target->aim[0];
            end->y = D_801447C0->target->aim[1];
            end->z = D_801447C0->target->aim[2];
        }
        mFlamethrowerEnd[D_801447AC].y = ((PlayerObj *)gObjPlayer)->move->groundY;
        nrm = &mFlamethrowerEndNormal[D_801447AC];
        nrm->x = D_801447C0->target->normal[0];
        nrm->y = D_801447C0->target->normal[1];
        nrm->z = D_801447C0->target->normal[2];
        steps = dist / 48;
        vy = dir.y / (steps + 1) + steps;
        {
            Vec4 aim;
            ObjMatrix mat;

            aim.x = tip.x + dir.x;
            aim.y = tip.y + vy * steps;
            aim.z = tip.z + dir.z;
            AnimateGobjRequestAim(D_801447C0, &aim, 1, 0);
            mat.m[0][0] = ((AnimNode *)D_801447C0->anim->unk18)->matrix[0];
            mat.m[0][1] = -((AnimNode *)D_801447C0->anim->unk18)->matrix[1];
            mat.m[0][2] = ((AnimNode *)D_801447C0->anim->unk18)->matrix[2];
            mat.m[1][0] = -((AnimNode *)D_801447C0->anim->unk18)->matrix[3];
            mat.m[1][1] = ((AnimNode *)D_801447C0->anim->unk18)->matrix[4];
            mat.m[1][2] = -((AnimNode *)D_801447C0->anim->unk18)->matrix[5];
            mat.m[2][0] = ((AnimNode *)D_801447C0->anim->unk18)->matrix[6];
            mat.m[2][1] = -((AnimNode *)D_801447C0->anim->unk18)->matrix[7];
            mat.m[2][2] = ((AnimNode *)D_801447C0->anim->unk18)->matrix[8];
            dir.x = mat.m[0][1];
            dir.y = mat.m[1][1];
            dir.z = mat.m[2][1];
        }
        dir.x = -dir.x;
        dir.y = 0;
        dir.z = -dir.z;
        GTEMath_VectorNormal(&dir, &dir);
        dir.y = vy;
        dir.x /= 85;
        dir.z /= 85;
        jet->vel[0] = dir.x;
        jet->vel[1] = dir.y;
        jet->vel[2] = dir.z;
        if (jet->frame->parent == NULL) {
            jet->frame->pos[0] = tip.x;
            jet->frame->pos[1] = -tip.y;
            jet->frame->pos[2] = tip.z;
        } else {
            EGSFrameSetPosition(jet->frame, 0, &tip.x);
        }
        r = rand() & 0x7F;
        g = rand() & 0xCF;
        poly->rgbc.word = ((rand() & 0xFF) << 16) + ((g + 0x30) << 8) + (r + 0x80) + 0x2C000000;
        poly->xy3 = 0x04000400;
        poly->xy2 = 0x04000400;
        poly->xy1 = 0x04000400;
        poly->xy0 = 0x04000400;
        poly->rgbc.c.code |= 2;
        if ((D_801447AC & 0xF) == 3) {
            n = D_801447AC >> 4;
            rnd = rand() & 0xFFF;
            c0 = FIX12(rnd * 220);
            c1 = FIX12(rnd * 200);
            c2 = FIX12(rnd * 160);
            ps = &D_801448CC[n];
            EGSDisplayPointSourceOn(ps);
            EGSDisplayPointSourceParams(ps, (rand() & 0xFF) | 0x100, (c2 << 16) + (c1 << 8) + c0);
        }
        if (D_80144950.on == 0) {
            EGSDisplayPointSourceOn(&D_80144950);
        } else {
            rnd2 = rand() & 0xFFF;
            b = FIX12(rnd2 * 180);
            gr = FIX12(rnd2 * 160);
            rd = FIX12(rnd2 * 120);
            EGSDisplayPointSourceParams(&D_80144950, 0x80, (rd << 16) + (gr << 8) + b);
        }
        if (++D_801447AC >= FLAME_JET_COUNT) {
            D_801447AC = 0;
        }
        return steps;
    }
    return -1;
}

#else
INCLUDE_ASM("asm/park2/nonmatchings/park2", func_80142BF4);
#endif
