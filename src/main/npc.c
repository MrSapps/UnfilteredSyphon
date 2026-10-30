#include "common.h"
#include "system.h"
/*
 * level.h's prototypes for these are off: NPCSetCopDeactivateCallback returns the
 * previous callback, NPCHazmatStart and DamageDestroyImmediately take s32 ids.
 */
#define NPCSetCopDeactivateCallback NPCSetCopDeactivateCallback_level_h
#define NPCHazmatStart NPCHazmatStart_level_h
#define DamageDestroyImmediately DamageDestroyImmediately_level_h
#include "level.h"
#undef NPCSetCopDeactivateCallback
#undef NPCHazmatStart
#undef DamageDestroyImmediately
void DamageDestroyImmediately(s32 by, s32 id);
#include "gameobj.h"
#include "gobj.h"
#include "game.h"

/* npc.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F94C)
SDATA_LOCAL(D_8010F950)
SDATA_LOCAL(D_8010F958)
SDATA_LOCAL(D_8011011C)
SDATA_LOCAL(D_80110120)
SDATA_LOCAL(D_80110124)
SDATA_LOCAL(D_80110168)
SDATA_LOCAL(D_80110180)
SDATA_LOCAL(D_80110184)
SDATA_LOCAL(npcNumGenerators)
SDATA_LOCAL(D_8010F96C)
SDATA_LOCAL(D_80110130)
SDATA_LOCAL(D_80110134)
SDATA_LOCAL(D_80110138)
SDATA_LOCAL(D_8011013C)
SDATA_LOCAL(D_80110140)
SDATA_LOCAL(D_80110144)
SDATA_LOCAL(D_80110148)
SDATA_LOCAL(D_8011014C)
SDATA_LOCAL(D_80110150)
SDATA_LOCAL(D_80110154)
SDATA_LOCAL(D_80110158)
SDATA_LOCAL(D_8011015C)
SDATA_LOCAL(D_80110160)
SDATA_LOCAL(D_80110164)
SDATA_LOCAL(NPCEndActivationFlag)
SDATA_LOCAL(NPCNumActive)
SDATA_LOCAL(NPCNumActiveTick)
SDATA_LOCAL(NPCNumEnemyAlive)
SDATA_LOCAL(NPCPopRemoveID)
SDATA_LOCAL(copKilledByPCFlag)
SDATA_LOCAL(npcAlertByScream)
SDATA_LOCAL(npcGrenadeLandedCheck)
SDATA_LOCAL(npcVisibilityDistance)
SDATA_LOCAL(pointActionMoveMode)
SDATA_LOCAL(getNPCIndex)
SDATA_LOCAL(gNpcAllOnRadarFlag)
SDATA_LOCAL(D_8010F970)
SDATA_LOCAL(npcPCInvisibleFlag)

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
    /* 0x24 */ u8 unk24[4];
    /* 0x28 */ s16 unk28; /* sound radius threshold */
    /* 0x2A */ u8 unk2A[2];
    /* 0x2C */ s16 unk2C;
    /* 0x2E */ u8 unk2E[2];
    /* 0x30 */ s16 unk30; /* tick of the last restore */
    /* 0x32 */ s16 unk32;
    /* 0x34 */ s16 unk34; /* sound radius */
    /* 0x36 */ u8 unk36[2];
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

extern void (*D_8010F94C)();      /* combat mode callback */
extern void (*D_8010F950)(s16 id); /* cop deactivate callback */
extern s32 D_8010F958;            /* NPCs may shoot */
extern void (*D_8011011C)();      /* interact entered range callback */
extern void (*D_80110120)();      /* interact exited range callback */
extern void (*D_80110124)();      /* interact other callback */
extern s32 D_80110168;            /* enabled generator count */
extern u8 D_80110180;             /* damage packet list head */
extern u8 D_80110184;             /* next damage packet to try */
extern u16 npcNumGenerators;
extern s32 D_8010F96C;            /* last generator NPC handed out */
extern s32 D_80110130;
extern PathPoint *D_80110134;     /* current NPC's path */
extern s16 D_80110138;
extern s16 D_8011013C;
extern s16 D_80110140;
extern s16 D_80110144;
extern s16 D_80110148;            /* first generator NPC object id */
extern s16 D_8011014C;
extern s16 D_80110150;
extern s16 D_80110154;
extern u8 D_80110158;
extern u8 D_8011015C;
extern u8 D_80110160;             /* cop damaged XA played */
extern u8 D_80110164;
extern u8 NPCEndActivationFlag;
extern u8 NPCNumActive;
extern s32 NPCNumActiveTick;
extern u8 NPCNumEnemyAlive;
extern s16 NPCPopRemoveID;
extern u8 copKilledByPCFlag;
extern u8 npcAlertByScream;
extern u8 npcGrenadeLandedCheck;
extern s32 npcVisibilityDistance;
extern s16 pointActionMoveMode;
extern s16 getNPCIndex;           /* NPCGetNextID position in npcList */
extern s32 gNpcAllOnRadarFlag;
extern s32 D_8010F970;            /* last NPC slot given a shot */
extern s32 npcPCInvisibleFlag;

