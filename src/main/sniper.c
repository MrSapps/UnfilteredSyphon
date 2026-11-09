#include "common.h"
#include "gameobj.h"
#include "gobj.h"

/* sniper.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_8001148C[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

/* 16 bytes copied as a block (camera move-info parameters) */
typedef struct SniperQuad {
    s32 w[4];
} SniperQuad;

/* Camera object (gCamera points at the game camera, gSniperCam is the scope camera). */
typedef struct SniperCamera {
    /* 0x000 */ void *display;
    /* 0x004 */ u8 unk004[0x948];
    /* 0x94C */ s32 unk94C;
    /* 0x950 */ u8 unk950[0x34];
    /* 0x984 */ VECTOR32 unk984;
    /* 0x994 */ u8 unk994[0xFC];
    /* 0xA90 */ s32 unkA90;
    /* 0xA94 */ u8 unkA94[0x34];
    /* 0xAC8 */ VECTOR32 unkAC8;
    /* 0xAD8 */ u8 unkAD8[0xFC];
    /* 0xBD4 */ s32 unkBD4;
    /* 0xBD8 */ u8 unkBD8[4];
    /* 0xBDC */ SniperQuad unkBDC;
    /* 0xBEC */ u8 unkBEC[0x20];
    /* 0xC0C */ SniperQuad unkC0C;
    /* 0xC1C */ u8 unkC1C[0xF8];
    /* 0xD14 */ struct SniperMatrix *unkD14;
    /* 0xD18 */ u8 unkD18[4];
    /* 0xD1C */ s32 unkD1C;
    /* 0xD20 */ s32 unkD20[3];
    /* 0xD2C */ u8 unkD2C[0x14];
    /* 0xD40 */ u8 unkD40;
} SniperCamera;

typedef enum AimMode {
    AIM_NONE = -1,
    AIM_GRENADE,
    AIM_GUN,
    AIM_SCOPE
} AimMode;

/* Line-of-sight hit record filled by SniperCheckLOS (D_801156D8). */
typedef struct SniperHitObj {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* low 10 bits: gameObjectsInfo index */
} SniperHitObj;

/* One line-of-sight intersection (losHitsSorted[] entries) */
typedef struct SniperHit {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 kind; /* 1: object, 2: see-through */
    /* 0x0C */ u8 unk0C[4];
    /* 0x10 */ VECTOR32 normal;
    /* 0x20 */ VECTOR32 pos;
    /* 0x30 */ u8 unk30[0x18];
    /* 0x48 */ SniperHitObj *obj;
    /* 0x4C */ struct SniperHitPoly *poly;
} SniperHit; /* size 0x50 */

typedef struct SniperHitPoly {
    /* 0x00 */ s16 unk00;
    /* 0x02 */ s16 flags; /* low 6 bits: material */
} SniperHitPoly;

/* Hit id of a LOS intersection: the object index, or -1 when it is not an object */
#define SNIPER_HIT_ID(hit) ((hit) != NULL && (hit)->kind == 1 ? (hit)->obj->id & 0x3FF : -1)

/* Line-of-sight query (_LOSPointAlongDirection / _LOSPointAlongAngleDirectionYXZ) */
typedef struct LOSRequest {
    /* 0x00 */ VECTOR32 *from;
    /* 0x04 */ VECTOR32 *to; /* filled in */
    /* 0x08 */ GameObjectRuntime *ignore;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ VECTOR32 *dir;
    /* 0x14 */ VECTOR32 *angles;
    /* 0x18 */ s32 length;
    /* 0x1C */ u8 *noHit;
    /* 0x20 */ VECTOR32 *hitPos;
    /* 0x24 */ VECTOR32 *hitNormal;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 *unk2C;
    /* 0x30 */ s32 *unk30;
} LOSRequest;

/* GameObjectInfo +0x24 as read here */
typedef struct SniperObjInfo {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ union {
        u8 weapon; /* explicit weapon, 0 for none */
        u32 flags; /* 0x3000: default weapon kind */
    } w;
    /* 0x28 */ u8 unk28[0x24];
} SniperObjInfo; /* size 0x4C */

typedef struct SniperAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags;
} SniperAnim;

