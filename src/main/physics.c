#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/physics", PhysicsGobjBasicInit);

INCLUDE_ASM("asm/main/nonmatchings/physics", PhysicsGobjInit);

INCLUDE_ASM("asm/main/nonmatchings/physics", PhysicsGobjUpdate);

INCLUDE_ASM("asm/main/nonmatchings/physics", PhysicsGobjRegisterForce);

INCLUDE_ASM("asm/main/nonmatchings/physics", PhysicsApplyFrictionToVelocity);