/* Level data header, see level.c */
typedef struct NPCLevelData {
    /* 0x00 */ u8 unk00[0x52];
    /* 0x52 */ s16 maxActiveNPCs;
} NPCLevelData;
extern NPCLevelData *gLevData;
extern VECTOR32 D_8011D1F0;       /* getPointActionVector result */
extern VECTOR32 D_8011D200;       /* NPCFaceDirection turn step */
extern Gobj *mGrenadeGobj[];
extern u8 gPlayerOnFireFlag;
extern u16 D_80106438[];          /* NPC health per level */
extern NPCPathCtrl D_8011E1A0[NPC_SLOTS];
extern s32 D_8011E890[NPC_SLOTS];
extern u32 D_80105F58[][8];       /* per object type info, 0x20 bytes each */
extern VECTOR32 D_8011D220;       /* generator spawn point */
extern VECTOR32 D_8011D230;       /* ... on screen */
extern s32 D_801457A4;

/* What a generated NPC was made from (genData[id - first generator NPC]) */
typedef struct NPCGenRecord {
    u8 gen;
    s8 path;
    s8 variant;
} NPCGenRecord;
extern NPCGenRecord genData[];

extern u8 D_80110128[NPC_SLOTS];
extern u8 gNPCLevelAlertFlag;
extern TargetInfo D_8011D240[NPC_SLOTS];
extern MoveInfo D_8011D750[NPC_SLOTS];
extern PointAction D_80106474[];
extern void **gCamera;
extern u8 gSniperCam[];
extern s32 gSniperMode;
extern s32 gCurrentWeapon;
extern LOSHit *losHitsSorted[];
extern u32 losSortedCount;

extern s32 npcList[NPC_SLOTS];    /* object id per slot, -1 when free */
extern NPCGenerator *generatorInfo;
extern DamagePacket D_8011E8E8[DAMAGE_PACKETS];
extern VECTOR32 D_8011E8C8;       /* LOS override position (pad is the active flag) */
#define D_8011E8D4 D_8011E8C8.pad
extern VECTOR32 D_8011E8A8;       /* player position as seen by NPCs */
extern VECTOR32 D_8011E8B8;       /* current NPC's position (y up) */
extern VECTOR32 grenadePos;
extern char D_8010F964[];         /* "s/npc.c" (.sdata) */
extern u8 iAnimationArray[];
extern NPCXALines D_801067C0[][6];
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
s32 rcos(s32 a);
s32 rsin(s32 a);
void MovementGobjRegisterCharacterMovement(Gobj *obj, s32 arg1, s32 arg2);
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
void InterfaceAddNPC(s16 id);
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

#ifdef NON_MATCHING
/* target hoists 0x1000000 into s1 for the hostage loop (real loop, not goto) */
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
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInitHostageTaker);
#endif

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

#ifdef NON_MATCHING
/* gameObjectsInfo base hoisted out of the hostage chain loop in the target; goto loop doesn't */
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
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickHostageTarget);
#endif

void NPCShootHostage(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    NPC_OF(obj)->flags |= 1;
    NPCSetAwarenessLevel(obj, 2, 1, 0);
    NPC_OF(obj)->flags &= ~0x80000;
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickNPCTarget);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGobjInit);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPointIQ);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCViewConeCheck);

void NPCSetShooting(s32 on) {
    D_8010F958 = on;
}

s32 NPCDebugGetNoDamageFlag(void) {
    return D_8010F958 == 0;
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetMoveMode);

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
                if (!((D_80105F58[obj->id][0] >> 8) & 7)) {
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
/* regalloc + i+row addu order (same as getPointActionLink) */
VECTOR32 *getPointActionVector(PathPoint *points, Gobj *obj) {
    VECTOR32 *v = NULL;
    NPC *npc = NPC_OF(obj);
    s32 i;
    s32 link;

    for (i = 0; i < 3; i++) {
        link = points[npc->curPoint].links[i];
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

#ifdef NON_MATCHING
/* cur+i addu operand order */
s32 getPointActionLink(PathPoint *points, Gobj *obj) {
    s32 found = -1;
    s32 link = -1;
    s32 i;

    for (i = 0; i < 3; i++) {
        if (points[NPC_OF(obj)->curPoint].links[i] >= 0) {
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
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", getPointActionLink);
#endif

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

#ifdef NON_MATCHING
/* i+row addu operand order */
s32 NPCGetGrenadeLink(Gobj *obj) {
    s32 found = -1;
    s32 i;
    s32 link;
    PathPoint *pt;
    s32 dist;
    NPC *npc = NPC_OF(obj);

    for (i = 0; i < 3; i++) {
        link = D_80110134[npc->curPoint].links[i];
        if (link >= 0) {
            pt = &D_80110134[link];
            if (((pt->flags & 0xF00) >> 8) == 1) {
                grenadePos.vx = pt->x;
                grenadePos.vy = pt->y;
                grenadePos.vz = pt->z;
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
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetGrenadeLink);
#endif

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCThrowGrenade);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", initPointAction);

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
                npc->unk41 = !((D_80105F58[obj->id][0] >> 8) & 7) ? 1 : 3;
            }
            npc->unk4A = 0;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCIQ);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCIQEveryFrame);

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

#ifdef NON_MATCHING
/* first NPC scan: i/ptr regs swapped */
void NPCGetNumActive(void) {
    s32 i;
    s32 id;
    Gobj *obj;

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
}
#else
INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetNumActive);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCheckGenerators);

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
/* first NPC scan: i/ptr regs swapped (same as NPCGetNumActive) */
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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCKill);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickAimer);

void NPCAim(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCAimAll);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosUpdateOtherStuff);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCTakeShot);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosCheck);

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

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCActivate);

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
