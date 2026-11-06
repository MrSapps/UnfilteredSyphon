#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "egsmath.h"

#define abs(x) __builtin_abs(x)

/* player.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_8001040C[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

u16 mUseInfo;            /* (common, $gp) message id of the pending use action, 0 for none */
SDATA_LOCAL(mUseInfo)
extern s16 D_801103BE;   /* object the pending use action targets, -1 for none */
extern s32 gPlayerOffset;
/* Player flashlight (EGS display light); the first word is nonzero while it is on */
typedef struct PlayerFlashlight {
    /* 0x00 */ s32 on;
    /* 0x04 */ u8 unk04[0x1C];
} PlayerFlashlight;
extern PlayerFlashlight gPlayerFlashlight;
extern u8 mPlayerFlashlightPS[];

/* Face the feet collision detector stands on */
typedef struct PlayerFloorFace {
    /* 0x00 */ s32 kind; /* 4: quad */
    /* 0x04 */ u8 *data; /* the face's material byte precedes it */
} PlayerFloorFace;

typedef struct PlayerFeetState {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 unk01;
    /* 0x02 */ u8 unk02;
    /* 0x03 */ u8 unk03;
} PlayerFeetState;

/* Feet collision detector (MoveInfo.coll) */
typedef struct PlayerFeet {
    /* 0x00 */ s32 mode;
    /* 0x04 */ u8 unk04[8];
    /* 0x0C */ VECTOR32 vel;
    /* 0x1C */ PlayerFeetState state;
    /* 0x20 */ u8 unk20[8];
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ u8 unk34[4];
    /* 0x38 */ PlayerFloorFace *floor;
} PlayerFeet;

/* Collision physics (MoveInfo.collFlags pointer at 0x1A8) */
typedef struct PlayerCollPhys {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u8 unk08[0x78];
    /* 0x80 */ s32 unk80;
    /* 0x84 */ u8 unk84[0xC];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ u8 unk94[0x18];
    /* 0xAC */ s32 unkAC;
    /* 0xB0 */ s32 unkB0;
} PlayerCollPhys;


/* Collision contact (0x88 bytes) */
typedef struct PlayerContact {
    /* 0x00 */ u8 unk00[0x88];
} PlayerContact;

/* Parameters of CollisionDetectorCreate */
typedef struct CollisionDetectorDesc {
    /* 0x00 */ struct EGSFrame *frame;
    /* 0x04 */ VECTOR32 offset;
    /* 0x14 */ s32 radius;
    /* 0x18 */ s32 minY;
    /* 0x1C */ s32 maxY;
    /* 0x20 */ s32 unk30;
    /* 0x24 */ s32 unk34;
    /* 0x28 */ s32 unk38;
    /* 0x2C */ s32 unk3C;
    /* 0x30 */ s32 unk40;
    /* 0x34 */ s32 unk44;
    /* 0x38 */ PlayerContact **contacts;
} CollisionDetectorDesc;

extern PlayerContact *D_80113D88[15]; /* body contacts */
extern PlayerContact *D_80113DC4[15]; /* head contacts */
extern PlayerContact *D_80113E00[15]; /* feet contacts */
extern PlayerContact D_80113E3C[15];
extern PlayerContact D_80114634[15];
extern PlayerContact D_80114E2C[15];
static s32 D_8010FF80; /* .sbss: body collision detector */
static s32 D_8010FF84; /* .sbss: head collision detector */
s32 mPlayerFeetCollisionDetector_Left; /* commons, $gp */
s32 mPlayerFeetCollisionDetector_Right;

void CollisionGobjInit(Gobj *gobj, s32 *kind);
s32 func_80025F18(Gobj *gobj);

/* Level data header: flashlight colour */
typedef struct PlayerLevData {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flashlightR;
    /* 0x09 */ u8 flashlightG;
    /* 0x0A */ u8 flashlightB;
} PlayerLevData;

extern PlayerLevData *gLevData;
void *mGabeAnim;      /* common, $gp */
SDATA_LOCAL(mGabeAnim)
Gobj *mGrenadeGobj[2]; /* common, $gp */
SDATA_LOCAL(mGrenadeGobj)
extern struct EGSFrame *mGrenadeFrame[2];
/* Fire button state */
typedef struct PlayerShootState {
    /* 0x0 */ u8 released;  /* fire released since the last shot */
    /* 0x4 */ s32 burst;    /* shots in the current burst; +100 once the burst is reported */
    /* 0x8 */ u32 lastTick; /* tick of the last shot (grenade: throw start) */
} PlayerShootState;
extern PlayerShootState mShootInfo;
s32 gPlayerNumConsecutiveHits; /* common, $gp */
SDATA_LOCAL(gPlayerNumConsecutiveHits)
extern u8 gPlayerOnFireFlag;

void LevelGetHMDModel(char *name, void **out);
void LevelGetModel(char *name, void **out);
void LevelGetGameObjectMatrix(s16 id, GOBJ_MATRIX *m);
s32 EGSAnimHMDCreate(void *model, void *anims, s32 unused, u8 shaded, GameObjectAnim **out, s32 arg5);
void EGSAnimSetReferenceAnim(GameObjectAnim *anim);
void EGSAnimSimpleCreate(void *model, struct EGSFrame *frame, s32 flags, GameObjectAnim **out);
void EGSDisplayAddAnim(void *display, void *anim);
void CobjAddBulletCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3);
void EGSDisplayPointSourceInit(void *ps);
void EGSDisplayPointSourceFrame(void *ps, struct EGSFrame *frame);
void EGSDisplayPointSourceParams(void *ps, s32 radius, s32 color);
void EGSDisplayFlashlightInit(void *light);
void EGSDisplayFlashlightFrame(void *light, struct EGSFrame *frame, s32 arg2);
void EGSDisplayFlashlightParams(void *light, u8 r, u8 g, u8 b);
void MovementGobjInit(Gobj *gobj, GOBJ_MATRIX *m, s32 arg2, s32 arg3, s32 arg4);
void MovementGobjInitCAS(Gobj *gobj, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
void PhysicsGobjBasicInit(Gobj *gobj, s32 arg1, s32 arg2);
void CollisionGobjBasicInit(Gobj *gobj, s32 a, s32 b, s32 c, s32 d, s32 e);
void EGSFrameCreate(struct EGSFrame **out);
void CollisionDetectorCreate(s32 *out, CollisionDetectorDesc *desc, Gobj *gobj);
s32 CollisionDetectorAddToList(s32 detector, s32 index, s32 *list);
void CollisionGobjSetPushDetectorList(Gobj *gobj, s32 list);
void CollisionGobjSetPushDetector(Gobj *gobj, s32 detector);

/* MoveInfo fields used here */
typedef struct PlayerMoveInfo {
    /* 0x000 */ u8 unk000[0x20];
    /* 0x020 */ VECTOR32 vel;
    /* 0x030 */ u8 unk030[0x10];
    /* 0x040 */ VECTOR32 launchPos; /* read by MovementAddGobj */
    /* 0x050 */ VECTOR32 step;      /* displacement per tick, 20.12 */
    /* 0x060 */ VECTOR32 launchVel;
    /* 0x070 */ VECTOR32 impulse;
    /* 0x080 */ u8 unk080[0x84];
    /* 0x104 */ s32 scale; /* 0x1000 = 1.0 */
    /* 0x108 */ u8 unk108[8];
    /* 0x110 */ s32 unk110;
    /* 0x114 */ u8 unk114[0x1C];
    /* 0x130 */ s32 restitution;
    /* 0x134 */ s32 friction;
    /* 0x138 */ u8 unk138[0x18];
    /* 0x150 */ VECTOR32 groundPos;
    /* 0x160 */ u8 unk160[0x48];
    /* 0x1A8 */ PlayerCollPhys *phys;
    /* 0x1AC */ u8 unk1AC[4];
    /* 0x1B0 */ PlayerFeet *feet;
} PlayerMoveInfo;

typedef struct PlayerAnimFrames {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ void **frames;
} PlayerAnimFrames;

extern u8 gSoundFootstep[];
s32 mFootFx; /* common, $gp */
SDATA_LOCAL(mFootFx)

s32 SpriteFootFxTextureGet(s32 foot);
void EGSDisplayRegisterFootprint(void *frame, s32 tex, VECTOR32 *pos);
void SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);

u8 mShootReqFlag; /* (common, $gp) 1: shot requested, 2: shot queued, 3: shot queued unrequested */
SDATA_LOCAL(mShootReqFlag)
extern s32 gCurrentWeapon;
extern u8 iAnimationArray[];

/* Only the field used here; see weapon.c for the full WeaponInfo. */
typedef struct WeaponInfo {
    /* 0x00 */ u8 unk00[0xA];
    /* 0x0A */ u8 startAmmo;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ u32 clipShift : 3;
    /* 0x0C */ u32 holdType : 3; /* selects the draw animation */
    /* 0x0C */ u32 unk0C_6 : 2;
    /* 0x0C */ u32 fireMode : 3; /* 0: single shot, 1: burst, 2: automatic */
    /* 0x0C */ u32 unk0C_11 : 21;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 deathAction;
    /* 0x14 */ u32 unk14_0 : 6;
    /* 0x14 */ u32 damageNear : 10;
    /* 0x14 */ u32 damageFar : 10;
    /* 0x14 */ u32 fireDelay : 5; /* ticks between shots */
    /* 0x14 */ u32 unk14_31 : 1;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ u32 flashColor;
} WeaponInfo; /* size 0x20 */
extern WeaponInfo gWeaponInfo[];

#define WEAPON_TASER 0xE

/* GameObjectInfo.flags24 viewed as the NPC weapon word: low byte = weapon, 0x3000 = default kind */
#define GOBJINFO_WEAPON(info) (*(u8 *)&(info)->flags24)
#define GOBJINFO_WORD24(info) (*(u32 *)&(info)->flags24)
/* Weapon an object holds (no player special case), as one expression (see weapon.c) */
#define GOBJINFO_NPC_WEAPON(info)                                                            \
    (GOBJINFO_WEAPON(info) != 0                ? GOBJINFO_WEAPON(info)                        \
     : (GOBJINFO_WORD24(info) & 0x3000) != 0x1000 ? ((GOBJINFO_WORD24(info) & 0x3000) == 0x2000 ? 0x14 : 0) \
                                               : 0x13)

