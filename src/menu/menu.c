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

/* "bin/menu.c" (the asserts here don't use the usual "s/..." path), defined below */
#define ASSERT_MENU(cond, line) if (!(cond)) EGSDebugAssert(1, 0, (char *)D_8013807C, line)

/* The 0xF000-byte work buffer is reused: slider boxes, or the weapon description strings */
#define MENU_BOXES ((MenuBox *)D_8013FE78)
#define MENU_STRINGS ((char **)D_8013FE78)

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

/* this file, for the tables below */
s32 MapLocateBaseExt(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateChurch(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateChurch2(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateInWhouse(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateMuseum(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateMuseum2(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocatePark(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateSilo(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateSubway(s32 map, VECTOR32 *pos, u32 *out);
s32 MapLocateWhouse(s32 map, VECTOR32 *pos, u32 *out);
void funcEvSpIOEx(s32 delta);
void func_801396B4(void);
void func_80139710(void);
void func_80139738(void);
void func_801398A4(void);
void func_80139994(void);
void func_801399C4(void);
void func_80139B54(void);
void func_80139C54(void);
void func_80139CE4(void);
void func_80139D24(void);
void func_80139FFC(s32 cursor);
void func_8013A220(s32 cursor);
void func_8013A60C(s32 cursor);
void func_8013AC64(u32 value);
void func_8013ACA4(void);
void func_8013ACAC(s32 delta);
void func_8013AE30(void);
void func_8013AFC8(void);
void func_8013B04C(void);
void func_8013B464(s32 delta);
void func_8013B8F8(s32 cursor);
void func_8013D04C(void);
void func_8013D19C(void);
void func_8013D95C(void);
void func_8013DC98(void);
void func_8013DEEC(void);
void func_8013DFB0(void);
void func_8013E184(void);
void func_8013E278(void);
void func_8013E30C(void);
void func_8013E324(void);
void func_8013E480(void);

static const s32 D_801377E4[5] = {0, 1, 1, 1, 0}; /* its first word doubles as "" */

/*
 * .bss.  The original assembler word-aligned every .lcomm (docs/data_migration.md);
 * aspsx 2.77 and maspsx align an n-byte .lcomm to n rounded up to a power of two, so
 * the items > 4 bytes that sit at 4-aligned offsets are split into 4-byte pieces.
 */
static u16 D_8013FE20[36]; /* text ids of the menu strings */
#define D_8013FE26 (&D_8013FE20[3])
#define D_8013FE66 D_8013FE20[35] /* status line text id */
static TextChar *D_8013FE68; /* menu characters */
static TextChar *D_8013FE6C; /* briefing text characters */
static TextChar *D_8013FE70; /* help line characters */
static s32 D_8013FE74; /* EGS file handle */
static void *D_8013FE78; /* work buffer */
static s32 D_8013FE7C_xy, D_8013FE7C_wh; /* saved CLUT area */
#define D_8013FE7C (*(RECT *)&D_8013FE7C_xy)
static u32 *D_8013FE84; /* CLUT backup */
static s32 D_8013FE88; /* menu hog header */
static s32 D_8013FE8C;
static s32 D_8013FE90;
static s32 D_8013FE94;
static s32 D_8013FE98;
static s32 D_8013FE9C; /* highlight step */
static s32 D_8013FEA0;
static s32 D_8013FEA4;
static s32 D_8013FEA8;
static s32 D_8013FEAC, D_8013FEB0, D_8013FEB4, D_8013FEB8; /* unused */
static MenuQuad *D_8013FEBC;
static MenuQuadPair *D_8013FEC0;
static MenuQuad *D_8013FEC4;
static u16 D_8013FEC8[2], D_8013FECC[2], D_8013FED0[2]; /* main menu text ids (D_8013FEC8[0..4]) */
static MenuStackEntry D_8013FED4[2], D_8013FED8[2], D_8013FEDC[2], D_8013FEE0[2], D_8013FEE4[2]; /* menu stack (D_8013FED4[0..9]) */
/* commons, laid out by the linker after the .bss */
u8 mSfxSampleCanBePlayedAgain;
s32 mIncomingInterfaceMode;
MapLocation mPlayerMapLocation;
TextChar *mMap; /* weapon icons */

/* .data (and the level map tables in .rdata) */
static char *D_8013F5C8[5] = { /* main menu */
    "Map\n",
    "Objectives\n",
    "Parameters\n",
    "Weapons\n",
    "Options\n",
};
static char *D_8013F5DC[7] = { /* options menu */
    "Review Briefing",
    "Restart Mission",
    "Restart At Last Checkpoint",
    "Select Previous Mission",
    "Game Settings",
    "Quit Game",
    "Cheats",
};
static char *D_8013F5F8[23] = { /* debug menu */
    "Select Level",
    "Invincibility",
    " on",
    " off",
    "Toggle CObj drawing",
    "Resurrect Gabriel",
    "Debug Display",
    " on",
    " off",
    "Lots o' Bullets",
    "Poly Debug",
    " on",
    " off",
    "Invisible",
    " on",
    " off",
    "To first zoom point",
    "End Level",
    "Save Game",
    "NPCs on Radar",
    " on",
    " off",
    "Complete Objectives",
};
static char *D_8013F654[19] = { /* level names */
    "Subway District",
    "Destroyed Subway",
    "Tunnel Chase",
    "Washington Park",
    "Freedom Memorial",
    "Expo Center Hunt",
    "Expo Center Search",
    "Terrorist Base",
    "Base Bunker",
    "Base Radar Tower",
    "Base Escape",
    "Secret Prison Top",
    "Secret Prison Bottom",
    "Catacombs",
    "Warehouse District",
    "Warehouse 76",
    "Mining Tunnels",
    "Silo Access Tunnels",
    "Missile Silo",
};
static char *D_8013F6A0[9] = { /* sound menu */
    "Sound FX\n",
    "Music\n",
    "Voice-over\n",
    "Controller",
    "Gamma Level",
    "\nInterface\t",
    "on ",
    "off",
    "Screen Centering",
};
static char *D_8013F6C4[30] = { /* controller menu */
    "   Aim\t\t\t\t  ",
    "?",
    "   Use\t\t\t\t  ",
    "?",
    "   Target Lock\t\t  ",
    "?",
    "   Strafe Left\t\t  ",
    "?",
    "   Strafe Right\t\t  ",
    "?",
    "   Roll\t\t\t\t  ",
    "?",
    "   Kneel\t\t\t\t  ",
    "?",
    "   Shoot\t\t\t\t  ",
    "?",
    "   Change Weapon\t  ",
    "?",
    "   Cancel   ",
    "Accept   ",
    "Reset",
    "Vibration\t",
    "on ",
    "off",
    "Invert Aim\t",
    "yes ",
    "no",
    "\nController Configuration:",
    "Preset config\t",
    "zzzzzzzzz",
};
static char *D_8013F73C = "Adjust game lighting levels by moving bar up or down"; /* brightness help */
static char *D_8013F740 = "Current Location"; /* "you" row text */
static char *D_8013F744[2] = {
    "\nNo\t",
    "Yes",
};
static char *D_8013F74C[10] = { /* help line per mode */
    "%x select\t%t exit menu",
    "%x select\t%t back",
    "%t back",
    "%x next\t%t back",
    "Press new button for action",
    "%x accept\t%t cancel",
    "Press %x to continue",
    "%x equip\t%t back",
    "%x select",
    "%t exit menu",
};
static char *D_8013F774 = "  none"; /* "no objectives" */
static char *D_8013F778 = "Cannot change weapons while hanging";
static char *D_8013F77C = "Controller missing.\n\nPlease reinsert controller into controller port 1 and press the START button to continue"; /* "no controller" */
static char *D_8013F780 = "Do you really want to abort this game?"; /* quit prompt */
static char *D_8013F784 = "Do you really want to restart this mission?"; /* restart-from-start prompt */
static char *D_8013F788 = "Do you really want to restart at the last checkpoint?"; /* restart prompt */
static char *D_8013F78C = "Mission Objectives:";
static char *D_8013F790 = "\nCompleted Objectives:";
static char *D_8013F794 = "\nFailed Objectives:";
static char *D_8013F798 = "Mission Parameters:"; /* objectives heading */
static char *D_8013F79C = "\nFailed Parameters:"; /* completed heading */
static char *D_8013F7A0 = "No Reconnaissance\nData Available"; /* map: "you are here" */
static char *D_8013F7A4 = "Location Unknown"; /* map: objective location */
static char *D_8013F7A8 = "Use D-Pad to center the image.\n\nHit %x to save  %t to Cancel";
static char *D_8013F7AC[3] = { /* controller configuration names */
    "Standard",
    "Alternate",
    "Custom",
};
static char *D_8013F7B8[2] = {
    "Ammo: ",
    "../...",
};
static u8 D_8013F7C0 = 255; /* menu text window */
static u8 D_8013F7C1 = 255;
static u8 D_8013F7C2 = 255; /* help line window */
static u8 D_8013F7C3 = 255; /* weapon rounds window */
static s32 D_8013F7C4 = 0; /* cursor / current value */
static s32 D_8013F7C8 = 0; /* first item whose text still has to be built */
static s32 D_8013F7CC = -1;
static MenuItem D_8013F7D0[13] = { /* briefing menu */
    {1, 0, D_8013FE20},
    {1, 0, &D_8013FE20[3]},
    {1, 0, &D_8013FE20[6]},
    {1, 0, &D_8013FE20[9]},
    {1, 0, &D_8013FE20[12]},
    {1, 0, &D_8013FE20[15]},
    {1, 0, &D_8013FE20[18]},
    {1, 0, &D_8013FE20[21]},
    {1, 0, &D_8013FE20[24]},
    {1, 0, &D_8013FE20[27]},
    {1, 0, &D_8013FE20[30]},
    {1, 0, &D_8013FE20[33]},
    {1, 0, &D_8013FE20[34]},
};
static MenuItem D_8013F86C[5] = { /* main menu */
    {1, D_8013F5C8, D_8013FEC8},
    {1, &D_8013F5C8[1], &D_8013FEC8[1]},
    {1, &D_8013F5C8[2], D_8013FECC},
    {1, &D_8013F5C8[3], &D_8013FECC[1]},
    {1, &D_8013F5C8[4], D_8013FED0},
};
static MenuItem D_8013F8A8[7] = { /* options menu */
    {1, D_8013F5DC, D_8013FE20},
    {1, &D_8013F5DC[1], &D_8013FE20[1]},
    {1, &D_8013F5DC[2], &D_8013FE20[2]},
    {1, &D_8013F5DC[3], &D_8013FE20[3]},
    {1, &D_8013F5DC[4], &D_8013FE20[4]},
    {1, &D_8013F5DC[5], &D_8013FE20[5]},
    {1, &D_8013F5DC[6], &D_8013FE20[6]},
};
static MenuItem D_8013F8FC[1] = { /* brightness */
    {1, &D_8013F73C, D_8013FE20},
};
#define D_8013F904 (D_8013F8FC[0].textIds)
static MenuItem D_8013F908[12] = { /* debug menu */
    {1, D_8013F5F8, D_8013FE20},
    {1, &D_8013F5F8[16], &D_8013FE20[1]},
    {1, &D_8013F5F8[22], &D_8013FE20[22]},
    {3, &D_8013F5F8[1], &D_8013FE20[2]},
    {1, &D_8013F5F8[4], &D_8013FE20[5]},
    {1, &D_8013F5F8[5], &D_8013FE20[6]},
    {3, &D_8013F5F8[6], &D_8013FE20[7]},
    {1, &D_8013F5F8[9], &D_8013FE20[10]},
    {3, &D_8013F5F8[10], &D_8013FE20[11]},
    {3, &D_8013F5F8[19], &D_8013FE20[19]},
    {3, &D_8013F5F8[13], &D_8013FE20[14]},
    {1, &D_8013F5F8[17], &D_8013FE20[18]},
};
static MenuItem D_8013F998[19] = { /* level select */
    {1, D_8013F654, D_8013FE20},
    {1, &D_8013F654[1], &D_8013FE20[1]},
    {1, &D_8013F654[2], &D_8013FE20[2]},
    {1, &D_8013F654[3], &D_8013FE20[3]},
    {1, &D_8013F654[4], &D_8013FE20[4]},
    {1, &D_8013F654[5], &D_8013FE20[5]},
    {1, &D_8013F654[6], &D_8013FE20[6]},
    {1, &D_8013F654[7], &D_8013FE20[7]},
    {1, &D_8013F654[8], &D_8013FE20[8]},
    {1, &D_8013F654[9], &D_8013FE20[9]},
    {1, &D_8013F654[10], &D_8013FE20[10]},
    {1, &D_8013F654[11], &D_8013FE20[11]},
    {1, &D_8013F654[12], &D_8013FE20[12]},
    {1, &D_8013F654[13], &D_8013FE20[13]},
    {1, &D_8013F654[14], &D_8013FE20[14]},
    {1, &D_8013F654[15], &D_8013FE20[15]},
    {1, &D_8013F654[16], &D_8013FE20[16]},
    {1, &D_8013F654[17], &D_8013FE20[17]},
    {1, &D_8013F654[18], &D_8013FE20[18]},
};
#define D_8013FAA0 (D_8013FA7C[3])
static MenuItem D_8013FA7C[7] = { /* sound menu */
    {1, D_8013F6A0, D_8013FE20},
    {1, &D_8013F6A0[1], &D_8013FE20[1]},
    {1, &D_8013F6A0[2], &D_8013FE20[2]},
    {3, &D_8013F6A0[5], &D_8013FE20[5]},
    {1, &D_8013F6A0[4], &D_8013FE20[4]},
    {1, &D_8013F6A0[8], &D_8013FE20[8]},
    {1, &D_8013F6A0[3], &D_8013FE20[3]},
};
static s8 D_8013FAD0[8][6] = {6, 0, -3, 3, -3, -3, 4, 4, -4, 0, 0, -5, -1, 6, -4, -4, 3, -3, -5, 4, 0, -6, 5, 0, -6, 0, 3, -3, 3, 3, -5, -5, 6, 0, 0, 4, -1, -6, 3, 3, -4, 4, 5, -5, 0, 3, -5, 0}; /* player arrow shape per direction */
const MapLocation mSubwayMapLocations[5] = {{0, 0, 1, -84}, {1, 0, 48, 83}, {2, 0, -17, -67}, {3, 1, -11, 3}, {4, 1, 28, -14}};
const MapLocation mSubway2MapLocations[3] = {{0, 255, 0, 0}, {1, 1, -19, -12}, {2, 1, -32, -78}};
const MapLocation mParkMapLocations[5] = {{0, 255, 0, 0}, {1, 0, 31, -8}, {2, 0, 14, -24}, {3, 0, 13, -44}, {4, 0, 15, -85}};
const MapLocation mBaseExtMapLocations[6] = {{0, 255, 0, 0}, {1, 255, 0, 0}, {2, 0, 70, 37}, {3, 0, 39, 8}, {4, 255, 0, 0}, {5, 0, -12, -46}};
const MapLocation mMuseumMapLocations[4] = {{0, 0, 8, -34}, {1, 255, 0, 0}, {2, 0, 27, 69}, {3, 255, 0, 0}};
const MapLocation mMuseum2MapLocations[4] = {{0, 255, 0, 0}, {1, 255, 0, 0}, {2, 255, 0, 0}, {2, 0, -47, 62}};
const MapLocation mChurch2MapLocations[3] = {{0, 255, 0, 0}, {1, 255, 0, 0}, {2, 0, 23, -41}};
const MapLocation mWhouseMapLocations[4] = {{0, 255, 0, 0}, {1, 255, 0, 0}, {2, 255, 0, 0}, {3, 0, 28, 0}};
const MapLocation mSiloMapLocations[6] = {{0, 255, 0, 0}, {1, 255, 0, 0}, {2, 0, 6, 56}, {3, 0, 54, 42}, {4, 0, 54, 42}, {5, 255, 0, 0}};
const MapLocation *mObjectiveMapLocations[19] = {
    mSubwayMapLocations, mSubway2MapLocations, 0, mParkMapLocations, mParkMapLocations,
    mMuseumMapLocations, mMuseum2MapLocations, mBaseExtMapLocations, 0, 0,
    mBaseExtMapLocations, 0, 0, 0, mWhouseMapLocations, 0, 0, 0, mSiloMapLocations,
};
typedef s32 (*MapLocator)(s32 map, VECTOR32 *pos, ScreenPos *out);
static MapLocator D_8013FB4C[19] = { /* map locator per level */
    (MapLocator)MapLocateSubway,   (MapLocator)MapLocateSubway,   0,
    (MapLocator)MapLocatePark,     (MapLocator)MapLocatePark,     (MapLocator)MapLocateMuseum,
    (MapLocator)MapLocateMuseum2,  (MapLocator)MapLocateBaseExt,  0,
    0,                             (MapLocator)MapLocateBaseExt,  (MapLocator)MapLocateChurch,
    (MapLocator)MapLocateChurch2,  0,                             (MapLocator)MapLocateWhouse,
    (MapLocator)MapLocateInWhouse, 0,                             0,
    (MapLocator)MapLocateSilo,
};
static char *D_8013FB98[8][2] = { /* objective rows: check mark, text */
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
    {"- ", 0},
};
static MenuItem D_8013FBD8[14] = { /* controller menu */
    {3, &D_8013F6C4[21], &D_8013FE20[21]},
    {3, &D_8013F6C4[24], &D_8013FE20[24]},
    {2, &D_8013F6C4[28], &D_8013FE20[28]},
    {1, &D_8013F6C4[27], &D_8013FE20[27]},
    {2, D_8013F6C4, D_8013FE20},
    {2, &D_8013F6C4[2], &D_8013FE20[2]},
    {2, &D_8013F6C4[4], &D_8013FE20[4]},
    {2, &D_8013F6C4[6], &D_8013FE20[6]},
    {2, &D_8013F6C4[8], &D_8013FE20[8]},
    {2, &D_8013F6C4[10], &D_8013FE20[10]},
    {2, &D_8013F6C4[12], &D_8013FE20[12]},
    {2, &D_8013F6C4[14], &D_8013FE20[14]},
    {2, &D_8013F6C4[16], &D_8013FE20[16]},
    {3, &D_8013F6C4[18], &D_8013FE20[18]},
};
static char *D_8013FC80 = 0; /* dialog message */
static MenuItem D_8013FC84[1] = { /* dialog */
    {1, &D_8013FC80, D_8013FE20},
};
static s32 D_8013FC90 = 0;
static char **D_8013FC94 = 0;
static u16 *D_8013FC98 = &D_8013FE20[1];
static u32 D_8013FC9C = 0; /* dialog type */
static void (*D_8013FCA0)() = 0; /* dialog callback */
static u32 D_8013FCA4 = -1; /* tick the menu opened at */
static u32 D_8013FCA8 = -1; /* tick of the next briefing page turn */
static u8 D_8013FCAC[5] = {8, 10, 11, 7, 5}; /* submenu per main menu row */
static s32 D_8013FCB4 = 0; /* current menu */
static u8 D_8013FCB8 = 0; /* menu stack depth */
static MenuItem *D_8013FCBC = 0; /* items of the current menu */
static u8 D_8013FCC0[14] = {5, 12, 19, 7, 14, 7, 2, 0, 0, 1, 0, 0, 0, 0}; /* items per menu */
static MenuItem *D_8013FCD0[14] = { /* items of each menu */
    D_8013F86C, D_8013F908, D_8013F998, D_8013FA7C, D_8013FBD8, D_8013F8A8, D_8013FC84,
    D_8013F7D0, D_8013F7D0, D_8013F8FC, D_8013F7D0, D_8013F7D0, D_8013F7D0, 0,
};
static void (*D_8013FD08[14])(void) = { /* per-menu select handlers */
    func_801396B4, func_80139738, func_80139994, func_801399C4, 0, func_801398A4,
    func_80139C54, func_80139B54, 0, func_80139CE4, 0, 0, func_80139710, func_80139D24,
};
static void (*D_8013FD40[14])(void) = { /* per-menu before-build handlers */
    0, 0, func_8013E30C, 0, 0, 0, 0, func_8013D04C, func_8013D19C, 0, func_8013D95C,
    func_8013DC98, func_8013DEEC, func_8013DFB0,
};
static void (*D_8013FD78[14])(void) = { /* per-menu after-build handlers */
    0, func_8013E184, 0, func_8013E278, func_8013E324, 0, 0, 0, 0, func_8013E480, 0, 0, 0,
    0,
};
static void (*D_8013FDB0[14])(void) = { /* per-menu leave handlers */
    0, 0, 0, func_8013AE30, 0, 0, 0, 0, func_8013AE8C, func_8013AFC8, 0, 0, 0,
    func_8013B04C,
};
static s32 D_8013FDE8 = 0xA; /* current help line */
static u8 D_8013FDEC[9] = {255, 255, 255, 255, 255, 255, 255, 255, 255};
static u8 D_8013FDF8[2][9] = {3, 4, 0, 2, 1, 5, 6, 7, 8, 1, 2, 7, 6, 5, 4, 0, 3, 8}; /* controller presets */
static u32 D_8013FE0C = 0; /* accept/default/cancel row selection */
static s32 D_8013FE10 = 0; /* direction being repeated */
static u16 mRepeatButtons = 0x840; /* buttons held last read */
static u16 mRepeatTicks = 0x0; /* reads since the last repeat */
static u16 mRepeatCount = 0x0; /* repeats so far */
static u16 mRepeatDelay = 0x4; /* reads between repeats */
static u16 D_8013FE1C = 0x1; /* slider step */

/* per-menu setup */
static void (*const D_80138044[14])(s32 cursor) = {
    func_80139FFC, funcEvSpIOEx, func_80139D64, func_8013B464, func_8013B8F8, func_80139D64, func_8013A60C,
    func_8013ACAC, func_8013A744, (void (*)(s32))func_8013AC64, func_8013A220, func_8013A220, func_8013A4F8,
    (void (*)(s32))func_8013ACA4,
};
/* the asserts' file name (the original source path, not the usual "s/..."); asm uses it too */
static const char D_8013807C[] = "bin/menu.c";
void iMenuVolumeMusicStop(void) {
    SysPauseSound(1);
    SysStopSong(1, 0);
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
}

#ifndef _PSYQ
/* addresses inside the data above that the asm functions use */
__asm__(".set D_8013F7D4, D_8013F7D0+4\n"
        ".set D_8013F7D8, D_8013F7D0+8\n"
        ".set D_8013FB9C, D_8013FB98+4\n"
        ".set D_8013FBE0, D_8013FBD8+8\n"
        ".set D_8013FBF8, D_8013FBD8+0x20\n"
        ".set D_8013FCC8, D_8013FCC0+8\n"
        ".set D_8013FCCC, D_8013FCC0+12\n"
        ".set D_8013FE26, D_8013FE20+6\n"
        ".set D_8013FE64, D_8013FE20+0x44\n"
        ".set D_8013FE96, D_8013FE94+2\n"
        ".set D_8013FE9A, D_8013FE98+2\n"
        ".set D_8013FEA6, D_8013FEA4+2\n"
        ".set D_8013FEAA, D_8013FEA8+2\n");
#endif

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

/* The quad colour word as a struct member: unlike setColorCode, the store can be
 * scheduled past the blend's stack stores. */
typedef struct MenuQuadColour {
    /* 0x00 */ u32 unk00[3];
    /* 0x0C */ u32 rgbc;
} MenuQuadColour;
#define setQuadColour(p, c) (((MenuQuadColour *)(p))->rgbc = (c))

/* Blends two colours (0x00BBGGRR) with a cosine pulse on the system tick. */
static inline u32 MenuBlendColour(u32 a, u32 b) {
    u8 out[4];
    s32 t;
    s32 i;

    t = (rcos(gSystemTick << 8) + 0x1000) >> 5;
    out[3] = 0;
    for (i = 0; i < 3; i++) {
        out[i] = (t * ((u8 *)&a)[i] + (0x100 - t) * ((u8 *)&b)[i]) >> 8;
    }
    return *(u32 *)out;
}

/* Pulses the colour of quad D_8013F7C4 - 1 (if any) and of D_8013FEBC. */
void func_801387B0(void) {
    if (D_8013F7C4 > 0) {
        setQuadColour(&D_8013FEC0[D_8013F7C4 - 1].q[0], MenuBlendColour(0x5C5CFF, 0xFFFF) | 0x28000000);
        setQuadColour(D_8013FEBC, MenuBlendColour(0xA4A45C, 0xC05C00) | 0x28000000);
        return;
    }
    setQuadColour(D_8013FEBC, MenuBlendColour(0x5CFFFF, 0x5CFF) | 0x28000000);
}

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
/* register allocation of the x/y invariants and the end-x register. Original: half=s3, x1&0xFFFF=s4, quad=s5,
 * top=s6 (priority half > x1 > quad > top); ours puts half last. `x1 = ...; x1 &= 0xFFFF;` + `x1 | top` removes
 * our extra loop-2 copy (22 lines) but not the order; u16 x1 is worse.
 * r5B: in the original `value - half - 0x39` stays in loop 1 (value reloaded there) and its masked value is ONE
 * pseudo (s4) used by both loops. Any temp form (`(..) & 0xFFFF`, u16, a separate masked var) lets loop.c hoist
 * the subtraction (31 lines); only a user var used after the loop stays. half needs ~8 refs (now 5) to land in s3. */
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


void func_80139738(void) {
    Gobj *player;
    s32 v;

    v = D_8013F7C4;
    switch (v) {
        case 0:
            func_80138DE0(2);
            break;
        case 1:
        case 2:
            v = 0xFF;
            gObjectives->unk24 = gObjectives->completed = gObjectives->unk18 = gObjectives->unk1C = v;
            gObjectives->unk20 = gObjectives->unk14 = ((struct { u8 v; } *)&gMissionFailed)->v = 0;
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

void func_80139B54(void) {
    s32 i;

    for (i = 0; i < 26; i++) {
        do {
            if (MENU_STRINGS[i] < (char *)D_8013FE78 || MENU_STRINGS[i] > (char *)D_8013FE78 + 0xF000) {
                StreamSynchronous();
            }
        } while (0);
    }
    func_80139044(MENU_STRINGS[D_8013F7D0[D_8013F7C4].textIds - D_8013FE20], 1, 0, func_80139A50);
    func_801389C8(gObjPlayer->cas->active != 8 ? 7 : 2);
}

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

void func_80139D64(s32 cursor) {
    MenuItem *item;
    RECT *rect;
    s32 i;

    if (D_80138044[D_8013FCB4] == func_80139D64) {
        func_801389C8(1);
    }
    if (gSystemTick >= D_8013FCA8) {
        if (D_8013F7C4 < D_8013FCC0[D_8013FCB4]) {
            MenuItem *cur = &D_8013FCBC[D_8013F7C4];

            if (*cur->textIds != 0xFFFF) {
                TextSetStringIntensity(*cur->textIds, 0x50, 0x82, 0xF0);
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
                for (i = cursor; i >= 0; i--) {
                    if (*D_8013FCBC[i].textIds != 0xFFFF) {
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
    const MapLocation *loc;
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

/* Menu pad state */
typedef struct MenuPad {
    /* 0x0 */ u8 status; /* 0xFF: no pad */
    /* 0x1 */ u8 unk1[3];
    /* 0x4 */ u16 buttons;
} MenuPad;


#define MENU_DIR_LEFT 1
#define MENU_DIR_RIGHT 2
#define MENU_DIR_UP 3
#define MENU_DIR_DOWN 4

void EGSDeviceControllerRead(MenuPad **pad, s32 port);
s32 func_8013B2B0(void);
s32 func_8013B38C(void);
s32 func_8013B820(s32 delta);
void func_8013A60C(s32 cursor);
void func_801396B4(void);
void func_80139044(char *msg, u32 type, s32 cursor, void (*done)());
void func_8013BBE4(u16 pressed);

/* Bit-string access tolerant of an unaligned base (the pad buttons are tested
 * through the address of a u16).  The two test variants differ only in whether
 * the bit index is a fresh local or the parameter itself, which changes how the
 * base address is kept in a loop. */
static inline u32 BitTest(void *base, s32 n) {
    u32 *words = base;
    s32 bit = n;
    s32 misalign = (u32)base & 3;
    s32 word;
    u32 mask;

    if (misalign) {
        bit = n + misalign * 8;
        words = (u32 *)((u32)base & ~3);
    }
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    return words[word] & mask;
}

static inline u32 BitTestAt(void *base, s32 bit) {
    u32 *words = base;
    s32 misalign = (u32)base & 3;
    s32 word;
    u32 mask;

    if (misalign) {
        bit = bit + misalign * 8;
        words = (u32 *)((u32)base & ~3);
    }
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    return words[word] & mask;
}

static inline void BitSet(void *base, s32 n) {
    u32 *words = base;
    s32 bit = n;
    s32 misalign = (u32)base & 3;
    s32 word;
    u32 mask;

    if (misalign) {
        bit = n + misalign * 8;
        words = (u32 *)((u32)base & ~3);
    }
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    words[word] |= mask;
}

void JoystickSetFlightControls(u8 enable);
void GameSysJoystickInit(void);
void func_8013E324(void);

/* Controller menu input: vibration and flight-control toggles, preset layouts,
 * button assignment and the accept/default/cancel row. */
void func_8013BBE4(u16 pressed) {
    MenuItem *item = &D_8013FBD8[D_8013F7C4];
    u32 used;
    s32 i;
    u32 n;
    s32 action;
    RECT *rect;

    if (D_8013F7C4 < 4) {
        if (BitTest(&pressed, 11)) {
            func_8013EE78();
            return;
        }
        if (BitTest(&pressed, 4)) {
            func_80138EE4();
            return;
        }
    }
    switch (D_8013F7C4) {
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            action = D_8013F7C4 - 4;
            for (i = 0; i < 9; i++) {
                if (BitTest(&pressed, i)) {
                    u8 old = D_8013FDEC[i];
                    s32 j;

                    if (old != action && old != 0xFF) {
                        D_8013FBD8[old + 4].textIds[1] = TextReplaceString(D_8013FBD8[old + 4].textIds[1], (char *)D_801377E4);
                    }
                    for (j = 0; j < 9; j++) {
                        if (D_8013FDEC[j] == action) {
                            D_8013FDEC[j] = 0xFF;
                        }
                    }
                    D_8013FDEC[i] = action;
                    item->textIds[1] = TextReplaceString(item->textIds[1], gButtonStrings[i]);
                    item = &D_8013FBD8[2];
                    item->textIds[1] = TextReplaceString(item->textIds[1], D_8013F7AC[func_8013BB3C()]);
                }
            }
            D_8013FE0C = 0;
            break;
        case 0:
            if (BitTest(&pressed, 13)) {
                EGSDeviceControllerVibrate(4, 0, 0);
                func_80139150(item, 0, 1);
            } else if (BitTest(&pressed, 15)) {
                EGSDeviceControllerVibrate(3, 0, 0);
                func_80139150(item, 1, 1);
            }
            D_8013FE0C = 0;
            break;
        case 1:
            if (BitTest(&pressed, 13)) {
                JoystickSetFlightControls(0);
                func_80139150(item, 0, 1);
            } else if (BitTest(&pressed, 15)) {
                JoystickSetFlightControls(1);
                func_80139150(item, 1, 1);
            }
            D_8013FE0C = 0;
            break;
        case 2:
            D_8013FE0C = 0;
            n = func_8013BB3C();
            if (BitTest(&pressed, 13)) {
                n++;
            }
            if (BitTest(&pressed, 15)) {
                n--;
            }
            if (n < 2) {
                for (i = 0; i < 9; i++) {
                    gJoyMap[i] = D_8013FDF8[n][i];
                }
                GameSysJoystickInit();
                func_8013E324();
            }
            break;
        case 3:
            break;
        case 13:
            if (BitTest(&pressed, 6)) {
                switch (D_8013FE0C) {
                    case 2:
                        for (i = 0; i < 9; i++) {
                            gJoyMap[i] = D_8013FDF8[0][i];
                        }
                        func_8013E324();
                        break;
                    case 1:
                        used = 0;
                        for (i = 0; i < 9; i++) {
                            if (D_8013FDEC[i] != 0xFF) {
                                BitSet(&used, D_8013FDEC[i]);
                            }
                        }
                        for (i = 0; i < 9; i++) {
                            if (!BitTestAt(&used, i)) {
                                break;
                            }
                        }
                        if (i == 9) {
                            for (i = 0; i < 9; i++) {
                                gJoyMap[i] = D_8013FDEC[i];
                            }
                            GameSysJoystickInit();
                            func_80138EE4();
                        } else {
                            func_8013B8F8(i + 4);
                        }
                        break;
                    case 0:
                        func_8013E324();
                        func_8013B8F8(2);
                        break;
                }
                break;
            }
            if (BitTest(&pressed, 13)) {
                switch (D_8013FE0C) {
                    case 0:
                        D_8013FE0C = 1;
                        TextSetStringIntensity(item->textIds[0], 0x50, 0x82, 0xF0);
                        TextSetStringIntensity(item->textIds[1], 0x8C, 0xF0, 0x8C);
                        break;
                    case 1:
                        D_8013FE0C = 2;
                        TextSetStringIntensity(item->textIds[1], 0x50, 0x82, 0xF0);
                        TextSetStringIntensity(item->textIds[2], 0xF0, 0x78, 0x78);
                        break;
                }
            } else if (BitTest(&pressed, 15)) {
                switch (D_8013FE0C) {
                    case 1:
                        D_8013FE0C = 0;
                        TextSetStringIntensity(item->textIds[0], 0xF0, 0x78, 0x78);
                        TextSetStringIntensity(item->textIds[1], 0x50, 0x82, 0xF0);
                        break;
                    case 2:
                        D_8013FE0C = 1;
                        TextSetStringIntensity(item->textIds[1], 0x8C, 0xF0, 0x8C);
                        TextSetStringIntensity(item->textIds[2], 0x50, 0x82, 0xF0);
                        break;
                }
            }
            rect = TextStringRect(item->textIds[D_8013FE0C], 0);
            rect->x -= 3;
            rect->y -= 2;
            rect->w += 6;
            rect->h += 4;
            func_80138AD4(rect->x, rect->y, rect->w, rect->h);
            break;
    }
}

/* Reads the pad and drives the menu: cursor moves, value changes, auto-repeat. */
void func_8013C530(void) {
    MenuPad *pad;
    u16 pressed;
    s32 dir = 0;
    s32 handled;
    u16 buttons;

    if (gSystemTick < D_8013FCA8) {
        return;
    }
    EGSDeviceControllerRead(&pad, 0);
    if (pad->status == 0xFF) {
        if (mRepeatButtons != 0xFFFF) {
            mRepeatButtons = 0xFFFF;
            func_80139044(D_8013F77C, 0, 0, NULL);
        }
        return;
    }
    buttons = pad->buttons;
    if (gSystemTick == D_8013FCA8) {
        mRepeatButtons = buttons;
        return;
    }
    if (!(buttons & mRepeatButtons)) {
        pressed = buttons & ~mRepeatButtons;
    } else {
        pressed = 0;
    }

    if (BitTest(&pressed, 14)) {
        dir = MENU_DIR_DOWN;
    }
    if (BitTest(&pressed, 12)) {
        if (dir == MENU_DIR_DOWN) {
            dir = 0;
        } else {
            dir = MENU_DIR_UP;
        }
    }
    if (dir == 0) {
        if (BitTest(&pressed, 15)) {
            dir = MENU_DIR_LEFT;
        }
        if (BitTest(&pressed, 13)) {
            if (dir == MENU_DIR_LEFT) {
                dir = 0;
            } else {
                dir = MENU_DIR_RIGHT;
            }
        }
    }

    if (pressed != 0) {
        handled = 0;
        if (dir == MENU_DIR_DOWN) {
            D_80138044[D_8013FCB4](D_8013F7C4 + 1);
        }
        if (dir == MENU_DIR_UP) {
            D_80138044[D_8013FCB4](D_8013F7C4 - 1);
        }
        switch (D_8013FCB4) {
            case 1:
                if (dir == MENU_DIR_RIGHT) {
                    handled = func_8013B38C();
                }
                if (dir == MENU_DIR_LEFT) {
                    handled = func_8013B2B0();
                }
                break;
            case 3:
                if (dir == MENU_DIR_RIGHT) {
                    handled = func_8013B820(D_8013FE1C);
                }
                if (dir == MENU_DIR_LEFT) {
                    handled = func_8013B820(-D_8013FE1C);
                }
                break;
            case 6:
                if (dir == MENU_DIR_RIGHT) {
                    func_8013A60C(D_8013F7C4 + 1);
                }
                if (dir == MENU_DIR_LEFT) {
                    func_8013A60C(D_8013F7C4 - 1);
                }
                handled = 1;
                break;
            case 4:
            case 9:
                handled = 1;
                break;
            case 13:
                switch (dir) {
                    case 0:
                        break;
                    case MENU_DIR_UP:
                        if (POSITION.y > 0x68) {
                            POSITION.y--;
                        }
                        break;
                    case MENU_DIR_DOWN:
                        if (POSITION.y < 0x88) {
                            POSITION.y++;
                        }
                        break;
                    case MENU_DIR_LEFT:
                        if (POSITION.x > 0xB0) {
                            POSITION.x--;
                        }
                        break;
                    case MENU_DIR_RIGHT:
                        if (POSITION.x < 0xD0) {
                            POSITION.x++;
                        }
                        break;
                }
                handled = 1;
                break;
        }
        if (!handled) {
            if (D_8013FCB4 == 0) {
                if (dir == MENU_DIR_LEFT) {
                    func_801396B4();
                }
            } else if (dir == MENU_DIR_RIGHT) {
                while (D_8013FCB8 != 0) {
                    func_80138EE4();
                }
                if (D_8013FCB4 != 0) {
                    func_80138B1C(0);
                }
            }
        }
        mRepeatTicks = 0;
        if (D_8013FCB4 == 4) {
            func_8013BBE4(pressed);
        } else {
            if (BitTest(&pressed, 11)) {
                func_8013EE78();
            }
            if (dir == 0) {
                if (BitTest(&pressed, 4)) {
                    if (D_8013FCB4 == 12 && D_8013F7C4 > 0) {
                        func_8013A4F8(D_8013F7C4 - 1);
                    } else {
                        func_80138EE4();
                    }
                }
                if (D_8013FD08[D_8013FCB4] != NULL && BitTest(&pressed, 6)) {
                    buttons |= 0x800;
                    D_8013FD08[D_8013FCB4]();
                }
            }
        }
    }

    if (buttons & 0xF000) {
        if (mRepeatButtons != 0 || D_8013FE10 == dir) {
            mRepeatButtons = buttons;
            mRepeatTicks++;
            if (mRepeatTicks > mRepeatDelay) {
                mRepeatTicks = 0;
                mRepeatButtons = buttons & 0xFFF;
                mRepeatCount++;
            }
            if (mRepeatCount >= 5) {
                if (mRepeatDelay != 0) {
                    mRepeatCount = 0;
                    mRepeatDelay--;
                } else {
                    D_8013FE1C = (mRepeatCount >> 4) + 2;
                }
            }
        } else {
            mRepeatButtons = buttons;
            mRepeatTicks = 0;
            mRepeatCount = 0;
            D_8013FE1C = 1;
            D_8013FE10 = dir;
            if ((dir == MENU_DIR_LEFT || dir == MENU_DIR_RIGHT || D_8013FCB4 == 13) && D_8013FCB4 != 6 &&
                D_8013FCB4 != 4) {
                mRepeatDelay = 1;
            } else {
                mRepeatDelay = 6;
            }
        }
    } else {
        mRepeatButtons = buttons;
        D_8013FE10 = 0;
        mRepeatCount = 0;
    }
    if (dir == MENU_DIR_UP || dir == MENU_DIR_DOWN) {
        SysPlaySong(5, 2, 0, 0);
    }
}

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

/* Sized 12 (padding included): as a plain literal, ccpsx packs the next string with no word padding. */
static const char D_mapFmt[12] = "MAP%d.TIM";

/* Map screen: load the level maps and place the player arrow. */
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
            sprintf(name, D_mapFmt, i + 1);
            D_8013F7CC = 8;
            if (StreamHogBlock(name, D_8013FE78, 0xF000, 0, NULL) == 0) {
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
            } else {
                maps = i;
                break;
            }
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
                    angle = ratan2(pos.vy, pos.vx);
                    angle += 0x100;
                    if (angle < 0) {
                        angle += 0x1000;
                    }
                    angle = FIXED_INT(angle * 8);
                    for (k = 0; k < 6; k++) {
                        arrow[k] = D_8013FAD0[angle][k];
                    }
                    for (k = 0; k < 2; k++) {
                        *(s32 *)&quad[k].x0 = MAP_XY(pt[0].x + arrow[2] - 0x39, pt[0].y + arrow[3] - 5);
                        *(s32 *)&quad[k].x1 = MAP_XY(pt[0].x + arrow[0] - 0x39, pt[0].y + arrow[1] - 5);
                        *(s32 *)&quad[k].x2 = MAP_XY(pt[0].x - 0x39, pt[0].y - 5);
                        *(s32 *)&quad[k].x3 = MAP_XY(pt[0].x + arrow[4] - 0x39, pt[0].y + arrow[5] - 5);
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

void func_8013DEEC(void) {
    MenuItem *items = D_8013F7D0;
    u16 *ids;
    char **names;
    s32 i;

    if (D_8013FE6C->unk28 == 0) {
        s32 one;

        i = 0;
        one = 1;
        ids = D_8013FE20;
        names = gBriefings->names;
        D_8013FE90 = 0;
        D_8013FE8C = 0;
        D_8013FEA8 = 0;
        D_8013FEA4 = 0;
        for (; i < 3; i++) {
            items[i].textIds = ids++;
            items[i].strings = names++;
            items[i].count = one;
        }
        items[3].count = 1;
        items[3].strings = &gBriefings->texts[D_8013F7C4];
        items[3].textIds = D_8013FE26;
        D_8013FCC0[12] = 4;
    }
}

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
            do {
                TextSetStringJustification(id, 0);
                D_8013FBD8[action + 4].textIds[1] = id;
            } while (0);
        }
    }
}

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
/* 65 lines left (r3C). The original reaches the .y halves through a forced address
 * (`((s16 *)&v)[1]` = INDIRECT_REF of a constant -> memory_address forces it into a reg,
 * like XY(v).x does), which gives the addiu/lhu/sh RMW form; the first test reads
 * `*(s32 *)&XY(D_8013FEA4)` (a2 base). Still open: the original computes all four deltas
 * before any store (ours sinks/hoists the stores differently) and the quad corner loads.
 * r6H: the /4s differ too: orig `bgez x; move t,x; addiu t,x,3; sra` (expand_divmod's adjusted copy
 * with x still live), ours adds in place; s16 deltas / re-reading XY(D_8013FEA4) are worse (68-73). */
void func_8013E5B0(void) {
    s32 dx0, dy0, dx1, dy1;
    s32 x, y;
    s32 x1, y1;
    MenuQuad *quad;

    if (*(s32 *)&XY(D_8013FEA4) == 0 && D_8013FEA8 == 0) {
        dx0 = XY(D_8013FE8C).x - XY(D_8013FE94).x;
        dy0 = XY(D_8013FE8C).y - XY(D_8013FE94).y;
        dx1 = XY(D_8013FE90).x - XY(D_8013FE98).x;
        dy1 = XY(D_8013FE90).y - XY(D_8013FE98).y;
        XY(D_8013FEA4).x = dx0;
        XY(D_8013FEA4).y = dy0;
        XY(D_8013FEA8).x = dx1;
        XY(D_8013FEA8).y = dy1;
        XY(D_8013FE9C).x = dx0 / 4;
        XY(D_8013FE9C).y = dy0 / 4;
        XY(D_8013FEA0).x = dx1 / 4;
        XY(D_8013FEA0).y = dy1 / 4;
    }
    HILITE_STEP(XY(D_8013FE94).x, XY(D_8013FE8C).x, XY(D_8013FE9C).x);
    HILITE_STEP(((s16 *)&D_8013FE94)[1], XY(D_8013FE8C).y, XY(D_8013FE9C).y);
    HILITE_STEP(XY(D_8013FE98).x, XY(D_8013FE90).x, XY(D_8013FEA0).x);
    HILITE_STEP(((s16 *)&D_8013FE98)[1], XY(D_8013FE90).y, XY(D_8013FEA0).y);
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
/* 11 lines left (r3C), 4 of them the "\\%s\\MENU.HOG;1" literal that the original has in
 * .text right before the function (needs a static const char[16] when unwrapping).
 * Fixed: the line colours are `(c ? 0xA0 : 0x32) | (c' ? bottom << 16 : top << 16)` (two
 * conditionals that jump threading merges; the b one tests !(j & 1) in the second arm),
 * computed into locals before the call; for-increment order top, bottom, i, item; a
 * reused temp `p` for the layout pointers keeps the stores in order. Still open: the
 * original copies chars from v1 into s5 (addiu v1; move s5,v1) and stores mMap right
 * after mem += 0x40F8 (ours sinks it).
 * r4D: the s5 copy survives only if some insn uses p between `p = mem + 0x3390` and
 * `chars = p` (flow makes the LOG_LINK from the first following use, so combine never sees
 * the copy): `D_8013FE68 = (TextChar *)p; chars = D_8013FE68;` reproduces the copy, but then
 * the D_8013FE68 store is early (original: late, from s5). The mMap store is moved by sched1
 * below `mem += 0xF000` (insn order 34,40,43,45,37).
 * r4H: 2 lines left. `base = mem + 0xF000` (a once-set pseudo is "birthing", so sched1 takes it
 * before the mMap store) fixed the mMap order. The copy: cse_insn's (set REG0 REG1) swap rewrites
 * `p = mem+0x3390; chars = p;` into `chars = mem+0x3390; p = chars` (dead) whenever chars outlives
 * p (make_regs_eqv makes it qty_first_reg); giving p a use after the loop stops the swap, but
 * then combine merges the copy because p dies there.
 * r5B: cse_insn only swaps when the copy directly follows p's set, and combine refuses only if mem changes (or a
 * call sits) between them: `c = mem+0x3390; ... mem += 0x40F8; chars = c;` keeps the copy (no swap/combine) but
 * sched1 leaves it late (chars is set twice -> not birthing; luid order), 6 lines. */
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
    s32 a, b;
    u8 *p;
    u8 *base;

    mem = EGSDisplayPacketAreaMalloc(0x15058);
    p = mem + 0x3390;
    chars = (TextChar *)p;
    p = mem + 0x39EC;
    D_8013FE70 = (TextChar *)p;
    p = mem + 0x3E38;
    D_8013FE6C = (TextChar *)mem;
    mem += 0x40F8;
    mMap = (TextChar *)p;
    D_8013FE78 = mem;
    base = mem + 0xF000;
    D_8013FEC4 = (MenuQuad *)(base + 0x200);
    D_8013FEBC = (MenuQuad *)(base + 0x41C);
    gMenuPolys = (MenuQuad *)(base + 0x464);
    D_8013FEC0 = (MenuQuadPair *)(base + 0x8C0);
    D_8013FE68 = chars;
    D_8013FE84 = (u32 *)base;
    gMenuLines = (MenuLine *)(base + 0xB00);
    lines = (MenuLine *)(base + 0x1580);
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
    for (i = 0, top = 0x14, bottom = 0x1F; i < 5; top += 16, bottom += 16, i++, item++) {
        *item->textIds = TextAddString(D_8013F7C1, *item->strings, -1, chars);
        if (*item->textIds == 0xFFFF) {
            func_8013EE78();
            break;
        }
        TextSetStringSpeed(*item->textIds, 0x800);
        chars += TextStringLength(*item->textIds, 1);
        for (j = 0; j < 4; j++) {
            a = ((j & 2) ? 0xA0 : 0x32) | ((j & 2) ? bottom << 16 : top << 16);
            b = ((j & 1) ? 0xA0 : 0x32) | (!(j & 1) ? bottom << 16 : top << 16);
            EGSDisplayBuildLineF(&lines[j + i * 4], 0xF08250, a, b);
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
        TextRemoveString(TextReplaceString(D_8013FE66, (char *)D_801377E4));
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

void func_8013F144(void) {
    TextChar *chars = D_8013FE6C;
    MenuItem *item;
    s32 menu;
    s32 i;
    s32 window;

    if (D_8013F7C0 != 0xFF) {
        u16 none = 0xFFFF;
        for (i = 34; i >= 0; i--) {
            D_8013FE20[i] = none;
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