/* .sdata */
s32 gSniperMode = 0;
s16 gSniperView[4] = { 0xC0, 0x78, 0, 0 }; /* scope view rect; interface sets it */
#define D_8010F800 (gSniperView[2])
#define D_8010F802 (gSniperView[3])
u8 mSniperCameraIsActive = 0;
s32 gSniperBonesHit = 0;
s16 gSniperGobjHit = 0;
s16 gSniperGobjBoxHit = 0;
static u16 D_8010F810 = 0xFFFF;
static s32 D_8010F814 = 0x33B; /* sniper-scope camera parameter */
static s32 D_8010F818 = 0x33B; /* game-camera parameter */
s32 sWindowCount = 0;

/* .sbss */
static s32 D_8010FFB8;
static s32 D_8010FFBC;
static s32 D_8010FFC0;
static s32 D_8010FFC4; /* bitmask of active aim modes (bit n = mode n) */
static s32 D_8010FFC8;
static u8 D_8010FFCC;
static s32 D_8010FFD0[2]; /* LOS request out-parameter (8-aligned) */

typedef struct SniperMatrix {
    s32 w[9];
} SniperMatrix;

extern SniperCamera *gCamera;
/* Line-of-sight result (.bss) */
static VECTOR32 D_801156D8;  /* hit position */
static VECTOR32 D_801156E8;  /* hit normal */
static SniperHit D_801156F8; /* what was hit */
static s32 D_80115748[3]; /* > 8 bytes (it is .bss); 0x80115754-0x80115760 left in the blob, sniper or sprite */
extern s32 losWindowCount;
extern s32 losWindowArray[];
extern s32 sWindowArray[];
void _LOSPointAlongDirection(LOSRequest *req);
void _LOSPointAlongAngleDirectionYXZ(LOSRequest *req);
void AnimateGobjRequestAim(GameObjectRuntime *rt, VECTOR32 *hit, s32 arg2, s32 arg3);
s32 TextIsStringShowing(u16 str);
void TextRemoveString(u16 str);
s32 func_800319FC(void);
s32 func_80031A04(void);
s32 func_80031A0C(SniperCamera *cam);

/* .data: the scope camera's mode tables (CameraInit arg8/arg9, cf. gamecam's D_800FE238) */
static struct {
    void *callbacks[3];
    s32 params[63];
} D_8010531C = {
    { func_800319FC, func_80031A04, func_80031A0C },
    {
    0x0, 0x1, 0x0, -0x1, -0x1, -0x1, -0x1, 0x0,
    0x1, 0x2, -0x1, -0x1, -0x1, -0x1, 0x0, 0x1,
    0x0, -0x1, -0x1, -0x1, -0x1, 0x0, 0x1, 0x0,
    -0x1, -0x1, -0x1, -0x1, 0x0, 0x2, 0x2, -0x1,
    -0x1, -0x1, -0x1, 0x0, 0x0, -0x1, -0x1, -0x1,
    -0x1, -0x1, 0x0, 0x2, 0x2, -0x1, -0x1, -0x1,
    -0x1, 0x0, 0x2, 0x2, -0x1, -0x1, -0x1, -0x1,
    0x1, 0x0, -0x1, -0x1, -0x1, -0x1, -0x1,
    },
};
static s32 D_80105424[0x6E] = {
    0x0, 0xB5, 0xA0000, 0xA0000, 0xA0000, 0x0, 0x3320, 0x1980,
    0x3320, 0x0, 0x400, 0x100, 0x400, 0x0, 0xA0000, 0xA0000,
    0xA0000, 0x0, 0x3320, 0x3320, 0x3320, 0x0, 0x100, 0x100,
    0x100, 0x0, 0xA0000, 0xA0000, 0xA0000, 0x0, 0x3320, 0x3320,
    0x3320, 0x0, 0x100, 0x100, 0x100, 0x0, 0xA0000, 0xA0000,
    0xA0000, 0x0, 0x3320, 0x1980, 0x3320, 0x0, 0x400, 0x100,
    0x400, 0x0, 0x1000000, 0x1000000, 0x1000000, 0x0, 0x2466, 0x2466,
    0x2466, 0x0, 0x200, 0x200, 0x200, 0x0, 0xA0000, 0x0,
    0x0, 0x0, 0x9980, 0x0, 0x0, 0x0, 0x400, 0x0,
    0x0, 0x0, 0x1000000, 0x1000000, 0x1000000, 0x0, 0x122D, 0x122D,
    0x122D, 0x0, 0x100, 0x100, 0x100, 0x0, -0xB000, -0xB000,
    -0xB000, 0x0, -0xB60B, -0xB60B, -0xB60B, 0x0, 0x1000, 0x1000,
    0x1000, 0x0, 0x2D82D, 0x0, 0x0, 0x0, 0x5B05B, 0x0,
    0x0, 0x0, 0x800, 0x0, 0x0, 0x0,
};
extern s32 gPlayerOffset;
extern u8 *gLevData;
extern SniperHit *losHitsSorted[];
extern s32 losSortedCount;
extern SniperHit *losBlocker;
extern u8 D_80105F55[][32];  /* per-weapon info, 0x20 bytes each (+0x15) */
extern u32 D_80105F58[][8];  /* per-weapon info, 0x20 bytes each (+0x18) */
extern u32 D_80105F60[][8];  /* per-weapon info +0x20: damage bits */
extern u16 D_80105F62[][16]; /* per-weapon info +0x22: long-range damage */
extern s32 gPlayerNumConsecutiveHits;
extern s16 gDefaultLevelDmgFX;
extern void (*gHitNPCCallback)(s16 id, s32 bones);

