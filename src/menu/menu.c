#include "common.h"
#include "system.h"
#include "level.h"
#include "gobj.h"
#include "weapon.h"

/*
 * menu.ovl: the pause menu (options, controller setup, objectives and level map).
 */

/* Packs a map position into the two s16 halves of a screen xy word. */
#define MAP_XY(x, y) (((x) & 0xFFFF) | ((y) << 16))

/* libgpu-style semi-transparency bit in a primitive's code byte */
/* color plus primitive code, as one word */
#define setColorCode(p, c) (*(u32 *)&(p)->r = (c))


/* The highlight rectangle words hold two s16 halves (x, y; 10.6 fixed point) */
#define XY(v) (*(ScreenPos *)&(v))
/* Moves cur a quarter of the way to target, snapping once within half a step */
#define HILITE_STEP(cur, target, step)          \
    if (ABS((step) / 2) < ABS((target) - (cur))) { \
        (cur) += (step);                          \
    } else {                                      \
        (cur) = (target);                         \
    }

/* Objective index of a map-menu row (each objective row has two text ids) */
#define OBJECTIVE_INDEX(row) ((D_8013F7D0[row].textIds - &D_8013FE20[1]) / 2)
#define MAP_LOCATION(row) \
    ((row) != 0 ? &mObjectiveMapLocations[gSysInfo.level][OBJECTIVE_INDEX(row)] : &mPlayerMapLocation)
#define OBJECTIVE_ON_MAP(row) (gObjectives != NULL && (gObjectives->unk1C & (1 << OBJECTIVE_INDEX(row))))

/* 20.12 fixed point to int, rounding toward zero */
#define FIXED_INT(x) ((x) < 0 ? -(-(x) >> 12) : (x) >> 12)

#define setSemiTrans(p, abe) ((abe) ? ((p)->code |= 2) : ((p)->code &= ~2))

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    s16 x, y;
} ScreenPos;

/* libgs GsIMAGE */
typedef struct {
    /* 0x00 */ u32 pmode;
    /* 0x04 */ s16 px, py;
    /* 0x08 */ u16 pw, ph;
    /* 0x0C */ u32 *pixel;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u16 cw, ch;
    /* 0x18 */ u32 *clut;
} GsIMAGE;

typedef struct {
    /* 0x00 */ u32 *tim;
    /* 0x04 */ void *slot;
    /* 0x08 */ u8 animated;
    /* 0x09 */ u8 loaded;
    /* 0x0C */ GsIMAGE image;
    /* 0x28 */ s32 unk28;
} EGSTexture;

/* Flat line built by EGSDisplayBuildLineF (0x18 bytes) */
typedef struct MenuLine {
    /* 0x00 */ s32 data; /* nonzero while in the display list */
    /* 0x04 */ s32 z;
    /* 0x08 */ u32 tag;
    /* 0x0C */ u8 r, g, b, code; /* written as one word by setColorCode */
    /* 0x10 */ s16 x0, y0;
    /* 0x14 */ s16 x1, y1;
} MenuLine;

/* Flat quad built by EGSDisplayBuildPoly4F (0x24 bytes) */
typedef struct MenuQuad {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u32 tag;
    /* 0x0C */ u8 r, g, b, code; /* written as one word by setColorCode */
    /* 0x10 */ s16 x0, y0;
    /* 0x14 */ s16 x1, y1;
    /* 0x18 */ s16 x2, y2;
    /* 0x1C */ s16 x3, y3;
    /* 0x20 */ s32 unk20;
} MenuQuad;

/* A slider/frame on screen: two lines and two quads */
typedef struct MenuBox {
    /* 0x00 */ MenuLine lines[2];
    /* 0x30 */ MenuQuad quads[2];
} MenuBox; /* size 0x78 */

typedef struct MenuQuadPair {
    MenuQuad q[2];
} MenuQuadPair;

/* One menu line: a label plus (count - 1) option strings, each with its text id */
typedef struct MenuItem {
    /* 0x00 */ s32 count;
    /* 0x04 */ char **strings;
    /* 0x08 */ u16 *textIds;
} MenuItem; /* size 0xC */

/* 4-byte block copy (unaligned) */
typedef struct MenuExt {
    char s[4];
} MenuExt;

typedef struct MenuStackEntry {
    u8 menu;
    u8 cursor;
} MenuStackEntry;

/* Mission briefing text of the current level */
typedef struct Briefings {
    /* 0x00 */ char *names[3];
    /* 0x0C */ s32 count; /* pages */
    /* 0x10 */ char **texts;
} Briefings;


/* One displayed character / icon: a GsSPRITE plus two words (0x2C bytes, see text.c) */
typedef struct TextChar {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x, y;
    /* 0x08 */ u16 w, h;
    /* 0x0C */ u16 tpage;
    /* 0x0E */ u8 u, v;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u8 r, g, b, pad17;
    /* 0x18 */ s16 mx, my;
    /* 0x1C */ s16 scalex, scaley;
    /* 0x20 */ s32 rotate;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28; /* nonzero while displayed */
} TextChar;

/* Layout of what func_8013E98C allocates from the display packet area (0x15058 bytes) */
typedef struct MenuMemory {
    /* 0x00000 */ TextChar briefChars[300];
    /* 0x03390 */ TextChar menuChars[37];
    /* 0x039EC */ TextChar helpChars[25];
    /* 0x03E38 */ TextChar icons[16];
    /* 0x040F8 */ u8 buffer[0xF000];
    /* 0x130F8 */ u32 clut[0x80];
    /* 0x132F8 */ MenuQuad background[15];
    /* 0x13514 */ MenuQuad frame[2];
    /* 0x1355C */ MenuQuad polys[31];
    /* 0x139B8 */ MenuQuadPair pairs[8];
    /* 0x13BF8 */ MenuLine lines[132];
} MenuMemory;

/* Where an objective (or the player) is drawn on the level map */
typedef struct MapLocation {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 map; /* map icon index, 0xFF: not on any map */
    /* 0x02 */ s8 x;
    /* 0x03 */ s8 y;
} MapLocation;

/* "bin/menu.c" (the asserts here don't use the usual "s/..." path) */
extern char D_8013807C[];
#define ASSERT_MENU(cond, line) if (!(cond)) EGSDebugAssert(1, 0, D_8013807C, line)

/* The 0xF000-byte work buffer is reused: slider boxes, or the weapon description strings */
#define MENU_BOXES ((MenuBox *)D_8013FE78)
#define MENU_STRINGS ((char **)D_8013FE78)

/* menu.ovl statics, still defined in the overlay's asm .rodata/.data/.bss */
extern char D_801377E4[]; /* "" */
extern void (*D_80138044[])(s32 cursor); /* per-menu setup */
extern char *D_8013F744[];
extern char *D_8013F74C[]; /* help line per mode */
extern char *D_8013F778;
extern char *D_8013F780; /* quit prompt */
extern char *D_8013F784; /* restart-from-start prompt */
extern char *D_8013F788; /* restart prompt */
extern char *D_8013F774; /* "no objectives" */
extern char *D_8013F78C;
extern char *D_8013F790;
extern char *D_8013F794;
extern char *D_8013F798; /* objectives heading */
extern char *D_8013F79C; /* completed heading */
extern char *D_8013F7A0; /* map: "you are here" */
extern char *D_8013F7A4; /* map: objective location */
extern char *D_8013F7A8;
extern char *D_8013F7AC[]; /* controller configuration names */
extern u8 D_8013F7C0; /* menu text window */
extern u8 D_8013F7C1;
extern u8 D_8013F7C2; /* help line window */
extern u8 D_8013F7C3; /* weapon rounds window */
extern char *D_8013F7B8[2];
extern s32 D_8013F7C4; /* cursor / current value */
extern s32 D_8013F7C8; /* first item whose text still has to be built */
extern s32 D_8013F7CC;
extern MenuItem D_8013F86C[]; /* main menu */
extern MenuItem D_8013F7D0[]; /* briefing menu */
extern u16 *D_8013F904;
extern MenuItem D_8013F908[]; /* debug menu */
extern MenuItem D_8013FA7C[]; /* sound menu */
extern MenuItem D_8013FAA0;
extern MenuItem D_8013FBD8[]; /* controller menu */
extern char *D_8013FB98[][2]; /* objective rows: check mark, text */
extern char *D_8013FC80; /* dialog message */
extern s32 D_8013FC90;
extern char **D_8013FC94;
extern u16 *D_8013FC98;
extern u32 D_8013FC9C; /* dialog type */
extern void (*D_8013FCA0)(); /* dialog callback */
extern u32 D_8013FCA4; /* tick the menu opened at */
extern u32 D_8013FCA8;
extern MenuItem *D_8013FCBC; /* items of the current menu */ /* tick of the next briefing page turn */
extern u8 D_8013FCAC[];
extern s32 D_8013FCB4; /* current menu */
extern u8 D_8013FCB8; /* menu stack depth */
extern u8 D_8013FCC0[]; /* items per menu */
extern s32 D_8013FDE8; /* current help line */
extern MenuItem *D_8013FCD0[]; /* items of each menu */
extern void (*D_8013FD40[])(void); /* per-menu before-build handlers */
extern void (*D_8013FD78[])(void); /* per-menu after-build handlers */
extern void (*D_8013FDB0[])(void); /* per-menu leave handlers */
extern u8 D_8013FDEC[];
extern u8 D_8013FDF8[][9];
extern u16 D_8013FE20[]; /* text ids of the menu strings */
extern u16 D_8013FE26[];
#define D_8013FE66 D_8013FE20[35] /* status line text id */
extern TextChar *D_8013FE6C; /* briefing text characters */
extern TextChar *D_8013FE68; /* menu characters */
extern TextChar *D_8013FE70; /* help line characters */
extern s32 D_8013FE74; /* EGS file handle */
extern s32 D_8013FE88; /* menu hog header */
extern void *D_8013FE78; /* work buffer */
extern RECT D_8013FE7C; /* saved CLUT area */
extern u32 *D_8013FE84; /* CLUT backup */
extern s32 D_8013FE8C;
extern s32 D_8013FE90;
extern s32 D_8013FE94;
extern s32 D_8013FE98;
extern s32 D_8013FE9C; /* highlight step */
extern s32 D_8013FEA0;
extern s32 D_8013FEA4;
extern s32 D_8013FEA8;
extern MenuQuad *D_8013FEBC;
extern MenuQuadPair *D_8013FEC0;
extern MenuQuad *D_8013FEC4;
extern u16 D_8013FEC8[];
extern MenuStackEntry D_8013FED4[];
extern u8 mSfxSampleCanBePlayedAgain;
extern MapLocation *mObjectiveMapLocations[];
extern MapLocation mPlayerMapLocation;
extern s32 (*D_8013FB4C[])(s32 map, VECTOR32 *pos, ScreenPos *out); /* map locator per level */
extern s8 D_8013FAD0[][6]; /* player arrow shape per direction */
extern char *D_8013F740; /* "you" row text */
extern TextChar *mMap; /* weapon icons */

