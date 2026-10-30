#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "scratch.h"

/* interface.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F878)
SDATA_LOCAL(D_8010F879)
SDATA_LOCAL(D_8010F87A)
SDATA_LOCAL(D_8010F880)
SDATA_LOCAL(D_8010F882)
SDATA_LOCAL(D_8010F884)
SDATA_LOCAL(gSpareTextWindow)
SDATA_LOCAL(D_8010F887)
SDATA_LOCAL(gInterfaceOn)
SDATA_LOCAL(D_8010F88C)
SDATA_LOCAL(D_8010F890)
SDATA_LOCAL(D_8010F892)
SDATA_LOCAL(D_8010F894)
SDATA_LOCAL(D_8010F898)
SDATA_LOCAL(mSniperViewMax)
SDATA_LOCAL(D_8010F89E)
SDATA_LOCAL(D_8010F8A0)
SDATA_LOCAL(D_8010F8A2)
SDATA_LOCAL(gMenuPolys)
SDATA_LOCAL(gMenuLines)
SDATA_LOCAL(D_8010F8AC)
SDATA_LOCAL(D_8010F8B0)
SDATA_LOCAL(D_8010F8B1)
SDATA_LOCAL(D_8010F8B2)
SDATA_LOCAL(D_8010F8B4)
SDATA_LOCAL(D_8010F8B8)
SDATA_LOCAL(D_8010F8BC)
SDATA_LOCAL(D_8010F8C0)
SDATA_LOCAL(D_8010F8C4)
SDATA_LOCAL(D_8010F8E0)
SDATA_LOCAL(D_8010F8E4)
SDATA_LOCAL(D_8010F8E8)
SDATA_LOCAL(D_8010F8F0)
SDATA_LOCAL(D_8010F8F4)
SDATA_LOCAL(D_8010F8F8)
SDATA_LOCAL(D_8010F8F9)
SDATA_LOCAL(D_8010FFE8)
SDATA_LOCAL(D_8010FFEC)
SDATA_LOCAL(D_8010FFF0)
SDATA_LOCAL(D_8010FFF4)
SDATA_LOCAL(D_8010FFF8)
SDATA_LOCAL(D_80110000)
SDATA_LOCAL(D_80110004)
SDATA_LOCAL(D_80110008)
SDATA_LOCAL(D_8011000C)
SDATA_LOCAL(D_80110010)
SDATA_LOCAL(D_80110014)
SDATA_LOCAL(D_80110018)
SDATA_LOCAL(D_8011001C)
SDATA_LOCAL(D_80110020)
SDATA_LOCAL(gInterfaceCurrentMode)
SDATA_LOCAL(gInterfaceWindow)
SDATA_LOCAL(mTargetCursorPosition)

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
    /* 0x08 */ u8 unk08[0x1C];
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

extern s32 D_80105F30[];
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
extern s32 D_80105F48;
extern s32 D_80105F2C[]; /* per-mode animation step, [mode] for modes 1-7 */

void EGSDisplayAddPoly(void *camera, void *poly);
void EGSDisplayDeletePoly(void *camera, void *poly);
void EGSDisplayBuildPoly4F(void *poly, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void iInterfaceRadarRemove(void);
void iInterfaceRadarAdd(void);
void iDeleteTargetCursor(void);
void func_8003E13C(void);
u16 WeaponMakeRoundsString(u16 str, s32 weapon);
void TextRemoveString();
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

typedef struct IfacePoly30 {
    /* 0x00 */ s32 link;
    /* 0x04 */ u8 unk04[0x2C];
} IfacePoly30;

/* Weapon icon sprite ranges, indexed by IfaceWeaponInfo.sprite. */
typedef struct IfaceIconDef {
    u8 first;
    u8 count;
} IfaceIconDef;

extern IfacePoly30 D_80117B38[6];       /* radar blips */
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

extern IfacePoly24 D_80117C70[2];       /* radar */
extern IfacePoly24 D_80117CB8[2];
extern IfacePolyG3 D_80117D00;
extern IfaceLine D_80117D24[5];
extern IfacePoly24 D_801174A0;
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
extern s32 D_801185C4;
extern IfacePoly24 D_80117A30[6];
extern IfacePoly24 D_80117B08;
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

extern s32 D_80105F44;
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

/* system.h declares EGSListAdd as void; it returns the new node (egsmisc.c). */
#define EGS_LIST_ADD ((EGSListNode *(*)(EGSListNode **, void *))EGSListAdd)
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
extern char D_8010F8EC[];
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
extern struct { u16 id; u16 zoom; } D_8010F8F4; /* sniper text string, last zoom shown */
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

INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceSetup);

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