/* TargetInfo of the shooter, as used by SniperShoot */
typedef struct SniperShooter {
    /* 0x00 */ u8 unk00[0x60];
    /* 0x60 */ s16 fx;
    /* 0x62 */ u8 unk62[2];
    /* 0x64 */ VECTOR32 hitPos;
    /* 0x74 */ u8 unk74[0x10];
    /* 0x84 */ VECTOR32 hitNormal;
} SniperShooter;

void DamageGOBJ(s16 from, s16 by, s16 target, s16 damage, s16 action);
void DamageExplosion(s32 from, VECTOR32 *pos, s32 arg2);
void DamageGasCloud(s32 from, VECTOR32 *pos, s32 arg2);
void DamageRadius(s16 from, VECTOR32 *pos, s32 damage, s32 arg3, s32 arg4, s32 arg5, s32 arg6);
void DamageTerrain(s32 from, VECTOR32 *pos, VECTOR32 *normal, VECTOR32 *dir, s32 fx);
s32 WeaponGetDeathAction(s32 weapon);
void ParticleHitWall(s32 from, VECTOR32 *pos, VECTOR32 *normal, s32 color);
void ParticleGrenadeLauncher(s16 id, VECTOR32 *pos, s32 arg2);
void ParticleTracer(GameObjectRuntime *rt, VECTOR32 *pos);
void NPCSoundGobj(s32 arg0, s32 from, void *move, s32 arg3);
s32 BulletTextureGet(s32 fx);
void EGSDisplayRegisterDamage(VECTOR32 *pos, VECTOR32 *normal, VECTOR32 *dir, s32 tex, s32 arg4);
extern void *gHeadShotMsg;
typedef struct SniperSysInfo {
    s16 level;
} SniperSysInfo;
extern SniperSysInfo gSysInfo;

extern u8 gPlayerRadioFlag;
int ratan2(int y, int x);
void NPCPickAimer(s16 id, s32 angle);
s32 HMDLOSLoop(GameObjectAnim *anim);
u16 TextAddMarker(u16 id, void *msg, s32 duration, s32 arg3);
void TextChangeStringDuration(u16 str, s32 ticks);
extern SniperCamera gSniperCam;
extern GameObjectRuntime *gObjPlayer;
extern s32 gCurrentWeapon;
extern s32 gInterfaceCurrentMode;
extern s32 gExeTick;

void GameCameraSetModeGrenade(s32 on, s32 arg1, s32 arg2);
void GameCameraSetModeAim(s32 on, s32 arg1, s32 arg2);
void GameCameraSetModeSniper(s32 on, s32 arg1, s32 arg2);
u8 GameCameraCheckMode(s32 mode);
void InterfaceSetMode(s32 mode);
void CameraAccessMoveInfo(SniperCamera *cam, s32 arg1, s32 arg2, s32 arg3);
void EGSDisplayAddAnim(void *display, void *anim);
void EGSDisplayDeleteAnim(void *display, void *anim);
void CameraInit(SniperCamera *cam, s32 arg1, s32 arg2, s32 arg3, s32 arg4, void *view, s32 arg6,
                s32 arg7, void *arg8, void *arg9, s32 arg10, s32 arg11);
