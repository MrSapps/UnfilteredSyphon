#include "common.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "scratch.h"

/* gamecam.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FF78)
SDATA_LOCAL(gCamera)

/* Not system.h: its EGSListAdd prototype returns void (egsmisc.c returns the new head). */
typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

EGSListNode *EGSListAdd(EGSListNode **list, void *data);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
void EGSListFlush(EGSListNode **list);
extern void **gCamera;

/* Script camera position slot (D_80113B2C.slots), 0x14 bytes. */
typedef struct GameCamSlot {
    /* 0x00 */ s32 mode; /* -1 when free */
    /* 0x04 */ VECTOR32 pos;
} GameCamSlot;

/* Camera shake (GameCameraShakeStart), 0x44 bytes, five slots. */
typedef struct GameCamShake {
    /* 0x00 */ u8 active;
    /* 0x04 */ s32 magnitude; /* <0: until stopped */
    /* 0x08 */ s32 rate;
    /* 0x0C */ VECTOR32 axis;
    /* 0x1C */ EGSFrame *frame; /* points at ownFrame, or a caller's frame */
    /* 0x20 */ EGSFrame ownFrame;
    /* 0x44 */
} GameCamShake;

/* Per-mode settings (D_80113BA0), 0x3C bytes. */
typedef struct GameCamModeInfo {
    /* 0x00 */ s32 view; /* index into D_80113B9C */
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ VECTOR32 lead;
    /* 0x28 */ VECTOR32 watch;
    /* 0x38 */ u8 unk38;
} GameCamModeInfo;

/* Camera view parameters (D_80113B9C), 0x9C bytes. */
typedef struct GameCamView {
    /* 0x00 */ VECTOR32 lead;
    /* 0x10 */ VECTOR32 watch;
    /* 0x20 */ u8 unk20[0x20];
    /* 0x40 */ s32 distance;
    /* 0x44 */ s32 pitch;
    /* 0x48 */ u8 unk48[0x54];
} GameCamView;

/* Game camera state, 0x80113B2C. */
typedef struct GameCamState {
    /* 0x000 */ Gobj *primary;
    /* 0x004 */ Gobj *target;
    /* 0x008 */ s32 unk008;
    /* 0x00C */ EGSFrame *fixedFrame; /* frame the fixed/interaction modes look from */
    /* 0x010 */ EGSFrame frame10;
    /* 0x034 */ EGSFrame frame34;
    /* 0x058 */ s32 mode;
    /* 0x05C */ s32 unk05C;
    /* 0x060 */ s32 action;
    /* 0x064 */ u32 modeMask;   /* bit per active mode */
    /* 0x068 */ s32 joystickMode;
    /* 0x06C */ u32 joystickMask; /* bit per requested joystick mode */
    /* 0x070 */ GameCamView *views;
    /* 0x074 */ GameCamModeInfo *modes;
    /* 0x078 */ s32 *modeData;  /* per mode */
    /* 0x07C */ s32 *modeJoystick; /* per mode: joystick mask bit */
    /* 0x080 */ s32 *joystickModes; /* per joystick mask bit */
    /* 0x084 */ u8 collision;
    /* 0x085 */ u8 unk085;
    /* 0x086 */ u8 cut;
    /* 0x088 */ s32 unk088;
    /* 0x08C */ s32 unk08C;
    /* 0x090 */ s32 unk090;
    /* 0x094 */ s32 unk094;
    /* 0x098 */ void *scrimAnim;
    /* 0x09C */ s32 scrimRate; /* <0: scrolling at -rate */
    /* 0x0A0 */ s8 scrimOn;    /* -1: no scrim */
    /* 0x0A4 */ EGSListNode *shakeList;
    /* 0x0A8 */ GameCamShake shakes[5];
    /* 0x1FC */ u8 unk1FC;
    /* 0x1FD */ u8 unk1FD;
    /* 0x1FE */ u8 unk1FE;
    /* 0x200 */ s32 transitionTime;
    /* 0x204 */ EGSListNode *slotList;
    /* 0x208 */ GameCamSlot slots[4];
} GameCamState;

extern GameCamState D_80113B2C;
extern s32 D_8010FF78;
extern s32 gInterfaceCurrentMode;

void GameSysJoystickSetMode(s32 mode);
void func_8001B000(s32 data, u8 on, u8 flag);
u8 func_8001C2F4(s32 mode, u8 on, u8 *flag, s32 *data);
s32 func_8001C3AC(s32 mode, u8 on);
void CameraDisplay(void *camera, u8 on);
void CameraActivate(void *camera, u8 on);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, void *arg);
void GameCameraActivate();
u8 func_80022318(u8 *on, s32 mode, VECTOR32 *pos, GameCamSlot *slot);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void GameCameraSetMode(s32 mode, s32 arg1, s32 arg2, s32 arg3, VECTOR32 *lead, VECTOR32 *watch, GameCamSlot *slot, u8 on, u8 flag);
extern s32 D_800FE4F8[]; /* modes with a default distance, -1 terminated */
extern s32 D_800FE54C[]; /* modes with a default pitch, -1 terminated */
void GameCameraModeSetDefaultDistance(s32 mode, s32 distance);
void GameCameraModeSetDefaultPitchAngle(s32 mode, s32 pitch);
void CameraGetDisplay(void *camera, void **display);
void EGSDisplayDeletePoly(void *display, void *poly);
void GSAddAnim(void *anim);
void GSDeleteAnim(void *anim);
extern void *gLevData;
extern u8 mPMI[][0x20];
EGSListNode *func_8002227C(s32 mode, VECTOR32 *pos, GameCamSlot *slot);
void iScrimRemoveVRAMScroll(void);
u8 func_80022414(s32 mode, VECTOR32 *pos, GameCamSlot *slot);
u8 func_800224F8(s32 mode, VECTOR32 *pos, GameCamSlot *slot);
s32 GameCameraSetModeDefault(u8 on, Gobj *primary, u8 arg2);

/* What gCamera points at, as far as this file goes. */
/* Animated camera parameter: mode 0 means only value.vx is meaningful. */
typedef struct GameCamParam {
    /* 0x00 */ s32 mode;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ VECTOR32 value;
    /* 0x18 */ u8 unk18[0x20];
    /* 0x38 */ VECTOR32 target;
} GameCamParam; /* 0x48 */

