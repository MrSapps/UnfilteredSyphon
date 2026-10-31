#include "common.h"
#include "game.h"


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

typedef struct PadAxis {
    s32 vx, vy, vz, pad;
} PadAxis;

/* Per-port pad state (0x3C bytes); PadInitDirect writes the raw buffers. */
typedef struct Controller {
    /* 0x00 */ u8 status;
    /* 0x01 */ u8 present; /* pad id byte (0x41 digital, 0x73 analog...), 0 when absent */
    /* 0x02 */ u8 analog;
    /* 0x03 */ u8 unk03;
    /* 0x04 */ u16 buttons; /* active high */
    /* 0x06 */ u8 rx;
    /* 0x07 */ u8 ry;
    /* 0x08 */ u8 lx;
    /* 0x09 */ u8 ly;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ PadAxis axis[3]; /* d-pad, left stick, right stick (-0x7F..0x7F) */
} Controller;

#define PAD_UP 12
#define PAD_RIGHT 13
#define PAD_DOWN 14
#define PAD_LEFT 15
#define PAD_SELECT 8
#define PAD_START 11

/*
 * .sbss/.bss: every item is 4-aligned and 4-padded. The original's assembler
 * word-aligned every .lcomm; aspsx 2.77 (and maspsx) align by size, so the small
 * ones are padded to a word here and the .bss is laid out as one block (below).
 */
static u8 D_80110200[4];        /* vibration motor levels ([2] used) */
static u8 D_80110204[4];        /* [0]: controllers started */
#define D_80110204 (D_80110204[0])
static s16 D_80110208[2];       /* [0]: screen width */
#define D_80110208 (D_80110208[0])
static s16 D_8011020C[2];       /* [0]: screen height */
#define D_8011020C (D_8011020C[0])
static s32 D_80110210;          /* unreferenced */
static volatile s32 D_80110214; /* system tick, counted in the VSync IRQ */
static Callback *D_80110218;    /* active callbacks */
static u8 D_8011021C;
extern u8 D_80122678[];         /* .bss block: crash screen DR_MODE, then: */
extern Controller gControllers[8];
extern u8 D_80122864[2][0x22];  /* raw PadInitDirect buffers, one per port */
extern s32 D_801228A8[100];     /* frame times */
/*
 * This TU's data, defined in original order (docs/data_migration.md).
 * .sdata (continued after EmergencyPrintNum: "%8lX", D_8010FF00):
 */
static u8 D_8010FEB8[8] = { 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 }; /* actuator alignment table */
static s32 D_8010FEC0 = 0;      /* colour mode */
static u8 D_8010FEC4 = 1;       /* system clock running */
static s32 D_8010FEC8 = 0;      /* tick at the last fps sample */
static s32 D_8010FECC = 0;      /* frames per second */
static s32 D_8010FED0 = 0;      /* frames since the last sample */
s32 gGlobalFrameCounter = 0;
static s32 D_8010FED8 = 0;      /* frame time count */
static u8 D_8010FEDC = 0;       /* callback slots initialised */
s32 lastCounterTickSort = 0;
s32 lastCounterTick = 0;
s32 HANGpc = 0;
s32 HANGra = 0;
s32 HANGsp = 0;
u8 HANG = 0;
u8 HANGwarn = 0;                /* unreferenced */
/* .data */
static u8 D_80107B30[10][2] = { /* font uv of 0-9 */
    { 0x00, 0x00 }, { 0x08, 0x00 }, { 0x10, 0x00 }, { 0x18, 0x00 }, { 0x00, 0x08 },
    { 0x08, 0x08 }, { 0x10, 0x08 }, { 0x18, 0x08 }, { 0x00, 0x10 }, { 0x08, 0x10 },
};
static u8 D_80107B44[6][2] = { /* font uv of A-F */
    { 0x38, 0x10 }, { 0x20, 0x18 }, { 0x28, 0x18 }, { 0x30, 0x18 }, { 0x38, 0x18 }, { 0x20, 0x20 },
};
/* commons and other TUs' data */
extern u8 gIsStreamingFlag;
extern Callback gCallbackArray[5];
extern EGSPoint POSITION;       /* screen centre */
extern s32 GsDRAWENV;

void ServiceCallbacks(void);
void func_800D133C(Controller *c, u8 *buf, s32 port);
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
/* global until EGSDeviceCounterSetPeriodicCallback is C */
Callback *FindFreeCallback(void) {
    s32 i;

    if (!D_8010FEDC) {
        D_8010FEDC = 1;
        for (i = 4; i >= 0; i--) {
            gCallbackArray[i].period = CALLBACK_FREE;
        }
    }
    for (i = 0; i < 5; i++) {
        if (gCallbackArray[i].period == CALLBACK_FREE) {
            gCallbackArray[i].period = 0;
            return &gCallbackArray[i];
        }
    }
    return NULL;
}