void CameraSetParent(SniperCamera *cam, SniperCamera *parent);
void CameraActivateAndDisplay(SniperCamera *cam, s32 on);
void CameraAccessFrameInfo(SniperCamera *cam, s32 arg1, s32 arg2, s32 arg3);
void EGSCameraSetProjectionAngle(void *display, s32 angle);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(void *a, void *b, s32 *dist);
void SniperCheckLOS(GameObjectRuntime *rt);
static int func_800303DC(void);
static void func_8003041C(int mode);
static void func_800305C4(void);

void SniperInit(void) {
    D_8010FFBC = -1;
    D_8010FFB8 = 0;
    D_8010FFC0 = 0;
    D_8010F810 = 0xFFFF;
    D_8010FFC4 = 0;
}

/* Highest active aim mode, -1 for none. */
static int func_800303DC(void) {
    int flags = D_8010FFC4;
    int i = 2;

    while (!((flags >> i) & 1)) {
        if (--i < 0) {
            break;
        }
    }
    return i;
}

static void func_8003041C(int mode) {
    if (func_800303DC() < mode) {
        switch (mode) {
        case 0:
            GameCameraSetModeGrenade(1, 0, 0);
            gSniperMode = 1;
            break;
        case 1:
            GameCameraSetModeAim(1, 0, 0);
            gSniperMode = mode;
            EGSDisplayDeleteAnim(gCamera->display, gObjPlayer->anim);
            break;
        case 2:
            GameCameraSetModeSniper(1, 0, 0);
            if (gCurrentWeapon == 12) {
                gSniperMode = 3;
                InterfaceSetMode(3);
                gCamera->unkD1C = D_8010F818;
                gCamera->unkD40 = 1;
                CameraAccessMoveInfo(gCamera, 1, 8, 1);
            } else if (gCurrentWeapon == 13) {
                gSniperMode = mode;
                InterfaceSetMode(2);
                gSniperCam.unkD1C = D_8010F814;
                gSniperCam.unkD40 = 1;
                CameraAccessMoveInfo(&gSniperCam, 1, 8, 1);
            } else if (gCurrentWeapon == 18) {
                gSniperMode = 4;
                InterfaceSetMode(5);
            }
            EGSDisplayDeleteAnim(gCamera->display, gObjPlayer->anim);
            break;
        case AIM_NONE:
        default:
            return;
        }
    }
    if (mode >= 0 && mode < 3) {
        D_8010FFC4 |= 1 << mode;
    }
}

static void func_800305C4(void) {
    int mode = func_800303DC();

    switch (mode) {
    case 0:
        GameCameraSetModeGrenade(0, 0, 0);
        break;
    case 1:
        GameCameraSetModeAim(0, 0, 0);
        goto add_anim;
    case 2:
        if (GameCameraCheckMode(16)) {
            if (gCurrentWeapon == 12) {
                if (gCamera->unkBD4 == 0) {
                    D_8010F818 = gCamera->unkC0C.w[0];
                } else {
                    *(SniperQuad *)&D_8010F818 = gCamera->unkC0C;
                }
            } else if (gCurrentWeapon == 13) {
                if (gSniperCam.unkBD4 == 0) {
                    D_8010F814 = gSniperCam.unkC0C.w[0];
                } else {
                    *(SniperQuad *)&D_8010F814 = gSniperCam.unkC0C;
                }
            }
        }
        GameCameraSetModeSniper(0, 0, 0);
    add_anim:
        EGSDisplayAddAnim(gCamera->display, gObjPlayer->anim);
        ((SniperAnim *)gObjPlayer->anim)->flags &= ~0x10;
        break;
    case AIM_NONE:
    default:
        return;
    }
    gSniperMode = 0;
    if (gInterfaceCurrentMode) {
        InterfaceSetMode(1);
    }
    D_8010FFC4 &= ~(1 << mode);
    mode = func_800303DC();
    if (mode >= 0) {
        D_8010FFC4 &= ~(1 << mode);
        func_8003041C(mode);
    }
}

u8 SniperSetMode(u8 on) {
    int mode;

    if (on == 1) {
        if ((gCurrentWeapon == 12 || gCurrentWeapon == 13 || gCurrentWeapon == 18) &&
            gObjPlayer->cas->active != 8) {
            mode = 2;
        } else {
            mode = 1;
            if (gCurrentWeapon == 19 || gCurrentWeapon == 20) {
                mode = 0;
            }
        }
        func_8003041C(mode);
    } else {
        func_800305C4();
    }
    return gSniperMode != 0;
}

