#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "scratch.h"


/* Screen-space point, passed by value (PsyQ DVECTOR layout). */
typedef struct IfacePoint {
    s16 vx, vy;
} IfacePoint;

typedef union IfacePointW {
    s32 w; /* (y << 16) | x */
    IfacePoint p;
} IfacePointW;

/* GsSPRITE plus extras, 0x2C bytes. */
typedef struct IfaceSprite {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x, y;
    /* 0x08 */ u16 w, h;
    /* 0x0C */ u16 tpage;
    /* 0x0E */ u8 u, v;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u8 r, g, b;
    /* 0x18 */ s16 mx, my;
    /* 0x1C */ s16 scalex, scaley;
    /* 0x20 */ s32 rotate;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
} IfaceSprite;

/* Node of a sprite group's list. */
typedef struct IfaceSpriteNode {
    /* 0x0 */ IfaceSprite *sprite;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ struct IfaceSpriteNode *next;
} IfaceSpriteNode;

/* Sprite group handle used with SpriteShowMulti/SpriteHideMulti (8 bytes). */
typedef struct IfaceMulti {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 count;
    /* 0x4 */ IfaceSpriteNode *sprites;
} IfaceMulti;

/* Flat line as built by EGSDisplayBuildLineF (EGS display link + LINE_F2). */
typedef struct IfaceLine {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ union {
        s32 word; /* code << 24 | bgr */
        u8 c[4];
    } color;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
} IfaceLine;

/* PsyQ RECT */
typedef struct IfaceRect {
    s16 x, y, w, h;
} IfaceRect;

/* Flat-shaded quad as built by EGSDisplayBuildPoly4F; nonzero link while displayed. */
typedef struct IfacePoly4 {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ union {
        s32 word; /* code << 24 | bgr */
        u8 c[4];
    } color;
    /* 0x10 */ s32 xy[4]; /* (y << 16) | x */
    /* 0x20 */ s32 unk20;
} IfacePoly4;

/* NPC info as far as the radar cares (GameObjectRuntime.npc). */
typedef struct IfaceNpc {
    /* 0x00 */ u8 unk00[0x4C];
    /* 0x4C */ s8 radarSlot;
} IfaceNpc;

/* Weapon table (gWeaponInfo), 0x20 bytes per weapon id. */
typedef struct IfaceWeaponInfo {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 sprite; /* index into D_80117514 */
    /* 0x09 */ u8 unk09[0x17];
} IfaceWeaponInfo;

/* Radar scale matrix pieces. */
typedef struct IfaceSV3 {
    s16 vx, vy, vz;
} IfaceSV3;

/*
 * interface.c's data (docs/data_migration.md), each section in address order.
 */
/* .data */
static IfaceRect D_80105EAC[8][2] = { /* compass letter quads per heading */
    { { -129, 72, 3, 16 }, { 0, 0, 0, 0 } },
    { { -139, 60, 13, -2 }, { -129, 60, 3, 8 } },
    { { -163, 60, 20, -2 }, { 0, 0, 0, 0 } },
    { { -180, 60, 13, -2 }, { -177, 60, -3, 8 } },
    { { -177, 72, -3, 16 }, { 0, 0, 0, 0 } },
    { { -180, 100, 13, 2 }, { -177, 100, -3, -8 } },
    { { -163, 100, 20, 2 }, { 0, 0, 0, 0 } },
    { { -139, 100, 13, 2 }, { -129, 100, 3, -8 } },
};
static s32 D_80105F2C[8] = { -1, -1, -1, -1, -1, -1, -1, -1 }; /* per-mode animation step, [mode] for modes 1-7 */
#define D_80105F30 (D_80105F2C + 1)
#define D_80105F44 (D_80105F2C[6])
#define D_80105F48 (D_80105F2C[7])

/* .sdata; the literals only asm functions use are named arrays */
static char D_8010F874[4] = "";
static u8 D_8010F878 = 0x28;            /* interface colour */
static u8 D_8010F879 = 0x30;
static u8 D_8010F87A = 0x50;
static u8 D_8010F87B = 0;               /* unreferenced */
static u8 D_8010F87C[3] = { 0x30, 0x50, 0x28 }; /* unreferenced */
static u16 D_8010F880 = 0xFFFF;         /* objective text strings */
static u16 D_8010F882 = 0xFFFF;
static u16 D_8010F884 = 0xFFFF;
u8 gSpareTextWindow = 0xFF;
static u8 D_8010F887 = 1;               /* radar on */
u8 gInterfaceOn = 1;
static s32 D_8010F88C = 0;
static u16 D_8010F890 = 0xFFFF;         /* rounds string */
static u16 D_8010F892 = 0xFFFF;         /* countdown string, 0xFFFF when none */
static s32 D_8010F894 = 5;
static s32 D_8010F898 = 1;              /* countdown running */
s16 mSniperViewMax = -100;
static s16 D_8010F89E = -45;
static u16 D_8010F8A0 = 200;            /* sniper view width */
static s16 D_8010F8A2 = 90;
IfacePoly4 *gMenuPolys = NULL;          /* menu background quads, [30] is the backdrop */
IfaceLine *gMenuLines = NULL;           /* menu outline lines */
static s32 D_8010F8AC = 0;              /* requested interface mode */
static u8 D_8010F8B0 = 0xD0;
static u8 D_8010F8B1 = 0xD0;
static u8 D_8010F8B2 = 0xD0;
static s32 D_8010F8B4 = 0;
static IfaceSprite *D_8010F8B8 = NULL;  /* direction sprites (5) */
static IfaceSprite *D_8010F8BC = NULL;  /* direction sprites (8) */
static s32 D_8010F8C0 = 0;              /* weapon select ticks, -5 when closed */
static void (*D_8010F8C4)() = NULL;     /* add level specific NPC callback */
static u8 D_8010F8C8[8] = { 0, 0, 0, 0, 0x80, 0x01, 0xF0, 0x00 }; /* RECT 0, 0, 384, 240 */
static char D_8010F8D0[8] = "......";
static IfaceSV3 D_8010F8D8 = { 0, 0, 0 }; /* radar scale matrix, last row */
static s32 D_8010F8E0 = 0;              /* countdown flicker digit */
static u8 D_8010F8E4 = 0;               /* weapon changed while selecting */
static s32 D_8010F8E8 = 6;              /* joystick mode to restore */
static char D_8010F8EC[4] = " ";
static s32 D_8010F8F0 = -1;             /* target-lock message tick */
static struct { u16 id; u16 zoom; } D_8010F8F4 = { 0xFFFF, 0 }; /* sniper text string, last zoom shown */
static u8 D_8010F8F8 = 0;               /* zoom text shown */
static u8 D_8010F8F9 = 0;               /* body part last reported */

/* .sbss (aspsx 2.77 packs .lcomms by size: D_8010FFF0 gets a pad, see particle.c) */
static s32 D_8010FFE8;
static s32 D_8010FFEC;                  /* target cursor size */
static s16 D_8010FFF0;                  /* radar scale */
static s16 D_8010FFF0_pad;
static s16 D_8010FFF4;
static IfaceRect D_8010FFF8;            /* health meter clut */
static s32 D_80110000;                  /* weapon shown */
static s32 D_80110004;                  /* danger, 0x1000 = none */
static s32 D_80110008;                  /* countdown ticks */
static s32 D_8011000C;                  /* countdown end ticks */
static void (*D_80110010)();            /* countdown done callback */
static s32 D_80110014;                  /* deferred countdown args */
static s32 D_80110018;
static void (*D_8011001C)();
static u8 D_80110020;                   /* mode transition in progress */
/* 0x80110024: psylink 8-aligns weapon's .sbss; the 4-byte gap is a splat piece */

/* .bss: laid out by hand (after InterfaceForceOff), see the end of the block below */
#ifndef _PSYQ
static u8 D_80117490_bss[0x15C8];
#endif

/* commons this TU reaches through $gp */
s32 gInterfaceCurrentMode;
void *gInterfaceWindow;
s16 mTargetCursorPosition;

extern IfaceWeaponInfo gWeaponInfo[];

extern u8 D_8010F887;                   /* radar on */
extern u8 gInterfaceOn;
extern u8 gSpareTextWindow;
extern s32 D_8010F88C;
extern u16 D_8010F890;                  /* rounds string */
extern u16 D_8010F892;                  /* countdown string, 0xFFFF when none */
extern s32 D_8010F894;
extern s32 D_8010F898;                  /* countdown running */
extern s32 D_8010F8AC;                  /* requested interface mode */
extern void (*D_8010F8C4)();            /* add level specific NPC callback */
extern s32 D_8010FFE8;
extern s32 D_8010FFEC;                  /* target cursor size */
extern s16 D_8010FFF0;                  /* radar scale */
extern s16 D_8010FFF4;
extern IfaceRect D_8010FFF8;            /* health meter clut */
extern s32 D_80110000;                  /* weapon shown */
extern s32 D_80110008;                  /* countdown ticks */
extern s32 D_8011000C;                  /* countdown end ticks */
extern void (*D_80110010)();            /* countdown done callback */
extern s32 D_80110014;                  /* deferred countdown args */
extern s32 D_80110018;
extern void (*D_8011001C)();
extern u8 D_80110020;                   /* mode transition in progress */
extern s32 gInterfaceCurrentMode;
extern void *gInterfaceWindow;

extern VECTOR32 mBombPosition; /* pad holds the bomb's object id */
extern IfacePoly4 mBombPoly;
extern VECTOR32 D_80117490; /* chase pointer target */
extern IfacePoly4 D_801174C4; /* chase pointer */
extern s16 D_80117B2C[];
extern s32 D_80117C58[];
extern s16 D_80117DFC[]; /* health meter clut */
extern IfaceMulti D_80117514[]; /* weapon icons */
extern s32 gCurrentWeapon;
extern s32 gSystemTick;
extern char *gStringFillerText[];
extern char *gCountdownFmt;
extern char D_80118104[];

void EGSDisplayAddPoly(void *camera, void *poly);
void EGSDisplayDeletePoly(void *camera, void *poly);
void EGSDisplayBuildPoly4F(void *poly, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void iInterfaceRadarRemove(void);
void iInterfaceRadarAdd(void);
void iDeleteTargetCursor(void);
void func_8003E13C(void);
u16 WeaponMakeRoundsString(u16 str, s32 weapon);
void TextRemoveString();
void EGSDisplayInfraredOn(void *camera, s32 mode);
void EGSDisplayInfraredOff(void *camera, s32 mode);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextSetWindowMini(u8 window, s32 on);
void TextSetWindowColor(u8 window, s32 r, s32 g, s32 b);
void TextSetWindowJustification(u8 window, s32 just);
void TextRemoveWindow(u8 window);
u16 TextAddString(u8 window, char *str, s32 arg2, s32 arg3);
u16 TextAddPersistentString(char *fmt, char *buf, s32 x, s32 y);
void TextSetStringJustification(u16 str, s32 just);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, void (*arg)());
s32 GameCamCheckScriptCamDoneCallback();
void InterfaceCountdownDisplay(void);
void InterfaceStartCountdown(s16 seconds, s16 endSeconds, void (*done)());
void InterfaceWeaponDisplayUpdate(void);
void SpriteHideMulti(void *window, IfaceMulti *multi);
void SpriteShowMulti(void *window, IfaceMulti *multi);
void *memcpy(void *dst, void *src, s32 n);
int rand(void);
u8 func_801417D0(u8 on, s32 arg1);

/* Weapon select slots, 0x24 bytes each. */
typedef struct IfaceSlot {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ u8 unk10[0x14];
} IfaceSlot;

/* Health meter texture header handed to InterfaceHealthMeterTexture. */
typedef struct IfaceTexture {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ IfaceRect rect; /* clut position, w = entries */
    /* 0x24 */ u16 *clut;
} IfaceTexture;

extern IfaceLine D_801179A0[6]; /* weapon-select bars, two lines each */
extern struct { s32 unk00; u8 unk04[0x14]; } D_801179B0[6]; /* D_801179A0[].xy0 */
extern IfaceSlot D_801175DC[4];

u8 func_8004146C(u8 active);
u8 func_80041BCC(u8 active);
u8 func_80042DA4(u8 active);
u8 func_80043B04(u8 active);
u8 func_80044AC0(u8 active);
u8 func_800451DC(u8 active);
u8 func_800453F8(u8 active);

extern s16 D_8011031E;                  /* target cursor lines shown */
extern IfaceLine D_80118264[36];        /* target cursor lines */
extern IfaceMulti D_80118A1C[7];        /* weapon select icons */
extern IfaceSprite *D_8010F8B8;         /* direction sprites (5) */
extern IfaceSprite *D_8010F8BC;         /* direction sprites (8) */
void SpriteSetMultiXY(IfaceMulti *multi, s32 x, s32 y);
void SpriteDestroyMulti(IfaceMulti *multi);
void SpriteDisposeTemporary(void *sprite);
void EGSDisplayDeleteSprite(void *camera, IfaceSprite *sprite);
s32 EGSTextureSetSpriteTransparency(IfaceSprite *sprite, s32 mode);
void EGSDisplayBuildLineF(IfaceLine *line, s32 color, s32 xy0, s32 xy1);
s32 WeaponGetNextID(s32 weapon);
s32 WeaponGetPreviousID(s32 weapon);
void func_8004072C(s32 weapon);

/* Display primitives of other sizes; only the EGS display link is touched here. */
typedef struct IfacePoly24 {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 color; /* code << 24 | bgr */
    /* 0x10 */ u8 unk10[0x14];
} IfacePoly24;

/* Gouraud quad (EGS display link + POLY_G4). */
typedef struct IfacePoly30 {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 c0;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 c1;
    /* 0x18 */ s32 xy1;
    /* 0x1C */ s32 c2;
    /* 0x20 */ s32 xy2;
    /* 0x24 */ s32 c3;
    /* 0x28 */ s32 xy3;
    /* 0x2C */ s32 unk2C;
} IfacePoly30;

/* Weapon icon sprite ranges, indexed by IfaceWeaponInfo.sprite. */
typedef struct IfaceIconDef {
    u8 first;
    u8 count;
} IfaceIconDef;

extern IfacePoly30 D_80117B38[6];       /* radar blips */
/* Flat triangle (EGS display link + POLY_F3). */
typedef struct IfacePolyF3 {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 color;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
    /* 0x18 */ s32 xy2;
} IfacePolyF3;

extern IfacePolyF3 D_8011896C[2];       /* sniper range arrows */
/* Gouraud triangle (EGS display link + POLY_G3). */
typedef struct IfacePolyG3 {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 c0; /* code << 24 | bgr */
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 c1;
    /* 0x18 */ s32 xy1;
    /* 0x1C */ s32 c2;
    /* 0x20 */ s32 xy2;
} IfacePolyG3;

extern IfacePoly4 D_80117C70[2];        /* radar frame */
extern IfacePoly24 D_80117CB8[2];
extern IfacePolyG3 D_80117D00;
extern IfaceLine D_80117D24[5];
extern IfacePoly24 D_801174A0; /* radar player arrow (POLY_G3 layout) */
int ratan2(int y, int x);
void ApplyMatrixLV(GOBJ_MATRIX *m, VECTOR32 *in, VECTOR32 *out);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);
#define FIX12(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))
extern const IfaceIconDef D_80011BA0[];
extern u8 gInEOLState;
extern s16 gLevelSpecificNpcID;
extern s16 gLevelSpecificNpcID2;
extern char *gLevelSpecificNpcName;
extern char *gLevelSpecificNpc2Name;
u16 TextAddMarker(u16 objId, char *str, s32 arg2, s32 arg3);
void NPCLosCheck(void);
void NPCAim(void);
void SniperUpdate(void);
void func_8003FBE4(void);
void func_8003D76C(void);
void WeaponUpdate(void);
void iInterfaceDoSniffer(void);
void func_800454B8(void);
void func_80045548(void);
IfaceSprite *SpriteGetTemporary(s32 count);
IfaceSprite *SpriteGet(s32 idx);
void SpriteCreateMulti(IfaceMulti *multi, s32 count, IfaceSprite *sprites);

/* Target cursor position x/y/z, then D_8011031E (an SVECTOR in the original; the
 * y/z/pad halves are addressed by their own symbols, which keeps them off $gp) */
extern s16 mTargetCursorPosition;
extern s16 D_8011031A;
extern s16 D_8011031C;
extern IfacePoly4 D_801185C4[16]; /* sniper/infrared view frame; [8..15] are the zoom bars */
extern IfacePoly4 D_80117A30[6];  /* radar NPC boxes */
extern IfacePoly4 D_80117B08;     /* radar target box */
extern const s32 D_80011C18[];          /* menu background vertices, (y << 16) | x */
extern const u8 D_80011D10[7][4];       /* menu background quads (indices into D_80011C18) */
s32 SquareRoot0(s32 x);
IfaceLine *func_80041378(IfaceLine *lines, s32 count, s32 color, s32 trans);

/* Returns the previous callback. */
void (*InterfaceSetAddLevelSpecificNPCCallback(void (*cb)()))() {
    void (*old)() = D_8010F8C4;
    D_8010F8C4 = cb;
    return old;
}

void InterfaceForceOff(void) {
    s32 i;
    s32 off;
    IfaceLine *poly;

    for (i = 1; i < 8; i++) {
        if (D_80105F30[i - 1] > 0) {
            D_80105F30[i - 1] = 1;
        }
    }
    i = 0;
    off = 0x04000400;
    poly = D_801179A0;
    for (; i < 6; i++) {
        (poly++)->xy1 = off;
        D_801179B0[i].unk00 = off;
    }
    D_801175DC[1].unk00 = D_801175DC[0].unk00;
    D_801175DC[1].unk08 = D_801175DC[0].unk08;
    for (i = 0; i < 4; i++) {
        D_801175DC[i].unk04 = D_801175DC[i].unk00;
        D_801175DC[i].unk0C = D_801175DC[i].unk08;
    }
}

