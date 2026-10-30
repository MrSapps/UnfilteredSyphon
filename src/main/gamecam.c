#include "common.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"

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
typedef struct GameCamCamera {
    /* 0x000 */ u8 unk000[0xA90];
    /* 0xA90 */ s32 unkA90;
    /* 0xA94 */ s32 unkA94;
    /* 0xA98 */ VECTOR32 unkA98;
} GameCamCamera;

extern const VECTOR32 D_800101E4; /* {1, 1, 1} */
extern const VECTOR32 D_800101F4; /* zero */
u8 EGSListFind(EGSListNode **list, s32 arg1, void *data);

/* Interpolated camera parameter (func_8001FF08 and friends). */
typedef struct GameCamTrack {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ void *unk24;
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ u8 unk34[4];
    /* 0x38 */ u8 moving;
    /* 0x39 */ u8 pad39[3];
    /* 0x3C */ u8 unk3C[8];
    /* 0x44 */ u8 unk44[0x10];
} GameCamTrack;

void func_8001FF08(GameCamTrack *track);
u8 func_800203F0(void *arg0, void *arg1, s32 arg2);
void func_80020978(GameCamTrack *track);

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

s32 GameCameraCheckMode(s32 mode) {
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
    if (((GameCamCamera *)gCamera)->unkA90 == 0) {
        out.vx = ((GameCamCamera *)gCamera)->unkA98.vx;
    } else {
        out = ((GameCamCamera *)gCamera)->unkA98;
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

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001E218);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001E5D8);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001E76C);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001ECC8);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001EF10);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001F864);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCameraInit);

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

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_8001FF08);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020230);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_800203F0);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020978);

void func_80020B58(GameCamTrack *track) {
    func_8001FF08(track);
    if (track->moving) {
        track->moving = func_800203F0(&track->unk44, track->unk24, track->unk30);
        if (track->moving) {
            func_80020978(track);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020BB8);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80020E90);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80021230);

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80021BA4);

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

#ifdef NON_MATCHING
/* free-slot search: original cross-jumps the found-slot path into the shake!=NULL move */
void GameCameraShakeStart(GameCamShake *shake, s32 magnitude, s32 rate, VECTOR32 *axis, EGSFrame *frame, VECTOR32 *pos) {
    s32 i;
    GameCamShake *s;

    if (magnitude == 0 || rate == 0 || axis == NULL || (magnitude < 0 && shake == NULL)) {
        return;
    }
    if (shake == NULL) {
        for (i = 0, s = D_80113B2C.shakes; i < 5; i++, s++) {
            if (!s->active) {
                shake = s;
                break;
            }
            if (shake == NULL || s->magnitude < shake->magnitude) {
                shake = s;
            }
        }
    }
    shake->active = 1;
    shake->magnitude = magnitude;
    shake->rate = rate;
    shake->axis = *axis;
    if (frame != NULL) {
        shake->frame = frame;
    } else if (pos != NULL) {
        shake->frame = &shake->ownFrame;
        if (shake->ownFrame.pos.pad == 0) {
            shake->ownFrame.pos.vx = pos->vx;
            shake->frame->pos.vy = -pos->vy;
            shake->frame->pos.vz = pos->vz;
        } else {
            EGSFrameSetPosition(&shake->ownFrame, 0, pos);
        }
    } else {
        shake->frame = NULL;
    }
    if (!EGSListFind(&D_80113B2C.shakeList, 0, shake)) {
        D_80113B2C.shakeList = EGSListAdd(&D_80113B2C.shakeList, shake);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/gamecam", GameCameraShakeStart);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/gamecam", func_80023D70);

INCLUDE_RODATA("asm/main/nonmatchings/gamecam", jtbl_80010274);
