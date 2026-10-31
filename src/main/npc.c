#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"

/* damage.c's packet list bytes, still in the damage blob (docs/data_migration.md) */
SDATA_LOCAL(D_80110180)
SDATA_LOCAL(D_80110184)

/* Path point (LevelGetPathInfo), 0xC bytes. */
typedef struct PathPoint {
    /* 0x0 */ s16 x, y, z;
    /* 0x6 */ u16 flags; /* bits 8-11: point action kind */
    /* 0x8 */ s8 links[3]; /* neighbouring points, -1 for none */
    /* 0xB */ u8 pad;
} PathPoint;

/* NPC state hanging off GameObjectRuntime.npc (level.h's NPCInfo, filled in). */
typedef struct NPC {
    /* 0x00 */ VECTOR32 dir; /* direction of travel along the path */
    /* 0x10 */ VECTOR32 lastSeenPos; /* where the player was last seen */
    /* 0x20 */ s32 flags;
    /* 0x24 */ struct NPCScriptStep *script; /* point action script */
    /* 0x28 */ s16 unk28; /* sound radius threshold */
    /* 0x2A */ s16 unk2A; /* player distance from the current path point */
    /* 0x2C */ s16 unk2C;
    /* 0x2E */ s16 unk2E;
    /* 0x30 */ s16 unk30; /* tick of the last restore */
    /* 0x32 */ s16 unk32;
    /* 0x34 */ s16 unk34; /* sound radius */
    /* 0x36 */ s16 unk36;
    /* 0x38 */ s16 unk38; /* ticks between shots */
    /* 0x3A */ u8 unk3A[2];
    /* 0x3C */ s32 casAction; /* CAS action in progress, 0 when done */
    /* 0x40 */ u8 unk40; /* sound kind */
    /* 0x41 */ u8 unk41;
    /* 0x42 */ u8 unk42;
    /* 0x43 */ u8 curPoint;
    /* 0x44 */ u8 nextPoint;
    /* 0x45 */ u8 unk45;
    /* 0x46 */ u8 unk46;
    /* 0x47 */ u8 nameMsgId;
    /* 0x48 */ u8 awareness;
    /* 0x49 */ u8 unk49;
    /* 0x4A */ u8 unk4A;
    /* 0x4B */ u8 unk4B;
    /* 0x4C */ s8 slot; /* index into npcList and the per-slot pools */
    /* 0x4D */ u8 unk4D;
    /* 0x4E */ u8 unk4E;
    /* 0x4F */ u8 xaTimer; /* ticks until the next random XA line */
    /* 0x50 */ u8 unk50;
    /* 0x51 */ u8 voice; /* row of D_801067C0 */
    /* 0x52 */ u8 unk52[2];
} NPC; /* size 0x54 */

#define NPC_OF(obj) ((NPC *)(obj)->npc)

/* TargetInfo+0x5C (an s16 object id; gameobj.h has u8 unk5C[2]) */
#define TARGET_VICTIM(t) (*(s16 *)(t)->unk5C)

/* GameObjectHealth with the fields gameobj.h doesn't have yet */
typedef struct NPCHealth {
    /* 0x00 */ u8 unk00[6];
    /* 0x06 */ s16 armor;
    /* 0x08 */ s16 health;
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 unkC; /* damage taken */
    /* 0x0E */ s16 unkE; /* damage type */
} NPCHealth;

/* GameObjectInfo+0x24 read as a word (gameobj.h only has a u16 there) */
#define GOBJ_INFO_FLAGS(info) (*(s32 *)&(info)->flags24)

/* Random XA lines, D_801067C0[voice][kind] */
typedef struct NPCXALines {
    /* 0x0 */ s16 *tracks;
    /* 0x4 */ u8 count;
    /* 0x5 */ u8 pad[3];
} NPCXALines;

/* MoveInfo fields not in gobj.h yet */
typedef struct NPCMove {
    /* 0x00 */ u8 unk00[0x40];
    /* 0x40 */ VECTOR32 unk40; /* position */
    /* 0x50 */ VECTOR32 unk50; /* 20.12 fixed point offset */
    /* 0x60 */ u8 unk60[0x60];
    /* 0xC0 */ VECTOR32 rot; /* facing angles (vy is the yaw) */
    /* 0xD0 */ u8 unkD0[0xD8];
    /* 0x1A8 */ struct NPCPathCtrl *pathCtrl; /* D_8011E1A0[slot] */
} NPCMove;

/* Scripted path movement (D_8011E1A0[], 0x128 bytes each) */
typedef struct NPCPathCtrl {
    /* 0x00 */ u8 unk00[0x44];
    /* 0x44 */ u8 active;
    /* 0x48 */ VECTOR32 from;
    /* 0x58 */ VECTOR32 to;
    /* 0x68 */ u8 unk68[0xC0];
} NPCPathCtrl; /* size 0x128 */

#define NPC_MOVE(obj) ((NPCMove *)(obj)->move)
#define MOVE_ROT(m) (((NPCMove *)(m))->rot)
#define FIX_TO_INT(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

/* Path point actions (D_80106474[], indexed by the point's action kind). */
typedef struct PointAction {
    /* 0x0 */ s16 casAction; /* CAS action to request, 0 for none */
    /* 0x2 */ s16 pad;
    /* 0x4 */ void (*func)(Gobj *obj);
    /* 0x8 */ s32 unk8;
} PointAction;

/* Line of sight hit, see gtegeo.c */
typedef struct LOSHitOwner {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
} LOSHitOwner;

typedef struct LOSHit {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 type; /* 1: game object, 2: window */
    /* 0x0C */ u8 unk0C[0x3C];
    /* 0x48 */ LOSHitOwner *owner;
} LOSHit;

/* Generator table entry (generatorInfo[], 0x10 bytes). */
typedef struct NPCGenerator {
    /* 0x0 */ u16 flags; /* 0x7FF: count, 0x3800: ?, 0x4000: enabled */
    /* 0x2 */ u16 unk02; /* GameObjectInfo flags for the NPCs, low 5 bits: kind */
    /* 0x4 */ u8 numPaths;
    /* 0x5 */ u8 unk05; /* number of variants */
    /* 0x6 */ u8 unk06[2];
    /* 0x8 */ s32 *paths;
    /* 0xC */ u8 *models; /* model index per variant */
} NPCGenerator;

/* Damage packets (pool of 16, linked through next). */
typedef struct DamagePacket {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ u8 inUse;
    /* 0x1D */ u8 next; /* 16 terminates the list */
    /* 0x1E */ u8 unk1E[2];
} DamagePacket;

#define NPC_SLOTS 6
#define DAMAGE_PACKETS 16

/*
 * npc.c's data. .sdata, .sbss and .bss each in address order: cc1 emits the
 * uninitialised statics in first-declaration order, so this block comes before any
 * other declaration of them.
 */
static void (*D_8010F94C)() = NULL;       /* combat mode callback */
static void (*D_8010F950)(s16 id) = NULL; /* cop deactivate callback */
s32 gNpcAllOnRadarFlag = 0;
static s32 D_8010F958 = 1;                /* NPCs may shoot */
s32 npcPCInvisibleFlag = 0;
static s32 D_8010F960 = 0;                /* NPCGetMoveMode: 1 kneel, -1 stand */
static char D_8010F964[8] = "s/npc.c";    /* __FILE__ (NPCObjDispatch, still asm, uses it too) */
static s32 D_8010F96C = 0;                /* last generator NPC handed out */
static s32 D_8010F970 = 0;                /* last NPC slot given a shot */
static s32 D_8010F974 = 0;                /* NPCLosCheck: npcList resume index */
static s32 D_8010F978 = -1;               /* NPCLosCheck: phase (-1 camera, 0 target, 1 dead) */
static s32 D_8010F97C = 0;                /* unreferenced */

/*
 * .sbss: the original assembler gave every .lcomm 4 bytes (see particle.c); the modern
 * build packs by size, so it gets explicit padding statics.
 */
#ifdef _PSYQ
#define SBSS_PAD2(n)
#define SBSS_PAD3(n)
#else
#define SBSS_PAD2(n) static s16 n;
#define SBSS_PAD3(n) static u8 n; static s16 n##_2;
#endif
static u8 D_80110118;
static void (*D_8011011C)();      /* interact entered range callback */
static void (*D_80110120)();      /* interact exited range callback */
static void (*D_80110124)();      /* interact other callback */
#ifdef _PSYQ
static u8 D_80110128[NPC_SLOTS];
#else
/* maspsx puts a stray nop before an indexed access to a small static (real aspsx does
 * not), so the modern build names the storage differently and aliases it below */
static u8 D_80110128_[NPC_SLOTS];
extern u8 D_80110128[];
#endif
static s32 D_80110130;
static PathPoint *D_80110134;     /* current NPC's path */
static s16 D_80110138; SBSS_PAD2(D_80110138_pad)
static s16 D_8011013C; SBSS_PAD2(D_8011013C_pad)
static s16 D_80110140; SBSS_PAD2(D_80110140_pad)
static s16 D_80110144; SBSS_PAD2(D_80110144_pad)
static s16 D_80110148; SBSS_PAD2(D_80110148_pad) /* first generator NPC object id */
static s16 D_8011014C; SBSS_PAD2(D_8011014C_pad)
static s16 D_80110150; SBSS_PAD2(D_80110150_pad)
static s16 D_80110154; SBSS_PAD2(D_80110154_pad)
static u8 D_80110158; SBSS_PAD3(D_80110158_pad)
static u8 D_8011015C; SBSS_PAD3(D_8011015C_pad)
static u8 D_80110160; SBSS_PAD3(D_80110160_pad) /* cop damaged XA played */
static u8 D_80110164; SBSS_PAD3(D_80110164_pad)
static s32 D_80110168;            /* enabled generator count */
#ifndef _PSYQ
/* psylink starts damage's .sbss 8-aligned (docs/psyq_build.md); GNU ld needs the pad */
static s32 D_8011016C_pad;
#endif

/* .bss */
static VECTOR32 D_8011D1D0;       /* NPCGetMoveMode retreat direction */
#define D_8011D1D4 D_8011D1D0.vy
#define D_8011D1D8 D_8011D1D0.vz
static VECTOR32 D_8011D1E0;       /* NPCPointIQ goal midpoint */
static VECTOR32 D_8011D1F0;       /* getPointActionVector result */
static VECTOR32 D_8011D200;       /* NPCFaceDirection turn step */
static VECTOR32 D_8011D210;       /* NPCIQEveryFrame movement direction */
static VECTOR32 D_8011D220;       /* generator spawn point */
static VECTOR32 D_8011D230;       /* ... on screen */
static TargetInfo D_8011D240[NPC_SLOTS];
static MoveInfo D_8011D750[NPC_SLOTS];
static NPCPathCtrl D_8011E1A0[NPC_SLOTS];
#ifdef _PSYQ
static s32 D_8011E890[NPC_SLOTS];
static VECTOR32 D_8011E8A8;       /* player position as seen by NPCs */
static VECTOR32 D_8011E8B8;       /* current NPC's position (y up) */
static VECTOR32 D_8011E8C8;       /* LOS override position (pad is the active flag) */
#else
/* The vectors are only 8-aligned; maspsx's 2.7x layout would 16-align them, so the
 * modern build reserves them with D_8011E890 and names them with .set below. */
static s32 D_8011E890[NPC_SLOTS + 12];
extern VECTOR32 D_8011E8A8;
extern VECTOR32 D_8011E8B8;
extern VECTOR32 D_8011E8C8;
#endif

/* commons this TU reaches through $gp */
u8 NPCEndActivationFlag;
u8 NPCNumActive;
s32 NPCNumActiveTick;
u8 NPCNumEnemyAlive;
s16 NPCPopRemoveID;
u8 copKilledByPCFlag;
u8 npcAlertByScream;
u8 npcGrenadeLandedCheck;
s32 npcVisibilityDistance;
s16 pointActionMoveMode;
s16 getNPCIndex;                  /* NPCGetNextID position in npcList */
s16 npcGrenadeThrowerID;
u16 grenadeWeaponRestore;
u16 npcNumGenerators;
extern u8 D_80110180;             /* damage packet list head */
extern u8 D_80110184;             /* next damage packet to try */

/* Level data header, see level.c */
typedef struct NPCLevelData {
    /* 0x00 */ u8 unk00[0x52];
    /* 0x52 */ s16 maxActiveNPCs;
    /* 0x54 */ u8 unk54[0x2C];
    /* 0x80 */ u8 sectorAdj[1][15]; /* neighbouring sectors per sector */
} NPCLevelData;

/* Generators to try per terrain sector (generatorActivationList[]) */
typedef struct NPCGenActivation {
    /* 0x0 */ s32 count;
    /* 0x4 */ u8 *gens;
} NPCGenActivation;

/* NPCGenerator.flags as bitfields */
typedef struct NPCGenBits {
    u16 count : 11;
    u16 unk11 : 3;
    u16 enabled : 1;
    u16 perm : 1;
} NPCGenBits;

extern void *gTerrSector;         /* 0x3C bytes per sector */
extern s32 gTerrSectorCount;
extern s16 gGabeSector;
extern NPCGenActivation *generatorActivationList;

static inline void *TerrGetSector(s32 id) {
    return (id < gTerrSectorCount && id >= 0) ? (u8 *)gTerrSector + id * 0x3C : NULL;
}
extern NPCLevelData *gLevData;
extern Gobj *mGrenadeGobj[];
extern u8 gPlayerOnFireFlag;

/* Grenade throw request shared with the weapon code */
typedef struct NPCGrenadeThrow {
    /* 0x0 */ u8 ready;
    /* 0x1 */ u8 pad[3];
    /* 0x4 */ s32 weapon;
    /* 0x8 */ s32 unk8;
    /* 0xC */ VECTOR32 *pos; /* the thrown grenade */
} NPCGrenadeThrow;
extern NPCGrenadeThrow *D_8011030C;

/* Point action script step, 0x14 bytes */
typedef struct NPCScriptStep {
    /* 0x00 */ s16 kind; /* index into D_80106474 */
    /* 0x02 */ s16 duration;
    /* 0x04 */ s16 unk04;
    /* 0x06 */ u16 moveMode;
    /* 0x08 */ s16 shoot;
} NPCScriptStep; /* size 0xA */
extern NPCScriptStep look[];
extern NPCScriptStep disarmBomb[];
extern NPCScriptStep kneelStandShoot[];
extern NPCScriptStep kneelRollShootRoll[];
extern NPCScriptStep strafeOutStrafeBack[];
extern NPCScriptStep grenade[];
extern NPCScriptStep flipSwitch[];
extern NPCScriptStep openDoor[];
extern s32 gPlayerFlashlight;
/* Weapon table (D_80105F58[], 0x20 bytes each) */
typedef struct NPCWeaponInfo {
    /* 0x00 */ u32 flags; /* bits 3-5: class, 8-10: burst */
    /* 0x04 */ u32 unk04;
    /* 0x08 */ u32 unk08 : 6;
    u32 nearDamage : 10;
    u32 farDamage : 10;
    u32 fireRate : 5;
    u32 unk08_31 : 1;
    /* 0x0C */ u32 unk0C;
    /* 0x10 */ s32 flash;
    /* 0x14 */ u8 unk14[0xC];
} NPCWeaponInfo;
extern NPCWeaponInfo D_80105F58[];


/* Weapon an NPC carries, 0 for none (GameObjectInfo+0x24); f is a scratch s32 */
#define NPC_WEAPON(id, f)                                                              \
    (*(u8 *)&gameObjectsInfo[id].flags24 != 0                                          \
         ? *(u8 *)&gameObjectsInfo[id].flags24                                         \
         : ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[id]) & 0x3000) != 0x1000 ? (f == 0x2000 ? 20 : 0) : 19))
extern s32 D_801457A4;

/* What a generated NPC was made from (genData[id - first generator NPC]) */
typedef struct NPCGenRecord {
    u8 gen;
    u8 path;
    u8 variant;
} NPCGenRecord;
extern NPCGenRecord genData[];

extern u8 gNPCLevelAlertFlag;
extern void **gCamera;
extern u8 gSniperCam[];
extern s32 gSniperMode;
extern s32 gCurrentWeapon;
extern LOSHit *losHitsSorted[];
extern u32 losSortedCount;

extern s32 npcList[NPC_SLOTS];    /* object id per slot, -1 when free */
extern NPCGenerator *generatorInfo;
extern DamagePacket D_8011E8E8[DAMAGE_PACKETS];
#define D_8011E8D4 D_8011E8C8.pad
extern VECTOR32 grenadePos;
extern u8 iAnimationArray[];
extern u8 bone[][4];              /* fire bones */
extern s16 gTaserGobjID;

s32 npcRequestKneel(Gobj *obj);
s32 npcRequestKneelRelease(Gobj *obj);
void doShootingLogic(Gobj *obj, s32 arg1);
void NPCInitSlot(Gobj *obj, s32 slot);
void *NPCCombatModeCallback(void (*cb)());
void *NPCSetCopDeactivateCallback(void (*cb)());
void *NPCSetInteractEnteredRangeCallback(void (*cb)());
void *NPCSetInteractExitedRangeCallback(void (*cb)());
void *NPCSetInteractOtherCallback(void (*cb)());
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);
void NPCSetAwarenessLevel(Gobj *obj, s32 level, s32 arg2, s32 arg3);
void NPCSoundFromPC(s32 kind, s32 radius);
void NPCSound(s32 kind, VECTOR32 *pos, s32 radius, s32 id);
void ParticleNPCFireHide(s16 id);
s32 NPCPointIQ(); /* (Gobj *obj, PathPoint *path), some callers leave out the path */
void NPCKneel(Gobj *obj);
void NPCStand(Gobj *obj);
void NPCNavigateActionPath(Gobj *obj);
void NPCPathDeltaToAction(PathPoint *path, Gobj *obj, VECTOR32 *target, VECTOR32 *out);
s32 getPointActionLink(PathPoint *points, Gobj *obj);
s32 NPCGetGrenadeLink(Gobj *obj);
s32 rcos(s32 a);
s32 rsin(s32 a);
void MovementGobjRegisterCharacterMovement(Gobj *obj, VECTOR32 *dir, s32 mode);
void ParticleBreath(s16 id);
extern s32 particleBreathFlag;
/* TargetInfo+0x64: aim point (gameobj.h has it inside unk62[]) */
#define TARGET_AIM(t) ((VECTOR32 *)((u8 *)(t) + 0x64))
void EGSGeometryFIX_VectorGetAngleFromXZPlane(VECTOR32 *dir, VECTOR32 *angles);
s32 ratan2(s32 y, s32 x);
int rand(void);
void CameraGetDisplay(void *camera, void **display);
void GSActivateGameObject(Gobj *obj, void *camera, s32 arg2);
void GSDeactivateGameObject(Gobj *obj, void *display, void *sniperDisplay);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, void *arg);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, u32 *dist);
void func_80065F5C(s32 id, s32 by);
void NPCHazmatStop(s32 id);
u8 NPCGetMoveMode(Gobj *obj);

void NPCShoot(Gobj *obj);
void NPCSwitch(Gobj *obj);
void NPCOpenDoor(Gobj *obj);
void NPCOpenDoorMessage(Gobj *obj);
void NPCHazmatDone(Gobj *obj);
void NPCThrowGrenade(Gobj *obj);
void NPCRollTowardPath(Gobj *obj);
void NPCRollAwayFromPath(Gobj *obj);
void NPCWalkToPoint(Gobj *obj);
void NPCStrafeTowardPath(Gobj *obj);
void NPCStrafeAwayFromPath(Gobj *obj);