u8 SniperToggleMode(void) {
    return SniperSetMode(gSniperMode == 0);
}

s32 SniperSetupCamera(void) {
    CameraInit(&gSniperCam, 0, 0, 0, 0x33B, gSniperView, 2, 1000, &D_8010531C, D_80105424, 0, 0);
    CameraSetParent(&gSniperCam, gCamera);
    gSniperCam.unkD1C = 0xB5;
    gSniperCam.unkD40 = 1;
    CameraAccessMoveInfo(&gSniperCam, 1, 8, 2);
    gSniperCam.unkD1C = 0x33B;
    gSniperCam.unkD40 = 1;
    CameraAccessMoveInfo(&gSniperCam, 1, 8, 3);
    D_8010F814 = 0x33B;
    D_8010F818 = 0x33B;
    return 1;
}

SniperCamera *SniperGetCam(void) {
    switch ((u32)gSniperMode) {
    case 2:
        return &gSniperCam;
    case 3:
    case 4:
        return gCamera;
    }
    return NULL;
}

VECTOR32 *SniperGetTarget(s32 *outId) {
    s32 id;
    s32 result;

    result = -1;
    if (outId != NULL) {
        if (D_801156F8.kind == 1) {
            id = SNIPER_HIT_ID(&D_801156F8);
            if (id >= 0 && id < gLevelGameObjCount &&
                gameObjectsInfo[id].runtime->npcState == 2 && gSniperBonesHit) {
                result = id;
            }
        }
        *outId = result;
    }
    return &D_801156D8;
}

void SniperCheckHits(s32 weapon) {
    s32 count = losSortedCount;
    s32 i;
    s32 id;
    SniperHit *hit;
    GameObjectRuntime *obj;
    s32 bones;

    gSniperBonesHit = 0;
    gSniperGobjHit = -1;
    gSniperGobjBoxHit = -1;
    for (i = 0; i < count; i++) {
        hit = losHitsSorted[i];
        id = SNIPER_HIT_ID(hit);
        if (hit->kind != 2) {
            losBlocker = hit;
        }
        if (id == -1) {
            if (hit->kind == 2) {
                continue;
            }
            return;
        }
        obj = gameObjectsInfo[id].runtime;
        if (obj->npcState != 2) {
            return;
        }
        bones = HMDLOSLoop(obj->anim);
        if (bones != 0) {
            gSniperBonesHit = bones;
            gSniperGobjHit = id;
            /* head shot on a live NPC */
            if (!(bones & 1) || obj->healthInfo->health <= 0 || gSniperMode != 1) {
                return;
            }
            if (weapon != 14 && D_80105F55[weapon][0] * ((D_80105F58[weapon][0] & 7) + 1) == 0) {
                return;
            }
            if (gSysInfo.level == 4) {
                return;
            }
            if (D_8010F810 == 0xFFFF) {
                D_8010F810 = TextAddMarker(id, gHeadShotMsg, 40, 0);
            } else {
                TextChangeStringDuration(D_8010F810, 40);
            }
            return;
        }
        gSniperGobjBoxHit = id;
    }
}

/* Camera parameter: a scalar (first word only) or a full vector */
#define CAMERA_GET_PARAM(flag, src, dst) \
    if ((flag) == 0) {                   \
        (dst).vx = (src).vx;             \
    } else {                             \
        (dst) = (src);                   \
    }

#define CAMERA_MATRIX() (*(GOBJ_MATRIX **)gCamera->display)

#define LOS_REQUEST_INIT(req, fromv, field, dirv) \
    (req).from = &(fromv);                        \
    (req).ignore = rt;                            \
    (req).unk0C = 0;                              \
    (req).field = &(dirv);                        \
    (req).length = 0x1900;                        \
    (req).noHit = &D_8010FFCC;                    \
    (req).hitPos = &D_801156D8;                   \
    (req).hitNormal = &D_801156E8;                \
    (req).unk28 = 0;                              \
    (req).unk2C = D_8010FFD0;                     \
    (req).unk30 = D_80115748; 