#ifdef _PSYQ
__asm__("\t.bss\n"
        "D_80117490:\t.space\t4\n"
        "D_80117494:\t.space\t4\n"
        "D_80117498:\t.space\t8\n"
        "D_801174A0:\t.space\t16\n"
        "D_801174B0:\t.space\t8\n"
        "D_801174B8:\t.space\t8\n"
        "D_801174C0:\t.space\t4\n"
        "D_801174C4:\t.space\t36\n"
        "D_801174E8:\t.space\t44\n"
        "D_80117514:\t.space\t184\n"
        "D_801175CC:\t.space\t16\n"
        "D_801175DC:\t.space\t4\n"
        "D_801175E0:\t.space\t4\n"
        "D_801175E4:\t.space\t4\n"
        "D_801175E8:\t.space\t20\n"
        "D_801175FC:\t.space\t4\n"
        "D_80117600:\t.space\t4\n"
        "D_80117604:\t.space\t4\n"
        "D_80117608:\t.space\t24\n"
        "D_80117620:\t.space\t4\n"
        "D_80117624:\t.space\t4\n"
        "D_80117628:\t.space\t28\n"
        "D_80117644:\t.space\t4\n"
        "D_80117648:\t.space\t4\n"
        "D_8011764C:\t.space\t16\n"
        "D_8011765C:\t.space\t264\n"
        "D_80117764:\t.space\t264\n"
        "D_8011786C:\t.space\t308\n"
        "D_801179A0:\t.space\t8\n"
        "D_801179A8:\t.space\t4\n"
        "D_801179AC:\t.space\t4\n"
        "D_801179B0:\t.space\t4\n"
        "D_801179B4:\t.space\t124\n"
        "D_80117A30:\t.space\t16\n"
        "D_80117A40:\t.space\t4\n"
        "D_80117A44:\t.space\t4\n"
        "D_80117A48:\t.space\t4\n"
        "D_80117A4C:\t.space\t188\n"
        "D_80117B08:\t.space\t4\n"
        "D_80117B0C:\t.space\t8\n"
        "D_80117B14:\t.space\t4\n"
        "D_80117B18:\t.space\t4\n"
        "D_80117B1C:\t.space\t4\n"
        "D_80117B20:\t.space\t4\n"
        "D_80117B24:\t.space\t8\n"
        "D_80117B2C:\t.space\t12\n"
        "D_80117B38:\t.space\t16\n"
        "D_80117B48:\t.space\t8\n"
        "D_80117B50:\t.space\t8\n"
        "D_80117B58:\t.space\t8\n"
        "D_80117B60:\t.space\t248\n"
        "D_80117C58:\t.space\t24\n"
        "D_80117C70:\t.space\t8\n"
        "D_80117C78:\t.space\t4\n"
        "D_80117C7C:\t.space\t4\n"
        "D_80117C80:\t.space\t4\n"
        "D_80117C84:\t.space\t4\n"
        "D_80117C88:\t.space\t4\n"
        "D_80117C8C:\t.space\t24\n"
        "D_80117CA4:\t.space\t4\n"
        "D_80117CA8:\t.space\t4\n"
        "D_80117CAC:\t.space\t4\n"
        "D_80117CB0:\t.space\t8\n"
        "D_80117CB8:\t.space\t8\n"
        "D_80117CC0:\t.space\t4\n"
        "D_80117CC4:\t.space\t60\n"
        "D_80117D00:\t.space\t4\n"
        "D_80117D04:\t.space\t8\n"
        "D_80117D0C:\t.space\t3\n"
        "D_80117D0F:\t.space\t5\n"
        "D_80117D14:\t.space\t4\n"
        "D_80117D18:\t.space\t4\n"
        "D_80117D1C:\t.space\t4\n"
        "D_80117D20:\t.space\t4\n"
        "D_80117D24:\t.space\t8\n"
        "D_80117D2C:\t.space\t4\n"
        "D_80117D30:\t.space\t4\n"
        "D_80117D34:\t.space\t4\n"
        "D_80117D38:\t.space\t100\n"
        "D_80117D9C:\t.space\t8\n"
        "D_80117DA4:\t.space\t4\n"
        "D_80117DA8:\t.space\t4\n"
        "D_80117DAC:\t.space\t4\n"
        "D_80117DB0:\t.space\t4\n"
        "D_80117DB4:\t.space\t72\n"
        "D_80117DFC:\t.space\t410\n"
        "D_80117F96:\t.space\t102\n"
        "D_80117FFC:\t.space\t264\n"
        "D_80118104:\t.space\t352\n"
        "D_80118264:\t.space\t12\n"
        "D_80118270:\t.space\t4\n"
        "D_80118274:\t.space\t4\n"
        "D_80118278:\t.space\t4\n"
        "D_8011827C:\t.space\t16\n"
        "D_8011828C:\t.space\t4\n"
        "D_80118290:\t.space\t4\n"
        "D_80118294:\t.space\t16\n"
        "D_801182A4:\t.space\t4\n"
        "D_801182A8:\t.space\t4\n"
        "D_801182AC:\t.space\t16\n"
        "D_801182BC:\t.space\t4\n"
        "D_801182C0:\t.space\t4\n"
        "D_801182C4:\t.space\t16\n"
        "D_801182D4:\t.space\t4\n"
        "D_801182D8:\t.space\t4\n"
        "D_801182DC:\t.space\t16\n"
        "D_801182EC:\t.space\t4\n"
        "D_801182F0:\t.space\t20\n"
        "D_80118304:\t.space\t4\n"
        "D_80118308:\t.space\t20\n"
        "D_8011831C:\t.space\t4\n"
        "D_80118320:\t.space\t20\n"
        "D_80118334:\t.space\t4\n"
        "D_80118338:\t.space\t4\n"
        "D_8011833C:\t.space\t16\n"
        "D_8011834C:\t.space\t4\n"
        "D_80118350:\t.space\t4\n"
        "D_80118354:\t.space\t16\n"
        "D_80118364:\t.space\t4\n"
        "D_80118368:\t.space\t20\n"
        "D_8011837C:\t.space\t4\n"
        "D_80118380:\t.space\t20\n"
        "D_80118394:\t.space\t4\n"
        "D_80118398:\t.space\t4\n"
        "D_8011839C:\t.space\t16\n"
        "D_801183AC:\t.space\t4\n"
        "D_801183B0:\t.space\t20\n"
        "D_801183C4:\t.space\t4\n"
        "D_801183C8:\t.space\t20\n"
        "D_801183DC:\t.space\t4\n"
        "D_801183E0:\t.space\t20\n"
        "D_801183F4:\t.space\t4\n"
        "D_801183F8:\t.space\t20\n"
        "D_8011840C:\t.space\t4\n"
        "D_80118410:\t.space\t20\n"
        "D_80118424:\t.space\t4\n"
        "D_80118428:\t.space\t20\n"
        "D_8011843C:\t.space\t4\n"
        "D_80118440:\t.space\t20\n"
        "D_80118454:\t.space\t4\n"
        "D_80118458:\t.space\t20\n"
        "D_8011846C:\t.space\t4\n"
        "D_80118470:\t.space\t20\n"
        "D_80118484:\t.space\t4\n"
        "D_80118488:\t.space\t20\n"
        "D_8011849C:\t.space\t4\n"
        "D_801184A0:\t.space\t20\n"
        "D_801184B4:\t.space\t4\n"
        "D_801184B8:\t.space\t20\n"
        "D_801184CC:\t.space\t4\n"
        "D_801184D0:\t.space\t20\n"
        "D_801184E4:\t.space\t4\n"
        "D_801184E8:\t.space\t20\n"
        "D_801184FC:\t.space\t4\n"
        "D_80118500:\t.space\t196\n"
        "D_801185C4:\t.space\t8\n"
        "D_801185CC:\t.space\t4\n"
        "D_801185D0:\t.space\t276\n"
        "D_801186E4:\t.space\t16\n"
        "D_801186F4:\t.space\t4\n"
        "D_801186F8:\t.space\t4\n"
        "D_801186FC:\t.space\t4\n"
        "D_80118700:\t.space\t24\n"
        "D_80118718:\t.space\t4\n"
        "D_8011871C:\t.space\t4\n"
        "D_80118720:\t.space\t4\n"
        "D_80118724:\t.space\t24\n"
        "D_8011873C:\t.space\t4\n"
        "D_80118740:\t.space\t4\n"
        "D_80118744:\t.space\t4\n"
        "D_80118748:\t.space\t24\n"
        "D_80118760:\t.space\t4\n"
        "D_80118764:\t.space\t4\n"
        "D_80118768:\t.space\t4\n"
        "D_8011876C:\t.space\t24\n"
        "D_80118784:\t.space\t4\n"
        "D_80118788:\t.space\t4\n"
        "D_8011878C:\t.space\t4\n"
        "D_80118790:\t.space\t24\n"
        "D_801187A8:\t.space\t4\n"
        "D_801187AC:\t.space\t4\n"
        "D_801187B0:\t.space\t4\n"
        "D_801187B4:\t.space\t440\n"
        "D_8011896C:\t.space\t8\n"
        "D_80118974:\t.space\t8\n"
        "D_8011897C:\t.space\t4\n"
        "D_80118980:\t.space\t4\n"
        "D_80118984:\t.space\t20\n"
        "D_80118998:\t.space\t4\n"
        "D_8011899C:\t.space\t4\n"
        "D_801189A0:\t.space\t4\n"
        "D_801189A4:\t.space\t15\n"
        "D_801189B3:\t.space\t36\n"
        "D_801189D7:\t.space\t21\n"
        "D_801189EC:\t.space\t48\n"
        "D_80118A1C:\t.space\t4\n"
        "D_80118A20:\t.space\t20\n"
        "D_80118A34:\t.space\t36\n"
        "\t.text");
#else
__asm__(".set D_80117490, D_80117490_bss+0x0\n"
        ".set D_80117494, D_80117490_bss+0x4\n"
        ".set D_80117498, D_80117490_bss+0x8\n"
        ".set D_801174A0, D_80117490_bss+0x10\n"
        ".set D_801174B0, D_80117490_bss+0x20\n"
        ".set D_801174B8, D_80117490_bss+0x28\n"
        ".set D_801174C0, D_80117490_bss+0x30\n"
        ".set D_801174C4, D_80117490_bss+0x34\n"
        ".set D_801174E8, D_80117490_bss+0x58\n"
        ".set D_80117514, D_80117490_bss+0x84\n"
        ".set D_801175CC, D_80117490_bss+0x13C\n"
        ".set D_801175DC, D_80117490_bss+0x14C\n"
        ".set D_801175E0, D_80117490_bss+0x150\n"
        ".set D_801175E4, D_80117490_bss+0x154\n"
        ".set D_801175E8, D_80117490_bss+0x158\n"
        ".set D_801175FC, D_80117490_bss+0x16C\n"
        ".set D_80117600, D_80117490_bss+0x170\n"
        ".set D_80117604, D_80117490_bss+0x174\n"
        ".set D_80117608, D_80117490_bss+0x178\n"
        ".set D_80117620, D_80117490_bss+0x190\n"
        ".set D_80117624, D_80117490_bss+0x194\n"
        ".set D_80117628, D_80117490_bss+0x198\n"
        ".set D_80117644, D_80117490_bss+0x1B4\n"
        ".set D_80117648, D_80117490_bss+0x1B8\n"
        ".set D_8011764C, D_80117490_bss+0x1BC\n"
        ".set D_8011765C, D_80117490_bss+0x1CC\n"
        ".set D_80117764, D_80117490_bss+0x2D4\n"
        ".set D_8011786C, D_80117490_bss+0x3DC\n"
        ".set D_801179A0, D_80117490_bss+0x510\n"
        ".set D_801179A8, D_80117490_bss+0x518\n"
        ".set D_801179AC, D_80117490_bss+0x51C\n"
        ".set D_801179B0, D_80117490_bss+0x520\n"
        ".set D_801179B4, D_80117490_bss+0x524\n"
        ".set D_80117A30, D_80117490_bss+0x5A0\n"
        ".set D_80117A40, D_80117490_bss+0x5B0\n"
        ".set D_80117A44, D_80117490_bss+0x5B4\n"
        ".set D_80117A48, D_80117490_bss+0x5B8\n"
        ".set D_80117A4C, D_80117490_bss+0x5BC\n"
        ".set D_80117B08, D_80117490_bss+0x678\n"
        ".set D_80117B0C, D_80117490_bss+0x67C\n"
        ".set D_80117B14, D_80117490_bss+0x684\n"
        ".set D_80117B18, D_80117490_bss+0x688\n"
        ".set D_80117B1C, D_80117490_bss+0x68C\n"
        ".set D_80117B20, D_80117490_bss+0x690\n"
        ".set D_80117B24, D_80117490_bss+0x694\n"
        ".set D_80117B2C, D_80117490_bss+0x69C\n"
        ".set D_80117B38, D_80117490_bss+0x6A8\n"
        ".set D_80117B48, D_80117490_bss+0x6B8\n"
        ".set D_80117B50, D_80117490_bss+0x6C0\n"
        ".set D_80117B58, D_80117490_bss+0x6C8\n"
        ".set D_80117B60, D_80117490_bss+0x6D0\n"
        ".set D_80117C58, D_80117490_bss+0x7C8\n"
        ".set D_80117C70, D_80117490_bss+0x7E0\n"
        ".set D_80117C78, D_80117490_bss+0x7E8\n"
        ".set D_80117C7C, D_80117490_bss+0x7EC\n"
        ".set D_80117C80, D_80117490_bss+0x7F0\n"
        ".set D_80117C84, D_80117490_bss+0x7F4\n"
        ".set D_80117C88, D_80117490_bss+0x7F8\n"
        ".set D_80117C8C, D_80117490_bss+0x7FC\n"
        ".set D_80117CA4, D_80117490_bss+0x814\n"
        ".set D_80117CA8, D_80117490_bss+0x818\n"
        ".set D_80117CAC, D_80117490_bss+0x81C\n"
        ".set D_80117CB0, D_80117490_bss+0x820\n"
        ".set D_80117CB8, D_80117490_bss+0x828\n"
        ".set D_80117CC0, D_80117490_bss+0x830\n"
        ".set D_80117CC4, D_80117490_bss+0x834\n"
        ".set D_80117D00, D_80117490_bss+0x870\n"
        ".set D_80117D04, D_80117490_bss+0x874\n"
        ".set D_80117D0C, D_80117490_bss+0x87C\n"
        ".set D_80117D0F, D_80117490_bss+0x87F\n"
        ".set D_80117D14, D_80117490_bss+0x884\n"
        ".set D_80117D18, D_80117490_bss+0x888\n"
        ".set D_80117D1C, D_80117490_bss+0x88C\n"
        ".set D_80117D20, D_80117490_bss+0x890\n"
        ".set D_80117D24, D_80117490_bss+0x894\n"
        ".set D_80117D2C, D_80117490_bss+0x89C\n"
        ".set D_80117D30, D_80117490_bss+0x8A0\n"
        ".set D_80117D34, D_80117490_bss+0x8A4\n"
        ".set D_80117D38, D_80117490_bss+0x8A8\n"
        ".set D_80117D9C, D_80117490_bss+0x90C\n"
        ".set D_80117DA4, D_80117490_bss+0x914\n"
        ".set D_80117DA8, D_80117490_bss+0x918\n"
        ".set D_80117DAC, D_80117490_bss+0x91C\n"
        ".set D_80117DB0, D_80117490_bss+0x920\n"
        ".set D_80117DB4, D_80117490_bss+0x924\n"
        ".set D_80117DFC, D_80117490_bss+0x96C\n"
        ".set D_80117F96, D_80117490_bss+0xB06\n"
        ".set D_80117FFC, D_80117490_bss+0xB6C\n"
        ".set D_80118104, D_80117490_bss+0xC74\n"
        ".set D_80118264, D_80117490_bss+0xDD4\n"
        ".set D_80118270, D_80117490_bss+0xDE0\n"
        ".set D_80118274, D_80117490_bss+0xDE4\n"
        ".set D_80118278, D_80117490_bss+0xDE8\n"
        ".set D_8011827C, D_80117490_bss+0xDEC\n"
        ".set D_8011828C, D_80117490_bss+0xDFC\n"
        ".set D_80118290, D_80117490_bss+0xE00\n"
        ".set D_80118294, D_80117490_bss+0xE04\n"
        ".set D_801182A4, D_80117490_bss+0xE14\n"
        ".set D_801182A8, D_80117490_bss+0xE18\n"
        ".set D_801182AC, D_80117490_bss+0xE1C\n"
        ".set D_801182BC, D_80117490_bss+0xE2C\n"
        ".set D_801182C0, D_80117490_bss+0xE30\n"
        ".set D_801182C4, D_80117490_bss+0xE34\n"
        ".set D_801182D4, D_80117490_bss+0xE44\n"
        ".set D_801182D8, D_80117490_bss+0xE48\n"
        ".set D_801182DC, D_80117490_bss+0xE4C\n"
        ".set D_801182EC, D_80117490_bss+0xE5C\n"
        ".set D_801182F0, D_80117490_bss+0xE60\n"
        ".set D_80118304, D_80117490_bss+0xE74\n"
        ".set D_80118308, D_80117490_bss+0xE78\n"
        ".set D_8011831C, D_80117490_bss+0xE8C\n"
        ".set D_80118320, D_80117490_bss+0xE90\n"
        ".set D_80118334, D_80117490_bss+0xEA4\n"
        ".set D_80118338, D_80117490_bss+0xEA8\n"
        ".set D_8011833C, D_80117490_bss+0xEAC\n"
        ".set D_8011834C, D_80117490_bss+0xEBC\n"
        ".set D_80118350, D_80117490_bss+0xEC0\n"
        ".set D_80118354, D_80117490_bss+0xEC4\n"
        ".set D_80118364, D_80117490_bss+0xED4\n"
        ".set D_80118368, D_80117490_bss+0xED8\n"
        ".set D_8011837C, D_80117490_bss+0xEEC\n"
        ".set D_80118380, D_80117490_bss+0xEF0\n"
        ".set D_80118394, D_80117490_bss+0xF04\n"
        ".set D_80118398, D_80117490_bss+0xF08\n"
        ".set D_8011839C, D_80117490_bss+0xF0C\n"
        ".set D_801183AC, D_80117490_bss+0xF1C\n"
        ".set D_801183B0, D_80117490_bss+0xF20\n"
        ".set D_801183C4, D_80117490_bss+0xF34\n"
        ".set D_801183C8, D_80117490_bss+0xF38\n"
        ".set D_801183DC, D_80117490_bss+0xF4C\n"
        ".set D_801183E0, D_80117490_bss+0xF50\n"
        ".set D_801183F4, D_80117490_bss+0xF64\n"
        ".set D_801183F8, D_80117490_bss+0xF68\n"
        ".set D_8011840C, D_80117490_bss+0xF7C\n"
        ".set D_80118410, D_80117490_bss+0xF80\n"
        ".set D_80118424, D_80117490_bss+0xF94\n"
        ".set D_80118428, D_80117490_bss+0xF98\n"
        ".set D_8011843C, D_80117490_bss+0xFAC\n"
        ".set D_80118440, D_80117490_bss+0xFB0\n"
        ".set D_80118454, D_80117490_bss+0xFC4\n"
        ".set D_80118458, D_80117490_bss+0xFC8\n"
        ".set D_8011846C, D_80117490_bss+0xFDC\n"
        ".set D_80118470, D_80117490_bss+0xFE0\n"
        ".set D_80118484, D_80117490_bss+0xFF4\n"
        ".set D_80118488, D_80117490_bss+0xFF8\n"
        ".set D_8011849C, D_80117490_bss+0x100C\n"
        ".set D_801184A0, D_80117490_bss+0x1010\n"
        ".set D_801184B4, D_80117490_bss+0x1024\n"
        ".set D_801184B8, D_80117490_bss+0x1028\n"
        ".set D_801184CC, D_80117490_bss+0x103C\n"
        ".set D_801184D0, D_80117490_bss+0x1040\n"
        ".set D_801184E4, D_80117490_bss+0x1054\n"
        ".set D_801184E8, D_80117490_bss+0x1058\n"
        ".set D_801184FC, D_80117490_bss+0x106C\n"
        ".set D_80118500, D_80117490_bss+0x1070\n"
        ".set D_801185C4, D_80117490_bss+0x1134\n"
        ".set D_801185CC, D_80117490_bss+0x113C\n"
        ".set D_801185D0, D_80117490_bss+0x1140\n"
        ".set D_801186E4, D_80117490_bss+0x1254\n"
        ".set D_801186F4, D_80117490_bss+0x1264\n"
        ".set D_801186F8, D_80117490_bss+0x1268\n"
        ".set D_801186FC, D_80117490_bss+0x126C\n"
        ".set D_80118700, D_80117490_bss+0x1270\n"
        ".set D_80118718, D_80117490_bss+0x1288\n"
        ".set D_8011871C, D_80117490_bss+0x128C\n"
        ".set D_80118720, D_80117490_bss+0x1290\n"
        ".set D_80118724, D_80117490_bss+0x1294\n"
        ".set D_8011873C, D_80117490_bss+0x12AC\n"
        ".set D_80118740, D_80117490_bss+0x12B0\n"
        ".set D_80118744, D_80117490_bss+0x12B4\n"
        ".set D_80118748, D_80117490_bss+0x12B8\n"
        ".set D_80118760, D_80117490_bss+0x12D0\n"
        ".set D_80118764, D_80117490_bss+0x12D4\n"
        ".set D_80118768, D_80117490_bss+0x12D8\n"
        ".set D_8011876C, D_80117490_bss+0x12DC\n"
        ".set D_80118784, D_80117490_bss+0x12F4\n"
        ".set D_80118788, D_80117490_bss+0x12F8\n"
        ".set D_8011878C, D_80117490_bss+0x12FC\n"
        ".set D_80118790, D_80117490_bss+0x1300\n"
        ".set D_801187A8, D_80117490_bss+0x1318\n"
        ".set D_801187AC, D_80117490_bss+0x131C\n"
        ".set D_801187B0, D_80117490_bss+0x1320\n"
        ".set D_801187B4, D_80117490_bss+0x1324\n"
        ".set D_8011896C, D_80117490_bss+0x14DC\n"
        ".set D_80118974, D_80117490_bss+0x14E4\n"
        ".set D_8011897C, D_80117490_bss+0x14EC\n"
        ".set D_80118980, D_80117490_bss+0x14F0\n"
        ".set D_80118984, D_80117490_bss+0x14F4\n"
        ".set D_80118998, D_80117490_bss+0x1508\n"
        ".set D_8011899C, D_80117490_bss+0x150C\n"
        ".set D_801189A0, D_80117490_bss+0x1510\n"
        ".set D_801189A4, D_80117490_bss+0x1514\n"
        ".set D_801189B3, D_80117490_bss+0x1523\n"
        ".set D_801189D7, D_80117490_bss+0x1547\n"
        ".set D_801189EC, D_80117490_bss+0x155C\n"
        ".set D_80118A1C, D_80117490_bss+0x158C\n"
        ".set D_80118A20, D_80117490_bss+0x1590\n"
        ".set D_80118A34, D_80117490_bss+0x15A4\n"
        ".set D_80105F30, D_80105F2C+4\n"
        ".set D_80105F34, D_80105F2C+8\n"
        ".set D_80105F38, D_80105F2C+12\n"
        ".set D_80105F3C, D_80105F2C+16\n"
        ".set D_80105F40, D_80105F2C+20\n"
        ".set D_80105F44, D_80105F2C+24\n"
        ".set D_80105F48, D_80105F2C+28\n");
#endif

void InterfacePlaceQuad(IfacePoly4 *poly, IfacePoint pos, s32 w, s32 h) {
    s32 left, right;

    if (poly->link == 0) {
        EGSDisplayAddPoly(gInterfaceWindow, poly);
    }
    left = (pos.vx - w) & 0xFFFF;
    right = (pos.vx + w) & 0xFFFF;
    poly->xy[0] = left | ((pos.vy - h) << 16);
    poly->xy[1] = right | ((pos.vy - h) << 16);
    poly->xy[2] = left | ((pos.vy + h) << 16);
    poly->xy[3] = right | ((pos.vy + h) << 16);
}

extern const IfacePoint D_80011BE4[]; /* weapon-select bar origins */
extern Gobj *gObjPlayer;

extern s16 POSITION;     /* screen centre x */
extern s16 D_8012C7FA;   /* screen centre y */
extern void *gTextCamera;
void TextRemoveWindowNow(u8 window);

extern s32 D_8010F8E0;                  /* countdown flicker digit */
extern u8 D_8010F878, D_8010F879, D_8010F87A; /* interface colour */
extern u8 D_8010F8B0, D_8010F8B1, D_8010F8B2;
u16 TextReplaceString(u16 str, char *text);
void TextSetStringIntensity(u16 str, u8 r, u8 g, u8 b);

/* A colour word with byte access (r, g, b, code). */
typedef union IfaceColor {
    s32 w;
    u8 c[4];
} IfaceColor;

extern u8 gTargetModeFlag;
extern s32 D_80117B14; /* D_80117B08.color */
int rcos(int a);

/* Head of a GPU primitive (PsyQ P_TAG). */
typedef struct IfacePrimTag {
    u32 tag;
    u8 r0, g0, b0, code;
} IfacePrimTag;