/* main.exe */
extern s32 gJoyMap[];
extern s32 gAmbientLight;
extern ScreenPos POSITION;
extern u16 D_8012D846;
extern void *gInterfaceWindow;
extern MenuLine *gMenuLines;
extern MenuQuad *gMenuPolys;
extern u8 gInterfaceOn;
extern u32 D_80105F80;
extern s32 gNpcAllOnRadarFlag;
extern u8 gSystemStopDebugDisplay;
extern s32 gColorCollisionFaces;
extern s32 npcPCInvisibleFlag;
extern Briefings *gBriefings;
void SysChangeLevel(u32 level);
void InterfaceSetMode(s32 mode);
void LevelDoRestart(void);
void SaveReturnToSOL(void);
void TextSetStringIntensity(u16 str, s32 r, s32 g, s32 b);
u16 TextAddString(u8 window, char *str, s32 ticks, TextChar *chars);
s32 TextStringLength(u16 str, s32 all);
void TextRemoveString(u16 str);
s32 TextRemoveWindow(u8 window);
s32 TextRemoveWindowNow(u8 window);
void TextSetWindowJustification(u8 window, s32 just);
void InterfacePlaceQuad(MenuQuad *quad, s32 xy, s32 arg2, s32 arg3);
void EGSDisplayAddPoly(void *window, void *poly);
void EGSDisplayDeletePoly(void *window, void *poly);
void EGSDisplayDeleteSprite(void *window, TextChar *sprite);
void LoadImage(RECT *rect, u32 *p);
void StoreImage(RECT *rect, u32 *p);
s32 NPCDebugGetNoDamageFlag(void);
void NPCSetShooting(s32 on);
void NPCPutAllOnRadar(s32 on);
s32 WeaponGetNextID(s32 weapon);
void WeaponSelect(Gobj *obj, s32 weapon, s32 arg2);
extern char *gUseMsgs[];
extern char *gButtonStrings[];
extern s32 gPacketAreaMallocSize;
extern s32 mIncomingInterfaceMode;
u8 JoystickGetFlightControls(void);
u8 LevelZoom(s32 arg0);
void TextSetWindowColor(u8 window, s32 r, s32 g, s32 b);
void TextSetWindowMini(u8 window, s32 on);
void FlushAllStreamRequests(void);
s32 ratan2(s32 y, s32 x);
void func_8013A744(s32 cursor);
void *EGSDisplayPacketAreaMalloc(u32 size);
s32 LevelGetMenuHogHeader(void);
void InterfaceBuildMenuBackgroundPolys(MenuQuad *polys);
void TextSpriteInitialize(TextChar *chars, s32 count);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextSetStringSpeed(u16 str, s32 speed);
void OpenHogForStreaming(char *path, s32 header);
void EGSDisplayFadeOff(void);
s32 EGSTextureCreate(EGSTexture *tex);
s32 EGSTextureCreateSprite(EGSTexture *tex, TextChar *sprite, s16 x, s16 y, s16 w, u16 h);
s32 EGSTextureLoad(EGSTexture *tex);
void TextActivateAllWindows(s32 on);
void EGSDisplayPacketAreaFree(void);
void InterfaceWeaponChange(void);
void func_8013AE8C(void);
s32 rcos(s32 a);
u16 TextAddPersistentString(char *str, TextChar *chars, s16 x, s16 y);
s32 LevelDoEOL(u8 force);
void WeaponUnlimitedAmmo(void);
void CASReset(s16 id);
void func_801417AC(void); /* level overlay (CATACOMB) */
void EGSDisplayAddSprite(void *window, TextChar *sprite);
void TextAddStringMulti(u8 window, s32 count, char **strs, s32 ticks, TextChar *chars, u16 *ids);
void WeaponMakeRoundsString(u16 str, s32 weapon);
u16 TextReplaceString(u16 str, char *text);
void TextSetStringJustification(u16 str, s32 just);
RECT *TextStringRect(u16 str, s32 all);
void TextUpdate(void);
void InterfaceAnimate(void);
void EGSDisplayBuildLineF(MenuLine *line, s32 color, s32 xy0, s32 xy1);
void EGSDisplayBuildPoly4F(MenuQuad *quad, s32 color, s32 xy0, s32 xy1, s32 xy2, s32 xy3);
void EGSDisplayBuildPoly4G(MenuQuad *quad, s32 c0, s32 xy0, s32 xy1, s32 xy2, s32 xy3, s32 c1, s32 c2, s32 c3);
s32 StreamHogBlock(char *name, void *dst, u32 size, s32 tag, void (*done)(s32));

/* this file */
void func_80138AD4(s32 x0, s32 y0, s32 x1, s32 y1);
void func_80138B1C(s32 menu);
void func_801389C8(s32 help);
void func_80139D64(s32 delta);
void func_80138DE0(s32 menu);
void func_80138EE4(void);
void func_80139230(MenuBox *box, s32 y, s32 width, s32 value);
void func_801394E4(MenuBox *box);
void func_8013A4F8(s32 page);
void func_8013B5E4(); /* display interrupt callback; also called with an argument */
void func_8013EE78(void);
void func_80138F8C(void);
void func_80138FD4(void);
void func_80139004(void);
void func_8013CD40(u32 tag);
u32 func_8013BB3C(void);
s32 func_8013BBA0(void);
void func_801387B0(void);
void func_8013C530(void);
void func_8013E5B0(void);
void func_8013E98C(void);
void func_8013F144(void);

void iMenuVolumeMusicStop(void) {
    SysPauseSound(1);
    SysStopSong(1, 0);
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
}

s32 MapLocateSubway(s32 map, VECTOR32 *pos, u32 *out) {
    s32 ret = 0;

    if (pos->vy >= 0x79F) {
        if (map == 0) {
            ret = 1;
            *out = MAP_XY(pos->vz / 114 - 0x3B, pos->vx / 95 - 0x45);
        }
    } else if (pos->vy >= 0x407) {
        if (map == 2) {
            ret = 1;
            *out = MAP_XY(pos->vx / 33 - 0x25, pos->vz / -34 + 0x6B);
        }
    } else if (pos->vy > 0) {
        if (map == 1) {
            ret = 1;
            *out = MAP_XY(pos->vx / 110 - 0x17, pos->vz / -138 - 4);
        }
    } else if (map == 1) {
        ret = 1;
        *out = MAP_XY(pos->vx / 118 + 0x1F, pos->vz / -140 - 6);
    }
    return ret;
}

s32 MapLocatePark(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 276 - 1, pos->vz / -280 + 3);
    return 1;
}

s32 MapLocateMuseum(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 95 + 0x45, pos->vz / -95 - 0x12);
    return 1;
}

s32 MapLocateMuseum2(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 51 + 0x22, pos->vz / -61 - 0xD6);
    return 1;
}

s32 MapLocateBaseExt(s32 map, VECTOR32 *pos, u32 *out) {
    if (pos->vy < -0x63) {
        return 0;
    }
    *out = MAP_XY(pos->vx / 107 - 0xD, pos->vz / -112 - 0x18);
    return 1;
}

s32 MapLocateChurch(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vz / 144 + 0x1B, pos->vx / 120 - 0xB);
    return 1;
}

s32 MapLocateChurch2(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vz / 133 + 0x36, pos->vx / 133 + 0x26);
    return 1;
}

s32 MapLocateWhouse(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 123 - 2, pos->vz / -150 + 0xE);
    return 1;
}

s32 MapLocateInWhouse(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 77 - 0x1B, pos->vz / -94 + 0x2A);
    return 1;
}

s32 MapLocateSilo(s32 map, VECTOR32 *pos, u32 *out) {
    *out = MAP_XY(pos->vx / 27 - 0x108, pos->vy / -33 + 0x1D);
    return 1;
}

#ifdef NON_MATCHING
/* stack frame is 8 bytes smaller (target keeps out at sp+0x20) */
void func_801387B0(void) {
    u32 a;
    u32 b;
    u8 out[4];
    s32 t;
    s32 i;

    if (D_8013F7C4 > 0) {
        a = 0x5C5CFF;
        b = 0xFFFF;
        t = (rcos(gSystemTick << 8) + 0x1000) >> 5;
        out[3] = 0;
        for (i = 0; i < 3; i++) {
            out[i] = (t * ((u8 *)&a)[i] + (0x100 - t) * ((u8 *)&b)[i]) >> 8;
        }
        a = 0xA4A45C;
        b = 0xC05C00;
        setColorCode(&D_8013FEC0[D_8013F7C4 - 1].q[0], *(u32 *)out | 0x28000000);
        t = (rcos(gSystemTick << 8) + 0x1000) >> 5;
        out[3] = 0;
        for (i = 0; i < 3; i++) {
            out[i] = (t * ((u8 *)&a)[i] + (0x100 - t) * ((u8 *)&b)[i]) >> 8;
        }
    } else {
        a = 0x5CFFFF;
        b = 0x5CFF;
        t = (rcos(gSystemTick << 8) + 0x1000) >> 5;
        out[3] = 0;
        for (i = 0; i < 3; i++) {
            out[i] = (t * ((u8 *)&a)[i] + (0x100 - t) * ((u8 *)&b)[i]) >> 8;
        }
    }
    setColorCode(D_8013FEBC, *(u32 *)out | 0x28000000);
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_801387B0);
#endif

void func_801389C8(s32 help) {
    TextChar *chars;
    char *str;

    if (D_8013FDE8 != help) {
        D_8013FDE8 = help;
        if (D_8013F7C2 != 0xFF) {
            if (help == 10) {
                TextRemoveWindow(D_8013F7C2);
            } else {
                TextRemoveWindowNow(D_8013F7C2);
            }
        }
        if (help != 10) {
            chars = D_8013FE70;
            str = D_8013F74C[help];
            TextSetWindowJustification(D_8013F7C2, 0);
            chars += TextStringLength(TextAddString(D_8013F7C2, str, -1, chars), 1);
            ASSERT_MENU(chars <= D_8013FE70 + 25, 1165);
        }
    }
}

void func_80138AD4(s32 x0, s32 y0, s32 x1, s32 y1) {
    D_8013FE8C = ((x0 << 6) & 0xFFFF) | (y0 << 22);
    D_8013FE90 = ((x1 << 6) & 0xFFFF) | (y1 << 22);
    D_8013FEA8 = 0;
    D_8013FEA4 = 0;
}