void SniperCheckLOS(GameObjectRuntime *rt) {
    s32 weapon = gCurrentWeapon;
    u32 i;
    SniperHit *blocker;
    VECTOR32 dir;
    VECTOR32 pos;
    VECTOR32 unused; /* unused; keeps the original stack layout */

    if (gSniperMode == 1) {
        if (weapon == 19 || weapon == 20) {
            CAMERA_GET_PARAM(gCamera->unk94C, gCamera->unk984, dir);
            CameraAccessFrameInfo(gCamera, 0, 1, 1);
            pos = *(VECTOR32 *)&gCamera->unkD1C;
        } else {
            VECTOR32 a;
            VECTOR32 b;
            s32 unused[2]; /* unused; keeps the original stack layout */

            CAMERA_GET_PARAM(gCamera->unk94C, gCamera->unk984, a);
            CAMERA_GET_PARAM(gCamera->unkA90, gCamera->unkAC8, b);
            dir.vx = a.vx - b.vx;
            dir.vy = a.vy - b.vy;
            dir.vz = a.vz - b.vz;
            pos.vx = CAMERA_MATRIX()->t[0];
            pos.vy = CAMERA_MATRIX()->t[1];
            pos.vz = CAMERA_MATRIX()->t[2];
            pos.vy = -pos.vy;
        }
        {
            LOSRequest req;

            LOS_REQUEST_INIT(req, pos, angles, dir);
            _LOSPointAlongAngleDirectionYXZ(&req);
        }
        AnimateGobjRequestAim(rt, &D_801156D8, 0, 0);
    } else {
        VECTOR32 eye;
        VECTOR32 fwd;
        LOSRequest req;

        eye.vx = CAMERA_MATRIX()->t[0];
        eye.vy = CAMERA_MATRIX()->t[1];
        eye.vz = CAMERA_MATRIX()->t[2];
        eye.vy = -eye.vy;
        fwd.vx = CAMERA_MATRIX()->m[0][2];
        fwd.vy = CAMERA_MATRIX()->m[1][2];
        fwd.vz = CAMERA_MATRIX()->m[2][2];
        fwd.vy = -fwd.vy;
        LOS_REQUEST_INIT(req, eye, dir, fwd);
        _LOSPointAlongDirection(&req);
    }
    SniperCheckHits(weapon);
    if (D_8010F810 != 0xFFFF) {
        if (!TextIsStringShowing(D_8010F810)) {
            D_8010F810 = 0xFFFF;
        } else if (gSniperGobjHit == -1 || !(gSniperBonesHit & 1)) {
            TextRemoveString(D_8010F810);
            D_8010F810 = 0xFFFF;
        }
    }
    blocker = losBlocker;
    if (blocker != NULL) {
        D_801156D8 = blocker->pos;
        D_801156E8 = blocker->normal;
        D_801156F8 = *blocker;
        D_8010FFC8 = losBlocker->poly->flags;
        D_8010FFC8 &= 0x3F;
    } else {
        D_8010FFCC = 1;
    }
    for (i = 0; i < losWindowCount; i++) {
        sWindowArray[i] = losWindowArray[i];
    }
    sWindowCount = losWindowCount;
}

void SniperUpdate(void) {
    if (gSniperMode) {
        SniperCheckLOS(gameObjectsInfo[gPlayerOffset].runtime);
    }
    if (!mSniperCameraIsActive && D_8010F802 && D_8010F800) {
        mSniperCameraIsActive = 1;
        CameraActivateAndDisplay(&gSniperCam, 1);
    } else if (mSniperCameraIsActive && (!D_8010F802 || !D_8010F800)) {
        gSniperCam.unkD1C = 0x33B;
        CameraAccessMoveInfo(&gSniperCam, 1, 8, 0);
        mSniperCameraIsActive = 0;
        CameraActivateAndDisplay(&gSniperCam, 0);
    }
}

/* Weapon held by object `id` (the player's current one, else from its placement data) */
#define GOBJ_INFO_W(id) (((SniperObjInfo *)&gameObjectsInfo[id])->w)
#define GOBJ_WEAPON(id)                                                                      \
    ((id) != gPlayerOffset                                                                   \
         ? (GOBJ_INFO_W(id).weapon ? GOBJ_INFO_W(id).weapon                                  \
            : (GOBJ_INFO_W(id).flags & 0x3000) == 0x1000 ? 19                                \
            : (GOBJ_INFO_W(id).flags & 0x3000) == 0x2000 ? 20                                \
                                                         : 0)                                \
         : gCurrentWeapon)

