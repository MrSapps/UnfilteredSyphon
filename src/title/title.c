#include "common.h"
#include "game.h"
#include "system.h"

/*
 * title.ovl: the title screen menu and the memory card load/save screens
 * (TitleExecute, TitleSaveScreen). Card operations are queued and polled
 * through the libmcrd functions that live in movie.ovl.
 */

/* Title modes (mTitleMode). */
#define TITLE_MODE_MAIN 0      /* "New game" / "Load game" */
#define TITLE_MODE_MESSAGE 1   /* memory card message box */
#define TITLE_MODE_SAVE 2      /* "Do you wish to save your game?" */
#define TITLE_MODE_ATTRACT 3   /* 989 logo / Eidetic / intro movies */
#define TITLE_MODE_SLOTS 4     /* "Choose save slot to use" */
#define TITLE_MODE_SLOT 5      /* a slot was picked */
#define TITLE_MODE_INIT 6      /* first frame */
#define TITLE_MODE_NEW_GAME 7  /* exit: start a new game */
#define TITLE_MODE_LOAD_GAME 8 /* exit: load the saved game */
#define TITLE_MODE_DONE 9      /* exit: back to the game */

/* Memory card state (mCardState); 1..6 are the MemCardSync errors it was set from. */
#define CARD_OK 0
#define CARD_UNKNOWN 7

/* Queued card operations (func_80142A54). */
#define CARD_OP_EXIST 0
#define CARD_OP_ACCEPT 1
#define CARD_OP_READ_HEADER 2
#define CARD_OP_WRITE_HEADER 3
#define CARD_OP_READ_SAVE 4
#define CARD_OP_SAVE 5
#define CARD_OP_SLOTS 7
#define CARD_OP_NONE 8

#define SAVE_SLOT_COUNT 5

/* libmcrd (linked into movie.ovl) */
typedef struct DIRENTRY {
    /* 0x00 */ char name[20];
    /* 0x14 */ s32 attr;
    /* 0x18 */ s32 size;
    /* 0x1C */ struct DIRENTRY *next;
    /* 0x20 */ s32 head;
    /* 0x24 */ char system[4];
} DIRENTRY; /* size 0x28 */

void MemCardStart(void);
void MemCardStop(void);
s32 MemCardExist(s32 chan);
s32 MemCardAccept(s32 chan);
s32 MemCardReadFile(s32 chan, char *file, void *adrs, s32 ofs, s32 bytes);
s32 MemCardWriteFile(s32 chan, char *file, void *adrs, s32 ofs, s32 bytes);
s32 MemCardGetDirentry(s32 chan, char *name, DIRENTRY *dir, s32 *files, s32 ofs, s32 max);
s32 MemCardSync(s32 mode, s32 *cmds, s32 *rslt);
s32 MemCardCreateFile(s32 chan, char *file, s32 blocks);
s32 MemCardFormat(s32 chan);
void MovieSetLoopFlag(s32 flag);
extern u8 gMovieOpenFlag;

typedef struct TitleRGB {
    u8 r, g, b;
} TitleRGB;

typedef struct TitleColor {
    u8 r, g, b, cd;
} TitleColor;

typedef struct EGSSprite {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ TitleRGB color;
    /* 0x17 */ u8 unk17[0x11];
    /* 0x28 */ u32 unk28; /* display list link */
} EGSSprite; /* size 0x2C */

typedef struct EGSTexture {
    /* 0x00 */ void *data;
    /* 0x04 */ u8 unk04[0x2C];
} EGSTexture; /* size 0x30 */

/* EGSDisplay primitives: a list link and a type, then the GPU packet. */
typedef struct TitlePolyF4 {
    /* 0x00 */ void *next;
    /* 0x04 */ s32 type;
    /* 0x08 */ u32 tag;
    /* 0x0C */ u32 rgbc;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
    /* 0x18 */ s32 xy2;
    /* 0x1C */ s32 xy3;
    /* 0x20 */ s32 unk20;
} TitlePolyF4; /* size 0x24 */

typedef struct TitleLineF2 {
    /* 0x00 */ void *next;
    /* 0x04 */ s32 type;
    /* 0x08 */ u32 tag;
    /* 0x0C */ u32 rgbc;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
} TitleLineF2; /* size 0x18 */

#define XY(x, y) (((x) & 0xFFFF) | ((y) << 16))

typedef struct TitleRect {
    s16 x, y, w, h;
} TitleRect;

/* Both halves of a rect at once */
#define RECT_XY(r) (*(s32 *)&(r).x)
#define RECT_WH(r) (*(s32 *)&(r).w)

/* One line of a menu: count strings (1 = plain, 2+ = choices on one line). */
typedef struct TitleMenuEntry {
    /* 0x00 */ s32 count;
    /* 0x04 */ char **strings;
    /* 0x08 */ u16 *textIds;
} TitleMenuEntry; /* size 0xC */

/* Header block at offset 0x200 of the save file. */
typedef struct SyphonHeader {
    /* 0x00 */ s32 level[SAVE_SLOT_COUNT]; /* level saved in each slot, 0 = empty */
    /* 0x14 */ s32 unk14[SAVE_SLOT_COUNT];
    /* 0x28 */ s16 lastSlot;
    /* 0x2A */ s16 lastSlot2;
    /* 0x2C */ u8 unk2C[0x54];
} SyphonHeader; /* size 0x80 */

/* main.exe */
extern void *gTextCamera;
extern void *gInterfaceWindow;
extern s32 gInterfaceCurrentMode;
extern char *gLevelPrettyNames[];
void EGSDisplayBuildPoly4F(void *poly, u32 color, s32 xy0, s32 xy1, s32 xy2, s32 xy3);
void EGSDisplayBuildLineF(void *line, u32 color, s32 xy0, s32 xy1);
void EGSDisplayAddPoly(void *camera, void *poly);
void EGSDisplayDeletePoly(void *camera, void *poly);
void EGSDisplayAddSprite(void *camera, EGSSprite *sprite);
void EGSDisplayDeleteSprite(void *camera, EGSSprite *sprite);
void *EGSDisplayPacketAreaMalloc(s32 size);
void EGSDisplayPacketAreaFree(void);
void EGSDisplayTextOverMovies(s32 on);
void EGSTextureCreate(EGSTexture *tex);
void EGSTextureLoad(EGSTexture *tex);
void EGSTextureCreateSprite(EGSTexture *tex, EGSSprite *sprite, s32 x, s32 y, s32 arg4, s32 arg5);
void EGSTextureSetSpriteTransparency(EGSSprite *sprite, s32 on);
void EGSDeviceControllerRead(u8 **pad, s32 port);
void SysLoadData(char *path, void *addr, s32 size);
void SysPlayMovie(MovieInfo *movie);
void SysChangeLevel(s32 level);
void SaveGetDataBuffer(void **buf, s32 *size);
void SavePrepareMemoryCardSave(void);
void SaveMemoryCardLoaded(void);
void InterfaceSetMode(s32 mode);
void InterfaceAnimate(void);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextRemoveWindowNow(u8 window);
void TextSetWindowJustification(u8 window, s32 centered);
void TextSpriteInitialize(void *buf, s32 count);
u16 TextAddString(u8 window, char *str, s32 arg2, EGSSprite *sprites);
void TextAddStringMulti(u8 window, s32 count, char **strs, s32 arg3, EGSSprite *sprites, u16 *ids);
void TextSetStringIntensity(u16 id, s32 r, s32 g, s32 b);
TitleRect *TextStringRect(u16 id, s32 arg1);
s32 TextStringLength(u16 id, s32 arg1);
void TextUpdate(void);