#ifdef NON_MATCHING
/* stack: the original's blend colour sits at sp+0x20 (frame 0x30); regalloc of the >>12 temp */
void func_8003C4F8(s32 slot, s32 level) {
    IfaceColor cur;
    IfaceColor want;
    IfaceColor mix;
    IfacePoly24 *blip;
    s32 i;
    s32 k;

    D_80117C58[slot] = level;
    blip = &D_80117A30[slot];
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
    want.w = level;
    cur.w = blip->color;
    for (i = 0; i < 3; i++) {
        if (cur.c[i] > want.c[i]) {
            want.c[i] = (want.c[i] < cur.c[i] - 10) ? cur.c[i] - 10 : want.c[i];
        } else {
            want.c[i] = (cur.c[i] + 10 < want.c[i]) ? cur.c[i] + 10 : want.c[i];
        }
    }
    blip->color = want.w | 0x28000000;
    if (gObjPlayer->target->targetId == D_80117B2C[slot] && gTargetModeFlag) {
        cur.w = want.w;
        want.w = 0x503028;
        k = (rcos(gSystemTick << 8) + 0x1000) >> 5;
        mix.c[3] = 0;
        for (i = 0; i < 3; i++) {
            mix.c[i] = (k * cur.c[i] + (0x100 - k) * want.c[i]) >> 8;
        }
        D_80117B14 = mix.w | 0x28000000;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003C4F8);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/interface", D_80011D3C);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003C724);

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
    for (i = 0, blip = D_80117B38, p = D_80117A30; i < 6; i++, p++) {
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
        D_80117C70[i].color = polyColor | 0x28000000;
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

#ifdef NON_MATCHING
/* stack layout: the original keeps both points in memory (lw-passed), loads off before storing xy */
void func_8003D53C(VECTOR32 *pos) {
    IfacePoint pt[2];
    u32 f;
    s32 c;

    if (pos != NULL) {
        if ((pos->vx < 0 ? -pos->vx : pos->vx) < 24 && (pos->vy < 0 ? -pos->vy : pos->vy) < 20) {
            *(s32 *)&pt[1] = 0x0050FF67; /* x -153, y 80 */
            f = ~gSystemTick & 7;
            c = f << 5;
            mBombPoly.color.word = ((c | 0x1F) << 16) | ((c & 0xFF) << 8) | (c & 0xFF) | 0x28000000;
            *(s32 *)&pt[0] = (u16)pos->vx | ((u16)pos->vy << 16);
            pt[0].vx += pt[1].vx;
            pt[0].vy += pt[1].vy;
            InterfacePlaceQuad(&mBombPoly, pt[0], f >> 1, f >> 1);
            mBombPoly.color.c[3] |= 2;
            return;
        }
    }
    mBombPoly.xy[0] = 0x04000400;
    mBombPoly.xy[1] = 0x04000400;
    mBombPoly.xy[2] = 0x04000400;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003D53C);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003D76C);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003E13C);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003E894);