/* MoveInfo as used here */
typedef struct SniperMove {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ VECTOR32 unk10;
    /* 0x20 */ VECTOR32 head;
} SniperMove;

typedef struct SniperFrame {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} SniperFrame;

typedef struct SniperAnimFrame {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ SniperFrame *frame;
} SniperAnimFrame;

#if !defined(_PSYQ) && !defined(NON_MATCHING)
/* SniperShoot (still INCLUDE_ASM) addresses these through their splat labels */
__asm__(
        ".set D_801156DC, D_801156D8+0x4\n"
        ".set D_801156E0, D_801156D8+0x8\n"
        ".set D_80115700, D_801156F8+0x8\n"
        ".set D_80115740, D_801156F8+0x48\n");
#endif

#ifdef NON_MATCHING
/* 28 masked lines left. The do { } while (0) around the kind == 1 branch doubles its refs
 * (flow.c loop weight) and fixed damage/rt/i -> $s0/$s1/$s2. Left: at.vy temp gets v1 not v0
 * (local-alloc), the dist if/else delay slot (orig repeats `sll` in the else arm), radius not
 * computed straight into a2, fx/who s-reg swap, and the GrenadeLauncher/Tracer arg order
 * (orig sets a2 / la a1 first; ternary/cast-prototype variants did not change it).
 * r7L: the original stores at.vy twice (t[1], then -vy after the t[2] load): `at.vz = ..;
 * at.vy = -at.vy;` gives that shape but swaps the v0/v1 temps (31). The arg order is a sched1
 * LUID tie (a1/a2 both priority 1, the later-emitted a2 lands next to the jal). */