void func_80138B1C(s32 menu) {
    s32 old = D_8013FCB4;
    u8 sub;

    if (D_8013FDB0[old] != NULL && (menu != 0 || old != D_8013FCAC[D_8013FED4[D_8013FCB8].cursor])) {
        D_8013FDB0[old]();
    }
    if (old == 0) {
        sub = D_8013FCAC[D_8013F7C4];
        if (D_8013FDB0[sub] != NULL && menu != sub) {
            D_8013FDB0[sub]();
        }
    } else if (D_8013FE66 != 0xFFFF) {
        TextRemoveString(D_8013FE66);
        D_8013FE66 = 0xFFFF;
    }
    if ((menu == 0 && old == D_8013FCAC[D_8013FED4[D_8013FCB8].cursor]) ||
        (old == 0 && menu == D_8013FCAC[D_8013F7C4])) {
        if (old == 0) {
            D_8013F7C4 = 0;
            D_8013FCB4 = menu;
            D_8013FCBC = D_8013FCD0[menu];
            D_80138044[menu](0);
        }
        if (menu == 0) {
            D_80138044[old](0x7F);
        }
        D_8013FCB4 = menu;
    } else {
        TextRemoveWindow(D_8013F7C0);
        TextRemoveWindow(D_8013F7C3);
        D_8013FCB4 = menu;
        D_8013F7C4 = 0;
        D_8013FCA8 = gSystemTick + 10;
    }
    D_8013F7C8 = 0;
    D_8013FCBC = D_8013FCD0[D_8013FCB4];
    TextSetWindowColor(D_8013F7C0, 0x50, 0x82, 0xF0);
    if (menu == 6 && D_8013FC9C != 1) {
        TextSetWindowJustification(D_8013F7C0, 0);
    } else {
        TextSetWindowJustification(D_8013F7C0, -1);
    }
    SysPlaySong(5, 1, 0, 0);
}

void func_80138DE0(s32 menu) {
    s32 i;

    while (D_8013FCB8 >= 10) {
        for (i = 0; i < 10; i++) {
            D_8013FED4[i].menu = D_8013FED4[i + 1].menu;
            D_8013FED4[i].cursor = D_8013FED4[i + 1].cursor;
        }
        D_8013FCB8--;
    }
    D_8013FED4[D_8013FCB8].menu = D_8013FCB4;
    D_8013FED4[D_8013FCB8].cursor = D_8013F7C4;
    D_8013FCB8++;
    func_80138B1C(menu);
}

void func_80138EE4(void) {
    iMenuVolumeMusicStop();
    if (D_8013FCB8 != 0) {
        D_8013FCB8--;
        func_80138B1C(D_8013FED4[D_8013FCB8].menu);
        D_8013F7C4 = D_8013FED4[D_8013FCB8].cursor;
        D_80138044[D_8013FCB4](D_8013F7C4);
    } else {
        func_8013EE78();
    }
}

void func_80138F8C(void) {
    func_8013EE78();
    InterfaceSetMode(0);
    D_8012D846 &= ~1;
    SysHalt(5);
}

void func_80138FD4(void) {
    func_8013EE78();
    InterfaceSetMode(0);
    LevelDoRestart();
}

void func_80139004(void) {
    SaveReturnToSOL();
    func_8013EE78();
    InterfaceSetMode(0);
    SysChangeLevel(gSysInfo.level);
}

void func_80139044(char *msg, u32 type, s32 cursor, void (*done)()) {
    D_8013FC9C = type;
    if (D_8013FCB4 == 6) {
        func_80138B1C(6);
    } else {
        func_80138DE0(6);
    }
    D_8013FCA0 = done;
    D_8013FC80 = msg;
    D_8013F7C4 = cursor;
    switch (type) {
        case 0:
            func_801389C8(6);
        case 1:
            D_8013FCC0[6] = 1;
            D_8013FC90 = 0;
            D_8013FC94 = NULL;
            break;
        case 2:
            func_801389C8(1);
            D_8013FCC0[6] = 2;
            D_8013FC90 = 2;
            D_8013FC94 = D_8013F744;
            break;
    }
}

void func_80139150(MenuItem *item, s32 on, s32 enabled) {
    if (enabled) {
        if (on) {
            TextSetStringIntensity(item->textIds[1], 0x8C, 0xF0, 0x8C);
            TextSetStringIntensity(item->textIds[2], 0xA0, 0x5A, 0x5A);
        } else {
            TextSetStringIntensity(item->textIds[1], 0x5A, 0xA0, 0x5A);
            TextSetStringIntensity(item->textIds[2], 0xF0, 0x78, 0x78);
        }
    } else if (on) {
        TextSetStringIntensity(item->textIds[1], 0x5A, 0xA0, 0x5A);
        TextSetStringIntensity(item->textIds[2], 0x64, 0x3C, 0x3C);
    } else {
        TextSetStringIntensity(item->textIds[1], 0x3C, 0x64, 0x3C);
        TextSetStringIntensity(item->textIds[2], 0xA0, 0x5A, 0x5A);
    }
}

#ifdef NON_MATCHING
/* register allocation of the x/y invariants and the end-x register */
void func_80139230(MenuBox *box, s32 y, s32 width, s32 value) {
    MenuQuad *quad = box->quads;
    MenuLine *line = box->lines;
    s32 i = 0;
    s32 top = (y - 5) << 16;
    s32 half = width / 2;
    s32 xy0;
    s32 x1;

    for (; i < 2; i++, line++) {
        if (i == 0) {
            xy0 = ((-half - 0x39) & 0xFFFF) | top;
        } else {
            xy0 = ((half - 0x39) & 0xFFFF) | top;
        }
        x1 = value - half - 0x39;
        EGSDisplayBuildLineF(line, 0xF08250, xy0, (x1 & 0xFFFF) | top);
        setSemiTrans(line, 1);
        line->z = 5;
        EGSDisplayAddPoly(gInterfaceWindow, line);
    }
    for (i = 0; i < 2; i++, quad++) {
        EGSDisplayBuildPoly4F(quad, 0xF08250, 0, 0, 0, 0);
        quad->z = i + 3;
        setSemiTrans(quad, 1);
        EGSDisplayAddPoly(gInterfaceWindow, quad);
        InterfacePlaceQuad(quad, MAP_XY(x1, y - 5), i * 2 + 3, i + 1);
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_80139230);
#endif

void func_801393C4(MenuBox *box, s32 selected) {
    u32 color = 0xF08250;
    MenuLine *line;
    MenuQuad *quad;
    s32 trans;

    if (selected) {
        color = 0xF0B4B4;
    }
    line = box->lines;
    quad = box->quads;
    trans = selected == 0;
    setColorCode(line, color | 0x40000000);
    setSemiTrans(line, trans);
    line++;
    setColorCode(line, color | 0x40000000);
    setSemiTrans(line, 1);
    setSemiTrans(quad, 1);
    quad++;
    setColorCode(quad, color | 0x28000000);
    setSemiTrans(quad, trans);
}

void func_80139470(MenuBox *box, s32 value) {
    s32 xy;
    s32 x = box->lines[0].x0 + value;

    xy = (x & 0xFFFF) | (box->lines[0].y0 << 16);
    box->lines[0].x1 = x;
    box->lines[1].x1 = x;
    InterfacePlaceQuad(&box->quads[0], xy, 3, 1);
    InterfacePlaceQuad(&box->quads[1], xy, 5, 2);
}

void func_801394E4(MenuBox *box) {
    MenuQuad *quad = box->quads;
    MenuLine *line;

    if (box->lines[0].data != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, &box->lines[0]);
    }
    line = &box->lines[1];
    if (box->lines[1].data != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, line);
    }
    if (box->quads[0].data != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, quad);
    }
    quad = &box->quads[1];
    if (box->quads[1].data != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, quad);
    }
}

void func_80139590(EGSTexture *tex) {
    if (D_8013F7CC != -1) {
        if (D_8013FE7C.x != tex->image.cx || D_8013FE7C.y != D_8013FE7C.x || D_8013FE7C.w != tex->image.cw ||
            D_8013FE7C.h != tex->image.ch) {
            DrawSync(0);
            LoadImage(&D_8013FE7C, D_8013FE84);
            D_8013F7CC = -1;
        }
        if (D_8013F7CC != -1) {
            return;
        }
    }
    D_8013FE7C.x = tex->image.cx;
    D_8013FE7C.y = tex->image.cy;
    D_8013FE7C.w = tex->image.cw;
    D_8013FE7C.h = tex->image.ch;
    DrawSync(0);
    StoreImage(&D_8013FE7C, D_8013FE84);
}

void func_801396B4(void) {
    s32 cursor = D_8013F7C4;

    if (cursor != 1 && cursor != 2 && (cursor != 0 || D_8013FCC0[8])) {
        func_80138DE0(D_8013FCAC[cursor]);
    }
}

void func_80139710(void) {
    func_8013A4F8(D_8013F7C4 + 1);
}

INCLUDE_RODATA("asm/menu/nonmatchings/menu", D_801377E4);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mSubwayMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mSubway2MapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mParkMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mBaseExtMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mMuseumMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mMuseum2MapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mChurch2MapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mWhouseMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", mSiloMapLocations);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", D_80138044);

INCLUDE_RODATA("asm/menu/nonmatchings/menu", D_8013807C);