#define SET_SEMI_TRANS(p) (((IfacePrimTag *)(p))->code |= 2)
#define PRIM_COLOR(p) (*(s32 *)&((IfacePrimTag *)(p))->r0)

/* Text colour passed by value. */
typedef struct IfaceRGB {
    u8 r, g, b, pad;
} IfaceRGB;

extern IfaceLine D_80117D9C[4];

extern u16 D_8010F880, D_8010F882, D_8010F884; /* objective text strings */
extern s16 D_8010F89E, D_8010F8A2;
extern s16 mSniperViewMax;

/* Sniffer gauge lines (D_80118264[10..27]) with the x halves addressable. */
typedef struct IfaceSnifferLine {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 color;
    /* 0x10 */ s16 x0, y0;
    /* 0x14 */ s16 x1, y1;
} IfaceSnifferLine;

extern IfaceSnifferLine D_80118354[18];
extern s32 D_80118364; /* D_80118354[0].x0/y0 */
extern s32 D_8011840C; /* D_80118354[7].x0/y0 */
extern u16 D_80118308, D_80118320, D_80118334, D_80118338, D_8011834C, D_80118350; /* x halves of lines 6-9 */
extern char *gStringSnifferLock[];
void EGSDisplayTransformWorldPointToScreen(VECTOR32 *world, void *camera, VECTOR32 *screen);

extern s32 gSniperMode;
VECTOR32 *SniperGetTarget(s32 arg0);
void func_8003EF48(VECTOR32 *screen, s32 fresh);
void func_8003F3F8(VECTOR32 *screen, s32 fresh);
void func_8003F8A0(VECTOR32 *out);
void func_8003FA60(VECTOR32 *pos);

extern s32 D_80118270[]; /* D_80118264[].color, as words */

#define XY(x, y) ((u16)(x) | ((u16)(y) << 16))

/* What *gCamera points at, as far as the interface goes. */
typedef struct IfaceCamera {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ u16 h; /* projection distance */
} IfaceCamera;

extern s32 D_8010F8C0;                  /* weapon select ticks, -5 when closed */
extern u8 D_8010F8E4;                   /* weapon changed while selecting */
extern s32 D_8010F8E8;                  /* joystick mode to restore */
extern IfaceLine D_801189EC[2];
extern IfacePoly4 D_801189A4[2];
extern s32 gPlayerOffset;
void WeaponSelect(Gobj *obj, s32 action, s32 arg2);
void WeaponReload(s32 weapon);
void CASRequestAction(s32 id, s32 action);
s32 GameSysJoystickSetMode(s32 mode);
s32 GameSysJoystickGetMode(void);
void func_800408A8(void);
void func_80040940(s32 weapon, s32 dir);

extern u16 D_80118A34; /* D_80118A1C[3].unk0 */

extern u16 D_8010F8A0;                  /* sniper view width */
extern s32 D_8010F8F0;                  /* target-lock message tick */
extern u8 D_8010F8F8;                   /* zoom text shown */
extern u8 D_8010F8F9;                   /* body part last reported */
extern struct { s32 color; u8 unk04[0x20]; } D_801186E4[8]; /* zoom bar polys from their colour word */
extern s16 gSniperGobjHit;
extern s32 gSniperBonesHit;
extern char *gTargetLockMsg;
extern char *gHeadMsg, *gWeaponMsg, *gHandMsg, *gBodyMsg, *gLimbMsg;
extern char *gTargetFmt;
extern char *gZoomFmt;
int sprintf(char *buf, const char *fmt, ...);
s32 TextIsStringShowing(u16 str);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011B90);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BA0);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BD0);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BD4);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BD8);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BDC);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BE0);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011BE4);

/* Per-mode animation steps, called every tick while a mode change is in progress. */
static u8 (*const D_80011BF4[])(u8) = {
    func_8004146C, func_80041BCC, func_80042DA4, func_80043B04,
    func_80044AC0, func_800451DC, func_800453F8,
};

u8 InterfaceAnimate(void) {
    s32 i;
    u8 busy = 0;
    u8 (*const *fn)(u8);

    if (D_80110020) {
        for (i = 1, fn = D_80011BF4; i < 8; fn++, i++) {
            if (i == D_8010F8AC) {
                busy |= (*fn)(1);
            } else {
                busy |= (*fn)(0);
            }
        }
        if (!busy) {
            D_80110020 = 0;
            gInterfaceCurrentMode = D_8010F8AC;
        }
    }
    func_8003E13C();
    return busy;
}

void InterfaceSetRadarWorldRadius(s32 radius) {
    D_8010FFF0 = 0x18000 / (radius << 5);
    D_8010FFF4 = -0x14000 / (radius << 5);
}

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011C10);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011C18);

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011D10);

/* The status meter quads (armor, health, target, danger), flat-shaded. */
typedef struct IfaceMeter {
    /* 0x00 */ s32 link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u32 tag;
    /* 0x0C */ IfaceColor color;
    /* 0x10 */ IfacePoint xy[4];
    /* 0x20 */ s32 unk20;
} IfaceMeter;

extern IfaceMeter D_801175CC[4];

static inline void iPointAdd(IfacePointW *pt, IfacePointW *d) {
    pt->p.vx += d->p.vx;
    pt->p.vy += d->p.vy;
}

/* Offsets a packed screen point to the radar centre. */
static inline s32 iRadarXY(s32 xy) {
    IfacePointW pt;
    IfacePointW center;

    center.w = 0x50FF67;
    pt.w = xy;
    iPointAdd(&pt, &center);
    {
        u8 unused[8]; /* leftover: inlined, it still takes 8 bytes of the caller's frame */
    }
    return pt.w;
}

#ifdef NON_MATCHING
/* loop induction variables and the frame (0x88 vs 0x90) differ throughout */
INCLUDE_PART1("asm/main/nonmatchings/interface", InterfaceSetup)

extern const VECTOR32 D_80011D2C;
extern IfaceSprite D_801174E8;          /* radar background sprite */
extern char D_80117FFC[];               /* rounds text sprites */
extern char D_8011765C[], D_80117764[], D_8011786C[]; /* meter caption buffers */
extern const IfacePoint D_80011BE0[];   /* meter origins */
extern const s32 D_80011BD0[];          /* meter colours */
void EGSDeviceGraphicsGetResolution(u16 *w, u16 *h);
void EGSCameraCreate(s32 arg0, VECTOR32 *pos, s32 arg2, void *arg3, s32 arg4, s32 arg5, s32 arg6, void **out);
void EGSDisplayAddSprite(void *window, IfaceSprite *sprite);
void EGSDisplayBuildPoly3F(void *prim, s32 color, s32 xy0, s32 xy1, s32 xy2);
void EGSDisplayBuildPoly3G(void *prim, s32 c0, s32 xy0, s32 xy1, s32 xy2, s32 c1, s32 c2);
void EGSDisplayBuildPoly4G(void *prim, s32 c0, s32 xy0, s32 xy1, s32 xy2, s32 xy3, s32 c1, s32 c2, s32 c3);
void LoadImage(IfaceRect *rect, void *pixels);
void SniperInit(void);
void SpriteInitTemporary(void);
void SpriteSuperSetXY(IfaceSprite *sprites, s32 count, s32 x, s32 y);
void TextSpriteInitialize(void *buf, s32 count);
s32 InterfaceSetMode(s32 mode);
void InterfaceSetRadarWorldRadius(s32 radius);
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);

#define OFFSCREEN 0x04000400

void InterfaceSetup(void) {
    VECTOR32 pos;
    u16 w;
    u16 h;
    s32 i;
    s32 j;
    IfaceSprite *spr;
    s32 n;

    pos = D_80011D2C;
    D_8010F88C = 0;
    D_80117490.vx = 0;
    D_80117490.vy = 0;
    D_80117490.vz = 0;
    EGSDeviceGraphicsGetResolution(&w, &h);
    EGSCameraCreate(0, &pos, 1, D_8010F8C8, 3, 3, 0x12C, &gInterfaceWindow);
    ((u16 *)gInterfaceWindow)[3] |= 1;
    for (i = 1; i < 23; i++) {
        spr = SpriteGet(D_80011BA0[i].first);
        n = D_80011BA0[i].count;
        SpriteSuperSetXY(spr, n, 400, 400);
        SpriteCreateMulti(&D_80117514[i], n, spr);
        for (j = 0; j < n; j++) {
            spr[j].unk24 = 0;
            spr[j].unk28 = 0;
        }
    }
    D_801174E8 = *SpriteGet(0);
    D_801174E8.unk24 = 7;
    EGSTextureSetSpriteTransparency(&D_801174E8, 1);
    EGSDisplayAddSprite(gInterfaceWindow, &D_801174E8);
    D_801174E8.h = 1;
    D_801174E8.w = 1;

    for (i = 0; i < 2; i++) {
        EGSDisplayBuildPoly4F(&D_80117C70[i], 0x503028, OFFSCREEN, OFFSCREEN, OFFSCREEN, OFFSCREEN);
        D_80117C70[i].unk04 = 3;
        SET_SEMI_TRANS(&D_80117C70[i].tag);
        D_80117C70[i].link = 0;
    }
    EGSDisplayBuildPoly3G(&D_80117D00, 0, 0x50FF67, OFFSCREEN, OFFSCREEN, 0x503028, 0x503028);
    D_80117D00.unk04 = 4;
    D_80117D00.link = 0;
    SET_SEMI_TRANS(&D_80117D00.tag);
    for (i = 0; i < 5; i++) {
        EGSDisplayBuildLineF(&D_80117D24[i], 0xE77E6B, OFFSCREEN, OFFSCREEN);
        D_80117D24[i].unk04 = 2;
        SET_SEMI_TRANS(&D_80117D24[i].tag);
        D_80117D24[i].link = 0;
    }
    for (i = 0; i < 2; i++) {
        EGSDisplayBuildPoly4F(&D_80117CB8[i], 0xE77E6B, OFFSCREEN, OFFSCREEN, OFFSCREEN, OFFSCREEN);
        D_80117CB8[i].link = 0;
        SET_SEMI_TRANS(&D_80117CB8[i].tag);
    }
    for (i = 0; i < 4; i++) {
        EGSDisplayBuildLineF(&D_80117D9C[i], 0xE77E6B, OFFSCREEN, OFFSCREEN);
        D_80117D9C[i].unk04 = 2;
        SET_SEMI_TRANS(&D_80117D9C[i].tag);
        D_80117D9C[i].link = 0;
    }
    for (i = 0; i < 6; i++) {
        EGSDisplayBuildPoly4F(&D_80117A30[i], 0, OFFSCREEN, OFFSCREEN, OFFSCREEN, OFFSCREEN);
        D_80117A30[i].link = 0;
        D_80117B2C[i] = -1;
        EGSDisplayBuildPoly4G(&D_80117B38[i], 0x400040, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF);
        D_80117B38[i].unk04 = 3;
        SET_SEMI_TRANS(&D_80117B38[i].tag);
        D_80117C58[i] = 0;
    }
    EGSDisplayBuildPoly4F(&D_80117B08, 0xFFFFFF, OFFSCREEN, OFFSCREEN, OFFSCREEN, OFFSCREEN);
    D_80117B08.unk04 = 1;
    D_80117B08.link = 0;
    EGSDisplayBuildPoly3G(&D_801174A0, 0xFEC4C4, iRadarXY(0), 0, 0, 0x75583E, 0x75583E);
    D_801174A0.link = 0;
    EGSDisplayBuildPoly3G(&D_801174C4, 0xFEC4C4, 0, 0, 0, 0x75583E, 0x75583E);
    D_801174C4.link = 0;
    D_8010FFEC = 0;
    SniperInit();
    SpriteInitTemporary();
    D_80110008 = 0x7FFFFFFF;
    D_8011000C = 0x7FFFFFFF;
    D_80110000 = 0;
    TextSpriteInitialize(D_80117FFC, 6);
    D_8010F890 = TextAddPersistentString(D_8010F8D0, D_80117FFC, 400, 400);
    D_8010F890 = TextReplaceString(D_8010F890, D_8010F874);
    TextSetStringIntensity(D_8010F890, 0xD0, 0xD0, 0xD0);
    D_8010F892 = 0xFFFF;
    TextSpriteInitialize(D_80118104, 8);
    TextSpriteInitialize(D_8011765C, 0x13);
    D_8010F880 = 0xFFFF;
    D_8010F882 = 0xFFFF;
    D_8010F884 = 0xFFFF;
    for (i = 0; i < 4; i++) {
        s32 top = (u16)D_80011BE0[i].vx | (D_80011BE0[i].vy << 16);
        s32 bottom = (u16)D_80011BE0[i].vx | ((D_80011BE0[i].vy + 4) << 16);

        EGSDisplayBuildPoly4F(&D_801175CC[i], D_80011BD0[i], top, top, bottom, bottom);
        D_801175CC[i].color.c[3] |= 2;
        D_801175CC[i].unk04 = 1;
        D_801175CC[i].link = 0;
        EGSDisplayAddPoly(gInterfaceWindow, &D_801175CC[i]);
        if (i > 0) {
            for (j = 0; j < 2; j++) {
                IfaceLine *line = &D_801179A0[(i - 1) * 2 + j];

                EGSDisplayBuildLineF(line, 0xE77E6B, OFFSCREEN, OFFSCREEN);
                D_801179A0[i].link = 0;
                EGSDisplayAddPoly(gInterfaceWindow, line);
                SET_SEMI_TRANS(&line->tag);
                line->unk04 = 0;
            }
        }
    }
    for (i = 0; i < 36; i++) {
        EGSDisplayBuildLineF(&D_80118264[i], 0x106010, OFFSCREEN, OFFSCREEN);
        SET_SEMI_TRANS(&D_80118264[i].tag);
    }
    for (i = 0; i < 26; i++) {
        EGSDisplayBuildPoly4F(&D_801185C4[i], 0x40FF40, 0, 0, 0, 0);
    }
    for (i = 0; i < 2; i++) {
        EGSDisplayBuildPoly3F(&((u8 *)D_8011896C)[i * 0x1C], 0x106010, 0, 0, 0);
    }
    for (i = 0x2D; i < 0x3B; i++) {
        spr = SpriteGet(i);
        EGSTextureSetSpriteTransparency(SpriteGet(i), 1);
        spr->b = 0x40;
        spr->g = 0x40;
        spr->r = 0x40;
        spr->unk28 = 0;
    }
    D_8010F8B8 = NULL;
    D_8010F8BC = SpriteGet(0x33);
    InterfaceSetMode(0);
    D_80110020 = 1;
    for (i = 7; i >= 0; i--) {
        D_80105F30[i - 1] = -1;
    }
    D_8010F8C0 = -5;
    InterfaceSetAddLevelSpecificNPCCallback(NULL);
    InterfaceSetRadarWorldRadius(0x3C);
    SysPostMessage(0xE, 4, -2, -2, 0, 0, 0, 0);
    LoadImage(&D_8010FFF8, D_80117DFC);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceSetup);
#endif

void func_8003C3F4(void) {
    s32 phase = (u32)gSystemTick % 6;
    s32 prev = 5;
    s32 i;
    IfacePoly30 *blip;

    if (phase != 0) {
        prev = phase - 1;
    }
    for (i = 0, blip = D_80117B38; i < 6; blip++, i++) {
        if (phase == i && D_80117C58[i] >= 0xE66) {
            if (blip->link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, blip);
            }
        } else if (i != prev && blip->link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, blip);
        }
    }
}

/* Steps each channel of colour from toward to by at most 10. */
static inline s32 iFadeColor(s32 from, s32 to) {
    IfaceColor cur;
    IfaceColor want;
    s32 c;

    want.w = to;
    cur.w = from;
    for (c = 0; c < 3; c++) {
        if (want.c[c] < cur.c[c]) {
            want.c[c] = (want.c[c] < cur.c[c] - 10) ? cur.c[c] - 10 : want.c[c];
        } else {
            want.c[c] = (cur.c[c] + 10 < want.c[c]) ? cur.c[c] + 10 : want.c[c];
        }
    }
    return want.w;
}

/* k/256 of colour a plus the rest of b, per channel. */
static inline s32 iBlendColor(IfaceColor *a, IfaceColor *b, s32 k) {
    u8 unused[8];
    u8 mix[4];
    s32 i;

    mix[3] = 0;
    for (i = 0; i < 3; i++) {
        mix[i] = (k * a->c[i] + (0x100 - k) * b->c[i]) >> 8;
    }
    return *(s32 *)mix;
}

/* Clamps each channel of *want to within 10 of *cur (a fade step from *cur toward *want). */
static inline void iStepColor(IfaceColor *cur, IfaceColor *want) {
    s32 c;

    for (c = 0; c < 3; c++) {
        if (want->c[c] < cur->c[c]) {
            want->c[c] = (want->c[c] < cur->c[c] - 10) ? cur->c[c] - 10 : want->c[c];
        } else {
            s32 v = want->c[c];
            if (cur->c[c] + 10 < v) {
                v = cur->c[c] + 10;
            }
            want->c[c] = v;
        }
    }
}

void func_8003C4F8(s32 slot, s32 level) {
    IfacePoly24 *blip;
    s32 k;
    IfaceColor cur;
    IfaceColor want;

    D_80117C58[slot] = level;
    blip = (IfacePoly24 *)&D_80117A30[slot];
    if (level < 0) {
        level = 0xFA6464;
    } else if (level < 0x553) {
        level *= 0x300;
        k = level >> 12;
        if (level < 0) {
            k = -(-level >> 12);
        }
        level = (k & 0xFF) | 0xFF00;
    } else if (level < 0xAA7) {
        level = (level - 0x553) * 0x300;
        k = level >> 12;
        if (level < 0) {
            k = -(-level >> 12);
        }
        level = ((~k & 0xFF) << 8) | 0xFF;
    } else {
        level = 0xFF;
    }
    cur.w = blip->color;
    want.w = level;
    iStepColor(&cur, &want);
    level = want.w;
    blip->color = level | 0x28000000;
    if (gObjPlayer->target->targetId == D_80117B2C[slot] && gTargetModeFlag) {
        cur.w = level;
        want.w = 0x503028;
        {
            u8 unused[8];
            u8 mix[4];
            u8 *m = mix;
            s32 t;
            s32 i;

            t = (rcos(gSystemTick << 8) + 0x1000) >> 5;
            m[3] = 0;
            for (i = 0; i < 3; i++) {
                m[i] = (t * cur.c[i] + (0x100 - t) * want.c[i]) >> 8;
            }
            D_80117B14 = *(s32 *)mix | 0x28000000;
        }
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011D3C);


int rsin(int a);

#ifdef NON_MATCHING
/* corner loop and switch match; the threat-arrow block hoists all four +-24/+-20 constants into s-registers (original keeps only 24 in s5) and the frame is 0x60 instead of 0x78 */
/* Radar blip for NPC slot at radar-space pos (vx, vy): corner box, target box, threat arrow. */
void func_8003C724(s32 slot, VECTOR32 *pos) {
    s32 w;
    s32 h;
    s32 x;
    s32 y;
    s32 n;
    s32 i;
    s32 d;
    s32 cx;
    s32 cy;
    s32 xy;

    if (D_80105F30[0] >= 12) {
        w = 24;
        h = 20;
    } else {
        w = D_80105F30[0] * 2;
        h = D_80105F30[0] * 20 / 12;
    }
    x = pos->vx;
    y = pos->vy;
    if (x > w) {
        x = w;
    } else if (x < -w) {
        x = -w;
    }
    if (y > h) {
        y = h;
    } else if (y < -h) {
        y = -h;
    }
    n = 4;
    if (gObjPlayer->target->targetId == D_80117B2C[slot] && gTargetModeFlag) {
        n = 8;
    }
    for (i = 0; i < n; i++) {
        d = 1;
        if (i >= 4) {
            d = 2;
        }
        if (i & 1) {
            cx = x + d;
            if (w < cx) {
                cx = w;
            }
        } else if (x - d < -w) {
            cx = -w;
        } else {
            cx = x - d;
        }
        if (i & 2) {
            cy = y + d;
            if (h < cy) {
                cy = h;
            }
        } else if (y - d < -h) {
            cy = -h;
        } else {
            cy = y - d;
        }
        xy = iRadarXY(XY(cx, cy));
        switch (i) {
        case 0:
            D_80117A30[slot].xy[0] = xy;
            break;
        case 1:
            D_80117A30[slot].xy[1] = xy;
            break;
        case 2:
            D_80117A30[slot].xy[2] = xy;
            break;
        case 3:
            D_80117A30[slot].xy[3] = xy;
            break;
        case 4:
            D_80117B08.xy[0] = xy;
            break;
        case 5:
            D_80117B08.xy[1] = xy;
            break;
        case 6:
            D_80117B08.xy[2] = xy;
            break;
        case 7:
            D_80117B08.xy[3] = xy;
            break;
        }
    }
    if (D_80117C58[slot] >= 0xE66) {
        IfacePoint p[3];
        IfacePoint *pp;
        s32 angle;
        s32 a;
        s32 s;
        s32 c;
        s32 kx;
        s32 ky;

        angle = ratan2(y, x);
        D_80117B38[slot].xy0 = iRadarXY(0);
        for (pp = p; pp < &p[2]; pp++) {
            if (pp != p) {
                a = angle - 0xAA;
            } else {
                a = angle + 0xAA;
            }
            s = rsin(a);
            c = rcos(a);
            if (s == 0) {
                pp->vy = 0;
                pp->vx = c > 0 ? 24 : -24;
            } else if (c == 0) {
                pp->vx = 0;
                pp->vy = s > 0 ? 20 : -20;
            } else {
                kx = 0x18000 / c;
                ky = 0x14000 / s;
                if (kx < 0) {
                    kx = -kx;
                }
                if (ky < 0) {
                    ky = -ky;
                }
                if (kx < ky) {
                    pp->vy = FIX12(s * 31);
                    pp->vx = c > 0 ? 24 : -24;
                } else {
                    pp->vx = FIX12(c * 31);
                    pp->vy = s > 0 ? 20 : -20;
                }
            }
        }
        if (p[0].vx == p[1].vx || p[0].vy == p[1].vy) {
            p[2].vx = (p[0].vx + p[1].vx) / 2;
            p[2].vy = (p[0].vy + p[1].vy) / 2;
        } else {
            p[2].vx = ABS(p[0].vx) < ABS(p[1].vx) ? p[1].vx : p[0].vx;
            p[2].vy = ABS(p[0].vy) < ABS(p[1].vy) ? p[1].vy : p[0].vy;
        }
        D_80117B38[slot].xy1 = iRadarXY(XY(p[0].vx, p[0].vy));
        D_80117B38[slot].xy2 = iRadarXY(XY(p[1].vx, p[1].vy));
        D_80117B38[slot].xy3 = iRadarXY(XY(p[2].vx, p[2].vy));
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003C724);
#endif

void iInterfaceRadarRemove(void) {
    s32 i;
    IfacePoly24 *p;
    IfacePoly30 *blip;

    EGSDisplayDeletePoly(gInterfaceWindow, &D_80117C70[0]);
    EGSDisplayDeletePoly(gInterfaceWindow, &D_80117C70[1]);
    EGSDisplayDeletePoly(gInterfaceWindow, &D_80117D00);
    for (i = 0; i < 5; i++) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_80117D24[i]);
    }
    for (i = 0, blip = D_80117B38, p = (IfacePoly24 *)D_80117A30; i < 6; i++, p++) {
        if (p->link) {
            EGSDisplayDeletePoly(gInterfaceWindow, p);
        }
        if (blip->link) {
            EGSDisplayDeletePoly(gInterfaceWindow, blip);
        }
        blip++;
    }
    for (i = 0, p = D_80117CB8; i < 2; i++, p++) {
        if (p->link) {
            EGSDisplayDeletePoly(gInterfaceWindow, p);
        }
    }
    if (D_80117B08.link) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_80117B08);
    }
    EGSDisplayDeletePoly(gInterfaceWindow, &D_801174A0);
    if (D_801174C4.link) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_801174C4);
    }
    mBombPoly.xy[0] = 0x04000400;
    mBombPoly.xy[1] = 0x04000400;
    mBombPoly.xy[2] = 0x04000400;
    mBombPoly.xy[3] = 0x04000400;
}

