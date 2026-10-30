#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsdebug", EGSDebugInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdebug", EGSDebugPrinter);

INCLUDE_ASM("asm/main/nonmatchings/egsdebug", EGSDebugAssertCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdebug", DoAssertPrint);

INCLUDE_RODATA("asm/main/nonmatchings/egsdebug", D_800137FC);

INCLUDE_ASM("asm/main/nonmatchings/egsdebug", EGSDebugAssert);