#ifdef NON_MATCHING
/* gObjectives/0xFF swap v0/v1 and gMissionFailed store is scheduled early */
void func_80139738(void) {
    Gobj *player;

    switch (D_8013F7C4) {
        case 0:
            func_80138DE0(2);
            break;
        case 1:
        case 2:
            gObjectives->unk24 = gObjectives->completed = gObjectives->unk18 = gObjectives->unk1C = 0xFF;
            gObjectives->unk20 = gObjectives->unk14 = gMissionFailed = 0;
            if (D_8013F7C4 == 1 && LevelZoom(0) && gSysInfo.level == 13) {
                func_801417AC();
            }
            func_8013EE78();
            break;
        case 5:
            player = gameObjectsInfo[gPlayerOffset].runtime;
            player->healthInfo->health = 150;
            player->healthInfo->armor = 600;
            if (player->cas->active == 6) {
                CASReset(player->id);
            }
            func_8013EE78();
            break;
        case 7:
            WeaponUnlimitedAmmo();
        case 4:
            func_8013EE78();
            break;
        case 11:
            func_8013EE78();
            LevelDoEOL(1);
            break;
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_80139738);
#endif

void func_801398A4(void) {
    switch (D_8013F7C4) {
        case 0:
            if (gBriefings != NULL) {
                func_80138DE0(12);
            } else {
                func_80138B1C(5);
            }
            break;
        case 1:
            func_80139044(D_8013F784, 2, 0, func_80139004);
            break;
        case 2:
            func_80139044(D_8013F788, 2, 0, func_80138FD4);
            break;
        case 4:
            func_80138DE0(3);
            break;
        case 5:
            func_80139044(D_8013F780, 2, 0, func_80138F8C);
            break;
        case 3:
            func_80138DE0(2);
            break;
        case 6:
            func_80138DE0(1);
            break;
    }
}

void func_80139994(void) {
    func_8013EE78();
    SysChangeLevel(D_8013F7C4);
}

void func_801399C4(void) {
    switch (D_8013F7C4) {
        case 6:
            func_80138DE0(4);
            break;
        case 4:
            func_80138DE0(9);
            break;
        case 5:
            func_80138DE0(13);
            func_80138AD4(0, 0, 0, 0);
            break;
    }
}

void func_80139A50(s32 arg0) {
    s32 n = D_8013FED4[D_8013FCB8].cursor;
    s32 weapon = 1;

    if (gObjPlayer->cas->active != 8) {
        if (mMap[n].unk28 != 0) {
            EGSDisplayDeleteSprite(gInterfaceWindow, &mMap[n]);
        }
        for (n--; n != -1; n--) {
            weapon = WeaponGetNextID(weapon);
        }
        func_8013EE78();
        WeaponSelect(gObjPlayer, weapon, 0);
    } else {
        func_80139044(D_8013F778, 0, 0, NULL);
    }
}

#ifdef NON_MATCHING
/* buffer pointer and string swap registers in the range check */
void func_80139B54(void) {
    s32 i;

    for (i = 0; i < 26; i++) {
        if (MENU_STRINGS[i] < (char *)D_8013FE78 || MENU_STRINGS[i] > (char *)D_8013FE78 + 0xF000) {
            StreamSynchronous();
        }
    }
    func_80139044(MENU_STRINGS[D_8013F7D0[D_8013F7C4].textIds - D_8013FE20], 1, 0, func_80139A50);
    func_801389C8(gObjPlayer->cas->active != 8 ? 7 : 2);
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_80139B54);
#endif

void func_80139C54(void) {
    s32 cursor = D_8013F7C4;

    func_80138EE4();
    switch (D_8013FC9C) {
        case 0:
        case 1:
            if (D_8013FCA0 != NULL) {
                D_8013FCA0(1);
            }
            break;
        case 2:
            if (cursor != 0) {
                D_8013FCA0(1);
            }
            break;
    }
}

void func_80139CE4(void) {
    gAmbientLight = (D_8013F7C4 << 16) + (D_8013F7C4 << 8) + D_8013F7C4;
    func_80138EE4();
}

void func_80139D24(void) {
    ((ScreenPos *)&D_8013F7C4)->x = POSITION.x;
    ((ScreenPos *)&D_8013F7C4)->y = POSITION.y;
    func_80138EE4();
}

#ifdef NON_MATCHING
/* cursor and item pointer swap s0/s1 */
void func_80139D64(s32 cursor) {
    MenuItem *item;
    MenuItem *p;
    RECT *rect;
    s32 i;

    if (D_80138044[D_8013FCB4] == func_80139D64) {
        func_801389C8(1);
    }
    if (gSystemTick >= D_8013FCA8) {
        if (D_8013F7C4 < D_8013FCC0[D_8013FCB4]) {
            if (*D_8013FCBC[D_8013F7C4].textIds != 0xFFFF) {
                TextSetStringIntensity(*D_8013FCBC[D_8013F7C4].textIds, 0x50, 0x82, 0xF0);
            }
        }
    }
    if (cursor != 0x7F) {
        if (cursor < 0) {
            cursor = D_8013FCC0[D_8013FCB4] - 1;
        } else if (cursor >= D_8013FCC0[D_8013FCB4]) {
            cursor = 0;
        }
        D_8013F7C4 = cursor;
        if (gSystemTick >= D_8013FCA8) {
            item = &D_8013FCBC[cursor];
            if (*item->textIds != 0xFFFF) {
                rect = TextStringRect(*item->textIds, 0);
                rect->x -= 3;
                rect->y -= 2;
                rect->w += 6;
                rect->h += 4;
                func_80138AD4(rect->x, rect->y, rect->w, rect->h);
                TextSetStringIntensity(*item->textIds, 0xB4, 0xB4, 0xF0);
            } else {
                for (i = cursor, p = item; i >= 0; i--, p--) {
                    if (*p->textIds != 0xFFFF) {
                        break;
                    }
                }
                D_8013F7C8 = i + 1;
                for (; i >= 0; i--) {
                    *D_8013FCBC[i].textIds = 0xFFFF;
                }
                TextRemoveWindowNow(D_8013F7C0);
                TextRemoveWindowNow(D_8013F7C3);
                func_8013F144();
            }
        }
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_80139D64);
#endif

void func_80139FFC(s32 cursor) {
    s32 i;
    s32 first = 0x70; /* main menu frame lines in gMenuLines */
    s32 row;
    MenuLine *lines;

    if (cursor < 0) {
        cursor = 4;
    } else if ((u32)cursor >= 5) {
        cursor = 0;
    }
    func_801389C8(cursor == 1 || cursor == 2 || (cursor == 0 && !D_8013FCC0[8]) ? 9 : 0);
    if (cursor != D_8013F7C4) {
        if (D_8013FDB0[D_8013FCAC[D_8013F7C4]] != NULL) {
            D_8013FDB0[D_8013FCAC[D_8013F7C4]]();
        }
        TextSetStringIntensity(D_8013F86C[D_8013F7C4].textIds[0], 0x50, 0x82, 0xF0);
        row = D_8013F7C4 * 4;
        lines = gMenuLines;
        for (i = 0; i < 4; i++) {
            setColorCode(&lines[row + (first + i)], 0x40F08250);
        }
        D_8013F7C4 = cursor;
        TextRemoveWindow(D_8013F7C0);
        TextRemoveWindow(D_8013F7C3);
        D_8013FCA8 = gSystemTick + 10;
    }
    TextSetStringIntensity(D_8013F86C[D_8013F7C4].textIds[0], 0xB4, 0xB4, 0xF0);
    row = D_8013F7C4 * 4;
    lines = gMenuLines;
    for (i = 0; i < 4; i++) {
        setColorCode(&lines[row + (first + i)], 0x40F0B4B4);
    }
    func_80138AD4(0x32, D_8013F7C4 * 16 + 0x14, 0x6E, 0xB);
}

void func_8013A220(s32 cursor) {
    s32 dir = cursor - D_8013F7C4;
    MenuItem *item;
    RECT *rect;

    func_801389C8(2);
    if (D_8013FCC0[D_8013FCB4] == 1) {
        goto reset;
    }
    if (dir != 0) {
        if (D_8013F7C4 < D_8013FCC0[D_8013FCB4]) {
            item = &D_8013FCBC[D_8013F7C4];
            if (item->textIds[0] != 0xFFFF) {
                TextSetStringIntensity(item->textIds[0], 0x50, 0x82, 0xF0);
            }
            if (item->textIds[1] != 0xFFFF) {
                TextSetStringIntensity(item->textIds[1], 0x50, 0x82, 0xF0);
            }
        }
    } else {
        dir = 1;
    }
    if (cursor == 0x7F) {
    reset:
        func_80138AD4(0, 0, 0, 0);
        return;
    }
    if (cursor < 0) {
        cursor = D_8013FCC0[D_8013FCB4] - 1;
    } else if (cursor >= D_8013FCC0[D_8013FCB4]) {
        cursor = 0;
    }
    if (D_8013F7D0[cursor].count == 1) {
        do {
            cursor += dir;
            if (cursor <= 0) {
                cursor = D_8013FCC0[D_8013FCB4] - 1;
            } else if (cursor >= D_8013FCC0[D_8013FCB4]) {
                cursor = 0;
            }
        } while (D_8013F7D0[cursor].count == 1);
    }
    item = &D_8013FCBC[cursor];
    D_8013F7C4 = cursor;
    ASSERT_MENU(item->textIds[1] != 0xFFFF, 2117);
    rect = TextStringRect(item->textIds[1], 0);
    rect->x -= 3;
    rect->y -= 2;
    rect->w += 6;
    rect->h += 4;
    func_80138AD4(rect->x, rect->y, rect->w, rect->h);
    TextSetStringIntensity(item->textIds[0], 0xB4, 0xB4, 0xF0);
    TextSetStringIntensity(item->textIds[1], 0xB4, 0xB4, 0xF0);
}

void func_8013A4F8(s32 page) {
    if (D_8013FCA8 < gSystemTick) {
        if (page < 0 || page >= gBriefings->count) {
            func_80138EE4();
            return;
        }
        D_8013F7C4 = page;
        TextRemoveString(*D_8013F7D0[D_8013FCC0[12] - 1].textIds);
        D_8013FCC0[12] = 1;
        D_8013FCA8 = gSystemTick + 10;
        D_8013F7D0[0].strings = &gBriefings->texts[D_8013F7C4];
        SysPlaySong(5, 2, 0, 0);
    } else {
        func_801389C8(3);
    }
}

void func_8013A60C(s32 cursor) {
    u16 *ids;
    RECT *rect;

    if (D_8013FC9C < 2) {
        func_80138AD4(0, 0, 0, 0);
    } else {
        ids = D_8013FC98;
        if (cursor >= D_8013FC90) {
            cursor = 0;
        } else if (cursor < 0) {
            cursor = D_8013FC90 - 1;
        }
        TextSetStringIntensity(ids[D_8013F7C4], 0x50, 0x82, 0xF0);
        D_8013F7C4 = cursor;
        TextSetStringIntensity(ids[cursor], 0xB4, 0xB4, 0xF0);
        rect = TextStringRect(ids[D_8013F7C4], 0);
        rect->x -= 3;
        rect->y -= 2;
        rect->w += 6;
        rect->h += 4;
        func_80138AD4(rect->x, rect->y, rect->w, rect->h);
    }
}

void func_8013A744(s32 cursor) {
    s32 old;
    MapLocation *loc;
    s32 map;
    s32 i;
    s32 xy;
    TextChar *chars;

    if (D_8013F7C4 > 0 && D_8013F7C4 < D_8013FCC0[8]) {
        setColorCode(&D_8013FEC0[D_8013F7C4 - 1].q[0], 0x28C08000);
    }
    old = D_8013F7C4;
    func_8013A220(cursor);
    if (cursor == 0x7F) {
        return;
    }
    loc = MAP_LOCATION(D_8013F7C4);
    map = loc->map;
    if (map != 0xFF && (D_8013F7C4 == 0 || OBJECTIVE_ON_MAP(D_8013F7C4))) {
        func_8013AE8C();
        mMap[map].unk28 = (s32)EGSListAdd((EGSListNode **)((u8 *)gInterfaceWindow + 0x90), &mMap[map]);
        for (i = 0; i < D_8013FCC0[8]; i++) {
            if (D_8013F7D0[i].count > 0) {
                loc = MAP_LOCATION(i);
                if (loc->map == map) {
                    xy = MAP_XY(loc->x - 0x39, loc->y - 5);
                    if (i != 0 && OBJECTIVE_ON_MAP(i)) {
                        if (D_8013F7C4 == i) {
                            D_8013FEC0[i - 1].q[0].z = 0;
                        } else {
                            setColorCode(&D_8013FEC0[i - 1].q[0], 0x28C08000);
                            D_8013FEC0[i - 1].q[0].z = 1;
                        }
                        InterfacePlaceQuad(&D_8013FEC0[i - 1].q[0], xy, 1, 1);
                        InterfacePlaceQuad(&D_8013FEC0[i - 1].q[1], xy, 2, 2);
                    } else if (D_8013FEBC->data == 0) {
                        EGSDisplayAddPoly(gInterfaceWindow, &D_8013FEBC[0]);
                        EGSDisplayAddPoly(gInterfaceWindow, &D_8013FEBC[1]);
                    }
                }
            }
        }
        if (D_8013FE66 != 0xFFFF) {
            TextRemoveString(D_8013FE66);
            D_8013FE66 = 0xFFFF;
        }
    } else if (D_8013FE66 == 0xFFFF) {
        chars = &D_8013FE6C[271];
        if (D_8013F7C4 == 0) {
            D_8013FE66 = TextAddPersistentString(D_8013F7A0, chars, -0x39, -0x11);
        } else {
            D_8013FE66 = TextAddPersistentString(D_8013F7A4, chars, -0x39, -0xD);
        }
        if (D_8013FE66 != 0xFFFF) {
            TextSetStringSpeed(D_8013FE66, 0x2000);
            TextSetStringJustification(D_8013FE66, 0);
            TextSetStringIntensity(D_8013FE66, 0xF0, 0xB4, 0xB4);
        }
    } else if (old == 0 || D_8013F7C4 == 0) {
        if (D_8013F7C4 == 0) {
            D_8013FE66 = TextReplaceString(D_8013FE66, D_8013F7A0);
        } else {
            D_8013FE66 = TextReplaceString(D_8013FE66, D_8013F7A4);
        }
    }
}

void func_8013AC64(u32 value) {
    MenuBox *box = D_8013FE78;

    if (value < 31) {
        D_8013F7C4 = value;
        *(s32 *)&box->lines[0].x0 = MAP_XY(-0x91, value * 5 - 0x5F);
        *(s32 *)&box->lines[0].x1 = MAP_XY(0x1F, value * 5 - 0x5F);
    }
}

void func_8013ACA4(void) {
}

void func_8013ACAC(s32 delta) {
    u16 ids[2];
    s32 n;
    s32 weapon;

    if (mMap[D_8013F7C4].unk28 != 0) {
        EGSDisplayDeleteSprite(gInterfaceWindow, &mMap[D_8013F7C4]);
    }
    TextRemoveWindowNow(D_8013F7C3);
    func_80139D64(delta);
    func_801389C8(1);
    if (delta != 0x7F) {
        n = D_8013F7C4;
        weapon = 1;
        for (n--; n != -1; n--) {
            weapon = WeaponGetNextID(weapon);
        }
        EGSDisplayAddSprite(gInterfaceWindow, &mMap[D_8013F7C4]);
        if (weapon == 24 || gWeaponInfo[weapon].clipSize * ((gWeaponInfo[weapon].flags & 7) + 1) != 0) {
            TextAddStringMulti(D_8013F7C3, 2, D_8013F7B8, -1, &D_8013FE6C[285], ids);
            WeaponMakeRoundsString(ids[1], weapon);
        }
    }
}

void func_8013AE30(void) {
    s32 i;

    D_8013F7CC = -1;
    for (i = 0; i < 3; i++) {
        func_801394E4(&MENU_BOXES[i]);
    }
}

void func_8013AE8C(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (mMap[i].unk28 != 0) {
            EGSDisplayDeleteSprite(gInterfaceWindow, &mMap[i]);
        }
    }
    if (D_8013FEBC->data != 0) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_8013FEBC[0]);
        EGSDisplayDeletePoly(gInterfaceWindow, &D_8013FEBC[1]);
    }
    for (i = 0; i < 8; i++) {
        if (D_8013FEC0[i].q[0].data != 0) {
            EGSDisplayDeletePoly(gInterfaceWindow, &D_8013FEC0[i].q[0]);
            EGSDisplayDeletePoly(gInterfaceWindow, &D_8013FEC0[i].q[1]);
        }
    }
    if (D_8013FE66 != 0xFFFF) {
        TextRemoveString(D_8013FE66);
        D_8013FE66 = 0xFFFF;
    }
}