/* title.ovl bss: one object (aspsx would 16-align separate arrays this big) */
static struct {
    /* 0x000 */ s32 cardOps[10];             /* card op queue */
    /* 0x028 */ TitlePolyF4 outline[13];     /* memory card outline */
    /* 0x1FC */ TitleLineF2 edges[13][2];    /* memory card outline edges */
    /* 0x46C */ TitleLineF2 grid[30];        /* memory card grid */
    /* 0x73C */ TitlePolyF4 highlight;       /* selection highlight */
} titleBss;
#define D_80144920 (titleBss.cardOps)
#define D_80144948 (titleBss.outline)
#define D_80144B1C (titleBss.edges)
#define D_80144D8C (titleBss.grid)
#define D_8014505C (titleBss.highlight)

/* commons: psylink lays them out after the .bss */
TitleRect mTitleSelection[3]; /* highlight: current, target, step (10.6 fixed point) */
u8 mFileBuffer[0x200];
u16 mTitleString[8];
s32 mNewMenu;
SyphonHeader mSyphonHeader;
EGSSprite mTitleScreenSprite[2];
EGSSprite *mTitleSprites;
u8 bkgd[0x28];
s16 mTitleModeStack[10][2];

static void func_80142D8C(void);
static void func_80142F84(void);
static void func_80142FEC(void);
static void func_80143084(void);
static void func_80143168(void);
static void func_801431E8(s32 item);
static void func_80143220(s32 item);
static void func_8014335C(s32 item);
static void func_801434B0(void);
static void func_801435A8(void);
static void func_801436B4(void);
static void func_80143718(void);
static void func_80143764(void);

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0};
#define EMPTY_STR ((char *)D_80140FE4) /* the tables' "" is the header's first word */

/* message per MemCardSync error */
static char *D_801445E8[8] = {
    EMPTY_STR,
    "\n\nMemory card not found",
    "\n\nMemory card seems to be bad.\nPlease replace and retry",
    "\n\nMemory card has been unexpectedly changed...\n\nPlease try again",
    "\n\nMemory card isn't formatted.\nFormat now?",
    "\n\nMemory card doesn't have a \nSyphon Filter slot on it yet.\nWould you like to create one?",
    "\n\nOverwrite existing save game?",
    "There is no room on memory card to save a game.\nPlease insert a different memory card and try again",
};
/* message per card operation */
static char *D_80144608[10] = {
    EMPTY_STR, "Searching memory card", EMPTY_STR, EMPTY_STR, "Reading save data",
    EMPTY_STR, EMPTY_STR, EMPTY_STR, EMPTY_STR, EMPTY_STR,
};
static const char D_801411C4[] = "\n\nFormat complete";
static const char D_801411D8[] = "\n\nSave complete";
static char *D_80144630[2] = {"\nNo\t", "Yes"};
static char *D_80144638[1] = {"\nPress %x to continue"};
static char *D_8014463C = "\n\nDo you wish to save your game?";
static char *D_80144640[2] = {"\nNo\t", "Yes"};
static char *D_80144648 = "\nCancel";
static char *D_8014464C = "Choose save slot to use\n";
static char *D_80144650 = "--- Empty Slot ---";
static char *D_80144654 = "This memory card is full.  Syphon Filter requires one block on the memory card.  "
                          "You will not be able to save your game to this memory card.  Continue anyway?";
static char *D_80144658 = "\n\nDo you wish to overwrite this slot?";
/* attract movies */
static MovieInfo D_8014465C[3] = {
    {"\\SOL\\989LOGO", 0x140, 0xF0},
    {"\\SOL\\EIDETIC", 0x140, 0xF0},
    {"\\SOL\\INTRO", 0x140, 0xF0, 0, 0x28, 1},
};
static s32 D_80144830 = 0; /* selected save slot */
u32 mMovieInterruptTime = 0;
u8 mTitleWindow = 0xFF;
static s16 D_8014483A = 0; /* card op queue: last taken */
static s16 D_8014483C = 0; /* card op queue: last added */
static s32 D_80144840 = CARD_UNKNOWN; /* memory card state */
static char *D_80144844 = NULL;       /* message box text */
/* message box menu */
static TitleMenuEntry D_80144848[2] = {{1, &D_80144844, &mTitleString[0]}, {0, NULL, &mTitleString[1]}};
static s32 D_80144860 = 0;            /* message box type */
static void (*D_80144864)(s32) = NULL; /* message box callback */
static s32 D_80144868 = 0;            /* card messages are suppressed */
/* save prompt menu */
static TitleMenuEntry D_8014486C[2] = {{1, &D_8014463C, &mTitleString[0]}, {2, D_80144640, &mTitleString[1]}};
/* slot menu */
static TitleMenuEntry D_80144884[7] = {{1, &D_8014464C, &mTitleString[0]}};
u32 mTitleMode = TITLE_MODE_INIT;
static s32 D_801448DC[6] = {2, 2, 2, 0, 0, 0}; /* menu lines per mode */
TitleMenuEntry *mTitleMenuTable[6] = {NULL, D_80144848, D_8014486C, NULL, D_80144884, NULL};
s32 mTitleModeStackDepth = 0;
s32 mCurItem = 0;
void (*mCardSuccessCallback)(void) = NULL;
void (*mCardFailedCallback)(void) = NULL;
static s32 D_8014491C = -1; /* pad buttons last frame */

