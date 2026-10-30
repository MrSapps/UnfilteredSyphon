#include "common.h"
#include "gameobj.h"

typedef struct ReactionEntry {
    /* 0x0 */ u8 key;
    /* 0x1 */ u8 pad[3];
    /* 0x4 */ u32 mask;          /* 0: any */
    /* 0x8 */ u32 requiredFlags; /* 0: any, else tested against *rt->flagsPtr */
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
            (e->requiredFlags == 0 || (e->requiredFlags & *rt->flagsPtr))) {
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

INCLUDE_ASM("asm/main/nonmatchings/joystick", iJoystickRelTimerCB);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035434);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035480);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_8003551C);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035584);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800355EC);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035688);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80035724);

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickInit);

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystick);

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickSetMode);

INCLUDE_ASM("asm/main/nonmatchings/joystick", GameSysJoystickGetMode);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickDeactivate);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickActivate);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickDirInputDeactivate);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickIsDirInputDeactivated);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickDeactivateMenu);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjIsWalkKeyPressed);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjIsInPureStrafe);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjIsInPureNonRollingStrafe);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjIsInFullRetreat);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjIsInFastWalkBack);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjShouldCollideWithEdges);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_8003642C);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036520);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036768);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114B8);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114C8);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_800114D8);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036848);

void func_8003699C(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800369A4);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800369AC);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80036D24);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800374E8);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80038720);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_800389E0);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_8003915C);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjTurn);

INCLUDE_ASM("asm/main/nonmatchings/joystick", func_80039B24);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjTeleport);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGobjTeleport_ByXZLookAtPoint);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickSetDirectionalInputType);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickGetFlightControls);

INCLUDE_ASM("asm/main/nonmatchings/joystick", JoystickSetFlightControls);

INCLUDE_RODATA("asm/main/nonmatchings/joystick", D_8001151C);
