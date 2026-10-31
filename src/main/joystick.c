#include "common.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

/* joystick.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_800114B8[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */
static const VECTOR32 D_800114C8 = { 0, 0, 0, 0 };
static const s32 D_800114D8[4] = { -0xAA, 0, 0, 0 }; /* unreferenced (an asm function's?) */

#define JOY_BUTTONS 16
#define JOY_MAPPED_BUTTONS 9 /* buttons remapped through gJoyMap, the rest are cleared */

#define JOY_ABS(x) ((x) >= 0 ? (x) : -(x))

/* Per mode, per button action (mJoyBtnInfo[mode][button], 0x1C bytes). */
typedef struct JoyBtnInfo {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 action; /* CAS action requested on release */
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 pad09[3];
    /* 0x0C */ s32 unk0C;     /* reaction table (ReactionSet.entries), 0: none */
    /* 0x10 */ u32 holdDelay; /* frames to hold before the press action fires, 0: at once */
    /* 0x14 */ u32 holdCount; /* JOY_HOLD_FIRED once fired */
    /* 0x18 */ s16 relTicks;
    /* 0x1A */ u8 pad1A[2];
} JoyBtnInfo;

/* Directional inputs (D_80105700, 0x1C bytes each). */
typedef struct JoyDirInfo {
    /* 0x00 */ s32 unk00; /* bit 5 set: keep, else reloaded from the defaults */
    /* 0x04 */ s32 unk04; /* likewise */
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 type;
} JoyDirInfo;

/* Joystick-side view of the movement data hanging off GameObjectRuntime.move (see movement.c). */
typedef struct JoyMoveView {
    /* 0x000 */ u8 unk000[0x24];
    /* 0x024 */ s32 velY;
    /* 0x028 */ u8 unk028[0x98];
    /* 0x0C0 */ VECTOR32 nextRot;      /* rotation.next.pos */
    /* 0x0D0 */ VECTOR32 nextRotDelta; /* rotation.next.delta, << 12 */
    /* 0x0E0 */ u8 unk0E0[0xC4];
    /* 0x1A4 */ u32 edgeFlags;
} JoyMoveView;

/* A joystick input being tracked (two bits of the pad state, current and previous). */
typedef struct JoyInput {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s32 kind; /* 6: not tracked */
    /* 0x08 */ u8 unk08[0x1C];
    /* 0x24 */ s32 bit0;
    /* 0x28 */ s32 bit1;
    /* 0x2C */ u8 down0;
    /* 0x2D */ u8 down1;
    /* 0x2E */ u8 unk2E[2];
    /* 0x30 */ s32 state; /* 0: up, 1: first pressed, 2: second pressed */
} JoyInput;


/* Combined directional input per stick (JoyController.sticks, 0x3C bytes). */
typedef struct JoyStickState {
    /* 0x00 */ VECTOR32 dir;
    /* 0x10 */ VECTOR32 unit;
    /* 0x20 */ s32 len;
    /* 0x24 */ s32 srcX; /* JoyDirInfo index the x part came from, -1: none */
    /* 0x28 */ s32 srcZ;
    /* 0x2C */ VECTOR32 turn; /* camera stick (sticks[1]): camera turn; aim stick (sticks[3]): turn.vz counts the hold */
} JoyStickState;

/* Joystick controller state (the one instance is D_801056C8). */
typedef struct JoyController {
    /* 0x000 */ u8 initialized;
    /* 0x001 */ u8 pad001[3];
    /* 0x004 */ s32 mode;     /* D_801056CC */
    /* 0x008 */ s32 prevMode; /* D_801056D0 */
    /* 0x00C */ struct JoyBtnInfo *btns; /* D_801056D4: mJoyBtnInfo[mode] */
    /* 0x010 */ u8 deactivated;     /* D_801056D8 */
    /* 0x011 */ u8 menuDeactivated; /* D_801056D9 */
    /* 0x012 */ u8 pad012[2];
    /* 0x014 */ s32 bitA; /* mapped index of the button with gJoyMap value 2 */
    /* 0x018 */ u8 downA;
    /* 0x019 */ u8 pad019[3];
    /* 0x01C */ s32 bitB; /* mapped index of the button with gJoyMap value 0 */
    /* 0x020 */ u8 downB;
    /* 0x021 */ u8 pad021[3];
    /* 0x024 */ s32 btnIndex4; /* mapped index of the button with gJoyMap value 4 */
    /* 0x028 */ s32 btnIndex3; /* mapped index of the button with gJoyMap value 3 */
    /* 0x02C */ u8 unk02C;
    /* 0x02D */ u8 unk02D;
    /* 0x02E */ u8 pad02E[2];
    /* 0x030 */ s32 unk030;
    /* 0x034 */ s32 unk034;
    /* 0x038 */ JoyDirInfo *dirs;
    /* 0x03C */ JoyStickState sticks[4];
    /* 0x12C */ struct JoyDirDefault *dirDefaults; /* 4 per set, indexed by JoyDirInfo.type */
    /* 0x130 */ u32 prevAimState;
    /* 0x134 */ u32 aimState; /* 0..5 */
    /* 0x138 */ s32 unk138;
    /* 0x13C */ s32 unk13C;
    /* 0x140 */ u8 dirActive;
    /* 0x141 */ u8 pad141[3];
    /* 0x144 */ s32 sniperZoom; /* sniper zoom step, -0x2D..0x2D */
    /* 0x148 */ s32 panTicks;   /* first-person side pan, signed by direction, up to 20 */
    /* 0x14C */ s32 runTicks; /* 10 after a type 7 move, then counts down */
    /* 0x150 */ u8 invertY;
} JoyController;

typedef struct JoyDirDefault {
    /* 0x0 */ s32 unk00;
    /* 0x4 */ s32 unk04;
} JoyDirDefault;

s32 GameCameraGetMode(void);
void EGSDeviceControllerRead(u8 **pad, s32 port);
void InterfaceWeaponSelect(s32 select, s32 dir);

/* Bit n of a bit array that may start at an unaligned address. */
static inline u8 JoyTestBit(u8 *bits, s32 n) {
    u32 misalign = (u32)bits & 3;
    s32 word;
    s32 bit;

    if (misalign != 0) {
        n += misalign * 8;
        bits = (u8 *)((u32)bits & ~3);
    }
    word = n >> 5;
    bit = 1 << (n & 0x1F);
    return (((u32 *)bits)[word] & bit) != 0;
}

/* As JoyTestBit, but the raw masked word. */
static inline u32 JoyBitMask(u8 *bits, s32 n) {
    u32 misalign = (u32)bits & 3;
    s32 word;
    s32 bit;

    if (misalign != 0) {
        n += misalign * 8;
        bits = (u8 *)((u32)bits & ~3);
    }
    word = n >> 5;
    bit = 1 << (n & 0x1F);
    return ((u32 *)bits)[word] & bit;
}

extern JoyBtnInfo mJoyBtnInfo[][JOY_BUTTONS];

