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
    /* 0x0C */ u8 r, g, b, code;
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

/* Six interface sprites, 0x18 bytes each. */
typedef struct IfaceSprite18 {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} IfaceSprite18;

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

extern IfaceSprite18 D_801179A0[6];
extern struct { s32 unk00; u8 unk04[0x14]; } D_801179B0[6]; /* D_801179A0[].unk10 */
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
    /* 0x04 */ u8 unk04[0x20];
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
extern IfacePoly24 D_80117C70[2];       /* radar */
extern IfacePoly24 D_80117CB8[2];
extern IfacePoly24 D_80117D00;
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
    IfaceSprite18 *poly;

    for (i = 1; i < 8; i++) {
        if (D_80105F30[i - 1] > 0) {
            D_80105F30[i - 1] = 1;
        }
    }
    i = 0;
    off = 0x04000400;
    poly = D_801179A0;
    for (; i < 6; i++) {
        (poly++)->unk14 = off;
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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003C4F8);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003D0E8);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003DF74);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003E13C);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003E894);

s32 func_8003ECA0(u8 big) {
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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003EF48);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003F3F8);

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003F8A0);

#ifdef NON_MATCHING
/* the original reloads the cursor position after each store (an SVECTOR there, so the stores may alias); with scalars GCC keeps them in registers */
void func_8003FA60(VECTOR32 *pos) {
    s32 dx = pos->vx - mTargetCursorPosition;
    s32 dy = pos->vy - D_8011031A;
    s32 dz = pos->vz - D_8011031C;
    s32 dist = SquareRoot0(dx * dx + dy * dy + dz * dz);

    if (dist > 320) {
        pos->vx = mTargetCursorPosition + dx * 320 / dist;
        pos->vy = D_8011031A + dy * 320 / dist;
        pos->vz = D_8011031C + dz * 320 / dist;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003FA60);
#endif

INCLUDE_ASM("asm/main/nonmatchings/interface", func_8003FBE4);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceCountdownDisplay);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceWeaponSelect);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", iSetupDirectionSprites);

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
            line->code |= 2;
        } else {
            line->code &= ~2;
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

INCLUDE_ASM("asm/main/nonmatchings/interface", func_800451DC);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", InterfaceSetSniperDoodads);

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

INCLUDE_ASM("asm/main/nonmatchings/interface", iInterfaceDoSniffer);