void InterfaceRadarTurnOff(void) {
    if (D_8010F887) {
        D_8010F887 = 0;
        iInterfaceRadarRemove();
    }
}

void iInterfaceRadarAdd(void) {
    s32 i;
    IfacePoly24 *p;

    EGSDisplayAddPoly(gInterfaceWindow, &D_80117C70[0]);
    EGSDisplayAddPoly(gInterfaceWindow, &D_80117C70[1]);
    EGSDisplayAddPoly(gInterfaceWindow, &D_80117D00);
    for (i = 0; i < 5; i++) {
        EGSDisplayAddPoly(gInterfaceWindow, &D_80117D24[i]);
    }
    if (D_801174A0.link == 0) {
        EGSDisplayAddPoly(gInterfaceWindow, &D_801174A0);
    }
    for (i = 0, p = D_80117CB8; i < 2; i++, p++) {
        if (p->link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, p);
        }
    }
    if ((D_80117490.vx != 0 || D_80117490.vy != 0 || D_80117490.vz != 0) && D_801174C4.link == 0) {
        EGSDisplayAddPoly(gInterfaceWindow, &D_801174C4);
    }
}

void InterfaceRadarTurnOn(void) {
    if (!D_8010F887) {
        D_8010F887 = 1;
        iInterfaceRadarAdd();
    }
}

u8 InterfaceRadarIsOn(void) {
    return D_8010F887;
}

void func_8003D0E8(s32 polyColor, s32 lineColor, s32 textColor) {
    s32 i;
    u8 r;
    u8 g;
    u8 b;

    polyColor &= 0xFFFFFF;
    for (i = 0; i < 2; i++) {
        D_80117C70[i].color.word = polyColor | 0x28000000;
        SET_SEMI_TRANS(&D_80117C70[i].tag);
    }
    lineColor &= 0xFFFFFF;
    D_80117D00.c0 = 0x30000000;
    D_80117D00.c1 = polyColor;
    D_80117D00.c2 = polyColor;
    SET_SEMI_TRANS(&D_80117D00.tag);
    for (i = 0; i < 5; i++) {
        D_80117D24[i].color.word = lineColor | 0x40000000;
        SET_SEMI_TRANS(&D_80117D24[i].tag);
    }
    for (i = 0; i < 4; i++) {
        D_80117D9C[i].color.word = lineColor | 0x40000000;
        SET_SEMI_TRANS(&D_80117D9C[i].tag);
    }
    for (i = 0; i < 6; i++) {
        D_801179A0[i].color.word = lineColor | 0x40000000;
        SET_SEMI_TRANS(&D_801179A0[i].tag);
    }
    for (i = 0; i < 2; i++) {
        D_80117CB8[i].color = lineColor | 0x28000000;
        SET_SEMI_TRANS(&D_80117CB8[i].tag);
    }
    *(s32 *)&D_8010F8B0 = textColor;
    r = ((u8 *)&textColor)[0];
    g = ((u8 *)&textColor)[1];
    b = ((u8 *)&textColor)[2];
    if (D_8010F880 != 0xFFFF) {
        TextSetStringIntensity(D_8010F880, r, g, b);
    }
    if (D_8010F882 != 0xFFFF) {
        TextSetStringIntensity(D_8010F882, r, g, b);
    }
    if (D_8010F884 != 0xFFFF) {
        TextSetStringIntensity(D_8010F884, r, g, b);
    }
    TextSetStringIntensity(D_8010F890, r, g, b);
    D_8010F894 = 5;
}

void InterfaceRemoveNPC(s32 id) {
    IfaceNpc *npc = (IfaceNpc *)gameObjectsInfo[id].runtime->npc;
    s32 slot;

    if (npc != NULL) {
        slot = npc->radarSlot;
        if (D_80117B2C[slot] != -1) {
            D_80117B2C[slot] = -1;
        }
        D_80117C58[slot] = 0;
    }
}

void InterfaceViralRadarInit(void) {
    EGSDisplayBuildPoly4F(&mBombPoly, 0, 0x04000400, 0x04000400, 0x04000400, 0x04000400);
    mBombPoly.link = 0;
    mBombPoly.unk04 = 2;
    EGSDisplayAddPoly(gInterfaceWindow, &mBombPoly);
    D_8010F88C = 1;
}

void InterfaceAddBomb(s32 id) {
    Gobj *obj = gameObjectsInfo[id].runtime;

    mBombPosition.vx = obj->anim->frame->pos.vx;
    mBombPosition.vy = obj->anim->frame->pos.vy;
    mBombPosition.vz = obj->anim->frame->pos.vz;
    mBombPosition.vy = -mBombPosition.vy;
    if (mBombPosition.vx == 0 && mBombPosition.vy == 0 && mBombPosition.vz == 0) {
        mBombPosition.vy = 1;
    }
    mBombPosition.pad = id;
}

void InterfaceDeleteBomb(void) {
    mBombPoly.xy[0] = mBombPoly.xy[1] = mBombPoly.xy[2] = mBombPoly.xy[3] = 0x04000400;
    mBombPosition.vx = 0;
    mBombPosition.vy = 0;
    mBombPosition.vz = 0;
}

void func_8003D53C(VECTOR32 *pos) {
    IfacePointW pt;
    IfacePointW center;
    u32 f;
    s32 c;
    s32 col;

    if (pos != NULL) {
        if ((pos->vx < 0 ? -pos->vx : pos->vx) < 24 && (pos->vy < 0 ? -pos->vy : pos->vy) < 20) {
            center.w = 0x50FF67;
            f = ~gSystemTick & 7;
            c = f << 5;
            col = ((c | 0x1F) << 16) | ((c & 0xFF) << 8) | (c & 0xFF);
            mBombPoly.color.word = col | 0x28000000;
            pt.w = (u16)pos->vx | ((u16)pos->vy << 16);
            iPointAdd(&pt, &center);
            {
                u8 unused[8]; /* frame is 0x28: 8 bytes beyond pt/center (cf. iRadarXY) */
            }
            /* passed as the word-aligned union (one lw from the stack); with the IfacePoint
             * prototype the struct goes in as two halves. menu.c declares the parameter s32. */
            ((void (*)(IfacePoly4 *, IfacePointW, s32, s32))InterfacePlaceQuad)(&mBombPoly, pt, f >> 1, f >> 1);
            mBombPoly.color.c[3] |= 2;
            return;
        }
    }
    mBombPoly.xy[0] = 0x04000400;
    mBombPoly.xy[1] = 0x04000400;
    mBombPoly.xy[2] = 0x04000400;
}

void InterfaceAddNPC(s32 id) {
    D_80117B2C[((IfaceNpc *)gameObjectsInfo[id].runtime->npc)->radarSlot] = id;
    if (gLevelSpecificNpcName != NULL && id == gLevelSpecificNpcID) {
        TextAddMarker(id, gLevelSpecificNpcName, 60, 0);
        if (D_8010F8C4 != NULL) {
            D_8010F8C4((s16)id);
        }
    }
    if (gLevelSpecificNpc2Name != NULL && id == gLevelSpecificNpcID2) {
        TextAddMarker(id, gLevelSpecificNpc2Name, 60, 0);
        if (D_8010F8C4 != NULL) {
            D_8010F8C4((s16)id);
        }
    }
}

typedef struct IfaceRows {
    IfaceSV3 r[3];
} IfaceRows;

typedef struct IfaceT3 {
    s32 t[3];
} IfaceT3;

extern IfaceSV3 D_8010F8D8;            /* radar scale matrix, last row */
extern const IfaceT3 D_80011D3C;
extern u8 copKilledByPCFlag;
void MatrixNormal_0(GOBJ_MATRIX *in, GOBJ_MATRIX *out);
GOBJ_MATRIX *TransposeMatrix(GOBJ_MATRIX *m0, GOBJ_MATRIX *m1);
GOBJ_MATRIX *MulMatrix0(GOBJ_MATRIX *m0, GOBJ_MATRIX *m1, GOBJ_MATRIX *m2);
void func_8003E894(GOBJ_MATRIX *m);
extern s32 D_80110004; /* danger, 0x1000 = none */
void func_8003C3F4(void);

/* The radar's scale matrix (x by D_8010FFF0, z by D_8010FFF4). */
static inline void iRadarScale(GOBJ_MATRIX *out) {
    GOBJ_MATRIX tmp;
    IfaceRows rows;
    IfaceSV3 r0;
    IfaceSV3 r1;

    r0.vy = 0;
    r0.vz = 0;
    r0.vx = D_8010FFF0;
    rows.r[0] = r0;
    r1.vx = 0;
    r1.vy = 0;
    r1.vz = D_8010FFF4;
    rows.r[1] = r1;
    rows.r[2] = D_8010F8D8;
    *(IfaceRows *)tmp.m = rows;
    *(IfaceT3 *)tmp.t = D_80011D3C;
    *out = tmp;
}

static inline s32 iXYAdd(s32 xy, s32 d) {
    IfacePointW pt;
    IfacePointW delta;

    delta.w = d;
    pt.w = xy;
    iPointAdd(&pt, &delta);
    return pt.w;
}

#ifdef NON_MATCHING
/* loop: the box/colour addresses get hoisted into s4-s6; a self-copy of m.t before func_8003E894 is missing */
/* Radar update: NPC boxes and blips (with the danger level they add), the target box and
 * the bomb pointer, all in the player's frame scaled to the radar. */
void func_8003D76C(void) {
    VECTOR32 pos;
    GOBJ_MATRIX m;
    GOBJ_MATRIX scale;
    IfacePoly4 *box;
    s32 i;
    s32 id;
    s32 type;
    s32 level;
    Gobj *obj;

    iRadarScale(&scale);
    m.m[0][0] = (*(EGSFrame **)*gCamera)->m[0][0];
    m.m[0][1] = -(*(EGSFrame **)*gCamera)->m[0][1];
    m.m[0][2] = (*(EGSFrame **)*gCamera)->m[0][2];
    m.m[1][0] = -(*(EGSFrame **)*gCamera)->m[1][0];
    m.m[1][1] = (*(EGSFrame **)*gCamera)->m[1][1];
    m.m[1][2] = -(*(EGSFrame **)*gCamera)->m[1][2];
    m.m[2][0] = (*(EGSFrame **)*gCamera)->m[2][0];
    m.m[2][1] = -(*(EGSFrame **)*gCamera)->m[2][1];
    m.m[2][2] = (*(EGSFrame **)*gCamera)->m[2][2];
    m.t[0] = (*(EGSFrame **)*gCamera)->pos.vx;
    m.t[1] = -(*(EGSFrame **)*gCamera)->pos.vy;
    m.t[2] = (*(EGSFrame **)*gCamera)->pos.vz;
    /* keep only the heading */
    m.m[2][1] = 0;
    m.m[1][2] = 0;
    m.m[1][0] = 0;
    m.m[0][1] = 0;
    m.m[1][1] = 0x1000;
    MatrixNormal_0(&m, &m);
    TransposeMatrix(&m, &m);
    func_8003E894(&m);
    MulMatrix0(&scale, &m, &m);
    for (i = 0, box = D_80117A30; i < 6; i++, box++) {
        id = D_80117B2C[i];
        if (id == -1) {
            if (box->link != 0) {
                if (box->color.word != 0x28503028) {
                    box->color.word = iFadeColor(box->color.word, 0x503028) | 0x28000000;
                } else {
                    EGSDisplayDeletePoly(gInterfaceWindow, box);
                }
            }
            continue;
        }
        obj = gameObjectsInfo[id].runtime;
        type = GOBJ_TYPE(id);
        if ((type == 0x35 && !copKilledByPCFlag) || type == 0x4C || type == 0x5C || type == 0x65) {
            level = -1;
        } else {
            level = obj->target->threat;
            if ((u32)level > 0x1000) {
                level = (level > 0x1000) << 12;
            }
            if (obj->target->targetId != gPlayerOffset) {
                level = 0;
            }
        }
        pos.vx = obj->move->pos.vx - gObjPlayer->move->pos.vx;
        pos.vy = obj->move->pos.vy - gObjPlayer->move->pos.vy;
        pos.vz = obj->move->pos.vz - gObjPlayer->move->pos.vz;
        ApplyMatrixLV(&m, &pos, &pos);
        func_8003C724(i, &pos);
        if (type == 0x35) {
            func_8003C4F8(i, -1);
            D_80117C58[i] = level;
        } else {
            func_8003C4F8(i, level);
        }
        D_80110004 = EGSMathFIXMul(D_80110004, 0x1000 - level);
        if (box->link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, box);
            box->color.word = 0x28FFFFFF;
            box->xy[0] = iXYAdd(box->xy[0], XY(-2, -2));
            box->xy[1] = iXYAdd(box->xy[1], XY(2, -2));
            box->xy[2] = iXYAdd(box->xy[2], XY(-2, 2));
            box->xy[3] = iXYAdd(box->xy[3], XY(2, 2));
        }
    }
    if (gTargetModeFlag && gObjPlayer->target->targetId >= 0) {
        if (D_80117B08.link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, &D_80117B08);
        }
    } else if (D_80117B08.link != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_80117B08);
    }
    if (D_8010F88C != 0) {
        if (mBombPosition.vx != 0 || mBombPosition.vy != 0 || mBombPosition.vz != 0) {
            pos.vx = mBombPosition.vx - gObjPlayer->move->pos.vx;
            pos.vy = mBombPosition.vy - gObjPlayer->move->pos.vy;
            pos.vz = mBombPosition.vz - gObjPlayer->move->pos.vz;
            ApplyMatrixLV(&m, &pos, &pos);
            func_8003D53C(&pos);
        } else {
            func_8003D53C(NULL);
        }
    }
    func_8003C3F4();
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003D76C);
#endif

