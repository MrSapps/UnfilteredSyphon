#include "common.h"
#include "game.h"

/* egsdev.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FEC0)
SDATA_LOCAL(D_8010FEC4)
SDATA_LOCAL(D_8010FEC8)
SDATA_LOCAL(D_8010FECC)
SDATA_LOCAL(D_8010FED0)
SDATA_LOCAL(D_8010FED8)
SDATA_LOCAL(D_8010FEDC)
SDATA_LOCAL(D_80110204)
SDATA_LOCAL(D_80110208)
SDATA_LOCAL(D_8011020C)
SDATA_LOCAL(D_80110214)
SDATA_LOCAL(D_80110218)
SDATA_LOCAL(D_8011021C)
SDATA_LOCAL(HANG)
SDATA_LOCAL(HANGpc)
SDATA_LOCAL(HANGra)
SDATA_LOCAL(HANGsp)
SDATA_LOCAL(gGlobalFrameCounter)
SDATA_LOCAL(lastCounterTick)
SDATA_LOCAL(lastCounterTickSort)

#define CALLBACK_FREE 0xCACACACA

typedef struct Callback {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ void *func; /* CALLBACK_FREE when the slot is unused */
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} Callback;

/* Per-port pad state (0x3C bytes); PadInitDirect writes the raw buffers. */
typedef struct Controller {
    /* 0x00 */ u8 status;
    /* 0x01 */ u8 unk01[0x3B];
} Controller;

extern s32 D_8010FEC0;          /* colour mode */
extern u8 D_8010FEC4;           /* system clock running */
extern s32 D_8010FECC;          /* frames per second */
extern s32 D_8010FED8;          /* frame time count */
extern u8 D_8010FEDC;           /* callback slots initialised */
extern u8 D_80110200[];
extern u8 D_80110204;           /* controllers started */
extern s16 D_80110208;          /* screen width */
extern s16 D_8011020C;          /* screen height */
extern s32 D_80110214;          /* system tick */
extern s32 D_80110218;
extern u8 D_8011021C;
extern u8 HANG;
extern s32 HANGpc;
extern s32 HANGra;
extern s32 HANGsp;
extern s32 gGlobalFrameCounter;
extern s32 lastCounterTick;
extern s32 lastCounterTickSort;
extern u8 gIsStreamingFlag;
extern s32 D_801228A8[100];     /* frame times */
extern u8 D_80122864[];         /* pad buffers */
extern Controller gControllers[8];
extern Callback gCallbackArray[5];
extern s32 GsDRAWENV;
extern char D_8001375C[];       /* "s/egsdev.c" */

void ServiceCallbacks(void);
s32 EGSDeviceGraphicsReInit(s32 w, s32 h, s32 mode);
void PadStopCom(void);
void PadStartCom(void);
void PadInitDirect(u8 *a, u8 *b);
s16 GetVideoMode(void);
void SetVideoMode(s16 mode);
void CdInit(void);
void CdControl(s32 com, s32 param, s32 result);
void VSyncCallback(void (*fn)(void));
void ResetGraph();
void func_80139680(s32 arg0); /* movie.ovl (libcard) */

/* Hand out a free periodic-callback slot (marked with CALLBACK_FREE). */
#ifdef NON_MATCHING
/* 5 insns short: search loop keeps separate slot and func pointers in the original */
/* global until EGSDeviceCounterSetPeriodicCallback is C */
Callback *FindFreeCallback(void) {
    Callback *cb;
    s32 i = 0;

    if (!D_8010FEDC) {
        D_8010FEDC = 1;
        for (i = 4; i >= 0; i--) {
            gCallbackArray[i].func = (void *)CALLBACK_FREE;
        }
        i = 0;
    }
    cb = gCallbackArray;
    do {
        if (cb->func == (void *)CALLBACK_FREE) {
            cb->func = NULL;
            return cb;
        }
        cb++;
        i++;
    } while (i < 5);
    return NULL;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdev", FindFreeCallback);
#endif

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceRender);

