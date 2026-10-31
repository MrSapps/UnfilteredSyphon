#include "common.h"
#include "game.h"
/* Data below in original order: .sdata D_8010FF20/24, .bss D_80122A38 (keep line count: __LINE__) */

/* libgs / libgpu types */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    /* 0x00 */ u32 pmode;
    /* 0x04 */ s16 px, py;
    /* 0x08 */ u16 pw, ph;
    /* 0x0C */ u32 *pixel;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u16 cw, ch;
    /* 0x18 */ u32 *clut;
} GsIMAGE;

/* GsSPRITE plus one extra word */
typedef struct {
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
} EGSSprite;

/* A VRAM location an animated texture is moved to (pool of 50). */
typedef struct VramSlot {
    /* 0x00 */ s16 x, y;
    /* 0x04 */ u8 used;
    /* 0x08 */ struct VramSlot *next;
} VramSlot;

typedef struct {
    /* 0x00 */ u32 *tim;
    /* 0x04 */ VramSlot *slot;
    /* 0x08 */ u8 animated;
    /* 0x09 */ u8 loaded;
    /* 0x0C */ GsIMAGE image;
    /* 0x28 */ s32 unk28;
} EGSTexture;

#define VRAM_SLOT_MAX 50

static VramSlot *D_8010FF20 = NULL; /* .sdata: list of used VRAM slots */
static s16 D_8010FF24 = 0;          /* .sdata: number of slots allocated from the pool */
static VramSlot D_80122A38[VRAM_SLOT_MAX]; /* .bss */
typedef struct {
    s16 x, y;
} ScreenPos;
extern ScreenPos POSITION;   /* screen offset (POSITION.y is D_8012C7FA) */

int DrawSync(int);
void GsGetTimInfo();
void LoadImage();
void MoveImage();
int GetTPage();

s32 EGSTextureCreate(EGSTexture *tex) {
    GsIMAGE *image = &tex->image;

    if (tex->tim == NULL || tex->tim[0] != 0x10) {
        return 0x13;
    }
    GsGetTimInfo(tex->tim + 1, image);
    tex->unk28 = 0;
    tex->loaded = 0;
    tex->animated = 0;
    return 0;
}

s32 EGSTextureCreateAnimated(EGSTexture *tex, RECT *pos) {
    VramSlot *slot;
    VramSlot *newSlot;

    if (tex == NULL) {
        return 0x13;
    }
    if ((u16)pos->x >= 0x400) {
        return 0xE;
    }
    if ((u16)pos->y >= 0x200) {
        return 0xE;
    }
    tex->animated = 1;
    slot = D_8010FF20;
    if (slot != NULL) {
        while (slot != NULL) {
            if (slot->x == pos->x && slot->y == pos->y) {
                tex->slot = slot;
                slot = NULL;
            } else if (slot->next == NULL) {
                newSlot = &D_80122A38[D_8010FF24++];
#line 158
                ASSERT(D_8010FF24 < VRAM_SLOT_MAX);
                if (newSlot != NULL) {
                    newSlot->x = pos->x;
                    newSlot->y = pos->y;
                    newSlot->used = 0;
                    newSlot->next = NULL;
                    tex->slot = newSlot;
                    slot->next = newSlot;
                    slot = NULL;
                }
            } else {
                slot = slot->next;
            }
        }
    } else {
        newSlot = &D_80122A38[D_8010FF24++];
#line 182
        ASSERT(D_8010FF24 < VRAM_SLOT_MAX);
        if (newSlot != NULL) {
            newSlot->x = pos->x;
            newSlot->y = pos->y;
            newSlot->used = 0;
            newSlot->next = NULL;
            D_8010FF20 = newSlot;
            tex->slot = newSlot;
        }
    }
    return 0;
}