static inline s32 WeaponOfGobj(Gobj *gobj) {
    s32 weapon;
    s32 kind;

    if (gobj->id != gPlayerOffset) {
        weapon = GOBJINFO_WEAPON(&gameObjectsInfo[gobj->id]);
        if (weapon == 0) {
            kind = GOBJINFO_WORD24(&gameObjectsInfo[gobj->id]) & 0x3000;
            if (kind != 0x1000) {
                weapon = kind == 0x2000 ? 0x14 : 0;
            } else {
                weapon = 0x13;
            }
        }
    } else {
        weapon = gCurrentWeapon;
    }
    return weapon;
}

/* Whether the object's weapon is not the taser (NPCs without a weapon byte never have one). */
#define GOBJ_WEAPON_NOT_TASER(g)                                                                       \
    ((g)->id != gPlayerOffset ? (GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) == 0 ||                     \
                                 GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) != WEAPON_TASER)            \
                              : gCurrentWeapon != WEAPON_TASER)

extern s32 gCASJoystickGobjID;
extern s32 gSniperMode;

void GameCameraSetModeHang(s32 hang, s32 arg1, s32 arg2);
u8 WeaponReload(s32 weapon);
void CASRequestAction(s32 id, s32 action);
void WeaponAdd(s32 weapon, s32 rounds);
void PlayerCASInit(Gobj *gobj, s32 arg1);
void WeaponSelect(Gobj *gobj, s32 weapon, s32 arg2);
void WeaponReplace(Gobj *gobj);
void InterfaceWeaponChange(void);
void SavePlayerPreserve(void);
void LevelLoadHMD(char **files, void *anims, s32 arg2);
void EGSGeometryFIX_VectorGetAngleFromXZPlane(VECTOR32 *v, void *out);
int ratan2(int y, int x);
void CollisionGobjReleaseModeGrabCAS(s16 id, s32 arg1);
void iWeaponShootCB(void);
void SysSoundPlayRandXA(void *list);
void PlayerFootCB(s16 id, s32 arg1, s32 arg2, u8 right);
void ParticleFootSplash(u8 right);
void func_800265FC(Gobj *gobj, u32 action);
void SniperSetMode(s32 mode);
void WeaponTaser(Gobj *gobj, s32 arg1);
void InterfaceWeaponSelect(s32 arg0, s32 arg1);
void EGSDisplayFlashlightOn(void *light);
void EGSDisplayFlashlightOff(void *light);
void EGSDisplayPointSourceOn(void *ps);
void EGSDisplayPointSourceOff(void *ps);
void UseMsgUpdate4FlashlightChange(void);
struct GameCamSlot;
void GameCameraSetModeFixed(s32 on, s32 arg1, void *pos, struct GameCamSlot *slot, s32 arg4);
u8 GameCameraCheckMode(s32 mode);
void GameCameraRequestGetBehind(void);
u8 SysSoundIsXAQueued(void);
void LevelRemoveContactMeiXingMsgForRadio(void);
void JoystickDeactivate(void);
void JoystickDirInputDeactivate(s32 deactivate, u8 arg1);
u8 PickupObjectIsNotMaxed(s32 objId);
u8 PickupObjectTake(u8 arg0);
void ObjectTeleportPlayer(s16 id, s32 sector);
s32 ObjectInvisibleMatchToRefID(s16 id, s16 type);
extern struct GameCamSlot D_80115624; /* camera slot used while operating an object */
void MovementGobjTeleport_ByXZLookAtPoint(Gobj *gobj, VECTOR32 *pos, VECTOR32 *lookAt, s32 arg3);
VECTOR32 *SniperGetTarget(s32 *outId);
void WeaponThrowGrenade(Gobj *gobj, Gobj *target, s32 power, VECTOR32 *aim);

void iWeaponGrenadeCB(Gobj *grenade);
void EGSDisplayDeleteAnim(void *display, GameObjectAnim *anim);
void DamageExplosion(s32 id, VECTOR32 *pos, u8 grenade);
void DamageGasCloud(s32 id, VECTOR32 *pos, s8 active);
void MovementRemoveGobj(Gobj *gobj);
void npcGrenadeLanded(u8 npc);
void ParticleFuse(VECTOR32 *pos);
extern s16 npcGrenadeThrowerID;

/* Line of sight request (see los.c) */
typedef struct LOSRequest {
    /* 0x00 */ VECTOR32 *from;
    /* 0x04 */ VECTOR32 *to;
    /* 0x08 */ Gobj *fromObj;
    /* 0x0C */ Gobj *toObj;
    /* 0x10 */ VECTOR32 *dir;
    /* 0x14 */ void *angles;
    /* 0x18 */ s32 distance;
    /* 0x1C */ u8 *noHit;
    /* 0x20 */ VECTOR32 *hitPos;
    /* 0x24 */ VECTOR32 *hitNormal;
    /* 0x28 */ void *hitInfo;
    /* 0x2C */ u8 *windowHit;
    /* 0x30 */ void *windowInfo;
} LOSRequest;

/* Owner of a line of sight hit face */
typedef struct LOSHitOwnerView {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
} LOSHitOwnerView;

/* Line of sight hit (see cobj.c) */
typedef struct LOSHitView {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 type; /* 0: terrain, 1: game object, 2: window */
    /* 0x0C */ u8 unk0C[0x3C];
    /* 0x48 */ LOSHitOwnerView *owner;
} LOSHitView;

u8 _LOSPoint2PointCollide(LOSRequest *req);
extern s32 losWindowCount;
extern Gobj *losWindowArray[];
extern LOSHitView *losHitsSorted[];
void MovementAddGobj(Gobj *gobj);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, void *arg);
void EGSMath_VectorModulus(VECTOR32 *v, s32 *out);

void WeaponGetRounds(s32 weapon, s32 *rounds, s32 *clips);
void WeaponSubRound(Gobj *gobj);
s16 WeaponGetDeathAction(s32 weapon);
void WeaponDoFiredCallback(s32 id, s32 weapon);
s32 UseLevelSpecificObject(void);
s32 UseKey(void);
void SniperShoot(void);
s32 TargetShoot(Gobj *gobj, s32 arg1, s32 arg2);
void EGSAnimGunFlash2(GameObjectAnim *anim, s32 time, u32 color);
u8 GameSysIsTime(s32 since, u32 ticks);
void DamageGOBJ(s16 from, s16 by, s16 to, s16 amount, s16 type);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void func_80025EA8(u8 on);
extern void (*gHitNPCCallback)(s16 id, s32 bones);
extern s32 particleTaserState;

extern VECTOR32 grenadePos; /* NPC grenade target */

/* .data; cc1 emits each table's literals right after it, in reverse, the short
 * ones into .sdata and the rest into .rdata (they start .rdata and .sdata) */
char *gGabeAnimFiles[156] = {
    "CLIMBA.HAN", "CW0.LWR", "CW0.UPR", "CW1.UPR", "CW1AIM.UPR", "CW2.UPR",
    "CW2AIM.UPR", "CW2PSG.UPR", "CW1RLD.UPR", "CW2RLD.UPR", "CWDRW0.UPR", "CWDRW1.UPR",
    "CWDRW2.UPR", "CWLOB.UPR", "CWRHL0.UPR", "CWRHL1.UPR", "CWRHL2.UPR", "CWSTP2.HAN",
    "DETH11A.HAN", "DETH11B.HAN", "DETH11C.HAN", "DETH11D.HAN", "DETH12A.HAN", "DETH12B.HAN",
    "DETH12C.HAN", "DETH12D.HAN", "DETH1A.HAN", "DETH1B.HAN", "DETH1C.HAN", "DETH1D.HAN",
    "DETH6A.HAN", "DETH6B.HAN", "DETH6C.HAN", "DETH6D.HAN", "DETHFIR0.HAN", "DISARM.UPR",
    "EXDETH00.HAN", "EXDTH12C.HAN", "FALL1.HAN", "FALL2.HAN", "FIREDANC.HAN", "FLEE0.HAN",
    "HLDDRPA.HAN", "HLDDRPB.HAN", "IDLE13.HAN", "IDLE2C.UPR", "IDLE3A.UPR", "IDLE5C.UPR",
    "IDLE6B.UPR", "IDLE7C.UPR", "JACK.HAN", "JP1.HAN", "KIKDR.HAN", "KN0.LWR",
    "KN1AIM.UPR", "KN2AIM.UPR", "KN2FIRE.UPR", "KN1RLD.UPR", "KN2RLD.UPR", "KNCW0.HAN",
    "KNCW1.HAN", "KNCW2.HAN", "KNDRW0.UPR", "KNDRW1.UPR", "KNDRW2.UPR", "KNLOB.UPR",
    "KNPKL0.UPR", "KNPKL1.UPR", "KNPKL2.UPR", "KNRHL0.UPR", "KNRHL1.UPR", "KNRHL2.UPR",
    "KNROL0.HAN", "KNROL1.HAN", "KNROL2.HAN", "KNST0.HAN", "KNST2.HAN", "KNTNA.LWR",
    "KNTNL.LWR", "KNTNR.LWR", "KNTOSU.HAN", "PKH0.UPR", "PKH2.UPR", "PUL4.HAN",
    "PUL8.HAN", "RADIOC1.UPR", "RADIOC2.UPR", "RADKN1.UPR", "RADKN2.UPR", "RN0.LWR",
    "RN0.UPR", "RN1AIM.UPR", "RN1RLD.UPR", "RN2.UPR", "RN2AIM.UPR", "RN2PSG.UPR",
    "RN2RLD.UPR", "RNDRW0.UPR", "RNDRW2.UPR", "RNFAL0B.HAN", "RNFAL2A.HAN", "RNFAL2B.HAN",
    "RNLOB.UPR", "RNLUN0.HAN", "RNLUN1.HAN", "RNLUN2.HAN", "RNRHL0.UPR", "RNRHL2.UPR",
    "RNSTTR0.HAN", "RNSTTR2.HAN", "SHIML.HAN", "SHIMR.HAN", "ST0.LWR", "ST02.UPR",
    "ST1AIM.UPR", "ST2AIM.UPR", "ST2FIRE.UPR", "ST2RLD.UPR", "STDRW0.UPR", "STDRW2.UPR",
    "STEPL0.LWR", "STEPR0.LWR", "STKN0.HAN", "STKN2.HAN", "STLOB.UPR", "STRHL0.UPR",
    "STRHL2.UPR", "STRLD1B.UPR", "STRNTRN0.HAN", "STRNTRN2.HAN", "STROL0.HAN", "STROL2.HAN",
    "STTNA.LWR", "SUREND1.HAN", "SWIT0_1.UPR", "SWIT2.UPR", "TURNL0.LWR", "TURNR0.LWR",
    "WK0.LWR", "WK0.UPR", "WK1AIM.UPR", "WK1RLD.UPR", "WK2.UPR", "WK2AIM.UPR",
    "WK2PSG.UPR", "WK2RLD.UPR", "WKDRW0.UPR", "WKDRW2.UPR", "WKFAL01.HAN", "WKFAL02.HAN",
    "WKFAL2A.HAN", "WKFAL2B.HAN", "WKLOB.UPR", "WKRHL0.UPR", "WKRHL2.UPR", NULL,
};
char *gLevelAnimFiles[9] = {
    "ADDL_ANIM_01", "ADDL_ANIM_02", "ADDL_ANIM_03", "ADDL_ANIM_04", "ADDL_ANIM_05", "ADDL_ANIM_06", "ADDL_ANIM_07", "ADDL_ANIM_08", NULL,
};
char *gAddlLevelAnimFiles[20][8] = {
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "HUDDLE.HAN", "VIDLEA.HAN", "VIRUSWLK.HAN", NULL, NULL, NULL, NULL, NULL },
    { "HUDDLE.HAN", "VIDLEA.HAN", "VIRUSWLK.HAN", NULL, NULL, NULL, NULL, NULL },
    { "XRUN0.HAN", "XRUN0.HAN", "TCRN.UPR", "TCRNAIM.UPR", "TCST.UPR", "TCSTAIM.UPR", "TCWK.UPR", "TCWKAIM.UPR" },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};