#ifdef NON_MATCHING
/* delay-slot/regalloc: the hide constant lands in the first branch's delay slot in the original */
s32 func_8003DF74(s32 idx, u8 grow) {
    const IfacePoint *pt = &D_80011BE4[idx];
    s32 w;
    s32 x;
    s32 done;

    idx *= 2;
    if (grow || D_801179A0[idx].xy0 != D_801179A0[idx].xy1) {
        w = (s16)D_801179A0[idx].xy1 - (s16)D_801179A0[idx].xy0;
        if (grow) {
            if (D_801179A0[idx].xy0 == D_801179A0[idx].xy1) {
                x = (pt->vx - 3) & 0xFFFF;
                D_801179A0[idx + 1].xy0 = x | (pt->vy << 16);
                D_801179A0[idx + 1].xy1 = x | ((pt->vy + 4) << 16);
                D_801179A0[idx].xy0 = x | ((pt->vy + 5) << 16);
            }
            w += 8;
            if (w > 52) {
                w = 53;
                done = 1;
            } else {
                done = 0;
            }
        } else {
            w -= 8;
            if (w < 0) {
                w = 0;
            }
            done = 0;
        }
        D_801179A0[idx].xy1 = ((pt->vx + (w - 3)) & 0xFFFF) | ((pt->vy + 5) << 16);
        return done;
    } else {
        D_801179A0[idx + 1].xy1 = 0x04000400;
        D_801179A0[idx + 1].xy0 = 0x04000400;
        D_801179A0[idx].xy1 = 0x04000400;
        D_801179A0[idx].xy0 = 0x04000400;
        return 1;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003DF74);
#endif

extern const u8 D_80011C10[];
extern const s32 D_80011BD4, D_80011BD8, D_80011BDC; /* meter colours */
extern char *gHealthMsg, *gArmorMsg, *gTargetMsg, *gDangerMsg;

/* Moves a meter quad's left or right edge (xy[0]/xy[2] or xy[1]/xy[3]). */
static inline void iMeterEdge(IfacePoint *xy, s32 top, s32 bottom) {
    xy[0].vx = top;
    xy[2].vx = bottom;
}

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
/* cur moved toward want by at most 5 */
#define IFACE_STEP(n, want, cur) \
    n = MAX(want, (cur) - 5);     \
    n = MIN(n, (cur) + 5)

#ifdef NON_MATCHING
/* regalloc: health lands in s0 not s1; 6 insns longer overall */
/* Status meters: armor/health, target health and danger, each with its caption. The bars
 * move at most 5 pixels a frame toward their value; an empty armor bar or a full danger
 * bar pulses. */
void func_8003E13C(void) {
    IfaceColor from;
    IfaceColor to;
    char *msg;
    s32 health;
    s32 armor;
    s32 armorShown;
    s32 healthShown;
    s32 k;
    s32 level;
    s32 shown;
    u16 str;

    msg = NULL;
    if (D_80011C10[D_8010F8AC] == 0) {
        GameObjectHealth *h = gObjPlayer->healthInfo;

        armor = h->armor;
        armor /= 12;
        health = h->health;
        health /= 3;
        if (armor == 0) {
            from.w = 0xFF;
            to.w = 0;
            k = iBlendColor(&from, &to, (rcos(gSystemTick << 8) + 0x1000) >> 5);
            msg = gHealthMsg;
            D_801175CC[1].color.w = k | 0x28000000;
        } else {
            D_801175CC[1].color.w = D_80011BD4 | 0x28000000;
            msg = gArmorMsg;
            D_801175CC[1].color.c[3] |= 2;
        }
    } else {
        health = 0;
        armor = 0;
    }
    armorShown = D_801175CC[0].xy[1].vx - D_801175CC[0].xy[0].vx;
    healthShown = D_801175CC[1].xy[1].vx - D_801175CC[0].xy[0].vx;
    if (msg != NULL) {
        str = D_8010F880;
        if (str != 0xFFFF) {
            if (armorShown > 0 && armor == 0) {
                D_8010F880 = TextReplaceString(str, msg);
            } else if (armorShown == 0 && armor > 0) {
                D_8010F880 = TextReplaceString(D_8010F880, msg);
            }
        } else if (health > 0) {
            D_8010F880 = TextAddPersistentString(msg, D_8011765C, -0xAC, -0x66);
            if (D_8010F880 != str) {
                TextSetStringIntensity(D_8010F880, D_8010F8B0, D_8010F8B1, D_8010F8B2);
            }
        }
    } else if (D_8010F880 != 0xFFFF) {
        TextRemoveString(D_8010F880);
        D_8010F880 = 0xFFFF;
    }
    func_8003DF74(0, health > 0);
    if (armor != armorShown) {
        s32 n;
        s32 top;
        s32 bottom;

        IFACE_STEP(n, armor, armorShown);
        top = D_801175CC[0].xy[0].vx + n;
        bottom = D_801175CC[0].xy[2].vx + n;
        iMeterEdge(&D_801175CC[1].xy[0], top, bottom);
        iMeterEdge(&D_801175CC[0].xy[1], top, bottom);
        health = MAX(health, n);
    } else {
        health = MAX(health, armor);
    }
    if (health != healthShown) {
        IFACE_STEP(health, health, healthShown);
        iMeterEdge(&D_801175CC[1].xy[1], D_801175CC[0].xy[0].vx + health, D_801175CC[0].xy[2].vx + health);
    }

    level = -1;
    if (D_80011C10[D_8010F8AC] == 0) {
        TargetInfo *target = gObjPlayer->target;

        if (target->targetId >= 0 && gTargetModeFlag) {
            level = target->unk58;
            if (level > 100) {
                level = 100;
            }
            level /= 2;
        }
    }
    str = D_8010F882;
    D_801175CC[2].color.w = D_80011BD8 | 0x28000000;
    D_801175CC[2].color.c[3] |= 2;
    shown = D_801175CC[2].xy[1].vx - D_801175CC[2].xy[0].vx;
    if (str == 0xFFFF) {
        if (level >= 0) {
            D_8010F882 = TextAddPersistentString(gTargetMsg, D_80117764, -0xAC, -0x48);
            if (D_8010F882 != str) {
                TextSetStringIntensity(D_8010F882, D_8010F8B0, D_8010F8B1, D_8010F8B2);
            }
        }
    } else if (level < 0) {
        TextRemoveString(str);
        D_8010F882 = 0xFFFF;
    }
    func_8003DF74(1, level != -1);
    if (level < 0) {
        level = 0;
    }
    if (level != shown) {
        IFACE_STEP(level, level, shown);
        iMeterEdge(&D_801175CC[2].xy[1], D_801175CC[2].xy[0].vx + level, D_801175CC[2].xy[2].vx + level);
    }

    level = 0;
    if (D_80011C10[D_8010F8AC] == 0) {
        level = 50 - FIX12(D_80110004 * 50);
        if (level < 3) {
            level = 0;
        } else if (level > 50) {
            level = 50;
        }
    }
    if (level >= 49) {
        from.w = 0;
        to.w = 0xFF;
        D_801175CC[3].color.w = iBlendColor(&from, &to, (rcos(gSystemTick << 8) + 0x1000) >> 5) | 0x28000000;
    } else {
        D_801175CC[3].color.w = D_80011BDC | 0x28000000;
        D_801175CC[3].color.c[3] |= 2;
    }
    str = D_8010F884;
    shown = D_801175CC[3].xy[1].vx - D_801175CC[3].xy[0].vx;
    if (str == 0xFFFF) {
        if (level > 0) {
            D_8010F884 = TextAddPersistentString(gDangerMsg, D_8011786C, -0xAC, -0x57);
            if (D_8010F884 != str) {
                TextSetStringIntensity(D_8010F884, D_8010F8B0, D_8010F8B1, D_8010F8B2);
            }
        }
    } else if (level == 0) {
        TextRemoveString(str);
        D_8010F884 = 0xFFFF;
    }
    func_8003DF74(2, level);
    if (level != shown) {
        IFACE_STEP(level, level, shown);
        iMeterEdge(&D_801175CC[3].xy[1], D_801175CC[3].xy[0].vx + level, D_801175CC[3].xy[2].vx + level);
    }
    D_80110004 = 0x1000;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003E13C);
#endif

/* Radar: the player's arrow (rotated into view space by m) and the compass letters. */
void func_8003E894(GOBJ_MATRIX *m) {
    VECTOR32 dir;
    s32 sx;
    s32 sz;
    s32 tipX;
    s32 tipY;
    s32 sideX;
    s32 sideY;
    s32 xy0;
    s32 xy1;
    s32 xy2;
    s32 angle;
    IfaceRect *r;
    IfacePoly4 *poly;
    s32 i;

    dir.vx = gObjPlayer->anim->frame->m[0][2];
    dir.vy = gObjPlayer->anim->frame->m[1][2];
    dir.vz = gObjPlayer->anim->frame->m[2][2];
    dir.vy = 0;
    GTEMath_VectorNormal(&dir, &dir);
    ApplyMatrixLV(m, &dir, &dir);
    sx = 1;
    if (dir.vx < 0) {
        sx = -1;
    }
    sz = 1;
    if (dir.vz < 0) {
        sz = -1;
    }
    tipX = (FIX12(dir.vx * 24) - sx) / 8;
    tipY = (FIX12(-dir.vz * 20) - sz) / 8;
    sideY = (FIX12(dir.vx * 20) - sx) / 6;
    sideX = (FIX12(-dir.vz * 24) - sz) / 6;
    xy0 = iRadarXY((tipX & 0xFFFF) | (tipY << 16));
    xy1 = iRadarXY(((sideX - tipX) & 0xFFFF) | ((-sideY - tipY) << 16));
    xy2 = iRadarXY(((-sideX - tipX) & 0xFFFF) | ((sideY - tipY) << 16));
    ((IfacePolyG3 *)&D_801174A0)->xy0 = xy0;
    ((IfacePolyG3 *)&D_801174A0)->xy1 = xy1;
    ((IfacePolyG3 *)&D_801174A0)->xy2 = xy2;


    dir.vx = m->m[0][2];
    dir.vy = m->m[1][2];
    dir.vz = m->m[2][2];
    angle = ratan2(dir.vz, dir.vx);
    angle += 0x100;
    if (angle < 0) {
        angle += 0x1000;
    }
    angle = FIX12(angle * 8);
    r = D_80105EAC[angle];
    poly = (IfacePoly4 *)D_80117CB8;
    for (i = 0; i < 2; i++, r++, poly++) {
        if (r->x == 0) {
            break;
        }
        poly->xy[0] = (r->x & 0xFFFF) | ((r->y & 0xFFFF) << 16);
        poly->xy[1] = ((r->x + r->w) & 0xFFFF) | ((r->y & 0xFFFF) << 16);
        poly->xy[2] = (r->x & 0xFFFF) | (((r->y + r->h) & 0xFFFF) << 16);
        poly->xy[3] = ((r->x + r->w) & 0xFFFF) | (((r->y + r->h) & 0xFFFF) << 16);
    }
    for (; i < 2; i++, poly++) {
        poly->xy[0] = poly->xy[1] = poly->xy[2] = poly->xy[3] = 0x04000400;
    }
}

s32 func_8003ECA0(big)
    u8 big;
{
    void *cam = *gCamera;
    s32 i;
    IfaceLine *line;

    if (D_801185C4[0].link) {
        return 0;
    }
    if (big) {
        for (i = 0, line = D_80118264; i < 36; i++, line++) {
            if (line->link == 0) {
                EGSDisplayAddPoly(cam, line);
            }
        }
        func_80041378(&D_80118264[0], 12, 0x40FF40, 0);
        func_80041378(&D_80118264[12], 8, 0x1EC01E, 0);
        func_80041378(&D_80118264[20], 8, 0x148014, 0);
        func_80041378(&D_80118264[28], 8, 0x0A400A, 0);
        D_8011031E = 1;
    } else {
        for (i = 0, line = D_80118264; i < 30; i++, line++) {
            if (line->link == 0) {
                EGSDisplayAddPoly(cam, line);
            }
        }
        func_80041378(&D_80118264[0], 12, 0x40FF40, 0);
        func_80041378(&D_80118264[12], 6, 0x1EC01E, 0);
        func_80041378(&D_80118264[18], 6, 0x148014, 0);
        func_80041378(&D_80118264[24], 6, 0x0A400A, 0);
        D_8011031E = 2;
    }
    return 1;
}

void iDeleteTargetCursor(void) {
    void *cam = *gCamera;
    s32 i;
    IfaceLine *line;

    if (D_8011031E) {
        for (i = 0, line = D_80118264; i < 36; i++, line++) {
            if (line->link) {
                EGSDisplayDeletePoly(cam, line);
            }
        }
        D_8011031E = 0;
    }
    D_8010FFEC = 0;
}

s32 func_8003EECC(s32 delta) {
    if (delta < 0) {
        if (D_8010FFEC == 0) {
            return 0;
        }
        D_8010FFEC += delta;
        if (D_8010FFEC <= 0) {
            iDeleteTargetCursor();
        }
    } else if (D_8010FFEC < 4) {
        D_8010FFEC += delta;
        if (D_8010FFEC > 4) {
            D_8010FFEC = 4;
        }
    }
    return D_8010FFEC;
}

void func_8003EF48(VECTOR32 *scr, s32 fresh) {
    IfaceLine *lines = D_80118264;
    s32 i;
    s32 size;
    s32 len;
    s32 half;
    s32 prod;
    s32 w;
    s32 t;

    for (i = 35; i >= 20; i--) {
        lines[i].xy0 = lines[i - 8].xy0;
        lines[i].xy1 = lines[i - 8].xy1;
        lines[i].unk04 = lines[i - 8].unk04 + 4;
    }
    if (fresh) {
        for (i = 20; i >= 12; i--) {
            lines[i].xy0 = lines[i - 8].xy0;
            lines[i].xy1 = lines[i - 8].xy1;
            lines[i].unk04 = lines[i - 8].unk04 + 4;
        }
    } else {
        for (i = 20; i >= 12; i--) {
            lines[i].xy1 = 0x04000400;
            lines[i].xy0 = 0x04000400;
        }
    }
    if (scr->vz > 0) {
        w = (*(IfaceCamera **)gCamera)->h * 60 / scr->vz;
        size = 4;
        if (w >= 4) {
            size = w;
        }
        w = size * D_8010FFEC / 4;
        half = w / 2;
        prod = w * D_8010FFEC;
        len = w;
        w = prod / 4;
        t = w / 3;
        lines[0].xy0 = XY(scr->vx - w * 2, scr->vy);
        lines[0].xy1 = XY(scr->vx - w, scr->vy);
        lines[1].xy0 = XY(scr->vx + w, scr->vy);
        lines[1].xy1 = XY(scr->vx + w * 2, scr->vy);
        lines[2].xy0 = XY(scr->vx, scr->vy - len);
        lines[2].xy1 = XY(scr->vx, scr->vy - half);
        lines[3].xy0 = XY(scr->vx, scr->vy + half);
        lines[3].xy1 = XY(scr->vx, scr->vy + len);
        lines[4].xy0 = XY(scr->vx - t, scr->vy - half);
        lines[4].xy1 = XY(scr->vx + t, scr->vy - half);
        lines[5].xy0 = XY(scr->vx - t, scr->vy + half);
        lines[5].xy1 = XY(scr->vx + t, scr->vy + half);
        lines[6].xy0 = XY(scr->vx - w, scr->vy - half);
        lines[6].xy1 = XY(scr->vx - w, scr->vy + half);
        lines[7].xy0 = XY(scr->vx + w, scr->vy - half);
        lines[7].xy1 = XY(scr->vx + w, scr->vy + half);
        lines[8].xy0 = XY(scr->vx - w, scr->vy - half);
        lines[8].xy1 = XY(scr->vx - (w - t), scr->vy - half);
        lines[9].xy0 = XY(scr->vx - w, scr->vy + half);
        lines[9].xy1 = XY(scr->vx - (w - t), scr->vy + half);
        lines[10].xy0 = XY(scr->vx + w, scr->vy - half);
        lines[10].xy1 = XY(scr->vx + (w - t), scr->vy - half);
        lines[11].xy0 = XY(scr->vx + w, scr->vy + half);
        lines[11].xy1 = XY(scr->vx + (w - t), scr->vy + half);
        for (i = 0; i < 12; i++) {
            lines[i].unk04 = (scr->vz >> 3) - 10;
        }
        return;
    }
    for (i = 0; i < 12; i++) {
        lines[i].xy1 = 0x04000400;
        lines[i].xy0 = 0x04000400;
    }
}

#ifdef NON_MATCHING
/* regalloc (lines base in t2 instead of t1), same code otherwise; sibling of func_8003EF48 */
void func_8003F3F8(VECTOR32 *scr, s32 fresh) {
    IfaceLine *lines = D_80118264;
    s32 i;
    s32 size;
    s32 len;
    s32 prod;
    s32 q;
    s32 h2;
    s32 h4;
    s32 t;

    for (i = 29; i >= 18; i--) {
        lines[i].xy0 = lines[i - 6].xy0;
        lines[i].xy1 = lines[i - 6].xy1;
        lines[i].unk04 = lines[i - 6].unk04 + 4;
    }
    if (fresh) {
        for (i = 17; i >= 12; i--) {
            lines[i].xy0 = lines[i - 8].xy0;
            lines[i].xy1 = lines[i - 8].xy1;
            lines[i].unk04 = lines[i - 8].unk04 + 4;
        }
    } else {
        for (i = 17; i >= 12; i--) {
            lines[i].xy1 = 0x04000400;
            lines[i].xy0 = 0x04000400;
        }
    }
    if (scr->vz > 0) {
        t = (*(IfaceCamera **)gCamera)->h * 30 / scr->vz;
        size = 4;
        if (t >= 4) {
            size = t;
        }
        len = size * D_8010FFEC / 4;
        prod = len * D_8010FFEC;
        lines[0].xy0 = XY(scr->vx - len * 2, scr->vy);
        lines[0].xy1 = XY(scr->vx - len, scr->vy);
        lines[1].xy0 = XY(scr->vx + len, scr->vy);
        lines[1].xy1 = XY(scr->vx + len * 2, scr->vy);
        q = prod / 4;
        h2 = q / 2;
        lines[2].xy0 = XY(scr->vx, scr->vy - q);
        lines[2].xy1 = XY(scr->vx, scr->vy - h2);
        lines[3].xy0 = XY(scr->vx, scr->vy + h2);
        lines[3].xy1 = XY(scr->vx, scr->vy + q);
        lines[4].xy0 = XY(scr->vx - len, scr->vy - h2);
        lines[4].xy1 = XY(scr->vx + len, scr->vy - h2);
        lines[5].xy0 = XY(scr->vx - len, scr->vy + h2);
        lines[5].xy1 = XY(scr->vx + len, scr->vy + h2);
        h4 = h2 / 2;
        lines[6].xy0 = XY(scr->vx - len, scr->vy - h2);
        lines[6].xy1 = XY(scr->vx - len, scr->vy - h4);
        lines[7].xy0 = XY(scr->vx - len, scr->vy + h2);
        lines[7].xy1 = XY(scr->vx - len, scr->vy + h4);
        lines[8].xy0 = XY(scr->vx + len, scr->vy - h2);
        lines[8].xy1 = XY(scr->vx + len, scr->vy - h4);
        lines[9].xy0 = XY(scr->vx + len, scr->vy + h2);
        lines[9].xy1 = XY(scr->vx + len, scr->vy + h4);
        t = h2 / 6;
        lines[10].xy0 = XY(scr->vx, scr->vy - t);
        lines[10].xy1 = XY(scr->vx, scr->vy + t);
        lines[11].xy0 = XY(scr->vx - len / 8, scr->vy);
        lines[11].xy1 = XY(scr->vx + len / 8, scr->vy);
        for (i = 0; i < 12; i++) {
            lines[i].unk04 = (scr->vz >> 3) - 10;
        }
        return;
    }
    for (i = 0; i < 12; i++) {
        lines[i].xy1 = 0x04000400;
        lines[i].xy0 = 0x04000400;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003F3F8);
#endif

void func_8003F8A0(VECTOR32 *out) {
    Gobj *obj = gameObjectsInfo[gObjPlayer->target->targetId].runtime;
    VECTOR32 a;
    VECTOR32 b;

    if (obj->npcState == 1 || obj->npcState == 2) {
        a.vx = obj->anim->frames[0]->pos.vx;
        a.vy = obj->anim->frames[0]->pos.vy;
        a.vz = obj->anim->frames[0]->pos.vz;
        a.vy = -a.vy;
        b.vx = obj->anim->frames[13]->pos.vx;
        b.vy = obj->anim->frames[13]->pos.vy;
        b.vz = obj->anim->frames[13]->pos.vz;
        b.vy = -b.vy;
        out->vx = (a.vx * 3 + b.vx) >> 2;
        out->vy = (a.vy * 3 + b.vy) >> 2;
        out->vz = (a.vz * 3 + b.vz) >> 2;
    } else {
        *out = obj->move->pos;
    }
}

/* The stores go through a plain s32 pointer, which keeps the cursor reloads after each one. */
void func_8003FA60(VECTOR32 *pos) {
    s32 dx = pos->vx - mTargetCursorPosition;
    s32 dy = pos->vy - D_8011031A;
    s32 dz = pos->vz - D_8011031C;
    s32 dist = SquareRoot0(dx * dx + dy * dy + dz * dz);
    s32 *p;

    if (dist > 320) {
        p = &pos->vx;
        *p = mTargetCursorPosition + dx * 320 / dist;
        p = &pos->vy;
        *p = D_8011031A + dy * 320 / dist;
        p = &pos->vz;
        *p = D_8011031C + dz * 320 / dist;
    }
}

void func_8003FBE4(void) {
    VECTOR32 pos;
    s32 targeting = 0;
    s32 sniper;
    s32 fresh;
    s32 r;
    s32 c;
    s32 i;

    if (gTargetModeFlag) {
        targeting = (u32)~gObjPlayer->target->targetId >> 31;
    }
    sniper = gSniperMode == 1;
    fresh = 0;
    if ((targeting || sniper) && gObjPlayer->healthInfo->health > 0) {
        if ((targeting || sniper) && D_8011031E == 0) {
            func_8003ECA0(targeting);
        } else if (targeting) {
            if (D_8011031E == 2) {
                iDeleteTargetCursor();
                func_8003ECA0(1);
            }
        } else if (D_8011031E == 1) {
            iDeleteTargetCursor();
            func_8003ECA0(0);
        }
        if (D_8011031E) {
            func_8003EECC(1);
            if (targeting) {
                func_8003F8A0(&pos);
            } else {
                pos.vx = SniperGetTarget(0)->vx;
                pos.vy = SniperGetTarget(0)->vy;
                pos.vz = SniperGetTarget(0)->vz;
            }
            if (targeting && D_8010FFEC >= 4) {
                func_8003FA60(&pos);
            }
            if ((u32)(pos.vx - mTargetCursorPosition + 16) < 33 && (u32)(pos.vy - D_8011031A + 16) < 33 &&
                (u32)(pos.vz - D_8011031C + 16) < 33) {
                if (D_8010FFEC < 4) {
                    fresh = 1;
                }
            } else {
                mTargetCursorPosition = pos.vx;
                D_8011031A = pos.vy;
                D_8011031C = pos.vz;
                fresh = 1;
            }
        }
    } else {
        if (!func_8003EECC(-1)) {
            return;
        }
        fresh = 1;
        pos.vx = mTargetCursorPosition;
        pos.vy = D_8011031A;
        pos.vz = D_8011031C;
    }
    EGSDisplayTransformWorldPointToScreen(&pos, *gCamera, &pos);
    pos.vy = -pos.vy;
    if (D_8011031E) {
        if (D_8011031E == 1) {
            func_8003EF48(&pos, fresh);
        } else {
            func_8003F3F8(&pos, fresh);
        }
        r = rand() & 0x1F;
        c = ((r + 0x21) << 16) | ((r | 0xE0) << 8) | (r + 0x21) | 0x40000000;
        for (i = 0x108; i >= 0; i -= 0x18) {
            *(s32 *)((u8 *)&D_80118264[0].color + i) = c;
        }
    }
}

void InterfaceUpdate(void) {
    if (!gInEOLState) {
        NPCLosCheck();
        NPCAim();
    }
    SniperUpdate();
    if (D_8010F8AC == 1) {
        SCRATCH_CALL0(func_8003FBE4);
    }
    if (D_8010F887 && gInterfaceOn && D_80117C70[0].link) {
        func_8003D76C();
    }
    WeaponUpdate();
    if (D_80110020) {
        InterfaceAnimate();
    } else {
        switch (D_8010F8AC) {
        case 0:
        case 1:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
            break;
        case 2:
        case 3:
            func_800454B8();
            break;
        case 5:
            iInterfaceDoSniffer();
            func_80045548();
            break;
        }
        func_8003E13C();
    }
    SysPostMessage(0xE, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
}

void InterfaceSetChasePointerTarget(VECTOR32 *target) {
    D_80117490.vx = target->vx;
    D_80117490.vy = target->vy;
    D_80117490.vz = target->vz;
    if (D_80117490.vx == 0 && D_80117490.vy == 0 && D_80117490.vz == 0) {
        D_80117490.vy = 1;
    }
    if (D_801174C4.link == 0 && D_8010F887) {
        EGSDisplayAddPoly(gInterfaceWindow, &D_801174C4);
    }
}

void InterfaceChasePointerOff(void) {
    D_80117490.vx = 0;
    D_80117490.vy = 0;
    D_80117490.vz = 0;
    if (D_801174C4.link) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_801174C4);
    }
}

void InterfaceHealthMeterTexture(IfaceTexture *tex) {
    s32 i;
    s16 *p;

    D_8010FFF8.x = tex->rect.x;
    D_8010FFF8.y = tex->rect.y;
    D_8010FFF8.w = tex->rect.w;
    D_8010FFF8.h = tex->rect.h;
    p = D_80117DFC;
    memcpy(p, tex->clut, D_8010FFF8.w * 2);
    for (i = 0; i < 0x100; i++, p++) {
        if (*p & 0x7FFF) {
            *p |= 0x8000;
        }
    }
    D_80117DFC[205] &= 0x7FFF;
}

void InterfaceWeaponDisplayUpdate(void) {
    if (gInterfaceOn) {
        D_8010F890 = WeaponMakeRoundsString(D_8010F890, gCurrentWeapon);
    }
}

void InterfaceCountdownPause(s32 pause) {
    D_8010F898 = !pause;
    if (D_8010F898 && D_8010F892 != 0xFFFF) {
        SysPostMessage(0xF, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
    }
}

void InterfaceCountdownDisplay(void) {
    char buf[9];
    s32 min;
    s32 sec;
    s32 tick;

    if (D_80110008 > 0) {
        min = D_80110008 / 1200;
        sec = D_80110008 % 1200;
        tick = sec % 20 / 3;
        sec /= 20;
        if (tick == 0 && sec == 0) {
            D_8010F8E0 = 0;
        } else {
            D_8010F8E0 -= 7;
            if (D_8010F8E0 < 0) {
                D_8010F8E0 += 10;
            }
        }
    } else {
        D_8010F8E0 = 0;
        min = 0;
        sec = 0;
        tick = 0;
    }
    buf[0] = min / 10 + '0';
    buf[1] = min % 10 + '0';
    buf[2] = ':';
    buf[3] = sec / 10 + '0';
    buf[4] = sec % 10 + '0';
    buf[5] = ':';
    buf[6] = tick + '0';
    buf[7] = D_8010F8E0 + '0';
    buf[8] = '\0';
    D_8010F892 = TextReplaceString(D_8010F892, buf);
    if (min > 0) {
        if (D_8010F894 != 2) {
            D_8010F894 = 2;
            if (D_8010F8AC == 1) {
                TextSetStringIntensity(D_8010F892, D_8010F878 + 20, D_8010F879 + 30, D_8010F87A + 40);
            } else {
                TextSetStringIntensity(D_8010F892, D_8010F8B0, D_8010F8B1, D_8010F8B2);
            }
        }
    } else if (D_8010F894 != 1) {
        D_8010F894 = 1;
        TextSetStringIntensity(D_8010F892, 0xC0, 0x50, 0x60);
    }
}

void InterfaceCountdownUpdate(void) {
    if (D_8010F898) {
        if (D_8011000C > D_80110008) {
            D_80110010();
            D_80110008 = 0x7FFFFFFF;
            D_8011000C = 0x7FFFFFFF;
            return;
        }
        D_80110008--;
        if (D_8010F892 != 0xFFFF) {
            InterfaceCountdownDisplay();
            SysPostMessage(0xF, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
        }
    }
}

void func_80040534(void) {
    InterfaceStartCountdown(D_80110014, D_80110018, D_8011001C);
}

void InterfaceStartCountdownAfterGameCam(s16 seconds, s16 endSeconds, void (*done)()) {
    D_80110014 = seconds;
    D_80110018 = endSeconds;
    D_8011001C = done;
    SysExeTickQueueTimer(GameCamCheckScriptCamDoneCallback, 20, func_80040534);
}

void InterfaceStartCountdown(s16 seconds, s16 endSeconds, void (*done)()) {
    D_80110010 = done;
    D_80110008 = seconds * 20;
    D_8011000C = endSeconds * 20;
    D_8010F892 = TextAddPersistentString(gCountdownFmt, D_80118104, -0x99, 0x32);
    TextSetStringJustification(D_8010F892, 0);
    SysPostMessage(0xF, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
    InterfaceCountdownDisplay();
    D_8010F894 = 5;
}

void InterfaceCountdownAbort(void) {
    TextRemoveString(D_8010F892);
    D_8010F892 = 0xFFFF;
    D_80110008 = 0;
}

void InterfaceCountdownReset(s32 ticks) {
    D_80110008 = ticks;
}

s16 InterfaceCountdownGet(void) {
    return D_80110008;
}

void InterfaceWeaponChange(void) {
    s32 sprite = gWeaponInfo[D_80110000].sprite;

    if (gInterfaceOn) {
        SpriteHideMulti(gInterfaceWindow, &D_80117514[sprite]);
        D_80110000 = gCurrentWeapon;
        SpriteShowMulti(gInterfaceWindow, &D_80117514[gWeaponInfo[gCurrentWeapon].sprite]);
        InterfaceWeaponDisplayUpdate();
    }
}

void func_8004072C(s32 weapon) {
    s32 i;
    s32 j;
    s32 x;
    const IfaceIconDef *def;
    IfaceSprite *tmp;
    IfaceSprite *src;

    for (i = 2; i != -1; i--) {
        weapon = WeaponGetPreviousID(weapon);
    }
    for (i = 0, x = -294; i < 7; i++, x += 98) {
        def = &D_80011BA0[gWeaponInfo[weapon].sprite];
        tmp = SpriteGetTemporary(6);
        src = SpriteGet(def->first);
        for (j = 0; j < def->count; j++) {
            tmp[j] = src[j];
            tmp[j].unk24 = 0;
            tmp[j].unk28 = 0;
        }
        SpriteCreateMulti(&D_80118A1C[i], j, tmp);
        SpriteShowMulti(gInterfaceWindow, &D_80118A1C[i]);
        SpriteSetMultiXY(&D_80118A1C[i], x, -80);
        weapon = WeaponGetNextID(weapon);
    }
}

void func_800408A8(void) {
    s32 i;
    IfaceSpriteNode *node;

    for (i = 0; i < 7; i++) {
        SpriteHideMulti(gInterfaceWindow, &D_80118A1C[i]);
        node = D_80118A1C[i].sprites;
        while (node->next) {
            node = node->next;
        }
        SpriteDisposeTemporary(node->sprite);
        SpriteDestroyMulti(&D_80118A1C[i]);
    }
}

void func_80040940(s32 weapon, s32 dir) {
    s32 i;
    s32 x;

    func_800408A8();
    if (dir > 0) {
        weapon = WeaponGetNextID(weapon);
    } else {
        weapon = WeaponGetPreviousID(weapon);
    }
    func_8004072C(weapon);
    for (i = 0; i < 7; i++) {
        x = i - 3;
        SpriteSetMultiXY(&D_80118A1C[i], (x + dir) * 98, -80);
    }
}

void InterfaceWeaponSelect(u8 held, s32 dir) {
    s32 i;
    s32 step;
    s32 n;
    s32 weapon;
    u8 sprite;
    s16 x;

    if ((u32)gInterfaceCurrentMode >= 2) {
        if (!held && D_8010F8C0 >= -4) {
            WeaponSelect(gObjPlayer, 27, 0);
            D_80110000 = gCurrentWeapon;
            InterfaceWeaponDisplayUpdate();
        }
        return;
    }
    if (held) {
        if (D_8010F8C0 == -5) {
            D_8010F8E8 = GameSysJoystickSetMode(6);
        } else {
            if (GameSysJoystickGetMode() != 6) {
                D_8010F8E8 = GameSysJoystickGetMode();
            }
            GameSysJoystickSetMode(6);
        }
    }
    if (dir != 0 && !D_8010F8E4) {
        if (D_8010F8C0 < 0) {
            goto done;
        }
        D_8010F8E4 = 1;
    }
    if (D_8010F8C0 >= 0) {
        if (held) {
            if (D_8010F8C0 == 0) {
                EGSDisplayBuildLineF(&D_801189EC[0], 0x808080, 0xFFA4FF38, 0xFFA400C8);
                EGSDisplayBuildLineF(&D_801189EC[1], 0x808080, 0xFFBCFF38, 0xFFBC00C8);
                EGSDisplayBuildPoly4F(&D_801189A4[0], 0x503028, 0xFFA6FF38, 0xFFA600C8, 0xFFBBFF38, 0xFFBB00C8);
                EGSDisplayBuildPoly4F(&D_801189A4[1], 0x503028, 0xFFA3FFCF, 0xFFA30031, 0xFFBEFFCF, 0xFFBE0031);
                D_801189A4[0].color.c[3] |= 2;
                D_801189A4[1].color.c[3] |= 2;
                EGSDisplayAddPoly(gInterfaceWindow, &D_801189A4[0]);
                EGSDisplayAddPoly(gInterfaceWindow, &D_801189A4[1]);
                EGSDisplayAddPoly(gInterfaceWindow, &D_801189EC[0]);
                EGSDisplayAddPoly(gInterfaceWindow, &D_801189EC[1]);
                weapon = gCurrentWeapon;
                SpriteHideMulti(gInterfaceWindow, &D_80117514[gWeaponInfo[weapon].sprite]);
                func_8004072C(weapon);
                D_8010F890 = TextReplaceString(D_8010F890, D_8010F8EC);
            }
            x = D_80118A34;
            if (x != 0) {
                step = x;
                if ((u32)(x + 32) > 64) {
                    step >>= 2;
                } else {
                    step >>= 1;
                }
                if (step == 0) {
                    step = -1;
                    if (D_80118A1C[3].unk0 > 0) {
                        step = 1;
                    }
                }
                for (i = 0; i < 7; i++) {
                    SpriteSetMultiXY(&D_80118A1C[i], D_80118A1C[i].unk0 - step, -80);
                }
            }
            if (dir != 0) {
                func_80040940(gCurrentWeapon, dir);
                n = dir;
                for (; n > 0; n--) {
                    WeaponSelect(gObjPlayer, 27, 1);
                }
                for (; n < 0; n++) {
                    WeaponSelect(gObjPlayer, 28, 1);
                }
            }
        } else if (D_8010F8C0 > 0) {
            sprite = gWeaponInfo[gCurrentWeapon].sprite;
            func_800408A8();
            EGSDisplayDeletePoly(gInterfaceWindow, &D_801189A4[0]);
            EGSDisplayDeletePoly(gInterfaceWindow, &D_801189A4[1]);
            EGSDisplayDeletePoly(gInterfaceWindow, &D_801189EC[0]);
            EGSDisplayDeletePoly(gInterfaceWindow, &D_801189EC[1]);
            if (gInterfaceOn) {
                SpriteShowMulti(gInterfaceWindow, &D_80117514[sprite]);
            }
        }
    }
done:
    if (held) {
        D_8010F8C0++;
        return;
    }
    if (D_8010F8C0 >= -4) {
        if (D_8010F8C0 < 2 && !D_8010F8E4) {
            WeaponSelect(gObjPlayer, 27, 0);
        } else if (D_8010F8E4) {
            WeaponReload(gCurrentWeapon);
            CASRequestAction(gPlayerOffset, 37);
        }
        D_8010F8C0 = -5;
        D_8010F8E4 = 0;
        D_80110000 = gCurrentWeapon;
        InterfaceWeaponDisplayUpdate();
        if (D_8010F8E8 != 6) {
            D_8010F8E8 = GameSysJoystickSetMode(D_8010F8E8);
        }
    }
}

u8 func_80040F08(u8 up, s32 *level) {
    if (up) {
        if (*level >= 13) {
            return 0;
        }
        if (++*level >= 13) {
            return 0;
        }
    } else {
        if (*level < 0) {
            return 0;
        }
        (*level)--;
    }
    return 1;
}

inline void iSetupDirectionSprites(s32 mode) {
    IfaceSprite *src;
    s32 i;
    s32 x;
    s32 y;

    switch (mode) {
    case 2:
        y = D_8010F89E + D_8010F8A2 + 25;
        x = mSniperViewMax - 27;
        break;
    case 3:
        y = -60;
        x = 150;
        break;
    default:
        return;
    }
    D_8010F8B8 = SpriteGetTemporary(5);
    src = SpriteGet(0x35);
    D_8010F8B8[0] = D_8010F8B8[4] = *src;
    src = SpriteGet(0x34);
    D_8010F8B8[1] = D_8010F8B8[3] = *src;
    src = SpriteGet(0x33);
    D_8010F8B8[2] = *src;
    for (i = 0; i < 5; i++) {
        D_8010F8B8[i].unk28 = (s32)EGSListAdd((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &D_8010F8B8[i]);
        D_8010F8B8[i].x = x;
    }
    for (i = 0; i < 8; i++) {
        D_8010F8BC[i].unk28 = (s32)EGSListAdd((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &D_8010F8BC[i]);
        D_8010F8BC[i].y = y;
    }
}

inline void iDestroyDirectionSprites(void) {
    s32 i;

    for (i = 0; i < 5; i++) {
        EGSDisplayDeleteSprite(gInterfaceWindow, &D_8010F8B8[i]);
    }
    for (i = 0; i < 8; i++) {
        EGSDisplayDeleteSprite(gInterfaceWindow, &D_8010F8BC[i]);
    }
    SpriteDisposeTemporary(D_8010F8B8);
    D_8010F8B8 = NULL;
}

inline void iAnimateDirectionSprites(s32 step) {
    s32 i;
    s32 d = 180 - step * 15;
    IfaceSprite *sp;

    for (i = 0; i < 5; i++) {
        sp = &D_8010F8B8[i];
        EGSTextureSetSpriteTransparency(sp, 1);
        if (sp->y >= -0xFF) {
            sp->y += d;
        }
    }
    for (i = 0; i < 8; i++) {
        sp = &D_8010F8BC[i];
        EGSTextureSetSpriteTransparency(sp, 1);
        if (sp->x >= -0xFF) {
            sp->x -= d;
        }
    }
}

IfaceLine *func_80041378(IfaceLine *lines, s32 count, s32 color, s32 trans) {
    IfaceLine *line = lines;
    s32 i;

    color = (color & 0xFFFFFF) | 0x40000000;
    for (i = 0; i < count; i++, line++) {
        EGSDisplayBuildLineF(line, color, 0x04000400, 0x04000400);
        if (trans) {
            line->color.c[3] |= 2;
        } else {
            line->color.c[3] &= ~2;
        }
        if (line->link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, line);
        }
    }
    return lines;
}

extern const u8 D_80011C10[];        /* per mode: keeps the target cursor */
extern const IfacePoint D_80011BE0[]; /* weapon-select bar origins, [0] unused here */
void TextMoveString(u16 str, s32 x, s32 y);

/* n * lvl / 12; inlined, so a constant n stays a mult. */
static inline s32 iLevelScale(s32 n, s32 lvl) {
    return n * lvl / 12;
}

#ifdef NON_MATCHING
/* the D_80117D24 line pointer stays in s0 from the line-0 stores to the loop (original reloads the base just before it), which shifts w/h/lvl registers */
/* Radar view (mode 1): one zoom step of the radar frame, weapon icons and text. */
u8 func_8004146C(u8 active) {
    s32 *level;
    s32 lvl;
    s32 w;
    s32 h;
    s32 i;
    s32 len;
    s32 x;
    s32 y;
    IfaceLine *line;

    if (D_80011C10[D_8010F8AC] == 0 && !active) {
        iDeleteTargetCursor();
        return 0;
    }
    level = &D_80105F30[0];
    if (!func_80040F08(active, level)) {
        return 0;
    }
    lvl = *level;
    if (!active) {
        if (lvl < 0) {
            if (D_8010F887) {
                iInterfaceRadarRemove();
            }
            for (i = 0, line = D_80117D9C; i < 4; i++, line++) {
                if (line->link) {
                    EGSDisplayDeletePoly(gInterfaceWindow, line);
                }
            }
            return 0;
        }
        iDeleteTargetCursor();
    } else if (lvl == 0) {
        if (D_8010F887) {
            iInterfaceRadarAdd();
        }
        func_80041378(D_80117D9C, 4, 0xE77E6B, 1);
    }

    w = lvl * 2;
    h = lvl * 20 / 12;
    D_80117C70[0].xy[0] = iRadarXY(XY(-w, -h));
    D_80117C70[0].xy[1] = iRadarXY(XY(w, -h));
    D_80117C70[0].xy[2] = iRadarXY(XY(-w, h));
    D_80117C70[0].xy[3] = iRadarXY(XY(w, h));
    D_80117D00.xy1 = iRadarXY(XY(-w, -h));
    D_80117D00.xy2 = iRadarXY(XY(w, -h));
    line = D_80117D24;
    line->xy0 = iRadarXY(XY(-w, 0));
    line->xy1 = iRadarXY(XY(w - 1, 0));

    w = lvl * 18 / 12 * lvl / 12;
    h = lvl * 15 / 12 * lvl / 12;
    D_80117C70[1].xy[0] = iRadarXY(XY(-w, -h));
    D_80117C70[1].xy[1] = iRadarXY(XY(w, -h));
    D_80117C70[1].xy[2] = iRadarXY(XY(-w, h));
    D_80117C70[1].xy[3] = iRadarXY(XY(w, h));

    h -= 7;
    if (h < 0) {
        h = 0;
    }
    w = h ? 9 : 0;
    for (i = 1, line++; i < 5; i++, line++) {
        line->xy0 = iRadarXY(XY((i & 1) ? w : -w, (i & 1) ? h : -h));
        line->xy1 = iRadarXY(XY((i & 2) ? w : -w, (i & 2) ? -h : h));
    }

    line = D_80117D9C;
    line->xy0 = XY(-182, -94);
    line->xy1 = XY(-182, lvl * 30 / 12 - 94);
    for (i = 1, line++; i < 4; i++, line++) {
        len = D_80011BE0[i].vx + 178;
        len = len * lvl / 12 * 4 - len * 3;
        if (len > 0) {
            line->xy0 = XY(-181, D_80011BE0[i].vy);
            line->xy1 = XY(len - 182, D_80011BE0[i].vy);
        } else {
            line->xy0 = line->xy1 = 0x04000400;
        }
    }

    x = 404 - iLevelScale(271, lvl);
    y = 91;
    for (i = 0; i < 23; i++) {
        SpriteSetMultiXY(&D_80117514[i], x, y);
    }
    TextMoveString(D_8010F890, x - 15, y + 3);
    if (D_8010F892 != 0xFFFF) {
        TextMoveString(D_8010F892, -153, 130 - lvl * 80 / 12);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8004146C);
#endif

extern s32 D_8010F8B4;
extern s16 gSniperView, D_8010F7FE, D_8010F800, D_8010F802; /* sniper view rectangle x, y, w, h */
extern char *gStringSniperSpareText;
extern char *D_80107818, *D_8010781C, *D_80107820;

/* Sniper view frame: POLY_F4s (D_801185C4) and POLY_F3s (D_8011896C). */
#define mSniperPolys D_801185C4
#define mSniperArrows D_8011896C
#define XY32(x, y) (((x) & 0xFFFF) | ((y) << 16))

#ifdef NON_MATCHING
/* regalloc in the corner-bracket loop and scheduling around the rangefinder/arrow blocks; the inlined iSetupDirectionSprites copies differ as in that function */
/* Sniper view (mode 2): one zoom step of the scope frame, rangefinder and sprites. */
u8 func_80041BCC(u8 active) {
    s32 *level;
    s32 lvl;
    s32 i;
    s32 d;
    s32 cx;
    s32 cy;
    s32 w;
    s32 h;
    IfaceSprite *spr;
    IfaceLine *line;

    if (D_80105F30[1] < 0 && (D_8010F8B4 != 0 || D_80118264[0].link != 0)) {
        return D_8010F8AC == 2;
    }
    level = &D_80105F30[1];
    if (!func_80040F08(active, level)) {
        return 0;
    }
    lvl = *level;
    if (!active) {
        if (gSpareTextWindow != 0xFF) {
            TextRemoveWindow(gSpareTextWindow);
            gSpareTextWindow = 0xFF;
        }
        if (lvl < 0) {
            iDestroyDirectionSprites();
            for (i = 0; i < 2; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &mSniperArrows[i]);
            }
            EGSDisplayDeleteSprite(gInterfaceWindow, SpriteGet(0x2D));
            for (i = 0; i < 26; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[i]);
            }
            for (i = 0; i < 14; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &mSniperPolys[i]);
            }
            return 0;
        }
    } else {
        if (gSpareTextWindow == 0xFF) {
            gSpareTextWindow = TextAddWindow((s16)(mSniperViewMax + D_8010F8A0 + 11), -2, 100, 100);
            TextSetWindowMini(gSpareTextWindow, 1);
            TextSetWindowColor(gSpareTextWindow, 0x10, 0x60, 0x10);
            TextAddString(gSpareTextWindow, gStringSniperSpareText, 30, 0);
            TextAddString(gSpareTextWindow, D_80107818, 20, 0);
            TextAddString(gSpareTextWindow, D_8010781C, 20, 0);
            TextAddString(gSpareTextWindow, D_80107820, 25, 0);
        }
        if (lvl == 0) {
            spr = SpriteGet(0x2D);
            spr->y = 0;
            spr->x = 0;
            spr->unk28 = (s32)EGSListAdd((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), spr);
            func_80041378(D_80118264, 26, 0x40FF40, 1);
            for (i = 0; i < 14; i++) {
                mSniperPolys[i].color.word = 0x2840FF40;
                if (i >= 8) {
                    SET_SEMI_TRANS(&mSniperPolys[i].tag);
                } else {
                    ((IfacePrimTag *)&mSniperPolys[i].tag)->code &= ~2;
                }
                EGSDisplayAddPoly(gInterfaceWindow, &mSniperPolys[i]);
            }
            for (i = 0; i < 2; i++) {
                EGSDisplayAddPoly(gInterfaceWindow, &mSniperArrows[i]);
            }
            iSetupDirectionSprites(D_8010F8AC);
        }
    }

    SpriteGet(0x2D)->scalex = (lvl << 12) / 12;
    cx = mSniperViewMax * lvl / 12;
    gSniperView = POSITION + cx;
    cy = D_8010F89E * lvl / 12;
    D_8010F7FE = D_8012C7FA + cy;
    w = (s16)D_8010F8A0 * lvl / 12;
    h = D_8010F8A2 * lvl / 12;
    if (w != 0 || h != 0) {
        D_8010F800 = w + 1;
        D_8010F802 = h + 1;
    } else {
        D_8010F800 = 0;
        D_8010F802 = 0;
    }

    /* corner brackets: two quads per corner */
    for (i = 0; i < 4; i++) {
        s32 x;
        s32 y;
        s32 x2;
        s32 y2;
        s32 x3;
        s32 y3;
        IfacePoly4 *p;

        x = (i & 1) ? cx : cx + w + 1;
        y = (i & 2) ? cy : cy + h + 1;
        x2 = (i & 1) ? x + 20 : x - 20;
        y2 = (i & 2) ? y + 15 : y - 15;
        x3 = (i & 1) ? x + 3 : x - 3;
        y3 = (i & 2) ? y + 2 : y - 2;
        if (i & 1) {
            x2 = x2 > 0 ? 0 : x2;
            x3 = x3 > 0 ? 0 : x3;
        } else {
            if (x2 < 0) {
                x2 = 0;
            }
            if (x3 < 0) {
                x3 = 0;
            }
        }
        if (i & 2) {
            y2 = y2 > 0 ? 0 : y2;
            y3 = y3 > 0 ? 0 : y3;
        } else {
            if (y2 < 0) {
                y2 = 0;
            }
            if (y3 < 0) {
                y3 = 0;
            }
        }
        p = &mSniperPolys[i * 2];
        p[0].xy[0] = p[1].xy[0] = XY32(x, y);
        p[0].xy[1] = p[1].xy[2] = XY32(x3, y3);
        p[1].xy[3] = XY32(x2, y3);
        p[0].xy[2] = XY32(x, y2);
        p[0].xy[3] = XY32(x3, y2);
        p[1].xy[1] = XY32(x2, y);
    }

    {
        s32 top = D_8010F89E + D_8010F8A2;
        s32 m = mSniperViewMax;

        mSniperPolys[8].xy[0] = XY32(cx, top + 15);
        mSniperPolys[8].xy[1] = XY32(cx + w + 1, top + 15);
        mSniperPolys[8].xy[2] = XY32(cx, top + 16);
        mSniperPolys[8].xy[3] = XY32(cx + w + 1, top + 16);
        mSniperPolys[9].xy[0] = XY32(cx, top + 9);
        mSniperPolys[9].xy[1] = XY32(cx + 2, top + 9);
        mSniperPolys[9].xy[2] = XY32(cx, top + 15);
        mSniperPolys[9].xy[3] = XY32(cx + 2, top + 15);
        mSniperPolys[10].xy[0] = XY32(cx + w - 1, top + 9);
        mSniperPolys[10].xy[1] = XY32(cx + w + 1, top + 9);
        mSniperPolys[10].xy[2] = XY32(cx + w - 1, top + 15);
        mSniperPolys[10].xy[3] = XY32(cx + w + 1, top + 15);
        mSniperPolys[11].xy[0] = XY32(m - 19, cy);
        mSniperPolys[11].xy[1] = XY32(m - 17, cy);
        mSniperPolys[11].xy[2] = XY32(m - 19, cy + h + 1);
        mSniperPolys[11].xy[3] = XY32(m - 17, cy + h + 1);
        mSniperPolys[12].xy[0] = XY32(m - 17, cy);
        mSniperPolys[12].xy[1] = XY32(m - 10, cy);
        mSniperPolys[12].xy[2] = XY32(m - 17, cy + 1);
        mSniperPolys[12].xy[3] = XY32(m - 10, cy + 1);
        mSniperPolys[13].xy[0] = XY32(m - 17, cy + h);
        mSniperPolys[13].xy[1] = XY32(m - 10, cy + h);
        mSniperPolys[13].xy[2] = XY32(m - 17, cy + h + 1);
        mSniperPolys[13].xy[3] = XY32(m - 10, cy + h + 1);
    }

    /* cross hairs: long arms when the view is big enough, jittering otherwise */
    if (w > 100) {
        D_80118264[0].xy0 = XY32(cx + 40, cy);
        D_80118264[0].xy1 = XY32(cx + w - 40, cy);
        D_80118264[1].xy0 = XY32(cx + 40, cy + h);
        D_80118264[1].xy1 = XY32(cx + w - 40, cy + h);
    } else {
        s32 r;

        r = rand() % (h + 1) - h / 2;
        D_80118264[0].xy0 = XY32(cx, r);
        D_80118264[0].xy1 = XY32(cx + w, r);
        r = rand() % (h + 1) - h / 2;
        D_80118264[1].xy0 = XY32(cx, r);
        D_80118264[1].xy1 = XY32(cx + w, r);
    }
    if (h > 70) {
        D_80118264[2].xy0 = XY32(cx, cy + 30);
        D_80118264[2].xy1 = XY32(cx, cy + h - 30);
        D_80118264[3].xy0 = XY32(cx + w, cy + 30);
        D_80118264[3].xy1 = XY32(cx + w, cy + h - 30);
    } else {
        s32 r;

        r = rand() % (h + 1) - h / 2;
        D_80118264[2].xy0 = XY32(cx, r);
        D_80118264[2].xy1 = XY32(cx + w, r);
        r = rand() % (h + 1) - h / 2;
        D_80118264[3].xy0 = XY32(cx, r);
        D_80118264[3].xy1 = XY32(cx + w, r);
    }

    /* rangefinder ticks */
    for (i = 0; i < 9; i++) {
        line = &D_80118264[4 + i];
        line->xy0 = XY32(mSniperViewMax - 18 + lvl * ((i & 1) ? 5 : 7) / 12, (i - 4) * h / 10);
        d = lvl / 3 + 18;
        line->xy1 = XY32(mSniperViewMax - d, (i - 4) * h / 10);
    }
    for (i = 0, line = &D_80118264[13]; i < 9; i++, line++) {
        s32 x = (i - 4) * w / 10;
        s32 top = D_8010F89E + D_8010F8A2;

        d = lvl / 4 + 15;
        line->xy0 = XY32(x, top + d);
        line->xy1 = XY32(x, top + 15 - ((i & 1) ? lvl / 3 : lvl / 2));
    }

    {
        s32 top = D_8010F89E + D_8010F8A2;
        s32 ax;
        s32 ay;

        mSniperArrows[0].xy0 = XY32(mSniperViewMax - 17, 0);
        mSniperArrows[1].xy0 = (top + 15) << 16;
        ax = lvl * 9 / 12 - 17;
        ax += mSniperViewMax;
        mSniperArrows[0].xy1 = XY32(ax, 3);
        mSniperArrows[0].xy2 = XY32(ax, -3);
        ay = lvl * 7 / 12 - 15;
        ay = top - ay;
        mSniperArrows[1].xy1 = XY32(4, ay);
        mSniperArrows[1].xy2 = XY32(-4, ay);
    }

    iAnimateDirectionSprites(lvl);

    switch (lvl) {
    case 0:
        D_80118264[22].xy0 = 0;
        break;
    case 3:
        D_80118264[23].xy0 = D_80118264[23].xy1 = 0x04000400;
        /* fallthrough */
    case 1:
    case 2:
        D_80118264[22].xy1 = ((D_8010F89E - 20) * lvl / 4) << 16;
        break;
    case 4:
        D_80118264[22].xy1 = (D_8010F89E - 20) << 16;
        D_80118264[23].xy0 = D_80118264[23].xy1 = D_80118264[22].xy1 | 1;
        break;
    case 6:
        D_80118264[24].xy0 = D_80118264[24].xy1 = 0x04000400;
        /* fallthrough */
    case 5:
        D_80118264[23].xy1 = XY32((lvl - 4) * 25, D_8010F89E - 20);
        break;
    case 7:
        D_80118264[23].xy1 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 16, D_8010F89E - 20);
        D_80118264[24].xy0 = D_80118264[24].xy1 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 16, D_8010F89E - 19);
        break;
    case 8:
        D_80118264[25].xy0 = D_80118264[25].xy1 = 0x04000400;
        D_80118264[24].xy1 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 16, (D_8010F89E - 20) / 2);
        break;
    case 9:
        D_80118264[24].xy1 = D_80118264[25].xy0 = D_80118264[25].xy1 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 16, -6);
        break;
    case 10:
        D_80118264[25].xy0 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 11, -5);
        D_80118264[25].xy1 = XY32(mSniperViewMax + (s16)D_8010F8A0 + 41, -5);
        break;
    }
    D_80118264[22].xy0 = XY32(0, cy - 1);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_80041BCC);
