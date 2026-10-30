#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsanim", FindMasterAnim);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimSimpleCreate);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimCreateMasterAnim);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimGetBox);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimGetFrame);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimInitBox);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimShowDetailDef);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimHideDetailDef);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimSetBox);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", InitMasters);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimGetTransformedZ);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimHMDCreate);

INCLUDE_ASM("asm/main/nonmatchings/egsanim", EGSAnimHMDLinkModel);

INCLUDE_RODATA("asm/main/nonmatchings/egsanim", gEGSMathIdentityMatrix);