/* .sdata (0x8010F7B0-0x8010F7B8, unreferenced, player or cas, stays in the blob) */
/* (defined before the probe tables: maspsx needs non-.sdata before the next asm) */
static u8 D_8010F7A8 = 0; /* set while the player stands in a hazard (0x40000 movement flag) */
static PlayerFeetState D_8010F7AC = { 1, 1, 1, 0 }; /* feet state set on jump */

/* Collision probe used while hanging on an edge, one per animation frame */
typedef struct EdgeGrabProbe {
    /* 0x00 */ s32 unk00[0x12];
} EdgeGrabProbe;

static EdgeGrabProbe D_80104D5C[4] = { /* grab */
    { { 0xE, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1 } },
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 3, -3, 0, 1, 0 } },
    { { 7, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0xC, 0, 0, 1, 0 } },
    { { 1, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};
static EdgeGrabProbe D_80104E7C[2] = {
    { { 0xE, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1 } },
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, -4, -3, 0, 1, 0 } },
};
static EdgeGrabProbe D_80104F0C[4] = {
    { { 0xA, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 2 } },
    { { 4, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 7, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0 } },
};
static EdgeGrabProbe D_8010502C[2] = {
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 8, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, -4, 6, 0, 1, 1 } },
};
static EdgeGrabProbe D_801050BC[2] = {
    { { 8, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, -4, 6, 0, 1, 1 } },
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};
static EdgeGrabProbe D_8010514C[1] = {
    { { 2, 0, 0, 0, 0, -1, 0, 0, 0,
        0, 0, 0, 0, -4, 6, 0, 1, 1 } },
};
static s16 D_80105194[0x22] = { /* "orders confirmed" XA list */
    0, 0, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static s16 D_801051D8[0x22] = { /* "mission failed" XA list */
    0, 0, 0xE3, 0xE4, 0xE5, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void func_80025A98(Gobj *gobj, struct EGSFrame **frameOut, MoveInfo *move);
void TargetInitGobj(Gobj *gobj, TargetInfo *target, s32 kind, u8 arg3);
void SniperSetupCamera(void);
u16 TextAddMarker(u16 id, char *str, s32 ticks, void *sprites);
void EGSDeviceUnhang(void);
void SoundProcessThreat(Gobj *player);
void EGSDisplayAnimateHMD(void *anim);
void AnimateGobj(Gobj *gobj, u8 active, u8 mode);
void GameCameraSetModeTarget(s32 on, Gobj *primary, Gobj *target, s32 arg3);
void GameCameraSetModeElevator(s32 on, Gobj *primary, s32 arg2);
void GameCameraSetModeDeath(s32 on, s32 arg1, Gobj *gobj, s32 arg3, VECTOR32 *lead, VECTOR32 *watch,
                            struct GameCamSlot *slot, s32 flag);
s32 GameCameraGetMode(void);
Gobj *GameCameraGetPrimaryGobj(void);
void LevelGameOver(s32 arg0);
void JoystickActivate(void);
void NPCPutFireOnNPCMove(s32 id);
void NPCPutFireOnNPCShow(s32 id);
s32 SysSoundPlayQueuedXA(s32 msg, s32 id);
void iPlayerShoot(Gobj *gobj, s32 action);
extern s32 HANGpc;
extern u8 gTargetModeFlag;
extern u8 gNotKilledByFallingObject;

/* GameObjectAnim fields used here */
typedef struct PlayerAnimView {
    /* 0x00 */ u8 unk00[0x16];
    /* 0x16 */ s16 riding; /* set while the object rides an elevator */
} PlayerAnimView;

/* GameObjectHealth fields used here */
typedef struct PlayerHealth {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ s16 damage; /* damage taken by the last hit */
} PlayerHealth;

/* TargetInfo fields used here */
typedef struct PlayerTargetInfo {
    /* 0x00 */ s16 targetId;
    /* 0x02 */ s16 kind;
    /* 0x04 */ u8 unk04[0xB0];
    /* 0xB4 */ s32 unkB4;
    /* 0xB8 */ s32 unkB8;
} PlayerTargetInfo;

void PLAYERObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    struct EGSFrame *frame = NULL;
    MoveInfo *move;
    Gobj *primary;
    s32 rumble;
    s32 ticks;
    s32 shortTicks;
    s32 r;

    frame = gobj->anim->frame;
    move = gobj->move;
    switch (msg->id) {
    case 2:
        func_80025A98(gobj, &frame, move);
        if (gSysInfo.level == 9) {
            TargetInitGobj(gobj, NULL, 3, 1);
        } else {
            TargetInitGobj(gobj, NULL, 1, 1);
        }
        ((PlayerTargetInfo *)gobj->target)->kind = 2;
        ((PlayerTargetInfo *)gobj->target)->unkB4 = 4;
        ((PlayerTargetInfo *)gobj->target)->unkB8 = 0;
        gTargetModeFlag = 0;
        PlayerCASInit(gobj, gWeaponInfo[gobj->id != gPlayerOffset ? GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id])
                                                                  : gCurrentWeapon]
                                .holdType);
        gCASJoystickGobjID = gobj->id;
        SniperSetupCamera();
        CASRequestAction(gobj->id, 1);
        SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        if (HANGpc != 0) {
            char buf[24];

            sprintf(buf, "Hung at 0x%8lX", HANGpc);
            TextAddMarker(gobj->id, buf, 200, 0);
            EGSDeviceUnhang();
        }
        break;
    case 10:
        SoundProcessThreat(gobj);
        CASRequestAction(gobj->id, 2);
        if (gSniperMode != 0) {
            EGSDisplayAnimateHMD(gobj->anim);
        }
        AnimateGobj(gobj, 1, 0);
        if ((gobj->cas->flags & 0x100002) == 0x100002) {
            gobj->anim->unk00[0xB] |= 1;
        } else {
            gobj->anim->unk00[0xB] &= ~1;
        }
        if (D_8010F7A8 == 0 && (gobj->move->collFlags & 0x40000)) {
            gobj->move->collFlags &= ~0x40000;
        }
        D_8010F7A8 = 0;
        SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        break;
    case 11:
        if (gobj->cas->flags & 0x10) {
            gTargetModeFlag = 0;
            CASRequestAction(gobj->id, 0xC);
            CASRequestAction(gobj->id, 0x2B);
            GameCameraSetModeTarget(0, 0, 0, 0);
            SniperSetMode(1);
        } else {
            SniperSetMode(0);
            CASRequestAction(gobj->id, 0x2D);
        }
        break;
    case 30:
        if (gobj->cas->flags & 0x2000000) {
            CASRequestAction(gobj->id, 0x2B);
        }
        break;
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 35:
    case 36:
        mUseInfo = msg->id;
        D_801103BE = *(s32 *)((u8 *)msg + 4); /* not msg->from: keeps the load after the mUseInfo store */
        break;
    case 25:
        mUseInfo = 0;
        D_801103BE = -1;
        break;
    case 7:
        iPlayerShoot(gobj, 7);
        break;
    case 8:
        iPlayerShoot(gobj, 8);
        break;
    case 32:
        LevelGameOver(-1);
        break;
    case 13:
        rumble = 0xE1;
        r = rand();
        ticks = r - r / 5 * 5; /* rand() % 5, spelled out: the % form reuses ticks for the quotient */
        shortTicks = ticks + 5;
        if (((PlayerHealth *)gobj->healthInfo)->damage < 50) {
            rumble = 0x7F;
        }
        EGSDeviceControllerVibrate(0, 1, (u8)shortTicks);
        EGSDeviceControllerVibrate(1, rumble, (u8)(ticks + 10));
        if (msg->from != 0xFFFE && msg->from == GOBJ_ID_666) {
            if (gPlayerOnFireFlag == 2) {
                NPCPutFireOnNPCMove(gPlayerOffset);
                if (gobj->cas->flags & 0x400) {
                    SysPostMessage(13, 4, msg->from, gobj->id, 0, 0, 0, 0);
                    DamageGOBJ(-1, -1, gobj->id, 15, 0x4C);
                } else {
                    gPlayerOnFireFlag = 0;
                }
            } else {
                SysPostMessage(13, 4, msg->from, gobj->id, 0, 0, 0, 0);
                if (gPlayerOnFireFlag == 0) {
                    NPCPutFireOnNPCShow(gPlayerOffset);
                    gPlayerOnFireFlag = 1;
                }
                if (gNotKilledByFallingObject) {
                    if (!(gobj->cas->flags & 0x400)) {
                        DamageGOBJ(-1, -1, gobj->id, 30, 0x4C);
                    } else {
                        gPlayerOnFireFlag = 2;
                    }
                }
                NPCPutFireOnNPCMove(gPlayerOffset);
            }
        }
        break;
    case 26:
        if (GOBJ_TYPE(msg->from) == 0x20 || gameObjectsInfo[msg->from].runtime->npcState == 9) {
            D_8010F7A8 = 1;
            gobj->move->collFlags |= 0x40000;
        }
        break;
    case 37:
        ((PlayerAnimView *)gameObjectsInfo[msg->from].runtime->anim)->riding = 1;
        if (gSniperMode != 0) {
            CASRequestAction(gobj->id, 0xD);
        }
        CASRequestAction(gobj->id, 5);
        JoystickDeactivate();
        GameCameraSetModeElevator(1, 0, 0);
        break;
    case 38:
        ((PlayerAnimView *)gameObjectsInfo[msg->from].runtime->anim)->riding = 0;
        GameCameraSetModeElevator(0, 0, 0);
        JoystickActivate();
        break;
    case 40:
        if (gSniperMode == 0) {
            if (particleTaserState != 0) {
                WeaponTaser(gobj, 0);
            }
            CASRequestAction(gobj->id, 0x2E);
        }
        break;
    case 43:
        if (gPlayerOnFireFlag == 0) {
            if (GameCameraGetMode() != 0x13) {
                if (gSniperMode != 0) {
                    CASRequestAction(gobj->id, 0xD);
                }
                gTargetModeFlag = 0;
                GameCameraSetModeTarget(0, 0, 0, 0);
                GameCameraSetModeDeath(1, 9, gobj, 0, NULL, NULL, NULL, 0);
            }
            SysSoundPlayQueuedXA(0x2C, gobj->id);
            gPlayerRadioFlag = 1;
        }
        break;
    case 44:
        if (GameCameraGetMode() == 0x13) {
            primary = GameCameraGetPrimaryGobj();
            SysPostMessage(0x13, 4, primary->id, primary->id, 0, 0, 0, 0);
        } else {
            GameCameraSetModeDeath(0, 9, gobj, 0, NULL, NULL, NULL, 1);
            GameCameraRequestGetBehind();
            GameSysSetLetterBoxMode(0);
            JoystickActivate();
        }
        if (gobj->cas->flags & 0x80000000) {
            CASRequestAction(gobj->id, 0x62);
        } else {
            CASRequestAction(gobj->id, 0x5C);
        }
        gPlayerRadioFlag = 0;
        break;
    }
}