/* .data (the gButtonStrings literals go to .sdata in reverse) */
s32 gJoyMap[JOY_MAPPED_BUTTONS] = { 3, 4, 0, 2, 1, 5, 6, 7, 8 };
char *gButtonStrings[JOY_MAPPED_BUTTONS] = { "%l2", "%r2", "%l1", "%r1", "%t", "%c", "%x", "%sq", "%se" };
/* .sdata */
static u16 D_8010F84C[2] = { 0, 0 }; /* [1]: pad buttons last seen by func_80039B24 */
/* .data: the controller state */
static JoyController D_801056C8 = { 0 };
/* default layouts, indexed by gJoyMap */
static JoyBtnInfo D_8010581C[JOY_MAPPED_BUTTONS] = {
    { 0x3C, 0xD, 1, { 0 }, 0, 0, 0, 0 },
    { 0x22, 0, 1, { 0 }, 0, 0, 0, 0 },
    { 3, 0x41, 1, { 0 }, 0, 0, 0, 0 },
    { 0x42, 0x43, 0, { 0 }, 0, 0, 0, 0 },
    { 0x42, 0x43, 0, { 0 }, 0, 0, 0, 0 },
    { 0x5E, 0, 1, { 0 }, 0, 0, 0, 0 },
    { 0xA, 0xB, 1, { 0 }, 0, 0, 0, 0 },
    { 0x15, 0x16, 0, { 0 }, 0, 0, 0, 0 },
    { 0x34, 0, 0, { 0 }, 0, 0, 0, 0 },
};
static JoyBtnInfo D_80105918[JOY_MAPPED_BUTTONS] = {
    { 0, 0xD, 1, { 0 }, 0, 0, 0, 0 },
    { 0x22, 0, 1, { 0 }, 0, 0, 0, 0 },
    { 3, 0x41, 1, { 0 }, 0, 0, 0, 0 },
    { 0x42, 0x43, 0, { 0 }, 0, 0, 0, 0 },
    { 0x42, 0x43, 0, { 0 }, 0, 0, 0, 0 },
    { 0x5E, 0, 1, { 0 }, 0, 0, 0, 0 },
    { 0xA, 0xB, 1, { 0 }, 0, 0, 0, 0 },
    { 0x15, 0x16, 0, { 0 }, 0, 0, 0, 0 },
    { 0x34, 0, 1, { 0 }, 0, 0, 7, 0 },
};
static JoyBtnInfo D_80105A14[JOY_MAPPED_BUTTONS] = {
    { 0, 0, 1, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 3, 0x41, 1, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 0, { 0 }, 0, 0, 0, 0 },
    { 0, 0, 1, { 0 }, 0, 0, 0, 0 },
};
static JoyDirInfo D_80105B10[4] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0x20, 0, 0, 0, 0, 2 },
};
static JoyDirDefault D_80105B80[20] = { /* 4 per set */
    { 0, 0 }, { 0xF, 0xD }, { 1, 1 }, { 0xF, 0xD },
    { 0, 0 }, { 6, 5 }, { 1, 1 }, { 6, 5 },
    { 0, 0 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
    { 0, 0 }, { 9, 9 }, { 1, 1 }, { 9, 9 },
    { 2, 2 }, { 2, 2 }, { 0, 0 }, { 0, 0 },
};
/* the code still names many fields by their splat symbols */
#define D_801056CC (D_801056C8.mode)
#define D_801056D0 (D_801056C8.prevMode)
#define D_801056D4 (D_801056C8.btns)
#define D_801056D8 (D_801056C8.deactivated)
#define D_801056D9 (D_801056C8.menuDeactivated)
#define D_801056E0 (D_801056C8.downA)
#define D_801056E8 (D_801056C8.downB)
#define D_801056F4 (D_801056C8.unk02C)
#define D_801056F5 (D_801056C8.unk02D)
#define D_801056F8 (D_801056C8.unk030)
#define D_801056FC (D_801056C8.unk034)
#define D_80105700 (D_801056C8.dirs)
#define D_801057F4 (D_801056C8.dirDefaults)
#define D_801057F8 (D_801056C8.prevAimState)
#define D_801057FC (D_801056C8.aimState)
#define D_80105800 (D_801056C8.unk138)
#define D_80105804 (D_801056C8.unk13C)
#define D_80105808 (D_801056C8.dirActive)
#define D_8010580C (D_801056C8.sniperZoom)
#define D_80105810 (D_801056C8.panTicks)
#define D_80105814 (D_801056C8.runTicks)
#define D_80105818 (D_801056C8.invertY)
#ifndef _PSYQ
/* INCLUDE_ASM functions address the fields through their splat labels */
__asm__(
        ".set D_801056CC, D_801056C8+0x4\n"
        ".set D_801056D4, D_801056C8+0xC\n"
        ".set D_801056D8, D_801056C8+0x10\n"
        ".set D_801056D9, D_801056C8+0x11\n"
        ".set D_801056E0, D_801056C8+0x18\n"
        ".set D_801056E8, D_801056C8+0x20\n"
        ".set D_801056F4, D_801056C8+0x2C\n"
        ".set D_801056F5, D_801056C8+0x2D\n"
        ".set D_801056F8, D_801056C8+0x30\n"
        ".set D_801056FC, D_801056C8+0x34\n"
        ".set D_80105700, D_801056C8+0x38\n"
        ".set D_801057F4, D_801056C8+0x12C\n"
        ".set D_801057F8, D_801056C8+0x130\n"
        ".set D_801057FC, D_801056C8+0x134\n"
        ".set D_80105800, D_801056C8+0x138\n"
        ".set D_80105804, D_801056C8+0x13C\n"
        ".set D_80105808, D_801056C8+0x140\n"
        ".set D_8010580C, D_801056C8+0x144\n"
        ".set D_80105810, D_801056C8+0x148\n"
        ".set D_80105814, D_801056C8+0x14C\n"
        ".set D_80105818, D_801056C8+0x150\n"
        );
#endif
extern s32 gCASJoystickGobjID;

void MovementGobjTeleport();
void MovementGobjTeleport_ByXZLookAtPoint();
void GameCameraRequestGetBehind(void);
void JoystickDirInputDeactivate(s32 deactivate, u8 arg1);
u8 JoystickGobjIsInPureStrafe(Gobj *gobj);
u8 JoystickGobjIsInFullRetreat();
u8 JoystickGobjIsWalkKeyPressed(Gobj *gobj);
u8 JoystickIsDirInputDeactivated(void);
u8 JoystickGobjIsInFastWalkBack(Gobj *gobj);

typedef struct ReactionEntry {
    /* 0x0 */ u8 key;
    /* 0x1 */ u8 pad[3];
    /* 0x4 */ u32 mask;          /* 0: any */
    /* 0x8 */ u32 requiredFlags; /* 0: any, else tested against rt->cas->flags */
    /* 0xC */ s32 result;        /* 0 terminates the table */
} ReactionEntry;

typedef struct ReactionSet {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ ReactionEntry *entries;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} ReactionSet;

/* First entry matching key/mask/flags; returns its result (0 if none). */
#ifdef NON_MATCHING
/* regalloc: the original never forms an e+0xC pointer. Static in the original (only caller: GameSysJoystick). */
s32 func_80035328(ReactionSet *set, u8 key, u32 mask, GameObjectRuntime *rt) {
    ReactionEntry *e;
    s32 result;
    s32 offset;

    result = 0;
    offset = 0;
    e = set->entries;
    do {
        if (e->key == key && (e->mask == 0 || (e->mask & mask)) &&
            (e->requiredFlags == 0 || (e->requiredFlags & rt->cas->flags))) {
            set->unk14 = set->unk10 + 1;
            result = e->result;
            break;
        }
        offset += sizeof(ReactionEntry);
        e++;
    } while (((ReactionEntry *)((u8 *)set->entries + offset))->result != 0);
    return result;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035328);
#endif

void iJoystickRelTimerCB(JoyBtnInfo *btn) {
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;

    if (btn->relTicks > 0) {
        CASRequestAction(gobj->id, btn->action);
    }
}

/* Clears the unmapped buttons of a mode. */
void func_80035434(s32 mode) {
    JoyBtnInfo *btn;
    u32 i;

    for (i = JOY_MAPPED_BUTTONS; i < JOY_BUTTONS; i++) {
        btn = &mJoyBtnInfo[mode][i];
        btn->unk00 = 0;
        btn->action = 0;
        btn->unk08 = 0;
        btn->holdDelay = 0;
        btn->relTicks = 0;
        btn->unk0C = 0;
    }
}

void func_80035480(void) {
    u32 i;

    for (i = 0; i < JOY_MAPPED_BUTTONS; i++) {
        mJoyBtnInfo[0][i] = D_8010581C[gJoyMap[i]];
    }
    func_80035434(0);
}

void func_8003551C(void) {
    s32 i;

    for (i = 0; i < JOY_BUTTONS; i++) {
        mJoyBtnInfo[1][i] = mJoyBtnInfo[0][i];
    }
}

void func_80035584(void) {
    s32 i;

    for (i = 0; i < JOY_BUTTONS; i++) {
        mJoyBtnInfo[3][i] = mJoyBtnInfo[0][i];
    }
}

void func_800355EC(void) {
    u32 i;

    for (i = 0; i < JOY_MAPPED_BUTTONS; i++) {
        mJoyBtnInfo[2][i] = D_80105918[gJoyMap[i]];
    }
    func_80035434(2);
}

void func_80035688(void) {
    s32 i;

    for (i = 0; i < JOY_MAPPED_BUTTONS; i++) {
        mJoyBtnInfo[6][i] = D_80105A14[gJoyMap[i]];
    }
    func_80035434(6);
}

void func_80035724(void) {
    s32 i;

    for (i = 0; i < JOY_BUTTONS; i++) {
        mJoyBtnInfo[7][i] = mJoyBtnInfo[0][i];
    }
}


/* D_801056E0 onwards: walk key flag, ..., per-port stick state at 0x50. */
typedef struct JoyStick {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ u8 unk0C[0x30];
} JoyStick;

typedef struct JoyGlobals2 {
    /* 0x00 */ u8 walkKey;
    /* 0x01 */ u8 unk01[0x4F];
    /* 0x50 */ JoyStick sticks[4];
} JoyGlobals2;

#define D_801056E0_s (*(JoyGlobals2 *)&D_801056E0)
void func_80035480(void);
void func_8003551C(void);
void func_80035584(void);
void func_800355EC(void);
void func_80035688(void);
void func_80035724(void);
s32 GameSysJoystickSetMode(s32 mode);

#ifdef NON_MATCHING
/* the original addresses walkKey through the sticks base register */
void GameSysJoystickInit(void) {
    JoyGlobals2 *g2;
    s32 i;

    if (D_801056C8.initialized == 0) {
        D_80105818 = 1;
    }
    D_801056C8.initialized = 1;
    func_80035480();
    func_8003551C();
    func_80035584();
    func_800355EC();
    func_80035688();
    func_80035724();
    GameSysJoystickSetMode(0);
    D_801056D4 = mJoyBtnInfo[0];
    for (i = 0; i < JOY_MAPPED_BUTTONS; i++) {
        switch (gJoyMap[i]) {
        case 2:
            D_801056C8.bitA = i;
            break;
        case 4:
            D_801056C8.btnIndex4 = i;
            break;
        case 3:
            D_801056C8.btnIndex3 = i;
            break;
        case 0:
            D_801056C8.bitB = i;
            break;
        }
    }
    g2 = &D_801056E0_s;
    g2->walkKey = 0;
    D_801056FC = 0xFD7;
    D_801056E8 = 0;
    D_801056F4 = 0;
    D_801056F5 = 0;
    D_801056F8 = 0;
    D_80105700 = D_80105B10;
    for (i = 0; i < 4; i++) {
        g2->sticks[i].unk00 = 0;
        g2->sticks[i].unk04 = 0;
        g2->sticks[i].unk08 = 0;
    }
    D_801057F4 = D_80105B80;
    D_801057F8 = 0;
    D_801057FC = 0;
    D_80105800 = 0;
    D_80105804 = 0;
    D_80105808 = 0;
    D_801056D8 = 0;
    D_801056D9 = 0;
    D_8010580C = 0;
    D_80105810 = 0;
    D_80105814 = 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickInit);
#endif

extern u8 gPlayerRadioFlag;
extern int gSystemState;
extern s32 gSystemTick;
static u16 D_8010FFE0; /* .sbss: pad buttons of the previous frame */
void SysStatePush(int state);
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);
void SysStopMovie(void);
void EGSDisplayQueueInterrupt(void (*fn)(), s32 delay, void *arg);
void PlayerConfirmOrders(void);
Gobj *GameCameraGetPrimaryGobj(void);
void func_8003915C(JoyController *joy, u8 *pad);
s32 func_80039B24(JoyController *joy);