/* .data */
static u16 D_80106438[20] = { 100, 100, 100, 100, 100, 100, 100, 150, 150, 150, 150, 150, 150, 150, 200, 200, 200, 200, 200, 100 }; /* NPC health per level */
static u8 D_80106460[20] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 0 };
static PointAction D_80106474[19] = {
    { 0, 0, NULL, 0 },
    { 0, 0, NPCKneel, 0 },
    { 0, 0, NPCStand, 0 },
    { 0, 0, NPCRollTowardPath, 0 },
    { 0, 0, NPCRollAwayFromPath, 0 },
    { 0, 0, NPCStrafeTowardPath, 0 },
    { 0, 0, NPCStrafeAwayFromPath, 0 },
    { 0, 0, NPCShoot, 0 },
    { 0, 0, NPCShoot, 0 },
    { 0, 0, NPCThrowGrenade, 0 },
    { 60, 0, NULL, 0 },
    { 0, 0, NPCSwitch, 0 },
    { 0, 0, NPCOpenDoor, 0 },
    { 0, 0, NPCOpenDoorMessage, 0 },
    { 21, 0, NULL, 0 },
    { 0, 0, NPCWalkToPoint, 0 },
    { 37, 0, NULL, 0 },
    { 0, 0, NPCHazmatDone, 0 },
    { 83, 0, NULL, 0 },
};
NPCScriptStep kneelStandShoot[5] = {
    { 1, 20, 0, 5, 1 },
    { 8, 30, 63, 5, 0 },
    { 2, 20, 0, 5, 1 },
    { 7, 30, 63, 5, 0 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep kneelRollShootRoll[8] = {
    { 1, 20, 0, 5, 1 },
    { 8, 2, 0, 5, 0 },
    { 3, 32766, 0, 6, 1 },
    { 7, 40, 63, 5, 0 },
    { 4, 32766, 0, 6, 1 },
    { 15, 32766, 0, 6, 0 },
    { 8, 30, 63, 5, 0 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep strafeOutStrafeBack[5] = {
    { 5, 32766, 0, 5, 0 },
    { 7, 40, 63, 5, 0 },
    { 6, 32766, 0, 5, 0 },
    { 8, 30, 63, 5, 0 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep look[2] = {
    { 10, 120, 31, 5, 0 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep openDoor[4] = {
    { 2, 20, 0, 5, 1 },
    { 12, 20, 0, 5, 1 },
    { 13, 1, 0, 5, 1 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep flipSwitch[3] = {
    { 2, 20, 0, 5, 1 },
    { 11, 20, 0, 5, 1 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep disarmBomb[3] = {
    { 18, 400, 0, 5, 1 },
    { 17, 20, 0, 5, 1 },
    { 0, 0, 0, 5, 0 },
};
NPCScriptStep grenade[4] = {
    { 16, 5, 0, 5, 1 },
    { 9, 32767, 0, 5, 1 },
    { 16, 5, 0, 5, 1 },
    { 0, 0, 0, 5, 0 },
};
static s16 D_801066B4[134] = {
    0x23D, 0x23E, 0x23F, 0x240, 0x23D, 0x23E, 0x23F, 0x240, 0x23C, 0x0, 0x1C9, 0x1CD,
    0x1D1, 0x1E9, 0x1E1, 0x201, 0x1C1, 0x1C5, 0x1C9, 0x1CD, 0x1D1, 0x0, 0x1F5, 0x0,
    0x1DD, 0x1F9, 0x103, 0x104, 0x105, 0x106, 0x102, 0x0, 0x107, 0x10F, 0x113, 0x0,
    0x109, 0x10B, 0x10D, 0x0, 0x109, 0x10B, 0x10D, 0x0, 0x118, 0x0, 0x183, 0x184,
    0x185, 0x0, 0x181, 0x182, 0x141, 0x142, 0x143, 0x144, 0x145, 0x14A, 0x14B, 0x17F,
    0x180, 0x0, 0x13F, 0x140, 0x13D, 0x13E, 0x153, 0x0, 0x150, 0x15E, 0x150, 0x15E,
    0xAC, 0xAD, 0xAE, 0x0, 0xAA, 0xAB, 0x12D, 0x12E, 0x12F, 0x0, 0x12B, 0x12C,
    0x13C, 0x0, 0x138, 0x0, 0x138, 0x0, 0x11A, 0x11B, 0x11C, 0x11D, 0x11E, 0x11F,
    0x119, 0x0, 0x1A4, 0x1A5, 0x1A2, 0x0, 0x1A6, 0x0, 0x1B0, 0x1B7, 0x1BD, 0x1BF,
    0x1B0, 0x1B7, 0x1BD, 0x1BF, 0x1A8, 0x1B9, 0x1B3, 0x0, 0x299, 0x29E, 0x296, 0x297,
    0x298, 0x0, 0x23D, 0x23E, 0x23F, 0x240, 0x290, 0x0, 0x28F, 0x290, 0x293, 0x294,
    0x295, 0x0,
};
static NPCXALines D_801067C0[11][6] = {
    { { &D_801066B4[0], 4 }, { &D_801066B4[4], 5 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 } },
    { { &D_801066B4[10], 6 }, { &D_801066B4[16], 5 }, { &D_801066B4[22], 1 }, { NULL, 2 }, { NULL, 2 }, { &D_801066B4[24], 2 } },
    { { &D_801066B4[26], 4 }, { &D_801066B4[30], 1 }, { &D_801066B4[32], 3 }, { &D_801066B4[36], 3 }, { &D_801066B4[40], 3 }, { &D_801066B4[44], 1 } },
    { { &D_801066B4[46], 3 }, { &D_801066B4[50], 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 } },
    { { &D_801066B4[52], 9 }, { &D_801066B4[62], 2 }, { &D_801066B4[64], 3 }, { &D_801066B4[68], 2 }, { &D_801066B4[70], 2 }, { NULL, 2 } },
    { { &D_801066B4[72], 3 }, { &D_801066B4[76], 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 } },
    { { &D_801066B4[78], 3 }, { &D_801066B4[82], 2 }, { &D_801066B4[84], 1 }, { &D_801066B4[86], 1 }, { &D_801066B4[88], 1 }, { NULL, 2 } },
    { { &D_801066B4[90], 6 }, { &D_801066B4[96], 1 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 }, { NULL, 2 } },
    { { &D_801066B4[98], 2 }, { &D_801066B4[100], 1 }, { &D_801066B4[102], 1 }, { &D_801066B4[104], 4 }, { &D_801066B4[108], 4 }, { &D_801066B4[112], 3 } },
    { { &D_801066B4[122], 4 }, { &D_801066B4[122], 4 }, { NULL, 2 }, { &D_801066B4[116], 5 }, { &D_801066B4[116], 5 }, { &D_801066B4[116], 5 } },
    { { &D_801066B4[122], 4 }, { &D_801066B4[122], 4 }, { &D_801066B4[126], 1 }, { &D_801066B4[128], 5 }, { &D_801066B4[128], 5 }, { &D_801066B4[128], 5 } },
};
u8 bone[4][4] = { { 11, 9, 11, 13 }, { 5, 3, 5, 13 }, { 12, 10, 12, 14 }, { 6, 4, 6, 14 } }; /* fire bones */

#ifndef _PSYQ
/* labels the asm functions use inside the vectors above */
__asm__(".set D_80110128, D_80110128_\n"
        ".set D_8011D1D4, D_8011D1D0+4\n"
        ".set D_8011D1D8, D_8011D1D0+8\n"
        ".set D_8011D1E4, D_8011D1E0+4\n"
        ".set D_8011D1E8, D_8011D1E0+8\n"
        ".set D_8011D1F4, D_8011D1F0+4\n"
        ".set D_8011D1F8, D_8011D1F0+8\n"
        ".set D_8011D214, D_8011D210+4\n"
        ".set D_8011D218, D_8011D210+8\n"
        ".set D_8011D224, D_8011D220+4\n"
        ".set D_8011D228, D_8011D220+8\n"
        ".set D_8011D238, D_8011D230+8\n"
        ".set D_8011E8A8, D_8011E890+0x18\n"
        ".set D_8011E8AC, D_8011E890+0x1C\n"
        ".set D_8011E8B0, D_8011E890+0x20\n"
        ".set D_8011E8B8, D_8011E890+0x28\n"
        ".set D_8011E8BC, D_8011E890+0x2C\n"
        ".set D_8011E8C0, D_8011E890+0x30\n"
        ".set D_8011E8C8, D_8011E890+0x38\n"
        ".set D_8011E8CC, D_8011E890+0x3C\n"
        ".set D_8011E8D0, D_8011E890+0x40\n"
        ".set D_8011E8D4, D_8011E890+0x44\n");
#endif

/* Wake an NPC up: full awareness, and leave its path unless it is a stationary type */
#define NPC_ALERT(obj, npc)                                                                 \
    {                                                                                      \
        NPCSetAwarenessLevel(obj, 2, 0, 1);                                                \
        if ((npc)->curPoint != (npc)->nextPoint && GOBJ_TYPE((obj)->id) != 2) {             \
            (npc)->flags |= 0x4000000;                                                     \
        }                                                                                  \
    }
void LevelSetLevelSpecificNpcID(s16 id);
void LevelSetLevelSpecificNpcID2(s16 id);
extern u8 D_80105F55[][0x20];     /* D_80105F58[weapon - 1].unk1D */
void doPointAction(Gobj *obj, s32 kind);
u8 GameSysIsTime(s16 tick, s32 period);
void WeaponSubRound(Gobj *obj);
void WeaponDoFiredCallback(s16 id, s32 weapon);
s32 TargetShoot(Gobj *obj, s32 arg1, s32 arg2);
void EGSAnimGunFlash2(GameObjectAnim *anim, s32 arg1, s32 arg2);
void SysSoundSetShootingExciteLevel(void);
void DamageGOBJ(s16 by, s16 from, s16 id, s16 amount, s32 type);
void TargetCalcProb(s32 id);
void TargetSetThreatLevel(s32 id);
void NPCIQ(Gobj *obj);
void NPCDeadContactTheLiving(Gobj *dead);
void NPCDropPickupStuff(s32 id);
s32 NPCViewConeCheck(Gobj *obj);
s32 NPCCheckGenerators(void);
extern s32 gActivationDistance;
void LevelGetGameObjectMatrix(s16 id, GOBJ_MATRIX *m);
void EGSFrameSetMatrix(EGSFrame *frame, s32 arg1, GOBJ_MATRIX *m);
void UseObject(s16 id);
void NPCDoAction(Gobj *obj);
void NPCDoDamage(Gobj *obj);
void NPCActivate(s32 id);
void NPCDeactivate(s32 id);
void NPCRestore(s32 id, u8 awareness, s32 flags, u8 point);
extern void (*gObjectInteractionCallback)();
void AnimateGobjRequestAim(Gobj *obj, VECTOR32 *pos, s32 arg2, s32 arg3);
s32 NPCPickHostageTarget(s32 id, s32 release);
void TargetAcquire(Gobj *obj, s32 arg1);
s32 TargetViewConeCheck(s16 id, VECTOR32 *from, VECTOR32 *dir, s32 range, s32 cone, VECTOR32 *eye);
void NPCListAdd(Gobj *obj);
void MovementAddGobj(Gobj *obj);
void CobjAddBulletCollisionBoxGobj(Gobj *obj, s32 arg1, s32 arg2, s32 arg3);
void CobjAddCollisionBoxGobj(Gobj *obj, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void WeaponReplace(Gobj *obj);
void NPCDisableGenerator(s32 id);
s32 NPCGenerate(s32 genIdx);
void EGSAnimHMDLinkModel(GameObjectAnim *anim, s32 model);
void NPCGobjInit(Gobj *obj);
void TargetInitGobj(Gobj *obj, TargetInfo *target, s32 arg2, s32 arg3);
void MovementGobjInit(Gobj *obj, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void MovementGobjInitCAS(Gobj *obj, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6);
void PhysicsGobjBasicInit(Gobj *obj, s32 arg1, s32 arg2);
void CollisionGobjBasicInit(Gobj *obj, u32 flags, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
u8 EGSDisplayHMDInActiveSector(GameObjectAnim *anim);
void NPCSoundGobj(s32 kind, s32 id, VECTOR32 *pos, s32 radius);
void NPCDropGun(s32 id);
void NPCPutFireOnNPCShow(s32 id);
void NPCPutFireOnNPCHide(s32 id);
void ParticleNPCFireFade(s16 id);
s32 ParticleNPCFireMove(s16 id, VECTOR32 *pos, s32 arg2);
s32 NPCCreateGenerator(NPCGenerator *gen, s32 id, s32 path, u32 variant);
s32 EGSDisplayGetFuzzyFoggedOutDistance();
void EGSDisplayTransformWorldPointToScreen(VECTOR32 *in, void *camera, VECTOR32 *out);
s32 NPCFindGobjIDForGenerator(void);
s32 NPCFindGeneratorPath(s32 idx);
void NPCRemoveDead(void);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void EGSFrameUpdateHierarchy(EGSFrame *frame);
void EGSDisplayHMDCache(GameObjectAnim *anim, s32 arg1);
void MovementRemoveGobj(Gobj *obj);
void CobjRemoveBulletCollisionBoxGobj(Gobj *obj);
void CobjRemoveCollisionBoxGobj(Gobj *obj);
void NPCSetLOSFirstSeen(Gobj *obj, s32 seen, s32 unused);
void NPCListDelete(Gobj *obj);
void npcHostageSetActivation(Gobj *obj, s32 activate);
void NPCGetNumActive(void);
void PickupAddUseMsg(s16 id, s32 arg1);
void PickupRemoveUseMsg(s16 id);
u8 SysSoundIsXAPlaying(void);
void SysStopXA(void);
void NPCIQEveryFrame(Gobj *obj);
void NPCAimAll(Gobj *obj);
void func_800664F8(Gobj *obj);
void NPCPutFireOnNPCMove(s32 id);
void NPCDeactivateGenerated(Gobj *obj);
void NPCRecordPlayerSighting(Gobj *obj);
void NPCPlayRandXA(Gobj *obj, s32 kind);
void NPCTryToDeactivate(s32 force, s32 unused1, s32 unused2);
void NPCDamaged(s32 id, s32 by);
void NPCTakeShot(Gobj *obj, s32 arg1);
void KeyDroppedByNPC(Gobj *obj);
void ParticleNPCFireShow(s16 id, VECTOR32 *pos, s32 size);
void InterfaceAddNPC(s32 id);
void InterfaceRemoveNPC(s16 id);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void func_801463A4(s16 id, void *anims); /* LevelCreateGOBJ (init.ovl) */
void PlayerCASInit(Gobj *obj, s32 arg1);
void WeaponDrop(Gobj *obj, s32 arg1);
s32 LevelGetPathInfo(s32 id, s32 *start, PathPoint **path);

/*
 * Walk the active NPCs:
 *     id = NPCGetFirstID();
 *     while (id >= 0) { ...; NPC_NEXT_ID(id, i); }
 */
static inline s32 NPCGetFirstID(void) {
    s32 i;
    s32 id;

    getNPCIndex = 0;
    for (i = 0; i < NPC_SLOTS; i++) {
        id = npcList[i];
        if (id >= 0) {
            getNPCIndex = i + 1;
            return id;
        }
    }
    return id;
}

#define NPC_SCAN_IDS(id, i)                  \
    for (; i < NPC_SLOTS; i++) {             \
        id = npcList[i];                     \
        if (id >= 0) {                       \
            getNPCIndex = i + 1;             \
            break;                           \
        }                                    \
    }
#define NPC_NEXT_ID(id, i) \
    id = -1;               \
    i = getNPCIndex;       \
    NPC_SCAN_IDS(id, i)

void NPCInitData(void) {
    NPCCombatModeCallback(0);
    NPCSetCopDeactivateCallback(0);
    NPCSetInteractEnteredRangeCallback(0);
    NPCSetInteractExitedRangeCallback(0);
    NPCSetInteractOtherCallback(0);
}

void NPCInit(void) {
    s32 i;

    for (i = 0; i < NPC_SLOTS; i++) {
        npcList[i] = -1;
        D_80110128[i] = 0;
    }
    NPCNumActive = 0;
    NPCNumActiveTick = 0;
    gNPCLevelAlertFlag = 0;
    D_80110138 = -1;
    D_8011013C = -1;
    D_80110140 = -1;
    D_80110148 = -1;
    D_80110158 = 0;
    D_8011015C = 0;
    D_80110150 = -1;
    D_80110154 = -1;
    copKilledByPCFlag = 0;
    D_80110160 = 0;
    D_80110164 = 0;
    npcNumGenerators = 0;
    D_80110130 = 0;
    D_8011E8D4 = 0;
    npcVisibilityDistance = 0;
    npcGrenadeLandedCheck = 0;
    npcAlertByScream = 0;
    NPCPopRemoveID = -1;
    NPCEndActivationFlag = 0;
}

void NPCPlayRandXA(Gobj *obj, s32 kind) {
    NPC *npc = NPC_OF(obj);
    s16 *tracks;
    s32 r;
    s32 offset;
    s32 timer;

    if (npc->xaTimer == 0) {
        tracks = D_801067C0[npc->voice][kind].tracks;
        if (tracks != NULL) {
            r = rand() % D_801067C0[npc->voice][kind].count;
            offset = 0;
            if (npc->voice == 1) {
                offset = obj->id & 3;
            }
            SysPlayXA(tracks[r] + offset, (s32)obj, 0);
            timer = (rand() & 0x3F) - 0x40;
            npc->xaTimer = timer;
            if (npc->voice >= 2) {
                npc->xaTimer = (u8)timer >> 1;
            }
        }
    }
}

void NPCInitHostageTaker(s32 id) {
    Gobj *cur = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(cur);
    s32 flags = npc->flags;
    s32 ref;
    GameObjectHealth *health;
    Gobj *taker = cur;

    if (!(flags & 0x1000000)) {
        npc->flags = flags | 0x10;
        NPC_OF(cur)->flags |= 0x1000000;
        cur->unk20[1] = EGSDisplayGetFuzzyFoggedOutDistance() + 0x40;
    } else {
        npc->flags = flags & ~0x10;
        NPC_OF(cur)->flags &= ~0x1000000;
        NPCSound(5, &cur->move->pos, 32000, cur->id);
    }
    NPC_OF(taker)->nameMsgId = 99;
    taker->target->targetId = gameObjectsInfo[cur->id].refId;
    do {
    next:
        ref = gameObjectsInfo[cur->id].refId;
        if (ref >= 0) {
            cur = gameObjectsInfo[ref].runtime;
            npc = NPC_OF(cur);
            if (npc != NULL) {
                if (cur->healthInfo->health > 0) {
                    if (!(npc->flags & 0x1000000)) {
                        npc->flags |= 0x1000000;
                    } else {
                        npc->flags &= ~0x1000000;
                    }
                    if (NPC_OF(cur)->flags & 0x1000000) {
                        NPC_OF(cur)->flags |= 0x8000000;
                        cur->unk20[1] = 0xFF;
                        cur->flags23 |= 2;
                        health = cur->healthInfo;
                        health->armor = 50;
                        health->health = 50;
                    } else {
                        NPC_OF(cur)->flags &= ~0x8000000;
                        CASRequestAction(cur->id, 0x56);
                        cur->flags23 = (cur->flags23 & ~1) | 8;
                    }
                    NPC_OF(cur)->nameMsgId &= ~1;
                    NPC_OF(cur)->awareness = 0;
                    NPC_OF(cur)->unk45 = 0;
                    NPC_OF(cur)->flags |= 8;
                    NPC_OF(cur)->flags |= 0x80000;
                }
                goto next;
            }
        }
    } while (0);
}

void npcHostageSetActivation(Gobj *obj, s32 activate) {
    s32 ref;

    if (NPC_OF(obj)->flags & 0x10) {
    next:
        ref = gameObjectsInfo[obj->id].refId;
        if (ref >= 0) {
            obj = gameObjectsInfo[ref].runtime;
            if (obj->npc != NULL) {
                if (obj->healthInfo->health > 0) {
                    if (activate) {
                        obj->flags23 = (obj->flags23 & ~2) | 1;
                        GSActivateGameObject(obj, *gCamera, 0);
                    } else {
                        obj->flags23 = (obj->flags23 & ~1) | 2;
                        GSDeactivateGameObject(obj, *gCamera, 0);
                    }
                }
                goto next;
            }
        }
    }
}

s32 NPCPickHostageTarget(s32 id, s32 release) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    Gobj *hostage;
    s32 target = -1;
    s32 ref;
    s32 flags;

    if (*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
        || ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000 || (flags & 0x3000) == 0x2000) {
        hostage = obj;
        D_80110138 = id;
        do {
        next:
            ref = gameObjectsInfo[hostage->id].refId;
            if (ref >= 0) {
                hostage = gameObjectsInfo[ref].runtime;
                if (hostage->npc != NULL && hostage->healthInfo->health > 0) {
                    target = ref;
                } else {
                    goto next;
                }
            }
        } while (0);
        if (target < 0 && release) {
            NPC_OF(obj)->flags &= ~1;
            NPC_OF(obj)->flags &= ~0x10;
            NPC_OF(obj)->flags &= ~0x1000000;
            target = gPlayerOffset;
            NPCSetAwarenessLevel(obj, 2, 1, 1);
            NPCRecordPlayerSighting(obj);
        }
    }
    return target;
}

void NPCShootHostage(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    NPC_OF(obj)->flags |= 1;
    NPCSetAwarenessLevel(obj, 2, 1, 0);
    NPC_OF(obj)->flags &= ~0x80000;
}

#ifdef NON_MATCHING
/* draft: right shape; stack slot order of best/dist2 and some scheduling differ */
s32 NPCPickNPCTarget(s32 self) {
    s32 copBest = 0x7FFFFFF;
    Gobj *obj = gameObjectsInfo[self].runtime;
    NPC *npc = NPC_OF(obj);
    s32 target = obj->target->targetId;
    s32 cop2Best = 0x7FFFFFF;
    VECTOR32 pos;
    s32 dist;
    VECTOR32 other;
    s32 best = 0x7FFFFFF;
    s32 dist2;
    s32 d3;
    s32 d4;
    s32 id;
    s32 i;
    Gobj *t;
    Gobj *o;

    if (gSysInfo.level == 14) {
        if (obj->healthInfo->health > 0) {
            if (target >= 0) {
                t = gameObjectsInfo[target].runtime;
            } else {
                t = NULL;
            }
            if (npc->flags & 0x2000000) {
                if (t == NULL || t->healthInfo->health <= 0 || (t->npc != NULL && !(NPC_OF(t)->flags & 0x200))) {
                    target = -1;
                    npc->flags &= ~0x2000000;
                }
            } else {
                pos = obj->move->pos;
                id = NPCGetFirstID();
                D_80110158 = 0;
                D_8011015C = 0;
                target = -1;
                if (npc->unk28 <= 480) {
                    target = gPlayerOffset;
                    best = npc->unk28;
                }
                while (id >= 0) {
                    o = gameObjectsInfo[id].runtime;
                    if (o->healthInfo->health > 0 && (NPC_OF(o)->flags & 1)) {
                        if (!(NPC_OF(o)->nameMsgId & 1)) {
                            D_80110158++;
                        } else {
                            D_8011015C++;
                        }
                        if (((NPC_OF(obj)->nameMsgId ^ 1) & 1) != ((NPC_OF(o)->nameMsgId ^ 1) & 1)) {
                            EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &o->move->pos, &dist);
                            if (dist < best) {
                                target = id;
                                best = dist;
                            }
                        }
                    }
                    NPC_NEXT_ID(id, i);
                }
                if (target < 0) {
                    target = gPlayerOffset;
                }
            }
            npc->unk4B = 0;
        }
    } else if (gSysInfo.level == 13) {
        target = -1;
        if (obj->healthInfo->health > 0) {
            D_80110138 = obj->id;
            pos = obj->move->pos;
            id = NPCGetFirstID();
            while (id >= 0) {
                if (id != obj->id) {
                    o = gameObjectsInfo[id].runtime;
                    other = o->move->pos;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &other, &dist2);
                    if (o->healthInfo->health != -1) {
                        D_8011D1D0.vx = pos.vx - other.vx;
                        D_8011D1D4 = pos.vy - other.vy;
                        D_8011D1D8 = pos.vz - other.vz;
                        D_8011D1D4 = 0;
                        GTEMath_VectorNormal(&D_8011D1D0, &D_8011D1D0);
                        if ((D_8011D1D0.vx * npc->dir.vx + D_8011D1D8 * npc->dir.vz < 0 || dist2 < 0x1E0) && dist2 < best) {
                            target = id;
                            best = dist2;
                        }
                    }
                }
                NPC_NEXT_ID(id, i);
            }
            npc->unk4B = 0;
            if (target < 0) {
                if (npc->unk41 != 0) {
                    npc->unk41 = 0;
                }
                return target;
            }
        }
    } else if (npc->flags & 0x10) {
        target = NPCPickHostageTarget(self, 1);
    } else if (obj->healthInfo->health > 0) {
        npc->unk32 = 100;
        if (!(NPC_OF(obj)->nameMsgId & 1) && D_80110154 == self && copKilledByPCFlag) {
            return gPlayerOffset;
        }
        other = obj->move->pos;
        id = NPCGetFirstID();
        D_80110158 = 0;
        if (NPC_OF(obj)->nameMsgId & 1) {
            if (gSysInfo.level != 3 || (obj->id & 1)) {
                target = gPlayerOffset;
                EGSGeometry_GetFuzzyDistancePoint2Point(&other, &gObjPlayer->move->pos, &best);
            } else {
                target = -1;
            }
        } else {
            target = -1;
        }
        while (id >= 0) {
            o = gameObjectsInfo[id].runtime;
            if (o->healthInfo->health > 0 && (NPC_OF(o)->flags & 1)) {
                if (!(NPC_OF(o)->nameMsgId & 1)) {
                    D_80110158++;
                    if (NPC_OF(o)->unk28 < copBest) {
                        copBest = NPC_OF(o)->unk28;
                        D_80110154 = o->id;
                    }
                    if (NPC_OF(obj)->nameMsgId & 1) {
                        EGSGeometry_GetFuzzyDistancePoint2Point(&other, &o->move->pos, &d3);
                        if (d3 < best) {
                            target = id;
                            best = d3;
                        }
                    }
                } else {
                    if (NPC_OF(o)->unk28 < cop2Best) {
                        cop2Best = NPC_OF(o)->unk28;
                        D_80110150 = o->id;
                    }
                    if (!(NPC_OF(obj)->nameMsgId & 1)) {
                        EGSGeometry_GetFuzzyDistancePoint2Point(&other, &o->move->pos, &d4);
                        if (d4 < best) {
                            target = id;
                            best = d4;
                        }
                    }
                }
            }
            NPC_NEXT_ID(id, i);
        }
        if (target < 0) {
            if ((NPC_OF(obj)->nameMsgId & 1) || !copKilledByPCFlag) {
                npcRequestKneelRelease(obj);
                npc->unk41 = 0;
                npc->unk45 = 0;
            } else {
                return gPlayerOffset;
            }
        }
    }
    return target;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickNPCTarget);
#endif

void NPCDamaged(s32 id, s32 by) {
    Gobj *obj;
    Gobj *attacker;

    if (id >= 0 && by >= 0) {
        obj = gameObjectsInfo[id].runtime;
        attacker = gameObjectsInfo[by].runtime;
        if (obj->npcState == 2 && (attacker->npcState == 1 || attacker->npcState == 2)) {
            obj->target->targetId = by;
            NPC_OF(obj)->flags |= 0x2000000;
        }
    }
}

void NPCSound(s32 kind, VECTOR32 *pos, s32 radius, s32 srcId) {
    s32 id;
    s32 i;
    Gobj *obj;
    NPC *npc;
    s32 dist;

    id = NPCGetFirstID();
    if (radius >= 0xC80 || kind == 0) {
        while (id >= 0) {
            if (id != srcId) {
                npc = NPC_OF(gameObjectsInfo[id].runtime);
                npc->unk34 = radius;
                npc->unk40 = kind;
            }
            NPC_NEXT_ID(id, i);
        }
    } else {
        while (id >= 0) {
            if (id != srcId) {
                obj = gameObjectsInfo[id].runtime;
                npc = NPC_OF(obj);
                if (npc->unk34 == 0 && npc != NULL) {
                    EGSGeometry_GetFuzzyDistancePoint2Point(pos, &obj->move->pos, &dist);
                    if (dist < radius) {
                        npc->unk34 = radius;
                        npc->unk40 = kind;
                    }
                }
            }
            NPC_NEXT_ID(id, i);
        }
    }
}

void NPCSoundFromPC(s32 kind, s32 radius) {
    s32 id;
    s32 i;
    NPC *npc;

    if (radius > 35000) {
        radius = 35000;
    }
    id = NPCGetFirstID();
    while (id >= 0) {
        npc = NPC_OF(gameObjectsInfo[id].runtime);
        if (npc != NULL && npc->unk34 < radius && npc->unk28 < radius) {
            npc->unk34 = radius;
            npc->unk40 = kind;
        }
        NPC_NEXT_ID(id, i);
    }
}

void NPCSoundGobj(s32 kind, s32 id, VECTOR32 *pos, s32 radius) {
    Gobj *obj;

    if (id >= 0) {
        obj = gameObjectsInfo[id].runtime;
        if (id == gPlayerOffset) {
            if (kind != 1) {
                NPCSoundFromPC(3, radius);
                return;
            }
            kind = 3;
        } else if (kind == 4) {
            NPC_OF(obj)->unk34 = radius;
            NPC_OF(obj)->unk40 = kind;
        }
        if (id == gPlayerOffset || (obj->npc != NULL && (!(NPC_OF(obj)->flags & 1) || obj->target->targetId == gPlayerOffset))) {
            if (pos == NULL) {
                pos = &obj->move->pos;
            }
            NPCSound(kind, pos, radius, id);
        }
    }
}

void NPCRegPCSound(s16 id) {
    if (id == gPlayerOffset && !(gExeTick & 7) && gObjPlayer->cas->active != 2) {
        NPCSoundFromPC(2, 0x1E0);
    }
}

void NPCGobjInit(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 pos;
    s32 start;
    s32 f;
    s32 refId;
    Gobj *ref;
    s32 type;
    u8 kind;
    s32 level;
    s32 w;
    s32 t;

    obj->npcState = 2;
    npc->curPoint = 0xFF;
    npc->unk49 = 0xFF;
    npc->unk38 = 20;
    npc->unk28 = 2400;
    npc->flags = 0;
    npc->nextPoint = 0;
    npc->dir.vz = 0;
    npc->dir.vy = 0;
    npc->dir.vx = 0;
    npc->unk46 = 5;
    LevelGetPathInfo(obj->id, &start, &D_80110134);
    if (D_80110134 == NULL) {
        start = 99;
    } else if (start < 0) {
        start = 2;
    } else if (start >= 99) {
        start = 98;
    }
    npc->nameMsgId = start;
    npc->awareness = 0;
    npc->unk32 = 100;
    npc->unk34 = 0;
    npc->voice = 0;
    w = NPC_WEAPON(obj->id, f);
    t = D_80105F55[w][0];
    npc->unk41 = 0;
    npc->unk30 = 0;
    npc->unk36 = t;
    if (obj->id == gPlayerOffset || (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x4000)) {
        if (GOBJ_TYPE(obj->id) != 2) {
            obj->healthInfo->armor = 600;
        } else {
            obj->healthInfo->armor = 2400;
        }
    }
    npc->unk45 = 0;
    npc->unk2C = 0;
    npc->unk42 = 0;
    npc->flags |= 0x800;
    NPCUninitSlot(obj);
    pos.vx = D_80110134->x;
    pos.vy = D_80110134->y;
    pos.vz = D_80110134->z;
    gameObjectsInfo[obj->id].matrix.t[0] = pos.vx;
    gameObjectsInfo[obj->id].matrix.t[1] = -pos.vy;
    gameObjectsInfo[obj->id].matrix.t[2] = pos.vz;
    if (obj->anim->frame->pos.pad == 0) {
        obj->anim->frame->pos.vx = pos.vx;
        obj->anim->frame->pos.vy = -pos.vy;
        obj->anim->frame->pos.vz = pos.vz;
    } else {
        EGSFrameSetPosition(obj->anim->frame, 0, &pos);
    }
    EGSFrameUpdateHierarchy(obj->anim->frame);
    npc->unk2E = pos.vy;
    npc->unk4B = (u8)D_80110134->flags >> 4;
    obj->anim->unk00[0xB] &= ~0x20;
    if ((GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x8000) && (GOBJ_TYPE(obj->id) != 0x5C || gSysInfo.level != 14)) {
        obj->flags23 |= 2;
    } else {
        obj->flags23 &= ~2;
    }
    npc->xaTimer = rand() | 0xC0;
    if (GOBJ_TYPE(obj->id) != 0x3C) {
        PlayerCASInit(obj, (D_80105F58[NPC_WEAPON(obj->id, f)].flags >> 3) & 7);
    }
    npc->casAction = 0;
    refId = gameObjectsInfo[obj->id].refId;
    if (refId >= 0 && (ref = gameObjectsInfo[refId].runtime) != NULL) {
        type = GOBJ_TYPE(ref->id);
        if (type == GOBJ_TYPE_BOMB) {
            npc->nameMsgId = 0;
            npc->voice = 1;
            npc->flags |= 8;
        } else if (NPC_OF(obj)->nameMsgId != 2 && (type == 0x37 || type == 0x2C)) {
            npc->flags |= 0x20000000;
        }
    }
    kind = npc->nameMsgId;
    if (kind < 2) {
        npc->nameMsgId = kind + 98;
        npc->flags |= 1;
        NPCSetAwarenessLevel(obj, 2, 1, 0);
        if (!(NPC_OF(obj)->nameMsgId & 1) && gSysInfo.level == 0) {
            npc->voice = 1;
        }
    } else {
        if (kind == 3) {
            npc->nameMsgId = 90;
            LevelSetLevelSpecificNpcID(obj->id);
        }
        if (npc->nameMsgId == 4) {
            npc->nameMsgId = 91;
            LevelSetLevelSpecificNpcID2(obj->id);
        }
        if (gSysInfo.level == 18) {
            npc->voice = 8;
        }
        level = (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x300) >> 8;
        if (level >= 2) {
            NPCSetAwarenessLevel(obj, level - 1, 1, 0);
        }
    }
    if ((gSysInfo.level == 11 || gSysInfo.level == 12) && GOBJ_TYPE(obj->id) == 0x5C) {
        npc->voice = 9;
    }
    if (GOBJ_TYPE(obj->id) == 0x65) {
        npc->voice = 10;
    }
}

void NPCPutFireOnNPCShow(s32 id) {
    VECTOR32 pos;
    s32 size;
    s32 i;
    Gobj *obj = gameObjectsInfo[id].runtime;

    for (i = 0; i < 4; i++) {
        size = 0x400;
        if (!(i & 1)) {
            size = 0x200;
        }
        pos.vx = obj->anim->frames[bone[i][0]]->pos.vx;
        pos.vy = obj->anim->frames[bone[i][0]]->pos.vy;
        pos.vz = obj->anim->frames[bone[i][0]]->pos.vz;
        pos.vy = -pos.vy;
        ParticleNPCFireShow(obj->id | (i << 12), &pos, size);
    }
    if (obj->npc != NULL) {
        NPC_OF(obj)->flags |= 4;
        if (obj->id != gTaserGobjID) {
            CASRequestAction(obj->id, 0x4F);
        }
    }
}

void NPCPutFireOnNPCHide(s32 id) {
    s32 i;
    Gobj *obj = gameObjectsInfo[id].runtime;

    for (i = 0; i < 4; i++) {
        ParticleNPCFireHide(obj->id | (i << 12));
    }
    if (obj->npc != NULL) {
        NPC_OF(obj)->flags &= ~4;
    }
}

void NPCPutFireOnNPCMove(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    s32 done = 0;
    s32 i;
    s32 last;
    VECTOR32 pos;
    s16 pid;

    if (obj->npc != NULL) {
        if (!(NPC_OF(obj)->flags & 4)) {
            NPCPutFireOnNPCShow(id);
        }
        if (obj->healthInfo->health != -1) {
            NPC_OF(obj)->unk32 = 100;
            if (!gPlayerOnFireFlag && NPC_OF(obj)->unk28 < 0x80) {
                SysPostMessage(0xD, 4, GOBJ_ID_666, gPlayerOffset, 0, 0, 0, 0);
            }
        }
        last = (gExeTick + id) & 3;
        i = last;
    } else {
        i = 0;
        last = 3;
    }
    for (; i <= last; i++) {
        pos.vx = obj->anim->frames[bone[i][rand() & 3]]->pos.vx;
        pos.vy = obj->anim->frames[bone[i][rand() & 3]]->pos.vy;
        pos.vz = obj->anim->frames[bone[i][rand() & 3]]->pos.vz;
        pos.vy = -pos.vy;
        pid = id | (i << 12);
        done = ParticleNPCFireMove(pid, &pos, 0);
        if (obj->healthInfo->health == -1 || (id == gPlayerOffset && gPlayerOnFireFlag == 2)) {
            ParticleNPCFireFade(pid);
        }
    }
    if (done && obj->npc != NULL) {
        NPC_OF(obj)->flags &= ~2;
    }
}

void NPCDeadContactTheLiving(Gobj *dead) {
    s32 id;
    s32 i;
    Gobj *obj;
    NPC *npc;
    s32 dist;
    VECTOR32 *pos = &dead->move->pos;

    id = NPCGetFirstID();
    while (id >= 0) {
        obj = gameObjectsInfo[id].runtime;
        if (obj->healthInfo->health > 0) {
            npc = NPC_OF(obj);
            if (npc != NULL) {
                EGSGeometry_GetFuzzyDistancePoint2Point(&obj->move->pos, pos, &dist);
                if (dist < 0x1E0 && npc->awareness != 2) {
                    NPCSetAwarenessLevel(obj, 1, 0, 1);
                }
            }
        }
        NPC_NEXT_ID(id, i);
    }
}

s32 npcRequestKneel(Gobj *obj) {
    CASState *cas = obj->cas;
    NPC *npc = NPC_OF(obj);
    s32 ok = 0;

    if (!(cas->flags & 0x80) && cas->active != 2 && npc->nameMsgId >= 11 && GOBJ_TYPE(obj->id) != 0x5C
        && !npcGrenadeLandedCheck && !(npc->flags & 0x200000) && obj->target->targetId >= 0) {
        CASRequestAction(obj->id, 0xA);
        ok = 1;
        npc->flags |= 0x200000;
    }
    return ok;
}

s32 npcRequestKneelRelease(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    s32 ok = 0;

    if (obj->cas->active == 2 && GOBJ_TYPE(obj->id) != 0x5C && !(npc->flags & 0x200000)) {
        CASRequestAction(obj->id, 0xB);
        ok = 1;
        npc->flags |= 0x200000;
    }
    return ok;
}

#ifdef NON_MATCHING
/* first NPC scan's found block not moved out of line like the target; tail branch layout */
void npcGrenadeLanded(s32 which) {
    s32 safe;
    Gobj *obj;
    s32 id;
    s32 i;
    VECTOR32 *grenade;
    s32 dist;
    NPC *npc;

    if (!(npcGrenadeLandedCheck & (which + 1))) {
        safe = 1;
        obj = NULL;
        grenade = &mGrenadeGobj[which]->move->pos;
        id = NPCGetFirstID();
        while (id >= 0) {
            obj = gameObjectsInfo[id].runtime;
            if (obj->healthInfo->health > 0 && NPC_OF(obj)->awareness != 0) {
                EGSGeometry_GetFuzzyDistancePoint2Point(&obj->move->pos, grenade, &dist);
                if (dist < 0xA00) {
                    npc = NPC_OF(obj);
                    npc->unk45 = 0;
                    npc->flags = (npc->flags | 0x4000000) & ~0x800000;
                    npcRequestKneelRelease(obj);
                    if (dist < 0x280) {
                        safe--;
                    }
                }
            }
            NPC_NEXT_ID(id, i);
        }
        npcAlertByScream = 0;
        npcGrenadeLandedCheck |= which + 1;
        if (which == 0 && !(rand() & 1) && safe >= 0 && safe == 0 && NPC_OF(obj)->voice == 0) {
            SysPlayXA((rand() & 1) ? 0x23A : 0x27B, (s32)obj, 0);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", npcGrenadeLanded);
#endif

#ifdef NON_MATCHING
/* draft: structure right, 17 insns long; abs/loop register allocation differs */
s32 NPCPointIQ(obj, path)
Gobj *obj;
PathPoint *path;
{
    VECTOR32 pt;
    s32 dist;
    s32 best;
    s32 d2;
    VECTOR32 away;
    VECTOR32 to;
    NPC *npc = NPC_OF(obj);
    VECTOR32 *goal = &npc->lastSeenPos;
    s32 range = 0;
    s32 skipKind = -1;
    u8 next;
    u8 cur;
    PathPoint *curPt;
    PathPoint *p;
    s32 i;
    s32 j;
    s32 link;
    s32 link2;
    s32 start;
    u8 name;

    if (npcGrenadeLandedCheck) {
        if (npcGrenadeLandedCheck & 1) {
            EGSGeometry_GetFuzzyDistancePoint2Point(&mGrenadeGobj[0]->move->pos, &D_8011E8B8, &dist);
            if (dist < 0xA00) {
                goal = &mGrenadeGobj[0]->move->pos;
                range = 0x780;
            }
        } else if (npcGrenadeLandedCheck & 2) {
            EGSGeometry_GetFuzzyDistancePoint2Point(D_8011030C->pos, &D_8011E8B8, &dist);
            if (dist < 0x500) {
                goal = D_8011030C->pos;
                range = 0x3C0;
            }
        }
    } else if (npcAlertByScream) {
        EGSGeometry_GetFuzzyDistancePoint2Point(&grenadePos, &D_8011E8B8, &dist);
        if (dist < 0x3C0) {
            range = 32000;
            goal = &grenadePos;
        }
    }
    if (range == 0) {
        name = npc->nameMsgId;
        if (name < 10) {
            range = 32000;
        } else if (npc->flags & 1) {
            if (gSysInfo.level == 13) {
                return path[npc->curPoint].links[0];
            }
            if (obj->target->targetId >= 0) {
                goal = &gameObjectsInfo[obj->target->targetId].runtime->move->pos;
                range = 32;
            } else {
                range = 32000;
            }
        } else {
            range = 32;
            if (npc->awareness != 1 && gameObjectsInfo[gPlayerOffset].runtime->healthInfo->health > 0) {
                range = (100 - name) << 5;
            }
        }
    } else {
        D_8011D1E0.vx = (goal->vx + D_8011E8A8.vx) >> 1;
        D_8011D1E0.vy = goal->vy;
        D_8011D1E0.vz = (goal->vz + D_8011E8A8.vz) >> 1;
        goal = &D_8011D1E0;
    }
    next = npc->curPoint;
    curPt = &path[next];
    pt.vx = curPt->x;
    pt.vy = curPt->y;
    pt.vz = curPt->z;
    EGSGeometry_GetFuzzyDistancePoint2Point(&pt, goal, &best);
    best = best - range >= 0 ? best - range : -(best - range);
    cur = next;
    if (((curPt->flags & 0xF00) >> 8) == 7) {
        skipKind = 7;
    }
    for (i = 0; i < 3; i++) {
        link = curPt->links[i];
        p = &path[link];
        if (link >= 0 && link != cur && ((p->flags & 0xF00) >> 8) != 1 && ((p->flags & 0xF00) >> 8) != skipKind) {
            pt.vx = p->x;
            pt.vy = p->y;
            pt.vz = p->z;
            EGSGeometry_GetFuzzyDistancePoint2Point(&pt, goal, &dist);
            dist = dist - range >= 0 ? dist - range : -(dist - range);
            for (j = 0; j < 3; j++) {
                link2 = p->links[j];
                if (link2 >= 0 && link2 != cur && link2 != link && link2 != 1 && link2 != skipKind) {
                    pt.vx = path[link2].x;
                    pt.vy = path[link2].y;
                    pt.vz = path[link2].z;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&pt, goal, &d2);
                    d2 = d2 - range >= 0 ? d2 - range : -(d2 - range);
                    if (d2 < dist) {
                        dist = d2;
                    }
                }
            }
            if (dist < best) {
                next = link;
                best = dist;
                npc->flags &= ~0x40000;
            }
        }
    }
    if (next == cur) {
        if (GOBJ_TYPE(obj->id) == 0x65 && obj->healthInfo->health < obj->healthInfo->armor && obj->id != gPlayerOffset
            && !(GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x4000)
            && *(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
            && (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) != 0x1000
            && (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) != 0x2000 && !(npc->flags & 0x8000000)) {
            CASRequestAction(obj->id, 0x55);
        }
    }
    if (next == cur || next == npc->nextPoint) {
        if (npc->flags & 2) {
            DamageGOBJ(-1, -1, obj->id, 0x7FFF, 0x4C);
            return next;
        }
        if (npc->awareness == 1) {
            start = rand() & 3;
            for (i = 0; i < 3; i++) {
                j = start + i;
                if (j >= 3) {
                    j -= 3;
                }
                link = path[cur].links[j];
                if (link >= 0 && link != cur && ((path[link].flags & 0xF00) >> 8) != 1
                    && ((path[link].flags & 0xF00) >> 8) != 7) {
                    next = link;
                    npc->flags |= 0x40000;
                }
            }
            if (next == cur || next == npc->nextPoint) {
                NPCSetAwarenessLevel(obj, 2, 1, 0);
            }
            return next;
        }
        if (npc->awareness == 2 && range <= 0x3C0 && !(obj->target->flags & 2)) {
            away.vx = goal->vx - D_8011E8B8.vx;
            away.vy = goal->vy - D_8011E8B8.vy;
            away.vz = goal->vz - D_8011E8B8.vz;
            for (i = 0; i < 3; i++) {
                link = path[cur].links[i];
                p = &path[link];
                if (link >= 0 && link != cur && ((p->flags & 0xF00) >> 8) != 1
                    && ((p->flags & 0xF00) >> 8) != skipKind) {
                    pt.vx = p->x;
                    pt.vy = p->y;
                    pt.vz = p->z;
                    to.vx = goal->vx - pt.vx;
                    to.vy = goal->vy - pt.vy;
                    to.vz = goal->vz - pt.vz;
                    if (to.vx * away.vx + to.vz * away.vz < 0) {
                        next = link;
                        npc->flags |= 0x40000;
                        break;
                    }
                }
            }
        }
    }
    return next;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPointIQ);
#endif

s32 NPCPickNextPoint(Gobj *obj, PathPoint *path, s32 cur) {
    NPC *npc = NPC_OF(obj);
    s32 next = -1;
    s32 link;
    u32 kind;

    if (npc->awareness == 0 || GOBJ_TYPE(obj->id) == 2) {
        link = path[cur].links[0];
        kind = (path[link].flags & 0xF00) >> 8;
        if (link >= 0 && kind != 1 && kind != 7) {
            next = link;
        }
    } else {
        next = NPCPointIQ(obj, path);
    }
    if (next >= 0) {
        if (npc->curPoint != next) {
            npc->nextPoint = npc->curPoint;
            npc->curPoint = next;
            npc->unk49 = 0xFF;
            return 1;
        }
        return 0;
    }
    return 0;
}

void NPCRethinkNextPoint(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    u8 next;
    u8 cur;

    if (!(npc->flags & 0x408)) {
        if (!(npc->flags & 0x800)) {
            if (NPCPointIQ(obj) == npc->nextPoint) {
                next = npc->nextPoint;
                cur = npc->curPoint;
                npc->unk49 = 0xFF;
                npc->curPoint = next;
                npc->nextPoint = cur;
                npc->dir.vx = -npc->dir.vx;
                npc->dir.vz = -npc->dir.vz;
            }
        } else {
            npc->unk45 = 0;
        }
    }
}

void NPCRecordPlayerSighting(Gobj *obj) {
    NPC *npc = NPC_OF(obj);

    if (obj->healthInfo->health > 0 && (npc->awareness != 1 || (obj->target->flags & 2))) {
        npc->lastSeenPos = D_8011E8A8;
        npc->unk32 = 100;
    }
}

void NPCSetAwarenessLevel(Gobj *obj, s32 level, s32 unused, s32 notify) {
    NPC *npc = NPC_OF(obj);
    s32 flags = npc->flags;

    if ((obj->healthInfo->health <= 0 || !(flags & 8)) && !npcPCInvisibleFlag) {
        NPCRecordPlayerSighting(obj);
        if (level != npc->awareness) {
            if (notify && level == 2 && D_8010F94C != NULL) {
                D_8010F94C(obj->id);
            }
            if (level != npc->awareness) {
                if (level == 2 || (flags & 1)) {
                    npc->awareness = 2;
                    npcRequestKneelRelease(obj);
                    if (NPCNumActive >= 2 && !(rand() & 3)) {
                        if (npc->flags & 0x100) {
                            NPCPlayRandXA(obj, 4);
                        } else {
                            NPCPlayRandXA(obj, 3);
                        }
                    }
                } else {
                    npc->awareness = 1;
                    npcRequestKneel(obj);
                }
            }
        }
    }
}

void npcSetLevelAlert(void) {
    s32 id;
    s32 i;
    Gobj *obj;

    id = NPCGetFirstID();
    while (id >= 0) {
        obj = gameObjectsInfo[id].runtime;
        if (obj->npc != NULL) {
            NPCSetAwarenessLevel(obj, 1, 0, 1);
        }
        NPC_NEXT_ID(id, i);
    }
    gNPCLevelAlertFlag = 1;
}

s32 NPCViewConeCheck(Gobj *obj) {
    s32 seen = 0;
    VECTOR32 *eye = &D_8011E8A8;
    NPC *npc = NPC_OF(obj);
    VECTOR32 dir;
    VECTOR32 tpos;
    s32 dist;
    s32 range;
    s32 cone;
    s32 tid;
    s32 near;

    TargetAcquire(obj, 0);
    dist = npc->unk28;
    if ((dist < npcVisibilityDistance - 0xA0 || ((npc->flags & 1) && obj->target->targetId != gPlayerOffset))
        && (obj->target->targetId != gPlayerOffset || !gPlayerFlashlight || dist >= 0xA00
            || (seen = 1, !(obj->target->flags & 2)))) {
        cone = 0x400;
        if (npc->awareness == 0) {
            cone = 0x2AA;
            near = (((npcVisibilityDistance - 0xA0) * 6) >> 4) + 0xA0;
            range = npcVisibilityDistance - near;
        } else {
            range = npcVisibilityDistance - 0xA0;
        }
        if (npc->awareness == 2) {
            tid = obj->target->targetId;
            if (tid == gPlayerOffset) {
                dir.vx = npc->lastSeenPos.vx - D_8011E8B8.vx;
                dir.vy = npc->lastSeenPos.vy - D_8011E8B8.vy;
                dir.vz = npc->lastSeenPos.vz - D_8011E8B8.vz;
            } else if (tid >= 0) {
                dir.vx = gameObjectsInfo[tid].runtime->move->pos.vx - D_8011E8B8.vx;
                dir.vy = gameObjectsInfo[tid].runtime->move->pos.vy - D_8011E8B8.vy;
                dir.vz = gameObjectsInfo[tid].runtime->move->pos.vz - D_8011E8B8.vz;
                tpos = gameObjectsInfo[tid].runtime->move->pos;
                eye = &tpos;
            } else {
                dir.vx = (*obj->anim->frames)->m[0][2];
                dir.vy = (*obj->anim->frames)->m[1][2];
                dir.vz = (*obj->anim->frames)->m[2][2];
                dir.vy = -dir.vy;
                dir.vx = -dir.vx;
                dir.vz = -dir.vz;
            }
        } else {
            dir.vx = obj->anim->frame->m[0][2];
            dir.vy = obj->anim->frame->m[1][2];
            dir.vz = obj->anim->frame->m[2][2];
            dir.vy = 0;
        }
        GTEMath_VectorNormal(&dir, &dir);
        seen = TargetViewConeCheck(obj->id, &D_8011E8B8, &dir, range, cone, eye);
    }
    if (seen) {
        if (obj->target->flags & 2) {
            NPCRecordPlayerSighting(obj);
        } else {
            seen = 0;
        }
    }
    return seen;
}

void NPCSetShooting(s32 on) {
    D_8010F958 = on;
}

s32 NPCDebugGetNoDamageFlag(void) {
    return D_8010F958 == 0;
}

#ifdef NON_MATCHING
/* flags CSE'd into the awareness branch (target reloads npc->flags there) */
u8 NPCGetMoveMode(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    u8 mode;
    s32 dist;
    s32 f;

    if ((npc->flags & 0x40000) && (mode = 6, obj->target->targetId >= 0)) {
        goto done;
    }
    if (npc->flags & 0xB) {
        mode = 7;
        if (gSysInfo.level != 13) {
            goto done;
        }
        dist = 0xC80;
        if (obj->target->targetId >= 0) {
            EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011E8B8, &gameObjectsInfo[obj->target->targetId].runtime->move->pos,
                                                    &dist);
        }
        if (*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
            && ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000 && (f & 0x3000) != 0x2000
            && (dist < 0x1E0 || gObjPlayer->target->threat >= 0xAB8)) {
            mode = 5;
            if (obj->target->targetId < 0) {
                goto retreat;
            }
            goto kneel;
        }
        if (obj->target->targetId < 0 || dist > 0x640) {
        retreat:
            D_8011D1D0.vx = D_8011E8B8.vx - D_8011E8A8.vx;
            D_8011D1D4 = D_8011E8B8.vy - D_8011E8A8.vy;
            D_8011D1D8 = D_8011E8B8.vz - D_8011E8A8.vz;
            D_8011D1D4 = 0;
            GTEMath_VectorNormal(&D_8011D1D0, &D_8011D1D0);
            if (D_8011D1D0.vx * npc->dir.vx + D_8011D1D8 * npc->dir.vz < 0 || npc->unk28 < 0x140) {
                D_8010F960 = -1;
                mode = 7;
            } else {
                goto range;
            }
        } else {
        range:
            if (npc->unk28 > 0x640) {
                mode = 5;
            } else {
                mode = 6;
                if (npc->unk28 > 0x280) {
                kneel:
                    D_8010F960 = 1;
                } else {
                    mode = npc->unk46;
                }
            }
        }
        if ((*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
             && ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000 && (f & 0x3000) != 0x2000)
            || D_8010F960 < 0) {
            if (D_8010F960 > 0) {
                if (obj->cas->active != 2 && !(npc->flags & 0x200000)) {
                    CASRequestAction(obj->id, 0xA);
                    npc->flags |= 0x200000;
                }
            } else if (D_8010F960 < 0) {
                npcRequestKneelRelease(obj);
            }
        }
    } else {
        mode = 6;
        if (npc->awareness != 0) {
            if (npc->unk28 < 800) {
                mode = 7;
                if ((npc->flags & 0x20000) && npc->nameMsgId >= 80 && (obj->id & 1) && !npcGrenadeLandedCheck) {
                    mode = 6;
                }
            } else {
                mode = 7;
            }
        }
    }
done:
    npc->unk46 = mode;
    return mode;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetMoveMode);
#endif

void NPCSetChaseStopFlag(s32 id, s32 stop) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(obj);

    if (stop) {
        npc->flags |= 0x1000000;
    } else {
        npc->flags &= ~0x1000000;
    }
    if (stop) {
        npc->unk4B = 0;
        NPCKneel(obj);
    } else {
        npc->unk4B = 0;
        npc->unk41 = 0;
        NPCStand(obj);
    }
    NPCSetAwarenessLevel(obj, 2, 1, 0);
}

s32 NPCDoNoDamage(void) {
    return 0;
}

void NPCPutAllOnRadar(s32 on) {
    s32 i;
    s32 id;
    Gobj *obj;

    if (on != gNpcAllOnRadarFlag) {
        gNpcAllOnRadarFlag = on;
        id = NPCGetFirstID();
        while (id >= 0) {
            obj = gameObjectsInfo[id].runtime;
            if (!(obj->target->flags & 8)) {
                if (on) {
                    InterfaceAddNPC(obj->id);
                } else {
                    InterfaceRemoveNPC(obj->id);
                }
            }
            NPC_NEXT_ID(id, i);
        }
    }
}

void NPCSetLOSFirstSeen(Gobj *obj, s32 seen, s32 unused) {
    if (((obj->target->flags >> 3) & 1) != seen && GOBJ_TYPE(obj->id) != 3) {
        if (!gNpcAllOnRadarFlag) {
            if (seen) {
                if (obj->healthInfo->health > 0) {
                    InterfaceAddNPC(obj->id);
                }
            } else {
                InterfaceRemoveNPC(obj->id);
            }
        }
        obj->target->flags = (obj->target->flags & ~8) | ((seen & 1) << 3);
    }
}

void doShootingLogic(Gobj *obj, s32 dist) {
    NPC *npc = NPC_OF(obj);
    s32 shoot = 0;
    s32 flags;
    s32 chance;
    Gobj *victim;
    VECTOR32 pos;
    s32 d;
    s32 mult;

    if (obj->target->targetId >= 0 && npc->unk41 == 0) {
        if (obj->id != gPlayerOffset) {
            if (*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
                && ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000 && (flags & 0x3000) != 0x2000) {
                return;
            }
        } else if (gCurrentWeapon == 0) {
            return;
        }
        if (npc->unk4B != 1 && gSysInfo.level != 4 && obj->healthInfo->health > 0
            && (u32)npc->unk38 < (u32)(gExeTick - obj->target->lastTick)) {
            if (npc->unk4B == 2) {
                shoot = 1;
            }
            if ((npc->flags & 0x100) && obj->target->unk58 > 0) {
                chance = dist;
                if (chance < 21) {
                    chance = 10;
                }
                if (chance >= (rand() & 0x3F) + (rand() & 0x1F)
                    && gameObjectsInfo[gPlayerOffset].runtime->healthInfo->health > 0) {
                    shoot = 1;
                }
            } else if (obj->target->targetId >= 0 && TARGET_VICTIM(obj->target) >= 0 && (obj->target->flags & 8)) {
                victim = gameObjectsInfo[TARGET_VICTIM(obj->target)].runtime;
                if (victim->healthInfo->health > 0 && victim->npc == NULL) {
                    pos.vx = victim->anim->frame->pos.vx;
                    pos.vy = victim->anim->frame->pos.vy;
                    pos.vz = victim->anim->frame->pos.vz;
                    pos.vy = -pos.vy;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011E8B8, &pos, &d);
                    if (d > 0x280) {
                        shoot = 1;
                    }
                }
            }
            if (shoot) {
                mult = 3;
                if (!((D_80105F58[obj->id].flags >> 8) & 7)) {
                    mult = 1;
                }
                npc->unk41 = mult * ((rand() & 3) + 1);
                if (!(obj->cas->flags & 0x20)) {
                    npcRequestKneelRelease(obj);
                }
            }
        }
    }
}

void NPCShoot(Gobj *obj) {
    if (NPC_OF(obj)->flags & 0x100) {
        doShootingLogic(obj, 0);
    }
}

#ifdef NON_MATCHING
/* regalloc (points kept in a2) + addu operand order */
VECTOR32 *getPointActionVector(PathPoint *points, Gobj *obj) {
    VECTOR32 *v = NULL;
    NPC *npc = NPC_OF(obj);
    s32 i;
    s32 link;

    for (i = 0; i < 3; i++) {
        link = ((s8 *)points[npc->curPoint].links)[i];
        if (link >= 0 && ((points[link].flags & 0xF00) >> 8) == 1) {
            D_8011D1F0.vx = points[link].x;
            D_8011D1F0.vy = points[link].y;
            v = &D_8011D1F0;
            D_8011D1F0.vz = points[link].z;
            break;
        }
    }
    if (v == NULL) {
        v = &D_8011D1F0;
        D_8011D1F0.vx = points[points[npc->curPoint].links[0]].x;
        D_8011D1F0.vy = points[points[npc->curPoint].links[0]].y;
        D_8011D1F0.vz = points[points[npc->curPoint].links[0]].z;
    }
    return v;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", getPointActionVector);
#endif

s32 getPointActionLink(PathPoint *points, Gobj *obj) {
    s32 found = -1;
    s32 link = -1;
    s32 i;

    for (i = 0; i < 3; i++) {
        if (((s8 *)points[NPC_OF(obj)->curPoint].links)[i] >= 0) {
            link = points[NPC_OF(obj)->curPoint].links[i];
            if (((points[link].flags & 0xF00) >> 8) == 1) {
                found = link;
                break;
            }
        }
    }
    if (found < 0) {
        found = link;
    }
    return found;
}

void NPCPathDeltaToAction(PathPoint *path, Gobj *obj, VECTOR32 *target, VECTOR32 *out) {
    VECTOR32 d;
    MoveInfo *move = obj->move;

    d.vx = target->vx - move->pos.vx;
    d.vy = 0;
    d.vz = target->vz - move->pos.vz;
    GTEMath_VectorNormal(&d, out);
}

void NPCKneel(Gobj *obj) {
    NPC *npc = NPC_OF(obj);

    if (!npcRequestKneel(obj)) {
        npc->unk2C = 0;
    }
}

void NPCStand(Gobj *obj) {
    NPC *npc = NPC_OF(obj);

    if (!npcRequestKneelRelease(obj)) {
        npc->unk2C = 0;
    }
}

void NPCSwitch(Gobj *obj) {
    s32 switchId = gameObjectsInfo[obj->id].refId;
    Gobj *sw = gameObjectsInfo[switchId].runtime;

    if (sw != NULL && !(sw->flags1 & 0x80)) {
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&NPC_OF(obj)->dir, &MOVE_ROT(obj->move));
        MOVE_ROT(obj->move).vy = ratan2(NPC_OF(obj)->dir.vx, NPC_OF(obj)->dir.vz);
        MOVE_ROT(obj->move).vz = 0;
        CASRequestAction(obj->id, 0x2F);
        SysPostMessage(0x14, 3, obj->id, switchId, 0, 0, 0, 0);
        if (gSysInfo.level == 13 && (NPC_OF(obj)->flags & 1)) {
            NPC_OF(obj)->flags &= ~1;
            NPC_OF(obj)->nameMsgId = 9;
        }
    }
}

void NPCOpenDoor(Gobj *obj) {
    Gobj *door = gameObjectsInfo[gameObjectsInfo[obj->id].refId].runtime;

    if (door != NULL && !(door->flags0 & 0x20)) {
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&NPC_OF(obj)->dir, &MOVE_ROT(obj->move));
        MOVE_ROT(obj->move).vy = ratan2(NPC_OF(obj)->dir.vx, NPC_OF(obj)->dir.vz);
        MOVE_ROT(obj->move).vz = 0;
        CASRequestAction(obj->id, 0x30);
    }
}

void NPCOpenDoorMessage(Gobj *obj) {
    s32 id = obj->id;
    s32 doorId = gameObjectsInfo[id].refId;
    Gobj *door = gameObjectsInfo[doorId].runtime;

    if (door != NULL && !(door->flags0 & 0x20)) {
        SysPostMessage(0x18, 3, id, doorId, 0, 0, 0, 0);
    }
}

void NPCHazmatDone(Gobj *obj) {
    CASRequestAction(obj->id, 0x3F);
    obj->flags23 = (obj->flags23 & ~1) | 8;
    SysPostMessage(0x24, 3, obj->id, gameObjectsInfo[obj->id].refId, 0, 0, 0, 0);
    NPC_OF(obj)->flags |= 0x1000000;
    SysPlayXA((rand() & 4) + 0x219 + (obj->id & 3), (s32)obj, 0);
}

s32 NPCGetGrenadeLink(Gobj *obj) {
    s32 found = -1;
    s32 i;
    s32 link;
    s32 dist;
    NPC *npc = NPC_OF(obj);

    for (i = 0; i < 3; i++) {
        link = ((s8 *)D_80110134[npc->curPoint].links)[i];
        if (link >= 0) {
            if ((((link + D_80110134)->flags & 0xF00) >> 8) == 1) {
                grenadePos.vx = (link + D_80110134)->x;
                grenadePos.vy = (link + D_80110134)->y;
                grenadePos.vz = (link + D_80110134)->z;
                EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011E8A8, &grenadePos, &dist);
                if (dist < 0x300) {
                    found = link;
                    break;
                }
            }
        }
    }
    return found;
}

void NPCThrowGrenade(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    s32 f;
    s32 r;
    s32 w;

    if (D_8011030C->ready && !(obj->cas->flags & 0x1000000)) {
        if (NPCGetGrenadeLink(obj) >= 0) {
            D_8011030C->ready = 0;
            npcGrenadeThrowerID = obj->id;
            npc->unk41 = 0;
            if (grenadePos.vy - D_8011E8B8.vy < 0x280) {
                grenadePos.vx = D_8011E8A8.vx + (rand() & 0x3F) - 0x1F;
                grenadePos.vz = D_8011E8A8.vz + (rand() & 0x3F) - 0x1F;
            } else {
                grenadePos.vy += 0x80;
            }
            D_8011030C->weapon = (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) != 0x2000 ? 19 : 20;
            NPCPathDeltaToAction(D_80110134, obj, &grenadePos, &npc->dir);
            CASRequestAction(obj->id, 0x3A);
            npc->unk30 = gExeTick;
            NPCSetLOSFirstSeen(obj, 1, 0);
            r = rand();
            npc->unk38 = D_80105F58[obj->id != gPlayerOffset ? NPC_WEAPON(obj->id, f) : gCurrentWeapon].fireRate + 20 + (r & 0x1F);
            npc->unk41 = 0;
        } else {
            if ((*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0 ? *(u8 *)&gameObjectsInfo[obj->id].flags24 == 19
                                                                : (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) == 0x1000)
                || NPC_WEAPON(obj->id, f) == 20) {
                *(u8 *)&gameObjectsInfo[obj->id].flags24 = grenadeWeaponRestore;
            }
            npc->unk2C = 0;
        }
    }
}

void NPCRollTowardPath(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    s32 link;
    VECTOR32 target;
    VECTOR32 cur;

    if (npc->unk2C == 0x7FFE) {
        link = getPointActionLink(D_80110134, obj);
        target.vx = D_80110134[link].x;
        target.vy = D_80110134[link].y;
        target.vz = D_80110134[link].z;
        NPCPathDeltaToAction(D_80110134, obj, &target, &npc->dir);
        cur.vx = D_80110134[npc->curPoint].x;
        cur.vy = D_80110134[npc->curPoint].y;
        cur.vz = D_80110134[npc->curPoint].z;
        NPC_MOVE(obj)->pathCtrl->active = 1;
        NPC_MOVE(obj)->pathCtrl->from = cur;
        NPC_MOVE(obj)->pathCtrl->to = target;
        npc->unk2C = 0x7FFF;
        npc->unk49 = link | 0x80;
        pointActionMoveMode = 5;
        npc->unk41 = 0;
        npc->flags |= 0x10000;
        return;
    }
    if (!(npc->flags & 0x10000)) {
        if (npc->unk49 & 0x80) {
            CASRequestAction(obj->id, 0x2E);
            npc->unk49 &= 0x7F;
        } else {
            npc->unk2C = 0;
        }
    }
}

void NPCRollAwayFromPath(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 target;
    VECTOR32 from;

    if (npc->unk2C == 0x7FFE) {
        target.vx = D_80110134[npc->curPoint].x;
        target.vy = D_80110134[npc->curPoint].y;
        target.vz = D_80110134[npc->curPoint].z;
        NPCPathDeltaToAction(D_80110134, obj, &target, &npc->dir);
        from.vx = D_80110134[npc->unk49].x;
        from.vy = D_80110134[npc->unk49].y;
        from.vz = D_80110134[npc->unk49].z;
        NPC_MOVE(obj)->pathCtrl->active = 1;
        NPC_MOVE(obj)->pathCtrl->from = from;
        NPC_MOVE(obj)->pathCtrl->to = target;
        npc->unk2C = 0x7FFF;
        pointActionMoveMode = 5;
        npc->unk41 = 0;
        npc->flags |= 0x10000;
        return;
    }
    if (!(npc->flags & 0x10000)) {
        pointActionMoveMode = 5;
        if (!(obj->cas->flags & 0x400)) {
            if (npc->unk49 != npc->curPoint) {
                npc->unk49 = npc->curPoint;
                CASRequestAction(obj->id, 0x2E);
            } else {
                npc->unk2C = 0;
            }
        }
    }
}

void NPCNavigateActionPath(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 target;
    VECTOR32 pos;
    VECTOR32 d;
    VECTOR32 off;

    target.vx = D_80110134[npc->unk49].x;
    target.vy = D_80110134[npc->unk49].y;
    target.vz = D_80110134[npc->unk49].z;
    off.vx = FIX_TO_INT(NPC_MOVE(obj)->unk50.vx);
    off.vy = FIX_TO_INT(NPC_MOVE(obj)->unk50.vy);
    off.vz = FIX_TO_INT(NPC_MOVE(obj)->unk50.vz);
    pos.vx = NPC_MOVE(obj)->unk40.vx + off.vx;
    pos.vy = NPC_MOVE(obj)->unk40.vy + off.vy;
    pos.vz = NPC_MOVE(obj)->unk40.vz + off.vz;
    d.vx = target.vx - pos.vx;
    d.vy = 0;
    d.vz = target.vz - pos.vz;
    GTEMath_VectorNormal(&d, &d);
    if (npc->dir.vx * d.vx + npc->dir.vz * d.vz <= 0) {
        npc->unk2C = 0;
    }
}

void NPCWalkToPoint(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 pt;

    if (npc->unk2C == 0x7FFE) {
        pt.vx = D_80110134[npc->curPoint].x;
        pt.vy = D_80110134[npc->curPoint].y;
        pt.vz = D_80110134[npc->curPoint].z;
        NPCPathDeltaToAction(D_80110134, obj, &pt, &npc->dir);
        npc->unk2C = 0x7FFF;
        pointActionMoveMode = 5;
        npc->flags |= 0x10000;
    } else if (!(npc->flags & 0x10000)) {
        npc->unk46 = 6;
        NPCNavigateActionPath(obj);
    }
}

void NPCStrafeTowardPath(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    s32 link;
    VECTOR32 target;
    VECTOR32 cur;

    if (npc->unk2C == 0x7FFE) {
        link = getPointActionLink(D_80110134, obj);
        target.vx = D_80110134[link].x;
        target.vy = D_80110134[link].y;
        target.vz = D_80110134[link].z;
        NPCPathDeltaToAction(D_80110134, obj, &target, &npc->dir);
        cur.vx = D_80110134[npc->curPoint].x;
        cur.vy = D_80110134[npc->curPoint].y;
        cur.vz = D_80110134[npc->curPoint].z;
        NPC_MOVE(obj)->pathCtrl->active = 1;
        NPC_MOVE(obj)->pathCtrl->from = cur;
        NPC_MOVE(obj)->pathCtrl->to = target;
        npc->unk2C = 0x7FFF;
        npc->unk49 = link;
        pointActionMoveMode = 5;
        npc->flags |= 0x10000;
    } else if (!(npc->flags & 0x10000)) {
        pointActionMoveMode = 7;
        NPCNavigateActionPath(obj);
    }
    D_80110138 = obj->id;
    npc->unk41 = 0x7F;
}

void NPCStrafeAwayFromPath(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 target;
    VECTOR32 from;

    if (npc->unk2C == 0x7FFE) {
        target.vx = D_80110134[npc->curPoint].x;
        target.vy = D_80110134[npc->curPoint].y;
        target.vz = D_80110134[npc->curPoint].z;
        NPCPathDeltaToAction(D_80110134, obj, &target, &npc->dir);
        from.vx = D_80110134[npc->unk49].x;
        from.vy = D_80110134[npc->unk49].y;
        from.vz = D_80110134[npc->unk49].z;
        NPC_MOVE(obj)->pathCtrl->active = 1;
        NPC_MOVE(obj)->pathCtrl->from = from;
        NPC_MOVE(obj)->pathCtrl->to = target;
        npc->unk2C = 0x7FFF;
        npc->unk49 = npc->curPoint;
        npc->flags |= 0x10000;
        pointActionMoveMode = 7;
        npc->unk41 = 0x7F;
        D_80110138 = obj->id;
        return;
    }
    if (!(npc->flags & 0x10000)) {
        npc->unk41 = 0x7F;
        pointActionMoveMode = 7;
        D_80110138 = obj->id;
        NPCNavigateActionPath(obj);
        if (npc->unk2C <= 0) {
            npc->unk41 = 0;
            D_8011013C = -1;
            npc->flags &= ~0x100000;
            npc->dir.vx = -npc->dir.vx;
            npc->dir.vz = -npc->dir.vz;
            npc->flags |= 0x10000;
        }
    }
}

void doPointAction(Gobj *obj, s32 kind) {
    if (D_80106474[kind].casAction != 0) {
        CASRequestAction(obj->id, D_80106474[kind].casAction);
    }
    if (D_80106474[kind].func != NULL) {
        D_80106474[kind].func(obj);
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/npc", D_80012070);

#ifdef NON_MATCHING
/* grenade weapon restore: target reloads obj->id for the second gameObjectsInfo address */
void initPointAction(PathPoint *path, Gobj *obj, s32 kind) {
    NPC *npc = NPC_OF(obj);
    s32 flags = npc->flags;
    s32 action;
    s32 ref;
    Gobj *bomb;
    s32 f;

    npc->flags = flags | 0x800;
    action = path[npc->curPoint].flags & 0xF;
    if ((action == 0 && (action = kind) == 0) || gameObjectsInfo[gPlayerOffset].runtime->healthInfo->health <= 0) {
        goto none;
    }
    npc->flags = flags | 0xC00;
    if (npc->awareness == 0 || action == kind) {
        if (action != 4) {
            if (action != 6) {
                goto none;
            }
            npc->script = look;
        } else {
            if (!(npc->flags & 8)) {
                goto flip;
            }
            ref = gameObjectsInfo[obj->id].refId;
            bomb = gameObjectsInfo[ref].runtime;
            if (bomb == NULL || (bomb->flags1 & 0x80)) {
                goto skip;
            }
            npc->script = disarmBomb;
            SysPostMessage(0x14, 3, obj->id, ref, 0, 0, 0, 0);
            npc->flags &= ~8;
            SysPlayXA((obj->id & 3) + 0x232, (s32)obj, 0);
        }
    } else {
        switch (action) {
        case 1:
            npc->script = kneelStandShoot;
            break;
        case 2:
            if (npc->awareness == 0 || (npc->flags & 0x100)) {
                goto skip;
            }
            npc->script = kneelRollShootRoll;
            npc->unk49 = npc->curPoint;
            break;
        case 7:
            if (npc->awareness == 0 || (npc->flags & 0x100) || D_8011013C >= 0) {
                goto skip;
            }
            npc->script = strafeOutStrafeBack;
            npc->unk49 = npc->curPoint;
            npcRequestKneelRelease(obj);
            D_80110138 = obj->id;
            D_8011013C = obj->id;
            break;
        case 3:
            if (npc->awareness != 2 || !D_8011030C->ready || NPCGetGrenadeLink(obj) < 0) {
                goto skip;
            }
            npcGrenadeThrowerID = obj->id;
            npc->unk41 = 0;
            if (!(*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0 ? *(u8 *)&gameObjectsInfo[obj->id].flags24 == 19
                                                                 : (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) == 0x1000)
                && NPC_WEAPON(obj->id, f) != 20) {
                grenadeWeaponRestore = *(u8 *)&gameObjectsInfo[obj->id].flags24;
                *(u8 *)&gameObjectsInfo[obj->id].flags24
                    = (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x3000) != 0x2000 ? 19 : 20;
            }
            npc->script = grenade;
            break;
        skip:
            npc->flags &= ~0x400;
            break;
        case 4:
            if (gSysInfo.level == 13 && GOBJ_TYPE(obj->id) == 0x65) {
                npc->flags &= ~0x400;
                NPC_OF(obj)->flags |= 0x1000000;
                break;
            }
        flip:
            npc->script = flipSwitch;
            break;
        case 5:
            npc->script = openDoor;
            break;
        default:
        none:
            npc->flags &= ~0x400;
            return;
        }
    }
    if (npc->flags & 0x400) {
        npc->flags &= ~0x800;
        npc->unk42 = 0;
        npc->unk2C = npc->script->duration;
        npc->unk46 = npc->script->moveMode;
        if (npc->script->shoot) {
            npc->flags |= 0x80000;
        } else {
            npc->flags &= ~0x80000;
        }
        doPointAction(obj, npc->script->kind);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", initPointAction);
#endif

void NPCRandomActions(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    u8 count;
    s32 flags;
    s32 r;

    if (GOBJ_TYPE(obj->id) != 0x5C && EGSDisplayHMDInActiveSector(obj->anim) && (obj->target->flags & 8)) {
        count = npc->unk4A + 1;
        npc->unk4A = count;
        if (*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
            && ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000 && (flags & 0x3000) != 0x2000
            && npc->nameMsgId < 2) {
            if (count > 20 && !(NPC_OF(obj)->flags & 0x1000000)) {
                NPC_OF(obj)->flags |= 0x1000000;
                CASRequestAction(obj->id, 0x55);
            }
        } else if (npc->unk4A > 60 && !(NPC_OF(obj)->flags & 0x1000000) && !(npc->flags & 0x400) && npc->unk41 == 0) {
            r = rand() & 7;
            if (r < 4) {
                if (obj->cas->active == 2) {
                    npcRequestKneelRelease(obj);
                } else {
                    npcRequestKneel(obj);
                }
            }
            if (r < 6) {
                NPCPlayRandXA(obj, 5);
                npc->unk4A = 0;
                return;
            }
            if ((*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
                 || ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000 || (flags & 0x3000) == 0x2000)
                && (obj->target->flags & 8) && (npc->flags & 0x20001) == 0x20000) {
                npc->unk41 = !((D_80105F58[obj->id].flags >> 8) & 7) ? 1 : 3;
            }
            npc->unk4A = 0;
        }
    }
}

void NPCIQ(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    u8 timer = npc->xaTimer;
    s32 flags = npc->flags;
    s32 seen;
    s32 f;
    s32 flags2;

    if (timer != 0) {
        npc->xaTimer = timer - 1;
    }
    if (!(flags & 0x400)) {
        NPCGetMoveMode(obj);
    }
    if (GOBJ_TYPE(obj->id) == 2) {
        NPCSetAwarenessLevel(obj, 2, 0, 0);
        D_80110138 = obj->id;
    }
    if ((flags & 0x20) && NPC_OF(obj)->unk4E > 80 && !EGSDisplayHMDInActiveSector(obj->anim)
        && obj->id != gObjPlayer->target->targetId) {
        NPCDeactivateGenerated(obj);
    } else {
        seen = flags & 0x100;
        if (npcPCInvisibleFlag) {
            seen = 0;
            npc->unk34 = 0;
            npc->flags &= ~0x100;
        }
        if (npc->unk34 > 0) {
            if (npc->awareness == 0) {
                NPC_ALERT(obj, npc);
            } else {
                if (npc->awareness == 1) {
                    NPCSetAwarenessLevel(obj, 2, 0, 0);
                } else {
                    NPCRecordPlayerSighting(obj);
                }
                if (gSysInfo.level == 18 && obj->id == gLevelSpecificNpcID) {
                    obj->flags23 = (obj->flags23 & ~1) | 8;
                    npc->nameMsgId = 8;
                    NPC_OF(obj)->flags &= ~0x1000000;
                    npc->unk45 = 0;
                }
            }
        }
        if (npc->awareness == 0 && seen) {
            NPC_ALERT(obj, npc);
        }
        if (npc->awareness == 1) {
            if (seen) {
                NPC_ALERT(obj, npc);
            } else {
                if (--npc->unk32 <= 0 && !(flags & 0x400)) {
                    NPCRecordPlayerSighting(obj);
                    if (flags & 0x40000) {
                        NPCSetAwarenessLevel(obj, 2, 1, 0);
                        npc->flags &= ~0x40000;
                    }
                }
                if (obj->cas->flags & 0x100) {
                    if (obj->id != gPlayerOffset) {
                        if (*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
                            && ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000
                            && (f & 0x3000) != 0x2000) {
                            goto skip;
                        }
                    } else if (gCurrentWeapon == 0) {
                        goto skip;
                    }
                    if (obj->cas != NULL) {
                        CASRequestAction(obj->id, 0x2D);
                    }
                }
            skip:;
            }
        }
        if (npc->awareness == 2) {
            if (!seen && (obj->target->flags & 8)) {
                if (--npc->unk32 <= 0 && !(flags & 0x400)) {
                    NPCSetAwarenessLevel(obj, 1, 0, 0);
                }
            }
            if (!(obj->cas->flags & 0x100)
                && (*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
                    || ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000 || (f & 0x3000) == 0x2000)
                && obj->cas != NULL) {
                CASRequestAction(obj->id, 0x2B);
            }
            if (!(flags & 0x80000)) {
                doShootingLogic(obj, obj->target->unk58);
            }
        }
        npc->unk34 = 0;
    }
    flags2 = npc->flags;
    npc->flags = flags2 & ~0x800000;
    if (npc->awareness != 0 && npc->unk4B != 1 && !(flags2 & 0x400) && GOBJ_TYPE(obj->id) != 2
        && (*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
            || ((f = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000 || (f & 0x3000) == 0x2000)) {
        if (npc->unk28 >= 0x60 && (npc->unk28 >= 0x140 || obj->target->targetId != gPlayerOffset || !(flags & 0x100))) {
            if (npc->awareness == 2 && !(flags & 0x800) && !npcGrenadeLandedCheck && (flags & 0x100)
                && npc->nameMsgId > 50
                && (obj->target->unk58 >= 80 || !(npc->flags & 0x20000) || npc->unk28 < (100 - npc->nameMsgId) << 5)
                && gameObjectsInfo[gPlayerOffset].runtime->healthInfo->health > 0) {
                if (!(npc->flags & 0x800000) && (obj->cas->flags & 8) && NPC_OF(obj)->casAction == 0) {
                    CASRequestAction(obj->id, 0x4B);
                    NPC_OF(obj)->casAction = 0x4B;
                }
                npc->flags |= 0x800000;
            }
        } else {
            npc->flags |= 0x800000;
        }
    }
}

s32 NPCFaceDirection(Gobj *obj, VECTOR32 *dir) {
    NPC *npc = NPC_OF(obj);
    VECTOR32 fwd;
    s32 ret = 1;
    s32 angle;
    s32 diff;

    fwd.vx = obj->anim->frame->m[0][2];
    fwd.vy = obj->anim->frame->m[1][2];
    fwd.vz = obj->anim->frame->m[2][2];
    fwd.vy = -fwd.vy;
    angle = ratan2(fwd.vz, fwd.vx);
    diff = ratan2(dir->vz, dir->vx) - angle;
    if (diff > 0x800) {
        diff -= 0x1000;
    }
    if (diff < -0x800) {
        diff += 0x1000;
    }
    if (ABS(diff) < 0x333) {
        npc->flags &= ~0x10000;
        EGSGeometryFIX_VectorGetAngleFromXZPlane(dir, &MOVE_ROT(obj->move));
        MOVE_ROT(obj->move).vy = ratan2(dir->vx, dir->vz);
        MOVE_ROT(obj->move).vz = 0;
        MovementGobjRegisterCharacterMovement(obj, 0, 5);
    } else {
        if (diff > 0) {
            angle += 0x333;
        } else {
            angle -= 0x333;
        }
        D_8011D200.vx = rcos(angle);
        D_8011D200.vy = 0;
        D_8011D200.vz = rsin(angle);
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&D_8011D200, &MOVE_ROT(obj->move));
        MOVE_ROT(obj->move).vy = ratan2(D_8011D200.vx, D_8011D200.vz);
        MOVE_ROT(obj->move).vz = 0;
        MovementGobjRegisterCharacterMovement(obj, 0, 5);
        if (!(npc->flags & 0x10000)) {
            npc->dir = *dir;
            npc->flags |= 0x10000;
        }
        ret = 0;
    }
    return ret;
}

#ifdef NON_MATCHING
/* draft: right shape (912 vs 906 insns); script-step loads and flag-update scheduling differ */
void NPCIQEveryFrame(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    u8 mode = npc->unk46;
    VECTOR32 pt;
    VECTOR32 pos;
    VECTOR32 off;
    u8 step;
    s16 kind;
    s32 flags;
    u32 k;
    VECTOR32 *dir;
    Gobj *t;

    if (npc->awareness != 0 && !((gExeTick + obj->id) & 0x1F)) {
        NPCSound(5, &obj->move->pos, 0x3C0, obj->id);
    }
    if (npc->flags & 0x400) {
        step = npc->unk42;
        kind = npc->script[step].kind;
        pointActionMoveMode = npc->script[step].moveMode;
        if (npc->unk2C == 0x7FFF) {
            doPointAction(obj, npc->script[step].kind);
        } else {
            npc->unk2C--;
        }
        if (npc->unk2C <= 0) {
            step++;
            if (kind == 0) {
                npc->unk2C = 0;
                npc->unk42 = 0;
                npc->flags &= ~0x400;
                npc->flags = ((npc->flags | 0x800) & ~0x80000) & ~0x2000;
            } else {
                npc->unk2C = npc->script[step].duration;
                if (npc->script[step].unk04 != 0) {
                    npc->unk2C += npc->script[step].unk04 & rand();
                }
                if (npc->script[step].shoot != 0) {
                    npc->flags |= 0x80000;
                } else {
                    npc->flags &= ~0x80000;
                }
                if (npc->flags & 0x80000) {
                    npc->unk41 = 0;
                }
                doPointAction(obj, npc->script[step].kind);
                npc->unk42 = step;
            }
        }
        D_8011D210 = npc->dir;
        mode = pointActionMoveMode;
    }
    if (gSysInfo.level == 18 && obj->id == gLevelSpecificNpcID2) {
        if (D_8011E8A8.vy < -900 || npc->awareness == 0) {
            npc->lastSeenPos.vx = 0x1ED4;
            npc->lastSeenPos.vy = -0x3F7;
            npc->lastSeenPos.vz = -0x2BE7;
            obj->target->targetId = -1;
            TARGET_AIM(obj->target)->vx = npc->lastSeenPos.vx - 0x10 + ((rand() & 0x1F) << 5);
            TARGET_AIM(obj->target)->vy = npc->lastSeenPos.vy;
            TARGET_AIM(obj->target)->vz = npc->lastSeenPos.vz - 0x10 + ((rand() & 0x1F) << 5);
            obj->target->unk58 = 200;
            if (--npc->unk2C <= 0) {
                npc->unk38 = 0;
                NPCTakeShot(obj, 0x140);
                npc->unk38 = 0xA0;
                npc->unk2C = 10;
            }
            npc->unk41 = 0;
            obj->target->targetId = gPlayerOffset;
            if (npc->awareness != 0) {
                NPC_OF(obj)->flags |= 0x1000000;
                npc->awareness = 0;
            }
            obj->target->threat = 0x1000;
        } else if (npc->unk41 == 0) {
            if (npc->unk2C <= 0) {
                npc->unk38 = 0;
                obj->target->threat = 0x1000;
                npc->unk41 = (rand() & 1) + 3 + (rand() & 1);
                npc->unk2C = 60;
            } else {
                npc->unk2C--;
                npc->unk38 = 100;
                obj->target->threat = npc->unk2C < 16 ? 0x1000 : 0xE65;
            }
        }
        D_80110138 = obj->id;
    }
    flags = npc->flags;
    if (!(flags & 0x400)) {
        if (D_80110134 == NULL) {
            mode = 5;
        } else {
            if ((flags & 0x800000) || (NPC_OF(obj)->flags & 0x1000000) || (flags & 0x10000)) {
                mode = 5;
            } else if (!(flags & 0x800)) {
                pt.vx = D_80110134[npc->curPoint].x;
                pt.vy = D_80110134[npc->curPoint].y;
                pt.vz = D_80110134[npc->curPoint].z;
                off.vx = FIX_TO_INT(NPC_MOVE(obj)->unk50.vx);
                off.vy = FIX_TO_INT(NPC_MOVE(obj)->unk50.vy);
                off.vz = FIX_TO_INT(NPC_MOVE(obj)->unk50.vz);
                pos.vx = NPC_MOVE(obj)->unk40.vx + off.vx;
                pos.vy = NPC_MOVE(obj)->unk40.vy + off.vy;
                D_8011D210.vx = pt.vx - pos.vx;
                D_8011D210.vy = 0;
                pos.vz = NPC_MOVE(obj)->unk40.vz + off.vz;
                D_8011D210.vz = pt.vz - pos.vz;
                if (npc->dir.vx * D_8011D210.vx + npc->dir.vz * D_8011D210.vz <= 0) {
                    npc->flags &= ~0x4000000;
                    if (!(npc->flags & 0x40000) && npc->awareness != 1) {
                        initPointAction(D_80110134, obj, 0);
                    } else {
                        initPointAction(D_80110134, obj, 6);
                    }
                } else {
                    if (flags & 0x4000000) {
                        NPCRethinkNextPoint(obj);
                        npc->flags &= ~0x4000000;
                    }
                    k = (D_80110134[npc->curPoint].flags & 0xF00) >> 8;
                    if (k == (D_80110134[npc->nextPoint].flags & 0xF00) >> 8) {
                        if (npc->awareness == 0) {
                            if (k == 3) {
                                mode = 7;
                            }
                        } else if (k == 2) {
                            npcRequestKneelRelease(obj);
                            mode = 6;
                        }
                    }
                }
            }
            if (mode != 5 && (npc->flags & 0x800)) {
                if (!(npc->unk45 & 0x1F)) {
                    pt.vx = D_80110134[npc->curPoint].x;
                    pt.vy = D_80110134[npc->curPoint].y;
                    pt.vz = D_80110134[npc->curPoint].z;
                    npc->unk4B = (u8)D_80110134[npc->curPoint].flags >> 4;
                    if (NPCPickNextPoint(obj, D_80110134, npc->curPoint)) {
                        pos.vx = D_80110134[npc->curPoint].x;
                        pos.vy = D_80110134[npc->curPoint].y;
                        pos.vz = D_80110134[npc->curPoint].z;
                        NPC_MOVE(obj)->pathCtrl->active = 1;
                        NPC_MOVE(obj)->pathCtrl->from = pt;
                        NPC_MOVE(obj)->pathCtrl->to = pos;
                        npc->dir.vy = 0;
                        npc->dir.vx = pos.vx - D_8011E8B8.vx;
                        npc->dir.vz = pos.vz - D_8011E8B8.vz;
                        npc->unk2E = pt.vy;
                        obj->move->unk12C = pt.vy;
                        GTEMath_VectorNormal(&npc->dir, &npc->dir);
                        D_8011D210 = npc->dir;
                        npc->flags &= ~0x800;
                        if (npc->awareness != 0) {
                            k = (D_80110134[npc->curPoint].flags & 0xF00) >> 8;
                            if (k == (D_80110134[npc->nextPoint].flags & 0xF00) >> 8) {
                                npc->unk41 = 0;
                                switch (k) {
                                case 4:
                                    npcRequestKneel(obj);
                                    break;
                                case 5:
                                    EGSGeometryFIX_VectorGetAngleFromXZPlane(&npc->dir, &MOVE_ROT(obj->move));
                                    mode = 7;
                                    MOVE_ROT(obj->move).vy = ratan2(npc->dir.vx, npc->dir.vz);
                                    MOVE_ROT(obj->move).vz = 0;
                                    CASRequestAction(obj->id, 0x2E);
                                    break;
                                }
                            }
                        }
                    } else {
                        initPointAction(D_80110134, obj, 0);
                        mode = 5;
                        npc->unk45++;
                    }
                } else {
                    mode = 5;
                    npc->unk45++;
                }
            }
        }
    }
    if (npc->flags & 0x10000) {
        NPCFaceDirection(obj, &npc->dir);
    } else if (mode == 5) {
        dir = NULL;
        if (npc->awareness == 2 && !(npc->flags & 0x2000) && !(obj->cas->flags & 0x20000000)
            && (obj->target->flags & 8)) {
            if (obj->target->targetId < 0) {
                if (gSysInfo.level == 0) {
                    D_8011D210.vx = 0xDE7 - D_8011E8B8.vx;
                    D_8011D210.vz = 0x1BE7 - D_8011E8B8.vz;
                } else {
                    D_8011D210.vx = 0;
                    D_8011D210.vz = 0;
                }
                D_8011D210.vy = 0;
            } else {
                if (obj->target->targetId != gPlayerOffset) {
                    t = gameObjectsInfo[obj->target->targetId].runtime;
                    D_8011D210.vx = t->move->pos.vx - D_8011E8B8.vx;
                    D_8011D210.vy = t->move->pos.vy - D_8011E8B8.vy;
                    D_8011D210.vz = t->move->pos.vz - D_8011E8B8.vz;
                } else {
                    D_8011D210.vx = npc->lastSeenPos.vx - D_8011E8B8.vx;
                    D_8011D210.vy = npc->lastSeenPos.vy - D_8011E8B8.vy;
                    D_8011D210.vz = npc->lastSeenPos.vz - D_8011E8B8.vz;
                }
                D_8011D210.vy = 0;
                GTEMath_VectorNormal(&D_8011D210, &D_8011D210);
            }
            dir = &D_8011D210;
        }
        MovementGobjRegisterCharacterMovement(obj, dir, 5);
        if (npc->awareness == 2 && !(npc->flags & 0x400) && npc->casAction == 0) {
            NPCRandomActions(obj);
        }
    } else {
        npc->unk4A = 0;
        pt.vx = obj->anim->frame->m[0][2];
        pt.vy = obj->anim->frame->m[1][2];
        pt.vz = obj->anim->frame->m[2][2];
        pt.vy = -pt.vy;
        if (mode == 7 && (obj->cas->flags & 2) && !(npc->flags & 0x400) && NPC_OF(obj)->casAction == 0) {
            CASRequestAction(obj->id, 0x47);
            npc->casAction = 0x47;
        }
        D_8011D210.vy = 0;
        GTEMath_VectorNormal(&D_8011D210, &D_8011D210);
        MovementGobjRegisterCharacterMovement(obj, &D_8011D210, mode);
        if (!(obj->target->flags & 8) && obj->target->targetId == gPlayerOffset && npc->unk28 < 0x3C0) {
            NPCSetLOSFirstSeen(obj, 1, 0);
        }
    }
    CASRequestAction(obj->id, 4);
    if (particleBreathFlag && !((gExeTick + obj->id * 4) & 0x1F)) {
        ParticleBreath(obj->id);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCIQEveryFrame);
#endif

void NPCGenDispatch(SysMsg *msg) {
    s32 id = msg->to;
    Gobj *obj;
    NPC *npc;

    if (D_80110148 < 0) {
        D_80110148 = id;
    }
    if (generatorInfo != NULL) {
        gameObjectsInfo[id].path = 0;
        gameObjectsInfo[id].modelIndex = *generatorInfo->models;
        func_801463A4(id, iAnimationArray);
        obj = gameObjectsInfo[id].runtime;
        if (obj->anim == NULL) {
            EGSDebugAssert(1, 0, D_8010F964, 4000);
        }
        npc = EGSMemMalloc(sizeof(NPC));
        if (npc == NULL) {
            EGSDebugAssert(1, 0, D_8010F964, 4002);
        }
        obj->unk20[1] = 0;
        obj->npc = (struct NPCInfo *)npc;
        obj->unk20[0] |= 0x40;
        PlayerCASInit(obj, 0);
        npc->flags = 0x20;
    }
    D_80110168 = 0;
}

s32 NPCFindGobjIDForGenerator(void) {
    s32 i;
    Gobj *obj;

    for (i = 0; i < 5; i++) {
        D_8010F96C++;
        if (D_8010F96C >= 5) {
            D_8010F96C = 0;
        }
        obj = gameObjectsInfo[D_80110148 + D_8010F96C].runtime;
        if (!(NPC_OF(obj)->flags & 0x200)) {
            return obj->id;
        }
    }
    return -1;
}

s32 NPCFindGeneratorPath(s32 idx) {
    NPCGenerator *gen = &generatorInfo[idx];
    s32 n = gen->numPaths;
    s32 start = rand() % n;
    s32 i;
    s32 j;
    s32 p;
    s32 path;

    for (i = 0; i < n; i++) {
        p = i + start;
        if (p >= n) {
            p -= n;
        }
        path = gen->paths[p];
        for (j = 0; j < 5; j++) {
            if (gameObjectsInfo[D_80110148 + j].runtime->healthInfo->health > 0
                && gameObjectsInfo[D_80110148 + j].path == path) {
                p = -1;
            }
        }
        if (p >= 0) {
            return p;
        }
    }
    return -1;
}

void NPCGetNumActive(void) {
    s32 i;
    s32 id;
    Gobj *obj;

    do {
        if ((u32)(gExeTick - NPCNumActiveTick) >= 2) {
            NPCNumActive = 0;
            NPCNumEnemyAlive = 0;
            getNPCIndex = 0;
            i = 0;
            NPC_SCAN_IDS(id, i)
            while (id >= 0) {
                obj = gameObjectsInfo[id].runtime;
                NPCNumActive++;
                if (obj->healthInfo->health > 0 && obj->target->targetId == gPlayerOffset) {
                    NPCNumEnemyAlive++;
                }
                NPC_NEXT_ID(id, i);
            }
            NPCNumActiveTick = gExeTick;
        }
    } while (0);
}

#ifdef NON_MATCHING
/* prologue scheduling and the kind/flags register pair differ */
s32 NPCCreateGenerator(NPCGenerator *gen, s32 id, s32 path, u32 variant) {
    GameObjectInfo *info = &gameObjectsInfo[id];
    s32 *paths = &gen->paths[path];
    Gobj *obj = info->runtime;
    u8 model = gen->models[variant];
    s32 elite;
    s32 kind;
    s32 flags;
    GameObjectHealth *health;
    u16 hp;
    void *sniper;

    info->pathStart = 99;
    info->path = *paths;
    info->modelIndex = model;
    elite = 0;
    if (gSysInfo.level == 14) {
        kind = !(rand() & 1) ? 6 : 8;
    } else {
        kind = 9;
    }
    if ((gen->unk02 & 0x1F) == 1) {
        if (gSysInfo.level == 14 && D_80110158 >= 3) {
            return 0;
        }
        if (gSysInfo.level == 0 && D_80110158 >= 2) {
            return 0;
        }
        gameObjectsInfo[id].pathStart = 0;
        flags = (gen->unk02 & 0xFFE0) | kind;
    } else if ((gen->unk02 & 0x1F) == 15) {
        if (gSysInfo.level == 14 && D_8011015C >= 3) {
            return 0;
        }
        if (gSysInfo.level == 0 && D_8011015C >= 3) {
            return 0;
        }
        gameObjectsInfo[id].pathStart = 1;
        if (gSysInfo.level == 3) {
            kind = 4;
        } else if (gSysInfo.level == 0) {
            kind = 2;
        }
        flags = (gen->unk02 & 0xFFE0) | kind;
    } else {
        flags = gen->unk02;
        if ((gen->unk02 & 0x1F) == 3) {
            elite = 1;
            flags = gen->unk02 & 0xFFE0;
        }
    }
    GOBJ_INFO_FLAGS(&gameObjectsInfo[id]) = flags & 0x7FFF;
    EGSAnimHMDLinkModel(obj->anim, *(s32 *)gameObjectsModelInfo[model].unk08);
    health = obj->healthInfo;
    hp = D_80106438[gSysInfo.level];
    health->armor = hp;
    health->health = hp;
    NPCGobjInit(obj);
    if (elite) {
        health = obj->healthInfo;
        health->armor = 500;
        health->health = 500;
        NPC_OF(obj)->flags |= 2;
    }
    NPC_OF(obj)->flags |= 0x20;
    sniper = NULL;
    if (gSniperMode && (gCurrentWeapon == 12 || gCurrentWeapon == 13 || gCurrentWeapon == 18)) {
        sniper = *(void **)gSniperCam;
    }
    obj->flags23 &= ~2;
    GSActivateGameObject(obj, *gCamera, (s32)sniper);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCreateGenerator);
#endif

#ifdef NON_MATCHING
/* create-failure 'return 0' gets merged into the epilogue; target keeps a separate j+li block */
s32 NPCGenerate(s32 genIdx) {
    s32 ret = 0;
    s32 dist;
    s32 force = (u32)gExeTick < 100;
    NPCGenerator *gen = &generatorInfo[genIdx];
    s32 id;
    Gobj *obj;
    s32 path;
    PathPoint *pt;
    s32 r;
    s32 slot;

    if (NPCNumActive < gLevData->maxActiveNPCs) {
        id = NPCFindGobjIDForGenerator();
        if (id >= 0) {
            obj = gameObjectsInfo[id].runtime;
            path = NPCFindGeneratorPath(genIdx);
            if (path >= 0) {
                pt = (PathPoint *)gen->paths[path];
                if (gSysInfo.level == 9) {
                    force = 1;
                    pt->y = D_801457A4;
                }
                D_8011D220.vx = pt->x;
                D_8011D220.vy = pt->y;
                D_8011D220.vz = pt->z;
                EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011D220, &D_8011E8A8, &dist);
                if (force || (u32)(dist - 0x3C1) < 0x1B7F) {
                    EGSDisplayTransformWorldPointToScreen(&D_8011D220, *gCamera, &D_8011D230);
                    if (force || dist > 0x640 || D_8011D230.vz <= 0 || (u32)(D_8011D230.vx + 200) >= 0x249) {
                        r = rand() % gen->unk05;
                        if (!NPCCreateGenerator(gen, id, path, r)) {
                            return 0;
                        }
                        ret = 1;
                        NPC_OF(obj)->unk28 = dist;
                        NPCNumActiveTick = gExeTick;
                        D_80110144 = gExeTick;
                        NPCNumActive++;
                        D_80110140 = obj->id;
                        slot = id - D_80110148;
                        genData[slot].gen = genIdx;
                        genData[slot].path = path;
                        genData[slot].variant = r;
                    }
                }
            }
        }
    } else {
        NPCRemoveDead();
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGenerate);
#endif

#ifdef NON_MATCHING
/* sector-count loop keeps the inline's NULL return path (target threads it away) */
s32 NPCCheckGenerators(void) {
    s32 tried;
    s32 n;
    u8 *adj;
    s32 i;
    s32 end;
    s32 step;
    void *sector;
    s32 sectorId;
    NPCGenActivation *act;
    s32 j;
    u8 g;
    s32 bit;
    NPCGenBits *gen;

    if (D_80110140 >= 0 && (u32)(gExeTick - D_80110144) < 21) {
        return 0;
    }
    tried = 0;
    i = 0;
    adj = gLevData->sectorAdj[gGabeSector];
    while (TerrGetSector(adj[i]) != NULL) {
        i++;
    }
    if (!(D_80110118 & 1)) {
        n = -1;
        end = i;
        step = 1;
    } else {
        n = i - 1;
        end = -2;
        step = -1;
    }
    i = n;
    for (; i != end; i += step) {
        if (i == -1) {
            sector = TerrGetSector(gGabeSector);
            sectorId = gGabeSector;
        } else {
            sector = TerrGetSector(adj[i]);
            sectorId = adj[i];
        }
        if (sector == NULL) {
            break;
        }
        act = &generatorActivationList[sectorId];
        for (j = 0; j < act->count; j++) {
            if (!(D_80110118 & 1)) {
                g = act->gens[j];
            } else {
                g = act->gens[act->count - j - 1];
            }
            bit = 1 << (g & 0x1F);
            if (!(tried & bit)) {
                gen = (NPCGenBits *)&generatorInfo[g];
                tried |= bit;
                if (gen->perm) {
                    if (gen->enabled && NPCGenerate(g)) {
                        gen->count--;
                        if (gen->count == 0) {
                            if (gen->perm) {
                                NPCDisableGenerator(g);
                            } else {
                                gen->enabled = 0;
                            }
                        }
                        D_80110118++;
                        return 1;
                    }
                } else if (gNPCLevelAlertFlag && gen->count && (u32)gExeTick > 100 && NPCGenerate(g)) {
                    gen->count--;
                    if (gen->count == 0) {
                        if (gen->perm) {
                            NPCDisableGenerator(g);
                        } else {
                            gen->enabled = 0;
                        }
                    }
                    D_80110118++;
                    return 1;
                }
            }
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCheckGenerators);
#endif

void NPCDeactivateGenerated(Gobj *obj) {
    void *disp;
    void *sniperDisp = NULL;

    CameraGetDisplay(gCamera, &disp);
    if (gSniperMode && (gCurrentWeapon == 12 || gCurrentWeapon == 13 || gCurrentWeapon == 18)) {
        CameraGetDisplay(&gSniperCam, &sniperDisp);
    }
    GSDeactivateGameObject(obj, disp, sniperDisp);
    gameObjectsInfo[obj->id].path = 0;
    NPC_OF(obj)->flags &= ~0x20;
}

void NPCInitTerrainGenerator(s32 id) {
    NPCGenerator *gen = &generatorInfo[id];

    if (!(gen->flags & 0x7FF)) {
        gen->flags |= 0x7FF;
    }
    gen->flags &= ~0x4000;
    npcNumGenerators++;
}

void NPCInitHotBoxGenerator(void) {
}

void NPCDisableGenerator(s32 id) {
    generatorInfo[id].flags &= ~0x4000;
    if (D_80110168 > 0) {
        D_80110168--;
    }
}

void NPCEnableGenerator(s32 id) {
    NPCGenerator *gen = &generatorInfo[id];

    if (!(gen->flags & 0x4000) && (gen->flags & 0x7FF)) {
        gen->flags = (gen->flags & ~0x3800) | 0x4000;
        D_80110168++;
    }
}

#ifdef NON_MATCHING
/* first NPC scan: i/ptr regs swapped */
void NPCTryToDeactivate(s32 force, s32 unused1, s32 unused2) {
    s32 best = 0;
    s32 target = gObjPlayer->target->targetId;
    Gobj *victim = NULL;
    s32 id;
    s32 i;
    s32 type;
    s32 score;
    Gobj *obj;
    NPC *npc;

    if ((u32)(gExeTick - D_8011014C) >= 6) {
        i = 0;
        getNPCIndex = 0;
        NPC_SCAN_IDS(id, i)
        while (id >= 0) {
            type = GOBJ_TYPE(id);
            obj = gameObjectsInfo[id].runtime;
            npc = NPC_OF(obj);
            if (type != 0x65 && type != 0x5C && id != gLevelSpecificNpcID && id != gLevelSpecificNpcID2
                && (force || (npc->flags & 0x20)
                    || (obj->healthInfo->health == -1 && obj->npcState != 8 && !(obj->anim->unk00[8] & 0x40))
                    || !EGSDisplayHMDInActiveSector(obj->anim))
                && id != target) {
                score = npc->unk28;
                if (npc->flags & 0x20) {
                    score += 100;
                }
                if (!EGSDisplayHMDInActiveSector(obj->anim)) {
                    score += 1000000;
                } else {
                    if (obj->npcState != 8 && !(obj->anim->unk00[8] & 0x40)) {
                        score += 100000;
                    }
                    if (obj->healthInfo->health == -1) {
                        score += 10000;
                    }
                    if (!(npc->flags & 0x100)) {
                        score += 1000;
                    }
                }
                if (best < score) {
                    best = score;
                    victim = obj;
                }
            }
            NPC_NEXT_ID(id, i);
        }
        if (victim != NULL) {
            D_8011014C = gExeTick;
            NPC_OF(victim)->unk41 = 0;
            if (NPC_OF(victim)->flags & 0x20) {
                NPCDeactivateGenerated(victim);
                NPCPopRemoveID = victim->id;
                return;
            }
            victim->flags23 |= 2;
            NPCPopRemoveID = victim->id;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCTryToDeactivate);
#endif

void NPCRemoveDead(void) {
    s32 best = 0;
    Gobj *victim = NULL;
    s32 id;
    s32 i;
    Gobj *obj;
    s32 score;

    id = NPCGetFirstID();
    while (id >= 0) {
        obj = gameObjectsInfo[id].runtime;
        if (obj->healthInfo->health == -1 && obj->npcState != 8 && !(obj->anim->unk00[8] & 0x40)
            && id != gObjPlayer->target->targetId) {
            score = NPC_OF(obj)->unk28;
            if (NPC_OF(obj)->flags & 0x20) {
                score += 100;
            }
            if (best < score) {
                best = score;
                victim = obj;
            }
        }
        NPC_NEXT_ID(id, i);
    }
    if (victim != NULL) {
        NPC_OF(victim)->unk41 = 0;
        if (NPC_OF(victim)->flags & 0x20) {
            NPCDeactivateGenerated(victim);
        } else {
            victim->flags23 |= 2;
        }
    }
}

s32 NPCActivationOK(s32 id) {
    s32 ok = 1;
    s32 force;
    s32 type;

    if (NPCEndActivationFlag) {
        return 0;
    }
    if (D_80110140 >= 0) {
        if (D_80110140 == id) {
            D_80110140 = -1;
        } else {
            ok = 0;
        }
    } else if (gNPCLevelAlertFlag && gSysInfo.level == 7) {
        if (id == gLevelSpecificNpcID) {
            ok = 0;
            if (NPCPopRemoveID < 0) {
                force = 0;
                goto deactivate;
            }
        } else {
            ok = 0;
        }
    } else if (NPCNumActive >= gLevData->maxActiveNPCs) {
        if (NPCPopRemoveID < 0) {
            force = 0;
            if (gSysInfo.level == 11 || gSysInfo.level == 12 || gSysInfo.level == 13) {
                type = GOBJ_TYPE(id);
                if (type == 0x65 || type == 0x5C) {
                    force = 1;
                }
            }
        deactivate:
            NPCTryToDeactivate(force, id, ok);
        }
        ok = 0;
    } else {
        NPCNumActive++;
        NPCNumActiveTick = gExeTick;
    }
    return ok;
}

void NPCDoAction(Gobj *obj) {
    NPC *npc = NPC_OF(obj);
    s32 start;

    LevelGetPathInfo(obj->id, &start, &D_80110134);
    D_8011E8B8.vx = obj->anim->frame->pos.vx;
    D_8011E8B8.vy = obj->anim->frame->pos.vy;
    D_8011E8B8.vz = obj->anim->frame->pos.vz;
    D_8011E8B8.vy = -D_8011E8B8.vy;
    if (npc != NULL && (npc->flags & 0x200)) {
        if (obj->healthInfo->health > 0) {
            NPCIQEveryFrame(obj);
            NPCAimAll(obj);
            if (GOBJ_TYPE(obj->id) == 0x5C) {
                func_800664F8(obj);
            }
        } else {
            npc->unk32--;
        }
        if (npc->flags & 4) {
            NPCPutFireOnNPCMove(obj->id);
        }
        if (npc->unk4E < 0xFF) {
            npc->unk4E++;
        }
        if (npc->unk4D != 0) {
            npc->unk4D--;
        }
    }
}

#ifdef NON_MATCHING
/* draft: link-search loops' regalloc and the shared DamageGOBJ tail differ (34 insns short) */
s32 NPCKill(s32 id, VECTOR32 *dir) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(obj);
    s32 start;
    VECTOR32 a;
    VECTOR32 b;
    s32 ret;
    s32 i;
    s32 link;
    s32 found;
    PathPoint *cur;
    s32 refId;
    Gobj *ref;
    s32 f;

    LevelGetPathInfo(obj->id, &start, &D_80110134);
    NPCSoundGobj(4, id, NULL, 0x280);
    NPCSetLOSFirstSeen(obj, 0, 0);
    obj->flags23 &= ~1;
    CASRequestAction(obj->id, 0x2D);
    ret = 0;
    if (npc->unk41 != 0) {
        if (!((D_80105F58[NPC_WEAPON(obj->id, f)].flags >> 8) & 7) || (rand() & 1)) {
            npc->unk41 = 0;
        } else {
            obj->target->targetId = -1;
        }
    }
    cur = &D_80110134[npc->curPoint];
    found = -1;
    if (((cur->flags & 0xF00) >> 8) == 7) {
        link = -1;
        for (i = 0; i < 3; i++) {
            if (cur->links[i] != i && cur->links[i] >= 0 && ((D_80110134[cur->links[i]].flags & 0xF00) >> 8) == 7) {
                link = cur->links[i];
                break;
            }
        }
        if (link >= 0) {
            b.vx = D_80110134[link].x;
            b.vy = D_80110134[link].y;
            b.vz = D_80110134[link].z;
            a.vx = D_80110134[npc->curPoint].x;
            a.vy = D_80110134[npc->curPoint].y;
            a.vz = D_80110134[npc->curPoint].z;
            dir->vx = b.vx - a.vx;
            dir->vy = 0;
            dir->vz = b.vz - a.vz;
            NPC_MOVE(obj)->pathCtrl->active = 1;
            NPC_MOVE(obj)->pathCtrl->from = a;
            NPC_MOVE(obj)->pathCtrl->to = b;
            npc->unk2E = b.vy;
            obj->move->unk12C = b.vy;
            refId = gameObjectsInfo[obj->id].refId;
            ret = 15;
            if (refId >= 0 && (ref = gameObjectsInfo[refId].runtime) != NULL && GOBJ_TYPE(ref->id) == 0x37
                && !(npc->flags & 0x20000000)) {
                ret = 17;
                DamageGOBJ(obj->id, obj->id, ref->id, D_80105F58[NPC_WEAPON(obj->id, f)].nearDamage, 0x892);
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (cur->links[i] != i && cur->links[i] >= 0
                && ((D_80110134[cur->links[i]].flags & 0xF00) >> 8) == 1
                && (D_80110134[cur->links[i]].flags & 0xF) == ((D_80110134[cur->links[i]].flags & 0xF00) >> 8)) {
                found = cur->links[i];
                break;
            }
        }
        if (found >= 0) {
            a.vx = D_80110134[found].x;
            a.vy = D_80110134[found].y;
            a.vz = D_80110134[found].z;
            b = obj->move->pos;
            b.vy = a.vy;
            NPC_MOVE(obj)->pathCtrl->active = 1;
            NPC_MOVE(obj)->pathCtrl->from = b;
            NPC_MOVE(obj)->pathCtrl->to = a;
            a.vy = 0;
            a.vx = b.vx - a.vx;
            a.vz = b.vz - a.vz;
            EGSGeometryFIX_VectorGetAngleFromXZPlane(&a, &MOVE_ROT(obj->move));
            ret = 17;
            MOVE_ROT(obj->move).vy = ratan2(a.vx, a.vz);
            MOVE_ROT(obj->move).vz = 0;
            refId = gameObjectsInfo[obj->id].refId;
            if (refId >= 0 && (ref = gameObjectsInfo[refId].runtime) != NULL && GOBJ_TYPE(ref->id) == 0x37
                && !(npc->flags & 0x20000000)) {
                DamageGOBJ(obj->id, obj->id, ref->id, D_80105F58[NPC_WEAPON(obj->id, f)].nearDamage, 0x892);
            }
        }
    }
    if (npc->flags & 0x400000) {
        CobjRemoveCollisionBoxGobj(obj);
        npc->flags &= ~0x400000;
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCKill);
#endif

void NPCDropGun(s32 id) {
    s32 start;
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc;

    if (obj->healthInfo->health > 0) {
        npc = NPC_OF(obj);
        if (npc->flags & 0x10) {
            if (!(npc->flags & 0x8000000)) {
                WeaponDrop(obj, 1);
                CASRequestAction(obj->id, 0x25);
                CASRequestAction(obj->id, 0x55);
            }
        } else {
            LevelGetPathInfo(id, &start, &D_80110134);
            npc->unk4A = 0;
            npc->unk45 = 0;
            npc->nameMsgId &= 1;
            npc->flags |= 0x4000000;
            if (NPCPointIQ(obj, D_80110134) != npc->curPoint) {
                WeaponDrop(obj, 1);
                CASRequestAction(obj->id, 0x25);
            }
        }
    }
}

void NPCDoDamage(Gobj *obj) {
    NPCHealth *health = (NPCHealth *)obj->healthInfo;
    s32 id = obj->id;
    NPC *npc = NPC_OF(obj);
    s32 amount = health->health * 64000;
    s32 armor = health->armor;

    if (armor < 100) {
        armor = 100;
    }
    NPCSoundGobj(4, id, NULL, amount / armor + 0x280);
    if (obj->healthInfo->health > 0 && npc->unk4D == 0 && ((NPCHealth *)obj->healthInfo)->unkC != 0) {
        if (GOBJ_TYPE(obj->id) == 0x5C) {
            PickupRemoveUseMsg(obj->id);
        }
        D_8011E8B8.vx = obj->anim->frame->pos.vx;
        D_8011E8B8.vy = obj->anim->frame->pos.vy;
        D_8011E8B8.vz = obj->anim->frame->pos.vz;
        D_8011E8B8.vy = -D_8011E8B8.vy;
        NPCSetAwarenessLevel(obj, 2, 1, 0);
        if (npc->nameMsgId > 40 && !(NPC_OF(obj)->flags & 2)) {
            npc->nameMsgId -= 20;
        }
        NPCRecordPlayerSighting(obj);
        if ((npc->flags & 0x20) && obj->id != gPlayerOffset && !(GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x4000)
            && NPCNumEnemyAlive == 1 && !(NPC_OF(obj)->flags & 1) && !(rand() & 7)) {
            NPCDropGun(obj->id);
        }
        npc->unk4D = 20;
        if (rand() & 1) {
            NPCPlayRandXA(obj, 0);
            return;
        }
    } else if (npc->flags & 2) {
        SysStopXA();
        return;
    } else if (obj->healthInfo->health <= 0 || ((NPCHealth *)obj->healthInfo)->unkE == 0x58) {
        return;
    }
    NPCPlayRandXA(obj, 1);
}

#ifdef NON_MATCHING
/* case 41: save/health regs swapped (a1/a2) */
void NPCObjDispatch(SysMsg *msg) {
    Gobj *obj = gameObjectsInfo[msg->to].runtime;
    NPC *npc;
    GOBJ_MATRIX m;
    GameObjectHealth *health;
    s32 save;
    s32 slot;
    s32 refId;
    Gobj *ref;
    s32 type;

    switch (msg->id) {
    case 2:
        if (obj->npc == NULL) {
            func_801463A4(obj->id, iAnimationArray);
            npc = EGSMemMalloc(sizeof(NPC));
            if (npc == NULL) {
                EGSDebugAssert(1, 0, D_8010F964, 4884);
            }
            obj->npc = (struct NPCInfo *)npc;
            LevelGetGameObjectMatrix(obj->id, &m);
            npc->unk2E = m.t[1];
            EGSFrameSetMatrix(obj->anim->frame, 0, &m);
            obj->unk20[0] |= 0x40;
            NPCGobjInit(obj);
            obj->unk20[1] = 0;
        }
        obj->unk04 = -1;
        break;
    case 10:
        if (!(obj->flags23 & 2)) {
            NPCDoAction(obj);
        }
        NPC_OF(obj)->flags &= ~0x200000;
        break;
    case 32:
        NPCDoDamage(obj);
        break;
    case 5:
        NPCDeactivate(obj->id);
        break;
    case 6:
        NPCActivate(obj->id);
        if (obj->cas->active == 10) {
            CASRequestAction(obj->id, 0x3E);
        }
        CASRequestAction(obj->id, 5);
        break;
    case 18:
        if (obj->healthInfo->health > 0 && GOBJ_TYPE(obj->id) != 0x5C) {
            refId = gameObjectsInfo[obj->id].refId;
            if (refId >= 0 && (ref = gameObjectsInfo[refId].runtime) != NULL) {
                type = GOBJ_TYPE(ref->id);
                if (type == GOBJ_TYPE_BOMB && (ref->flags1 & 0x80)) {
                    break;
                }
            }
            obj->flags23 &= ~2;
        }
        break;
    case 20:
    case 39:
        if (!(obj->flags0 & 0x20)) {
            obj->flags0 |= 0x20;
            gObjectInteractionCallback(obj->id, msg);
            UseObject(obj->id);
        }
        break;
    case 41:
        save = 0;
        health = obj->healthInfo;
        if (health->health > 0 && NPC_OF(obj)->curPoint != 0xFF) {
            if (obj->flags23 & 2) {
                health->health = -1;
            } else {
                save = NPC_OF(obj)->flags;
                save |= 0x40;
                if (obj->flags0 & 0x20) {
                    save |= 0x80;
                } else {
                    save &= ~0x80;
                }
                if (save & 0x20) {
                    if (save & 0x200) {
                        slot = obj->id - D_80110148;
                        save = (save & 0xFF) | (genData[slot].gen << 8) | (genData[slot].path << 16)
                               | (genData[slot].variant << 24);
                    } else {
                        save = 0;
                    }
                }
                *(s32 *)msg->data[0] = save;
                *(s16 *)msg->data[1] = NPC_OF(obj)->awareness | (NPC_OF(obj)->curPoint << 8);
            }
        }
        if ((NPC_OF(obj)->flags & 0x20) && save == 0) {
            obj->healthInfo->health = -1;
        }
        break;
    case 42:
        save = *(s32 *)msg->data[0];
        if (save != 0) {
            NPCRestore(obj->id, ((u8 *)msg->data[1])[0], save, ((u8 *)msg->data[1])[1]);
        } else if (obj->healthInfo->health <= 0) {
            obj->healthInfo->health = -1;
            obj->flags23 |= 2;
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCObjDispatch);
#endif

void NPCPickAimer(s32 id, s32 dist) {
    Gobj *obj;
    NPC *npc;
    s32 limit;
    s32 flags;
    s32 wflags;

    if (gSysInfo.level != 4) {
        obj = gameObjectsInfo[id].runtime;
        npc = NPC_OF(obj);
        limit = 0x400;
        if (npc->flags & 0x10) {
            D_80110138 = id;
        } else if (D_80110138 < 0 && (!(obj->cas->flags & 2) || NPCNumEnemyAlive == 1)
                   && obj->healthInfo->health > 0 && npc->awareness == 2) {
            if ((obj->id != gPlayerOffset
                     ? (*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
                        || ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000
                        || (flags & 0x3000) == 0x2000)
                     : gCurrentWeapon != 0)
                && GOBJ_TYPE(id) != 0x4C && !(npc->flags & 0x2000)) {
                D_80110138 = id;
            }
        } else if (id == D_80110138) {
            if (dist < 0x333 && (obj->cas->flags & 2) && NPCNumEnemyAlive >= 2) {
                D_80110138 = -1;
            }
            limit = 0x2000;
        }
        if (((D_80105F58[NPC_WEAPON(obj->id, wflags)].flags >> 3) & 7) == 2) {
            limit = 0x200;
        }
        if (dist <= limit) {
            npc->flags |= 0x20000;
        } else {
            npc->flags &= ~0x20000;
        }
    }
}

void NPCAim(void) {
}

void NPCAimAll(Gobj *obj) {
    s32 tid = obj->target->targetId;
    VECTOR32 pos;
    Gobj *t;
    s32 hostage;
    s32 flags;

    if (D_80110138 == obj->id) {
        if (!(obj->cas->flags & 0x400) && NPC_OF(obj)->unk4D == 0) {
            if (NPC_OF(obj)->flags & 0x10) {
                hostage = NPCPickHostageTarget(obj->id, 0);
                if (hostage >= 0) {
                    tid = hostage;
                    obj->target->targetId = hostage;
                }
            }
            if (tid >= 0) {
                t = gameObjectsInfo[tid].runtime;
                if (t->anim != NULL && (t->npc == NULL || (NPC_OF(t)->flags & 0x200))) {
                    if (t->npcState == 1 || t->npcState == 2) {
                        pos.vx = (*t->anim->frames)->pos.vx;
                        pos.vy = (*t->anim->frames)->pos.vy;
                        pos.vz = (*t->anim->frames)->pos.vz;
                        pos.vy = -pos.vy - 8;
                    } else {
                        pos.vx = t->anim->frame->pos.vx;
                        pos.vy = t->anim->frame->pos.vy;
                        pos.vz = t->anim->frame->pos.vz;
                        pos.vy = -pos.vy;
                    }
                    if (gSysInfo.level == 18) {
                        AnimateGobjRequestAim(obj, &NPC_OF(obj)->lastSeenPos, 1, 0);
                    } else {
                        AnimateGobjRequestAim(obj, &pos, 1, 0);
                    }
                }
            }
        }
    } else if (NPC_OF(obj)->awareness == 2 && tid >= 0) {
        if (obj->id != gPlayerOffset) {
            if (*(u8 *)&gameObjectsInfo[obj->id].flags24 == 0
                && ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) != 0x1000 && (flags & 0x3000) != 0x2000) {
                return;
            }
        } else if (gCurrentWeapon == 0) {
            return;
        }
        if (GOBJ_TYPE(obj->id) != 0x4C && !(NPC_OF(obj)->flags & 0x2000)) {
            t = gameObjectsInfo[tid].runtime;
            if (t->anim != NULL && (t->npc == NULL || (NPC_OF(t)->flags & 0x200))) {
                if (t->npcState == 1 || t->npcState == 2) {
                    pos.vx = (*t->anim->frames)->pos.vx;
                    pos.vy = (*t->anim->frames)->pos.vy;
                    pos.vz = (*t->anim->frames)->pos.vz;
                    pos.vy = -pos.vy - 8;
                } else {
                    pos.vx = t->anim->frame->pos.vx;
                    pos.vy = t->anim->frame->pos.vy;
                    pos.vz = t->anim->frame->pos.vz;
                    pos.vy = -pos.vy;
                }
                if ((obj->target->flags & 2) || tid != gPlayerOffset) {
                    AnimateGobjRequestAim(obj, &pos, 1, 1);
                } else {
                    AnimateGobjRequestAim(obj, &NPC_OF(obj)->lastSeenPos, 1, 1);
                }
            }
        }
    }
}

void LOSOverride(VECTOR32 *pos) {
    if (pos != NULL) {
        D_8011E8C8.vx = pos->vx;
        D_8011E8C8.vy = pos->vy;
        D_8011E8C8.vz = pos->vz;
        D_8011E8D4 = 1;
    } else {
        D_8011E8D4 = 0;
    }
}

/* Is the object linked to a key (type 0x1C)? */
static inline s32 NPCHasKey(Gobj *obj) {
    s32 ref = gameObjectsInfo[obj->id].refId;
    s32 ret = 0;

    if (ref != -1 && ref != GOBJ_ID_666) {
        ret = gameObjectsModelInfo[gameObjectsInfo[ref].modelIndex].type == 0x1C;
    }
    return ret;
}

#ifdef NON_MATCHING
/* inlined key test result needs an extra reg copy before the branch */
void NPCDropPickupStuff(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    s32 flags;

    if (*(u8 *)&gameObjectsInfo[obj->id].flags24 != 0
        || ((flags = GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id])) & 0x3000) == 0x1000 || (flags & 0x3000) == 0x2000
        || obj->id == gPlayerOffset || (flags & 0x4000)) {
        WeaponDrop(obj, 1);
    }
    if (NPCHasKey(obj)) {
        KeyDroppedByNPC(obj);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDropPickupStuff);
#endif

#ifdef NON_MATCHING
/* drop flag gets v1 instead of reusing v0 */
void NPCLosUpdateOtherStuff(s32 id) {
    VECTOR32 point;
    s32 start;
    PathPoint *path;
    s32 dist;
    Gobj *obj;
    NPC *npc;
    Gobj *aimer;
    s32 flags;
    TargetInfo *target;
    s16 bonus;

    D_8011E8A8 = gameObjectsInfo[gPlayerOffset].runtime->move->pos;
    if (id == gPlayerOffset) {
        npcVisibilityDistance = gActivationDistance = EGSDisplayGetFuzzyFoggedOutDistance();
        if (gSysInfo.level == 3) {
            npcVisibilityDistance = 0xA00;
        }
        NPCGetNumActive();
        if (gSysInfo.level != 9) {
            NPCCheckGenerators();
        }
        if (D_80110138 >= 0
            && ((aimer = gameObjectsInfo[D_80110138].runtime)->healthInfo->health <= 0
                || (NPC_OF(aimer)->awareness != 2 && !(NPC_OF(aimer)->flags & 0x10)))
            && gSysInfo.level != 18) {
            D_80110138 = -1;
        }
    } else {
        obj = gameObjectsInfo[id].runtime;
        npc = NPC_OF(obj);
        D_8011E8B8 = obj->move->pos;
        LevelGetPathInfo(obj->id, &start, &path);
        point.vx = path[npc->curPoint].x;
        point.vy = path[npc->curPoint].y;
        point.vz = path[npc->curPoint].z;
        EGSGeometry_GetFuzzyDistancePoint2Point(&point, &D_8011E8A8, &dist);
        npc->unk2A = dist;
        EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011E8B8, &D_8011E8A8, &dist);
        npc->unk28 = dist;
        if (NPCViewConeCheck(obj)) {
            npc->flags |= 0x100;
        } else {
            npc->flags &= ~0x100;
        }
        if (npcPCInvisibleFlag) {
            npc->flags &= ~0x100;
        }
        if (obj->anim->unk00[8] & 0x40) {
            npc->flags |= 0x4000;
        } else {
            npc->flags &= ~0x4000;
        }
        if (obj->healthInfo->health > 0) {
            NPCIQ(obj);
        } else {
            s32 drop;

            NPCDeadContactTheLiving(obj);
            if (obj->healthInfo->health == 0) {
                VECTOR32 pos;
                VECTOR32 pc;
                s32 d;

                pc = gameObjectsInfo[gPlayerOffset].runtime->move->unk10C;
                pos.vx = obj->anim->frame->pos.vx;
                pos.vy = obj->anim->frame->pos.vy;
                pos.vz = obj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &pc, &d);
                npc->unk28 = d;
                drop = d < 0xA0;
            } else {
                drop = obj->move->collFlags & 0x100000;
            }
            if (drop) {
                NPCDropPickupStuff(obj->id);
            }
        }
    }
    TargetCalcProb(id);
    if (id != gPlayerOffset) {
        Gobj *self = gameObjectsInfo[id].runtime;
        NPC *selfNpc = NPC_OF(self);

        flags = selfNpc->flags;
        if ((flags & 0x80010) == 0x10) {
            self->target->unk58 = 200;
        } else if ((flags & 0x20000000) && (target = self->target)->targetId == gameObjectsInfo[self->id].refId) {
            if (target->flags & 2) {
                target->unk58 = 200;
            }
        } else {
            bonus = self->target->unk58;
            if (bonus > 0 && (selfNpc->flags & 1) && self->target->targetId != gPlayerOffset) {
                self->target->unk58 = bonus + 50;
            }
        }
    }
    if (gSysInfo.level != 18) {
        TargetSetThreatLevel(id);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosUpdateOtherStuff);
#endif

void NPCTakeShot(Gobj *obj, s32 dist) {
    NPC *npc = NPC_OF(obj);
    s32 restore = -1;
    s32 weapon;
    TargetInfo *target;
    s32 hit;
    Gobj *victim;
    s32 r;
    s32 d;
    u32 damage;
    s32 f;

    weapon = obj->id != gPlayerOffset ? NPC_WEAPON(obj->id, f) : gCurrentWeapon;
    if (obj->target->unk58 > 0 && GameSysIsTime(npc->unk30, D_80105F58[weapon].fireRate)) {
        target = obj->target;
        npc->unk30 = gExeTick;
        NPCSoundGobj(0, obj->id, NULL, 32000);
        if (!(target->flags & 8) && obj->healthInfo->health > 0) {
            NPCSetLOSFirstSeen(obj, 1, 0);
        }
        if (--npc->unk41 == 0) {
            r = rand();
            npc->unk38 = D_80105F58[obj->id != gPlayerOffset ? NPC_WEAPON(obj->id, f) : gCurrentWeapon].fireRate
                         + (((r & 0x1F) + 20) >> D_80106460[gSysInfo.level]);
        }
        WeaponSubRound(obj);
        if (weapon != 19 && weapon != 20) {
            WeaponDoFiredCallback(obj->id, weapon);
        }
        if (npc->unk4D != 0
            || (obj->target->targetId != gPlayerOffset && gameObjectsInfo[obj->id].refId <= 0 && gSysInfo.level != 13)) {
            if (obj->target->targetId >= 0) {
                EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011E8B8,
                                                        &gameObjectsInfo[obj->target->targetId].runtime->move->pos, &d);
                if (d > 0x140) {
                    restore = obj->target->targetId;
                    target->targetId = -1;
                }
            }
        }
        hit = TargetShoot(obj, 0, 0);
        CASRequestAction(obj->id, 0x15);
        {
            VECTOR32 pos = obj->move->pos; /* unused */
        }
        EGSAnimGunFlash2(obj->anim, 2, D_80105F58[NPC_WEAPON(obj->id, f)].flash);
        SysSoundSetShootingExciteLevel();
        if (restore < 0) {
            if (hit) {
                victim = gameObjectsInfo[obj->target->targetId].runtime;
                if (dist > 1920) {
                    dist = 1920;
                }
                if (dist <= 960) {
                    damage = D_80105F58[obj->id != gPlayerOffset ? NPC_WEAPON(obj->id, f) : gCurrentWeapon].nearDamage;
                } else {
                    damage = D_80105F58[obj->id != gPlayerOffset ? NPC_WEAPON(obj->id, f) : gCurrentWeapon].farDamage;
                }
                DamageGOBJ(obj->id, obj->id, victim->id, damage, 0xF);
                D_8011E890[npc->slot] = gExeTick + 60;
                if (victim->id == gPlayerOffset) {
                    NPCPlayRandXA(obj, 2);
                }
            }
        } else {
            target->targetId = restore;
        }
    }
}

void NPCDoShootingSchedule(void) {
    s32 i;
    s32 id;
    Gobj *obj;
    NPC *npc;

    s32 flags;

    for (i = NPC_SLOTS - 1; i != -1; i--) {
        D_8010F970++;
        if (D_8010F970 >= NPC_SLOTS) {
            D_8010F970 = 0;
        }
        id = npcList[D_8010F970];
        if (id >= 0) {
            obj = gameObjectsInfo[id].runtime;
            if (obj->healthInfo->health != -1) {
                npc = NPC_OF(obj);
                flags = npc->flags;
                if (npc->unk41 && (flags & 0x20000) && (!(flags & 0x10000) || obj->id == D_80110138)
                    && (obj->move->collFlags & 0x100000) && npc->casAction == 0) {
                    NPCTakeShot(obj, npc->unk28);
                }
                return;
            }
        }
    }
}

/* Object id a line of sight hit belongs to, -1 for none */
static inline s32 LOSHitGobjId(LOSHit *hit) {
    if (hit == NULL) {
        return -1;
    }
    if (hit->type == 1) {
        return hit->owner->id & 0x3FF;
    }
    return -1;
}

s32 NPCLosToPCProcess(s32 self, s32 pc) {
    u32 i;
    s32 seen = 0;
    s32 id;

    for (i = 0; i < losSortedCount; i++) {
        id = LOSHitGobjId(losHitsSorted[i]);
        if (id != self) {
            if (id == pc) {
                seen = 1;
                break;
            }
            if (id < 0) {
                break;
            }
        }
    }
    return seen;
}

/* _LOSPoint2Point query, see gtegeo.c */
typedef struct NPCLosP2P {
    /* 0x00 */ VECTOR32 *from;
    /* 0x04 */ VECTOR32 *to;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ Gobj *gobj;
    /* 0x10 */ s32 unk10[3];
    /* 0x1C */ u8 *hit;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
} NPCLosP2P;

/* _LOSPointAlongDirection query, see gtegeo.c */
typedef struct NPCLosRay {
    /* 0x00 */ VECTOR32 *from;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ VECTOR32 *dir;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 length;
    /* 0x1C */ u8 *hit;
    /* 0x20 */ VECTOR32 *point;
    /* 0x24 */ VECTOR32 *normal;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ u8 *hitWindow;
    /* 0x30 */ NPCLosP2P *window;
} NPCLosRay;

void _LOSPoint2Point(NPCLosP2P *q);
void _LOSPointAlongDirection(NPCLosRay *q);
s16 LOSGetSortOrder(void);
s32 HMDLOSLoop(GameObjectAnim *anim);
void func_801442B4(void *threat, u8 *inSight);
void NPCLosUpdateOtherStuff(s32 id);

#ifdef NON_MATCHING
/* draft: structure follows the m2c output; not yet matched */
void NPCLosCheck(void) {
    VECTOR32 dir;
    VECTOR32 from;
    u8 inSight;
    VECTOR32 a;
    VECTOR32 b;
    s32 dist;
    NPCLosP2P p2p;
    u8 hitP2P;
    NPCLosRay ray;
    u8 hit;
    u8 hitWindow;
    s32 d2;
    Gobj *player;
    s32 tries;
    s32 done = 0;
    s32 id;
    s32 i;
    Gobj *obj;
    Gobj *self;
    Gobj *aim;
    TargetInfo *target;
    s32 forced;
    s32 hitId;
    u32 n;
    LOSHit *h;
    LOSHit *last;
    s32 type;

    tries = 2;
    if ((u32)gExeTick < 5) {
        return;
    }
    player = gameObjectsInfo[gPlayerOffset].runtime;
    for (;;) {
        id = -1;
        i = D_8010F974;
        getNPCIndex = D_8010F974;
        NPC_SCAN_IDS(id, i)
        if (id < 0) {
            getNPCIndex = 0;
            if (--tries == 0) {
                D_8010F974 = 0;
                goto update;
            }
            if (++D_8010F978 > 0) {
                D_8010F978 = -1;
            }
            id = gPlayerOffset;
            if (id == gPlayerOffset) {
                id = NPCGetFirstID();
                if (id < 0) {
                    getNPCIndex = 0;
                    goto update;
                }
            }
        }
        obj = gameObjectsInfo[id].runtime;
        D_8010F974 = getNPCIndex;
        if (GOBJ_TYPE(obj->id) == 3) {
            func_801442B4(&obj->target->threat, &inSight);
            if (((obj->target->flags >> 3) & 1) != inSight) {
                if (inSight) {
                    InterfaceAddNPC(obj->id);
                } else if (obj->npc != NULL) {
                    InterfaceRemoveNPC(obj->id);
                }
                obj->target->flags = (obj->target->flags & ~8) | ((inSight & 1) << 3);
            }
            if (!inSight) {
                player->target->targetId = -1;
            }
            D_8010F974 = getNPCIndex;
            return;
        }
        if (!EGSDisplayHMDInActiveSector(obj->anim)) {
            obj->target->flags &= ~2;
            goto update;
        }
        if (obj->healthInfo->health <= 0 && D_8010F978 > 0) {
            if (id != gPlayerOffset) {
                b = gameObjectsInfo[gPlayerOffset].runtime->move->unk10C;
                a.vx = obj->anim->frame->pos.vx;
                a.vy = obj->anim->frame->pos.vy;
                a.vz = obj->anim->frame->pos.vz;
                a.vy = -a.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&a, &b, &dist);
                NPC_OF(obj)->unk28 = dist;
            }
        } else if (D_8010F978 < 0) {
            self = gameObjectsInfo[id].runtime;
            b = self->move->unk10C;
            b.vy += 0x20;
            a.vx = ((EGSFrame *)*gCamera)->pos.vx;
            a.vy = ((EGSFrame *)*gCamera)->pos.vy;
            done = 1;
            a.vz = ((EGSFrame *)*gCamera)->pos.vz;
            p2p.from = &a;
            p2p.to = &b;
            a.vy = -a.vy;
            p2p.unk08 = 0;
            p2p.gobj = self;
            p2p.hit = &hitP2P;
            p2p.unk20 = 0;
            p2p.unk24 = 0;
            p2p.unk28 = 0;
            p2p.unk2C = 0;
            p2p.unk30 = 0;
            _LOSPoint2Point(&p2p);
            *(s16 *)&self->anim->unk14[2] = self->cas->active != 8 ? LOSGetSortOrder() : -1;
            if (hitP2P == 0 && *(u32 *)&losHitsSorted[0]->unk00[4] < 0x80) {
                self->anim->unk00[0xB] &= ~0x40;
                if (self->healthInfo->health == -1) {
                    self->anim->unk00[0xB] &= ~0x20;
                }
            } else {
                self->anim->unk00[0xB] |= 0x40;
                if (hitP2P && id != gPlayerOffset && (self->anim->unk00[8] & 0x40) && self->healthInfo->health > 0) {
                    NPCSetLOSFirstSeen(self, 1, 1);
                } else if (self->healthInfo->health <= 0) {
                    self->anim->unk00[0xB] |= 0x20;
                }
            }
        } else if (D_8010F978 == 0) {
            last = NULL;
            hitWindow = 0;
            hit = 0;
            self = gameObjectsInfo[id].runtime;
            target = self->target;
            forced = 0;
            if (id != gPlayerOffset) {
                if (target->targetId != gPlayerOffset && id != gObjPlayer->target->targetId
                    && (NPC_OF(self)->flags & 0x10000000)) {
                    type = GOBJ_TYPE(id);
                    if (type != GOBJ_TYPE_COP && type != 0x4C && type != 0x5C) {
                        forced = 1;
                    }
                }
                if (!(NPC_OF(self)->flags & 0x10000000)) {
                    NPC_OF(self)->flags |= 0x10000000;
                } else {
                    NPC_OF(self)->flags &= ~0x10000000;
                }
            }
            if (forced) {
                from.vx = (*self->anim->frames)->pos.vx;
                from.vy = -(*self->anim->frames)->pos.vy - 8;
                from.vz = (*self->anim->frames)->pos.vz;
                dir.vx = (*gObjPlayer->anim->frames)->pos.vx - from.vx;
                dir.vy = -(*gObjPlayer->anim->frames)->pos.vy - 8 - from.vy;
                dir.vz = (*gObjPlayer->anim->frames)->pos.vz - from.vz;
                ray.from = &from;
                ray.dir = &dir;
                ray.length = 0x1900;
                ray.unk08 = 0;
                ray.unk0C = 0;
                ray.hit = &hit;
                ray.point = NULL;
                ray.normal = NULL;
                ray.unk28 = 0;
                ray.hitWindow = NULL;
                ray.window = NULL;
                _LOSPointAlongDirection(&ray);
                done = 1;
                if (!hit) {
                    hit = NPCLosToPCProcess(id, gPlayerOffset);
                }
                target->flags = (target->flags & ~0x10) | ((hit & 1) << 4);
                goto update;
            }
            aim = NULL;
            if (target->targetId >= 0) {
                aim = gameObjectsInfo[target->targetId].runtime;
                if (aim != NULL
                    && ((aim->anim->frames != NULL && self->anim->frames != NULL)
                        || ((NPC_OF(self)->flags & 0x20000000) && aim->id == gameObjectsInfo[self->id].refId))) {
                    if (aim->id == gPlayerOffset && D_8011E8C8.pad) {
                        dir.vx = D_8011E8C8.vx;
                        dir.vy = D_8011E8C8.vy;
                        dir.vz = D_8011E8C8.vz;
                    } else if (aim->npcState == 1 || aim->npcState == 2) {
                        dir.vx = (*aim->anim->frames)->pos.vx;
                        dir.vy = -(*aim->anim->frames)->pos.vy - 8;
                        dir.vz = (*aim->anim->frames)->pos.vz;
                    } else {
                        dir.vx = aim->anim->frame->pos.vx;
                        dir.vy = -aim->anim->frame->pos.vy;
                        dir.vz = aim->anim->frame->pos.vz;
                    }
                    from.vx = (*self->anim->frames)->pos.vx;
                    from.vy = -(*self->anim->frames)->pos.vy - 8;
                    from.vz = (*self->anim->frames)->pos.vz;
                    done = 1;
                    dir.vx -= from.vx;
                    dir.vy -= from.vy;
                    dir.vz -= from.vz;
                }
            }
            if (!done) {
                done = 1;
                if (self->npcState == 1 || self->npcState == 2) {
                    from.vx = (*self->anim->frames)->pos.vx;
                    from.vy = -(*self->anim->frames)->pos.vy - 8;
                    from.vz = (*self->anim->frames)->pos.vz;
                } else {
                    from.vx = self->anim->frame->pos.vx;
                    from.vy = -self->anim->frame->pos.vy;
                    from.vz = self->anim->frame->pos.vz;
                }
                if (self->id == gPlayerOffset && (self->cas->flags & 0x2000000)) {
                    dir.vx = ((EGSFrame *)*gCamera)->m[0][2];
                    dir.vy = 0;
                    dir.vz = ((EGSFrame *)*gCamera)->m[2][2];
                } else {
                    dir.vx = self->anim->frame->m[0][2];
                    dir.vy = -self->anim->frame->m[1][2];
                    dir.vz = self->anim->frame->m[2][2];
                }
                aim = NULL;
            }
            ray.from = &from;
            ray.dir = &dir;
            ray.length = 0x1900;
            ray.hit = &hit;
            ray.point = TARGET_AIM(target);
            ray.normal = (VECTOR32 *)((u8 *)target + 0x84);
            ray.hitWindow = &hitWindow;
            ray.unk08 = 0;
            ray.unk0C = 0;
            ray.unk28 = 0;
            ray.window = &p2p;
            _LOSPointAlongDirection(&ray);
            last = NULL;
            for (n = 0; n < losSortedCount; n++) {
                h = losHitsSorted[n];
                hitId = LOSHitGobjId(h);
                if (hitId == id) {
                    continue;
                }
                last = h;
                if (hitId >= 0) {
                    if (aim != NULL && hitId == aim->id) {
                        if (gameObjectsInfo[hitId].runtime->npcState == 1 || gameObjectsInfo[hitId].runtime->npcState == 2) {
                            HMDLOSLoop(gameObjectsInfo[hitId].runtime->anim);
                        }
                        *TARGET_AIM(target) = *(VECTOR32 *)((u8 *)last + 0x20);
                        *(VECTOR32 *)((u8 *)target + 0x84) = *(VECTOR32 *)((u8 *)last + 0x10);
                        hit = 1;
                        last = n + 1 < losSortedCount ? losHitsSorted[n + 1] : NULL;
                        break;
                    }
                    if (gameObjectsInfo[hitId].runtime->npcState == 2 && !HMDLOSLoop(gameObjectsInfo[hitId].runtime->anim)) {
                        continue;
                    }
                } else if (h->type == 2) {
                    continue;
                }
                hit = 0;
                break;
            }
            if (last != NULL) {
                *(VECTOR32 *)((u8 *)target + 0x74) = *(VECTOR32 *)((u8 *)last + 0x20);
                *(VECTOR32 *)((u8 *)target + 0x94) = *(VECTOR32 *)((u8 *)last + 0x10);
                target->unk60 = ((s16 *)*(void **)((u8 *)last + 0x4C))[1] & 0x3F;
                *(s16 *)target->unk5C = last->type == 1 ? last->owner->id & 0x3FF : -1;
            } else {
                GTEMath_VectorNormal(&dir, &dir);
                ((VECTOR32 *)((u8 *)target + 0x94))->vx = dir.vx;
                ((VECTOR32 *)((u8 *)target + 0x94))->vy = dir.vy;
                ((VECTOR32 *)((u8 *)target + 0x94))->vz = dir.vz;
                ((VECTOR32 *)((u8 *)target + 0x74))->vx = from.vx + dir.vx;
                ((VECTOR32 *)((u8 *)target + 0x74))->vy = from.vy + dir.vy;
                *(s16 *)target->unk5C = -1;
                target->unk60 = 0x19;
                ((VECTOR32 *)((u8 *)target + 0x74))->vz = from.vz + dir.vz;
            }
            if (aim->id == gPlayerOffset && D_8011E8C8.pad && !hit) {
                EGSGeometry_GetFuzzyDistancePoint2Point(&from, TARGET_AIM(target), &d2);
                if (NPC_OF(self)->unk28 < d2) {
                    hit = 1;
                }
            }
            target->flags = (target->flags & ~2) | ((hit & 1) << 1);
            target->unk5A = aim->id;
            if (hitWindow && (u8)(s32)p2p.to == 2) {
                target->unk5E = ((s16 *)p2p.from)[1];
            } else {
                target->unk5E = -1;
            }
            if (aim->id == gPlayerOffset) {
                if (hit && (self->anim->unk00[8] & 0x40)) {
                    NPCSetLOSFirstSeen(self, 1, 1);
                }
                if (!hit) {
                    hit = NPCLosToPCProcess(id, gPlayerOffset);
                }
                target->flags = (target->flags & ~0x10) | ((hit & 1) << 4);
            } else if (self->id == gPlayerOffset) {
                if (hit && *(s16 *)target->unk5C >= 0) {
                    Gobj *seen = gameObjectsInfo[*(s16 *)target->unk5C].runtime;

                    if (seen->npcState == 2) {
                        if (NPC_OF(aim)->awareness != 0) {
                            npcRequestKneel(aim);
                        }
                        if (NPC_OF(seen)->awareness != 0) {
                            npcRequestKneelRelease(seen);
                        }
                    }
                }
                if (aim != NULL) {
                    if (!hit) {
                        hit = NPCLosToPCProcess(gPlayerOffset, id);
                    }
                    aim->target->flags = (aim->target->flags & ~0x10) | ((hit & 1) << 4);
                }
            }
        }
        if (done) {
            break;
        }
    }
update:
    if (id >= 0) {
        NPCLosUpdateOtherStuff(id);
        if (npcAlertByScream) {
            npcAlertByScream--;
        }
    }
    NPCDoShootingSchedule();
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosCheck);
#endif

void NPCInitSlot(Gobj *obj, s32 slot) {
    VECTOR32 from;
    VECTOR32 to;
    s32 start;

    NPC_OF(obj)->slot = slot;
    TargetInitGobj(obj, &D_8011D240[slot], 2, 2);
    obj->target->unk02 = 1;
    *(s32 *)obj->target->unkB4 = 0;
    obj->target->unkB8 = 0;
    obj->move = &D_8011D750[slot];
    MovementGobjInit(obj, 0, 0, 0, 0);
    NPC_MOVE(obj)->pathCtrl = &D_8011E1A0[slot];
    MovementGobjInitCAS(obj, 0x75C0, 0x18500, 0x1000, 0x333, 0x333, 0);
    PhysicsGobjBasicInit(obj, -1, 1);
    CollisionGobjBasicInit(obj, 0x80000001, 0, 0, -1, -1);
    D_8011E890[slot] = 0xA0;
    LevelGetPathInfo(obj->id, &start, &D_80110134);
    from.vx = D_80110134[NPC_OF(obj)->nextPoint].x;
    from.vy = D_80110134[NPC_OF(obj)->nextPoint].y;
    from.vz = D_80110134[NPC_OF(obj)->nextPoint].z;
    to.vx = D_80110134[NPC_OF(obj)->curPoint].x;
    to.vy = D_80110134[NPC_OF(obj)->curPoint].y;
    to.vz = D_80110134[NPC_OF(obj)->curPoint].z;
    NPC_MOVE(obj)->pathCtrl->active = 1;
    NPC_MOVE(obj)->pathCtrl->from = from;
    NPC_MOVE(obj)->pathCtrl->to = to;
    NPC_OF(obj)->flags &= ~0x400;
    NPC_OF(obj)->unk45 = 0;
    D_80110128[slot] = 0;
}

void NPCUninitSlot(Gobj *obj) {
    NPC_OF(obj)->slot = -1;
    obj->target = NULL;
    obj->move = NULL;
}

void NPCListAdd(Gobj *obj) {
    s32 slot = -1;
    s32 i;

    for (i = 0; i < NPC_SLOTS; i++) {
        if (npcList[i] < 0) {
            slot = i;
            break;
        }
    }
    npcList[slot] = obj->id;
    NPCInitSlot(obj, slot);
}

void NPCListDelete(Gobj *obj) {
    npcList[NPC_OF(obj)->slot] = -1;
    NPCUninitSlot(obj);
}

void NPCActivate(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(obj);
    s32 ref;
    Gobj *hostage;

    if (!(npc->flags & 0x200) && npc != NULL) {
        if (npc->curPoint == 0xFF) {
            npc->curPoint = 0;
        }
        EGSDisplayHMDCache(obj->anim, 1);
        NPCListAdd(obj);
        MovementAddGobj(obj);
        CobjAddBulletCollisionBoxGobj(obj, 1, 0, 0);
        WeaponReplace(obj);
        if (!(npc->flags & 0x400000)) {
            CobjAddCollisionBoxGobj(obj, 1, 0, 1, 0);
            npc->flags |= 0x400000;
        }
        if (gNPCLevelAlertFlag && !(GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x300)) {
            NPCSetAwarenessLevel(obj, 1, 0, 0);
        }
        npc->flags |= 0x200;
        obj->anim->unk00[0xB] &= ~0x40;
        NPCSetLOSFirstSeen(obj, 0, 0);
        if (npc->flags & 2) {
            npc->awareness = 2;
            NPCPutFireOnNPCShow(id);
            SysPlayXA(0x288, (s32)obj, 0);
        }
        npc->lastSeenPos = D_8011E8A8;
        npc->unk4E = 0;
        npc->unk4D = 0;
        if (gSysInfo.level == 18) {
            obj->flags23 |= 1;
            npc->unk2C = 60;
            npc->unk38 = 60;
            if (id != gLevelSpecificNpcID) {
                npc->nameMsgId = 11;
            }
        }
        if (npc->flags & 0x8000000) {
            if (obj->healthInfo->health > 0) {
                CASRequestAction(id, 0x55);
            }
        } else {
            ref = gameObjectsInfo[obj->id].refId;
            hostage = gameObjectsInfo[ref].runtime;
            if (ref >= 0 && hostage != NULL && hostage->npc != NULL) {
                npcHostageSetActivation(obj, 1);
            }
        }
        if (id == gLevelSpecificNpcID) {
            npc->xaTimer = 0;
            if (gSysInfo.level == 0) {
                npc->voice = 2;
            } else if (gSysInfo.level == 3) {
                npc->voice = 6;
            } else if (gSysInfo.level == 2) {
                npc->voice = 7;
            } else {
                npc->voice = 3;
            }
        } else if (id == gLevelSpecificNpcID2) {
            npc->xaTimer = 0;
            if (gSysInfo.level == 5) {
                npc->voice = 5;
            } else if (gSysInfo.level == 6) {
                npc->voice = 7;
            } else {
                npc->voice = 4;
            }
        } else if (obj->id == gPlayerOffset || (GOBJ_INFO_FLAGS(&gameObjectsInfo[obj->id]) & 0x4000)) {
            npc->flags |= 0x40000000;
        }
        if (gNpcAllOnRadarFlag) {
            InterfaceAddNPC(id);
        }
    }
}

void NPCDeactivate(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(obj);
    s32 flags;
    s32 ref;
    Gobj *hostage;

    if (npc->flags & 0x200) {
        if (npc != NULL) {
            if (NPCPopRemoveID == id) {
                NPCPopRemoveID = -1;
            }
            if (gNpcAllOnRadarFlag) {
                InterfaceRemoveNPC(obj->id);
            }
            EGSDisplayHMDCache(obj->anim, 0);
            MovementRemoveGobj(obj);
            WeaponDrop(obj, 0);
            CobjRemoveBulletCollisionBoxGobj(obj);
            if (npc->flags & 0x400000) {
                CobjRemoveCollisionBoxGobj(obj);
                npc->flags &= ~0x400000;
            }
            NPCSetLOSFirstSeen(obj, 0, 0);
            NPCListDelete(obj);
            flags = npc->flags;
            npc->flags = flags & ~0x200;
            if (flags & 4) {
                NPCPutFireOnNPCHide(id);
            }
            if (!(npc->flags & 0x20) && obj->healthInfo->health == -1) {
                obj->flags23 |= 2;
            }
            ref = gameObjectsInfo[obj->id].refId;
            hostage = gameObjectsInfo[ref].runtime;
            if (ref >= 0 && hostage != NULL && hostage->npc != NULL) {
                npcHostageSetActivation(obj, 0);
            }
        }
        if (D_8010F950 != NULL && GOBJ_TYPE(id) == GOBJ_TYPE_COP) {
            D_8010F950(id);
        }
    }
    if (obj->id == D_80110138) {
        D_80110138 = -1;
    }
    if (obj->id == D_8011013C) {
        D_8011013C = -1;
    }
    NPCGetNumActive();
}

void NPCRestore(s32 id, u8 awareness, s32 flags, u8 point) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    NPC *npc = NPC_OF(obj);
    VECTOR32 pos;
    s32 start;
    s32 gen;

    if (!(flags & 0x40)) {
        obj->flags23 |= 2;
        return;
    }
    obj->flags23 &= ~2;
    if (flags & 0x80) {
        obj->flags0 |= 0x20;
    } else {
        obj->flags0 &= ~0x20;
    }
    if ((flags & 0x20) && id >= 0) {
        gen = (flags >> 8) & 0xFF;
        if (gen != 0xFF) {
            NPCCreateGenerator(&generatorInfo[gen], id, (flags >> 16) & 0xFF, (u32)flags >> 24);
        }
    }
    LevelGetPathInfo(id, &start, &D_80110134);
    if (point != 0) {
        pos.vx = D_80110134[point].x;
        pos.vy = D_80110134[point].y;
        pos.vz = D_80110134[point].z;
        gameObjectsInfo[id].matrix.t[0] = pos.vx;
        gameObjectsInfo[id].matrix.t[1] = pos.vy;
        gameObjectsInfo[id].matrix.t[2] = pos.vz;
        EGSFrameSetPosition(obj->anim->frame, 0, &pos);
        EGSFrameUpdateHierarchy(obj->anim->frame);
    }
    npc->awareness = awareness;
    npc->curPoint = point;
    npc->nextPoint = point;
    npc->flags = (npc->flags & ~0x3F) | (flags & 0x3F);
    if (awareness) {
        npc->lastSeenPos.vx = gObjPlayer->anim->frame->pos.vx;
        npc->lastSeenPos.vy = gObjPlayer->anim->frame->pos.vy;
        npc->lastSeenPos.vz = gObjPlayer->anim->frame->pos.vz;
        npc->lastSeenPos.vy = -npc->lastSeenPos.vy;
        npc->awareness = 2;
    }
    npc->unk30 = gExeTick;
}

s32 NPCAddGobjToRadar(s32 id) {
    s32 slot = -1;
    s32 i;
    Gobj *obj = gameObjectsInfo[id].runtime;

    for (i = 0; i < NPC_SLOTS; i++) {
        if (npcList[i] < 0) {
            slot = i;
            break;
        }
    }
    npcList[slot] = id;
    obj->target = &D_8011D240[slot];
    obj->move = &D_8011D750[slot];
    return slot;
}

void NPCDeleteGobjFromRadar(s32 id, s32 slot) {
    npcList[slot] = -1;
}

void NPCCopDeactivateWashingtonCallback(s16 id) {
    s32 ref = gameObjectsInfo[id].refId;

    if (ref != -1 && ref != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[ref].modelIndex].type == GOBJ_TYPE_BOMB
        && !(gameObjectsInfo[ref].runtime->flags1 & 0x80)) {
        DamageDestroyImmediately(-1, id);
    }
}

void func_80065D90(Gobj *arg) {
    Gobj *obj = arg;
    VECTOR32 me;
    VECTOR32 pc;
    u32 dist;

    if (obj->healthInfo->health > 0) {
        pc = gObjPlayer->move->pos;
        me = obj->move->pos;
        EGSGeometry_GetFuzzyXZDistancePoint2Point(&me, &pc, &dist);
        if (dist < 0xF00) {
            SysExeTickQueueTimer(func_80065D90, 2, arg);
            return;
        }
        func_80065F5C(obj->id, obj->id);
    }
}

void NPCHazmatStart(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    obj->flags23 &= ~2;
    GSActivateGameObject(obj, *gCamera, 0);
    obj->flags23 |= 1;
    SysExeTickQueueTimer(func_80065D90, 10, obj);
}

void NPCHazmatStop(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    obj->flags23 = (obj->flags23 & ~1) | 8;
    SysExeTickQueueTimer(func_80065D90, 0, obj);
}

void func_80065F5C(s32 id, s32 by) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    NPCHazmatStop(id);
    if (obj->healthInfo->health > 0) {
        DamageDestroyImmediately(id, by);
    }
}

void NPCCopDamagedCallback(s16 id, s16 by) {
    s32 type = GOBJ_TYPE(id);

    if (!D_80110160 && (type == GOBJ_TYPE_COP || type == 0x4C)) {
        D_80110160 = 1;
        SysPlayXA(0x7F, (s32)gameObjectsInfo[id].runtime, 0);
    }
}

void NPCCopKilledWashington(s16 id, s16 by, s32 refId, s32 objective, s32 objective2) {
    if (by == gPlayerOffset) {
        if (gObjectives == NULL || !(gObjectives->unk20 & (1 << objective))) {
            if (++gLevelInnocentsKilled == 1) {
                if (refId != -1 && !(gameObjectsInfo[refId].runtime->flags1 & 0x80) && refId != GOBJ_ID_666
                    && gameObjectsModelInfo[gameObjectsInfo[refId].modelIndex].type == GOBJ_TYPE_BOMB) {
                    func_80065F5C(id, by);
                    if (gObjectives != NULL) {
                        gObjectives->unk14 |= 1 << objective2;
                    }
                }
                if (SysSoundIsXAPlaying()) {
                    SysStopXA();
                }
                LevelMissionFailed((s16)objective, 0x80, 0);
            }
        }
    } else if (refId != -1) {
        if (!(gameObjectsInfo[refId].runtime->flags1 & 0x80) && refId != GOBJ_ID_666
            && gameObjectsModelInfo[gameObjectsInfo[refId].modelIndex].type == GOBJ_TYPE_BOMB
            && (gObjectives == NULL || !(gObjectives->unk14 & (1 << objective2)))) {
            if (SysSoundIsXAPlaying()) {
                SysStopXA();
            }
            func_80065F5C(id, by);
            LevelMissionFailed((s16)objective2, 0x7A, 1);
        }
    }
}

void NPCDamagedByPC(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;
    s32 type;

    if (gSysInfo.level != 14) {
        if (NPC_OF(obj)->flags & 1) {
            type = GOBJ_TYPE(id);
            if (type != GOBJ_TYPE_COP && type != 0x4C && type != 0x5C) {
                D_80110164 = 1;
            } else if (gSysInfo.level == 0 && !copKilledByPCFlag && obj->healthInfo->health <= 0) {
                copKilledByPCFlag = 1;
            }
        }
    } else {
        NPCDamaged(id, gPlayerOffset);
    }
    NPCSetAwarenessLevel(obj, 2, 0, 1);
}

s32 NPCGetCamera(s32 id) {
    s32 ret;
    s32 cam = ObjectInvisibleMatchToRefID(id, 0x77);

    ret = -1;
    if (id != cam) {
        ret = cam;
    }
    return ret;
}

void *NPCCombatModeCallback(void (*cb)()) {
    void *old = D_8010F94C;
    D_8010F94C = cb;
    return old;
}

void *NPCSetCopDeactivateCallback(void (*cb)()) {
    void *old = D_8010F950;
    D_8010F950 = (void (*)(s16))cb;
    return old;
}

void *NPCSetInteractEnteredRangeCallback(void (*cb)()) {
    void *old = D_8011011C;
    D_8011011C = cb;
    return old;
}

void *NPCSetInteractExitedRangeCallback(void (*cb)()) {
    void *old = D_80110120;
    D_80110120 = cb;
    return old;
}

void *NPCSetInteractOtherCallback(void (*cb)()) {
    void *old = D_80110124;
    D_80110124 = cb;
    return old;
}

void func_800664F8(Gobj *obj) {
    VECTOR32 pos;
    VECTOR32 pc;
    s32 dist;
    u8 was;

    if (!(obj->flags0 & 0x20)) {
        pc = gObjPlayer->move->unk10C;
        pos.vx = obj->anim->frame->pos.vx;
        pos.vy = obj->anim->frame->pos.vy;
        pos.vz = obj->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &pc, &dist);
        was = D_80110128[NPC_OF(obj)->slot];
        D_80110128[NPC_OF(obj)->slot] = dist < 0xA0;
        if (dist < 0x140) {
            NPC_OF(obj)->flags |= 0x1000000;
        } else {
            NPC_OF(obj)->flags &= ~0x1000000;
        }
        if (was != D_80110128[NPC_OF(obj)->slot]) {
            if (was) {
                if (D_80110120 != NULL) {
                    D_80110120(obj->id);
                }
                PickupRemoveUseMsg(obj->id);
                SysPostMessage(0x19, 3, obj->id, gPlayerOffset, 0, 0, 0, 0);
            } else {
                if (D_8011011C != NULL) {
                    D_8011011C(obj->id);
                }
                PickupAddUseMsg(obj->id, 3);
                SysPostMessage(0x14, 4, obj->id, gPlayerOffset, 0, 0, 0, 0);
            }
        } else if (D_80110124 != NULL) {
            D_80110124(obj->id);
        }
    }
}

DamagePacket *func_80066714(void) {
    s32 i;
    s32 idx = D_80110184;
    DamagePacket *p = NULL;

    if (D_80110180 == DAMAGE_PACKETS) {
        p = &D_8011E8E8[0];
        D_8011E8E8[0].next = DAMAGE_PACKETS;
        D_80110180 = 0;
        D_80110184 = 1;
    } else {
        for (i = 0; i < DAMAGE_PACKETS; i++) {
            idx++;
            if (idx >= DAMAGE_PACKETS) {
                idx = 0;
            }
            if (!D_8011E8E8[idx].inUse) {
                p = &D_8011E8E8[idx];
                D_80110184 = idx;
                p->next = D_80110180;
                D_80110180 = idx;
                break;
            }
        }
    }
    return p;
}

void func_800667C4(DamagePacket *p, s32 prev) {
    if (prev != DAMAGE_PACKETS) {
        D_8011E8E8[prev].next = p->next;
    } else {
        D_80110180 = p->next;
    }
}
