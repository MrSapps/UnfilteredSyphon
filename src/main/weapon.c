#include "common.h"

INCLUDE_RODATA("asm/main/nonmatchings/weapon", D_80011E8C);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponInit);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponUpdate);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponAdd);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponRemove);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSpecialUse);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponHasMaxAmmo);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponAddAmmo);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetTip);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetPreviousID);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetNextID);

INCLUDE_ASM("asm/main/nonmatchings/weapon", iWeaponAllocateAnim);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponReplace);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSelect);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponDrop);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponReload);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponPlayReloadSndCASAnimCB);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponDoFiredCallback);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSubRound);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetRounds);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetDeathAction);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSetWeaponFiredCallback);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSetTaserHitNpcCallback);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponTaser);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetSaveInfo);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponRestoreSaveInfo);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponUnlimitedAmmo);

void WeaponTorchCASAnimCB(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponMakeRoundsString);
