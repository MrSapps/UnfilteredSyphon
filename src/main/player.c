#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "egsmath.h"

/* player.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F7A8)
SDATA_LOCAL(D_8010FF80)
SDATA_LOCAL(D_8010FF84)
SDATA_LOCAL(gPlayerNumConsecutiveHits)
SDATA_LOCAL(mFootFx)
SDATA_LOCAL(mGabeAnim)
SDATA_LOCAL(mGrenadeGobj)
SDATA_LOCAL(mPlayerFeetCollisionDetector_Left)
SDATA_LOCAL(mPlayerFeetCollisionDetector_Right)
SDATA_LOCAL(mShootReqFlag)
SDATA_LOCAL(mUseInfo)

extern u16 mUseInfo;     /* message id of the pending use action, 0 for none */
extern s16 D_801103BE;   /* object the pending use action targets, -1 for none */
extern s32 gPlayerOffset;
/* Player flashlight (EGS display light); the first word is nonzero while it is on */
typedef struct PlayerFlashlight {
    /* 0x00 */ s32 on;
    /* 0x04 */ u8 unk04[0x1C];
} PlayerFlashlight;
extern PlayerFlashlight gPlayerFlashlight;
extern u8 mPlayerFlashlightPS[];
extern u8 D_801051D8[]; /* "mission failed" XA list */
extern u8 D_80105194[]; /* "orders confirmed" XA list */

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

extern PlayerFeetState D_8010F7AC; /* feet state set on jump */

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
extern s32 D_8010FF80; /* body collision detector */
extern s32 D_8010FF84; /* head collision detector */
extern s32 mPlayerFeetCollisionDetector_Left;
extern s32 mPlayerFeetCollisionDetector_Right;

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
extern void *mGabeAnim;
extern Gobj *mGrenadeGobj[2];
extern struct EGSFrame *mGrenadeFrame[2];
extern u8 mShootInfo;
extern s32 D_80127CCC;
extern s32 gPlayerNumConsecutiveHits;
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
void CollisionDetectorAddToList(s32 detector, s32 index, s32 *list);
void CollisionGobjSetPushDetectorList(Gobj *gobj, s32 list);
void CollisionGobjSetPushDetector(Gobj *gobj, s32 detector);

/* MoveInfo fields used here */
typedef struct PlayerMoveInfo {
    /* 0x000 */ u8 unk000[0x104];
    /* 0x104 */ s32 scale; /* 0x1000 = 1.0 */
    /* 0x108 */ u8 unk108[8];
    /* 0x110 */ s32 unk110;
    /* 0x114 */ u8 unk114[0x3C];
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
extern s32 mFootFx;

s32 SpriteFootFxTextureGet(s32 foot);
void EGSDisplayRegisterFootprint(void *frame, s32 tex, VECTOR32 *pos);
void SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);

extern u8 mShootReqFlag; /* 1: shot requested, 2: shot queued, 3: shot queued unrequested */
extern s32 gCurrentWeapon;
extern char *gGabeAnimFiles[];
extern char *gLevelAnimFiles[8];
extern char *gAddlLevelAnimFiles[][8];
extern u8 iAnimationArray[];

/* Only the field used here; see weapon.c for the full WeaponInfo. */
typedef struct WeaponInfo {
    /* 0x00 */ u8 unk00[0xA];
    /* 0x0A */ u8 startAmmo;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ u32 clipShift : 3;
    /* 0x0C */ u32 holdType : 3; /* selects the draw animation */
    /* 0x0C */ u32 unk0C_6 : 26;
    /* 0x10 */ u8 unk10[0x10];
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
void GameCameraSetModeFixed(s32 on, s32 arg1, void *camera, s32 arg3, s32 arg4);
void MovementGobjTeleport_ByXZLookAtPoint(Gobj *gobj, VECTOR32 *pos, VECTOR32 *lookAt, s32 arg3);
VECTOR32 *SniperGetTarget(s32 *outId);
void WeaponThrowGrenade(Gobj *gobj, Gobj *target, s32 power, VECTOR32 *aim);

extern u32 D_80127CD0; /* tick the grenade throw started */
extern VECTOR32 grenadePos; /* NPC grenade target */

INCLUDE_RODATA("asm/main/nonmatchings/player", D_8001040C);

INCLUDE_ASM("asm/main/nonmatchings/player", PLAYERObjDispatch);

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
void func_80025A98(Gobj *gobj, struct EGSFrame **frameOut) {
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
    mShootInfo = 1;
    D_80127CD0 = 0;
    D_80127CCC = 0;
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

#ifdef NON_MATCHING
/* Register allocation only in the last store: the original keeps the move pointer in v1 and sets the return value early */
/* Set up the player's collision: body, head and feet detectors. */
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
#else
INCLUDE_ASM("asm/main/nonmatchings/player", func_80025F18);
#endif

#ifdef NON_MATCHING
/* Draft: floor material lookup and SysPlaySong argument setup are scheduled differently. */
void PlayerLandCASAnimCB(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    VECTOR32 pos = ((PlayerMoveInfo *)gobj->move)->groundPos;
    PlayerFloorFace *floor;
    u8 *material;
    u8 sound;

    if (id == gPlayerOffset) {
        floor = ((PlayerMoveInfo *)gobj->move)->feet->floor;
        material = floor->data;
        if (floor->kind == 4) {
            material -= 2;
        } else {
            material -= 0xD;
        }
        sound = gSoundFootstep[*material & 0x1F];
    } else {
        sound = gSoundFootstep[0];
    }
    SysPlaySong(0, sound + 2, gobj, 0);
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

INCLUDE_RODATA("asm/main/nonmatchings/player", jtbl_80010C1C);

/* Collision probe used while hanging on an edge, one per animation frame */
typedef struct EdgeGrabProbe {
    /* 0x00 */ u8 unk00[0x48];
} EdgeGrabProbe;

extern EdgeGrabProbe D_80104D5C[]; /* grab */
extern EdgeGrabProbe D_80104E7C[];
extern EdgeGrabProbe D_80104F0C[];
extern EdgeGrabProbe D_8010502C[];
extern EdgeGrabProbe D_801050BC[];
extern EdgeGrabProbe D_8010514C[];

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

static inline void PlayerCameraFixOn(s16 id, s32 arg2, VECTOR32 *pos) {
    pos->vx = gameObjectsInfo[id].matrix.t[0];
    pos->vy = gameObjectsInfo[id].matrix.t[1];
    pos->vz = gameObjectsInfo[id].matrix.t[2];
    GameCameraSetModeFixed(1, 0, pos, arg2, 1);
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
void func_80026B84(Gobj *gobj, Gobj *used, s32 arg2, s32 camera) {
    VECTOR32 pos;

    if (camera != -1 && GOBJ_TYPE(camera) == 0x77) {
        PlayerCameraFixOn(camera, arg2, &pos);
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

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerUseCAS);

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
        D_80127CD0 = 0;
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

void PlayerShootCASAnimCB(s16 id) {
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

INCLUDE_ASM("asm/main/nonmatchings/player", iPlayerShoot);

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
            held = now - D_80127CD0;
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

INCLUDE_ASM("asm/main/nonmatchings/player", iWeaponGrenadeCB);

INCLUDE_ASM("asm/main/nonmatchings/player", WeaponThrowGrenade);

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