void LoadPlayerAnimations(u8 gabe) {
    s32 i;
    char *file;

    if (gabe) {
        LevelLoadHMD(gGabeAnimFiles, iAnimationArray, 0);
    } else {
        for (i = 0; i < 8; i++) {
            file = gAddlLevelAnimFiles[gSysInfo.level][i];
            if (file == NULL) {
                gLevelAnimFiles[i] = NULL;
            } else {
                strcpy(gLevelAnimFiles[i], file);
            }
        }
        LevelLoadHMD(gLevelAnimFiles, iAnimationArray, 0x9B);
    }
}

#ifdef NON_MATCHING
/* gObjPlayer store scheduled earlier; the constant 1 is CSE'd across the grenade loop into s2 */
/* Create the player object: Gabe's model, collision, flashlight and the two grenade objects. */
void func_80025A98(Gobj *gobj, struct EGSFrame **frameOut, MoveInfo *move) {
    GOBJ_MATRIX m;
    VECTOR32 pos;
    void *model;
    s32 i;

    LevelGetHMDModel("GABE.HMD", &mGabeAnim);
    EGSAnimHMDCreate(mGabeAnim, iAnimationArray, 0, 0x1E, &gobj->anim, 0);
    *(u16 *)&gobj->anim->unk14[0] = gobj->id + 0x2000;
    EGSAnimSetReferenceAnim(gobj->anim);
    gobj->anim = gobj->anim;
    gObjPlayer = gobj;
    EGSDisplayAddAnim(*gCamera, gobj->anim);
    *(s16 *)&gobj->anim->unk14[2] = -1;
    CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
    EGSDisplayPointSourceInit(mPlayerFlashlightPS);
    EGSDisplayPointSourceFrame(mPlayerFlashlightPS, gobj->anim->frames[8]);
    EGSDisplayPointSourceParams(mPlayerFlashlightPS, 0xC8, 0x20282828);
    *frameOut = gobj->anim->frame;
    LevelGetGameObjectMatrix(gobj->id, &m);
    pos.vx = m.t[0];
    pos.vy = m.t[1];
    pos.vz = m.t[2];
    pos.vy += 0x69;
    m.t[0] = pos.vx;
    m.t[1] = pos.vy;
    m.t[2] = pos.vz;
    MovementGobjInit(gobj, &m, 0, 0, 1);
    MovementGobjInitCAS(gobj, 0x75C0, 0x18500, 0x1000, 0xE66, 0x599, 1);
    PhysicsGobjBasicInit(gobj, 0x1000, 1);
    func_80025F18(gobj);
    for (i = 0; i < 2; i++) {
        mGrenadeGobj[i] = EGSMemMalloc(sizeof(Gobj));
#line 1027
        ASSERT(mGrenadeGobj[i] != NULL);
        EGSFrameCreate(&mGrenadeFrame[i]);
        LevelGetModel("GRENADE.GMD", &model);
        EGSAnimSimpleCreate(model, mGrenadeFrame[i], 0x04010000, &mGrenadeGobj[i]->anim);
        mGrenadeGobj[i]->anim->frames = NULL;
        mGrenadeGobj[i]->anim->unk00[9] = 0x11;
        mGrenadeGobj[i]->anim->frames = NULL;
        mGrenadeGobj[i]->move = NULL;
        MovementGobjInit(mGrenadeGobj[i], &m, 0, 0, 0);
        PhysicsGobjBasicInit(mGrenadeGobj[i], 0x1000, 1);
        CollisionGobjBasicInit(mGrenadeGobj[i], 0x80000001, 0x400, 0x800, -1, -1);
        mGrenadeGobj[i]->cas = NULL;
        mGrenadeGobj[i]->id = GOBJ_ID_666;
        mGrenadeGobj[i]->flags0 = 1;
        mGrenadeGobj[i]->unk04 = 0x13;
    }
    gobj->flags23 = 0;
    gobj->healthInfo = (GameObjectHealth *)gameObjectsInfo[gobj->id].unk38;
    gobj->npcState = 1;
    EGSDisplayFlashlightInit(&gPlayerFlashlight);
    EGSDisplayFlashlightFrame(&gPlayerFlashlight, gobj->anim->frames[0], 1);
    EGSDisplayFlashlightParams(&gPlayerFlashlight, gLevData->flashlightR, gLevData->flashlightG, gLevData->flashlightB);
    mShootInfo.released = 1;
    mShootInfo.lastTick = 0;
    mShootInfo.burst = 0;
    mShootReqFlag = 0;
    gSniperMode = 0;
    if (gSysInfo.level == 3) {
        mFootFx = 1;
    } else if (gSysInfo.level == 7 || gSysInfo.level == 9) {
        mFootFx = 2;
    } else {
        mFootFx = 0;
    }
    gPlayerNumConsecutiveHits = 0;
    mUseInfo = 0;
    D_801103BE = -1;
    gPlayerOnFireFlag = 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/player", func_80025A98);
#endif

void func_80025EA8(u8 on) {
    if (on) {
        EGSDisplayFlashlightOn(&gPlayerFlashlight);
        EGSDisplayPointSourceOn(mPlayerFlashlightPS);
    } else {
        EGSDisplayFlashlightOff(&gPlayerFlashlight);
        EGSDisplayPointSourceOff(mPlayerFlashlightPS);
    }
    UseMsgUpdate4FlashlightChange();
}

s32 func_80025F18(Gobj *gobj) {
    CollisionDetectorDesc desc;
    s32 kind = 3;
    s32 list;
    s32 i;

    for (i = 0; i < 15; i++) {
        D_80113D88[i] = &D_80113E3C[i];
        D_80113DC4[i] = &D_80114634[i];
        D_80113E00[i] = &D_80114E2C[i];
    }
    CollisionGobjInit(gobj, &kind);

    desc.frame = gobj->anim->frame;
    desc.offset.vx = 0;
    desc.offset.vy = -0x49;
    desc.offset.vz = 0;
    desc.radius = 0x20;
    desc.minY = 0x80000001;
    desc.maxY = 0x7FFFFFFF;
    desc.unk30 = 0xB50;
    desc.unk34 = 0x7FFFFFFF;
    desc.unk38 = 0;
    desc.unk3C = 0;
    desc.unk40 = 0x1000;
    desc.unk44 = 0x1000;
    desc.contacts = D_80113D88;
    CollisionDetectorCreate(&D_8010FF80, &desc, gobj);

    desc.frame = gobj->anim->frames[0xE];
    desc.offset.vx = 0;
    desc.offset.vy = -4;
    desc.offset.vz = 0;
    desc.radius = 0x14;
    desc.minY = 0x80000001;
    desc.maxY = 0x7FFFFFFF;
    desc.unk30 = 0xB50;
    desc.unk34 = 0x7FFFFFFF;
    desc.unk38 = 0;
    desc.unk3C = 0;
    desc.unk40 = 0x1000;
    desc.unk44 = 0x1000;
    desc.contacts = D_80113DC4;
    CollisionDetectorCreate(&D_8010FF84, &desc, gobj);

    desc.frame = gobj->anim->frames[7];
    desc.offset.vx = 0;
    desc.offset.vy = 3;
    desc.offset.vz = -5;
    desc.radius = 0x10;
    desc.minY = 0x80000001;
    desc.maxY = 0x7FFFFFFF;
    desc.unk30 = 0xB50;
    desc.unk34 = 0x7FFFFFFF;
    desc.unk38 = 0;
    desc.unk3C = 0;
    desc.unk40 = 0x1000;
    desc.unk44 = 0x1000;
    desc.contacts = D_80113E00;
    CollisionDetectorCreate(&mPlayerFeetCollisionDetector_Right, &desc, gobj);

    desc.frame = gobj->anim->frames[1];
    desc.offset.vx = 0;
    desc.offset.vy = 3;
    desc.offset.vz = -5;
    desc.radius = 0x10;
    desc.minY = 0x80000001;
    desc.maxY = 0x7FFFFFFF;
    desc.unk30 = 0xB50;
    desc.unk34 = 0x7FFFFFFF;
    desc.unk38 = 0;
    desc.unk3C = 0;
    desc.unk40 = 0x1000;
    desc.unk44 = 0x1000;
    desc.contacts = D_80113E00;
    CollisionDetectorCreate(&mPlayerFeetCollisionDetector_Left, &desc, gobj);

    ((PlayerMoveInfo *)gobj->move)->feet->unk30 = 0;
    ((PlayerMoveInfo *)gobj->move)->feet->unk28 = D_8010FF80;
    list = 0;
    ((PlayerMoveInfo *)gobj->move)->feet->unk2C = D_8010FF80;
    CollisionDetectorAddToList(mPlayerFeetCollisionDetector_Right, 0, &list);
    CollisionDetectorAddToList(mPlayerFeetCollisionDetector_Left, 1, &list);
    CollisionDetectorAddToList(D_8010FF84, 2, &list);
    CollisionGobjSetPushDetectorList(gobj, list);
    CollisionGobjSetPushDetector(gobj, -1);
    ((PlayerMoveInfo *)gobj->move)->feet->mode = 3;
    ((PlayerMoveInfo *)gobj->move)->phys->unk04 = 0x599;
    return 1;
}

#ifdef NON_MATCHING
/* Per-arm SysPlaySong calls (cross-jumped; the original shares only a3/a1+2/jal) and per-arm
 * material = floor->data - k took this from 12 to 8 lines. Left: the original keeps material in
 * v0 and the kind constant 4 in a0 (ours a1/v0), and sets a0 = 0 in the bne delay slot. */
void PlayerLandCASAnimCB(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    VECTOR32 pos = ((PlayerMoveInfo *)gobj->move)->groundPos;
    PlayerFloorFace *floor;
    u8 *material;

    if (id == gPlayerOffset) {
        floor = ((PlayerMoveInfo *)gobj->move)->feet->floor;
        if (floor->kind == 4) {
            material = floor->data - 2;
        } else {
            material = floor->data - 0xD;
        }
        SysPlaySong(0, gSoundFootstep[*material & 0x1F] + 2, gobj, 0);
    } else {
        SysPlaySong(0, gSoundFootstep[0] + 2, gobj, 0);
    }
    if (mFootFx == 2) {
        EGSDisplayRegisterFootprint(((PlayerAnimFrames *)gobj->anim)->frames[7], SpriteFootFxTextureGet(0), &pos);
        EGSDisplayRegisterFootprint(((PlayerAnimFrames *)gobj->anim)->frames[1], SpriteFootFxTextureGet(1), &pos);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/player", PlayerLandCASAnimCB);
#endif

#ifdef NON_MATCHING
/* floor material regalloc (constant 4/a0, index t0) and right-test scheduling */
void PlayerFootCB(s16 id, s32 arg1, s32 arg2, u8 right) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    PlayerFloorFace *floor;
    u8 *material;
    s32 kind;
    VECTOR32 pos;
    void **frame;
    s32 tex;
    void *f;

    if (id == gPlayerOffset) {
        floor = ((PlayerMoveInfo *)gobj->move)->feet->floor;
        material = floor->data;
        if (floor->kind == 4) {
            material -= 2;
        } else {
            material -= 0xD;
        }
        kind = *material & 0x1F;
        if (!(gobj->cas->flags & 0x100000)) {
            SysPlaySong(0, (right != 0) + gSoundFootstep[kind], gobj, 0);
        }
        if (mFootFx == 1) {
            ParticleFootSplash(right);
        }
    } else if (!(gobj->cas->flags & 0x100000)) {
        SysPlaySong(0, (right != 0) + gSoundFootstep[0], gobj, 0);
    }
    if (mFootFx == 2) {
        frame = ((PlayerAnimFrames *)gobj->anim)->frames;
        if (right) {
            frame += 7;
        } else {
            frame += 1;
        }
        f = *frame;
        EGSDisplayRegisterFootprint(f, SpriteFootFxTextureGet(right == 0), (pos = ((PlayerMoveInfo *)gobj->move)->groundPos, &pos));
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/player", PlayerFootCB);
#endif

void PlayerRightFootCASAnimCB(s16 id, s32 arg1, s32 arg2) {
    PlayerFootCB(id, arg1, arg2, 1);
}

void PlayerLeftFootCASAnimCB(s16 id, s32 arg1, s32 arg2) {
    PlayerFootCB(id, arg1, arg2, 0);
}

void PlayerFootCASAnimCB(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    PlayerFeet *feet;

    if (gobj != NULL && gobj->move != NULL && (feet = ((PlayerMoveInfo *)gobj->move)->feet) != NULL) {
        feet->state.unk00 = 0;
        feet->state.unk01 = 0;
        feet->state.unk02 = 0;
    }
}

/* Jump/land feet callback: 0x18 starts a jump whose height reaches the grabbed edge. */
void func_800265FC(Gobj *gobj, u32 action) {
    VECTOR32 vel = { 0, 0, 0, 0 };
    PlayerFeetState state = D_8010F7AC;
    PlayerFeet *feet = ((PlayerMoveInfo *)gobj->move)->feet;
    PlayerCollPhys *phys;
    s32 top;
    s32 height;
    s32 speed;
    s32 scale;
    PlayerMoveInfo *move;
    s32 feetY;
    s32 size;
    s32 base;

    switch (action) {
    case 6:
        if (feet != NULL) {
            feet->state.unk00 = 0;
            feet->state.unk01 = 0;
            feet->state.unk02 = 0;
        }
        break;
    case 7:
        if (feet != NULL && (gobj->cas->flags & 0x100000)) {
            feet->state.unk00 = 0;
            feet->state.unk01 = 0;
            feet->state.unk02 = 0;
        }
        break;
    case 0x18:
        if (feet != NULL) {
            feet->state = state;
        }
        SysPlaySong(0, 0x26, gobj, 0);
        move = (PlayerMoveInfo *)gobj->move;
        feetY = move->unk110 - 9;
        size = move->phys->unk90;
        base = move->phys->unk80;
        height = size >= 0 ? base + (size >> 1) : base - (-size >> 1);
        height = height - feetY - 0xA9;
        if (height < 0) {
            height = 0;
        }
        speed = (SquareRoot12(height * 0x148B0 + 0x6980) - 0x2916) / 2;
        if (speed < 0) {
            speed = 0;
        }
        vel.vy -= speed;
        break;
    }
    scale = ((PlayerMoveInfo *)gobj->move)->scale;
    if (scale != 0x1000) {
        vel.vx = EGSMathFIXMul(vel.vx, scale);
        vel.vy = EGSMathFIXMul(vel.vy, scale);
        vel.vz = EGSMathFIXMul(vel.vz, scale);
    }
    if (feet != NULL) {
        feet->vel = vel;
    } else if (gobj != NULL && gobj->move != NULL && ((PlayerMoveInfo *)gobj->move)->phys != NULL) {
        phys = ((PlayerMoveInfo *)gobj->move)->phys;
        phys->unkAC = -vel.vz;
        phys->unkB0 = -vel.vy;
    }
}

void PlayerJumpCASAnimCB(s16 id, u32 action) {
    func_800265FC(gameObjectsInfo[id].runtime, action);
}



void CollisionGobjSetModeEdgeGrab(Gobj *gobj, EdgeGrabProbe *probe);

void PlayerEdgeGrabCASAnimCB(s16 id, s32 action, s32 frame) {
    Gobj *gobj = gameObjectsInfo[id].runtime;

    switch (action) {
    case 0x19:
        CollisionGobjSetModeEdgeGrab(gobj, &D_80104D5C[frame]);
        break;
    case 0x1A:
        CollisionGobjSetModeEdgeGrab(gobj, &D_80104E7C[frame]);
        break;
    case 0x1B:
    case 0x5D:
        CollisionGobjSetModeEdgeGrab(gobj, &D_80104F0C[frame]);
        break;
    case 0x1D:
        if (gobj->cas->active == 8) {
            CollisionGobjSetModeEdgeGrab(gobj, &D_8010502C[frame]);
        }
        break;
    case 0x1E:
        if (gobj->cas->active == 8) {
            CollisionGobjSetModeEdgeGrab(gobj, &D_801050BC[frame]);
        }
        break;
    case 0x1C:
    case 0x1F:
        CollisionGobjSetModeEdgeGrab(gobj, &D_8010514C[frame]);
        break;
    }
}

void PlayerClimbDownCAS(s16 id, s32 arg1) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    VECTOR32 dir;

    dir.vx = ((s16 *)gobj->anim->frame)[2];
    dir.vy = ((s16 *)gobj->anim->frame)[5];
    dir.vz = ((s16 *)gobj->anim->frame)[8];
    dir.vy = -dir.vy;
    dir.vx = -dir.vx;
    dir.vy = -dir.vy;
    dir.vz = -dir.vz;
    EGSGeometryFIX_VectorGetAngleFromXZPlane(&dir, (u8 *)gobj->move + 0xC0);
    *(s32 *)((u8 *)gobj->move + 0xC4) = ratan2(dir.vx, dir.vz);
    *(s32 *)((u8 *)gobj->move + 0xC8) = 0;
    CollisionGobjReleaseModeGrabCAS(id, arg1);
    CASRequestAction(id, 0x20);
}

void PlayerOpenDoorCASAnimCB(s16 id, s32 action) {
    if (action == 0x30 && mUseInfo != 0 && D_801103BE != -1) {
        SysPostMessage(mUseInfo, 4, id, D_801103BE, 0, 0, 0, 0);
    }
}

static inline void PlayerCameraFixOn(s16 id, struct GameCamSlot *slot, VECTOR32 *pos) {
    pos->vx = gameObjectsInfo[id].matrix.t[0];
    pos->vy = gameObjectsInfo[id].matrix.t[1];
    pos->vz = gameObjectsInfo[id].matrix.t[2];
    GameCameraSetModeFixed(1, 0, pos, slot, 1);
}

static inline u8 PlayerObjectIsInactive(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 ret = 0;

    if (gobj == NULL || gobj->anim == NULL) {
        ret = 1;
    }
    return ret;
}

/* Use action start: fix the camera on a camera object, turn to face the used object. */
void func_80026B84(Gobj *gobj, Gobj *used, struct GameCamSlot *slot, s32 camera) {
    VECTOR32 pos;

    if (camera != -1 && GOBJ_TYPE(camera) == 0x77) {
        PlayerCameraFixOn(camera, slot, &pos);
    }
    if (PlayerObjectIsInactive(D_801103BE)) {
        pos.vx = gameObjectsInfo[D_801103BE].matrix.t[0];
        pos.vy = gameObjectsInfo[D_801103BE].matrix.t[1];
        pos.vz = gameObjectsInfo[D_801103BE].matrix.t[2];
    } else {
        pos.vx = used->anim->frame->pos.vx;
        pos.vy = used->anim->frame->pos.vy;
        pos.vz = used->anim->frame->pos.vz;
        pos.vy = -pos.vy;
    }
    if (used->npcState != 9) {
        MovementGobjTeleport_ByXZLookAtPoint(gobj, NULL, &pos, 0);
    }
}

/* Use action CAS callback: pick the use animation for mUseInfo, then post the use message. */
void PlayerUseCAS(s16 id, s32 action) {
    u8 started = 0;
    Gobj *gobj = gameObjectsInfo[id].runtime;
    u8 done = 0;
    Gobj *used;
    s32 camera;

    switch (action) {
    case 0x22:
        if (!(gobj->cas->flags & 0x200)) {
            if (SysSoundIsXAQueued()) {
                LevelRemoveContactMeiXingMsgForRadio();
                GameSysSetLetterBoxMode(1);
                JoystickDeactivate();
                CASRequestAction(id, 0x23);
                CASRequestAction(id, 0x5B);
                started = 1;
            } else {
                switch (mUseInfo) {
                case 0x14:
                    if (!(gobj->cas->flags & 0x100000)) {
                        CASRequestAction(id, 0x2F);
                        started = 1;
                        done = 1;
                    }
                    break;
                case 0x15:
                    CASRequestAction(id, 0x32);
                    started = 1;
                    done = 1;
                    break;
                case 0x16:
                    if (PickupObjectIsNotMaxed(D_801103BE)) {
                        CASRequestAction(id, 0x24);
                        started = 1;
                        done = 1;
                    }
                    break;
                case 0x17:
                    if (PickupObjectIsNotMaxed(D_801103BE)) {
                        if (gobj->cas->flags & 0x100000) {
                            CASRequestAction(id, 0x24);
                            started = 1;
                            done = 1;
                        } else {
                            CASRequestAction(id, 0x40);
                            started = 1;
                            done = 1;
                        }
                    }
                    break;
                case 0x18:
                    if (!(gobj->cas->flags & 0x100000)) {
                        CASRequestAction(id, 0x30);
                        started = 1;
                        done = 1;
                    }
                    break;
                case 0x23:
                    CASRequestAction(id, 0x23);
                    if (D_801103BE != -1) {
                        SysPostMessage((u8)mUseInfo, 4, id, D_801103BE, 0, 0, 0, 0);
                    }
                    started = 1;
                    break;
                case 0x24:
                    CASRequestAction(id, 0x3F);
                    started = 1;
                    done = 1;
                    break;
                }
            }
        }
        if (!started && (u32)(gSniperMode - 2) >= 2) {
            CASRequestAction(id, 0x23);
            if ((gobj->move->collFlags & 0xE0) && gobj->cas->active != 8) {
                CASRequestAction(id, 0x17);
            } else {
                CASRequestAction(id, 0x4E);
            }
        } else if (gSniperMode != 0) {
            CASRequestAction(id, 0x23);
        }
        break;
    case 0x2F:
    case 0x32:
    case 0x3F:
        done = 1;
        if (mUseInfo != 0 && D_801103BE != -1) {
            SysPostMessage((u8)mUseInfo, 4, id, D_801103BE, 0, 0, 0, 0);
        }
        CASRequestAction(id, 0x23);
        break;
    case 0x24:
    case 0x40:
        done = 1;
        PickupObjectTake(1);
        CASRequestAction(id, 0x23);
        break;
    case 0x31:
        done = 1;
        CASRequestAction(id, 0x23);
        break;
    case 0x23:
        done = 1;
        break;
    default:
        CASRequestAction(id, 0x23);
        break;
    }
    if (done && id >= 0 && id == gCASJoystickGobjID) {
        if (action == 0x22 && D_801103BE != -1) {
            used = gameObjectsInfo[D_801103BE].runtime;
            camera = used->unk04;
            if (camera != -1) {
                if (GOBJ_TYPE(camera) == 0x22) {
                    ObjectTeleportPlayer(camera, used->npcState != 9 ? -2 : -1);
                    camera = gameObjectsInfo[camera].refId;
                }
                if (GOBJ_TYPE(camera) == 0x77) {
                    func_80026B84(gobj, used, &D_80115624, camera);
                }
            } else if (used->npcState == 2) {
                func_80026B84(gobj, used, &D_80115624, ObjectInvisibleMatchToRefID(used->id, 0x77));
            }
            JoystickDirInputDeactivate(-1, 0);
        } else {
            JoystickDirInputDeactivate(0, 0);
            if (GameCameraCheckMode(0xD)) {
                GameCameraRequestGetBehind();
            }
            GameCameraSetModeFixed(0, 0, NULL, &D_80115624, 1);
        }
    }
}

void PlayerChangeWpnCAS(s16 id, s32 action) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 hold;

    if (!GOBJ_WEAPON_NOT_TASER(gobj)) {
        WeaponTaser(gobj, 0);
    }
    switch (action) {
    case 0x34:
        if (gobj->cas->flags & 0x200) {
            SniperSetMode(0);
        }
        mShootInfo.lastTick = 0;
        InterfaceWeaponSelect(1, 0);
        break;
    case 0x25:
        InterfaceWeaponSelect(0, 0);
        WeaponReplace(gobj);
        hold = gWeaponInfo[GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id])].holdType;
        if (gPlayerFlashlight.on) {
            func_80025EA8(0);
        }
        switch (hold) {
        case 0:
            CASRequestAction(id, 0x26);
            break;
        case 4:
            CASRequestAction(id, 0x27);
            break;
        case 1:
            if (GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]) == 0x15) {
                CASRequestAction(gobj->id, 0x2B);
                func_80025EA8(1);
            }
            CASRequestAction(id, 0x28);
            break;
        case 2:
            CASRequestAction(id, 0x29);
            break;
        }
        break;
    }
}

