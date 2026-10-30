#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/path", PathSetupEX);

INCLUDE_ASM("asm/main/nonmatchings/path", PathGetVelocity);

INCLUDE_ASM("asm/main/nonmatchings/path", PathGetNumPoints);

INCLUDE_ASM("asm/main/nonmatchings/path", PathGetPoint);

INCLUDE_ASM("asm/main/nonmatchings/path", PathSetPositionToPoint);

INCLUDE_ASM("asm/main/nonmatchings/path", PathObjectIsAtPoint);

INCLUDE_ASM("asm/main/nonmatchings/path", PathObjectIsAtClosestPoint);

INCLUDE_ASM("asm/main/nonmatchings/path", PathHandleEx);

INCLUDE_ASM("asm/main/nonmatchings/path", PathHandleOneWay);

INCLUDE_RODATA("asm/main/nonmatchings/path", D_800102C4);