void func_8013AFC8(void) {
    MenuBox *box = D_8013FE78;
    MenuLine *line = &box->lines[1];

    EGSDisplayDeletePoly(gInterfaceWindow, &box->lines[0]);
    EGSDisplayDeletePoly(gInterfaceWindow, line);
    gAmbientLight = (D_8013F7C4 << 16) + (D_8013F7C4 << 8) + D_8013F7C4;
    setSemiTrans(D_8013FEC4, 1);
}

void func_8013B04C(void) {
    s32 i;

    InterfaceSetMode(4);
    *(s32 *)&D_8013FEC4->x0 = MAP_XY(-0xC0, -0x78);
    *(s32 *)&D_8013FEC4->x1 = MAP_XY(0xC0, -0x78);
    *(s32 *)&D_8013FEC4->x2 = MAP_XY(-0xC0, 0x78);
    *(s32 *)&D_8013FEC4->x3 = MAP_XY(0xC0, 0x78);
    for (i = 1; i < 15; i++) {
        EGSDisplayAddPoly(gInterfaceWindow, &D_8013FEC4[i]);
    }
    for (i = 0; i < 5; i++) {
        TextSetStringIntensity(D_8013FEC8[i], 0x50, 0x82, 0xF0);
    }
    POSITION.x = ((ScreenPos *)&D_8013F7C4)->x;
    POSITION.y = ((ScreenPos *)&D_8013F7C4)->y;
    D_8012D846 &= ~1;
}

s32 func_8013B14C(void) {
    s32 flags = (NPCDebugGetNoDamageFlag() != 0) << 3;

    if (gNpcAllOnRadarFlag) {
        flags |= 0x200;
    }
    if (!gSystemStopDebugDisplay) {
        flags |= 0x40;
    }
    if (gColorCollisionFaces) {
        flags |= 0x100;
    }
    if (npcPCInvisibleFlag) {
        flags |= 0x400;
    }
    return flags;
}

void funcEvSpIOEx(s32 delta) {
    s32 flags = func_8013B14C();
    MenuItem *item = &D_8013F908[D_8013F7C4];

    if (item->count == 3) {
        func_80139150(item, (flags & (1 << D_8013F7C4)) == (1 << D_8013F7C4), 0);
    }
    func_80139D64(delta);
    func_801389C8(1);
    item = &D_8013F908[D_8013F7C4];
    if (item->count == 3) {
        func_80139150(item, (flags & (1 << D_8013F7C4)) == (1 << D_8013F7C4), 1);
    }
}

s32 func_8013B2B0(void) {
    MenuItem *item = &D_8013F908[D_8013F7C4];

    if (item->count != 3) {
        return 0;
    }
    func_80139150(item, 1, 1);
    switch (D_8013F7C4) {
        case 3:
            NPCSetShooting(0);
            break;
        case 9:
            NPCPutAllOnRadar(1);
            break;
        case 6:
            gSystemStopDebugDisplay = 0;
            break;
        case 8:
            gColorCollisionFaces = 1;
            break;
        case 10:
            npcPCInvisibleFlag = 1;
            break;
    }
    return 1;
}

s32 func_8013B38C(void) {
    MenuItem *item = &D_8013F908[D_8013F7C4];

    if (item->count != 3) {
        return 0;
    }
    func_80139150(item, 0, 1);
    switch (D_8013F7C4) {
        case 3:
            NPCSetShooting(1);
            break;
        case 9:
            NPCPutAllOnRadar(0);
            break;
        case 6:
            gSystemStopDebugDisplay = 1;
            break;
        case 8:
            gColorCollisionFaces = 0;
            break;
        case 10:
            npcPCInvisibleFlag = 0;
            break;
    }
    return 1;
}

void func_8013B464(s32 delta) {
    MenuItem *item;
    u8 stop = 1;

    item = &D_8013FA7C[D_8013F7C4];
    switch (D_8013F7C4) {
        case 0:
        case 1:
        case 2:
            func_801393C4(&MENU_BOXES[D_8013F7C4], 0);
            break;
        case 3:
            func_80139150(item, gInterfaceOn, 0);
            break;
    }
    func_80139D64(delta);
    if (delta != 0x7F) {
        func_801389C8(D_8013F7C4 >= 4 ? 1 : 2);
        item = &D_8013FA7C[D_8013F7C4];
        switch (D_8013F7C4) {
            case 0:
            case 1:
            case 2:
                stop = 0;
                func_801393C4(&MENU_BOXES[D_8013F7C4], 1);
                break;
            case 3:
                func_80139150(item, gInterfaceOn, 1);
                break;
        }
        if (stop) {
            iMenuVolumeMusicStop();
        } else {
            SysPauseSound(0);
        }
    }
}

void func_8013B5E4() {
    mSfxSampleCanBePlayedAgain = 1;
}

void func_8013B5F8(void) {
    VECTOR32 pos;

    SysPauseSound(0);
    if (mSfxSampleCanBePlayedAgain) {
        pos = gObjPlayer->move->pos;
        SysPlaySong(1, 0, 0, (s32)&pos);
        mSfxSampleCanBePlayedAgain = 0;
        EGSDisplayQueueInterrupt(func_8013B5E4, (D_80105F80 >> 26) & 0x1F, 0xE0C00000);
    }
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
}

void func_8013B6AC(void) {
    SysPauseSound(0);
    SysStopSong(1, 0);
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
}

void func_8013B6F4(void) {
    SysPauseSound(0);
    if (mSfxSampleCanBePlayedAgain && !SysSoundIsXAPlaying()) {
        SysStopSong(1, 0);
        SysPlayXA(0xD0, 0, 0);
        mSfxSampleCanBePlayedAgain = 0;
        EGSDisplayQueueInterrupt(func_8013B5E4, ((D_80105F80 >> 26) & 0x1F) + 10, 0xE0C00000);
    }
}

void func_8013B780(s32 delta) {
    s32 channel = D_8013F7C4;
    s16 vol = SysSoundGetMasterVolume(channel) + delta;

    if (vol < 0) {
        vol = 0;
    }
    if (vol >= 0x80) {
        vol = 0x7F;
    }
    SysSoundSetMasterVolume(channel, vol);
    func_80139470(&MENU_BOXES[channel], vol);
}