#endif

#ifdef NON_MATCHING
/* regalloc of the hoisted goggle-frame invariants (the original also uses fp and spills one sign word); the busy return is placed inside the inlined iSetupDirectionSprites */
/* Infrared view (mode 3): one zoom step of the goggle frame, rangefinder and sprites. */
u8 func_80042DA4(u8 active) {
    IfaceSprite *spr = SpriteGet(0x2E); /* goggle frame, 5 sprites */
    s32 *level;
    s32 lvl;
    s32 i;
    s32 y;
    s32 h;
    s32 scale;
    IfaceLine *line;
    IfacePoly4 *p;

    if (D_80105F30[2] < 0) {
        if (D_8010F8B4 != 0) {
            goto busy;
        }
        {
            IfacePolyF3 *f3;

            for (i = 0, f3 = mSniperArrows; i < 2; i++, f3++) {
                if (f3->link != 0) {
                    goto busy;
                }
            }
        }
        {
            IfaceLine *l;

            for (i = 0, l = D_80118264; i < 18; i++, l++) {
                if (l->link != 0) {
                    goto busy;
                }
            }
        }
        {
            IfacePoly4 *q;

            for (i = 0, q = mSniperPolys; i < 16; i++, q++) {
                if (q->link != 0) {
                    goto busy;
                }
            }
        }
    }
    level = &D_80105F30[2];
    if (!func_80040F08(active, level)) {
        return 0;
    }
    lvl = *level;
    if (lvl < 0 && !active) {
        {
            iDestroyDirectionSprites();
            for (i = 0; i < 2; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &mSniperArrows[i]);
            }
            for (i = 0; i < 18; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[i]);
            }
            for (i = 0; i < 16; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &mSniperPolys[i]);
            }
            for (i = 0; i < 5; i++) {
                EGSDisplayDeleteSprite(gInterfaceWindow, &spr[i]);
            }
            EGSDisplayInfraredOff(*gCamera, 0);
            SysStatePush(6);
            func_8003D0E8(0x503028, 0xE77E6B, 0xD0D0D0);
            D_8010F894 = 5;
            for (i = 0x19; i < 0x1C; i++) {
                IfaceSprite *s = SpriteGet(i);

                s->b = 0x80;
                s->g = 0x80;
                s->r = 0x80;
            }
            for (i = 0; i < 4; i++) {
                D_801179A0[i].color.word = 0x40E77E6B;
                SET_SEMI_TRANS(&D_801179A0[i].tag);
            }
            return 0;
        }
    }
    if (active && lvl == 0) {
        iSetupDirectionSprites(D_8010F8AC);
        spr[1].mx = -1;
        spr[0].mx = spr[0].w;
        spr[2].mx = spr[2].w + (u16)((spr[3].w >> 1) - 1);
        spr[3].mx = (spr[3].w >> 1) - 1;
        spr[4].mx = 1 - (spr[3].w >> 1);
        for (i = 0; i < 5; i++) {
            spr[i].b = 0x20;
            spr[i].g = 0x20;
            spr[i].r = 0x20;
            spr[i].y = 0;
            spr[i].x = 0;
            spr[i].unk28 = (s32)EGSListAdd((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &spr[i]);
        }
        for (i = 0; i < 16; i++) {
            mSniperPolys[i].color.word = 0x28106010;
            SET_SEMI_TRANS(&mSniperPolys[i].tag);
            EGSDisplayAddPoly(gInterfaceWindow, &mSniperPolys[i]);
        }
        func_80041378(D_80118264, 18, 0x106010, 1);
        for (i = 0; i < 2; i++) {
            SET_SEMI_TRANS(&mSniperArrows[i].tag);
            EGSDisplayAddPoly(gInterfaceWindow, &mSniperArrows[i]);
        }
        EGSDisplayInfraredOn(*gCamera, 0);
        SysStatePush(6);
        func_8003D0E8(0x285030, 0x40FF40, 0x20D020);
        D_8010F894 = 5;
        for (i = 0x19; i < 0x1C; i++) {
            IfaceSprite *s = SpriteGet(i);

            s->b = 0x20;
            s->r = 0x20;
            s->g = 0xA0;
        }
    }

    scale = (lvl << 12) / 12;
    spr[1].scalex = scale;
    spr[0].scalex = scale;
    spr[4].scaley = scale;
    spr[3].scaley = scale;
    spr[2].scaley = scale;

    /* goggle frame: four quads per side */
    for (i = 0; i < 2; i++) {
        s32 x0;
        s32 x1;
        s32 x2;
        s32 dx;
        s32 tx;
        s32 y2;
        s32 ty;
        s32 ty2;

        y = i ? lvl * 50 / 12 : -(lvl * 50) / 12;
        y2 = i ? y + 2 : y - 2;
        x0 = i ? 140 : -140;
        x1 = i ? 137 : -137;
        x2 = i ? 155 : -155;
        dx = i ? -1 : 1;
        tx = i ? 2 : -2;
        ty = i ? lvl * 31 / 12 : -(lvl * 31) / 12;
        ty2 = i ? lvl * 50 / 12 : -(lvl * 50) / 12;
        p = &mSniperPolys[i * 4];
        p[0].xy[0] = XY32(i ? -1 : 1, y);
        p[0].xy[1] = p[1].xy[3] = XY32(x0, y);
        p[0].xy[2] = XY32(i ? -1 : 1, y2);
        p[0].xy[3] = XY32(x0, y2);
        p[1].xy[2] = XY32(x1, y);
        p[1].xy[1] = XY32(x0, -y);
        p[2].xy[0] = XY32(x1, -y2);
        p[2].xy[3] = XY32(x2, -y);
        p[2].xy[1] = XY32(x2, -y2);
        p[3].xy[0] = XY32(tx, ty);
        p[3].xy[1] = XY32(dx, ty);
        p[3].xy[3] = XY32(dx, ty2);
        p[1].xy[0] = p[2].xy[2] = XY32(x1, -y);
        p[3].xy[2] = XY32(tx, ty2);
    }

    /* rangefinder ticks */
    for (i = 0; i < 9; i++) {
        line = &D_80118264[i];
        line->xy0 = XY32(139 - lvl * ((i & 1) ? 5 : 8) / 12, (i - 4) * (lvl * 10) / 12);
        line->xy1 = XY32(lvl / 3 + 139, (i - 4) * (lvl * 10) / 12);
    }
    for (i = 0; i < 9; i++) {
        line = &D_80118264[9 + i];
        line->xy0 = XY32((i + 1) * -140 / 10, lvl * -55 / 12);
        line->xy1 = XY32((i + 1) * -140 / 10, (i & 1) ? -(lvl * 48) / 12 : -(lvl * 46) / 12);
    }

    /* zoom bars */
    y = 52;
    for (i = 0, p = &mSniperPolys[8]; i < 8; i++, p++) {
        h = i * 2 + 4;
        y -= h + 2;
        p->xy[0] = XY32(-149, y);
        p->xy[1] = XY32(-144, y);
        p->xy[2] = XY32(-149, y + h * lvl / 12);
        p->xy[3] = XY32(-144, y + h * lvl / 12);
    }

    mSniperArrows[0].xy0 = XY32(140 - lvl / 4, 0);
    mSniperArrows[0].xy1 = XY32(140 - lvl, 3);
    mSniperArrows[0].xy2 = XY32(140 - lvl, -3);
    mSniperArrows[1].xy0 = XY32(-70, -(lvl * 50) / 12);
    mSniperArrows[1].xy1 = XY32(-66, lvl * -43 / 12);
    mSniperArrows[1].xy2 = XY32(-74, lvl * -43 / 12);

    iAnimateDirectionSprites(lvl);
    return 1;

busy:
    return D_8010F8AC == 3;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_80042DA4);