#define JOY_CTRL ((JoyController *)&D_801056C8)

/* The movement stick is pushed. */
static inline u8 JoyIsMoving(void) {
    u8 moving = 0;

    if (JOY_CTRL->sticks[0].unit.vx != 0 || JOY_CTRL->sticks[0].unit.vy != 0 || JOY_CTRL->sticks[0].unit.vz != 0) {
        moving = 1;
    }
    return moving;
}
#define JOY_HOLD_FIRED 666

/* Per-frame joystick handling: movie skip, radio orders, button actions, pause. */
void GameSysJoystick(void) {
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    u8 *pad;
    u16 buttons;
    s32 i;
    JoyBtnInfo *btn;
    s32 action;
    Gobj *primary;
    JoyController *joy;
    JoyBtnInfo **tbl;
    s32 *mode;

    if (JOY_CTRL->initialized == 0 || (u32)gSystemTick < 4 || gobj == NULL) {
        return;
    }
    EGSDeviceControllerRead(&pad, 0);
    buttons = *(u16 *)(pad + 4);
    if (gSystemState == 3) {
        if (D_8010F84C[0]++ > 0x14 && JoyTestBit((u8 *)&buttons, 6)) {
            D_8010F84C[0] = 0;
            SysStopMovie();
        }
        return;
    }
    if (gPlayerRadioFlag) {
        if (D_8010F84C[0]++ > 0x14 && buttons != D_8010FFE0) {
            if (D_8010FFE0 < buttons && JoyTestBit((u8 *)&buttons, 6)) {
                D_8010F84C[0] = 0;
                PlayerConfirmOrders();
                return;
            }
            /* reorg puts this store in a delay slot, so it also happens when buttons > D_8010FFE0 */
            if (buttons != D_8010FFE0 && buttons < D_8010FFE0) {
                D_8010FFE0 = buttons;
            }
        }
        return;
    }
    if (JOY_CTRL->mode == 5 && GameCameraGetMode() == 19 && buttons != 0 && buttons != D_8010FFE0 &&
        D_8010F84C[0]++ > 0x14) {
        if (D_8010FFE0 < buttons && buttons != 0) {
            primary = GameCameraGetPrimaryGobj();
            SysPostMessage(0x13, 4, primary->id, primary->id, 0, 0, 0, 0);
            D_8010F84C[0] = 0;
        } else if (buttons != D_8010FFE0 && buttons < D_8010FFE0) {
            D_8010FFE0 = buttons;
        }
    }
    joy = JOY_CTRL;
    func_8003915C(joy, pad);
    if (D_801056CC == 5 || D_801056D8 != 0) {
        return;
    }
    i = 0;
    tbl = &joy->btns; /* the original keeps &joy->btns in a register across the loop */
    for (; i < JOY_BUTTONS; i++) {
        btn = &(*tbl)[i];
        if (JoyTestBit((u8 *)&buttons, i)) {
            if (btn->unk08 && JoyTestBit((u8 *)&D_8010FFE0, i) && btn->holdDelay < btn->holdCount) {
                continue;
            }
            action = 0;
            if (btn->unk0C != 0) {
                action = func_80035328((ReactionSet *)btn, JoyIsMoving(), buttons, gobj);
            }
            if (action == 0) {
                action = btn->unk00;
            }
            if (btn->holdDelay == 0 || btn->holdCount >= btn->holdDelay) {
                if (btn->holdDelay == 0 || btn->holdCount != JOY_HOLD_FIRED) {
                    btn->holdCount = JOY_HOLD_FIRED;
                    btn->relTicks = -JOY_ABS(btn->relTicks);
                    CASRequestAction(gobj->id, action);
                }
            } else if (btn->holdCount != JOY_HOLD_FIRED) {
                btn->holdCount++;
            }
        } else if (JoyTestBit((u8 *)&D_8010FFE0, i)) {
            if (btn->holdCount < btn->holdDelay) {
                CASRequestAction(gobj->id, btn->unk00);
            }
            btn->holdCount = 0;
            if (btn->relTicks != 0) {
                btn->relTicks = JOY_ABS(btn->relTicks);
                EGSDisplayQueueInterrupt(iJoystickRelTimerCB, btn->relTicks, &(*tbl)[i]);
            } else {
                CASRequestAction(gobj->id, btn->action);
            }
        }
    }
    mode = &JOY_CTRL->mode; /* addressed through &mode in the original */
    if (*mode == 6) {
        func_80039B24(JOY_CTRL);
    }
    if (*mode != 5 && D_801056D9 == 0) {
        if ((JoyTestBit((u8 *)&buttons, 11) && !JoyTestBit((u8 *)&D_8010FFE0, 11)) || pad[0] == 0xFF) {
            D_8010FFE0 |= 0x800;
            SysStatePush(7);
        } else {
            D_8010FFE0 = buttons;
        }
    }
    JOY_CTRL->prevMode = D_801056CC;
}

s32 GameSysJoystickSetMode(s32 mode) {
    s32 i;
    s32 old;

    i = 0;
    old = D_801056C8.mode;
    D_801056C8.mode = mode;
    D_801056D4 = mJoyBtnInfo[mode];
    for (; i < JOY_BUTTONS; i++) {
        *(s32 *)((u8 *)D_801056D4 + i * sizeof(JoyBtnInfo) + 0x14) = mJoyBtnInfo[old][i].holdCount;
    }
    return old;
}

s32 GameSysJoystickGetMode(void) {
    return D_801056CC;
}

void JoystickDeactivate(void) {
    D_801056D8 = 1;
}

void JoystickActivate(void) {
    D_801056D8 = 0;
}

void JoystickDirInputDeactivate(s32 deactivate, u8 arg1) {
    D_80105804 = deactivate;
    D_80105808 = arg1;
}

u8 JoystickIsDirInputDeactivated(void) {
    s32 ret = 0;

    if (D_80105804 != 0 || D_80105808 == 1 || D_801056CC == 5 || D_801056CC == 7 || D_801056D8 == 1) {
        ret = 1;
    }
    return ret;
}

void JoystickDeactivateMenu(u8 deactivate) {
    D_801056D9 = deactivate;
}

u8 JoystickGobjIsWalkKeyPressed(Gobj *gobj) {
    if (gobj->id != gCASJoystickGobjID) {
        return 0;
    }
    return D_801056E0;
}

u8 JoystickGobjIsInPureStrafe(Gobj *gobj) {
    s32 i;

    if (gobj->id != gCASJoystickGobjID) {
        return 0;
    }
    if (D_801056CC != 5) {
        for (i = 0; i < 4; i++) {
            if (i != 3) {
                if (D_80105700[i].unk08 != 0 || D_80105700[i].unk0C != 0 || D_80105700[i].unk10 != 0) {
                    return 0;
                }
            }
        }
        return D_801056F8 != 0;
    }
    return 0;
}

u8 JoystickGobjIsInPureNonRollingStrafe(Gobj *gobj) {
    if (JoystickGobjIsInPureStrafe(gobj) && gobj->cas->active != 9) {
        return 1;
    }
    return 0;
}

u8 JoystickGobjIsInFullRetreat(gobj)
Gobj *gobj;
{
    if (gobj->id == gCASJoystickGobjID) {
        return D_801057FC >= 2;
    }
    return 0;
}

u8 JoystickGobjIsInFastWalkBack(Gobj *gobj) {
    if (gobj->id == gCASJoystickGobjID && gobj->cas->active == 1 && JoystickGobjIsInFullRetreat() &&
        !JoystickGobjIsWalkKeyPressed(gobj)) {
        return 1;
    }
    return 0;
}