void EGSDeviceSetVideoType(s16 mode) {
    SetVideoMode(mode);
}

s16 EGSDeviceGetVideoType(void) {
    return GetVideoMode();
}

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsReInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceGraphicsClearScreen);

void EGSDeviceGraphicsSetColorMode(s32 mode) {
    EGSDeviceGraphicsReInit(D_80110208, D_8011020C, mode);
}

s32 EGSDeviceGraphicsGetColorMode(s32 *out) {
    *out = D_8010FEC0;
    return 0;
}

void EGSDeviceGraphicsSetResolution(s32 w, s32 h) {
    EGSDeviceGraphicsReInit(w, h, D_8010FEC0);
}

s32 EGSDeviceGraphicsGetResolution(u16 *w, u16 *h) {
    *w = D_80110208;
    *h = D_8011020C;
    return 0;
}

s32 EGSDeviceGraphicsReset(s32 mode) {
    if (mode >= 4 || mode == 2) {
        return 1;
    }
    ResetGraph();
    return 0;
}

s32 EGSDeviceControllerInit(void) {
    func_80139680(0);
    PadInitDirect(D_80122864, D_80122864 + 0x22);
    PadStartCom();
    D_80110204 = 1;
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/egsdev", func_800D133C);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerPoll);

s32 EGSDeviceControllerRead(Controller **out, u32 port) {
    if (port < 8) {
        *out = &gControllers[port];
        return 0;
    }
    EGSDebugAssert(1, 0, D_8001375C, 0x2D5);
    return 1;
}

s32 EGSDeviceControllerGetStatus(u32 port, u8 *out) {
    if (port < 8) {
        *out = gControllers[port].status;
        return 0;
    }
    EGSDebugAssert(1, 0, D_8001375C, 0x2EF);
    return 1;
}

void func_800D1DF0(s32 i) {
    D_80110200[i] = 0;
}

INCLUDE_RODATA("asm/main/nonmatchings/egsdev", D_8001375C);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceControllerVibrate);

s32 EGSDeviceControllerStop(void) {
    PadStopCom();
    return 0;
}

void EGSDeviceUnhang(void) {
    HANG = 0;
    HANGra = 0;
    HANGsp = 0;
    HANGpc = 0;
    lastCounterTickSort = lastCounterTick;
}

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EmergencyPrintNum);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", ServiceCallbacks);

s32 EGSDeviceCounterInitSystemClock(void) {
    D_80110214 = 0;
    D_80110218 = 0;
    D_8011021C = 0;
    VSyncCallback(ServiceCallbacks);
    return 0;
}

s32 EGSDeviceCounterGetSystemTick(s32 *out) {
    *out = D_80110214;
    return 0;
}

s32 EGSDeviceCounterSetSystemTick(s32 tick) {
    D_80110214 = tick;
    return 0;
}

s32 EGSDeviceCounterStopSystemClock(void) {
    D_8010FEC4 = 0;
    return 0;
}

s32 EGSDeviceCounterStartSystemClock(void) {
    D_8010FEC4 = 1;
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterSetPeriodicCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceCounterUnsetPeriodicCallback);

s32 EGSDeviceGetFPS(s32 *out) {
    *out = D_8010FECC;
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceFrameRateLastNFrames);

s32 EGSDeviceGetFrameCountInfo(s32 *count, s32 *max, s32 **times) {
    *count = D_8010FED8;
    *max = 100;
    *times = D_801228A8;
    return 1;
}

s32 EGSDeviceGetFrameCounter(s32 *out) {
    *out = gGlobalFrameCounter;
    return 0;
}

s32 EGSDeviceGraphicsGetDrawingEnvironment(void **out) {
    *out = &GsDRAWENV;
    return 0;
}

s32 EGSDeviceCdInit(void) {
    CdInit();
    CdControl(0xE, 0x80, 0);
    return 0;
}

s32 EGSDeviceCdIsStreaming(u8 *out) {
    *out = gIsStreamingFlag;
    return 0;
}

s32 EGSDevicePlatformInit(void) {
    return 0;
}