#endif

extern IfacePoly4 *gMenuPolys; /* menu background quads, [30] is the backdrop */
extern IfaceLine *gMenuLines;  /* menu outline lines */

#ifdef NON_MATCHING
/* regalloc throughout (pointer/index registers in the sweep and tick loops); structure and stores follow the original */
/* Menu view (mode 4): one step of the menu background sweep and its tick lines.
 * D_80118264[0..5] are the menu's highlight and pointer lines. */
u8 func_80043B04(u8 active) {
    s32 *level;
    s32 prev;
    s32 lvl;
    s32 i;
    s32 from;   /* quads shown before this step */
    s32 to;     /* quads shown after it */
    s32 tick0;  /* left ticks, before/after */
    s32 tick1;
    s32 rule0;  /* bottom ticks, before/after */
    s32 rule1;
    s32 mark0;  /* menu markers, before/after */
    s32 mark1;
    const s32 *v;
    IfacePoly4 *poly;
    IfaceLine *line;

    if (D_80105F30[3] < 0) {
        IfaceLine *l;

        for (i = 0, l = D_80118264; i < 6; i++, l++) {
            if (l->link != 0) {
                return D_8010F8AC == 4;
            }
        }
    }
    if (gMenuPolys == NULL) {
        if (D_80105F30[3] != -1) {
            IfaceLine *l;

            D_80105F30[3] = -1;
            for (i = 0, l = D_80118264; i < 36; i++, l++) {
                if (l->link != 0) {
                    EGSDisplayDeletePoly(gInterfaceWindow, l);
                }
            }
        }
        return active;
    }
    level = &D_80105F30[3];
    prev = *level;
    if (!func_80040F08(active, level)) {
        return 0;
    }
    from = prev * 30 / 12;
    tick0 = prev * 26 / 12;
    rule0 = tick0;
    mark0 = prev * 20 / 12;
    lvl = *level;
    to = lvl * 30 / 12;
    tick1 = lvl * 26 / 12;
    rule1 = tick1;
    mark1 = lvl * 20 / 12;
    if (lvl <= 0) {
        if (active) {
            EGSDisplayBuildLineF(&D_80118264[0], 0xF060F0, 0x04000400, 0x04000400);
            for (i = 1, line = &D_80118264[1]; i < 36; i++, line++) {
                EGSDisplayBuildLineF(line, 0xB4645A, 0x04000400, 0x04000400);
            }
            if (D_80118264[0].link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[0]);
            }
            return active;
        }
        if (lvl < 0) {
            return 0;
        }
    } else if (lvl >= 12 && (!active || lvl >= 13)) {
        return !active;
    }
    if (D_80117C70[0].link != 0) {
        func_8003D76C();
    }

    if (active) {
        v = &D_80011C18[from * 2];
        poly = &gMenuPolys[from];
        line = &gMenuLines[from * 2];
        if (gMenuPolys[30].link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, &gMenuPolys[30]);
            gMenuPolys[30].color.word = 0x28B46450;
            SET_SEMI_TRANS(&gMenuPolys[30].tag);
        }
        for (i = from; i < to; i++, v += 2, poly++, line += 2) {
            EGSDisplayBuildPoly4F(poly, 0xA0463C, v[0], v[1], v[2], v[3]);
            poly->unk04 = 6;
            SET_SEMI_TRANS(&poly->tag);
            if (poly->link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, poly);
            }
            if (i != 14) {
                EGSDisplayBuildLineF(&line[0], 0xB4645A, v[0], v[2]);
                line[0].unk04 = 5;
                if (line[0].link == 0) {
                    EGSDisplayAddPoly(gInterfaceWindow, &line[0]);
                }
            }
            if (i != 16) {
                EGSDisplayBuildLineF(&line[1], 0xB4645A, v[1], v[3]);
                line[1].unk04 = 5;
                if (line[1].link == 0) {
                    EGSDisplayAddPoly(gInterfaceWindow, &line[1]);
                }
            }
        }
        if (to < 30) {
            EGSDisplayBuildPoly4F(poly, 0xFFB478, v[0], v[1], v[2], v[3]);
            poly->unk04 = 6;
            EGSDisplayAddPoly(gInterfaceWindow, poly);
            EGSDisplayBuildLineF(&line[0], 0xFFB478, v[1], v[3]);
            line[0].unk04 = 5;
            EGSDisplayAddPoly(gInterfaceWindow, &line[0]);
            EGSDisplayBuildLineF(&line[1], 0xFFB478, v[1], v[3]);
            line[1].unk04 = 5;
            EGSDisplayAddPoly(gInterfaceWindow, &line[1]);
        }
        switch (to) {
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            if (D_80118264[1].link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[1]);
            }
            D_80118264[1].xy0 = XY32(115, 4);
            D_80118264[1].xy1 = XY32(-((to - 26) * 56) / 4 + 115, 4);
            /* fallthrough */
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            if (D_80118264[3].link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[3]);
            }
            D_80118264[3].xy0 = XY32(-54, 98);
            D_80118264[3].xy1 = XY32((to - 10) * -115 / 20 - 54, 98);
            /* fallthrough */
        case 8:
        case 9:
            if (D_80118264[4].link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[4]);
            }
            D_80118264[4].xy0 = XY32(-169, 90);
            D_80118264[4].xy1 = XY32(-169, (to - 8) * 8 / 22 + 90);
            /* fallthrough */
        case 6:
        case 7:
            if (D_80118264[5].link == 0) {
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[5]);
            }
            D_80118264[5].xy0 = XY32(-169, 51);
            D_80118264[5].xy1 = XY32(-169, -((to - 6) * 81) / 24 + 51);
            break;
        }
        if (D_80118264[2].link == 0) {
            EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[2]);
        }
        D_80118264[2].xy0 = XY32(35, -94);
        D_80118264[2].xy1 = XY32(to * 78 / 30 + 35, -94);

        for (i = tick0, line = &gMenuLines[60 + tick0]; i <= tick1; i++, line++) {
            s32 y = i >= 18 ? -10 - (i * 10 - 180) : i * 10 - 89;
            s32 xr;
            s32 xl;

            if (i == 0) {
                xr = 31;
            } else if (i < 18) {
                xr = 34;
            } else {
                xr = i == 18 ? 154 : 156;
            }
            xl = -156;
            if (i >= 7) {
                if (i == 7) {
                    xl = -151;
                } else {
                    xl = -147;
                    if (i >= 18) {
                        if (i == 18) {
                            xl = 43;
                        } else {
                            xl = 43;
                            if (i < 25) {
                                xl = 42;
                            }
                        }
                    }
                }
            }
            if (i == tick1) {
                if (tick1 < 26) {
                    EGSDisplayBuildLineF(line, 0xF0B478, XY32(xl, y), XY32(xr, y));
                    line->unk04 = 5;
                    EGSDisplayAddPoly(gInterfaceWindow, line);
                }
            } else {
                EGSDisplayBuildLineF(line, 0x8C3C46, XY32(xl, y), XY32(xr, y));
                line->unk04 = 5;
                SET_SEMI_TRANS(&line->tag);
                if (line->link == 0) {
                    EGSDisplayAddPoly(gInterfaceWindow, line);
                }
            }
        }
        for (i = rule0, line = &gMenuLines[86 + rule0]; i <= rule1; i++, line++) {
            s32 x = i >= 16 ? i * 12 - 192 + 44 : i * 12 - 151;
            s32 ya;
            s32 yb;

            if (i == 0) {
                ya = -20;
            } else {
                ya = 88;
                if (i >= 15) {
                    ya = 86;
                    if (i != 15) {
                        if (i == 16) {
                            ya = -10;
                        } else {
                            ya = -9;
                            if (i < 25) {
                                ya = -5;
                            }
                        }
                    }
                }
            }
            if (i == 0) {
                yb = -94;
            } else {
                yb = -95;
                if (i >= 15) {
                    yb = -91;
                    if (i != 15) {
                        if (i == 16) {
                            yb = -82;
                        } else {
                            yb = -84;
                            if (i < 25) {
                                yb = -85;
                            }
                        }
                    }
                }
            }
            if (i == rule1) {
                if (rule1 < 26) {
                    EGSDisplayBuildLineF(line, 0xF0B478, XY32(x, ya), XY32(x, yb));
                    line->unk04 = 5;
                    EGSDisplayAddPoly(gInterfaceWindow, line);
                }
            } else {
                EGSDisplayBuildLineF(line, 0x8C3C46, XY32(x, ya), XY32(x, yb));
                line->unk04 = 5;
                SET_SEMI_TRANS(&line->tag);
                if (line->link == 0) {
                    EGSDisplayAddPoly(gInterfaceWindow, line);
                }
            }
        }
        for (i = mark0; i < mark1; i++) {
            EGSDisplayAddPoly(gInterfaceWindow, &gMenuLines[112 + i]);
        }
        if (i < 12) {
            D_80118264[0].xy0 = gMenuLines[112 + i].xy0;
            D_80118264[0].xy1 = gMenuLines[112 + i].xy1;
            return 1;
        }
        goto hide;
    }

    poly = &gMenuPolys[to];
    line = &gMenuLines[to * 2];
    if (gMenuPolys[30].link != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, &gMenuPolys[30]);
    }
    for (i = to; i < from; i++, poly++, line += 2) {
        if (poly->link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, poly);
        }
        if (line[0].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &line[0]);
        }
        if (line[1].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &line[1]);
        }
    }
    switch (to) {
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
        D_80118264[1].xy0 = XY32(115, 4);
        D_80118264[1].xy1 = XY32(-((to - 26) * 56) / 4 + 115, 4);
    show3:
        D_80118264[3].xy0 = XY32(-54, 98);
        D_80118264[3].xy1 = XY32((to - 10) * -115 / 20 - 54, 98);
        /* fallthrough */
    case 8:
    case 9:
        D_80118264[4].xy0 = XY32(-169, 90);
        D_80118264[4].xy1 = XY32(-169, (to - 8) * 8 / 22 + 90);
        D_80118264[5].xy0 = XY32(-169, 51);
        D_80118264[5].xy1 = XY32(-169, -((to - 6) * 81) / 24 + 51);
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
        if (D_80118264[1].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[1]);
        }
        goto show3;
    default:
        if (D_80118264[3].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[3]);
        }
        if (D_80118264[4].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[4]);
        }
        if (D_80118264[5].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[5]);
        }
        break;
    }
    i = to * 78 / 30;
    if (i <= 0) {
        if (D_80118264[2].link != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[2]);
        }
    } else {
        D_80118264[2].xy0 = XY32(35, -94);
        D_80118264[2].xy1 = XY32(i + 35, -94);
    }
    for (i = tick1, line = &gMenuLines[60 + tick1]; i < tick0; i++, line++) {
        EGSDisplayDeletePoly(gInterfaceWindow, line);
    }
    for (i = rule1, line = &gMenuLines[86 + rule1]; i < rule0; i++, line++) {
        EGSDisplayDeletePoly(gInterfaceWindow, line);
    }
    for (i = mark1; i < mark0; i++) {
        EGSDisplayDeletePoly(gInterfaceWindow, &gMenuLines[112 + i]);
    }
    if (D_80118264[0].link != 0) {
    hide:
        EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[0]);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_80043B04);