/* .bss, laid out by hand: a separate .lcomm per array would be 16-aligned */
#ifdef _PSYQ
__asm__("\t.bss\nD_80122678:\t.space\t12\ngControllers:\t.space\t480\n"
        "D_80122864:\t.space\t68\nD_801228A8:\t.space\t400\n\t.text");
#else
static u8 D_80122678[0x3C0];
__asm__(".set gControllers, D_80122678+0xC\n"
        ".set D_80122864, D_80122678+0x1EC\n"
        ".set D_801228A8, D_80122678+0x230\n");
#endif

void EGSDeviceRender(void) {
    volatile s32 *tick;
    u32 ticks;

    D_8010FED0++;
    gGlobalFrameCounter++;
    if (++D_8010FED8 == 100) {
        D_8010FED8 = 0;
    }
    D_801228A8[D_8010FED8] = D_80110214;
    tick = &D_80110214;
    ticks = *tick - D_8010FEC8;
    if (GetVideoMode() != 0 ? ticks >= 50 : ticks >= 60) {
        s32 now = *tick;
        s32 frames = D_8010FED0;

        D_8010FED0 = 0;
        D_8010FEC8 = now;
        D_8010FECC = frames;
    }
    EGSDisplayDraw();
}

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
    PadInitDirect(D_80122864[0], D_80122864[1]);
    PadStartCom();
    D_80110204 = 1;
    return 0;
}

/* Bit n of a bit array that may start at an unaligned address. */
static inline u8 TestBit(u8 *bits, s32 n) {
    u32 misalign = (u32)bits & 3;
    s32 word;
    s32 bit;

    if (misalign != 0) {
        n += misalign * 8;
        bits = (u8 *)((u32)bits & ~3);
    }
    word = n >> 5;
    bit = 1 << (n & 0x1F);
    return (((u32 *)bits)[word] & bit) != 0;
}

static inline void SetBit(u8 *bits, s32 n) {
    u32 misalign = (u32)bits & 3;
    s32 word;
    s32 bit;

    if (misalign != 0) {
        n += misalign * 8;
        bits = (u8 *)((u32)bits & ~3);
    }
    word = n >> 5;
    bit = 1 << (n & 0x1F);
    ((u32 *)bits)[word] |= bit;
}

static inline void ClearBit(u8 *bits, s32 n) {
    u32 misalign = (u32)bits & 3;
    s32 word;
    s32 bit;

    if (misalign != 0) {
        n += misalign * 8;
        bits = (u8 *)((u32)bits & ~3);
    }
    word = n >> 5;
    bit = 1 << (n & 0x1F);
    ((u32 *)bits)[word] &= ~bit;
}

s32 PadGetState(s32 port);
s32 PadInfoMode(s32 port, s32 term, s32 offs);
s32 PadSetActAlign(s32 port, u8 *table);
void PadSetAct(s32 port, u8 *table, s32 len);

#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define BUTTON_BITS(c) ((u8 *)&(c)->buttons)

