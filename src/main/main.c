#include "common.h"

void ResetCallback(void);
void EGSDeviceSetVideoType(s32 type);
void MainLoop(void);

/* cc1 inserts the __main() constructor call itself */
int main(void) {
    ResetCallback();
    EGSDeviceSetVideoType(0);
    MainLoop();
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/main", MainLoop);

INCLUDE_RODATA("asm/main/nonmatchings/main", jtbl_80010000);