s32 func_8003ECA0(big)
    u8 big;
{
    void *cam = *gCamera;
    s32 i;
    IfaceLine *line;

    if (D_801185C4) {
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

#ifdef NON_MATCHING
/* regalloc: the original keeps len in t2 (one extra move); otherwise the same code */
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
        lines[i].xy0 = lines[i - 12].xy0;
        lines[i].xy1 = lines[i - 12].xy1;
        lines[i].unk04 = lines[i - 12].unk04 + 4;
    }
    if (fresh) {
        for (i = 20; i >= 12; i--) {
            lines[i].xy0 = lines[i - 12].xy0;
            lines[i].xy1 = lines[i - 12].xy1;
            lines[i].unk04 = lines[i - 12].unk04 + 4;
        }
    } else {
        for (i = 20; i >= 12; i--) {
            lines[i].xy1 = 0x04000400;
            lines[i].xy0 = 0x04000400;
        }
    }
    if (scr->vz > 0) {
        t = (*(IfaceCamera **)gCamera)->h * 60 / scr->vz;
        size = 4;
        if (t >= 4) {
            size = t;
        }
        len = size * D_8010FFEC / 4;
        half = len / 2;
        prod = len * D_8010FFEC;
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
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003EF48);
#endif

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

#ifdef NON_MATCHING
/* regalloc in the minute/second split (sec computed in v1, then moved to a3) */
void InterfaceCountdownDisplay(void) {
    char buf[9];
    s32 min;
    s32 sec;
    s32 rem;
    s32 tick;

    if (D_80110008 > 0) {
        min = D_80110008 / 1200;
        rem = D_80110008 % 1200;
        sec = rem / 20;
        tick = rem % 20 / 3;
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
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceCountdownDisplay);
#endif

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

#ifdef NON_MATCHING
/* regalloc: scroll step and icon pointer swap s1/s2 */
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
                step = x >> 1;
                if ((u32)(x + 32) > 64) {
                    step = x >> 2;
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
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceWeaponSelect);
#endif

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

#ifdef NON_MATCHING
/* regalloc in the sprite block copies (the original keeps a copy of the sprite base in t2) */
void iSetupDirectionSprites(s32 mode) {
    IfaceSprite *spr;
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
    spr = D_8010F8B8;
    spr[4] = *src;
    *spr = spr[4];
    src = SpriteGet(0x34);
    spr = D_8010F8B8;
    spr[3] = *src;
    spr = D_8010F8B8;
    spr[1] = spr[3];
    src = SpriteGet(0x33);
    spr = D_8010F8B8;
    spr[2] = *src;
    for (i = 0; i < 5; i++) {
        D_8010F8B8[i].unk28 = (s32)EGS_LIST_ADD((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &D_8010F8B8[i]);
        D_8010F8B8[i].x = x;
    }
    for (i = 0; i < 8; i++) {
        D_8010F8BC[i].unk28 = (s32)EGS_LIST_ADD((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &D_8010F8BC[i]);
        D_8010F8BC[i].y = y;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", iSetupDirectionSprites);
#endif

void iDestroyDirectionSprites(void) {
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

void iAnimateDirectionSprites(s32 step) {
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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8004146C);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_80041BCC);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_80042DA4);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_80043B04);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_80044AC0);

#ifdef NON_MATCHING
/* regalloc (s-register order); also the original keeps &D_80105F44 in s1 across the first call (see func_800453F8) */
u8 func_800451DC(u8 active) {
    s32 *level = &D_80105F44;
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

#ifdef NON_MATCHING
/* the original keeps &D_80105F48 in s1 across the first call; GCC here rematerializes it */
u8 func_800453F8(u8 on) {
    s32 *level = (s32 *)&D_80105F48;

    if (func_80040F08(on, level) != 0) {
        return func_801417D0(on, *level);
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_800453F8);
#endif

#ifdef NON_MATCHING
/* regalloc: old lands in a1 instead of v1, so the gInterfaceOn load is scheduled above the sb */
s32 InterfaceSetMode(s32 mode) {
    s32 old = D_8010F8AC;

    if (old != mode) {
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
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceSetMode);
#endif

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
/* first draft: same flow, but register allocation and the first loop's -256 store differ (not hoisted in the original) */
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
            sp->y = (y + off > h / 2 || y + off < -h / 2) ? -256 : y + off;
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
