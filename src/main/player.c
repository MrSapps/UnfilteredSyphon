#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

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
extern s32 gPlayerFlashlight;
extern u8 mPlayerFlashlightPS[];
extern u8 D_801051D8[]; /* "mission failed" XA list */
extern u8 D_80105194[]; /* "orders confirmed" XA list */

/* Face the feet collision detector stands on */
typedef struct PlayerFloorFace {
    /* 0x00 */ s32 kind; /* 4: quad */
    /* 0x04 */ u8 *data; /* the face's material byte precedes it */
} PlayerFloorFace;

typedef struct PlayerFeet {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D;
    /* 0x1E */ u8 unk1E;
    /* 0x1F */ u8 unk1F[0x19];
    /* 0x38 */ PlayerFloorFace *floor;
} PlayerFeet;

/* MoveInfo fields used here */
typedef struct PlayerMoveInfo {
    /* 0x000 */ u8 unk000[0x150];
    /* 0x150 */ VECTOR32 groundPos;
    /* 0x160 */ u8 unk160[0x50];
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
    /* 0x0B */ u8 unk0B[0x15];
} WeaponInfo; /* size 0x20 */
extern WeaponInfo gWeaponInfo[];

#define WEAPON_TASER 0xE

/* GameObjectInfo.flags24 viewed as the NPC weapon word: low byte = weapon, 0x3000 = default kind */
#define GOBJINFO_WEAPON(info) (*(u8 *)&(info)->flags24)
#define GOBJINFO_WORD24(info) (*(u32 *)&(info)->flags24)

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
void PlayerFootCB(s16 id, s32 arg1, s32 arg2, s32 right);
void func_800265FC(Gobj *gobj);
void SniperSetMode(s32 mode);
void EGSDisplayFlashlightOn(void *light);
void EGSDisplayFlashlightOff(void *light);
void EGSDisplayPointSourceOn(void *ps);
void EGSDisplayPointSourceOff(void *ps);
void UseMsgUpdate4FlashlightChange(void);

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

INCLUDE_ASM("asm/main/nonmatchings/player", func_80025A98);

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

INCLUDE_ASM("asm/main/nonmatchings/player", func_80025F18);

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

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerFootCB);

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
        feet->unk1C = 0;
        feet->unk1D = 0;
        feet->unk1E = 0;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/player", func_800265FC);

void PlayerJumpCASAnimCB(s16 id) {
    func_800265FC(gameObjectsInfo[id].runtime);
}

INCLUDE_RODATA("asm/main/nonmatchings/player", jtbl_80010C1C);

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerEdgeGrabCASAnimCB);

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

INCLUDE_ASM("asm/main/nonmatchings/player", func_80026B84);

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerUseCAS);

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerChangeWpnCAS);

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

INCLUDE_ASM("asm/main/nonmatchings/player", PlayerGrenadeCASAnimCB);

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