void PlayerCameraModeCAS(s16 id, s32 action) {
    if (id >= 0 && id == gCASJoystickGobjID) {
        switch (action) {
        case 0x1C:
        case 0x1F:
            GameCameraSetModeHang(1, 0, 0);
            break;
        case 0x20:
        case 0x21:
        case 0x3E:
        case 0x44:
            GameCameraSetModeHang(0, 0, 0);
            break;
        }
    }
}

void iWeaponShootCB(void) {
    Gobj *gobj = gameObjectsInfo[gPlayerOffset].runtime;

    if (gobj->cas->flags & 0x100) {
        CASRequestAction(gobj->id, 0x45);
        if (mShootReqFlag == 2 && GOBJ_WEAPON_NOT_TASER(gobj)) {
            CASRequestAction(gobj->id, 0x15);
        }
    }
    mShootReqFlag = 0;
}

void PlayerShootCASAnimCB(s16 id, s32 action, s32 frame) {
    Gobj *gobj;
    u32 flags;
    s32 delay;

    if (id == gPlayerOffset) {
        gobj = gameObjectsInfo[id].runtime;
        flags = gobj->cas->flags;
        delay = 0x14;
        if (flags & 2) {
            delay = 5;
        } else if (!(flags & 4)) {
            if (flags & 8) {
                delay = 0xA;
            }
        }
        if (gobj->cas->flags & 0x100000) {
            delay += 3;
        }
        if (mShootReqFlag == 1) {
            EGSDisplayQueueInterrupt(iWeaponShootCB, delay, 0);
            mShootReqFlag = 2;
        } else if (mShootReqFlag == 0) {
            EGSDisplayQueueInterrupt(iWeaponShootCB, delay, 0);
            mShootReqFlag = 3;
        }
    }
}

