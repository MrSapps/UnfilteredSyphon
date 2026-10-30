#include "common.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

SDATA_LOCAL(D_8010F84C)

#define JOY_BUTTONS 16
#define JOY_MAPPED_BUTTONS 9 /* buttons remapped through gJoyMap, the rest are cleared */

/* Per mode, per button action (mJoyBtnInfo[mode][button], 0x1C bytes). */
typedef struct JoyBtnInfo {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 action; /* CAS action requested on release */
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 pad09[3];
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
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
    /* 0x028 */ u8 unk028[0x17C];
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

/* Per-object joystick controller state. */
typedef struct JoyController {
    /* 0x000 */ u8 unk000[0x30];
    /* 0x030 */ s32 unk030;
    /* 0x034 */ u8 unk034[4];
    /* 0x038 */ JoyDirInfo *dirs;
    /* 0x03C */ u8 unk03C[0xF0];
    /* 0x12C */ struct JoyDirDefault *dirDefaults; /* 4 per set, indexed by JoyDirInfo.type */
    /* 0x130 */ u8 unk130[0xC];
    /* 0x13C */ s32 unk13C;
    /* 0x140 */ u8 dirActive;
} JoyController;

typedef struct JoyDirDefault {
    /* 0x0 */ s32 unk00;
    /* 0x4 */ s32 unk04;
} JoyDirDefault;

s32 GameCameraGetMode(void);
void EGSDeviceControllerRead(u8 **pad, s32 port);
void InterfaceWeaponSelect(s32 select, s32 dir);
extern u16 D_8010F84C[2]; /* [1]: pad buttons last seen by func_80039B24 */

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

extern JoyBtnInfo mJoyBtnInfo[][JOY_BUTTONS];
extern s32 gJoyMap[JOY_MAPPED_BUTTONS];
extern JoyBtnInfo D_8010581C[]; /* default layout, indexed by gJoyMap */
extern JoyBtnInfo D_80105918[];
extern JoyBtnInfo D_80105A14[];
extern s32 gCASJoystickGobjID;
extern s32 D_801056CC;          /* joystick mode */
extern JoyBtnInfo *D_801056D4;  /* mJoyBtnInfo[mode] */
extern u8 D_801056D8;           /* joystick deactivated */
extern u8 D_801056D9;           /* menu deactivated */
extern u8 D_801056E0;           /* walk key pressed */
extern s32 D_801056F8;
extern JoyDirInfo *D_80105700;
extern u32 D_801057FC;          /* retreat level */
extern s32 D_80105804;          /* directional input deactivated */
extern u8 D_80105808;
extern u8 D_80105818;           /* flight controls */

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
        btn->unk10 = 0;
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

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickInit);

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystick);

#ifdef NON_MATCHING
/* mode load/store: the original keeps &D_801056CC in a register and stores mode before computing mJoyBtnInfo[mode] */
s32 GameSysJoystickSetMode(s32 mode) {
    s32 i;
    s32 old;

    i = 0;
    old = D_801056CC;
    D_801056CC = mode;
    D_801056D4 = mJoyBtnInfo[mode];
    for (; i < JOY_BUTTONS; i++) {
        *(s32 *)((u8 *)D_801056D4 + i * sizeof(JoyBtnInfo) + 0x14) = mJoyBtnInfo[old][i].unk14;
    }
    return old;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickSetMode);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036520);

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

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114B8);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114C8);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114D8);

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

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800369AC);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036D24);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800374E8);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80038720);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800389E0);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_8003915C);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjTurn);

#ifdef NON_MATCHING
/* last test: the original branches on bit 11 with 1 in the delay slot instead of sltiu */
s32 func_80039B24(void) {
    u8 *pad;
    s32 dir = 0;
    u32 i;
    s32 sel;

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
    InterfaceWeaponSelect(i < JOY_MAPPED_BUTTONS && JoyTestBit(pad + 4, i) ? (JoyTestBit(pad + 4, 0xB) ? 0 : 1) : 0, dir);
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

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_8001151C);