/* The game camera (what gCamera points at), as far as this file goes. */
typedef struct GameCamCamera {
    /* 0x000 */ void **display;
    /* 0x004 */ u8 unk004[0x1B0];
    /* 0x1B4 */ GameCamParam pos;
    /* 0x1FC */ u8 unk1FC[0x240];
    /* 0x43C */ GameCamParam watch;
    /* 0x484 */ u8 unk484[0xFC];
    /* 0x580 */ GameCamParam eye;
    /* 0x5C8 */ u8 unk5C8[0x240];
    /* 0x808 */ GameCamParam dist;
    /* 0x850 */ u8 unk850[0xFC];
    /* 0x94C */ GameCamParam angle;
    /* 0x994 */ u8 unk994[0xFC];
    /* 0xA90 */ GameCamParam unkA90;
    /* 0xAD8 */ u8 unkAD8[0xFC];
    /* 0xBD4 */ GameCamParam fov;
    /* 0xC1C */ u8 unkC1C[0x100];
    /* 0xD1C */ VECTOR32 unkD1C;
    /* 0xD2C */ s32 unkD2C;
    /* 0xD30 */ VECTOR32 unkD30;
    /* 0xD40 */ u8 unkD40;
} GameCamCamera;

#define CAM ((GameCamCamera *)gCamera)

extern const VECTOR32 D_800101E4; /* {1, 1, 1} */
extern const VECTOR32 D_800101F4; /* zero */
void EGSGeometry_VectorRotateYXZ(VECTOR32 *in, VECTOR32 *angle, VECTOR32 *out);
void CameraAccessMoveInfo(void *camera, s32 arg1, s32 arg2, s32 arg3);
u8 EGSListFind(EGSListNode **list, s32 arg1, void *data);

typedef struct GameCamSVec {
    s16 vx, vy, vz, pad;
} GameCamSVec;

/* Interpolated camera parameter (func_8001FF08 and friends). */
typedef struct GameCamPlane {
    /* 0x00 */ s32 count;    /* vertices */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ GameCamSVec *normal;
    /* 0x0C */ u8 unk0C[8];
    /* 0x14 */ GameCamSVec *verts[5];
} GameCamPlane;

typedef struct GameCamTrack {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ VECTOR32 vel;
    /* 0x24 */ GameCamPlane *plane;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 moving;
    /* 0x39 */ u8 pad39[3];
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ VECTOR32 unk44;
    /* 0x54 */ VECTOR32 unk54;
    /* 0x64 */ VECTOR32 unk64;
    /* 0x74 */ VECTOR32 unk74;
} GameCamTrack;

void func_8001FF08(GameCamTrack *track);
u8 func_800203F0(VECTOR32 *pos, GameCamPlane *plane, s32 radius);
void func_80020978(GameCamTrack *track);

extern s32 gExeTick;
int EGSMathFIXMul(int a, int b);
void EGSDisplayAddPoly(void *display, void *poly);

/* Scrim animation (GSAddAnim); its frame is at 0xC. */
typedef struct GameCamAnim {
    /* 0x0 */ u8 unk0[0xC];
    /* 0xC */ EGSFrame *frame;
} GameCamAnim;

#define ABS(x) ((x) < 0 ? -(x) : (x))

void func_8001E76C(void);
void func_8001D91C(void);
void func_8001E1DC(void);
void func_8001AF98(void);
s32 func_8001E218(s32 arg0);
s32 func_8001E5D8(VECTOR32 *out);
s32 func_80021BA4(void);
u8 GameCameraCheckMode(s32 mode);

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define FIX12(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

/* Axis-aligned box. */
typedef struct GameCamBox {
    VECTOR32 min;
    VECTOR32 max;
} GameCamBox;

void RotMatrixYXZ(GameCamSVec *rot, GOBJ_MATRIX *m);
void CameraAccessFrameInfo(void *camera, s32 arg1, s32 arg2, s32 arg3);
u8 func_80021230(VECTOR32 *cur, VECTOR32 *d, VECTOR32 *dir, VECTOR32 *pos);

extern GameCamView D_800FCF84[];
extern GameCamModeInfo D_800FDC50[];
extern s32 D_800FE13C[];
extern s32 D_800FE190[];
extern s32 D_800FE1E4[];
extern u8 D_8010F714[];
extern u8 D_800FE238[];
extern u8 D_800FE340[];
int rcos(int a);
int rsin(int a);
void *memset(void *p, int c, int n);
void CameraInit(void *camera, s32 arg1, VECTOR32 *pos, VECTOR32 *angle, s32 proj, void *arg5, s32 arg6, s32 arg7, void *arg8, void *arg9, s32 arg10, s32 arg11);

int EGSMathFIXDiv(int a, int b);

#define abs(x) __builtin_abs(x)

/* Convex outline in the xz plane (camera bounds). */
typedef struct GameCamOutline {
    /* 0x00 */ s32 count;
    /* 0x04 */ VECTOR32 v[1];
} GameCamOutline;

/* Cheap |(x, z)| (octagonal approximation). */
static inline s32 iApproxLength(s32 x, s32 z) {
    s32 ax = abs(x);
    s32 az = abs(z);
    s32 sum = ax + az;
    s32 diff = ax - az;
    s32 half = sum >> 1;
    s32 quarter = sum >> 2;

    if (diff < 0) {
        diff = az - ax;
    }
    if (diff < half) {
        sum -= quarter;
    } else if (diff <= half + quarter) {
        sum -= sum >> 3;
    }
    return sum;
}

static inline void iDotXZ(VECTOR32 *a, VECTOR32 *b, s32 *out) {
    *out = a->vx * b->vx + a->vz * b->vz;
}

static inline void iDot3(VECTOR32 *a, VECTOR32 *b, s32 *out) {
    *out = a->vx * b->vx + a->vy * b->vy + a->vz * b->vz;
}

static inline void iFixDiv(s32 a, s32 b, s32 *out) {
    *out = EGSMathFIXDiv(a, b);
}

typedef struct GameCamOutline5 {
    /* 0x00 */ s32 count;
    /* 0x04 */ VECTOR32 v[5];
} GameCamOutline5;

void EGSCameraSetProjectionAngle(void *camera, s32 angle);
void EGSFrameSetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *dir, s32 arg3);
void func_8001EF10(void);
void func_80023D70(void);

void EGSMath_VectorModulus(VECTOR32 *v, s32 *out);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);

INCLUDE_RODATA("asm/main/nonmatchings/gamecam", D_800101E4);

INCLUDE_RODATA("asm/main/nonmatchings/gamecam", D_800101F4);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001B000);

u8 func_8001C2F4(s32 mode, u8 on, u8 *flag, s32 *data) {
    s32 i;

    if (!on) {
        if (!(D_80113B2C.modeMask & (1 << mode))) {
            return 0;
        }
        D_80113B2C.modeMask &= ~(1 << mode);
    } else {
        D_80113B2C.modeMask |= 1 << mode;
    }
    for (i = 20; i >= 0; i--) {
        if (D_80113B2C.modeMask & (1 << i)) {
            if (mode < i) {
                return 0;
            }
            *data = D_80113B2C.modeData[i];
            return 1;
        }
    }
    return 0;
}