u8 JoystickGobjShouldCollideWithEdges(Gobj *gobj) {
    JoyMoveView *move;
    u8 state;
    u32 flags;

    if (gobj->id != gCASJoystickGobjID) {
        return 0;
    }
    state = gobj->cas->active;
    if (state != 4 && !(gobj->cas->flags & 0x1000) && state != 10 && !(gobj->cas->flags & 0x8000) &&
        state != 12 && state != 2) {
        return 0;
    }
    if (JoystickGobjIsInFastWalkBack(gobj)) {
        return 0;
    }
    move = (JoyMoveView *)gobj->move;
    flags = move->edgeFlags;
    if ((flags & 0x100) && move->velY > -0x1D709) {
        return 0;
    }
    if (flags & 0x80000) {
        return 0;
    }
    state = gobj->cas->active;
    if (state == 9 || state == 6) {
        return 0;
    }
    return 1;
}

void func_8003642C(JoyInput *input, u32 *pad) {
    u8 *bits;
    u8 was0;
    u8 was1;

    if (input->kind == 6) {
        return;
    }
    bits = (u8 *)(pad + 1);
    was0 = input->down0;
    was1 = input->down1;
    input->down0 = JoyTestBit(bits, input->bit0);
    input->down1 = JoyTestBit(bits, input->bit1);
    if (input->down0) {
        if (!input->down1 || !was0) {
            input->state = 1;
        } else if (!was1) {
            input->state = 2;
        }
    } else if (input->down1) {
        input->state = 2;
    } else {
        input->state = 0;
    }
}

s32 EGSMath_VectorModulus(VECTOR32 *v, s32 *out);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);

#define JOY_SIGN(x, mag) ((x) > 0 ? (mag) : (x) < 0 ? -(mag) : 0)

/* Stick vector to an XZ direction: analog with a dead zone, or 8-way digital. */
void func_80036520(VECTOR32 *in, u8 analog, VECTOR32 *out) {
    s32 len;

    if (analog) {
        *out = *in;
        out->vy = 0;
        EGSMath_VectorModulus(out, &len);
        if (len < 0x38) {
            out->vx = 0;
            out->vy = 0;
            out->vz = 0;
        } else if (len >= 0x7F) {
            GTEMath_VectorNormal(out, out);
        } else {
            out->vx = (out->vx << 12) * (len - 0x37);
            out->vy = (out->vy << 12) * (len - 0x37);
            out->vz = (out->vz << 12) * (len - 0x37);
            out->vx = out->vx / (len * 0x48);
            out->vy = out->vy / (len * 0x48);
            out->vz = out->vz / (len * 0x48);
        }
        return;
    }
    out->vy = 0;
    if (in->vx != 0 && in->vz != 0) {
        out->vx = JOY_SIGN(in->vx, 0xB50);
        out->vz = JOY_SIGN(in->vz, 0xB50);
    } else {
        out->vx = JOY_SIGN(in->vx, 0x1000);
        out->vz = JOY_SIGN(in->vz, 0x1000);
    }
}

void func_80036768(JoyController *joy) {
    JoyDirInfo *dirs = joy->dirs;
    s32 i;

    if (joy->dirActive == 1) {
        joy->dirActive = 0;
        for (i = 0; i < 4; i++) {
            if (dirs[i].unk08 != 0 || dirs[i].unk0C != 0 || dirs[i].unk10 != 0) {
                joy->dirActive = 1;
                break;
            }
        }
    }
    if (JoystickIsDirInputDeactivated()) {
        for (i = 0; i < 4; i++) {
            dirs[i].unk08 = 0;
            dirs[i].unk0C = 0;
            dirs[i].unk10 = 0;
        }
        joy->unk030 = 0;
    }
    if (joy->unk13C > 0) {
        joy->unk13C--;
    }
}




void func_80036848(JoyController *joy) {
    JoyDirInfo *dirs = joy->dirs;
    JoyDirDefault *defaults;
    JoyDirDefault *def;
    s32 set;
    s32 i;

    switch (GameCameraGetMode()) {
        case 14:
        case 15:
        case 16:
        case 17:
            set = 4;
            break;
        case 9:
            set = 3;
            break;
        case 5:
        case 13:
            set = 2;
            break;
        default:
            set = joy->unk030 != 0;
            break;
    }
    defaults = joy->dirDefaults + set * 4;
    for (i = 0; i < 4; i++) {
        def = &defaults[dirs[i].type];
        if (!JoyTestBit((u8 *)&dirs[i].unk00, 5)) {
            dirs[i].unk00 = def->unk00;
        }
        if (!JoyTestBit((u8 *)&dirs[i].unk04, 5)) {
            dirs[i].unk04 = def->unk04;
        }
    }
}

void func_8003699C(void) {
}

/* Global: still called from asm func_8003915C. */
void func_800369A4(u8 *arg0) {
    *(s32 *)(arg0 + 0x138) = 0;
}

/* What joystick.c needs of camera.c's Camera: display->frame. */
typedef struct JoyCameraDisplay {
    /* 0x00 */ EGSFrame *frame;
} JoyCameraDisplay;

typedef struct JoyCamera {
    /* 0x000 */ JoyCameraDisplay *display;
    /* 0x004 */ u8 unk004[0x948];
    /* 0x94C */ s32 angleFull; /* 0: only angle.vx is meaningful */
    /* 0x950 */ u8 unk950[0x34];
    /* 0x984 */ VECTOR32 angle;
    /* 0x994 */ u8 unk994[0x240];
    /* 0xBD4 */ s32 zoomFull; /* 0: only zoom.vx is meaningful */
    /* 0xBD8 */ u8 unkBD8[4];
    /* 0xBDC */ VECTOR32 zoom;
    /* 0xBEC */ u8 unkBEC[0x130];
    /* 0xD1C */ VECTOR32 move;      /* request for CameraAccessMoveInfo */
    /* 0xD2C */ u8 unkD2C[4];
    /* 0xD30 */ VECTOR32 moveFlags; /* which parts of move apply */
    /* 0xD40 */ u8 moveCommit;
} JoyCamera;

extern JoyCamera *gCamera;
s32 EGSMathFIXMul(s32 a, s32 b);


/* Joystick-side view of a CharacterMovement (movement.c) for turning. */
typedef struct JoyTurnMove {
    /* 0x00 */ s32 turnSpeedStill;
    /* 0x04 */ s32 turnSpeedMoving;
    /* 0x08 */ VECTOR32 input; /* stick direction, character relative */
    /* 0x18 */ VECTOR32 vel;
    /* 0x28 */ s32 speed;
    /* 0x2C */ VECTOR32 dir; /* overrides vel when nonzero */
    /* 0x3C */ s32 type;     /* 5, 8, 9: standing still */
} JoyTurnMove;

#define JOY_TURNMOVE(gobj) (*(JoyTurnMove **)((u8 *)(gobj)->move + 0x1A8))

typedef struct JoySVector {
    s16 vx, vy, vz, pad;
} JoySVector;

void RotMatrixYXZ(JoySVector *r, GOBJ_MATRIX *m);
s32 ratan2(s32 y, s32 x);
void EGSGeometry_AngleInterpolate(s32 from, s32 to, s32 step, u8 mode, s32 *out, s32 *remain);

#define JOY_FIX_TO_INT(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

static inline void JoyAngleToMatrixYXZ(VECTOR32 *angle, GOBJ_MATRIX *m) {
    JoySVector r;

    r.vx = -angle->vx;
    r.vy = angle->vy;
    r.vz = -angle->vz;
    RotMatrixYXZ(&r, m);
    m->m[0][1] = -m->m[0][1];
    m->m[1][0] = -m->m[1][0];
    m->m[1][2] = -m->m[1][2];
    m->m[2][1] = -m->m[2][1];
}

static inline u8 JoyGobjIsStill(Gobj *gobj) {
    s32 type = JOY_TURNMOVE(gobj)->type;
    s32 still = 0;

    if ((gobj->cas->flags & 2) || type == 5 || type == 8 || type == 9) {
        still = 1;
    }
    return still;
}

/* Facing direction (matrix column 2) for YXZ angles. */
static inline void JoyAngleToForward(VECTOR32 *angle, VECTOR32 *fwd) {
    GOBJ_MATRIX m;

    JoyAngleToMatrixYXZ(angle, &m);
    fwd->vx = m.m[0][2];
    fwd->vy = m.m[1][2];
    fwd->vz = m.m[2][2];
}

static inline void JoyArctan(s32 y, s32 x, s32 *out) {
    *out = ratan2(y, x);
}