#endif

extern char *gStringSnifferSpareText;
extern char *D_80107828;
extern char *D_8010782C;


static inline s32 iMax(s32 a, s32 b) {
    return a < b ? b : a;
}

/* Infrared view (mode 5): one zoom step of the line frame. */
u8 func_80044AC0(u8 active) {
    s32 *level = &D_80105F30[4];
    s32 lvl;
    s32 i;
    s32 y;
    s32 x;
    s32 bx;
    s32 by;
    s32 by2;
    s32 ox;
    s32 oy;
    s32 ix;
    s32 iy;

    if (!func_80040F08(active, level)) {
        return 0;
    }
    lvl = *level;
    if (!active) {
        if (gSpareTextWindow != 0xFF) {
            TextRemoveWindow(gSpareTextWindow);
            gSpareTextWindow = 0xFF;
        }
    } else if (lvl >= 9 && gSpareTextWindow == 0xFF) {
        gSpareTextWindow = TextAddWindow(-132, 36, 100, 100);
        TextSetWindowMini(gSpareTextWindow, 1);
        TextSetWindowColor(gSpareTextWindow, 0x7F, 0x52, 0x27);
        TextSetWindowJustification(gSpareTextWindow, 1);
        TextAddString(gSpareTextWindow, gStringSnifferSpareText, 45, 0);
        TextAddString(gSpareTextWindow, D_80107828, 45, 0);
        TextAddString(gSpareTextWindow, D_8010782C, 30, 0);
    }
    if (lvl == 0) {
        if (active) {
            EGSDisplayInfraredOn(*gCamera, 1);
            func_8003D0E8(0x1E5080, 0x4DAEFF, 0x20D0D0);
            SysStatePush(6);
            for (i = 0; i < 28; i++) {
                EGSDisplayBuildLineF(&D_80118264[i], 0x4DAEFF, 0, 0);
                EGSDisplayAddPoly(gInterfaceWindow, &D_80118264[i]);
            }
            D_80118264[0].xy0 = XY(0, 120);
            D_80118264[1].xy0 = XY(0, -120);
            D_80118264[6].xy0 = XY(192, 0);
            D_80118264[7].xy0 = XY(-192, 0);
        } else {
            EGSDisplayInfraredOff(*gCamera, 1);
            func_8003D0E8(0x503028, 0xE77E6B, 0xD0D0D0);
            SysStatePush(6);
            for (i = 0; i < 28; i++) {
                EGSDisplayDeletePoly(gInterfaceWindow, &D_80118264[i]);
            }
        }
    }

    y = iMax(120 - lvl * 10, 26);
    D_80118264[0].xy1 = XY(0, y);
    D_80118264[1].xy1 = XY(0, -y);
    D_80118264[2].xy0 = XY(-42, y);
    D_80118264[2].xy1 = XY(42, y);
    D_80118264[3].xy0 = XY(-42, -y);
    D_80118264[3].xy1 = XY(42, -y);
    D_80118264[4].xy0 = XY(-42, y);
    D_80118264[4].xy1 = XY(-42, y + 8);
    D_80118264[5].xy0 = XY(-42, y + 8);
    D_80118264[5].xy1 = XY(-32, y + 8);

    x = iMax(192 - lvl * 16, 36);
    D_80118264[6].xy1 = XY(x, 0);
    D_80118264[7].xy1 = XY(-x, 0);
    D_80118264[8].xy0 = XY(x, 10);
    D_80118264[8].xy1 = XY(x, -10);
    D_80118264[9].xy0 = XY(-x, 10);
    D_80118264[9].xy1 = XY(-x, -10);

    bx = lvl * 58 / 12 - 29;
    if (bx < 0) {
        bx = 0;
    }
    by = lvl * 40 / 12 - 20;
    if (by < 0) {
        by = 0;
    }
    by2 = by - 7;
    if (bx != 0 || by != 0) {
        D_80118264[10].xy0 = XY(-bx, -by2);
        D_80118264[10].xy1 = D_80118264[11].xy0 = XY(-bx, -by);
        D_80118264[11].xy1 = D_80118264[12].xy0 = XY(bx, -by);
        D_80118264[12].xy1 = D_80118264[13].xy0 = XY(bx, by);
        D_80118264[13].xy1 = D_80118264[14].xy0 = XY(-bx, by);
        D_80118264[14].xy1 = XY(-bx, by2);
        D_80118264[15].xy0 = XY(bx, -by2);
        D_80118264[15].xy1 = D_80118264[16].xy0 = XY(bx, -by);
        D_80118264[16].xy1 = D_80118264[17].xy0 = XY(-bx, -by);
        D_80118264[17].xy1 = D_80118264[18].xy0 = XY(-bx, by);
        D_80118264[18].xy1 = D_80118264[19].xy0 = XY(bx, by);
        D_80118264[19].xy1 = XY(bx, by2);
    } else {
        D_80118264[10].xy0 = D_80118264[10].xy1 = D_80118264[11].xy0 = D_80118264[11].xy1 =
        D_80118264[12].xy0 = D_80118264[12].xy1 = D_80118264[13].xy0 = D_80118264[13].xy1 =
        D_80118264[14].xy0 = D_80118264[14].xy1 = D_80118264[15].xy0 = D_80118264[15].xy1 =
        D_80118264[16].xy0 = D_80118264[16].xy1 = D_80118264[17].xy0 = D_80118264[17].xy1 =
        D_80118264[18].xy0 = D_80118264[18].xy1 = D_80118264[19].xy0 = D_80118264[19].xy1 = 0x04000400;
    }

    ox = lvl * 11;
    oy = lvl * 41 / 12;
    ix = ox - MIN(lvl * 32 / 12, 16);
    if (ox - MIN(lvl * 32 / 12, 16) < 0) {
        ix = 0;
    }
    iy = oy - MIN(lvl * 16 / 12, 8);
    if (oy - MIN(lvl * 16 / 12, 8) < 0) {
        iy = 0;
    }
    D_80118264[20].xy0 = D_80118264[21].xy0 = XY(-ox, -oy);
    D_80118264[20].xy1 = XY(-ox, -iy);
    D_80118264[21].xy1 = XY(-ix, -oy);
    D_80118264[22].xy0 = D_80118264[23].xy0 = XY(ox, -oy);
    D_80118264[22].xy1 = XY(ox, -iy);
    D_80118264[23].xy1 = XY(ix, -oy);
    D_80118264[24].xy0 = D_80118264[25].xy0 = XY(ox, oy);
    D_80118264[24].xy1 = XY(ox, iy);
    D_80118264[25].xy1 = XY(ix, oy);
    D_80118264[26].xy0 = D_80118264[27].xy0 = XY(-ox, oy);
    D_80118264[26].xy1 = XY(-ox, iy);
    D_80118264[27].xy1 = XY(-ix, oy);
    return 1;
}

#ifdef NON_MATCHING
/* regalloc (s-register order) and the line pointer init lands after the loop entry test */
u8 func_800451DC(u8 active) {
    s32 *level = &D_80105F30[5];
    s32 lvl;
    s32 i;
    s32 n;
    s32 x;
    s32 y;
    s32 cx;
    s32 cy;
    IfaceLine *line;

    if (!func_80040F08(active, level)) {
        return 0;
    }
    lvl = *level;
    if (lvl == 0 && active) {
        line = D_80118264;
        y = 0;
        x = 0;
        for (i = 0; i < 18; i++) {
            cx = POSITION - 10;
            EGSDisplayBuildLineF(line, 0x503028, ((x - cx) & 0xFFFF) | 0xFF6A0000, ((x - cx) & 0xFFFF) | 0x00960000);
            line->color.c[3] |= 2;
            line++;
            cy = D_8012C7FA - 6;
            EGSDisplayBuildLineF(line, 0x503028, ((y - cy) << 16) | 0xFF38, ((y - cy) << 16) | 0xC8);
            line->color.c[3] |= 2;
            line++;
            x += 21;
            y += 13;
        }
    }
    if (active) {
        n = lvl * 3;
        for (i = 0, line = D_80118264; i < n; i++, line++) {
            if (line->link == 0) {
                EGSDisplayAddPoly(gTextCamera, line);
            }
        }
        return 1;
    }
    if (gSpareTextWindow != 0xFF) {
        TextRemoveWindowNow(gSpareTextWindow);
        gSpareTextWindow = 0xFF;
    }
    n = (12 - lvl) * 3;
    if (n > 36) {
        n = 36;
    }
    for (i = 0, line = D_80118264; i < n; i++, line++) {
        if (line->link != 0) {
            EGSDisplayDeletePoly(gTextCamera, line);
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_800451DC);
#endif

u8 func_800453F8(u8 on) {
    s32 *level = &D_80105F30[6];

    if (!func_80040F08(on, level)) {
        return 0;
    }
    return func_801417D0(on, *level);
}

s32 InterfaceSetMode(s32 mode) {
    s32 old;

    if (mode != D_8010F8AC) {
        old = D_8010F8AC;
        D_80110020 = 1;
        if (gInterfaceOn == 0 && mode == 1) {
            D_8010F8AC = 0;
        } else {
            D_8010F8AC = mode;
        }
        gInterfaceCurrentMode = 9;
        return old;
    }
    return mode;
}

void func_800454B8(void) {
    if (D_8010F8AC != 2) {
        if (D_8010F8AC != 3) {
            return;
        }
    }
    if ((rand() & 0xF) == 0xF) {
        D_8010FFE8 = rand() % 6;
    } else if (D_8010FFE8 > 0) {
        D_8010FFE8--;
    }
}

void func_80045548(void) {
    s32 r = rand();

    if ((r & 0x3F) == (gSystemTick & 0x3F)) {
        TextAddString(gSpareTextWindow, gStringFillerText[(r >> 8) & 3], 20, 0);
    }
}

#ifdef NON_MATCHING
/* loops: the original does not strength-reduce the sprite field addresses (offsets stay in the sh/sw) and keeps the up-counting index in loops 2/3; here loop 1 counts down and loops 2/3 get reduced pointers */
void InterfaceSetSniperDoodads(s32 angle, s32 pitch, s32 zoom) {
    char buf[16];
    s32 hi;
    s32 lo;
    s32 h;
    s32 step;
    s32 mid;
    s32 off;
    s32 x;
    s32 y;
    s32 i;
    s32 n;
    char *msg;
    IfaceSprite *sp;

    switch (D_8010F8AC) {
    case 2:
        h = D_8010F8A2;
        hi = (s16)D_8010F8A0 / 2;
        lo = -(s16)D_8010F8A0 / 2;
        break;
    case 3:
        hi = 0;
        lo = -140;
        h = 100;
        break;
    default:
        return;
    }
    step = (hi - lo) * 3 / 10;
    angle *= 60;
    mid = (hi + lo) / 2;
    off = -(angle / 1024);
    sp = D_8010F8B8;
    if (sp != NULL) {
        for (i = 0, y = -60; i < 5; i++, sp++, y += 30) {
            n = y + off;
            if (n > h / 2 || n < -h / 2) {
                n = -256;
            }
            sp->y = n;
        }
    }
    off = -(pitch * 8 * step / 4096);
    sp = D_8010F8BC;
    for (i = 0, x = 0; i < 8; i++, sp++, x += step) {
        n = off + x + mid;
        if (n > hi) {
            n -= step * 8;
            off -= step * 8;
            if (n > hi) {
                sp->x = -256;
                continue;
            }
        }
        if (n < lo) {
            sp->x = -256;
        } else {
            sp->x = n;
        }
    }
    if (D_8010F8AC == 3) {
        n = 8 - (zoom - 181) * 8 / 646;
        for (i = 0; i < 8; i++) {
            if (i < n) {
                D_801186E4[i].color = 0x2840FF40;
            } else {
                D_801186E4[i].color = 0x28106010;
            }
        }
        return;
    }
    if (gSniperGobjHit >= 0 && gameObjectsInfo[gSniperGobjHit].runtime->npcState == 2) {
        if (D_8010F8F0 == -1) {
            D_8010F8F0 = gSystemTick + 10;
        } else if (D_8010F8F0 == gSystemTick) {
            TextAddString(gSpareTextWindow, gTargetLockMsg, 60, 0);
        } else if ((u32)gSystemTick >= (u32)(D_8010F8F0 + 10)) {
            msg = NULL;
            if (gSniperBonesHit & 1) {
                if (D_8010F8F9 != 1) {
                    msg = gHeadMsg;
                    D_8010F8F9 = 1;
                }
            } else if (gSniperBonesHit & 0x8000) {
                if (D_8010F8F9 != 5) {
                    msg = gWeaponMsg;
                    D_8010F8F9 = 5;
                }
            } else if (gSniperBonesHit & 0x104) {
                if (D_8010F8F9 != 3) {
                    msg = gHandMsg;
                    D_8010F8F9 = 3;
                }
            } else if (gSniperBonesHit & 0x6000) {
                if (D_8010F8F9 != 2) {
                    msg = gBodyMsg;
                    D_8010F8F9 = 2;
                }
            } else if (D_8010F8F9 != 4) {
                msg = gLimbMsg;
                D_8010F8F9 = 4;
            }
            if (msg != NULL) {
                sprintf(buf, gTargetFmt, msg);
                if (D_8010F8F4.id == 0xFFFF) {
                    D_8010F8F4.id = TextAddString(gSpareTextWindow, buf, -1, 0);
                } else {
                    D_8010F8F4.id = TextReplaceString(D_8010F8F4.id, buf);
                }
            }
            D_8010F8F0 = -2;
        }
    } else {
        if (TextIsStringShowing(D_8010F8F4.id)) {
            TextRemoveString(D_8010F8F4.id);
        }
        D_8010F8F4.id = 0xFFFF;
        D_8010F8F0 = -1;
        D_8010F8F9 = 0;
    }
    if (zoom == D_8010F8F4.zoom) {
        if (!D_8010F8F8) {
            D_8010F8F8 = 1;
            sprintf(buf, gZoomFmt, 0xA6F99 / (zoom * zoom));
            TextAddString(gSpareTextWindow, buf, 50, 0);
        }
    } else {
        D_8010F8F4.zoom = zoom;
        D_8010F8F8 = 0;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceSetSniperDoodads);
#endif

void InterfaceBuildMenuBackgroundPolys(IfacePoly4 *poly) {
    u32 i;
    s32 j;

    EGSDisplayBuildPoly4F(poly, 0, 0xFF88FF40, 0xFF8800C0, 0x0078FF40, 0x007800C0);
    poly->color.c[3] |= 2;
    poly->unk04 = 6;
    EGSDisplayAddPoly(gInterfaceWindow, poly);
    for (i = 0; i < 7; i++) {
        for (j = 0; j < 2; j++) {
            poly++;
            EGSDisplayBuildPoly4F(poly, 0, D_80011C18[D_80011D10[i][0]], D_80011C18[D_80011D10[i][1]],
                                  D_80011C18[D_80011D10[i][2]], D_80011C18[D_80011D10[i][3]]);
            poly->color.c[3] |= 2;
            poly->unk04 = 6;
            EGSDisplayAddPoly(gInterfaceWindow, poly);
        }
    }
}

void iInterfaceDoSniffer(void) {
    VECTOR32 scr;
    s32 color = 0x404DAEFF;
    s32 dist;
    s32 half;
    s32 i;
    IfaceSnifferLine *lines;

    if (mBombPosition.vx != 0 || mBombPosition.vy != 0 || mBombPosition.vz != 0) {
        EGSDisplayTransformWorldPointToScreen(&mBombPosition, *gCamera, &scr);
        dist = 0;
        if (scr.vz > 0 && (scr.vx < 0 ? -scr.vx : scr.vx) < 30) {
            dist = -((scr.vy < 0 ? -scr.vy : scr.vy) < 21) & 30;
        }
    } else {
        dist = 0;
    }
    lines = D_80118354;
    half = (s16)(D_80118364 - D_8011840C) / 2;
    if (half + 5 < dist) {
        dist = half + 5;
    }
    dist = (dist < half - 5) ? half - 5 : dist;
    if (dist == 30 && (gSystemTick & 4)) {
        color = 0x40070EDB;
        if (!(gSystemTick & 0x1B)) {
            TextAddString(gSpareTextWindow, gStringSnifferLock[((u32)gSystemTick >> 5) & 3], 20, 0);
        }
    }
    dist -= half;
    if (lines->color != color) {
        for (i = 17; i >= 0; i--) {
            lines[i].color = color;
        }
    }
    if (dist != 0) {
        for (i = 0; i < 5; i++) {
            lines[i].x0 += dist;
            lines[i].x1 += dist;
        }
        for (i = 5; i < 10; i++) {
            lines[i].x0 -= dist;
            lines[i].x1 -= dist;
        }
        D_80118308 += dist;
        D_80118320 -= dist;
        D_80118334 += dist;
        D_80118338 += dist;
        D_8011834C -= dist;
        D_80118350 -= dist;
    }
}

/* $gp commons: let maspsx use $gp for them (kept after the last function, in .text) */
SDATA_LOCAL(gInterfaceCurrentMode)
SDATA_LOCAL(gInterfaceWindow)
