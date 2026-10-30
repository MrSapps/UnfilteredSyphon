#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemInit);

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemRegisterVariableOverlayEnd);

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemAvail);

void EGSMemDumpHeap(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemMalloc);

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemFree);

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemSetHeap);

INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemGetHeap);

INCLUDE_RODATA("asm/main/nonmatchings/egsmem", D_80013848);