/* Convert between camera-relative stick input and world XZ (toWorld: 1). */
void func_800369AC(VECTOR32 *local, VECTOR32 *world, u8 toWorld) {
    VECTOR32 x;
    VECTOR32 y;
    VECTOR32 z;
    GOBJ_MATRIX m;
    s32 *px;
    s32 *pz;

    if (toWorld == 1 && local->vx == 0 && local->vz == 0) {
        world->vx = 0;
        world->vy = 0;
        world->vz = 0;
        return;
    }
    if (toWorld == 0 && world->vx == 0 && world->vz == 0) {
        local->vx = 0;
        local->vy = 0;
        local->vz = 0;
        return;
    }
    m.m[0][0] = gCamera->display->frame->m[0][0];
    m.m[0][1] = -gCamera->display->frame->m[0][1];
    m.m[0][2] = gCamera->display->frame->m[0][2];
    m.m[1][0] = -gCamera->display->frame->m[1][0];
    m.m[1][1] = gCamera->display->frame->m[1][1];
    m.m[1][2] = -gCamera->display->frame->m[1][2];
    m.m[2][0] = gCamera->display->frame->m[2][0];
    m.m[2][1] = -gCamera->display->frame->m[2][1];
    m.m[2][2] = gCamera->display->frame->m[2][2];
    x.vx = m.m[0][0];
    x.vy = m.m[1][0];
    x.vz = m.m[2][0];
    y.vx = m.m[0][1];
    y.vy = m.m[1][1];
    y.vz = m.m[2][1];
    z.vx = m.m[0][2];
    z.vy = m.m[1][2];
    z.vz = m.m[2][2];
    if (JOY_ABS(x.vy) >= 0xB50) {
        x.vx = z.vz;
        z.vy = 0;
        x.vy = 0;
        x.vz = -z.vx;
    } else {
        x.vy = 0;
        z.vx = -x.vz;
        z.vy = 0;
        z.vz = x.vx;
    }
    if (toWorld == 1) {
        px = &x.vx;
        px[0] = EGSMathFIXMul(x.vx, local->vx);
        px[1] = EGSMathFIXMul(x.vy, local->vx);
        px[2] = EGSMathFIXMul(x.vz, local->vx);
        pz = &z.vx;
        pz[0] = EGSMathFIXMul(z.vx, local->vz);
        pz[1] = EGSMathFIXMul(z.vy, local->vz);
        pz[2] = EGSMathFIXMul(z.vz, local->vz);
        world->vx = x.vx + z.vx;
        world->vy = x.vy + z.vy;
        world->vz = x.vz + z.vz;
    } else {
        local->vx = EGSMathFIXMul(world->vx, x.vx) + EGSMathFIXMul(world->vy, x.vy) +
                    EGSMathFIXMul(world->vz, x.vz);
        local->vy = 0;
        local->vz = EGSMathFIXMul(world->vx, z.vx) + EGSMathFIXMul(world->vy, z.vy) +
                    EGSMathFIXMul(world->vz, z.vz);
    }
}

s32 MovementGobjRegisterCharacterMovement(Gobj *gobj, VECTOR32 *vel, s32 type);
u8 GameCameraCheckMode(s32 mode);
u8 GameCameraCheckJoystickMode(s32 mode);
Gobj *GameCameraGetPrimaryGobj(void);
s32 GameCameraGetAction(void);
void GameCameraSetAction(s32 action);
void GameCameraCurrentModeGetDefaultDistance(s32 *dist);
void CameraAccessMoveInfo(void *camera, s32 op, s32 index, s32 arg3);

typedef struct JoyCollInfo {
    /* 0x00 */ s32 mode; /* 1: grabbing */
} JoyCollInfo;

#define JOY_GRABBING(gobj)                                                                     \
    ((gobj) != NULL && (gobj)->move != NULL && (gobj)->move->coll != NULL &&                   \
     ((JoyCollInfo *)(gobj)->move->coll)->mode == 1)

#define JOY_VEC_NONZERO(v) ((v).vx != 0 || (v).vy != 0 || (v).vz != 0)


#define JOY_WRAP_ANGLE(a)          \
    if ((a) > 0x800) {             \
        (a) -= 0x1000;             \
    } else if ((a) < -0x800) {     \
        (a) += 0x1000;             \
    }

static inline void JoyDot(VECTOR32 *a, VECTOR32 *b, s32 *out) {
    *out = EGSMathFIXMul(a->vx, b->vx) + EGSMathFIXMul(a->vy, b->vy) + EGSMathFIXMul(a->vz, b->vz);
}

/* The rotation angles the movement update will produce next. */
static inline void JoyGobjGetNextAngle(Gobj *gobj, VECTOR32 *angle) {
    VECTOR32 d;

    d.vx = JOY_FIX_TO_INT(((JoyMoveView *)gobj->move)->nextRotDelta.vx);
    d.vy = JOY_FIX_TO_INT(((JoyMoveView *)gobj->move)->nextRotDelta.vy);
    d.vz = JOY_FIX_TO_INT(((JoyMoveView *)gobj->move)->nextRotDelta.vz);
    angle->vx = ((JoyMoveView *)gobj->move)->nextRot.vx + d.vx;
    JOY_WRAP_ANGLE(angle->vx);
    angle->vy = ((JoyMoveView *)gobj->move)->nextRot.vy + d.vy;
    JOY_WRAP_ANGLE(angle->vy);
    angle->vz = ((JoyMoveView *)gobj->move)->nextRot.vz + d.vz;
    JOY_WRAP_ANGLE(angle->vz);
}

/* Movement stick to a character movement request (type 5: still, 6/7: moving). */
s32 func_80036D24(JoyController *joy) {
    VECTOR32 world = D_800114C8;
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    JoyStickState *stick = &joy->sticks[0];
    JoyTurnMove *cm;
    s32 type;
    VECTOR32 fwd;
    VECTOR32 local;
    VECTOR32 angle;
    s32 speed;
    s32 moveType;
    u8 fpsCam;
    u8 notAhead;
    s32 dot;

    if (gobj == NULL || gobj->move == NULL || JOY_TURNMOVE(gobj) == NULL) {
        return 0;
    }
    cm = JOY_TURNMOVE(gobj);
    type = cm->type;
    JoyGobjGetNextAngle(gobj, &angle);
    {
        GOBJ_MATRIX m;

        JoyAngleToMatrixYXZ(&angle, &m);
        fwd.vx = m.m[0][2];
        fwd.vy = m.m[1][2];
        fwd.vz = m.m[2][2];
    }
    fpsCam = 0;
    if (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) || GameCameraCheckMode(3) ||
        GameCameraCheckMode(8)) {
        fpsCam = 1;
    }
    if (JoystickIsDirInputDeactivated()) {
        world.vx = 0;
        world.vy = 0;
        world.vz = 0;
        MovementGobjRegisterCharacterMovement(gobj, &world, 5);
        local.vx = 0;
        local.vy = 0;
        local.vz = 0;
        cm->input = local;
        CASRequestAction(gobj->id, 4);
        return 1;
    }
    local = stick->unit;
    if (stick->unit.vx != 0 || stick->unit.vy != 0 || stick->unit.vz != 0) {
        func_800369AC(&stick->unit, &world, 1);
        speed = stick->len;
    } else {
        speed = 0;
        world.vx = 0;
        world.vy = 0;
        world.vz = 0;
    }
    if (joy->unk138 == 3 || (joy->unk138 == 4 && local.vx == 0 && local.vy == 0 && local.vz == 0)) {
        if (type == 5 || type == 8 || type == 9) {
            speed = 0;
            world.vx = 0;
            world.vy = 0;
            world.vz = 0;
            local.vx = 0;
            local.vy = 0;
            local.vz = 0;
        } else {
            speed = 0x1000;
            world.vx = gobj->anim->frame->m[0][2];
            world.vy = gobj->anim->frame->m[1][2];
            world.vz = gobj->anim->frame->m[2][2];
            world.vy = -world.vy;
            func_800369AC(&local, &world, 0);
        }
        moveType = type;
    } else if (GameCameraCheckMode(16)) {
        moveType = 5;
    } else if (((JoyCollInfo *)gobj->move->coll)->mode == 1 && gobj->cas->active != 8) {
        moveType = 5;
    } else if (!(gobj->cas->active == 4 || gobj->cas->active == 10 || gobj->cas->active == 12) &&
               (world.vx != 0 || world.vy != 0 || world.vz != 0)) {
        if (gobj->cas->active == 8) {
            if (JOY_ABS(local.vz) < 0xF0A) {
                moveType = 6;
            } else {
                moveType = 5;
            }
        } else if (!(gobj->cas->active == 1 || gobj->cas->active == 2 || gobj->cas->active == 9)) {
            moveType = 5;
        } else {
            JoyDot(&fwd, &world, &dot);
            if (joy->aimState >= 2) {
                dot = -dot;
            }
            notAhead = dot < 0xDDC;
            if ((type == 5 || type == 8 || type == 9) &&
                ((fpsCam && joy->unk030 == 0) ? (local.vz >= -0xA49 && local.vz <= 0xA49) : notAhead)) {
                moveType = 5;
            } else if (joy->prevAimState >= 2 && joy->aimState < 2) {
                moveType = 5;
            } else if (joy->prevAimState < 2 && joy->aimState >= 2) {
                moveType = 5;
            } else if (joy->aimState >= 2) {
                moveType = 6;
            } else {
                moveType = 7;
            }
        }
    } else {
        moveType = 5;
    }
    if ((moveType == 6 && joy->aimState < 2) ||
        (moveType == 7 && (gobj->cas->active == 2 || (gobj->cas->flags & 0x100000)))) {
        if (((JoyMoveView *)gobj->move)->edgeFlags & 8) {
            moveType = 5;
        }
    }
    MovementGobjRegisterCharacterMovement(gobj, &world, moveType);
    CASRequestAction(gobj->id, 4);
    cm->speed = speed;
    cm->input = local;
    if (moveType == 7) {
        joy->runTicks = 10;
    } else if (joy->runTicks > 0) {
        joy->runTicks--;
    } else {
        joy->runTicks = 0;
    }
    return 1;
}