s32 func_8013B820(s32 delta) {
    MenuItem *item = &D_8013FA7C[D_8013F7C4];

    switch (D_8013F7C4) {
        case 0:
            func_8013B780(delta);
            func_8013B5F8();
            return 1;
        case 1:
            func_8013B780(delta);
            func_8013B6AC();
            return 1;
        case 2:
            func_8013B780(delta);
            func_8013B6F4();
            return 1;
        case 3:
            gInterfaceOn = (u32)delta >> 31;
            func_80139150(item, (u32)delta >> 31, 1);
            return 1;
    }
    return 0;
}

void func_8013B8F8(s32 cursor) {
    MenuItem *item = &D_8013FBD8[D_8013F7C4];
    s32 help;

    switch (D_8013F7C4) {
        case 0:
            func_80139150(item, EGSDeviceControllerVibrate(5, 0, 0) == 7, 0);
            break;
        case 1:
            func_80139150(item, JoystickGetFlightControls(), 0);
            break;
        case 13:
            TextSetStringIntensity(item->textIds[2], 0x50, 0x82, 0xF0);
        default:
            TextSetStringIntensity(item->textIds[1], 0x50, 0x82, 0xF0);
            break;
    }
    if (cursor == 12) {
        cursor = 24 - D_8013F7C4;
    } else if (func_8013BBA0()) {
        if (cursor < 4) {
            cursor = 13;
        } else if (cursor >= 14) {
            cursor = 4;
        }
    } else if (cursor == 3) {
        cursor = 6 - D_8013F7C4;
    }
    func_80139D64(cursor);
    if (D_8013F7C4 == 13) {
        help = 8;
    } else {
        help = 2;
        if (D_8013F7C4 >= 4) {
            help = 4;
        }
    }
    func_801389C8(help);
    item = &D_8013FBD8[D_8013F7C4];
    switch (D_8013F7C4) {
        case 0:
            func_80139150(item, EGSDeviceControllerVibrate(5, 0, 0) == 7, 1);
            break;
        case 1:
            func_80139150(item, JoystickGetFlightControls(), 1);
            break;
        case 13:
            TextSetStringIntensity(item->textIds[0], 0xF0, 0x78, 0x78);
            break;
        case 2:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            TextSetStringIntensity(item->textIds[1], 0xB4, 0xB4, 0xF0);
            break;
    }
}

u32 func_8013BB3C(void) {
    u32 n;
    s32 i;

    for (n = 0; n < 2; n++) {
        for (i = 0; i < 9; i++) {
            if (D_8013FDEC[i] != D_8013FDF8[n][i]) {
                break;
            }
        }
        if (i == 9) {
            break;
        }
    }
    return n;
}

s32 func_8013BBA0(void) {
    s32 i;

    for (i = 0; i < 9; i++) {
        if (D_8013FDEC[i] != gJoyMap[i]) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013BBE4);

INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013C530);

void func_8013CD40(u32 tag) {
    char name[24];
    EGSTexture tex;
    s32 weapon;
    s32 i;
    s32 busy;
    TextChar *icon;
    char *p;
    char **strings;

    if (tag < 26) {
        weapon = D_8013F7D0[tag].textIds - D_8013FE20;
        icon = &mMap[tag];
        tex.tim = D_8013FE78;
        EGSTextureCreate(&tex);
        EGSTextureCreateSprite(&tex, icon, POSITION.x + 99, POSITION.y - 45, 0, 0);
        if (weapon == 1) {
            func_80139590(&tex);
            D_8013F7CC = 7;
            DrawSync(0);
        } else {
            tex.image.cx = tex.image.cy = tex.image.cw = tex.image.ch = 0;
        }
        EGSTextureLoad(&tex);
        icon->unk24 = 3;
        DrawSync(0);
        do {
            if (D_8013F7CC == 7) {
                weapon = WeaponGetNextID(weapon);
                if (weapon != 1) {
                    strcpy(name, gWeaponInfo[weapon].name);
                    i = 0;
                    while (name[i] != '.') {
                        ASSERT_MENU(name[i] != '\0', 3570);
                        i++;
                    }
                    i++;
                    *(MenuExt *)&name[i] = *(MenuExt *)"TIM";
                    busy = StreamHogBlock(name, D_8013FE78, 0xF000, tag + 1, func_8013CD40);
                } else {
                    busy = StreamHogBlock("WEAPDESC.TXT", MENU_STRINGS + 26, 0xF000, -1, func_8013CD40);
                }
            } else {
                busy = 0;
            }
        } while (busy != 0 && weapon != 1);
    } else {
        p = (char *)(MENU_STRINGS + 26);
        strings = MENU_STRINGS;
        for (i = 0; i < 26; i++) {
            while (!(*p >= '0' && *p <= '9') && !(*p >= 'A' && *p <= 'Z') && *p != '.') {
                p++;
            }
            *strings++ = p;
            while (*p != '*') {
                p++;
            }
            *p = '\0';
        }
    }
}

void func_8013D04C(void) {
    MenuItem *item = D_8013F7D0;
    s32 weapon = 1;
    s32 i;

    do {
        item->count = 1;
        item->strings = &gUseMsgs[gWeaponInfo[weapon].nameMsg];
        item->textIds = &D_8013FE20[weapon];
        item++;
    } while ((weapon = WeaponGetNextID(weapon)) != 1);
    D_8013FCC0[7] = item - D_8013F7D0;
    if (D_8013F7CC != 7) {
        for (i = 0; i < 16; i++) {
            mMap[i].h = 0;
            mMap[i].w = 0;
        }
        D_8013F7CC = 7;
        StreamHogBlock("GLOKSIL.TIM", D_8013FE78, 0xF000, 0, func_8013CD40);
    }
}

#ifdef NON_MATCHING
/* player arrow: angle +0x100/+0x1000 get combined, and the marker quad load is ordered differently */
void func_8013D19C(void) {
    char name[24];
    EGSTexture tex;
    ScreenPos pt[2];
    VECTOR32 pos;
    s32 arrow[6];
    MenuItem *item = D_8013F7D0;
    char **row = D_8013FB98[0];
    s32 maps = 0;
    s32 i;
    s32 j;
    s32 k;
    s32 angle;
    MenuQuad *quad;
    EGSFrame *frame;
    s32 cursor;

    row[1] = D_8013F740;
    item->count = 2;
    item->strings = row;
    row += 2;
    item->textIds = D_8013FE20;
    item++;
    ASSERT_MENU(gObjectives != NULL, 3666);
    ASSERT_MENU(gObjectives->primaryCount < 9, 3667);
    if (D_8013F7CC != 8) {
        for (i = 0; i < 16; i++) {
            mMap[i].h = 0;
            mMap[i].w = 0;
        }
        EGSDisplayBuildPoly4F(&D_8013FEBC[0], 0x5CFFFF, 0, 0, 0, 0);
        EGSDisplayBuildPoly4F(&D_8013FEBC[1], 0, 0, 0, 0, 0);
        D_8013FEBC[0].z = 1;
        D_8013FEBC[1].z = 2;
        for (i = 0; i < 8; i++) {
            EGSDisplayBuildPoly4F(&D_8013FEC0[i].q[0], 0, 0, 0, 0, 0);
            EGSDisplayBuildPoly4F(&D_8013FEC0[i].q[1], 0, 0, 0, 0, 0);
            D_8013FEC0[i].q[1].z = 2;
        }
        tex.tim = D_8013FE78;
        FlushAllStreamRequests();
        for (i = 0; i < 16; i++) {
            sprintf(name, "MAP%d.TIM", i + 1);
            D_8013F7CC = 8;
            if (StreamHogBlock(name, D_8013FE78, 0xF000, 0, NULL) != 0) {
                maps = i;
                break;
            }
            StreamSynchronous();
            EGSTextureCreate(&tex);
            EGSTextureCreateSprite(&tex, &mMap[i], POSITION.x - 0x39, POSITION.y - 5, 0, 0);
            if (i == 0) {
                func_80139590(&tex);
            } else {
                tex.image.cx = tex.image.cy = tex.image.cw = tex.image.ch = 0;
            }
            DrawSync(0);
            EGSTextureLoad(&tex);
            mMap[i].unk24 = 3;
        }
        D_8013F7CC = 8;
        if (maps > 0 && D_8013FB4C[gSysInfo.level] != NULL) {
            for (i = 0; i < maps; i++) {
                pos = gObjPlayer->move->pos;
                quad = D_8013FEBC;
                if (D_8013FB4C[gSysInfo.level](i, &pos, &pt[0])) {
                    mPlayerMapLocation.map = i;
                    frame = gObjPlayer->anim->frame;
                    pos.vx += frame->m[0][2];
                    pos.vz += frame->m[2][2];
                    D_8013FB4C[gSysInfo.level](i, &pos, &pt[1]);
                    pos.vy = pt[1].y - pt[0].y;
                    pos.vx = pt[1].x - pt[0].x;
                    angle = ratan2(pos.vy, pos.vx) + 0x100;
                    if (angle < 0) {
                        angle += 0x1000;
                    }
                    angle = FIXED_INT(angle * 8);
                    for (k = 0; k < 6; k++) {
                        arrow[k] = D_8013FAD0[angle][k];
                    }
                    for (k = 0; k < 2; k++, quad++) {
                        *(s32 *)&quad->x0 = MAP_XY(pt[0].x + arrow[2] - 0x39, pt[0].y + arrow[3] - 5);
                        *(s32 *)&quad->x1 = MAP_XY(pt[0].x + arrow[0] - 0x39, pt[0].y + arrow[1] - 5);
                        *(s32 *)&quad->x2 = MAP_XY(pt[0].x - 0x39, pt[0].y - 5);
                        *(s32 *)&quad->x3 = MAP_XY(pt[0].x + arrow[4] - 0x39, pt[0].y + arrow[5] - 5);
                        for (j = 0; j < 6; j++) {
                            if (arrow[j] > 0) {
                                arrow[j]++;
                            } else if (arrow[j] < 0) {
                                arrow[j]--;
                            }
                        }
                    }
                    break;
                }
            }
            if (i == maps) {
                mPlayerMapLocation.map = 0xFF;
            }
        } else {
            mPlayerMapLocation.map = 0xFF;
        }
    } else {
        setColorCode(D_8013FEBC, 0x285CFFFF);
    }
    if (mObjectiveMapLocations[gSysInfo.level] != NULL) {
        for (i = 0; i < gObjectives->primaryCount; i++) {
            if (gObjectives != NULL && (gObjectives->unk18 & (1 << i)) && !(gObjectives->completed & (1 << i)) &&
                !(gObjectives->unk14 & (1 << i))) {
                row[1] = gObjectives->primaryTexts[i];
                item->count = 2;
                item->strings = row;
                row += 2;
                item->textIds = &D_8013FE20[(i + 1) * 2];
                item++;
            }
        }
    }
    if (mMap[0].w != 0) {
        D_8013FCC0[8] = item - D_8013F7D0;
    } else {
        D_8013FCC0[8] = 0;
    }
    cursor = D_8013F7C4;
    func_8013A744(0);
    func_8013A744(0x7F);
    D_8013F7C4 = cursor;
    D_80138044[D_8013FCB4](cursor);
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013D19C);
#endif

