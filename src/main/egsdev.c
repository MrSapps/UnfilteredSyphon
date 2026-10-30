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
SDATA_LOCAL(D_80110200)
SDATA_LOCAL(D_80110201)
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

/* Periodic vsync callback, run from ServiceCallbacks (VSync IRQ). */
typedef struct Callback {
    /* 0x0 */ void (*func)(struct Callback *);
    /* 0x4 */ s32 period; /* CALLBACK_FREE when the slot is unused */
    /* 0x8 */ s32 counter;
    /* 0xC */ struct Callback *next;
} Callback;

typedef struct EGSRect {
    s16 x, y, w, h;
} EGSRect;

typedef struct EGSPoint {
    s16 x, y;
} EGSPoint;

/* SPRT used by the crash screen */
typedef struct EmergencySprite {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s16 x;
    /* 0x0A */ s16 y;
    /* 0x0C */ u8 u;
    /* 0x0D */ u8 v;
    /* 0x0E */ u16 clut;
    /* 0x10 */ s16 w;
    /* 0x12 */ s16 h;
} EmergencySprite;

/* Per-port pad state (0x3C bytes); PadInitDirect writes the raw buffers. */
typedef struct Controller {
    /* 0x00 */ u8 status;
    /* 0x01 */ u8 unk01[0x3B];
} Controller;

extern s32 D_8010FEC0;          /* colour mode */
extern u8 D_8010FEC4;           /* system clock running */
extern s32 D_8010FEC8;          /* tick at the last fps sample */
extern s32 D_8010FECC;          /* frames per second */
extern s32 D_8010FED0;          /* frames since the last sample */
extern s32 D_8010FED8;          /* frame time count */
extern u8 D_8010FEDC;           /* callback slots initialised */
extern u8 D_80110200[];        /* vibration motor levels */
extern u8 D_80110201;
extern u8 D_80110204;           /* controllers started */
extern s16 D_80110208;          /* screen width */
extern s16 D_8011020C;          /* screen height */
extern volatile s32 D_80110214; /* system tick, counted in the VSync IRQ */
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
extern Callback *D_80110218;    /* active callbacks */
extern EGSPoint POSITION;       /* screen centre */
extern char D_8010FEF8[];       /* "%08X" */
extern u8 D_80107B30[10][2];    /* font uv of 0-9 */
extern u8 D_80107B44[6][2];     /* font uv of A-F */
extern s32 GsDRAWENV;
extern char D_8001375C[];       /* "s/egsdev.c" */

void ServiceCallbacks(void);
Callback *FindFreeCallback(void);
s32 EGSDeviceGraphicsClearScreen(u8 *rgb);
void func_800D1DF0(s32 i);
void GsInitGraph(u16 w, u16 h, s32 mode, s32 dither, s32 vram);
void GsInitGraph2(u16 w, u16 h, s32 mode, s32 dither, s32 vram);
void GsDefDispBuff(s32 x0, s32 y0, s32 x1, s32 y1);
void GsInit3D(void);
void VSync(s32 mode);
void ClearImage(EGSRect *rect, u8 r, u8 g, u8 b);
void EGSDisplayDraw(void);

void DrawPrim(void *p);


