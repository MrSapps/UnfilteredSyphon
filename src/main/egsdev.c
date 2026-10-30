#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsdev", FindFreeCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceRender);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceSetVideoType);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGetVideoType);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsReInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsClearScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsSetColorMode);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsGetColorMode);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsSetResolution);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsGetResolution);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsReset);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", func_800D133C);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerPoll);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerRead);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerGetStatus);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", func_800D1DF0);

INCLUDE_RODATA("asm/main/nonmatchings/egsdev", D_8001375C);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerVibrate);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerStop);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceUnhang);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EmergencyPrintNum);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", ServiceCallbacks);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterInitSystemClock);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterGetSystemTick);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterSetSystemTick);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterStopSystemClock);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterStartSystemClock);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterSetPeriodicCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterUnsetPeriodicCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGetFPS);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceFrameRateLastNFrames);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGetFrameCountInfo);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGetFrameCounter);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsGetDrawingEnvironment);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCdInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCdIsStreaming);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDevicePlatformInit);