void func_8013D95C(void) {
    MenuItem *item = D_8013F7D0;
    u16 *ids = D_8013FE20;
    s32 i;
    s32 done;

    item->count = 1;
    item->textIds = ids++;
    item->strings = &D_8013F78C;
    item++;
    for (i = 0; i < gObjectives->primaryCount; i++) {
        if (gObjectives != NULL && (gObjectives->unk18 & (1 << i)) && !(gObjectives->completed & (1 << i)) &&
            !(gObjectives->unk14 & (1 << i))) {
            D_8013FB98[i][1] = gObjectives->primaryTexts[i];
            item->count = 2;
            item->strings = D_8013FB98[i];
            item->textIds = ids;
            ids += 2;
            item++;
        }
    }
    done = 0;
    for (i = 0; i < gObjectives->primaryCount; i++) {
        if (gObjectives != NULL && (gObjectives->unk18 & (1 << i)) && (gObjectives->completed & (1 << i)) &&
            !(gObjectives->unk14 & (1 << i))) {
            if (done == 0) {
                item->count = 1;
                item->textIds = ids++;
                item->strings = &D_8013F790;
                item++;
            }
            done++;
            D_8013FB98[i][1] = gObjectives->primaryTexts[i];
            item->count = 2;
            item->strings = D_8013FB98[i];
            item->textIds = ids;
            ids += 2;
            item++;
        }
    }
    done = 0;
    for (i = 0; i < gObjectives->primaryCount; i++) {
        if (gObjectives != NULL && (gObjectives->unk18 & (1 << i)) && (gObjectives->unk14 & (1 << i))) {
            if (done == 0) {
                item->count = 1;
                item->textIds = ids++;
                item->strings = &D_8013F794;
                item++;
            }
            done++;
            D_8013FB98[i][1] = gObjectives->primaryTexts[i];
            item->count = 2;
            item->strings = D_8013FB98[i];
            item->textIds = ids;
            ids += 2;
            item++;
        }
    }
    D_8013FCC0[10] = item - D_8013F7D0;
}

void func_8013DC98(void) {
    MenuItem *item = D_8013F7D0;
    u16 *ids = D_8013FE20;
    s32 i;
    s32 done;

    item->count = 1;
    item->textIds = ids++;
    item->strings = &D_8013F798;
    item++;
    for (i = 0; i < gObjectives->count; i++) {
        if (gObjectives != NULL && (gObjectives->unk24 & (1 << i)) && !(gObjectives->unk20 & (1 << i))) {
            D_8013FB98[i][1] = gObjectives->texts[i];
            item->count = 2;
            item->strings = D_8013FB98[i];
            item->textIds = ids;
            ids += 2;
            item++;
        }
    }
    done = 0;
    for (i = 0; i < gObjectives->count; i++) {
        if (gObjectives != NULL && (gObjectives->unk24 & (1 << i)) && (gObjectives->unk20 & (1 << i))) {
            if (done == 0) {
                item->count = 1;
                item->textIds = ids++;
                item->strings = &D_8013F79C;
                item++;
            }
            done++;
            D_8013FB98[i][1] = gObjectives->texts[i];
            item->count = 2;
            item->strings = D_8013FB98[i];
            item->textIds = ids;
            ids += 2;
            item++;
        }
    }
    if (item == &D_8013F7D0[1]) {
        item->count = 1;
        item++;
        D_8013F7D0[1].strings = &D_8013F774;
        D_8013F7D0[1].textIds = ids;
    }
    D_8013FCC0[11] = item - D_8013F7D0;
}