extern s32 gSniperMode;
JoyCamera *SniperGetCam(void);
void CameraAccessFrameInfo(void *camera, s32 op, s32 which, s32 arg3);
void InterfaceSetSniperDoodads(s32 angle, s32 pitch, s32 zoom);
void LOSGetGobjLOSTestPoint(Gobj *gobj, VECTOR32 *out);
void LOSOverride(VECTOR32 *pos);

/* A camera vector record that may hold only its x part. */
#define JOY_CAM_ZOOM(cam, out)        \
    if ((cam)->zoomFull == 0) {       \
        (out).vx = (cam)->zoom.vx;    \
    } else {                          \
        (out) = (cam)->zoom;          \
    }

/* Angle of the look stick; behind the player when aiming. */
#define JOY_LOOK_ANGLE(look) ratan2((look)->unit.vx, (look)->unit.vz)
#define JOY_LOOK_ANGLE_BACK(look) (-ratan2((look)->unit.vx, -(look)->unit.vz))

/* Turning towards the look stick (dead zone 0x11C, full at 0x11C + 2960). */
#define JOY_SCALE_DEADZONE(speed, angle) ((JOY_FIX_TO_INT((speed) * 4) * (JOY_ABS(angle) - 0x11C)) / 2960 << 12)
#define JOY_SCALE(speed, angle) ((JOY_FIX_TO_INT((speed) * 4) * JOY_ABS(angle)) / 4096 << 12)

#ifdef NON_MATCHING
/* draft: structure follows the asm, but speed lives in a stack slot (sp74) and the u8 flags spill; stack layout and regalloc differ */
/* Look stick to camera: sniper zoom, side pans, and the camera turn rate. */
s32 func_800374E8(JoyController *joy) {
    VECTOR32 cam;
    VECTOR32 flags;
    u8 *pad;
    VECTOR32 vec;
    JoyStickState *move;
    JoyStickState *look;
    Gobj *gobj;
    u8 still;
    u8 fpsCam;
    u8 behindCam;
    s32 maxSpeed;
    s32 speed;
    s32 i;

    still = 0;
    move = &joy->sticks[1];
    gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    flags = D_800114C8;
    look = &joy->sticks[2];
    if ((gobj->cas->flags & 2) || JOY_TURNMOVE(gobj)->type == 5 || JOY_TURNMOVE(gobj)->type == 8 ||
        JOY_TURNMOVE(gobj)->type == 9) {
        still = 1;
    }
    fpsCam = 0;
    behindCam = 0;
    if (GameCameraCheckMode(0xE) || GameCameraCheckMode(0xF) || GameCameraCheckMode(0x10) ||
        GameCameraCheckMode(0x11)) {
        fpsCam = 1;
        behindCam = 0;
    }
    if (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) || GameCameraCheckMode(3) ||
        GameCameraCheckMode(8)) {
        behindCam = 1;
    }

    if (gSniperMode == 2 || gSniperMode == 3) {
        VECTOR32 zoom;

        EGSDeviceControllerRead(&pad, 0);
        if (JoyBitMask(pad + 4, 4)) {
            if (joy->sniperZoom > 0) {
                joy->sniperZoom = 0;
            } else {
                i = joy->sniperZoom - 0x2D;
                if (i < -0x2D) {
                    i = -0x2D;
                }
                joy->sniperZoom = i;
            }
        } else if (JoyBitMask(pad + 4, 5) && joy->sniperZoom >= 0) {
            i = joy->sniperZoom + 0x2D;
            if (i > 0x2D) {
                i = 0x2D;
            }
            joy->sniperZoom = i;
        } else {
            joy->sniperZoom = 0;
        }
        SniperGetCam()->move.vx = joy->sniperZoom;
        SniperGetCam()->moveCommit = 1;
        CameraAccessMoveInfo(SniperGetCam(), 3, 8, 1);
        CameraAccessFrameInfo(gCamera, 0, 0, 4);
        vec = gCamera->move;
        JOY_CAM_ZOOM(SniperGetCam(), zoom);
        InterfaceSetSniperDoodads(vec.vx, vec.vy, zoom.vx);
    } else {
        joy->sniperZoom = 0;
    }

    if (GameCameraCheckMode(0xE) || GameCameraCheckMode(0xF) || GameCameraCheckMode(0x10)) {
        VECTOR32 los;
        VECTOR32 eye;
        VECTOR32 *override;

        vec = D_800114C8;
        i = JOY_ABS(joy->panTicks);
        override = NULL;
        if (i < 1 || i > 19) {
            LOSGetGobjLOSTestPoint(gobj, &los);
            eye.vx = gCamera->display->frame->pos.vx;
            eye.vy = gCamera->display->frame->pos.vy;
            override = &los;
            eye.vz = gCamera->display->frame->pos.vz;
            eye.vy = -eye.vy;
            los.vx = eye.vx;
            los.vz = eye.vz;
        }
        LOSOverride(override);
        if (joy->unk030 == 1) {
            vec.vx = 0x80;
            if (joy->panTicks <= 0) {
                joy->panTicks = 1;
            } else if (joy->panTicks < 20) {
                joy->panTicks++;
            }
        } else if (joy->unk030 == 2) {
            vec.vx = -0x80;
            if (joy->panTicks >= 0) {
                joy->panTicks = -1;
            } else if (joy->panTicks >= -19) {
                joy->panTicks--;
            }
        } else {
            vec.vx = 0;
            joy->panTicks = 0;
        }
        flags.vx = 1;
        flags.vy = 0;
        flags.vz = 0;
        gCamera->move = vec;
        gCamera->moveFlags = flags;
        gCamera->moveCommit = 0;
        CameraAccessMoveInfo(gCamera, 1, 1, 1);
    } else {
        joy->panTicks = 0;
        LOSOverride(NULL);
    }

    if (gSniperMode >= 2) {
        maxSpeed = 0x2D000;
        speed = maxSpeed;
    } else {
        if (!fpsCam && behindCam) {
            maxSpeed = 0x4C000;
            if (gobj->cas->active != 10 && !(gobj->cas->flags & 0x8000) && gobj->cas->active != 12 &&
                gobj->cas->active != 7 && !(gobj->cas->flags & 0x2000) && gobj->cas->active != 4 &&
                !(gobj->cas->flags & 0x1000)) {
                maxSpeed = 0x72000;
            }
        } else {
            maxSpeed = 0x4C000;
        }
        speed = maxSpeed;
        if (JOY_VEC_NONZERO(look->unit)) {
            if (gobj->cas->active == 10 || (gobj->cas->flags & 0x8000) || gobj->cas->active == 12 ||
                gobj->cas->active == 7 || (gobj->cas->flags & 0x2000)) {
                if (JOY_ABS(look->unit.vz) >= 0xA4A) {
                    speed = 0;
                }
            } else if (gobj->cas->active == 4 || (gobj->cas->flags & 0x1000)) {
                speed = 0;
            } else if (joy->unk030 != 0) {
                i = JOY_FIX_TO_INT(speed * 4);
                if (JOY_ABS(JOY_LOOK_ANGLE(look)) > 0x400) {
                    speed = (i / 4) << 12;
                } else {
                    speed = (i * JOY_ABS(JOY_LOOK_ANGLE(look))) / 4096 << 12;
                }
            } else if (!still) {
                if (look->srcX == 1 || look->srcZ == 1 || look->srcX == 0 || look->srcZ == 0) {
                    if (joy->aimState >= 2) {
                        if (JOY_ABS(JOY_LOOK_ANGLE_BACK(look)) - 0x11C <= 0) {
                            speed = 0;
                        } else {
                            speed = JOY_SCALE_DEADZONE(speed, JOY_LOOK_ANGLE_BACK(look));
                        }
                    } else if (JOY_ABS(JOY_LOOK_ANGLE(look)) - 0x11C <= 0) {
                        speed = 0;
                    } else {
                        speed = JOY_SCALE_DEADZONE(speed, JOY_LOOK_ANGLE(look));
                    }
                } else if (joy->aimState >= 2) {
                    speed = JOY_SCALE(speed, JOY_LOOK_ANGLE_BACK(look));
                } else {
                    speed = JOY_SCALE(speed, JOY_LOOK_ANGLE(look));
                }
            }
        }
    }

    speed = EGSMathFIXMul(speed, fpsCam ? move->len : look->len);
    if ((look->srcX == 1 || look->srcZ == 1 || look->srcX == 0 || look->srcZ == 0) && !still && behindCam) {
        move->turn.vz = 0;
        if (move->dir.vx > 0) {
            move->turn.vx = speed;
        } else if (move->dir.vx < 0) {
            move->turn.vx = -speed;
        } else {
            move->turn.vx = 0;
        }
    } else {
        VECTOR32 rate;
        VECTOR32 cur;
        VECTOR32 side;
        s32 along;
        s32 across;

        if (fpsCam) {
            if (gSniperMode >= 2) {
                rate.vy = 0x126;
                rate.vz = 0x1000;
                JOY_CAM_ZOOM(SniperGetCam(), rate);
                rate.vy = rate.vy * rate.vx / 827;
            } else {
                rate.vy = 0x80;
                rate.vz = 0x266;
            }
        } else {
            rate.vy = 0x199;
            rate.vz = 0x1000;
        }
        if (rate.vy > 0x1000) {
            rate.vy = 0x1000;
        }
        if (rate.vz > 0x1000) {
            rate.vz = 0x1000;
        }
        rate.vy = EGSMathFIXMul(rate.vy, speed);
        rate.vz = EGSMathFIXMul(rate.vz, maxSpeed);
        if (speed != 0 && rate.vy < 0x3D) {
            rate.vy = 0x3D;
        }
        if (maxSpeed != 0 && rate.vz < 0x3D) {
            rate.vz = 0x3D;
        }
        cur = move->turn;
        vec = move->unit;
        if (vec.vx == 0 && vec.vy == 0 && vec.vz == 0 && JOY_VEC_NONZERO(cur)) {
            GTEMath_VectorNormal(&cur, &vec);
            vec.vx = -vec.vx;
            vec.vz = -vec.vz;
            vec.vy = -vec.vy;
        }
        side.vy = 0;
        side.vx = vec.vz;
        side.vz = -vec.vx;
        JoyDot(&cur, &vec, &along);
        JoyDot(&cur, &side, &across);
        if (along > speed) {
            along = along - rate.vz < speed ? speed : along - rate.vz;
        } else if (along >= 0) {
            along = along + rate.vy > speed ? speed : along + rate.vy;
        } else {
            along = along + rate.vz > 0 ? 0 : along + rate.vz;
        }
        if (across > 0) {
            across = across - rate.vy < 0 ? 0 : across - rate.vy;
        } else if (across < 0) {
            across = across + rate.vy > 0 ? 0 : across + rate.vy;
        }
        vec.vx = EGSMathFIXMul(vec.vx, along);
        vec.vy = EGSMathFIXMul(vec.vy, along);
        vec.vz = EGSMathFIXMul(vec.vz, along);
        side.vx = EGSMathFIXMul(side.vx, across);
        side.vy = EGSMathFIXMul(side.vy, across);
        side.vz = EGSMathFIXMul(side.vz, across);
        move->turn.vx = vec.vx + side.vx;
        move->turn.vy = vec.vy + side.vy;
        move->turn.vz = vec.vz + side.vz;
    }

    cam.vx = move->turn.vz;
    cam.vz = 0;
    cam.vy = move->turn.vx;
    if (JOY_ABS(cam.vx) > 0x1000) {
        cam.vx = JOY_FIX_TO_INT(cam.vx);
    } else {
        cam.vx = cam.vx > 0 ? 1 : cam.vx >> 31;
    }
    if (JOY_ABS(cam.vy) > 0x1000) {
        cam.vy = JOY_FIX_TO_INT(cam.vy);
    } else {
        cam.vy = cam.vy > 0 ? 1 : cam.vy >> 31;
    }
    if (joy->invertY == 0) {
        cam.vx = -cam.vx;
    }
    if (joy->aimState == 3) {
        cam.vy = 0x800;
    }
    flags.vx = 1;
    flags.vy = 1;
    flags.vz = 0;
    gCamera->move = cam;
    gCamera->moveFlags = flags;
    gCamera->moveCommit = 1;
    CameraAccessMoveInfo(gCamera, 3, 6, 1);
    if (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) || GameCameraCheckMode(3) ||
        GameCameraCheckMode(8)) {
        VECTOR32 none;

        none = D_800114C8;
        flags.vx = 1;
        flags.vy = 0;
        flags.vz = 0;
        gCamera->move = none;
        gCamera->moveFlags = flags;
        gCamera->moveCommit = 0;
        CameraAccessMoveInfo(gCamera, 1, 6, 2);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800374E8);
