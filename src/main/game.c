#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/game", SYSGameInit);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysInitMoveInfo);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysIsTime);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysTableHook);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysFuzzyMathXZDistance);

INCLUDE_ASM("asm/main/nonmatchings/game", func_80016690);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysEOL);

INCLUDE_RODATA("asm/main/nonmatchings/game", D_800100D4);

INCLUDE_RODATA("asm/main/nonmatchings/game", D_800100E4);

INCLUDE_RODATA("asm/main/nonmatchings/game", D_800100F0);

INCLUDE_ASM("asm/main/nonmatchings/game", func_80016764);

void func_80016840(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysGetYRotationFromZ);

void GameSysRestartLevel(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysRequestMove);

INCLUDE_ASM("asm/main/nonmatchings/game", func_800168FC);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysProcessMoveRequests);

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysClearMoveRequests);

INCLUDE_ASM("asm/main/nonmatchings/game", GameGetGobj);

INCLUDE_ASM("asm/main/nonmatchings/game", GameFreeGobj);

void func_80016C58(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/game", GameSysSetLetterBoxMode);