s32 EGSDeviceGraphicsReInit(s32 w, s32 h, u32 mode);
void PadStopCom(void);
void PadStartCom(void);
void PadInitDirect(u8 *a, u8 *b);
s32 GetVideoMode(void);
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
            gCallbackArray[i].period = CALLBACK_FREE;
        }
        i = 0;
    }
    cb = gCallbackArray;
    do {
        if (cb->period == CALLBACK_FREE) {
            cb->period = 0;
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

#ifdef NON_MATCHING
/* tick reload is scheduled above the frame-time store here */
void EGSDeviceRender(void) {
    u32 ticks;

    D_8010FED0++;
    gGlobalFrameCounter++;
    if (++D_8010FED8 == 100) {
        D_8010FED8 = 0;
    }
    D_801228A8[D_8010FED8] = D_80110214;
    ticks = D_80110214 - D_8010FEC8;
    if (GetVideoMode() != 0 ? ticks >= 50 : ticks >= 60) {
        s32 frames = D_8010FED0;

        D_8010FED0 = 0;
        D_8010FEC8 = D_80110214;
        D_8010FECC = frames;
    }
    EGSDisplayDraw();
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceRender);
#endif

void EGSDeviceSetVideoType(s16 mode) {
    SetVideoMode(mode);
}

s16 EGSDeviceGetVideoType(void) {
    return GetVideoMode();
}

s32 EGSDeviceGraphicsInit(s32 w, s32 h, s32 mode) {
    if (w > 0 && h > 0 && (u32)mode < 2) {
        GsInitGraph((u16)w, (u16)h, 4, 1, mode == 1);
        GsDefDispBuff(0, 0, 0, (u16)h);
        GsInit3D();
        D_80110208 = w;
        D_8011020C = h;
        D_8010FEC0 = mode;
        EGSDeviceGraphicsClearScreen(NULL);
        return 0;
    }
    return 1;
}

s32 EGSDeviceGraphicsReInit(s32 w, s32 h, u32 mode) {
    if (w <= 0) {
        w = D_80110208;
    }
    if (h <= 0) {
        h = D_8011020C;
    }
    if (mode >= 2) {
        mode = D_8010FEC0;
    }
    GsInitGraph2((u16)w, (u16)h, 4, 1, mode == 1);
    GsDefDispBuff(0, 0, 0, (u16)h);
    POSITION.x = w >> 1;
    POSITION.y = h >> 1;
    D_80110208 = w;
    D_8011020C = h;
    D_8010FEC0 = mode;
    EGSDeviceGraphicsClearScreen(NULL);
    return 0;
}

s32 EGSDeviceGraphicsClearScreen(u8 *rgb) {
    EGSRect rect;

    rect.x = 0;
    rect.y = 0;
    if (D_8010FEC0 == 0) {
        rect.w = D_80110208;
    } else if (D_8010FEC0 == 1) {
        rect.w = D_80110208 * 3 / 2;
    } else {
        return 1;
    }
    rect.h = D_8011020C * 2;
    VSync(0);
    ResetGraph(1);
    if (rgb == NULL) {
        ClearImage(&rect, 0, 0, 0);
    } else {
        ClearImage(&rect, rgb[0], rgb[1], rgb[2]);
    }
    return 0;
}

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

s32 EGSDeviceControllerVibrate(s32 cmd, s32 value, s32 ticks) {
    switch (cmd) {
    case 0:
        if (D_80110204) {
            if ((u8)value) {
                D_80110200[cmd] = 1;
            } else {
                D_80110200[cmd] = 0;
            }
            EGSDisplayQueueInterrupt(func_800D1DF0, (u8)ticks, cmd);
        }
        break;
    case 1:
        if (D_80110204) {
            D_80110200[cmd] = value;
            EGSDisplayQueueInterrupt(func_800D1DF0, (u8)ticks, cmd);
        }
        break;
    case 3:
        D_80110204 = 1;
        break;
    case 4:
        D_80110204 = 0;
    case 2:
        D_80110200[0] = 0;
        D_80110201 = 0;
        break;
    }
    return D_80110204 ? 7 : 6;
}

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

#ifdef NON_MATCHING
/* original indexes buf[i] each pass; GCC here strength-reduces to a pointer */
/* Print `digits` hex digits of n with the debug font sprite (crash screen). */
void EmergencyPrintNum(EmergencySprite *sprite, u32 n, s32 digits) {
    char buf[24];
    s32 i;
    u8 c;

    sprintf(buf, D_8010FEF8, n);
    for (i = 0; i < digits; i++) {
        c = buf[i];
        if ((u32)(c - '0') < 10) {
            sprite->u = D_80107B30[c - '0'][0];
            sprite->v = D_80107B30[c - '0'][1];
        } else {
            sprite->u = D_80107B44[c - 'A'][0];
            sprite->v = D_80107B44[c - 'A'][1];
        }
        DrawPrim(sprite);
        DrawSync(0);
        sprite->x += 8;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdev", EmergencyPrintNum);
#endif

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

s32 EGSDeviceCounterSetPeriodicCallback(void (*func)(Callback *), s32 period, Callback **out) {
    u8 lock = D_8011021C;
    Callback *cb;

    if (func == NULL || period == 0) {
        return 1;
    }
    D_8011021C = 1;
    cb = FindFreeCallback();
    if (cb == NULL || cb == (Callback *)-1) {
        D_8011021C = lock;
        return 3;
    }
    cb->func = func;
    cb->period = period;
    cb->counter = period;
    cb->next = D_80110218;
    D_80110218 = cb;
    *out = cb;
    D_8011021C = lock;
    return 0;
}

s32 EGSDeviceCounterUnsetPeriodicCallback(Callback *cb) {
    Callback *cur;
    Callback *prev;
    u8 lock;

    if (cb == NULL) {
        return 1;
    }
    cur = D_80110218;
    lock = D_8011021C;
    D_8011021C = 1;
    prev = NULL;
    while (cur != NULL && cur != cb) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        D_8011021C = lock;
        return 1;
    }
    if (prev == NULL) {
        D_80110218 = cur->next;
    } else {
        prev->next = cur->next;
    }
    cur->period = CALLBACK_FREE;
    D_8011021C = lock;
    return 0;
}

s32 EGSDeviceGetFPS(s32 *out) {
    *out = D_8010FECC;
    return 0;
}

#ifdef NON_MATCHING
/* uses one more saved register */
s32 EGSDeviceFrameRateLastNFrames(s16 frames) {
    s32 then = D_8010FED8 - frames;
    s32 now = D_801228A8[D_8010FED8];
    s32 hz = 60;
    u32 fps;

    if (GetVideoMode() != 0) {
        hz = 50;
    }
    if (then < 0) {
        then += 100;
    }
    fps = (u32)(frames * hz) / (u32)(now - D_801228A8[then]);
    if (fps > (u32)(hz >> 1)) {
        fps = hz >> 1;
    }
    return fps;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdev", EGSDeviceFrameRateLastNFrames);
#endif

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