/* NPCInfo fields used here */
typedef struct PlayerNPCInfo {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ s16 playerDist; /* distance to the player, updated by the NPC brain */
} PlayerNPCInfo;

#define WEAPON_IS_GRENADE(w) ((u32)((w) - 0x13) < 2)

/* Player fire button: action 7 pressed, 8 released. */
void iPlayerShoot(Gobj *gobj, s32 action) {
    s32 weapon = gobj->id != gPlayerOffset ? GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]) : gCurrentWeapon;
    s32 rounds;
    s32 clips;
    Gobj *target;

    if (!(gobj->cas->flags2 & 0x10) &&
        ((gobj->cas->flags & 0x1080000) ||
         (gobj->cas->active == 8 && (!(gobj->cas->flags & 0x200000) || WEAPON_IS_GRENADE(weapon))))) {
        if (action == 8) {
            WeaponTaser(gobj, 0);
        }
        return;
    }
    WeaponGetRounds(weapon, &rounds, &clips);
    if (action == 7) {
        if (WEAPON_IS_GRENADE(weapon)) {
            if (rounds != 0 && mShootInfo.released != 0 && mGrenadeGobj[0]->flags0 != 0) {
                mShootInfo.released = 0;
                mShootInfo.lastTick = gExeTick;
            }
            if (gPlayerNumConsecutiveHits != 0) {
                gPlayerNumConsecutiveHits = 0;
            }
            return;
        }
        if (weapon == 0x15 || weapon == 0x12) {
            return;
        }
        if ((u32)(weapon - 0x18) < 2) {
            UseLevelSpecificObject();
            return;
        }
        if (weapon == 0x17) {
            UseKey();
            return;
        }
        if (gSniperMode == 0) {
            if (!(gobj->cas->flags & 0x100)) {
                CASRequestAction(gobj->id, 0x2B);
                mShootReqFlag = 1;
                if (gobj->cas->active == 8) {
                    PlayerShootCASAnimCB(gobj->id, 0x15, 1);
                }
                return;
            }
            if ((gobj->cas->flags & 0x120) == 0x100 && !(gobj->cas->flags2 & 0x10)) {
                if (gobj->cas->active == 8) {
                    PlayerShootCASAnimCB(gobj->id, 0x15, 1);
                }
                return;
            }
        }
        if (weapon == WEAPON_TASER && particleTaserState != 3) {
            WeaponTaser(gobj, 1);
            return;
        }
        if ((u32)(gWeaponInfo[weapon].fireMode - 1) < 2) {
            if (gWeaponInfo[weapon].holdType == 1) {
                CASRequestAction(gobj->id, 0x3A);
            } else if (gWeaponInfo[weapon].holdType == 2) {
                CASRequestAction(gobj->id, 0x3B);
            }
        }
        if (GameSysIsTime(mShootInfo.lastTick, gWeaponInfo[weapon].fireDelay) &&
            ((gWeaponInfo[weapon].fireMode == 0 && mShootInfo.released != 0) || (u32)(gWeaponInfo[weapon].fireMode - 1) < 2)) {
            mShootInfo.released = 0;
            mShootInfo.lastTick = gExeTick;
            if (rounds != 0) {
                mShootInfo.burst++;
                WeaponSubRound(gobj);
                EGSAnimGunFlash2(gobj->anim, 3, gWeaponInfo[gCurrentWeapon].flashColor);
                if (gSniperMode != 0) {
                    SniperShoot();
                } else if (TargetShoot(gobj, 1, 0)) {
                    target = gameObjectsInfo[gobj->target->targetId].runtime;
                    if (gHitNPCCallback != NULL) {
                        gHitNPCCallback(target->id, 0);
                    }
                    if (target->healthInfo->health != 0x7FFF) {
                        VECTOR32 pos;
                        s32 dist;

                        if (target->npc != NULL) {
                            dist = ((PlayerNPCInfo *)target->npc)->playerDist;
                        } else {
                            pos.vx = target->anim->frame->pos.vx;
                            pos.vy = target->anim->frame->pos.vy;
                            pos.vz = target->anim->frame->pos.vz;
                            pos.vy = -pos.vy;
                            EGSGeometry_GetFuzzyDistancePoint2Point(&gobj->move->pos, &pos, &dist);
                        }
                        DamageGOBJ(gobj->id, gobj->id, target->id,
                                   dist <= 0x3C0 ? gWeaponInfo[weapon].damageNear : gWeaponInfo[weapon].damageFar,
                                   WeaponGetDeathAction(weapon));
                    }
                    gPlayerNumConsecutiveHits++;
                } else {
                    gPlayerNumConsecutiveHits = 0;
                }
                if (!WEAPON_IS_GRENADE(weapon)) {
                    WeaponDoFiredCallback(gPlayerOffset, weapon);
                }
            } else if (mShootInfo.burst > 100) {
                mShootInfo.burst = 0;
            } else if (clips != 0 && gobj->cas->active != 8) {
                WeaponSubRound(gobj);
            } else {
                SysPlaySong(1, 0x12, 0, 0);
            }
        }
        if (mShootInfo.burst == 101 || mShootInfo.burst == 102) {
            SysPostMessage(7, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        } else if (mShootInfo.burst > 102) {
            mShootInfo.burst = 0;
        }
    } else if (action == 8) {
        mShootInfo.released = 1;
        if (weapon == 0x15) {
            if (gPlayerFlashlight.on) {
                CASRequestAction(gobj->id, 0x2C);
                func_80025EA8(0);
            } else {
                CASRequestAction(gobj->id, 0x2B);
                func_80025EA8(1);
            }
        }
        if (weapon == WEAPON_TASER) {
            WeaponTaser(gobj, 0);
            return;
        }
        if (WEAPON_IS_GRENADE(weapon) && mGrenadeGobj[0]->flags0 != 0 && rounds != 0) {
            mGrenadeGobj[0]->flags0 = 0;
            mGrenadeGobj[0]->unk04 = weapon;
            CASRequestAction(gobj->id, 0x3A);
            WeaponSubRound(gobj);
            return;
        }
        if (gWeaponInfo[weapon].fireMode == 1 && rounds != 0 && mShootInfo.burst < 3) {
            mShootInfo.burst += 100;
            SysPostMessage(7, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        } else {
            mShootInfo.burst = 0;
        }
    }
}

void PlayerGrenadeCASAnimCB(s16 id) {
    Gobj *target = NULL;
    Gobj *gobj = gameObjectsInfo[id].runtime;
    VECTOR32 *aim = NULL;
    s32 now = gExeTick;
    s32 power;
    s32 outId;
    s32 dist;
    u32 held;
    s16 targetId;

    if (id == gPlayerOffset) {
        targetId = gobj->target->targetId;
        if (targetId >= 0) {
            dist = 100;
            if (gobj->target->unk58 <= 100) {
                dist = gobj->target->unk58;
            }
            target = gameObjectsInfo[targetId].runtime;
            power = ((100 - dist) / 10) << 5;
        } else {
            held = now - mShootInfo.lastTick;
            if (held < 20) {
                held = 20;
            }
            if (held > 80) {
                held = 80;
            }
            power = held << 5;
        }
        if (gSniperMode != 0) {
            aim = SniperGetTarget(&outId);
        }
    } else {
        aim = &grenadePos;
        power = 0;
    }
    WeaponThrowGrenade(gobj, target, power, aim);
}

/* 20.12 fixed point to integer, rounding towards zero */
#define GRENADE_FIX(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

/* Cheap |v|: largest component plus a quarter of the other two. */
static inline s32 GrenadeFuzzyLength(s32 x, s32 y, s32 z) {
    VECTOR32 a;
    s32 t;

    a.vz = abs(z);
    a.vx = abs(x);
    a.vy = abs(y);
    if (a.vx < a.vy) {
        t = a.vx;
        a.vx = a.vy;
        a.vy = t;
    }
    if (a.vx < a.vz) {
        t = a.vx;
        a.vx = a.vz;
        a.vz = t;
    }
    return a.vx + ((a.vy + a.vz) >> 2);
}

/*
 * Whether the grenade flies freely from 'from' to 'to'; fills in where and what it hit otherwise.
 * A window in the way breaks and does not stop it.
 */
static inline s32 GrenadeLOS(VECTOR32 *from, VECTOR32 *to, Gobj *obj, VECTOR32 *hitPos, VECTOR32 *hitNormal) {
    LOSRequest req;
    u8 noHit;

    req.from = from;
    req.to = to;
    req.fromObj = obj;
    req.toObj = NULL;
    req.noHit = &noHit;
    req.hitPos = hitPos;
    req.hitNormal = hitNormal;
    req.hitInfo = NULL;
    _LOSPoint2PointCollide(&req);
    if (losWindowCount != 0) {
        DamageGOBJ(gPlayerOffset, gPlayerOffset, losWindowArray[0]->id, 100, 0);
        return 1;
    }
    return noHit;
}

static inline void GrenadeDot(VECTOR32 *a, VECTOR32 *b, s32 *out) {
    *out = EGSMathFIXMul(a->vx, b->vx) + EGSMathFIXMul(a->vy, b->vy) + EGSMathFIXMul(a->vz, b->vz);
}

static inline void GrenadeFixDiv(s32 a, s32 b, s32 *out) {
    *out = EGSMathFIXDiv(a, b);
}

#ifdef NON_MATCHING
/* window path keeps a dead return value (no jump threading); bounce block scheduled differently */
/* Per-tick grenade update: fly, bounce off what it hits, explode after 60 ticks. */
void iWeaponGrenadeCB(Gobj *grenade) {
    VECTOR32 pos;
    VECTOR32 hitPos;
    VECTOR32 hitNormal;
    VECTOR32 step;
    VECTOR32 dir;
    VECTOR32 next;
    VECTOR32 ahead;
    VECTOR32 push;
    VECTOR32 vel;
    VECTOR32 tang;
    VECTOR32 norm;
    PlayerMoveInfo *move;
    Gobj *hit;
    LOSHitView *los;
    s32 kind;
    s32 thrower;
    s32 id;
    s32 dot;
    s32 ratio;
    s32 scale;
    Gobj *gobj = grenade;
    void *display = *gCamera;

    pos = grenade->move->pos;
    kind = grenade->unk04;
    if (grenade->flags1++ > 60) {
        EGSDisplayDeleteAnim(display, grenade->anim);
        pos = grenade->move->pos;
        thrower = grenade == mGrenadeGobj[0] ? gPlayerOffset : npcGrenadeThrowerID;
        if (kind == 0x14) {
            DamageGasCloud(thrower, &pos, 1);
        } else {
            DamageExplosion(thrower, &pos, 1);
        }
        MovementRemoveGobj(gobj);
        gobj->flags0 = 1;
        return;
    }
    move = (PlayerMoveInfo *)gobj->move;
    step = move->step;
    next.vx = pos.vx + GRENADE_FIX(step.vx);
    next.vy = pos.vy + GRENADE_FIX(step.vy);
    next.vz = pos.vz + GRENADE_FIX(step.vz);
    GTEMath_VectorNormal((VECTOR *)&step, (VECTOR *)&dir);
    ahead.vx = next.vx + GRENADE_FIX(dir.vx * 19);
    ahead.vy = next.vy + GRENADE_FIX(dir.vy * 19);
    ahead.vz = next.vz + GRENADE_FIX(dir.vz * 19);
    if (!GrenadeLOS(&pos, &ahead, gobj, &hitPos, &hitNormal)) {
        los = losHitsSorted[0];
        hit = NULL;
        id = los != NULL && los->type == 1 ? los->owner->id & 0x3FF : -1;
        if (id >= 0) {
            hit = gameObjectsInfo[id].runtime;
        }
        if (hit == NULL || (u32)(hit->npcState - 1) >= 2 || hit->healthInfo->health == -1) {
            npcGrenadeLanded(grenade != mGrenadeGobj[0]);
            push.vx = hitPos.vx - next.vx;
            push.vy = hitPos.vy - next.vy;
            push.vz = hitPos.vz - next.vz;
            push.vx <<= 12;
            push.vy <<= 12;
            push.vz <<= 12;
            ((PlayerMoveInfo *)gobj->move)->step.vx += push.vx;
            ((PlayerMoveInfo *)gobj->move)->step.vy += push.vy;
            ((PlayerMoveInfo *)gobj->move)->step.vz += push.vz;
            push.vx = hitNormal.vx << 4;
            push.vy = hitNormal.vy << 4;
            push.vz = hitNormal.vz << 4;
            ((PlayerMoveInfo *)gobj->move)->step.vx += push.vx;
            ((PlayerMoveInfo *)gobj->move)->step.vy += push.vy;
            ((PlayerMoveInfo *)gobj->move)->step.vz += push.vz;
            vel.vx = ((PlayerMoveInfo *)gobj->move)->launchVel.vx + ((PlayerMoveInfo *)gobj->move)->impulse.vx;
            vel.vy = ((PlayerMoveInfo *)gobj->move)->launchVel.vy + ((PlayerMoveInfo *)gobj->move)->impulse.vy;
            vel.vz = ((PlayerMoveInfo *)gobj->move)->launchVel.vz + ((PlayerMoveInfo *)gobj->move)->impulse.vz;
            GrenadeDot(&vel, &hitNormal, &dot);
            norm.vx = EGSMathFIXMul(hitNormal.vx, dot);
            norm.vy = EGSMathFIXMul(hitNormal.vy, dot);
            norm.vz = EGSMathFIXMul(hitNormal.vz, dot);
            tang.vx = vel.vx - norm.vx;
            tang.vy = vel.vy - norm.vy;
            tang.vz = vel.vz - norm.vz;
            scale = ((PlayerMoveInfo *)gobj->move)->friction;
            GrenadeFixDiv(GrenadeFuzzyLength(norm.vx, norm.vy, norm.vz), GrenadeFuzzyLength(vel.vx, vel.vy, vel.vz) + 1, &ratio);
            scale *= ratio;
            scale = -GRENADE_FIX(scale);
            tang.vx = EGSMathFIXMul(tang.vx, scale);
            tang.vy = EGSMathFIXMul(tang.vy, scale);
            tang.vz = EGSMathFIXMul(tang.vz, scale);
            ((PlayerMoveInfo *)gobj->move)->impulse.vx += tang.vx;
            ((PlayerMoveInfo *)gobj->move)->impulse.vy += tang.vy;
            ((PlayerMoveInfo *)gobj->move)->impulse.vz += tang.vz;
            scale = -((PlayerMoveInfo *)gobj->move)->restitution - 0x1000;
            norm.vx = EGSMathFIXMul(norm.vx, scale);
            norm.vy = EGSMathFIXMul(norm.vy, scale);
            norm.vz = EGSMathFIXMul(norm.vz, scale);
            ((PlayerMoveInfo *)gobj->move)->impulse.vx += norm.vx;
            ((PlayerMoveInfo *)gobj->move)->impulse.vy += norm.vy;
            ((PlayerMoveInfo *)gobj->move)->impulse.vz += norm.vz;
            if ((u32)(norm.vx + 0xCCC0) > 0x19980 || (u32)(norm.vy + 0xCCC0) > 0x19980 ||
                (u32)(norm.vz + 0xCCC0) > 0x19980) {
                SysPlaySong(1, 0xB, gobj, 0);
            }
            if (kind == 0x14) {
                gobj->flags1 = 61;
            }
        }
    }
    ParticleFuse(&pos);
    SysExeTickQueueTimer(iWeaponGrenadeCB, 1, grenade);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/player", iWeaponGrenadeCB);
#endif

/* Grenade flight: x/z speed (also drop height) and launch y speed, kept in memory */
typedef struct GrenadeArc {
    /* 0x00 */ s32 dist;
    /* 0x04 */ s32 t;
    /* 0x08 */ s32 speed;
    /* 0x0C */ s32 vy;
} GrenadeArc;

#define GRENADE_GRAVITY -0x2916

/* Launch a grenade from the thrower's hand at a target object, an aim point, or forwards. */
void WeaponThrowGrenade(Gobj *gobj, Gobj *target, s32 power, VECTOR32 *aim) {
    VECTOR32 vel;
    VECTOR32 dir;
    VECTOR32 pos;
    VECTOR32 dest;
    VECTOR32 base;
    GrenadeArc arc;
    Gobj **slot;
    Gobj *grenade;
    void *display;
    s32 speed;
    s32 spread;
    s32 root;
    s32 dy;

    if (gobj->id != gPlayerOffset) {
        slot = &mGrenadeGobj[1];
    } else {
        slot = &mGrenadeGobj[0];
    }
    grenade = *slot;
    pos.vx = gobj->anim->frames[8]->pos.vx;
    pos.vy = gobj->anim->frames[8]->pos.vy;
    pos.vz = gobj->anim->frames[8]->pos.vz;
    pos.vy = -pos.vy;
    if (target == NULL && aim == NULL) {
        if (gobj->cas->flags & 0x2000000) {
            dir.vx = gobj->anim->frames[0]->m[0][2];
            dir.vy = gobj->anim->frames[0]->m[1][2];
            dir.vz = gobj->anim->frames[0]->m[2][2];
            dir.vy = -dir.vy;
            dir.vx = -dir.vx;
            dir.vy = -dir.vy;
            dir.vz = -dir.vz;
        } else {
            dir.vx = gobj->anim->frame->m[0][2];
            dir.vy = gobj->anim->frame->m[1][2];
            dir.vz = gobj->anim->frame->m[2][2];
            dir.vy = -dir.vy;
        }
        dir.vy += 0x1000;
        GTEMath_VectorNormal((VECTOR *)&dir, (VECTOR *)&dir);
        speed = (power << 12) / 15;
        vel.vx = EGSMathFIXMul(dir.vx, speed);
        vel.vy = EGSMathFIXMul(dir.vy, speed);
        vel.vz = EGSMathFIXMul(dir.vz, speed);
        base = ((PlayerMoveInfo *)gobj->move)->vel;
        vel.vx += base.vx;
        vel.vy += base.vy;
        vel.vz += base.vz;
    } else {
        if (aim != NULL) {
            dest.vx = aim->vx;
            dest.vy = aim->vy;
            dest.vz = aim->vz;
        } else {
            dest = target->move->pos;
            if (power != 0) {
                spread = power >> 1;
                dest.vx += rand() % power - spread;
                dest.vz += rand() % power - spread;
            }
        }
        dir.vx = dest.vx - pos.vx;
        dir.vy = dest.vy - pos.vy;
        dir.vz = dest.vz - pos.vz;
        dy = dir.vy;
        dir.vy = 0;
        EGSMath_VectorModulus(&dir, &arc.dist);
        dir.vx = EGSMathFIXDiv(dir.vx, arc.dist);
        dir.vy = EGSMathFIXDiv(dir.vy, arc.dist);
        dir.vz = EGSMathFIXDiv(dir.vz, arc.dist);
        if (arc.dist == 0) {
            arc.vy = 0;
            arc.speed = 0;
        } else if (GRENADE_FIX(dy * 0x2333) >= arc.dist) {
            arc.t = -(dy * 0x2666);
            arc.t = EGSMathFIXDiv(arc.t, GRENADE_GRAVITY);
            root = SquareRoot12(arc.t);
            arc.speed = EGSMathFIXDiv(arc.dist << 12, root);
            arc.vy = EGSMathFIXDiv(dy * 0x2333, root);
        } else {
            arc.t = (dy - arc.dist) << 13;
            arc.t = EGSMathFIXDiv(arc.t, GRENADE_GRAVITY);
            arc.speed = EGSMathFIXDiv(arc.dist << 12, SquareRoot12(arc.t));
            arc.vy = arc.speed;
        }
        vel.vx = EGSMathFIXMul(dir.vx, arc.speed);
        vel.vy = EGSMathFIXMul(dir.vy, arc.speed);
        vel.vz = EGSMathFIXMul(dir.vz, arc.speed);
        vel.vy = arc.vy;
    }
    ((PlayerMoveInfo *)grenade->move)->launchPos = pos;
    ((PlayerMoveInfo *)grenade->move)->launchVel = vel;
    MovementAddGobj(grenade);
    display = *gCamera;
    grenade->flags1 = 0;
    EGSDisplayAddAnim(display, grenade->anim);
    SysExeTickQueueTimer(iWeaponGrenadeCB, 1, grenade);
}

void PlayerChangeSniperWpnCAS(s16 id) {
    if (gameObjectsInfo[id].runtime->cas->flags & 0x200) {
        SniperSetMode(1);
    }
}

void PlayerReloadCAS(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 weapon = WeaponOfGobj(gobj);
    u8 manual;

    if ((u32)(gSniperMode - 2) >= 2) {
        manual = 0;
        if (weapon == 0x10 || (u32)(weapon - 6) < 2) {
            manual = 1;
        }
        if (WeaponReload(weapon) || manual) {
            if (!manual) {
                CASRequestAction(gobj->id, 0x3D);
            } else {
                CASRequestAction(gobj->id, 0x54);
            }
        }
    }
}

void PlayerReinit(void) {
    gObjPlayer->healthInfo->health = 150;
    gObjPlayer->healthInfo->armor = 600;
    WeaponAdd(1, gWeaponInfo[1].startAmmo * 3);
    WeaponAdd(0xD, gWeaponInfo[0xD].startAmmo);
    WeaponAdd(WEAPON_TASER, 99);
    WeaponAdd(0x15, 0);
    PlayerCASInit(gObjPlayer, 1);
    WeaponSelect(gObjPlayer, 1, 1);
    WeaponReplace(gObjPlayer);
    InterfaceWeaponChange();
    SavePlayerPreserve();
}

void PlayerConfirmOrders(void) {
    SysStopXA();
    SysSoundPlayRandXA(D_80105194);
    if (gPlayerRadioFlag) {
        SysPostMessage(0x2C, 4, gPlayerOffset, gPlayerOffset, 0, 0, 0, 0);
    }
}

void PlayerFailMission(void) {
    SysSoundPlayRandXA(D_801051D8);
}