#endif


/* Joystick-side view of MovementInfo.charMove (movement.c's CharacterMovement). */
typedef struct JoyCharMove {
    /* 0x00 */ u8 unk00[0x3C];
    /* 0x3C */ s32 type; /* 5, 8, 9: standing still */
} JoyCharMove;

#ifdef NON_MATCHING
/* branch layout of the aim-height test and the still flag copy differ */
/* Update the aim state machine from the aiming stick. */
void func_80038720(JoyController *joy) {
    s32 still = 0;
    u32 state = joy->aimState;
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    VECTOR32 v = joy->sticks[3].unit;
    s32 type = (*(JoyCharMove **)((u8 *)gobj->move + 0x1A8))->type;
    JoyStickState *aim = &joy->sticks[3];
    s32 lowY;
    s32 highY;
    u32 next;

    if ((gobj->cas->flags & 2) || type == 5 || (u32)(type - 8) < 2) {
        still = 1;
    }
    lowY = -0x578;
    if (still) {
        lowY = -0xA49;
    }
    highY = 0x578;
    if (joy->aimState >= 2 && joy->unk138 == 3) {
        next = 4;
        if (state == 5) {
            next = 5;
        }
    } else if ((gobj->cas->active != 1 && gobj->cas->active != 2) ||
               (!GameCameraCheckMode(0) && !GameCameraCheckMode(1) && !GameCameraCheckMode(2) &&
                !GameCameraCheckMode(3) && !GameCameraCheckMode(8)) ||
               joy->unk030 != 0) {
        next = 0;
    } else if (v.vx != 0 || v.vy != 0 || v.vz != 0) {
        if (state < 2 ? lowY < v.vz : highY < v.vz) {
            next = 0;
        } else if (state == 1) {
            aim->turn.vz = -1;
            next = 2;
        } else if (state == 2) {
            if ((double)aim->turn.vz > -2.0) {
                aim->turn.vz = aim->turn.vz - 1;
                next = 2;
            } else {
                next = 4;
            }
        } else if (state == 4) {
            next = 4;
        } else {
            next = 5;
        }
    } else if (state == 2) {
        next = 3;
    } else if (gobj->cas->flags & 2) {
        next = 1;
    } else {
        next = state;
        if (next == 3) {
            next = 0;
        }
    }
    joy->prevAimState = state;
    joy->aimState = next;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80038720);
#endif

/* Pick the camera action and the character's facing override from the sticks. */
void func_800389E0(JoyController *joy) {
    JoyStickState *move = &joy->sticks[0];
    JoyStickState *strafe = &joy->sticks[1];
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    JoyStickState *look = &joy->sticks[2];
    JoyTurnMove *cm;
    u8 fpsCam;
    s32 action;
    VECTOR32 angle;
    VECTOR32 fwd;
    s32 dist;

    if (gobj == NULL || gobj->move == NULL || JOY_TURNMOVE(gobj) == NULL) {
        return;
    }
    cm = JOY_TURNMOVE(gobj);
    fpsCam = 0;
    if (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) || GameCameraCheckMode(3) ||
        GameCameraCheckMode(8)) {
        fpsCam = 1;
    }
    action = GameCameraGetAction();
    if (gCamera->angleFull == 0) {
        angle.vx = gCamera->angle.vx;
    } else {
        angle = gCamera->angle;
    }
    angle.vx = 0;
    angle.vz = 0;
    JoyAngleToForward(&angle, &fwd);
    if ((action == 7 || action == 5) &&
        (JOY_VEC_NONZERO(move->unit) || JOY_VEC_NONZERO(strafe->unit) || JOY_VEC_NONZERO(look->unit))) {
        action = 0;
    }
    if (!(gobj->cas->flags & 0x1000) && !JOY_GRABBING(gobj)) {
        if (action == 5) {
            goto clear;
        }
        if (action == 7) {
            goto done;
        }
        if (!(gobj->cas->active == 1 || gobj->cas->active == 2 || gobj->cas->active == 9)) {
            goto clear;
        }
        if ((GameCameraCheckMode(14) || GameCameraCheckMode(15) || GameCameraCheckMode(16) ||
             GameCameraCheckMode(17) || fpsCam) &&
            cm->type == 5 && (gobj->cas->flags & 2) && gobj->cas->active != 9 && !(gobj->cas->flags & 0x400) &&
            strafe->dir.vx != 0) {
            cm->type = strafe->dir.vx > 0 ? 9 : 8;
        }
        if (joy->aimState >= 2 && GameCameraCheckMode(9)) {
            cm->dir.vx = -cm->vel.vx;
            cm->dir.vy = -cm->vel.vy;
            cm->dir.vz = -cm->vel.vz;
            goto done;
        }
        if ((GameCameraCheckMode(14) || GameCameraCheckMode(15) || GameCameraCheckMode(16) ||
             GameCameraCheckMode(17)) &&
            JoyGobjIsStill(gobj) && !JOY_VEC_NONZERO(move->dir) && strafe->dir.vx != 0) {
            goto faceFwd;
        }
        if (GameCameraCheckJoystickMode(1) || GameCameraGetPrimaryGobj() != gameObjectsInfo[gCASJoystickGobjID].runtime) {
            if (JOY_VEC_NONZERO(cm->vel)) {
                goto followVel;
            }
            goto done;
        }
        if (!fpsCam || joy->unk138 == 3) {
            goto clear;
        }
        if (joy->unk030 != 0) {
        followVel:
            cm->dir = cm->vel;
            goto done;
        }
        if ((JoyGobjIsStill(gobj) && gobj->cas->active != 9 && !(gobj->cas->flags & 0x400)) ||
            JOY_VEC_NONZERO(look->dir)) {
        faceFwd:
            cm->dir = fwd;
        }
        goto done;
    clear:
        cm->dir.vx = 0;
        cm->dir.vy = 0;
        cm->dir.vz = 0;
    }