#ifdef NON_MATCHING
/* loop setup: the constant 1 is materialized after the ids pointer (3 insns) */
void func_8013DEEC(void) {
    MenuItem *items = D_8013F7D0;
    u16 *ids;
    char **names;
    s32 i;

    if (D_8013FE6C->unk28 == 0) {
        i = 0;
        ids = D_8013FE20;
        names = gBriefings->names;
        D_8013FE90 = 0;
        D_8013FE8C = 0;
        D_8013FEA8 = 0;
        D_8013FEA4 = 0;
        for (; i < 3; i++) {
            items[i].textIds = ids++;
            items[i].strings = names++;
            items[i].count = 1;
        }
        items[3].count = 1;
        items[3].strings = &gBriefings->texts[D_8013F7C4];
        items[3].textIds = D_8013FE26;
        D_8013FCC0[12] = 4;
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013DEEC);
#endif

void func_8013DFB0(void) {
    s32 i;
    u16 id;
    RECT *rect;

    InterfaceSetMode(1);
    func_801389C8(10);
    for (i = 1; i < 15; i++) {
        EGSDisplayDeletePoly(gInterfaceWindow, &D_8013FEC4[i]);
    }
    for (i = 0; i < 5; i++) {
        TextSetStringIntensity(D_8013FEC8[i], 0, 0, 0);
    }
    id = TextAddPersistentString(D_8013F7A8, D_8013FE6C, 0, 0);
    TextSetStringJustification(id, 0);
    TextSetStringIntensity(id, 0xA0, 0x60, 0x70);
    rect = TextStringRect(id, 0);
    *(s32 *)&D_8013FEC4->x0 = MAP_XY(rect->x - 5, rect->y - 4);
    *(s32 *)&D_8013FEC4->x1 = MAP_XY(rect->x + rect->w + 5, rect->y - 4);
    *(s32 *)&D_8013FEC4->x2 = MAP_XY(rect->x - 5, rect->y + rect->h + 4);
    *(s32 *)&D_8013FEC4->x3 = MAP_XY(rect->x + rect->w + 5, rect->y + rect->h + 4);
    D_8013FE66 = id;
    ((ScreenPos *)&D_8013F7C4)->x = POSITION.x;
    ((ScreenPos *)&D_8013F7C4)->y = POSITION.y;
    D_8012D846 |= 1;
}

void func_8013E184(void) {
    MenuItem *item = D_8013F908;
    s32 flags = func_8013B14C();
    u32 i;

    for (i = 0; i < 12; i++, item++) {
        switch (item->count) {
            case 1:
                break;
            case 3:
                if ((flags & (1 << i)) == (1 << i)) {
                    TextSetStringIntensity(item->textIds[1], 0x5A, 0xA0, 0x5A);
                    TextSetStringIntensity(item->textIds[2], 0x64, 0x3C, 0x3C);
                } else {
                    TextSetStringIntensity(item->textIds[1], 0x3C, 0x64, 0x3C);
                    TextSetStringIntensity(item->textIds[2], 0xA0, 0x5A, 0x5A);
                }
                break;
        }
    }
}

void func_8013E278(void) {
    s32 i;

    func_8013B5E4(0);
    for (i = 0; i < 3; i++) {
        func_80139230(&MENU_BOXES[i], i * 16 - 0x47, 0x80, SysSoundGetMasterVolume(i));
    }
    func_80139150(&D_8013FAA0, gInterfaceOn, 0);
}

void func_8013E30C(void) {
    D_8013F7C4 = gSysInfo.level;
}

#ifdef NON_MATCHING
/* id and the row offset swap s0/s1 in the button-name loop */
void func_8013E324(void) {
    s32 i;
    s32 action;
    MenuItem *item;
    u16 id;

    for (i = 0; i < 9; i++) {
        D_8013FDEC[i] = gJoyMap[i];
    }
    D_8013FBD8[2].textIds[1] = TextReplaceString(D_8013FBD8[2].textIds[1], D_8013F7AC[func_8013BB3C()]);
    item = D_8013FBD8;
    func_80139150(item, EGSDeviceControllerVibrate(5, 0, 0) == 7, 0);
    item++;
    func_80139150(item, JoystickGetFlightControls(), 0);
    for (i = 0; i < 9; i++) {
        action = D_8013FDEC[i];
        if (action >= 0) {
            id = TextReplaceString(D_8013FBD8[action + 4].textIds[1], gButtonStrings[i]);
            TextSetStringJustification(id, 0);
            D_8013FBD8[action + 4].textIds[1] = id;
        }
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013E324);
#endif

void func_8013E480(void) {
    MenuLine *line = D_8013FE78;
    MenuQuad *quad;
    s32 level;

    func_80139D64(0);
    func_801389C8(5);
    level = gAmbientLight & 0xFF;
    D_8013F7CC = -1;
    gAmbientLight = 0;
    D_8013F7C4 = level;
    EGSDisplayBuildLineF(line, 0xFF5C00, 0, 0);
    quad = (MenuQuad *)(line + 1);
    EGSDisplayBuildPoly4G(quad, 0x5A1E1E, MAP_XY(-0x89, -0x5F), MAP_XY(0x17, -0x5F), MAP_XY(-0x89, 0x37),
                          MAP_XY(0x17, 0x37), 0x5A1E1E, 0, 0);
    line->z = 1;
    quad->z = 2;
    EGSDisplayAddPoly(gInterfaceWindow, line);
    EGSDisplayAddPoly(gInterfaceWindow, quad);
    setSemiTrans(D_8013FEC4, 0);
    TextSetStringIntensity(*D_8013F904, 0xB4, 0xB4, 0xF0);
}

#ifdef NON_MATCHING
/* delta loads are scheduled differently; &D_8013FEA4 is not CSE'd into a register */
void func_8013E5B0(void) {
    s32 dx0, dy0, dx1, dy1;
    s32 x, y;
    s32 x1, y1;
    MenuQuad *quad;

    if (D_8013FEA4 == 0 && D_8013FEA8 == 0) {
        XY(D_8013FEA4).x = dx0 = XY(D_8013FE8C).x - XY(D_8013FE94).x;
        XY(D_8013FEA4).y = dy0 = XY(D_8013FE8C).y - XY(D_8013FE94).y;
        XY(D_8013FEA8).x = dx1 = XY(D_8013FE90).x - XY(D_8013FE98).x;
        XY(D_8013FEA8).y = dy1 = XY(D_8013FE90).y - XY(D_8013FE98).y;
        XY(D_8013FE9C).x = dx0 / 4;
        XY(D_8013FE9C).y = dy0 / 4;
        XY(D_8013FEA0).x = dx1 / 4;
        XY(D_8013FEA0).y = dy1 / 4;
    }
    HILITE_STEP(XY(D_8013FE94).x, XY(D_8013FE8C).x, XY(D_8013FE9C).x);
    HILITE_STEP(XY(D_8013FE94).y, XY(D_8013FE8C).y, XY(D_8013FE9C).y);
    HILITE_STEP(XY(D_8013FE98).x, XY(D_8013FE90).x, XY(D_8013FEA0).x);
    HILITE_STEP(XY(D_8013FE98).y, XY(D_8013FE90).y, XY(D_8013FEA0).y);
    x = XY(D_8013FE94).x >> 6;
    y = XY(D_8013FE94).y >> 6;
    x1 = x + (XY(D_8013FE98).x >> 6);
    y1 = y + (XY(D_8013FE98).y >> 6);
    quad = &gMenuPolys[30];
    *(s32 *)&quad->x0 = MAP_XY(x, y);
    *(s32 *)&quad->x1 = MAP_XY(x1, y);
    *(s32 *)&quad->x2 = MAP_XY(x, y1);
    *(s32 *)&quad->x3 = MAP_XY(x1, y1);
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013E5B0);
#endif

#ifdef NON_MATCHING
/* layout pointers: stores are batched differently and the frame-line loop allocates differently */
void func_8013E98C(void) {
    char path[32];
    u8 *mem;
    TextChar *chars;
    MenuLine *lines;
    MenuItem *item;
    u32 i;
    s32 j;
    s32 top;
    s32 bottom;

    mem = EGSDisplayPacketAreaMalloc(0x15058);
    chars = (TextChar *)(mem + 0x3390);
    D_8013FE70 = (TextChar *)(mem + 0x39EC);
    D_8013FE6C = (TextChar *)mem;
    mMap = (TextChar *)(mem + 0x3E38);
    mem += 0x40F8;
    D_8013FE78 = mem;
    mem += 0xF000;
    D_8013FEC4 = (MenuQuad *)(mem + 0x200);
    D_8013FEBC = (MenuQuad *)(mem + 0x41C);
    gMenuPolys = (MenuQuad *)(mem + 0x464);
    D_8013FEC0 = (MenuQuadPair *)(mem + 0x8C0);
    D_8013FE68 = chars;
    D_8013FE84 = (u32 *)mem;
    gMenuLines = (MenuLine *)(mem + 0xB00);
    lines = (MenuLine *)(mem + 0x1580);
    D_8013FE88 = LevelGetMenuHogHeader();
    InterfaceBuildMenuBackgroundPolys(D_8013FEC4);
    TextSpriteInitialize(D_8013FE68, 37);
    TextSpriteInitialize(D_8013FE6C, 300);
    TextSpriteInitialize(D_8013FE70, 25);
    TextActivateAllWindows(0);
    D_8013F7C0 = TextAddWindow(-0x8C, -0x55, 0xA5, 0x96);
    D_8013F7C1 = TextAddWindow(0x32, 0x16, 0x6E, 0x64);
    D_8013F7C3 = TextAddWindow(0x2C, -0x50, 0x6D, 0x46);
    D_8013F7C2 = TextAddWindow(-0x8C, 0x50, 0xA5, 0xA);
    TextSetWindowColor(D_8013F7C0, 0x50, 0x82, 0xF0);
    TextSetWindowColor(D_8013F7C3, 0x50, 0x82, 0xF0);
    TextSetWindowColor(D_8013F7C1, 0x50, 0x82, 0xF0);
    TextSetWindowColor(D_8013F7C2, 0xA0, 0x60, 0x70);
    TextSetWindowJustification(D_8013F7C1, 0);
    TextSetWindowMini(D_8013F7C3, 1);
    item = D_8013FCBC = D_8013F86C;
    for (i = 0, top = 0x14, bottom = 0x1F; i < 5; i++, top += 16, bottom += 16, item++) {
        *item->textIds = TextAddString(D_8013F7C1, *item->strings, -1, chars);
        if (*item->textIds == 0xFFFF) {
            func_8013EE78();
            break;
        }
        TextSetStringSpeed(*item->textIds, 0x800);
        chars += TextStringLength(*item->textIds, 1);
        for (j = 0; j < 4; j++) {
            EGSDisplayBuildLineF(&lines[i * 4 + j], 0xF08250,
                                 (j & 2) ? 0xA0 | (bottom << 16) : 0x32 | (top << 16),
                                 (j & 1) ? 0xA0 | (top << 16) : 0x32 | (bottom << 16));
        }
    }
    EGSDisplayBuildPoly4F(&gMenuPolys[30], 0, 0, 0, 0, 0);
    gMenuPolys[30].z = 5;
    sprintf(path, "\\%s\\MENU.HOG;1", gLevelNames[gSysInfo.level]);
    OpenHogForStreaming(path, D_8013FE88);
    func_80139FFC(0);
    XY(D_8013FE94).x = 0x32;
    XY(D_8013FE94).y = 0x17;
    D_8013FEA0 = 0;
    D_8013FE9C = 0;
    D_8013FEA8 = 0;
    D_8013FEA4 = 0;
    XY(D_8013FE98).x = 0;
    XY(D_8013FE98).y = 0xB;
    EGSDisplayFadeOff();
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013E98C);
#endif

void func_8013EDC8(void) {
    D_8012D846 &= ~1;
    EGSDeviceControllerVibrate(2, 0, 0);
    SysStopXA();
    SysPauseSound(1);
    D_8013F7C0 = 0xFF;
    D_8013F7C1 = 0xFF;
    D_8013F7C2 = 0xFF;
    D_8013F7C3 = 0xFF;
    D_8013FDE8 = 10;
    D_8013FE66 = 0xFFFF;
    D_8013FCB4 = 0;
    D_8013F7C4 = 0;
    D_8013FCB8 = 0;
    D_8013FCA4 = gSystemTick;
}

void func_8013EE78(void) {
    void (*leave)(void);
    MenuQuad *quad;
    MenuLine *line;

    if (D_8013FE66 != 0xFFFF) {
        TextRemoveString(TextReplaceString(D_8013FE66, D_801377E4));
        D_8013FE66 = 0xFFFF;
    }
    TextRemoveWindowNow(D_8013F7C0);
    TextRemoveWindowNow(D_8013F7C1);
    leave = D_8013FDB0[D_8013FCB4];
    if (leave != NULL || (D_8013FCB4 == 0 && (leave = D_8013FDB0[D_8013FCAC[D_8013F7C4]]) != NULL)) {
        leave();
    }
    for (quad = gMenuPolys; quad < gMenuPolys + 31; quad++) {
        EGSDisplayDeletePoly(gInterfaceWindow, quad);
    }
    for (quad = D_8013FEC4; quad < D_8013FEC4 + 15; quad++) {
        EGSDisplayDeletePoly(gInterfaceWindow, quad);
    }
    for (line = gMenuLines; line < gMenuLines + 132; line++) {
        EGSDisplayDeletePoly(gInterfaceWindow, line);
    }
    TextRemoveWindowNow(D_8013F7C2);
    TextRemoveWindowNow(D_8013F7C3);
    func_8013AE8C();
    D_8013FCA4 = -1;
    D_8013FCA8 = -1;
    D_8013FE6C = NULL;
    gMenuPolys = NULL;
    gMenuLines = NULL;
    TextActivateAllWindows(1);
    InterfaceSetMode(1);
    EGSDisplayPacketAreaFree();
    InterfaceWeaponChange();
    SysStatePop();
    LoadImage(&D_8013FE7C, D_8013FE84);
    StreamVramPage(14, 0);
    StreamVramPage(15, 0);
    StreamVramPage(30, 0);
    D_8013F7CC = -1;
    EGSDisplayQueueInterrupt(func_8013B5E4, 0, 0xE0C00000);
    EGSFileClose(&D_8013FE74);
    D_8012D846 |= 1;
    EGSDisplayFadeFull();
    EGSDisplayFade(-1, 30, NULL);
}

#ifdef NON_MATCHING
/* fill loop: 0xFFFF and the counter are loaded in swapped order (2 insns) */
void func_8013F144(void) {
    TextChar *chars = D_8013FE6C;
    MenuItem *item;
    s32 menu;
    s32 i;
    s32 window;

    if (D_8013F7C0 != 0xFF) {
        for (i = 34; i >= 0; i--) {
            D_8013FE20[i] = 0xFFFF;
        }
        if (D_8013FCB4 == 0) {
            menu = D_8013FCAC[D_8013F7C4];
        } else {
            menu = D_8013FCB4;
        }
        if (D_8013FD40[menu] != NULL) {
            D_8013FD40[menu]();
        }
        item = &D_8013FCD0[menu][D_8013F7C8];
        if (menu == 8 || menu == 9) {
            window = D_8013F7C3;
            TextSetWindowMini(window, 1);
        } else {
            window = D_8013F7C0;
        }
        if (D_8013FCB4 == 12 && chars->unk28 != 0) {
            do {
                chars++;
            } while (chars->unk28 != 0);
        }
        if (item != NULL) {
            for (i = D_8013F7C8; i < D_8013FCC0[menu]; i++, item++) {
                if (item->count != 0) {
                    if (item->count == 1) {
                        *item->textIds = TextAddString(window, *item->strings, -1, chars);
                    } else {
                        TextAddStringMulti(window, item->count, item->strings, -1, chars, item->textIds);
                    }
                    if (*item->textIds == 0xFFFF) {
                        if (D_8013FCB4 == 8 || (D_8013FCB4 == 0 && D_8013FCAC[D_8013F7C4] == 8)) {
                            i = D_8013FCC0[menu];
                            continue;
                        }
                        func_8013EE78();
                        return;
                    }
                    chars += TextStringLength(*item->textIds, 1);
                    ASSERT_MENU(chars < D_8013FE6C + 300, 4686);
                }
            }
        }
        if (D_8013FD78[menu] != NULL) {
            D_8013FD78[menu]();
        }
        if (D_8013FCB4 != 0) {
            D_80138044[D_8013FCB4](D_8013F7C4);
        }
    }
}
#else
INCLUDE_ASM("asm/menu/nonmatchings/menu", func_8013F144);
#endif

void MenuExecute(void) {
    if (gSystemTick < D_8013FCA4) {
        mIncomingInterfaceMode = 0;
        func_8013EDC8();
        if (gPacketAreaMallocSize != 0) {
            InterfaceSetMode(0);
        }
    }
    if (gPacketAreaMallocSize == 0) {
        InterfaceSetMode(4);
        func_8013E98C();
        D_8013FCA8 = gSystemTick;
    }
    if (gSystemTick == D_8013FCA8) {
        func_8013F144();
    }
    if (gSystemTick >= D_8013FCA4) {
        if (D_8013FCB4 == 8) {
            func_801387B0();
        }
        if (D_8013FE94 != D_8013FE8C || D_8013FE98 != D_8013FE90) {
            func_8013E5B0();
        }
        func_8013C530();
        TextUpdate();
        InterfaceAnimate();
    }
}