s32 func_8001C3AC(s32 mode, u8 on) {
    s32 bit = D_80113B2C.modeJoystick[mode];
    s32 i;

    if (!on) {
        if (!(D_80113B2C.joystickMask & (1 << bit))) {
            return 0;
        }
        D_80113B2C.joystickMask &= ~(1 << bit);
    } else {
        D_80113B2C.joystickMask |= 1 << bit;
    }
    for (i = 20; i >= 0; i--) {
        if (D_80113B2C.joystickMask & (1 << i)) {
            GameSysJoystickSetMode(D_80113B2C.joystickModes[i]);
            D_80113B2C.joystickMode = D_80113B2C.joystickModes[i];
            return 1;
        }
    }
    GameSysJoystickSetMode(5);
    D_80113B2C.joystickMode = 5;
    return 1;
}

s32 GameCameraGetMode(void) {
    return D_80113B2C.mode;
}

u8 GameCameraCheckMode(s32 mode) {
    return D_80113B2C.mode == mode;
}

s32 GameCameraGetAction(void) {
    return D_80113B2C.action;
}

s32 GameCameraCheckAction(s32 action) {
    return D_80113B2C.action == action;
}

void GameCameraSetAction(s32 action) {
    D_80113B2C.action = action;
}

s32 GameCameraCheckJoystickMode(s32 mode) {
    return D_80113B2C.joystickMode == mode;
}