done:
    if (joy->aimState == 3) {
        action = 7;
        cm->dir = fwd;
    }
    if (joy->aimState >= 2 && (fpsCam || GameCameraCheckMode(9))) {
        GameCameraCurrentModeGetDefaultDistance(&dist);
        dist = dist >= 0 ? dist >> 1 : -(-dist >> 1);
        gCamera->move.vx = dist;
        gCamera->moveCommit = 0;
        CameraAccessMoveInfo(gCamera, 1, 5, 1);
    }
    GameCameraSetAction(action);
}

s32 func_80036D24(JoyController *joy);
s32 func_800374E8(JoyController *joy);
void func_800389E0(JoyController *joy);
void func_80038720(JoyController *joy);

/* Per-frame update of a joystick controller from the raw pad data. */
void func_8003915C(JoyController *joy, u8 *pad) {
    JoyDirInfo *dirs = joy->dirs;
    JoyStickState *sticks = joy->sticks;
    s32 i;
    s32 j;
    u32 xbit;
    u32 zbit;
    u8 both;

    joy->downA = JoyTestBit(pad + 4, joy->bitA);
    joy->downB = JoyTestBit(pad + 4, joy->bitB);
    func_8003642C((JoyInput *)joy, (u32 *)pad);
    for (i = 0; i < 4; i++) {
        sticks[i].dir.vx = 0;
        sticks[i].dir.vy = 0;
        sticks[i].dir.vz = 0;
        sticks[i].srcZ = -1;
        sticks[i].srcX = -1;
    }
    func_80036520((VECTOR32 *)(pad + 0xC), 0, (VECTOR32 *)&dirs[2].unk08);
    if (joy->unk030 == 1) {
        dirs[3].unk08 = 0x1000;
    } else if (joy->unk030 == 2) {
        dirs[3].unk08 = -0x1000;
    } else {
        dirs[3].unk08 = 0;
    }
    dirs[3].unk0C = 0;
    dirs[3].unk10 = 0;
    if (pad[2]) {
        func_80036520((VECTOR32 *)(pad + 0x1C), 1, (VECTOR32 *)&dirs[1].unk08);
        func_80036520((VECTOR32 *)(pad + 0x2C), 1, (VECTOR32 *)&dirs[0].unk08);
    } else {
        dirs[1].unk08 = 0;
        dirs[1].unk0C = 0;
        dirs[1].unk10 = 0;
        dirs[0].unk08 = 0;
        dirs[0].unk0C = 0;
        dirs[0].unk10 = 0;
    }
    func_80036768(joy);
    func_80036848(joy);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            xbit = JoyBitMask((u8 *)&dirs[i].unk00, j);
            zbit = JoyBitMask((u8 *)&dirs[i].unk04, j);
            both = 0;
            if (xbit && zbit && (dirs[i].unk08 != 0 || dirs[i].unk10 != 0)) {
                both = 1;
            }
            if (both || (xbit && dirs[i].unk08 != 0)) {
                sticks[j].dir.vx = dirs[i].unk08;
                sticks[j].srcX = i;
            }
            if (both || (zbit && dirs[i].unk10 != 0)) {
                sticks[j].dir.vz = dirs[i].unk10;
                sticks[j].srcZ = i;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        EGSMath_VectorModulus(&sticks[i].dir, &sticks[i].len);
        if (sticks[i].len != 0) {
            GTEMath_VectorNormal(&sticks[i].dir, &sticks[i].unit);
        } else {
            sticks[i].unit.vx = 0;
            sticks[i].unit.vy = 0;
            sticks[i].unit.vz = 0;
        }
        if (sticks[i].len > 0x1000) {
            sticks[i].dir = sticks[i].unit;
        }
    }
    ((void (*)(JoyController *))func_8003699C)(joy);
    func_800369A4((u8 *)joy);
    func_80038720(joy);
    func_80036D24(joy);
    func_800374E8(joy);
    func_800389E0(joy);
}

/* Turn the joystick character towards its movement direction (returned in out). */
void JoystickGobjTurn(VECTOR32 *out) {
    Gobj *gobj = gameObjectsInfo[gCASJoystickGobjID].runtime;
    JoyTurnMove *cm;
    VECTOR32 vel;
    VECTOR32 angle;
    VECTOR32 fwd;
    s32 yaw;
    s32 target;
    s32 step;
    s32 remain;
    s32 speed;
    s32 diff;

    if (gobj == NULL || gobj->move == NULL || JOY_TURNMOVE(gobj) == NULL) {
        return;
    }
    cm = JOY_TURNMOVE(gobj);
    if (cm->dir.vx != 0 || cm->dir.vy != 0 || cm->dir.vz != 0) {
        *out = cm->dir;
    } else {
        *out = cm->vel;
    }
    if (gobj->cas->flags & 0xC000000) {
        cm->turnSpeedMoving = 0xC00;
    } else {
        cm->turnSpeedMoving = 0x400;
    }
    cm->turnSpeedStill = 0xC00;
    cm->turnSpeedMoving = 0xC00;
    cm->turnSpeedStill = 0xC00;
    cm->turnSpeedMoving = 0xC00;
    vel = gobj->move->unkA0;
    angle = gobj->move->rot;
    angle.vx += JOY_FIX_TO_INT(vel.vx);
    angle.vy += JOY_FIX_TO_INT(vel.vy);
    angle.vz += JOY_FIX_TO_INT(vel.vz);
    JoyAngleToForward(&angle, &fwd);
    {
        VECTOR32 push = D_800114C8;

        if (out->vx != 0 || out->vy != 0 || out->vz != 0) {
            yaw = angle.vy;
            JoyArctan(out->vx, out->vz, &target);
            if (cm->type == 5 || cm->type == 8 || cm->type == 9 || cm->type == 6) {
                speed = cm->turnSpeedStill;
            } else {
                speed = cm->turnSpeedMoving;
            }
            EGSGeometry_AngleInterpolate(yaw, target, speed, 1, &step, &remain);
            diff = step + remain;
            if (gobj->cas->active == 9) {
                if (JOY_ABS(diff) >= 0x78F) {
                    step = 0;
                }
            } else if (JOY_ABS(diff) >= 0x78F && JoystickGobjIsInPureStrafe(gobj)) {
                if ((D_801056F8 == 1 && diff < 0) || (D_801056F8 == 2 && diff > 0)) {
                    step = -step;
                }
            }
            step <<= 12;
            if (JoyGobjIsStill(gobj)) {
                push.vy = (vel.vy * 6 + step * 10) / 22;
            } else if (D_801056F8 == 0 && (GameCameraCheckMode(0) || GameCameraCheckMode(1) || GameCameraCheckMode(2) ||
                                           GameCameraCheckMode(3) || GameCameraCheckMode(8))) {
                push.vy = (vel.vy * 8 + step * 4) / 20;
            } else {
                push.vy = (vel.vy * 8 + step * 4) / 32;
            }
            gobj->move->unkF0.vx += push.vx;
            gobj->move->unkF0.vy += push.vy;
            gobj->move->unkF0.vz += push.vz;
        }
    }
    if (cm->dir.vx != 0 || cm->dir.vy != 0 || cm->dir.vz != 0) {
        *out = cm->dir;
    } else {
        *out = cm->vel;
    }
}

#ifdef NON_MATCHING
/* last test: the original branches on bit 11 with 1 in the delay slot instead of sltiu */
s32 func_80039B24(JoyController *joy) {
    u8 *pad;
    s32 dir = 0;
    u32 i;

    EGSDeviceControllerRead(&pad, 0);
    if (D_8010F84C[1] != *(u16 *)(pad + 4)) {
        if (JoyTestBit(pad + 4, 0)) {
            dir = -1;
        } else if (JoyTestBit(pad + 4, 1)) {
            dir = 1;
        }
        D_8010F84C[1] = *(u16 *)(pad + 4);
    }
    for (i = 0; i < JOY_MAPPED_BUTTONS; i++) {
        if (gJoyMap[i] == 8) {
            break;
        }
    }
    InterfaceWeaponSelect((i < JOY_MAPPED_BUTTONS && JoyBitMask(pad + 4, i)) ? !JoyBitMask(pad + 4, 0xB) : 0, dir);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80039B24);
#endif

void JoystickGobjTeleport(Gobj *gobj, s32 x, s32 y, s32 z) {
    if (gobj == gameObjectsInfo[gCASJoystickGobjID].runtime) {
        MovementGobjTeleport(gobj, x, y, z);
        GameCameraRequestGetBehind();
        JoystickDirInputDeactivate(5, 0);
    }
}

void JoystickGobjTeleport_ByXZLookAtPoint(Gobj *gobj, s32 x, s32 z, s32 arg3) {
    if (gobj == gameObjectsInfo[gCASJoystickGobjID].runtime) {
        MovementGobjTeleport_ByXZLookAtPoint(gobj, x, z, arg3);
        GameCameraRequestGetBehind();
        JoystickDirInputDeactivate(5, 0);
    }
}

void JoystickSetDirectionalInputType(s32 dir, s32 type) {
    D_80105700[dir].type = type;
}

u8 JoystickGetFlightControls(void) {
    return D_80105818;
}

void JoystickSetFlightControls(u8 enable) {
    D_80105818 = enable;
}

/* (the texture names that followed in .rdata are sprite.c's) */