/* Decode one raw pad packet into a Controller. */
void func_800D133C(Controller *c, u8 *buf, s32 port) {
    s32 state;
    s32 v;
    s32 i;
    s32 x;
    s32 z;
    s32 az;

    if (buf[0] == 0xFF) {
        c->status = 0xFF;
        c->present = 0;
        return;
    }
    c->status = buf[0];
    c->present = buf[1];
    c->analog = 0;
    c->buttons = ~((buf[2] << 8) | buf[3]);
    c->rx = buf[4];
    c->ry = buf[5];
    c->lx = buf[6];
    c->ly = buf[7];
    state = PadGetState(port);
    if (PadInfoMode(port, 2, 0) != 0 && D_80110204 != 0 && state == 6) {
        if (PadSetActAlign(port, D_8010FEB8) != 0) {
            PadSetAct(port, D_80110200, 2);
        }
    }
    c->axis[0].vx = 0;
    c->axis[0].vy = 0;
    c->axis[0].vz = 0;
    if (TestBit(BUTTON_BITS(c), PAD_UP)) {
        c->axis[0].vz = 0x7F;
    }
    if (TestBit(BUTTON_BITS(c), PAD_DOWN)) {
        c->axis[0].vz -= 0x7F;
    }
    if (TestBit(BUTTON_BITS(c), PAD_RIGHT)) {
        c->axis[0].vx += 0x7F;
    }
    if (TestBit(BUTTON_BITS(c), PAD_LEFT)) {
        c->axis[0].vx -= 0x7F;
    }
    if (TestBit(BUTTON_BITS(c), PAD_UP) && TestBit(BUTTON_BITS(c), PAD_DOWN)) {
        ClearBit(BUTTON_BITS(c), PAD_UP);
        ClearBit(BUTTON_BITS(c), PAD_DOWN);
    }
    if (TestBit(BUTTON_BITS(c), PAD_RIGHT) && TestBit(BUTTON_BITS(c), PAD_LEFT)) {
        ClearBit(BUTTON_BITS(c), PAD_RIGHT);
        ClearBit(BUTTON_BITS(c), PAD_LEFT);
    }
    if (TestBit(BUTTON_BITS(c), PAD_UP) || TestBit(BUTTON_BITS(c), PAD_DOWN)) {
        ClearBit(BUTTON_BITS(c), PAD_RIGHT);
        ClearBit(BUTTON_BITS(c), PAD_LEFT);
    } else if (TestBit(BUTTON_BITS(c), PAD_RIGHT) || TestBit(BUTTON_BITS(c), PAD_LEFT)) {
        ClearBit(BUTTON_BITS(c), PAD_UP);
        ClearBit(BUTTON_BITS(c), PAD_DOWN);
    }
    if (c->present == 0x23 || c->present == 0x53 || c->present == 0x73) {
        c->analog = 1;
        v = c->lx - 0x7F;
        if (v >= 0x80) {
            v = 0x7F;
        }
        c->axis[1].vx = v;
        c->axis[1].vy = 0;
        c->axis[1].vz = (c->ly - 0x7F < 0x80) ? 0x7F - c->ly : -0x7F;
        v = c->rx - 0x7F;
        if (v >= 0x80) {
            v = 0x7F;
        }
        c->axis[2].vx = v;
        c->axis[2].vy = 0;
        c->axis[2].vz = (c->ry - 0x7F < 0x80) ? 0x7F - c->ry : -0x7F;
        if (c->axis[0].vx == 0 && c->axis[0].vy == 0 && c->axis[0].vz == 0) {
            for (i = 1; i < 3; i++) {
                z = c->axis[i].vz;
                az = ABS(z);
                if (az > 0x60 || ABS(c->axis[i].vx) > 0x60) {
                    x = c->axis[i].vx;
                    if (ABS(x) < az) {
                        if (z > 0x60) {
                            SetBit(BUTTON_BITS(c), PAD_UP);
                        } else if (z < -0x60) {
                            SetBit(BUTTON_BITS(c), PAD_DOWN);
                        }
                    } else if (x > 0x60) {
                        SetBit(BUTTON_BITS(c), PAD_RIGHT);
                    } else if (x < -0x60) {
                        SetBit(BUTTON_BITS(c), PAD_LEFT);
                    }
                    break;
                }
            }
        }
    }
}

/* Parse both ports; a multitap (0x80) fills four controller records per port. */
s32 EGSDeviceControllerPoll(void) {
    s32 port;

    for (port = 0; port < 2; port++) {
        if (D_80122864[port][1] == 0x80) {
            func_800D133C(&gControllers[port * 4], &D_80122864[port][2], port);
            func_800D133C(&gControllers[port * 4 + 1], &D_80122864[port][2 + 8], port);
            func_800D133C(&gControllers[port * 4 + 2], &D_80122864[port][2 + 16], port);
            func_800D133C(&gControllers[port * 4 + 3], &D_80122864[port][2 + 24], port);
        } else {
            func_800D133C(&gControllers[port * 4], D_80122864[port], port);
            gControllers[port * 4 + 1].present = 0;
            gControllers[port * 4 + 2].present = 0;
            gControllers[port * 4 + 3].present = 0;
        }
    }
    return 0;
}

s32 EGSDeviceControllerRead(Controller **out, u32 port) {
    if (port < 8) {
        *out = &gControllers[port];
        return 0;
    }
    EGSDebugAssert(1, 0, __FILE__, 0x2D5);
    return 1;
}

s32 EGSDeviceControllerGetStatus(u32 port, u8 *out) {
    if (port < 8) {
        *out = gControllers[port].status;
        return 0;
    }
    EGSDebugAssert(1, 0, __FILE__, 0x2EF);
    return 1;
}