s32 GameCameraSetModeDefault(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 0;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeFixedDirection(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 1;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeFollowAndWatch(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 2;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeGetBehind(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 3;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeSnapToWall(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 4;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeHang(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 5;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeWaist(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 6;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeHead(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 7;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeStrafe(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 8;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeTarget(u8 on, Gobj *primary, Gobj *target, u8 arg3) {
    u8 flag = arg3;
    s32 data;
    s32 mode = 9;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        if (target != NULL) {
            D_80113B2C.target = target;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeFirstPerson(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 10;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeRail(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 11;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeElevator(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 12;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeFixed(u8 on, Gobj *primary, VECTOR32 *pos, GameCamSlot *slot, u8 arg4) {
    u8 flag;
    s32 data;
    s32 mode = 13;

    flag = arg4;
    if (func_80022318(&on, mode, pos, slot)) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        if (pos != NULL) {
            EGSFrameSetPosition(&D_80113B2C.frame34, 0, pos);
        } else {
            VECTOR32 v;

            v.vx = (*(EGSFrame **)*gCamera)->pos.vx;
            v.vy = (*(EGSFrame **)*gCamera)->pos.vy;
            v.vz = (*(EGSFrame **)*gCamera)->pos.vz;
            v.vy = -v.vy;
            EGSFrameSetPosition(&D_80113B2C.frame34, 0, &v);
        }
        D_80113B2C.fixedFrame = &D_80113B2C.frame34;
        if (func_8001C2F4(mode, on, &flag, &data) == 1) {
            func_8001B000(data, on, flag);
        }
        func_8001C3AC(mode, on);
    }
    return 1;
}

s32 GameCameraSetModeLook(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 14;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeAim(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 15;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeSniper(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 16;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

s32 GameCameraSetModeGrenade(u8 on, Gobj *primary, u8 arg2) {
    u8 flag = arg2;
    s32 data;
    s32 mode = 17;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
    return 1;
}

void GameCameraSetModeInteraction(u8 on, Gobj *primary, VECTOR32 *pos, GameCamSlot *slot, u8 arg4) {
    u8 flag;
    s32 data;
    s32 mode = 18;

    flag = arg4;
    if (func_80022318(&on, mode, pos, slot)) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        if (pos != NULL) {
            EGSFrameSetPosition(&D_80113B2C.frame34, 0, pos);
        } else {
            VECTOR32 v;

            v.vx = (*(EGSFrame **)*gCamera)->pos.vx;
            v.vy = (*(EGSFrame **)*gCamera)->pos.vy;
            v.vz = (*(EGSFrame **)*gCamera)->pos.vz;
            v.vy = -v.vy;
            EGSFrameSetPosition(&D_80113B2C.frame34, 0, &v);
        }
        D_80113B2C.fixedFrame = &D_80113B2C.frame34;
        if (func_8001C2F4(mode, on, &flag, &data) == 1) {
            func_8001B000(data, on, flag);
        }
        func_8001C3AC(mode, on);
    }
}

void GameCameraSetModeScripted(u8 on, Gobj *primary, Gobj *target, u8 arg3) {
    u8 flag = arg3;
    s32 data;
    s32 mode = 19;

    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        if (primary != NULL) {
            D_80113B2C.primary = primary;
        }
        if (target != NULL) {
            D_80113B2C.target = target;
        }
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
}

void func_8001D26C(s32 idx, s32 arg1, s32 arg2, s32 arg3, VECTOR32 *lead, VECTOR32 *watch, u8 arg6) {
    D_80113B2C.modes[idx].unk0C = arg1;
    if (arg2) {
        D_80113B2C.modes[idx].unk10 = arg2;
    }
    if (arg3) {
        D_80113B2C.modes[idx].unk14 = arg3;
    }
    if (lead != NULL) {
        D_80113B2C.modes[idx].lead = *lead;
    }
    if (watch != NULL) {
        D_80113B2C.modes[idx].watch = *watch;
    }
    D_80113B2C.modes[idx].unk38 = arg6;
}

void GameCameraSetMode(s32 mode, s32 arg1, s32 arg2, s32 arg3, VECTOR32 *lead, VECTOR32 *watch, GameCamSlot *slot, u8 on, u8 flag) {
    s32 data;

    if (arg1 == 2 && !func_80022318(&on, mode, lead, slot)) {
        return;
    }
    func_8001D26C(D_80113B2C.modeData[mode], arg1, arg2, arg3, lead, watch, flag);
    if (func_8001C2F4(mode, on, &flag, &data) == 1) {
        func_8001B000(data, on, flag);
    }
    func_8001C3AC(mode, on);
}

void GameCameraSetModeDeath(u8 on, s32 arg1, s32 arg2, s32 arg3, VECTOR32 *lead, VECTOR32 *watch, GameCamSlot *slot, u8 flag) {
    GameCameraSetMode(20, arg1, arg2, arg3, lead, watch, slot, on, flag);
}

void GameCameraResetModeToDefault(u8 flag) {
    s32 i;

    for (i = 1; i < 21; i++) {
        GameCameraSetMode(i, 0, 0, 0, NULL, NULL, NULL, 0, flag);
    }
    EGSListFlush(&D_80113B2C.slotList);
    GameCameraSetMode(0, 0, 0, 0, NULL, NULL, NULL, 1, flag);
    D_80113B2C.action = 0;
}

s32 func_8001D568(void) {
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCameraUpdateLeadAndWatchOffsets);

/* Leftover of a removed computation; the frame keeps an outgoing-argument area. */
void func_8001D89C(VECTOR32 *pos) {
    u8 args[0x10];
    VECTOR32 in;
    VECTOR32 out;
    u8 unused[0x48];

    in = *pos;
    if (((GameCamCamera *)gCamera)->unkA90.mode == 0) {
        out.vx = ((GameCamCamera *)gCamera)->unkA90.value.vx;
    } else {
        out = ((GameCamCamera *)gCamera)->unkA90.value;
    }
    pos->vx = out.vx;
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001D91C);

/* As func_8001D89C: only the copy of a zero vector is left. */
void func_8001E1DC(void) {
    u8 args[0x10];
    VECTOR32 v;
    u8 unused[0x50];

    v = D_800101F4;
}

#ifdef NON_MATCHING
/* stack: the original reuses the rotation-matrix slots (sp+0x70) for the mode-5 vectors; here they get fresh slots */
/* As in func_8001F864, the distance fetch writes a whole vector over a single word. */
s32 func_8001E218(s32 arg0) {
    VECTOR32 cur;
    VECTOR32 angle;
    VECTOR32 dir;
    VECTOR32 off;
    VECTOR32 eye;
    VECTOR32 scale;

    scale = D_800101F4;
    if (CAM->angle.mode == 0) {
        angle.vx = CAM->angle.value.vx;
    } else {
        angle = CAM->angle.value;
    }
    {
        GOBJ_MATRIX m;
        GameCamSVec rot;

        rot.vx = -angle.vx;
        rot.vy = angle.vy;
        rot.vz = -angle.vz;
        RotMatrixYXZ(&rot, &m);
        m.m[0][1] = -m.m[0][1];
        m.m[1][2] = -m.m[1][2];
        m.m[1][0] = -m.m[1][0];
        m.m[2][1] = -m.m[2][1];
        dir.vx = m.m[0][2];
        dir.vy = m.m[1][2];
        dir.vz = m.m[2][2];
    }
    {
    s32 dist;

    if (CAM->dist.mode == 0) {
        dist = CAM->dist.value.vx;
    } else {
        *(VECTOR32 *)&dist = CAM->dist.value;
    }
    off.vx = EGSMathFIXMul(dir.vx, dist);
    off.vy = EGSMathFIXMul(dir.vy, dist);
    off.vz = EGSMathFIXMul(dir.vz, dist);
    }
    CameraAccessFrameInfo(gCamera, 0, 1, 1);
    cur = CAM->unkD1C;
    cur.vx += D_80113B2C.unk088;
    cur.vy += D_80113B2C.unk08C;
    cur.vz += D_80113B2C.unk090;
    eye.vx = cur.vx - off.vx;
    eye.vy = cur.vy - off.vy;
    eye.vz = cur.vz - off.vz;
    if (D_80113B2C.mode == 5) {
        VECTOR32 p;
        VECTOR32 delta;
        s32 len;

        if (CAM->pos.mode == 0) {
            p.vx = CAM->pos.target.vx;
        } else {
            p = CAM->pos.target;
        }
        delta.vx = p.vx - eye.vx;
        delta.vy = p.vy - eye.vy;
        delta.vz = p.vz - eye.vz;
        EGSMath_VectorModulus(&delta, &len);
        if (len > 16) {
            GTEMath_VectorNormal(&delta, &delta);
            delta.vx = EGSMathFIXMul(delta.vx, 16);
            delta.vy = EGSMathFIXMul(delta.vy, 16);
            delta.vz = EGSMathFIXMul(delta.vz, 16);
            eye.vx += delta.vx;
            eye.vy += delta.vy;
            eye.vz += delta.vz;
        } else {
            eye = p;
        }
    }
    scale.vx = 1;
    scale.vy = 1;
    scale.vz = 1;
    CAM->unkD1C = eye;
    CAM->unkD30 = scale;
    CAM->unkD40 = 0;
    CameraAccessMoveInfo(gCamera, 1, 0, 1);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001E218);
#endif

s32 func_8001E5D8(VECTOR32 *out) {
    VECTOR32 pos;
    VECTOR32 angle;
    VECTOR32 off;
    VECTOR32 scale;

    scale = D_800101E4;
    if (CAM->angle.mode == 0) {
        angle.vx = CAM->angle.value.vx;
    } else {
        angle = CAM->angle.value;
    }
    off.vx = 0;
    off.vy = 0;
    off.vz = 0x2C0;
    EGSGeometry_VectorRotateYXZ(&off, &angle, &off);
    if (CAM->pos.mode == 0) {
        pos.vx = CAM->pos.target.vx;
    } else {
        pos = CAM->pos.target;
    }
    out->vx = pos.vx + off.vx;
    out->vy = pos.vy + off.vy;
    out->vz = pos.vz + off.vz;
    CAM->unkD1C = *out;
    CAM->unkD30 = scale;
    CAM->unkD40 = 0;
    CameraAccessMoveInfo(gCamera, 1, 2, 1);
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001E76C);

s32 func_8001ECC8(void) {
    VECTOR32 pos;

    if (D_80113B2C.cut == 1) {
        func_8001E76C();
    } else {
        func_8001D91C();
        func_8001E1DC();
        func_8001AF98();
        func_8001E218(0);
        func_8001E5D8(&pos);
        SCRATCH_CALL0(func_80021BA4);
    }
    if (D_80113B2C.action == 7 ||
        (D_80113B2C.unk085 && !GameCameraCheckMode(14) && !GameCameraCheckMode(15) &&
         !GameCameraCheckMode(16) && !GameCameraCheckMode(10))) {
        VECTOR32 lo = { -1, 0, -1, 0 };
        VECTOR32 hi = { 1, 0, 1, 0 };

        CAM->unkD1C = lo;
        CAM->unkD30 = hi;
        CAM->unkD40 = 0;
        CameraAccessMoveInfo(gCamera, 1, 0, 5);
        CAM->unkD1C = lo;
        CAM->unkD30 = hi;
        CAM->unkD40 = 0;
        CameraAccessMoveInfo(gCamera, 1, 3, 5);
    }
    if (D_80113B2C.transitionTime != 0 && D_80113B2C.transitionTime > 0) {
        D_80113B2C.transitionTime--;
    }
    D_80113B2C.cut = 0;
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001EF10);

/* The fov fetch copies a whole vector into a single word (as the original does,
 * spilling over the saved registers above it); only the first word is used. */
s32 func_8001F864(GameCamCamera *cam) {
    VECTOR32 pos;
    VECTOR32 eye;
    VECTOR32 at;
    VECTOR32 watch;
    VECTOR32 dir;
    s32 fov;
    EGSFrame *frame;

    if (cam->fov.mode == 0) {
        fov = cam->fov.value.vx;
    } else {
        *(VECTOR32 *)&fov = cam->fov.value;
    }
    EGSCameraSetProjectionAngle(cam->display, fov);
    frame = *(EGSFrame **)cam->display;
    if (cam->pos.mode == 0) {
        pos.vx = cam->pos.value.vx;
    } else {
        pos = cam->pos.value;
    }
    if (cam->eye.mode == 0) {
        eye.vx = cam->eye.value.vx;
    } else {
        eye = cam->eye.value;
    }
    at = eye;
    EGSFrameSetPosition(frame, 0, &at);
    if (cam->watch.mode == 0) {
        watch.vx = cam->watch.value.vx;
    } else {
        watch = cam->watch.value;
    }
    dir.vx = watch.vx - pos.vx;
    dir.vy = watch.vy - pos.vy;
    dir.vz = watch.vz - pos.vz;
    EGSFrameSetOrientation(frame, 0, &dir, 0);
    func_8001EF10();
    func_80023D70();
    return 1;
}

void GameCameraInit(void) {
    VECTOR32 pos = { 0, 0x180, 0, 0 };
    VECTOR32 angle = { 0, 0x800, 0, 0 };
    s32 i;
    s32 j;
    s32 k;
    s32 none;
    GameCamCamera *cam;

    D_8010FF78 = 0;
    CameraInit(gCamera, 0, &pos, &angle, rcos(0x19D) * 192 / rsin(0x19D), &D_8010F714, 0, 2400, &D_800FE238,
               &D_800FE340, 0, 0);
    memset(&D_80113B2C, 0, sizeof(GameCamState));
    D_80113B2C.mode = -1;
    D_80113B2C.joystickMode = 5;
    D_80113B2C.views = D_800FCF84;
    D_80113B2C.modes = D_800FDC50;
    D_80113B2C.modeData = D_800FE13C;
    D_80113B2C.modeJoystick = D_800FE190;
    D_80113B2C.joystickModes = D_800FE1E4;
    D_80113B2C.unk05C = 0;
    D_80113B2C.action = 0;
    D_80113B2C.modeMask = 0;
    D_80113B2C.joystickMask = 0;
    D_80113B2C.collision = 1;
    D_80113B2C.unk085 = 0;
    D_80113B2C.cut = 1;
    for (i = 0; i < 21; i++) {
        D_80113B2C.modes[i].unk04 = 0;
        D_80113B2C.modes[i].unk08 = 0;
        D_80113B2C.modes[i].unk0C = 0;
        D_80113B2C.modes[i].unk10 = 0;
        D_80113B2C.modes[i].unk14 = 0;
        D_80113B2C.modes[i].lead.vx = 0;
        D_80113B2C.modes[i].lead.vy = 0;
        D_80113B2C.modes[i].lead.vz = 0;
        D_80113B2C.modes[i].watch.vx = 0;
        D_80113B2C.modes[i].watch.vy = 0;
        D_80113B2C.modes[i].watch.vz = 0;
        D_80113B2C.modes[i].unk38 = 0;
    }
    D_80113B2C.scrimAnim = NULL;
    for (j = 4 * sizeof(GameCamShake); j >= 0; j -= sizeof(GameCamShake)) {
        ((GameCamShake *)((u8 *)D_80113B2C.shakes + j))->active = 0;
    }
    D_80113B2C.shakeList = NULL;
    D_80113B2C.unk1FC = 1;
    D_80113B2C.unk1FD = 1;
    D_80113B2C.unk1FE = 1;
    D_80113B2C.transitionTime = 0;
    none = -1;
    for (k = 3 * sizeof(GameCamSlot); k >= 0; k -= sizeof(GameCamSlot)) {
        ((GameCamSlot *)((u8 *)D_80113B2C.slots + k))->mode = none;
    }
    D_80113B2C.slotList = NULL;
    cam = CAM;
    cam->unkD40 = 1;
    cam->unkD1C.vx = 181;
    CameraAccessMoveInfo(gCamera, 1, 8, 2);
    cam = CAM;
    cam->unkD40 = 1;
    cam->unkD1C.vx = 827;
    CameraAccessMoveInfo(gCamera, 1, 8, 3);
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCameraActivate);

void GameCameraDisplay(u8 on) {
    CameraDisplay(gCamera, on);
}

void GameCameraActivateAndDisplay(u8 on) {
    GameCameraActivate(on);
    GameCameraDisplay(on);
}

void GameCameraRequestCut(void) {
    D_80113B2C.cut = 1;
}

void GameCameraRequestCutToBehind(void) {
    D_80113B2C.cut = 1;
    D_80113B2C.action = 5;
}

void GameCameraRequestGetBehind(void) {
    D_80113B2C.action = 5;
}

s32 GameCameraSetCollision(u8 on) {
    D_80113B2C.collision = on;
    return 1;
}

s32 GameCameraToggleCollision(void) {
    D_80113B2C.collision = D_80113B2C.collision != 1;
    return 1;
}

#ifdef NON_MATCHING
/* scheduling: the original loads track->unk10 before the second dot product finishes, and stores the first FIXDiv product in a call delay slot */
void func_8001FF08(GameCamTrack *track) {
    VECTOR32 n;
    VECTOR32 v;
    VECTOR32 d;
    VECTOR32 m;
    VECTOR32 q;
    s32 a;
    s32 b;
    s32 c;

    track->moving = 0;
    n.vx = track->plane->normal->vx;
    n.vy = track->plane->normal->vy;
    n.vz = track->plane->normal->vz;
    v.vx = FIX12(track->vel.vx);
    v.vy = FIX12(track->vel.vy);
    v.vz = FIX12(track->vel.vz);
    iDot3(&v, &n, &a);
    a = -a;
    if (a < -0x54C0) {
        return;
    }
    d.vx = track->pos.vx - track->plane->verts[0]->vx;
    d.vy = track->pos.vy - track->plane->verts[0]->vy;
    d.vz = track->pos.vz - track->plane->verts[0]->vz;
    iDot3(&d, &n, &b);
    d.vx <<= 12;
    d.vy <<= 12;
    d.vz <<= 12;
    if (b - a - (track->unk10 << 12) > track->unk28 || b < -track->unk2C) {
        return;
    }
    track->moving = 1;
    track->unk3C = b;
    track->unk40 = a;
    if (track->unk34 >= a) {
        m.vx = EGSMathFIXMul(n.vx, a - b);
        m.vy = EGSMathFIXMul(n.vy, a - b);
        m.vz = EGSMathFIXMul(n.vz, a - b);
        q.vx = track->vel.vx + m.vx;
        q.vy = track->vel.vy + m.vy;
        q.vz = track->vel.vz + m.vz;
    } else {
        iFixDiv(b, a, &c);
        q.vx = EGSMathFIXMul(track->vel.vx, c);
        q.vy = EGSMathFIXMul(track->vel.vy, c);
        q.vz = EGSMathFIXMul(track->vel.vz, c);
    }
    track->unk44.vx = d.vx + q.vx;
    track->unk44.vy = d.vy + q.vy;
    track->unk44.vz = d.vz + q.vz;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001FF08);
#endif

u8 func_80020230(VECTOR32 *pos, GameCamOutline *outline, s32 radius) {
    VECTOR32 normal;
    VECTOR32 rel;
    s32 dot;
    s32 i;
    s32 next;
    s32 len;

    radius = FIX12(radius);
    for (i = 0; i < outline->count; i++) {
        next = i + 1;
        if (next == outline->count) {
            next = 0;
        }
        normal.vx = outline->v[i].vz - outline->v[next].vz;
        normal.vz = outline->v[next].vx - outline->v[i].vx;
        len = iApproxLength(normal.vx, normal.vz) > 1 ? iApproxLength(normal.vx, normal.vz) : 1;
        rel.vx = pos->vx - outline->v[i].vx;
        rel.vz = pos->vz - outline->v[i].vz;
        iDotXZ(&rel, &normal, &dot);
        if (radius * len < dot) {
            return 0;
        }
    }
    return 1;
}

u8 func_800203F0(VECTOR32 *pos, GameCamPlane *plane, s32 radius) {
    VECTOR32 p;
    GameCamOutline5 o;
    s32 ax;
    s32 ay;
    s32 az;
    s32 m;
    s32 i;

    ax = abs(plane->normal->vx);
    ay = abs(plane->normal->vy);
    az = abs(plane->normal->vz);
    m = MAX(ax, MAX(ay, az));
    o.count = plane->count;
    if (m == ax) {
        if (plane->normal->vx < 0) {
            p.vx = FIX12(pos->vy);
            p.vz = FIX12(pos->vz);
            o.v[0].vx = 0;
            o.v[0].vz = 0;
            for (i = 1; i < plane->count; i++) {
                o.v[i].vx = plane->verts[i]->vy - plane->verts[0]->vy;
                o.v[i].vz = plane->verts[i]->vz - plane->verts[0]->vz;
            }
        } else {
            p.vx = -FIX12(pos->vy);
            p.vz = FIX12(pos->vz);
            o.v[0].vx = 0;
            o.v[0].vz = 0;
            for (i = 1; i < plane->count; i++) {
                o.v[i].vx = plane->verts[0]->vy - plane->verts[i]->vy;
                o.v[i].vz = plane->verts[i]->vz - plane->verts[0]->vz;
            }
        }
    } else if (m == ay) {
        if (plane->normal->vy >= 0) {
            p.vx = FIX12(pos->vx);
            p.vz = FIX12(pos->vz);
            o.v[0].vx = 0;
            o.v[0].vz = 0;
            for (i = 1; i < plane->count; i++) {
                o.v[i].vx = plane->verts[i]->vx - plane->verts[0]->vx;
                o.v[i].vz = plane->verts[i]->vz - plane->verts[0]->vz;
            }
        } else {
            p.vx = -FIX12(pos->vx);
            p.vz = FIX12(pos->vz);
            o.v[0].vx = 0;
            o.v[0].vz = 0;
            for (i = 1; i < plane->count; i++) {
                o.v[i].vx = plane->verts[0]->vx - plane->verts[i]->vx;
                o.v[i].vz = plane->verts[i]->vz - plane->verts[0]->vz;
            }
        }
    } else if (plane->normal->vz < 0) {
        p.vx = FIX12(pos->vx);
        p.vz = FIX12(pos->vy);
        o.v[0].vx = 0;
        o.v[0].vz = 0;
        for (i = 1; i < plane->count; i++) {
            o.v[i].vx = plane->verts[i]->vx - plane->verts[0]->vx;
            o.v[i].vz = plane->verts[i]->vy - plane->verts[0]->vy;
        }
    } else {
        p.vx = -FIX12(pos->vx);
        p.vz = FIX12(pos->vy);
        o.v[0].vx = 0;
        o.v[0].vz = 0;
        for (i = 1; i < plane->count; i++) {
            o.v[i].vx = plane->verts[0]->vx - plane->verts[i]->vx;
            o.v[i].vz = plane->verts[i]->vy - plane->verts[0]->vy;
        }
    }
    return func_80020230(&p, (GameCamOutline *)&o, radius);
}

#ifdef NON_MATCHING
/* regalloc: track and the plane distance swap s0/s1 */
void func_80020978(GameCamTrack *track) {
    VECTOR32 n;
    VECTOR32 q;
    VECTOR32 p;
    s32 r = track->unk10 << 12;
    s32 dist = track->unk40;
    GameCamSVec *normal = track->plane->normal;
    s32 d = track->unk3C;

    if (track->unk34 >= dist) {
        n.vx = normal->vx;
        n.vy = normal->vy;
        n.vz = normal->vz;
        d = dist - d + r;
        track->unk54.vx = EGSMathFIXMul(n.vx, d);
        track->unk54.vy = EGSMathFIXMul(n.vy, d);
        track->unk54.vz = EGSMathFIXMul(n.vz, d);
        track->unk64.vx = EGSMathFIXMul(n.vx, dist);
        track->unk64.vy = EGSMathFIXMul(n.vy, dist);
        track->unk64.vz = EGSMathFIXMul(n.vz, dist);
        track->unk74.vx = track->unk54.vx - track->unk64.vx;
        track->unk74.vy = track->unk54.vy - track->unk64.vy;
        track->unk74.vz = track->unk54.vz - track->unk64.vz;
    } else {
        p.vx = EGSMathFIXDiv(track->vel.vx, dist);
        p.vy = EGSMathFIXDiv(track->vel.vy, dist);
        p.vz = EGSMathFIXDiv(track->vel.vz, dist);
        d = d - r;
        q.vx = EGSMathFIXMul(p.vx, d);
        q.vy = EGSMathFIXMul(p.vy, d);
        q.vz = EGSMathFIXMul(p.vz, d);
        track->unk54.vx = q.vx - track->vel.vx;
        track->unk54.vy = q.vy - track->vel.vy;
        track->unk54.vz = q.vz - track->vel.vz;
        track->unk64 = track->unk54;
        track->unk74.vx = 0;
        track->unk74.vy = 0;
        track->unk74.vz = 0;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020978);
#endif

void func_80020B58(GameCamTrack *track) {
    func_8001FF08(track);
    if (track->moving) {
        track->moving = func_800203F0(&track->unk44, track->plane, track->unk30);
        if (track->moving) {
            func_80020978(track);
        }
    }
}

void func_80020BB8(VECTOR32 *pos, s32 radius, s32 yLow, s32 yHigh, VECTOR32 *delta, s32 rxz, s32 ry, GameCamBox *box) {
    box->min.vx = pos->vx + FIX12(delta->vx <= 0 ? delta->vx - rxz : -rxz) - radius;
    box->min.vy = yLow + FIX12(delta->vy <= 0 ? delta->vy - ry : -ry);
    box->min.vz = pos->vz + FIX12(delta->vz <= 0 ? delta->vz - rxz : -rxz) - radius;
    box->max.vx = pos->vx + FIX12(MAX(delta->vx, 0) + rxz) + radius;
    box->max.vy = yHigh + FIX12(MAX(delta->vy, 0) + ry);
    box->max.vz = pos->vz + FIX12(MAX(delta->vz, 0) + rxz) + radius;
}

#ifdef NON_MATCHING
/* the final 'not all beyond max.vz' test: the original fills the first branch's delay slot with the result 1 */
u8 func_80020E90(GameCamBox *box, GameCamPlane *plane) {
    GameCamSVec *a = plane->verts[0];
    GameCamSVec *b = plane->verts[1];
    GameCamSVec *c = plane->verts[2];
    GameCamSVec *d;

    if (plane->count == 3) {
        if (a->vy < box->min.vy && b->vy < box->min.vy && c->vy < box->min.vy) {
            return 0;
        }
        if (box->max.vy < a->vy && box->max.vy < b->vy && box->max.vy < c->vy) {
            return 0;
        }
        if (a->vx < box->min.vx && b->vx < box->min.vx && c->vx < box->min.vx) {
            return 0;
        }
        if (box->max.vx < a->vx && box->max.vx < b->vx && box->max.vx < c->vx) {
            return 0;
        }
        if (a->vz < box->min.vz && b->vz < box->min.vz && c->vz < box->min.vz) {
            return 0;
        }
        return box->max.vz >= a->vz || box->max.vz >= b->vz || box->max.vz >= c->vz;
    }
    if (plane->count == 4) {
        d = plane->verts[3];
        if (a->vy < box->min.vy && b->vy < box->min.vy && c->vy < box->min.vy && d->vy < box->min.vy) {
            return 0;
        }
        if (box->max.vy < a->vy && box->max.vy < b->vy && box->max.vy < c->vy && box->max.vy < d->vy) {
            return 0;
        }
        if (a->vx < box->min.vx && b->vx < box->min.vx && c->vx < box->min.vx && d->vx < box->min.vx) {
            return 0;
        }
        if (box->max.vx < a->vx && box->max.vx < b->vx && box->max.vx < c->vx && box->max.vx < d->vx) {
            return 0;
        }
        if (a->vz < box->min.vz && b->vz < box->min.vz && c->vz < box->min.vz && d->vz < box->min.vz) {
            return 0;
        }
        return box->max.vz >= a->vz || box->max.vz >= b->vz || box->max.vz >= c->vz || box->max.vz >= d->vz;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020E90);
#endif

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80021230);

#ifdef NON_MATCHING
/* the CameraAccessFrameInfo mode select: original loads gCamera after the arms (shared call tail) */
s32 func_80021BA4(void) {
    VECTOR32 cur;
    VECTOR32 pos;
    VECTOR32 d;
    VECTOR32 dir;
    VECTOR32 angle;

    if (CAM->pos.mode == 0) {
        pos.vx = CAM->pos.target.vx;
    } else {
        pos = CAM->pos.target;
    }
    D_80113B2C.unk085 = 0;
    if (D_80113B2C.collision && D_80113B2C.mode != 10 && D_80113B2C.mode != 13 && D_80113B2C.mode != 19 &&
        D_80113B2C.unk05C != 3) {
        GOBJ_MATRIX m;
        GameCamSVec rot;
        s32 which;

        if (D_80113B2C.mode == 13 || D_80113B2C.mode == 18 || D_80113B2C.unk05C == 2) {
            which = 2;
        } else {
            which = 1;
        }
        CameraAccessFrameInfo(gCamera, 0, which, 1);
        cur = CAM->unkD1C;
        d.vx = pos.vx - cur.vx;
        d.vy = pos.vy - cur.vy;
        d.vz = pos.vz - cur.vz;
        if (CAM->angle.mode == 0) {
            angle.vx = CAM->angle.target.vx;
        } else {
            angle = CAM->angle.target;
        }
        rot.vx = -angle.vx;
        rot.vy = angle.vy;
        rot.vz = -angle.vz;
        RotMatrixYXZ(&rot, &m);
        m.m[0][1] = -m.m[0][1];
        m.m[1][2] = -m.m[1][2];
        m.m[1][0] = -m.m[1][0];
        m.m[2][1] = -m.m[2][1];
        dir.vx = m.m[0][2];
        dir.vy = m.m[1][2];
        dir.vz = m.m[2][2];
        if (func_80021230(&cur, &d, &dir, &pos)) {
            D_80113B2C.unk085 = 1;
        }
    }
    {
        VECTOR32 scale;

        scale.vx = 1;
        scale.vy = 1;
        scale.vz = 1;
        CAM->unkD1C = pos;
        CAM->unkD30 = scale;
    }
    CAM->unkD40 = 0;
    CameraAccessMoveInfo(gCamera, 1, 3, 1);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80021BA4);
#endif

void GameCameraModeGetDefaultLeadOffset(s32 mode, VECTOR32 *out) {
    *out = D_80113B2C.views[D_80113B2C.modes[mode].view].lead;
}

void GameCameraModeGetDefaultWatchOffset(s32 mode, VECTOR32 *out) {
    *out = D_80113B2C.views[D_80113B2C.modes[mode].view].watch;
}

void GameCameraModeGetDefaultDistance(s32 mode, s32 *out) {
    *out = D_80113B2C.views[D_80113B2C.modes[mode].view].distance;
}

void GameCameraCurrentModeGetDefaultDistance(s32 *out) {
    *out = D_80113B2C.views[D_80113B2C.modes[D_80113B2C.mode].view].distance;
}

void GameCameraModeSetDefaultDistance(s32 mode, s32 distance) {
}

void GameCameraModeSetDefaultPitchAngle(s32 mode, s32 pitch) {
}

void GameCameraSetDefaultDistance(s32 distance) {
    s32 i;

    for (i = 0; i < 21; i++) {
        if (D_800FE4F8[i] == -1) {
            break;
        }
        GameCameraModeSetDefaultDistance(D_800FE4F8[i], distance);
    }
}

void GameCameraSetDefaultPitchAngle(s32 pitch) {
    s32 i;

    for (i = 0; i < 21; i++) {
        if (D_800FE54C[i] == -1) {
            break;
        }
        GameCameraModeSetDefaultPitchAngle(D_800FE54C[i], pitch);
    }
}

Gobj *GameCameraGetPrimaryGobj(void) {
    return D_80113B2C.primary;
}

void GameCameraShakeStart(GameCamShake *shake, s32 magnitude, s32 rate, VECTOR32 *axis, EGSFrame *frame, VECTOR32 *pos) {
    s32 i;
    GameCamShake *s;
    GameCamShake *use;

    if (magnitude == 0 || rate == 0 || axis == NULL || (magnitude < 0 && shake == NULL)) {
        return;
    }
    if (shake != NULL) {
        use = shake;
    } else {
        use = NULL;
        for (i = 0, s = D_80113B2C.shakes; i < 5; i++, s++) {
            if (!s->active) {
                use = s;
                break;
            }
            if (use == NULL || s->magnitude < use->magnitude) {
                use = s;
            }
        }
    }
    use->active = 1;
    use->magnitude = magnitude;
    use->rate = rate;
    use->axis = *axis;
    if (frame != NULL) {
        use->frame = frame;
    } else if (pos != NULL) {
        use->frame = &use->ownFrame;
        if (use->ownFrame.pos.pad == 0) {
            use->ownFrame.pos.vx = pos->vx;
            use->frame->pos.vy = -pos->vy;
            use->frame->pos.vz = pos->vz;
        } else {
            EGSFrameSetPosition(&use->ownFrame, 0, pos);
        }
    } else {
        use->frame = NULL;
    }
    if (!EGSListFind(&D_80113B2C.shakeList, 0, use)) {
        D_80113B2C.shakeList = EGSListAdd(&D_80113B2C.shakeList, use);
    }
}

void GameCameraShakeModifyMagnitude(GameCamShake *shake, VECTOR32 *axis) {
    shake->axis = *axis;
}

void GameCameraShakeStop(GameCamShake *shake) {
    shake->magnitude = 0;
}

s32 GameCameraGetTransitionTime(void) {
    return D_80113B2C.transitionTime;
}

void GameCameraSetTransitionTime(s32 ticks) {
    D_80113B2C.transitionTime = ticks;
}

EGSListNode *func_8002227C(s32 mode, VECTOR32 *pos, GameCamSlot *slot) {
    EGSListNode *node = D_80113B2C.slotList;
    GameCamSlot *s;

    if (pos == NULL && slot == NULL) {
        return NULL;
    }
    for (; node != NULL; node = node->next) {
        s = node->data;
        if (s == slot) {
            return node;
        }
        if (slot == NULL && s->mode == mode && s->pos.vx == pos->vx && s->pos.vy == pos->vy &&
            s->pos.vz == pos->vz) {
            return node;
        }
    }
    return node;
}

u8 func_80022318(u8 *on, s32 mode, VECTOR32 *pos, GameCamSlot *slot) {
    EGSListNode *node;

    if ((*on && func_80022414(mode, pos, slot)) || (!*on && func_800224F8(mode, pos, slot))) {
        if (!*on) {
            for (node = D_80113B2C.slotList; node != NULL; node = node->next) {
                if (((GameCamSlot *)node->data)->mode == mode) {
                    *on = 1;
                    break;
                }
            }
        }
        return 1;
    }
    return 0;
}

u8 func_80022414(s32 mode, VECTOR32 *pos, GameCamSlot *slot) {
    GameCamSlot *use = NULL;
    GameCamSlot *s;
    s32 i;
    s32 free;

    if (pos != NULL) {
        if (slot != NULL) {
            use = slot;
        } else {
            i = 0;
            free = -1;
            s = D_80113B2C.slots;
            for (; i < 4; i++, s++) {
                if (s->mode == free) {
                    use = s;
                    break;
                }
            }
        }
        if (use != NULL && func_8002227C(mode, pos, slot) == NULL) {
            use->mode = mode;
            use->pos = *pos;
            D_80113B2C.slotList = EGSListAdd(&D_80113B2C.slotList, use);
            return 1;
        }
    }
    return 0;
}

u8 func_800224F8(s32 mode, VECTOR32 *pos, GameCamSlot *slot) {
    EGSListNode *node = func_8002227C(mode, pos, slot);

    if (node == NULL) {
        return 0;
    }
    EGSListRemove(&D_80113B2C.slotList, node);
    ((GameCamSlot *)node->data)->mode = -1;
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80022548);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_800228D0);

void GameCamCheckScriptCamDoneCallback(void (*done)()) {
    if (D_8010FF78 != 0 || gInterfaceCurrentMode == 0 || gInterfaceCurrentMode == 9) {
        SysExeTickQueueTimer(GameCamCheckScriptCamDoneCallback, 1, done);
        return;
    }
    if (done != NULL) {
        done();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", CAMERAObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCamDeathViewCASAnimCB);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCameraScrimInit);

void iScrimRemoveVRAMScroll(void) {
    void *display;
    s32 i;

    CameraGetDisplay(gCamera, &display);
    for (i = 0; i < 13; i++) {
        EGSDisplayDeletePoly(display, &mPMI[i]);
    }
}

void GameCameraSetScrimOnFlag(u8 on) {
    if (D_80113B2C.scrimOn != -1) {
        D_80113B2C.scrimOn = on;
        if (on) {
            GSAddAnim(D_80113B2C.scrimAnim);
            if (*(s16 *)((u8 *)gLevData + 0x54)) {
                D_80113B2C.scrimRate = -1;
            }
        } else {
            GSDeleteAnim(D_80113B2C.scrimAnim);
            if (D_80113B2C.scrimRate > 0) {
                iScrimRemoveVRAMScroll();
            }
            D_80113B2C.scrimRate = 0;
        }
    }
}

void GameCameraSetScrimScrollRate(s32 rate) {
    if (D_80113B2C.scrimRate > 0 || rate == 0) {
        iScrimRemoveVRAMScroll();
    }
    D_80113B2C.scrimRate = -rate;
}

#ifdef NON_MATCHING
/* the original does not cross-jump the two scroll-rate stores (sw stays in the abs branch's delay slot) */
void func_80023D70(void) {
    VECTOR32 pos;
    VECTOR32 dir;
    void *display;
    s32 i;
    s32 rate;

    if ((u8)(D_80113B2C.scrimOn + 1) >= 2) {
        pos.vx = (*(EGSFrame **)*gCamera)->pos.vx;
        pos.vy = (*(EGSFrame **)*gCamera)->pos.vy;
        pos.vz = (*(EGSFrame **)*gCamera)->pos.vz;
        pos.vy = -pos.vy;
        dir.vx = (*(EGSFrame **)*gCamera)->m[0][2];
        dir.vy = (*(EGSFrame **)*gCamera)->m[1][2];
        dir.vz = (*(EGSFrame **)*gCamera)->m[2][2];
        dir.vy = -dir.vy;
        dir.vx = EGSMathFIXMul(dir.vx, 32);
        dir.vy = EGSMathFIXMul(dir.vy, 32);
        dir.vz = EGSMathFIXMul(dir.vz, 32);
        pos.vx += dir.vx;
        pos.vy += dir.vy;
        pos.vz += dir.vz;
        EGSFrameSetPosition(((GameCamAnim *)D_80113B2C.scrimAnim)->frame, 0, &pos);
    }
    if (D_80113B2C.scrimRate != 0) {
        CameraGetDisplay(gCamera, &display);
        rate = D_80113B2C.scrimRate;
        if ((u32)gExeTick % (u32)(rate < 0 ? -rate : rate) == 0 && rate < 0) {
            for (i = 0; i < 13; i++) {
                EGSDisplayAddPoly(display, mPMI[i]);
            }
            D_80113B2C.scrimRate = ABS(D_80113B2C.scrimRate);
        } else if (D_80113B2C.scrimRate > 0) {
            for (i = 0; i < 13; i++) {
                EGSDisplayDeletePoly(display, mPMI[i]);
            }
            D_80113B2C.scrimRate = -D_80113B2C.scrimRate;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80023D70);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/gamecam", jtbl_80010274);