/* memory card file header: "SC", icon frames, title, CLUT, icon */
const u8 mFileHeader[0x280] = {
    0x53, 0x43, 0x13, 0x01, 0x82, 0x72, 0x82, 0x99, 0x82, 0x90, 0x82, 0x88, 0x82, 0x8F, 0x82, 0x8E,
    0x81, 0x40, 0x82, 0x65, 0x82, 0x89, 0x82, 0x8C, 0x82, 0x94, 0x82, 0x85, 0x82, 0x92, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x63, 0xB5, 0x56, 0x73, 0x4E, 0x31, 0x46, 0xEF, 0x3D, 0xAD, 0x35, 0x6B, 0x2D, 0x29, 0x25,
    0x08, 0x21, 0xE7, 0x1C, 0xC6, 0x18, 0xA5, 0x14, 0x84, 0x10, 0x63, 0x0C, 0x21, 0x04, 0x00, 0x80,
    0xEC, 0xCC, 0x9A, 0xCA, 0xAB, 0xA9, 0xDC, 0xCE, 0xEE, 0x9C, 0x88, 0xA7, 0x8A, 0x69, 0xD9, 0xEE,
    0xDD, 0x79, 0x3B, 0x75, 0x57, 0x87, 0xA5, 0xDD, 0xBC, 0xA6, 0x16, 0x33, 0x33, 0xE3, 0x74, 0xDC,
    0x8B, 0xE5, 0x45, 0xED, 0x5D, 0xF2, 0x54, 0xBA, 0x69, 0xF4, 0x8D, 0x35, 0x95, 0xF9, 0x34, 0xA7,
    0x67, 0xFE, 0x7F, 0x00, 0x50, 0xFF, 0x4E, 0x95, 0xE7, 0x6A, 0xE7, 0x2E, 0xFB, 0x7B, 0xFA, 0x75,
    0x5B, 0xA0, 0x13, 0x05, 0x35, 0xA2, 0x40, 0x79, 0x29, 0xD2, 0x07, 0x91, 0x03, 0xE5, 0x21, 0x77,
    0x67, 0x74, 0x1F, 0xF1, 0x02, 0x6F, 0x62, 0x87, 0x9A, 0x37, 0xCB, 0xF4, 0xD4, 0x1A, 0x85, 0xBA,
    0xCC, 0x58, 0x41, 0xFB, 0x4B, 0x30, 0xA7, 0xDD, 0xCD, 0x59, 0x74, 0xFF, 0x6F, 0x42, 0xB8, 0xDE,
    0xCE, 0xAA, 0xCD, 0x15, 0xD7, 0x8B, 0xC7, 0xEE, 0xCC, 0x48, 0x12, 0x11, 0x11, 0x41, 0xB7, 0xCD,
    0xED, 0xDE, 0xBC, 0xEC, 0xCE, 0xCB, 0xED, 0xDE, 0xFE, 0xCE, 0x9A, 0xEB, 0xCE, 0x8B, 0xEC, 0xEF,
    0xEE, 0x9D, 0x5C, 0xC9, 0x9C, 0x99, 0xD8, 0xEF, 0xEE, 0xCA, 0x37, 0x66, 0x66, 0xE4, 0xB5, 0xEE,
    0xDD, 0xE8, 0x56, 0xED, 0x7E, 0xF3, 0x95, 0xEE, 0xAD, 0xF6, 0x8C, 0x46, 0x96, 0xF9, 0x55, 0xDD,
    0x8B, 0xFE, 0x7F, 0x11, 0x52, 0xFF, 0x6E, 0xC8, 0xE9, 0x7A, 0xE7, 0x4E, 0xFC, 0x7B, 0xFA, 0xA7,
    0x6C, 0xB1, 0x24, 0x17, 0x47, 0xB3, 0x51, 0x8A, 0x4A, 0xD5, 0x18, 0xA4, 0x14, 0xE7, 0x53, 0x89,
    0x99, 0x88, 0x2F, 0xF4, 0x33, 0x7E, 0xA5, 0xB9, 0xDC, 0x6C, 0xDC, 0xF5, 0xD5, 0x3B, 0xD9, 0xDE,
    0xEE, 0x9D, 0x54, 0xFB, 0x4B, 0x62, 0xEC, 0xEE, 0xEE, 0x9D, 0x95, 0xFF, 0x7F, 0x74, 0xEC, 0xEF,
    0xEE, 0xBC, 0xCD, 0x26, 0xD7, 0xAC, 0xEA, 0xEF, 0xDC, 0x6A, 0x24, 0x43, 0x23, 0x53, 0xD9, 0xDE,
    0xED, 0xEE, 0xCE, 0xEE, 0xEE, 0xEC, 0xEE, 0xDE, 0xFE, 0xEF, 0xCC, 0xFE, 0xEF, 0xBD, 0xFE, 0xEF,
    0xFE, 0xDF, 0x8D, 0xFE, 0xEF, 0xBC, 0xFC, 0xEF, 0xFE, 0xDE, 0x89, 0xBC, 0xDB, 0xE8, 0xE9, 0xEF,
    0xFE, 0xEE, 0x88, 0xEE, 0xAE, 0xF7, 0xE8, 0xEF, 0xFE, 0xFB, 0xAD, 0x78, 0xB8, 0xFB, 0xC7, 0xEF,
    0xCE, 0xFE, 0x9F, 0x74, 0x87, 0xFF, 0x9E, 0xEE, 0xFC, 0x8B, 0xE9, 0x9E, 0xFD, 0x8C, 0xFB, 0xEA,
    0x8D, 0xC5, 0x56, 0x5A, 0x7A, 0xC5, 0x84, 0xBC, 0x7C, 0xEB, 0x7A, 0xD9, 0x78, 0xEA, 0xA8, 0xBB,
    0xDB, 0xBE, 0x6F, 0xF9, 0x86, 0x9F, 0xEB, 0xCD, 0xFE, 0xBF, 0xED, 0xF7, 0xE7, 0x7C, 0xFE, 0xEF,
    0xFE, 0xEF, 0x79, 0xFC, 0x7C, 0xC5, 0xFF, 0xEF, 0xFE, 0xEE, 0xBB, 0xFF, 0x8F, 0xD9, 0xFE, 0xEF,
    0xFE, 0xDE, 0xDE, 0x47, 0xE8, 0xCD, 0xED, 0xEF, 0xED, 0x8C, 0x56, 0x86, 0x57, 0x85, 0xEB, 0xDE,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* per mode: confirm pressed */
static void (*const D_801415B0[6])(void) = {
    func_80142D8C, func_80142F84, func_80142FEC, func_80143084, func_80143168, NULL,
};
/* per mode: select item */
static void (*const D_801415C8[6])(s32) = {
    func_801431E8, func_80143220, func_80143220, NULL, func_8014335C, NULL,
};
/* per mode: menu built */
static void (*const D_801415E0[6])(void) = {
    func_801434B0, func_801435A8, func_801435A8, func_801436B4, func_80143718, NULL,
};
/* per mode: every frame */
static void (*const D_801415F8[6])(void) = {func_80143764, NULL, NULL, NULL, NULL, NULL};

/* Bit test tolerant of an unaligned base. */
static inline u32 BitTest(void *base, s32 bit) {
    u32 *words = base;
    s32 misalign = (u32)words & 3;

    if (misalign) {
        bit = misalign * 8 + bit;
        words = (u32 *)((u32)words & ~3);
    }
    return words[bit >> 5] & (1 << (bit & 0x1F));
}

#define TITLE_ASSERT(cond, line) \
    if (!(cond)) EGSDebugAssert(1, 0, D_80141680, line)

static void func_80142098(u32 mode);
static void func_8014216C(void);
static void func_801423B0(s32 op);
static s32 func_80142434(void);
static void func_801424AC(void);
static void func_801424D8(char *msg, u32 type, s32 arg2, void (*cb)(s32));
static s32 func_80142888(s32 result);
static void func_80142A54(s32 op);
static void func_80142D5C(s32 yes);
static void func_80143084(void);
static void func_80143EF8(void);
static void func_801444E4(void);
static void func_80144510(void);
static void func_80144544(void);

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define SLIDE(f)                                                                     \
    if (ABS(mTitleSelection[2].f) > ABS(mTitleSelection[0].f - mTitleSelection[1].f)) { \
        mTitleSelection[2].f = 0;                                                    \
        mTitleSelection[0].f = mTitleSelection[1].f;                                 \
    } else {                                                                         \
        mTitleSelection[0].f += mTitleSelection[2].f;                                \
    }

#ifdef NON_MATCHING
/* Memory card outline: 14 pairs of outer/inner points (x | y << 16). */
static const s32 D_80141610[14][2] = {
    { 0xFFD1007B, 0xFFD60072 }, { 0xFFD1FF85, 0xFFD6FF8A }, { 0xFFDAFF7C, 0xFFDCFF84 },
    { 0x0026FF7C, 0x001CFF84 }, { 0x0030FF86, 0x0026FF8E }, { 0x0035FF8B, 0x0026FF98 },
    { 0x0035FFF0, 0x0026FFEB }, { 0x002BFFFA, 0x0026FFF6 }, { 0x002B0078, 0x00260065 },
    { 0x001F0084, 0x001F006C }, { 0x00050084, 0x0004006C }, { 0xFFFD0084, 0xFFF80078 },
    { 0xFFDA0084, 0xFFDC0078 }, { 0xFFD1007B, 0xFFD60072 },
};

/* regalloc and giv choice in the add loop */
/*
 * Builds (frame 0) and shows/hides the memory card drawing a piece at a time;
 * frame counts up to 12.
 */
u8 AnimateMemCardScreen(u8 on, s32 frame) {
    s32 i;
    s32 x;
    s32 nPolys;
    s32 nLines;
    TitleLineF2 *line;

    if (frame <= 0 && on) {
        const s32 (*pt)[2] = D_80141610;
        TitlePolyF4 *poly = D_80144948;
        TitleLineF2 *edge = D_80144B1C[0];
        u32 n;

        line = D_80144D8C;
        for (n = 0; n < ARRAY_COUNT(D_80144948); n++, pt++) {
            s32 nextOuter, nextInner;

            nextOuter = pt[1][0];
            nextInner = pt[1][1];
            EGSDisplayBuildPoly4F(poly, 0x5A281E, pt[0][0], pt[0][1], nextOuter, nextInner);
            poly->type = 2;
            poly++;
            EGSDisplayBuildLineF(edge, 0xB4645A, pt[0][0], nextOuter);
            edge->type = 1;
            edge++;
            EGSDisplayBuildLineF(edge, 0xB4645A, pt[0][1], nextInner);
            edge->type = 1;
            edge++;
        }
        for (i = 0, x = -118; i < 20; i++, x += 12) {
            EGSDisplayBuildLineF(line, 0x461E28, XY(x, -42), XY(x, 38));
            line->type = 7;
            line++;
        }
        for (i = 0, x = -37; i < 8; i++, x += 10) {
            EGSDisplayBuildLineF(line, 0x461E28, XY(-124, x), XY(120, x));
            line->type = 7;
            line++;
        }
        EGSDisplayBuildLineF(line++, 0xB4645A, XY(-11, 48), XY(128, 48));
        EGSDisplayBuildLineF(line, 0xB4645A, XY(128, 35), XY(128, 48));
    }
    if (frame < 0) {
        return on;
    }
    if (on) {
        nPolys = ARRAY_COUNT(D_80144948) * frame / 12;
        nLines = ARRAY_COUNT(D_80144D8C) * frame / 12;
        for (i = 0; i < nPolys; i++) {
            if (D_80144948[i].next == NULL) {
                EGSDisplayAddPoly(gTextCamera, &D_80144948[i]);
            } else {
                D_80144948[i].rgbc = 0x285A281E;
            }
            if (D_80144B1C[i][0].next == NULL) {
                EGSDisplayAddPoly(gTextCamera, &D_80144B1C[i][0]);
                EGSDisplayAddPoly(gTextCamera, &D_80144B1C[i][1]);
            }
        }
        for (i = 0; i < nLines; i++) {
            if (D_80144D8C[i].next == NULL) {
                EGSDisplayAddPoly(gTextCamera, &D_80144D8C[i]);
            } else {
                D_80144D8C[i].rgbc = 0x40461E28;
            }
        }
    } else {
        nPolys = ARRAY_COUNT(D_80144948) - ARRAY_COUNT(D_80144948) * frame / 12;
        nLines = ARRAY_COUNT(D_80144D8C) - ARRAY_COUNT(D_80144D8C) * frame / 12;
        for (i = 0; i < nPolys; i++) {
            TitlePolyF4 *poly = &D_80144948[i];

            if (poly->next != NULL) {
                EGSDisplayDeletePoly(gTextCamera, poly);
                EGSDisplayDeletePoly(gTextCamera, &D_80144B1C[i][0]);
                EGSDisplayDeletePoly(gTextCamera, &D_80144B1C[i][1]);
            }
        }
        for (i = 0; i < nLines; i++) {
            line = &D_80144D8C[i];
            if (line->next != NULL) {
                EGSDisplayDeletePoly(gTextCamera, line);
            }
        }
    }
    if (nPolys < ARRAY_COUNT(D_80144948)) {
        TitlePolyF4 *poly;

        D_80144948[nPolys].rgbc = 0x28FFB478;
        poly = &D_80144948[nPolys];
        if (poly->next == NULL) {
            EGSDisplayAddPoly(gTextCamera, poly);
        }
    }
    if (nLines < ARRAY_COUNT(D_80144D8C)) {
        TitleLineF2 *last = &D_80144D8C[nLines];

        last->rgbc = 0x40F0B478;
        if (last->next == NULL) {
            EGSDisplayAddPoly(gTextCamera, last);
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/title/nonmatchings/title", AnimateMemCardScreen);
#endif

static const char D_80141680[] = "bin/title.c";
static const char D_8014168C[] = "BASCUS-94240SYPHON";

/* Starts sliding the selection highlight to a new rect. */
static void func_80141CE0(s32 x, s32 y, s32 w, s32 h) {
    RECT_XY(mTitleSelection[1]) = ((x << 6) & 0xFFFF) | (y << 22);
    RECT_WH(mTitleSelection[1]) = ((w << 6) & 0xFFFF) | (h << 22);
    mTitleSelection[2].x = (mTitleSelection[1].x - mTitleSelection[0].x) / 8;
    mTitleSelection[2].y = (mTitleSelection[1].y - mTitleSelection[0].y) / 8;
    mTitleSelection[2].w = (mTitleSelection[1].w - mTitleSelection[0].w) / 8;
    mTitleSelection[2].h = (mTitleSelection[1].h - mTitleSelection[0].h) / 8;
}

/* Moves the selection highlight one step. */
static void func_80141DC8(void) {
    s32 x, y, x2, y2;

    SLIDE(x);
    SLIDE(y);
    SLIDE(w);
    SLIDE(h);
    x = mTitleSelection[0].x >> 6;
    y = mTitleSelection[0].y >> 6;
    x2 = x + (mTitleSelection[0].w >> 6);
    y2 = y + (mTitleSelection[0].h >> 6);
    D_8014505C.xy0 = XY(x, y);
    D_8014505C.xy1 = XY(x2, y);
    D_8014505C.xy2 = XY(x, y2);
    D_8014505C.xy3 = XY(x2, y2);
}

/* Pushes the current mode and switches to a new one. */
static void func_80142098(u32 mode) {
    TITLE_ASSERT(mTitleModeStackDepth < 10, 0x360);
    TITLE_ASSERT(mode < 6, 0x361);
    if (mTitleMode != TITLE_MODE_ATTRACT) {
        mTitleModeStack[mTitleModeStackDepth][1] = mTitleMode;
        mTitleModeStack[mTitleModeStackDepth][0] = mCurItem;
        mTitleModeStackDepth++;
    }
    mTitleMode = mode;
    mNewMenu = 1;
    mCurItem = 0;
}

/* Back to the previous mode. */
static void func_8014216C(void) {
    if (mTitleModeStackDepth >= 2) {
        if (mTitleMode != TITLE_MODE_MESSAGE && mTitleMode != TITLE_MODE_SLOT) {
            func_801424AC();
        }
        mNewMenu = 1;
        mTitleModeStackDepth--;
        mCurItem = mTitleModeStack[mTitleModeStackDepth][0];
        mTitleMode = mTitleModeStack[mTitleModeStackDepth][1];
    }
}

static void func_80142208(void) {
    s32 i;

    mTitleWindow = TextAddWindow(-124, -38, 232, 79);
    EGSDisplayFadeFull();
    EGSDisplayFade(-1, 0x33, NULL);
    mTitleModeStackDepth = 0;
    MemCardStart();
    D_80144840 = CARD_UNKNOWN;
    func_801423B0(CARD_OP_ACCEPT);
    mTitleSprites = EGSDisplayPacketAreaMalloc(0x2418);
    TextSpriteInitialize(mTitleSprites, 0xD2);
    EGSDisplayBuildPoly4F(&D_8014505C, 0xB46450, 0, 0, 0, 0);
    D_8014505C.type = 1;
    ((u8 *)&D_8014505C.rgbc)[3] |= 2;
    EGSDisplayAddPoly(gTextCamera, &D_8014505C);
    for (i = 0; i < 3; i++) {
        RECT_XY(mTitleSelection[i]) = RECT_WH(mTitleSelection[i]) = 0;
    }
}

static void func_80142324(void) {
    MemCardStop();
    D_80144840 = CARD_UNKNOWN;
    EGSDisplayDeletePoly(gTextCamera, &D_8014505C);
    InterfaceSetMode(0);
    TextRemoveWindowNow(mTitleWindow);
    EGSDisplayPacketAreaFree();
    if (*(void **)bkgd != NULL) {
        EGSDisplayDeletePoly(gInterfaceWindow, bkgd);
    }
}

/* Queues a card operation. */
static void func_801423B0(s32 op) {
    if (++D_8014483C >= 10) {
        D_8014483C = 0;
    }
    D_80144920[D_8014483C] = op;
    TITLE_ASSERT(D_8014483C != D_8014483A, 0x3DD);
}

/* Takes the next queued card operation. */
static s32 func_80142434(void) {
    if (D_8014483A == D_8014483C) {
        return CARD_OP_NONE;
    }
    if (++D_8014483A >= 10) {
        D_8014483A = 0;
    }
    return D_80144920[D_8014483A];
}

static void func_801424AC(void) {
    D_8014483A = -1;
    D_8014483C = -1;
    mCardSuccessCallback = NULL;
    mCardFailedCallback = NULL;
}

/* Shows a message box; type 1: "Press X", 2: No/Yes, 4: no choices. */
static void func_801424D8(char *msg, u32 type, s32 arg2, void (*cb)(s32)) {
    if (D_80144868 == 0) {
        if (mTitleMode != TITLE_MODE_MESSAGE) {
            func_80142098(TITLE_MODE_MESSAGE);
        } else {
            if (msg == D_80144844) {
                return;
            }
            mNewMenu = 1;
        }
        D_80144864 = cb;
        D_80144844 = msg;
        D_80144860 = type;
        switch (type) {
        case 0:
            EGSDebugAssert(1, 0, D_80141680, 0x417);
            break;
        case 1:
            D_80144848[1].count = type;
            D_80144848[1].strings = D_80144638;
            break;
        case 4:
            D_80144848[1].count = 0;
            D_80144848[1].strings = NULL;
            break;
        case 2:
            D_80144848[1].count = type;
            D_80144848[1].strings = D_80144630;
            break;
        }
    }
}

/* "Format now?" answered. */
static void func_80142600(s32 yes) {
    if (yes) {
        MemCardSync(0, NULL, NULL);
        if (func_80142888(MemCardFormat(0))) {
            D_80144840 = 5;
            func_80142A54(D_8014483A < D_8014483C ? D_80144920[D_8014483A] : CARD_OP_NONE);
        }
    }
}

/* "Create a slot?" answered. */
static void func_80142688(s32 yes) {
    s32 i;

    if (yes) {
        MemCardSync(0, NULL, NULL);
        if (func_80142888(MemCardCreateFile(0, D_8014168C, 1))) {
            D_80144840 = CARD_OK;
            MemCardWriteFile(0, D_8014168C, mFileHeader, 0, 0x280);
            for (i = 0; i < SAVE_SLOT_COUNT; i++) {
                mSyphonHeader.level[i] = 0;
                mSyphonHeader.unk14[i] = 0;
            }
            mSyphonHeader.lastSlot = 0;
            mSyphonHeader.lastSlot2 = 0;
            func_80143EF8();
            MemCardSync(0, NULL, NULL);
            func_80142A54(D_8014483A < D_8014483C ? D_80144920[D_8014483A] : CARD_OP_NONE);
        }
    } else {
        func_801424AC();
    }
}

/* "Overwrite this slot?" answered. */
static void func_801427B0(s32 yes) {
    void *buf;
    s32 size;

    if (yes) {
        SavePrepareMemoryCardSave();
        SaveGetDataBuffer(&buf, &size);
        MemCardWriteFile(0, D_8014168C, buf, size * D_80144830 + 0x280, size);
        mSyphonHeader.level[D_80144830] = gSysInfo.level;
        mCardSuccessCallback = func_80144510;
        mSyphonHeader.unk14[D_80144830] = gSysInfo.level;
        func_801423B0(CARD_OP_WRITE_HEADER);
    } else {
        while (mTitleModeStackDepth >= 2) {
            func_8014216C();
        }
    }
}

/* Handles a MemCardSync result; returns nonzero on success. */
static s32 func_80142888(s32 result) {
    void (*cb)(void);

    if (result == 0) {
        if (D_8014483A >= D_8014483C) {
            mCardFailedCallback = NULL;
            cb = mCardSuccessCallback;
            if (cb != NULL) {
                mCardSuccessCallback = NULL;
                cb();
            }
        }
        return 1;
    }
    switch (result) {
    case 3:
        D_80144840 = CARD_UNKNOWN;
        break;
    case 1:
        D_80144840 = 1;
        break;
    case 7:
        D_80144840 = 6;
        break;
    case 5:
        D_80144840 = 5;
        break;
    case 4:
        D_80144840 = 4;
        break;
    default:
        D_80144840 = 2;
        break;
    case 6:
        break;
    }
    cb = mCardFailedCallback;
    if (cb != NULL) {
        mCardFailedCallback = NULL;
        cb();
        return 0;
    }
    switch (result) {
    case 3:
        func_80142A54(CARD_OP_ACCEPT);
        break;
    case 1:
    case 2:
    case 6:
    case 7:
        func_801424D8(D_801445E8[result], 1, 0, NULL);
        func_801424AC();
        break;
    case 5:
        func_801424D8(D_801445E8[result], 2, 0, func_80142688);
        break;
    case 4:
        func_801424D8(D_801445E8[result], 2, 0, func_80142600);
        break;
    default:
        EGSDebugAssert(1, 0, D_80141680, 0x50B);
        break;
    }
    return 0;
}


/* Starts a card operation and shows its message. */
static void func_80142A54(s32 op) {
    void *buf;
    s32 size;

    if (op == CARD_OP_NONE) {
        return;
    }
    switch (op) {
    case CARD_OP_EXIST:
        MemCardExist(0);
        break;
    case CARD_OP_ACCEPT:
        MemCardAccept(0);
        break;
    case CARD_OP_READ_HEADER:
        MemCardReadFile(0, D_8014168C, &mSyphonHeader, 0x200, sizeof(SyphonHeader));
        break;
    case CARD_OP_WRITE_HEADER:
        mSyphonHeader.lastSlot = mSyphonHeader.lastSlot2 = D_80144830;
        MemCardWriteFile(0, D_8014168C, &mSyphonHeader, 0x200, sizeof(SyphonHeader));
        break;
    case CARD_OP_READ_SAVE:
        if (mSyphonHeader.level[D_80144830] != 0) {
            SaveGetDataBuffer(&buf, &size);
            MemCardReadFile(0, D_8014168C, buf, size * D_80144830 + 0x280, size);
        }
        break;
    case CARD_OP_SAVE:
        if (mSyphonHeader.level[D_80144830] > 0) {
            func_801424D8(D_80144658, 2, 0, func_801427B0);
        } else {
            func_801427B0(1);
        }
        break;
    case CARD_OP_SLOTS:
        func_80142098(TITLE_MODE_SLOTS);
        break;
    }
    if (*D_80144608[op] != '\0') {
        func_801424D8(D_80144608[op], 4, 0, NULL);
    }
}


static inline void SpriteSetRGB(EGSSprite *sprite, u8 r, u8 g, u8 b) {
    sprite->color.r = r;
    sprite->color.g = g;
    sprite->color.b = b;
}

static inline void SpriteGetRGB(TitleRGB *c, EGSSprite *sprite) {
    c->r = sprite->color.r;
    c->g = sprite->color.g;
    c->b = sprite->color.b;
}

static inline void SpritePutRGB(EGSSprite *sprite, TitleRGB *c) {
    sprite->color.r = c->r;
    sprite->color.g = c->g;
    sprite->color.b = c->b;
}

/* Black, the starting colour of the menu sprites. */
static const TitleColor D_80141704 = { 0, 0, 0, 0 };

/* Loads the "New game" / "Load game" sprites. */
static void func_80142C30(void) {
    EGSTexture tex;
    TitleColor black = D_80141704;
    char *files[2] = { "\\COMMON\\NEWB.TIM;1", "\\COMMON\\LOADB.TIM;1" };
    void *addr;
    s32 i;

    addr = mTitleSprites;
    tex.data = mTitleSprites;
    for (i = 0; i < 2; i++) {
        SysLoadData(files[i], &addr, 0x4000);
        EGSTextureCreate(&tex);
        EGSTextureLoad(&tex);
        if (i == 0) {
            EGSTextureCreateSprite(&tex, &mTitleScreenSprite[i], 60, 165, 0, 0);
        } else {
            EGSTextureCreateSprite(&tex, &mTitleScreenSprite[i], 60, 196, 0, 0);
        }
        SpriteSetRGB(&mTitleScreenSprite[i], black.r, black.g, black.b);
        EGSTextureSetSpriteTransparency(&mTitleScreenSprite[i], 1);
    }
    TextSpriteInitialize(mTitleSprites, 0xD2);
}

/* "This memory card is full" answered. */
static void func_80142D5C(s32 yes) {
    if (yes) {
        func_80142324();
        mTitleMode = TITLE_MODE_NEW_GAME;
    }
}

#ifdef NON_MATCHING
/* the original hoists the three black.r/g/b byte loads above the in-loop copy of D_80141704 (loop invariant motion) */
/* Main menu: X pressed. */
static void func_80142D8C(void) {
    s32 i;

    MovieSetLoopFlag(0);
    SysStopMovie();
    EGSDisplayTextOverMovies(0);
    InterfaceSetMode(7);
    for (i = 0; i < 2; i++) {
        TitleColor black = D_80141704;

        SpriteSetRGB(&mTitleScreenSprite[i], black.r, black.g, black.b);
    }
    switch (mCurItem) {
    case 0: {
        DIRENTRY dir[15];
        s32 nFiles;
        s32 j;
        s32 blocks;

        while (D_8014483A < D_8014483C) {
            MemCardSync(0, NULL, NULL);
            func_80142A54(func_80142434());
        }
        MemCardSync(0, NULL, NULL);
        if (D_80144840 == 5 && func_80142888(MemCardGetDirentry(0, "*", dir, &nFiles, 0, 15))) {
            blocks = 0;
            for (j = 0; j < nFiles; j++) {
                blocks += (dir[j].size + 0x1FFF) / 0x2000;
            }
            if (blocks >= 15) {
                D_80144868 = 0;
                func_801424D8(D_80144654, 2, 1, func_80142D5C);
                break;
            }
        }
        func_80142D5C(1);
        break;
    }
    case 1:
        func_801423B0(CARD_OP_SLOTS);
        func_801423B0(CARD_OP_WRITE_HEADER);
        func_801423B0(CARD_OP_READ_SAVE);
        mCardSuccessCallback = func_80144544;
        break;
    }
}
#else
INCLUDE_ASM("asm/title/nonmatchings/title", func_80142D8C);
#endif

/* Message box: X pressed. */
static void func_80142F84(void) {
    s32 item;
    void (*cb)(s32);

    if (D_80144860 != 4) {
        item = mCurItem;
        func_8014216C();
        cb = D_80144864;
        D_80144860 = 0;
        if (cb != NULL) {
            D_80144864 = NULL;
            cb(item);
        }
    }
}

/* Save prompt: X pressed. */
static void func_80142FEC(void) {
    D_80144868 = 0;
    switch (mCurItem) {
    case 0:
    default:
        func_80142324();
        mTitleMode = TITLE_MODE_DONE;
        break;
    case 1:
        if (D_80144840 != 1) {
            if (D_80144840 != CARD_OK) {
                func_801423B0(CARD_OP_WRITE_HEADER);
            }
            func_801423B0(CARD_OP_SLOTS);
            func_801423B0(CARD_OP_SAVE);
        } else {
            func_80142888(1);
        }
        break;
    }
}

/* Attract mode: next movie, back to the menu after the last. */
static void func_80143084(void) {
    if (gSystemTick >= mMovieInterruptTime && (mCurItem != 0 || !gMovieOpenFlag)) {
        SysStopMovie();
        if (++mCurItem < ARRAY_COUNT(D_8014465C)) {
            UnpauseTerrainStream();
            SysPlayMovie(&D_8014465C[mCurItem]);
            mMovieInterruptTime = gSystemTick + 20;
        } else {
            func_80142098(TITLE_MODE_MAIN);
        }
    }
}

/* Slot menu: X pressed. */
static void func_80143168(void) {
    if (mCurItem < D_801448DC[TITLE_MODE_SLOTS] - 1) {
        D_80144830 = mCurItem - 1;
        TITLE_ASSERT((u32)D_80144830 < SAVE_SLOT_COUNT, 0x67F);
        func_80142098(TITLE_MODE_SLOT);
    } else {
        func_801424AC();
        func_8014216C();
    }
}

static void func_801431E8(s32 item) {
    if (item >= D_801448DC[TITLE_MODE_MAIN]) {
        item = 0;
    }
    if (item < 0) {
        item = D_801448DC[TITLE_MODE_MAIN] - 1;
    }
    mCurItem = item;
}

static void func_80143220(s32 item) {
    TitleMenuEntry *menu;
    s32 count;
    u16 *ids;
    TitleRect *rect;

    if (mTitleMode != TITLE_MODE_MESSAGE || D_80144860 == 2) {
        menu = mTitleMenuTable[mTitleMode];
        count = menu[1].count;
        ids = menu[1].textIds;
        if (item >= count) {
            item = 0;
        }
        if (item < 0) {
            item = count - 1;
        }
        TextSetStringIntensity(ids[mCurItem], 0x70, 0x70, 0x90);
        mCurItem = item;
        TextSetStringIntensity(ids[item], 0xA0, 0xC0, 0xF0);
        rect = TextStringRect(ids[mCurItem], 0);
        rect->x -= 3;
        rect->y -= 2;
        rect->w += 6;
        rect->h += 4;
        func_80141CE0(rect->x, rect->y, rect->w, rect->h);
    }
}

static void func_8014335C(s32 item) {
    TitleMenuEntry *entry;
    TitleRect *rect;

    if (item <= 0) {
        item = D_801448DC[TITLE_MODE_SLOTS] - 1;
    } else if (item >= D_801448DC[TITLE_MODE_SLOTS]) {
        item = 1;
    }
    entry = &D_80144884[0];
    TextSetStringIntensity(*entry->textIds, 0xA0, 0xC0, 0xF0);
    entry = &D_80144884[mCurItem];
    TextSetStringIntensity(*entry->textIds, 0x70, 0x70, 0x90);
    entry = &mTitleMenuTable[mTitleMode][item];
    mCurItem = item;
    TextSetStringIntensity(*entry->textIds, 0xA0, 0xC0, 0xF0);
    rect = TextStringRect(*entry->textIds, 0);
    rect->x -= 3;
    rect->y -= 2;
    rect->w += 6;
    rect->h += 4;
    func_80141CE0(rect->x, rect->y, rect->w, rect->h);
}

/* Main menu shown: title movie behind it. */
static void func_801434B0(void) {
    MovieInfo movie = { "\\SOL\\TITLE", 320, 240 };

    InterfaceSetMode(0);
    InterfaceForceOff();
    InterfaceAnimate();
    RECT_XY(mTitleSelection[0]) = RECT_WH(mTitleSelection[0]) = 0;
    mTitleSelection[0].x = 1;
    {
        EGSSprite *sprites = mTitleScreenSprite;

        if (sprites[0].unk28 == 0) {
            EGSDisplayAddSprite(gTextCamera, &sprites[0]);
            EGSDisplayAddSprite(gTextCamera, &sprites[1]);
        }
    }
    SysPlayMovie(&movie);
    MovieSetLoopFlag(1);
    EGSDisplayTextOverMovies(1);
}

/* Message box / save prompt shown. */
static void func_801435A8(void) {
    TitleMenuEntry *menu;
    s32 i;

    menu = mTitleMenuTable[mTitleMode];
    TextSetStringIntensity(*menu[0].textIds, 0xA0, 0xC0, 0xF0);
    menu++;
    if (D_801448DC[mTitleMode] >= 2 && menu->count >= 2) {
        for (i = 0; i < menu->count; i++) {
            if (i == mCurItem) {
                TextSetStringIntensity(menu->textIds[i], 0xA0, 0xC0, 0xF0);
            } else {
                TextSetStringIntensity(menu->textIds[i], 0x70, 0x70, 0x90);
            }
        }
    }
}

/* Attract mode started. */
static void func_801436B4(void) {
    if (mCurItem == 0) {
        UnpauseTerrainStream();
        SysPlayMovie(&D_8014465C[0]);
        mMovieInterruptTime = gSystemTick + 20;
    } else {
        func_80143084();
    }
}

/* Slot menu shown: start on the last used slot. */
static void func_80143718(void) {
    if (D_80144840 == CARD_OK) {
        mCurItem = mSyphonHeader.lastSlot + 1;
    } else {
        func_8014216C();
    }
}

#ifdef NON_MATCHING
/* regalloc: sprite pointer and colour value swap a0/a1 */
/* Fades the "New game" / "Load game" sprites. */
static void func_80143764(void) {
    TitleRGB c;
    s32 i;

    if (mTitleMode == TITLE_MODE_MAIN) {
        if (D_80144840 == CARD_OK) {
            if (D_801448DC[TITLE_MODE_MAIN] != 2) {
                D_801448DC[TITLE_MODE_MAIN] = 2;
            }
        } else if (D_801448DC[TITLE_MODE_MAIN] == 2) {
            D_801448DC[TITLE_MODE_MAIN] = 1;
            if (mCurItem > 0) {
                mCurItem = 0;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        SpriteGetRGB(&c, &mTitleScreenSprite[i]);
        if (i >= D_801448DC[TITLE_MODE_MAIN] || D_80144840 == CARD_UNKNOWN || mTitleMode != TITLE_MODE_MAIN) {
            if (c.r == 0) {
                continue;
            }
            if (c.r > 10) {
                c.r -= 10;
            } else {
                c.r = 0;
            }
        } else if (i == mCurItem) {
            if (c.r == 200) {
                continue;
            }
            if (c.r < 190) {
                c.r += 10;
            } else {
                c.r = 200;
            }
        } else {
            if (c.r == 70) {
                continue;
            }
            if (c.r > 80) {
                c.r -= 10;
            } else if (c.r < 60) {
                c.r += 10;
            } else {
                c.r = 70;
            }
        }
        c.g = c.b = c.r;
        SpritePutRGB(&mTitleScreenSprite[i], &c);
    }
}
#else
INCLUDE_ASM("asm/title/nonmatchings/title", func_80143764);
#endif

/* Builds the text of the current menu. */
static void func_80143920(void) {
    TitleMenuEntry *menu;
    s32 i;
    s32 sprites;

    sprites = 0;
    menu = mTitleMenuTable[mTitleMode];
    func_80141CE0(0, 0, 0, 0);
    for (i = 7; i >= 0; i--) {
        mTitleString[i] = 0xFFFF;
    }
    if (mTitleWindow != 0xFF) {
        TextRemoveWindowNow(mTitleWindow);
        if (menu != NULL) {
            TextSetWindowJustification(mTitleWindow, mTitleMode == TITLE_MODE_MAIN);
            for (i = 0; i < D_801448DC[mTitleMode]; i++, menu++) {
                if (menu->count != 0) {
                    if (menu->count == 1) {
                        *menu->textIds = TextAddString(mTitleWindow, *menu->strings, -1, &mTitleSprites[sprites]);
                    } else {
                        TextAddStringMulti(mTitleWindow, menu->count, menu->strings, -1, &mTitleSprites[sprites],
                                           menu->textIds);
                    }
                    TITLE_ASSERT(*menu->textIds != 0xFFFF, 0x7F8);
                    TextSetStringIntensity(*menu->textIds, 0x70, 0x70, 0x90);
                    sprites += TextStringLength(*menu->textIds, 1);
                }
            }
        }
    }
    mNewMenu = 0;
    if (D_801415E0[mTitleMode] != NULL) {
        D_801415E0[mTitleMode]();
    }
    if (D_801415C8[mTitleMode] != NULL) {
        D_801415C8[mTitleMode](mCurItem);
    }
}

#ifdef NON_MATCHING
/* cse does not carry &pressed/misalign across the first conditional call */
/* Reads the pad and runs the current mode's handlers. */
static void func_80143BA8(void) {
    u8 *pad;
    u32 pressed;
    u16 buttons;
    s32 res;

    EGSDeviceControllerRead(&pad, 0);
    buttons = *(u16 *)(pad + 4);
    pressed = buttons & ~D_8014491C;
    D_8014491C = buttons;
    if (pad[0] != 0xFF && pressed != 0) {
        TITLE_ASSERT(D_801415B0[mTitleMode] != NULL, 0x821);
        res = BitTest(&pressed, 6);
        if (res) {
            D_801415B0[mTitleMode]();
        }
        if (D_801415C8[mTitleMode] != NULL) {
            res = BitTest(&pressed, 14);
            if (!res) {
                res = BitTest(&pressed, 13);
                if (!res || mCurItem >= D_801448DC[mTitleMode] - 1) {
                    goto up;
                }
            }
            D_801415C8[mTitleMode](mCurItem + 1);
        up:
            res = BitTest(&pressed, 12);
            if (!res) {
                res = BitTest(&pressed, 15);
                if (!res || mCurItem <= 0) {
                    goto back;
                }
            }
            D_801415C8[mTitleMode](mCurItem - 1);
        }
    back:
        if (mTitleMode != TITLE_MODE_MESSAGE) {
            res = BitTest(&pressed, 4);
            if (res) {
                func_8014216C();
            }
        }
    }
}
#else
INCLUDE_ASM("asm/title/nonmatchings/title", func_80143BA8);
#endif

/* Fills in the slot menu from the save header. */
static void func_80143EF8(void) {
    s32 i;

    if (D_80144840 == CARD_UNKNOWN) {
        D_80144840 = CARD_OK;
    }
    for (i = 0; i < SAVE_SLOT_COUNT;) {
        i++;
        D_80144884[i].count = 1;
        D_80144884[i].textIds = &mTitleString[i];
        if (mSyphonHeader.level[i - 1] > 0) {
            D_80144884[i].strings = &gLevelPrettyNames[mSyphonHeader.level[i - 1]];
        } else {
            D_80144884[i].strings = &D_80144650;
        }
    }
    i++;
    D_80144884[i].count = 1;
    D_80144884[i].strings = &D_80144648;
    D_80144884[i].textIds = &mTitleString[i];
    D_801448DC[TITLE_MODE_SLOTS] = i + 1;
}

void TitleExecute(void) {
    s32 type;
    s32 result;
    s32 i;

    InterfaceAnimate();
    if (mTitleMode >= TITLE_MODE_NEW_GAME) {
        if (gInterfaceCurrentMode == 0) {
            for (i = 0; i < 2; i++) {
                if (&mTitleScreenSprite[i].unk28 != NULL) {
                    EGSDisplayDeleteSprite(gTextCamera, &mTitleScreenSprite[i]);
                }
            }
            SysStatePop();
            switch ((s32)mTitleMode) {
            case TITLE_MODE_LOAD_GAME:
                SaveMemoryCardLoaded();
                break;
            case TITLE_MODE_NEW_GAME:
                SysChangeLevel(0);
                break;
            case TITLE_MODE_DONE:
                break;
            }
        } else {
            func_80143764();
        }
        return;
    }
    if (mTitleMode == TITLE_MODE_INIT) {
        func_80142208();
        func_80142C30();
        D_80144868 = 1;
        if ((u32)gSystemTick < 40) {
            func_80142098(TITLE_MODE_ATTRACT);
        } else {
            func_80142098(TITLE_MODE_MAIN);
        }
    }
    if (D_801415F8[mTitleMode] != NULL) {
        D_801415F8[mTitleMode]();
    }
    if (D_80144840 != CARD_UNKNOWN || mTitleMode == TITLE_MODE_ATTRACT) {
        if (mNewMenu) {
            func_80143920();
        } else {
            func_80143BA8();
            if (mTitleMode >= TITLE_MODE_INIT) {
                return;
            }
        }
        if (mTitleMode == TITLE_MODE_ATTRACT) {
            if (!gMovieOpenFlag) {
                func_80143084();
            }
            return;
        }
    }
    switch (MemCardSync(1, &type, &result)) {
    case 0:
        break;
    case -1:
        if ((mTitleMode != TITLE_MODE_MESSAGE || D_80144860 == 4) && mTitleMode != TITLE_MODE_SLOTS &&
            D_8014483A < D_8014483C) {
            func_80142A54(func_80142434());
        } else {
            func_80142A54(CARD_OP_EXIST);
        }
        break;
    case 1:
        if (mTitleMode == TITLE_MODE_MESSAGE && D_80144860 == 4 && D_8014483A >= D_8014483C) {
            func_8014216C();
        }
        switch ((u32)type) {
        case 1:
            if (result == 3) {
                D_80144840 = CARD_UNKNOWN;
                func_80142A54(CARD_OP_ACCEPT);
            } else if (result == 1) {
                if (D_80144840 != result) {
                    D_80144840 = result;
                    while (mTitleMode != TITLE_MODE_SAVE && mTitleMode != TITLE_MODE_MAIN) {
                        func_8014216C();
                    }
                    func_801424AC();
                }
            } else if (D_80144840 == CARD_OK) {
                func_80142888(result);
            } else if (D_80144840 == CARD_UNKNOWN) {
                func_80142A54(CARD_OP_READ_HEADER);
            }
            break;
        case 2:
            if (func_80142888(result) && D_80144840 == CARD_UNKNOWN) {
                func_80142A54(CARD_OP_READ_HEADER);
            }
            break;
        case 3:
            if (D_80144840 == CARD_UNKNOWN) {
                mCardFailedCallback = func_801424AC;
            }
            if (func_80142888(result)) {
                func_80143EF8();
            }
            break;
        case 4:
            func_80142888(result);
            break;
        }
        break;
    }
    if (RECT_XY(mTitleSelection[2]) != 0 || RECT_WH(mTitleSelection[2]) != 0) {
        func_80141DC8();
    }
    TextUpdate();
}

static void func_801444E4(void) {
    func_80142324();
    mTitleMode = TITLE_MODE_DONE;
}

/* Save written. */
static void func_80144510(void) {
    func_801424D8(D_801411D8, 1, 0, (void (*)(s32))func_801444E4);
}

/* Save loaded: start it, or a new game if the slot was empty. */
static void func_80144544(void) {
    if (mSyphonHeader.level[D_80144830] != 0) {
        func_80142324();
        mTitleMode = TITLE_MODE_LOAD_GAME;
    } else {
        func_80142324();
        mTitleMode = TITLE_MODE_NEW_GAME;
    }
}

void TitleSaveScreen(void) {
    func_80142208();
    func_80142098(TITLE_MODE_SAVE);
    InterfaceSetMode(7);
    mCurItem = 1;
    D_80144868 = 1;
}