void func_800D1DF0(s32 i) {
    D_80110200[i] = 0;
}

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
        D_80110200[1] = 0;
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
static char D_8010FEF8[5]; /* defined below */
/* v0/v1 swapped for the font-table index and loaded byte */
/* Print `digits` hex digits of n with the debug font sprite (crash screen). */
void EmergencyPrintNum(EmergencySprite *sprite, u32 n, s32 digits) {
    char buf[16];
    s32 i;
    u8 c;

    sprintf(buf, D_8010FEF8, n);
    for (i = 0; i < digits; i++) {
        c = buf[i];
        if (c >= '0' && c <= '9') {
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

/* BIOS thread control block of the interrupted thread (only the saved registers used) */
typedef struct EGSThreadContext {
    /* 0x00 */ u8 unk00[0x7C];
    /* 0x7C */ u32 sp;
    /* 0x80 */ u32 fp;
    /* 0x84 */ u32 ra;
    /* 0x88 */ u32 epc;
} EGSThreadContext;

/* Current thread's context, kept by the BIOS at 0x110 */
#define CURRENT_THREAD_CONTEXT (*(EGSThreadContext **)0x110)

extern int gSystemState;
extern u8 gSysHaltState;
/* .sdata after EmergencyPrintNum's "%8lX" (a static here: the function is still asm) */
static char D_8010FEF8[5] = "%8lX";
static EGSRect D_8010FF00 = { 0, 0, 384, 512 }; /* crash screen clear rect */
/* .data */
static EmergencySprite D_80107B50 = {           /* crash screen digit sprite */
    { 0, 0, 0, 0, 0x80, 0x80, 0x80, 0 }, -170, 80, 8, 0x20, 0, 6, 8
};
void MainLoop(void);
void GsSwapDispBuff(void);
void SetSprt(void *p);
u16 GetClut(s32 x, s32 y);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void SetDrawMode(void *p, s32 dfe, s32 dtd, s32 tpage, void *tw);

/*
 * VSync IRQ handler: counts the system tick and runs the periodic callbacks. If the
 * main loop stops sorting frames for too long it shows a crash screen, and Select or
 * Start then restarts MainLoop.
 */
void ServiceCallbacks(void) {
    EGSThreadContext *tc = CURRENT_THREAD_CONTEXT;
    u32 elapsed;
    EGSRect rect;
    Controller *ctrl;
    Callback *cb;
    Callback *next;
    u32 sp;
    u32 pc;

    elapsed = lastCounterTick - lastCounterTickSort;
    lastCounterTick++;
    if (gSystemState == 2) {
        lastCounterTickSort = lastCounterTick;
    }
    if (!HANG && elapsed > 400 && (tc->epc >> 28) != 1) {
        rect = D_8010FF00;
        if (elapsed < 302) {
            ResetGraph(1);
            GsSwapDispBuff();
            ClearImage(&rect, 20, 20, 40);
            DrawSync(0);
        } else if (elapsed < 304) {
            SetSprt(&D_80107B50);
            D_80107B50.clut = GetClut(0x300, 0x1E3);
            D_80107B50.x = -170;
            D_80107B50.y = 100;
            ResetGraph(1);
            GsSwapDispBuff();
            SetDrawMode(D_80122678, 0, 0, GetTPage(1, 1, 0x340, 0), NULL);
            DrawPrim(D_80122678);
            EmergencyPrintNum(&D_80107B50, tc->epc, 8);
            D_80107B50.x += 20;
            EmergencyPrintNum(&D_80107B50, tc->ra, 8);
            D_80107B50.x += 20;
            EmergencyPrintNum(&D_80107B50, tc->sp, 8);
        }
        EGSDeviceControllerPoll();
        EGSDeviceControllerRead(&ctrl, 0);
        if (TestBit(BUTTON_BITS(ctrl), PAD_SELECT) || TestBit(BUTTON_BITS(ctrl), PAD_START)) {
            sp = tc->sp;
            pc = tc->epc;
            tc->sp = 0x801FFFF8;
            tc->epc = (u32)MainLoop;
            gSysHaltState = 2;
            gSystemState = 2;
            HANG = 1;
            HANGpc = pc;
            HANGra = tc->ra;
            HANGsp = sp;
        }
    }
    if (D_8010FEC4 == 1) {
        D_80110214 += 1;
    }
    cb = D_80110218;
    if (cb != NULL && D_8011021C != 1) {
        D_8011021C = 1;
        do {
            next = cb->next;
            if (cb->counter == 1) {
                cb->counter = cb->period;
                cb->func(cb);
            } else {
                cb->counter--;
            }
            cb = next;
        } while (cb != NULL);
        D_8011021C = 0;
    }
}

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

s32 EGSDeviceFrameRateLastNFrames(s16 frames) {
    s32 then = D_8010FED8 - frames;
    s32 now = D_801228A8[D_8010FED8];
    s32 hz;
    u32 fps;

    if (GetVideoMode() != 0) {
        hz = 50;
    } else {
        hz = 60;
    }
    if (then < 0) {
        then += 100;
    }
    fps = (u32)(frames * hz) / (u32)(now - D_801228A8[then]);
    return (fps > (u32)(hz >> 1)) ? hz >> 1 : fps;
}

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