void SniperShoot(void) {
    VECTOR32 from;
    VECTOR32 at;
    s32 dist;
    s32 weapon;
    s32 damage = 0;
    u32 i;
    GameObjectRuntime *rt;
    s32 bones;

    from = ((SniperMove *)gObjPlayer->move)->pos;
    weapon = gCurrentWeapon;
    ((SniperShooter *)gObjPlayer->target)->hitPos = D_801156D8;
    ((SniperShooter *)gObjPlayer->target)->hitNormal = D_801156E8;
    if (sWindowCount != 0) {
        for (i = 0; i < sWindowCount; i++) {
            DamageGOBJ(gPlayerOffset, gPlayerOffset, ((GameObjectRuntime *)sWindowArray[i])->id,
                       (D_80105F60[weapon][0] >> 6) & 0x3FF, 0x892);
        }
    }
    if (D_8010FFCC == 0) {
        if (weapon == 19) {
            DamageExplosion(gPlayerOffset, &D_801156D8, 1);
            return;
        }
        if (weapon == 20) {
            DamageGasCloud(gPlayerOffset, &D_801156D8, 1);
            return;
        }
        if (D_801156F8.kind == 1) {
            do {
                rt = gameObjectsInfo[SNIPER_HIT_ID(&D_801156F8)].runtime;
                if (rt->npcState == 2) {
                    if (gSniperBonesHit) {
                        if (rt->healthInfo->health != 0x7FFF) {
                            ((SniperShooter *)gObjPlayer->target)->hitPos = *SniperGetTarget(NULL);
                            if (gHitNPCCallback != NULL) {
                                gHitNPCCallback(rt->id, gSniperBonesHit);
                            }
                            bones = gSniperBonesHit;
                            if (bones & 1) {
                                damage = (D_80105F60[weapon][0] >> 1) & 0x7FE0;
                            } else if (bones & 0x6000) {
                                damage = D_80105F60[weapon][0] >> 6;
                                damage &= 0x3FF;
                                if (rt->id == gPlayerOffset || (GOBJ_INFO_W(rt->id).flags & 0x4000)) {
                                    damage = -damage;
                                }
                            } else {
                                damage = 0;
                                if (bones & 0x8000) {
                                    ParticleHitWall(gPlayerOffset, &D_801156D8, &D_801156E8, 0xC0FFFF);
                                }
                            }
                        } else {
                            damage = 0;
                        }
                    } else if (gSniperGobjBoxHit >= 0) {
                        NPCSoundGobj(1, gPlayerOffset, gameObjectsInfo[gSniperGobjBoxHit].runtime->move, 0x1E0);
                    }
                } else {
                    at.vx = ((SniperAnimFrame *)rt->anim)->frame->t[0];
                    at.vy = ((SniperAnimFrame *)rt->anim)->frame->t[1];
                    at.vy = -at.vy;
                    at.vz = ((SniperAnimFrame *)rt->anim)->frame->t[2];
                    EGSGeometry_GetFuzzyDistancePoint2Point(&from, &at, &dist);
                    if (dist < 0x3C1) {
                        damage = D_80105F60[weapon][0] >> 6;
                    } else {
                        damage = D_80105F62[weapon][0];
                    }
                    damage &= 0x3FF;
                }
                if (damage == 0) {
                    return;
                }
                if ((s32)((D_80105F60[weapon][0] >> 6) & 0x3FF) < damage) {
                    DamageGOBJ(gPlayerOffset, gPlayerOffset, rt->id, 0x7FFF, 0x58);
                } else {
                    DamageGOBJ(gPlayerOffset, gPlayerOffset, rt->id, damage, WeaponGetDeathAction(weapon));
                }
                if (rt->npcState != 2) {
                    return;
                }
                gPlayerNumConsecutiveHits = gPlayerNumConsecutiveHits < 0 ? 1 : gPlayerNumConsecutiveHits + 1;
            } while (0);
            return;
        } else {
            VECTOR32 dir;
            s32 fx;
            s32 who;
            s32 tex;
            s32 w;

            dir.vx = from.vx - D_801156D8.vx;
            dir.vy = from.vy - D_801156D8.vy;
            dir.vz = from.vz - D_801156D8.vz;
            w = gCurrentWeapon;
            if (D_8010FFC8 == 0) {
                D_8010FFC8 = gDefaultLevelDmgFX;
            }
            ((SniperShooter *)gObjPlayer->target)->fx = D_8010FFC8;
            if (w == 6 || w == 7) {
                s32 dist2;
                s32 radius;

                EGSGeometry_GetFuzzyDistancePoint2Point(&from, &at, &dist2);
                if (dist2 < 0x3C1) {
                    radius = D_80105F60[w][0] >> 6;
                } else {
                    radius = D_80105F62[w][0];
                }
                radius &= 0x3FF;
                DamageRadius(gPlayerOffset, &D_801156D8, radius, 0x60, 0x20, 0x11, 0);
            }
            fx = D_8010FFC8;
            who = gPlayerOffset;
            tex = BulletTextureGet(fx);
            if (tex != 0) {
                EGSDisplayRegisterDamage(&D_801156D8, &D_801156E8, &dir, tex, 0);
            }
            DamageTerrain(who, &D_801156D8, &D_801156E8, &dir, fx);
        }
    } else if (weapon == 16) {
        ParticleGrenadeLauncher(gObjPlayer->id, &D_801156D8, 0);
    } else {
        ((SniperShooter *)gObjPlayer->target)->fx = 0x19;
        ParticleTracer(gObjPlayer, &D_801156D8);
    }
    gPlayerNumConsecutiveHits = gPlayerNumConsecutiveHits > 0 ? -1 : gPlayerNumConsecutiveHits - 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/sniper", SniperShoot);
#endif

/* referenced from .data (camera callbacks) */
s32 func_800319FC(void) {
    return 1;
}

/* referenced from .data (camera callbacks) */
s32 func_80031A04(void) {
    return 1;
}

/* referenced from .data (camera callbacks) */
s32 func_80031A0C(SniperCamera *cam) {
    /* Only 8 bytes, but the full 16-byte block is copied into it (overrunning into the
     * saved registers) when cam->unkBD4 is set. */
    s32 proj[2];

    if (cam->unkBD4 == 0) {
        proj[0] = cam->unkBDC.w[0];
    } else {
        *(SniperQuad *)proj = cam->unkBDC;
    }
    EGSCameraSetProjectionAngle(cam->display, proj[0]);
    CameraAccessFrameInfo(cam, 0, 0, 9);
    proj[1] = (s32)cam->unkD14;
    if (proj[1]) {
        **(SniperMatrix **)cam->display = *(SniperMatrix *)proj[1];
    }
    return 1;
}

/* func_80031AEC..func_800323C4 (target scoring actions) are target.c's: target.c's .sbss
 * (8-aligned in psylink) starts with D_8010FFD8, which they use. */