s32 EGSTextureLoad(EGSTexture *tex) {
    RECT rect;
    RECT clutRect;

    DrawSync(0);
    if (&tex->image == NULL) {
        return 0x13;
    }
    rect.x = tex->image.px;
    rect.y = tex->image.py;
    rect.w = tex->image.pw;
    rect.h = tex->image.ph;
    LoadImage(&rect, tex->image.pixel);
    if ((tex->image.pmode >> 3) & 1) {
        clutRect.x = tex->image.cx;
        clutRect.y = tex->image.cy;
        clutRect.w = tex->image.cw;
        clutRect.h = tex->image.ch;
        LoadImage(&clutRect, tex->image.clut);
    }
    tex->loaded = 1;
    return 0;
}

s32 EGSTextureMove(EGSTexture *tex) {
    GsIMAGE *image = &tex->image;
    RECT rect;
    u8 canMove = 1;

    if (image == NULL) {
        return 0x13;
    }
    if (tex->loaded != 1) {
        return 0x19;
    }
    if (tex->slot != NULL && tex->slot->used == tex->loaded) {
        canMove = 0;
    }
    if (canMove == 1) {
        tex->slot->used = 1;
        rect.x = image->px;
        rect.y = image->py;
        rect.w = image->pw;
        rect.h = image->ph;
        MoveImage(&rect, tex->slot->x, tex->slot->y);
    }
    return 0;
}

s32 EGSTextureCreateSprite(EGSTexture *tex, EGSSprite *sprite, s16 x, s16 y, s16 w, u16 h) {
    s32 mode;
    s32 shift;
    u32 tpage;

    if (tex == NULL) {
        return 0x13;
    }
    mode = tex->image.pmode & 3;
    shift = 2 - mode;
    tpage = GetTPage(mode, 0, tex->image.px, tex->image.py);
    sprite->attribute = mode << 24;
    tpage &= 0x1F;
    sprite->x = x - POSITION.x;
    sprite->y = y - POSITION.y;
    if (w == 0) {
        sprite->w = tex->image.pw << shift;
    } else {
        sprite->w = w;
    }
    if (h == 0) {
        sprite->h = tex->image.ph;
    } else {
        sprite->h = h;
    }
    sprite->tpage = tpage;
    sprite->mx = sprite->w >> 1;
    sprite->my = sprite->h >> 1;
    if (tpage < 16) {
        sprite->u = (tex->image.px - tpage * 64) << shift;
    } else {
        sprite->u = (tex->image.px - (tpage - 16) * 64) << shift;
    }
    sprite->v = tex->image.py;
    sprite->cx = tex->image.cx;
    sprite->cy = tex->image.cy;
    sprite->r = 0x80;
    sprite->g = 0x80;
    sprite->b = 0x80;
    sprite->rotate = 0;
    sprite->scalex = 0x1000;
    sprite->scaley = 0x1000;
    sprite->unk24 = 0;
    return 0;
}

s32 EGSTextureSetSpriteDimensions(EGSSprite *sprite, u16 w, u16 h) {
    if (sprite == NULL) {
        return 0x16;
    }
    sprite->w = w;
    sprite->h = h;
    return 0;
}

s32 EGSTextureGetSpriteXY(EGSSprite *sprite, s16 *xy) {
    if (sprite == NULL) {
        return 0x16;
    }
    xy[0] = sprite->x + POSITION.x;
    xy[1] = sprite->y + POSITION.y;
    return 0;
}

s32 EGSTextureSetSpriteTransparency(EGSSprite *sprite, s32 mode) {
    u32 attr;
    u32 cleared;
    u32 set;

    if (sprite == NULL) {
        return 0x16;
    }
    attr = sprite->attribute;
    attr &= ~0x40000000;
    attr &= ~0x10000000;
    cleared = attr & ~0x20000000;
    sprite->attribute = cleared;
    if (mode != 4) {
        set = cleared | 0x40000000;
        set |= (mode & 3) << 28;
        sprite->attribute = set;
    }
    return 0;
}
