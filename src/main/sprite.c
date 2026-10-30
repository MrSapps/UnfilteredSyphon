#include "common.h"

#include "game.h"

SDATA_LOCAL(D_8010F864)
SDATA_LOCAL(D_8010F868)

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

/* (see egstex.c) */
typedef struct {
    /* 0x00 */ u32 *tim;
    /* 0x04 */ void *slot;
    /* 0x08 */ u8 animated;
    /* 0x09 */ u8 loaded;
    /* 0x0C */ u8 image[0x1C]; /* GsIMAGE */
    /* 0x28 */ s32 unk28;
} EGSTexture; /* size 0x2C */

/* A GsSPRITE plus the display list node it is linked by. */
typedef struct {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x, y;
    /* 0x08 */ u16 w, h;
    /* 0x0C */ u8 unk0C[8];
    /* 0x14 */ u8 r, g, b;
    /* 0x17 */ u8 unk17[0x11];
    /* 0x28 */ EGSListNode *node;
} SpriteEntry;

typedef struct {
    u8 r, g, b, pad;
} SpriteColor;

/* A group of sprites moved/shown together. */
typedef struct {
    /* 0x00 */ s16 x, y;
    /* 0x04 */ EGSListNode *sprites;
} SpriteMulti;

typedef struct {
    s16 x, y;
} ScreenPos;

/* A set of textures loaded together (0xC bytes). */
typedef struct {
    /* 0x00 */ s16 count;
    /* 0x04 */ char **names;
    /* 0x08 */ EGSTexture *textures;
} SpriteGroup;

#define SPRITE_TEMP_MAX 42

extern ScreenPos POSITION; /* screen offset (POSITION.y is D_8012C7FA) */

extern char *D_8010F864; /* foot fx texture names */
extern char *D_8010F868;
extern s32 D_80105E44[]; /* bullet type -> bullet texture */
extern SpriteEntry D_80115760[64]; /* interface sprites */
extern char *D_80105C20[64]; /* interface texture names */
extern SpriteColor D_8010F86C;
extern EGSTexture D_80116B50[];
extern EGSTexture D_80116C58[2];
extern EGSTexture D_80116CB0[SPRITE_TEMP_MAX]; /* temporary textures */
extern s32 D_801173E8[SPRITE_TEMP_MAX];

s32 EGSHogGetBlockByName(void *hog, char *name, EGSTexture *tex);
s32 EGSTextureCreate(EGSTexture *tex);
s32 EGSTextureLoad(EGSTexture *tex);
s32 EGSTextureCreateSprite(EGSTexture *tex, SpriteEntry *sprite, s16 x, s16 y, s16 w, u16 h);
void EGSTextureSetSpriteTransparency(SpriteEntry *sprite, s32 mode);
void InterfaceHealthMeterTexture(EGSTexture *tex);
void *EGSListAdd(EGSListNode **list, void *data);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
void EGSDisplayDeleteSprite(void *display, SpriteEntry *sprite);
void EGSTextureGetSpriteXY(SpriteEntry *sprite, ScreenPos *pos);

u8 SpriteInitTextureRam(void *hog, char *name, EGSTexture *tex)
{
    u8 ok = EGSHogGetBlockByName(hog, name, tex) == 0;

    if (ok == 1 && EGSTextureCreate(tex) != 0) {
        ok = 0;
    }
    return ok;
}

void SpriteCreateMulti(SpriteMulti *multi, s32 n, SpriteEntry *sprites)
{
    ScreenPos pos;
    SpriteEntry *t = sprites;
    s32 k = n;
    SpriteEntry *s = sprites;
    s32 i = n;

    multi->x = multi->y = 0;
    multi->sprites = NULL;
    for (i = n - 1; i != -1; i--) {
        EGSListAdd(&multi->sprites, s++);
    }
    while (--k != -1) {
        EGSTextureGetSpriteXY(t++, &pos);
        multi->x += pos.x - POSITION.x;
        multi->y += pos.y - POSITION.y;
    }
    multi->x /= n;
    multi->y /= n;
}

void SpriteDestroyMulti(SpriteMulti *multi)
{
    while (multi->sprites != NULL) {
        EGSListRemove(&multi->sprites, multi->sprites);
    }
}

void SpriteSetMultiXY(SpriteMulti *multi, s32 x, s32 y)
{
    EGSListNode *node = multi->sprites;
    s32 dx = x - multi->x;
    s32 dy = y - multi->y;
    SpriteEntry *sprite;

    while (node != NULL) {
        sprite = node->data;
        sprite->x += dx;
        sprite->y += dy;
        node = node->next;
    }
    multi->x = x;
    multi->y = y;
}

void SpriteShowMulti(u8 *display, SpriteMulti *multi)
{
    EGSListNode *node = multi->sprites;
    SpriteEntry *sprite;

    while (node != NULL) {
        sprite = node->data;
        sprite->node = EGSListAdd((EGSListNode **)(display + 0x90), sprite);
        node = node->next;
    }
}

void SpriteHideMulti(void *display, SpriteMulti *multi)
{
    EGSListNode *node = multi->sprites;

    while (node != NULL) {
        EGSDisplayDeleteSprite(display, node->data);
        node = node->next;
    }
}

void SpriteRecenterMulti(SpriteMulti *multi)
{
    EGSListNode *node = multi->sprites;
    s32 n = 0;
    ScreenPos pos;

    if (node != NULL) {
        while (node != NULL) {
            EGSTextureGetSpriteXY(node->data, &pos);
            multi->x += pos.x - POSITION.x;
            multi->y += pos.y - POSITION.y;
            node = node->next;
            n++;
        }
        multi->x /= n;
        multi->y /= n;
    }
}

/* Lays out a row of 1-5 sprites centred on (x, y). */
/* Lays out a row of 1-5 sprites centred on (x, y). */
/* Lays out a row of 1-5 sprites centred on (x, y). */
/* Lays out a row of 1-5 sprites centred on (x, y). */
/* Lays out a row of 1-5 sprites centred on (x, y). */
/* Lays out a row of 1-5 sprites centred on (x, y). */
void SpriteSuperSetXY(SpriteEntry *s, s32 count, s32 x, s32 y)
{
    s32 half;

    if (count == 2) {
        s[0].x = x - s[0].w / 2;
        s[0].y = y;
        s[1].x = x + s[1].w / 2;
        s[1].y = y;
    } else if (count == 3) {
        half = s[1].w / 2;
        s[0].x = x - s[0].w / 2 - half;
        s[0].y = y;
        s[1].x = x;
        s[1].y = y;
        s[2].x = x + s[2].w / 2 + half;
        s[2].y = y;
    } else if (count == 1) {
        s[0].x = x;
        s[0].y = y;
    } else if (count == 4) {
        s[0].x = x - s[0].w / 2 - s[1].w;
        s[0].y = y;
        s[1].x = x - s[1].w / 2;
        s[1].y = y;
        s[2].x = x + s[2].w / 2;
        s[2].y = y;
        s[3].x = s[2].w + (x + s[3].w / 2);
        s[3].y = y;
    } else if (count == 5) {
        s[0].x = x - s[0].w / 2 - s[1].w - s[2].w / 2;
        s[0].y = y;
        s[1].x = x - s[1].w / 2 - s[2].w / 2;
        s[1].y = y;
        s[2].x = x;
        s[2].y = y;
        s[3].x = x + s[3].w / 2 + s[2].w / 2;
        s[3].y = y;
        s[4].x = s[3].w + (x + s[2].w / 2) + s[4].w / 2;
        s[4].y = y;
    }
}

u8 SpriteSetupInterface(void *hog)
{
    EGSTexture tex;
    SpriteColor color;
    SpriteEntry *sprite;
    u8 ok;
    s32 i;

    for (i = 0; i < 64; i++) {
        ok = SpriteInitTextureRam(hog, D_80105C20[i], &tex);
        if (ok) {
            if (EGSTextureLoad(&tex) != 0) {
                ok = 0;
            }
            sprite = &D_80115760[i];
            if (EGSTextureCreateSprite(&tex, sprite, 0x80, 0x80, 0, 0) != 0) {
                ok = 0;
            }
            color = D_8010F86C;
            sprite->r = color.r;
            sprite->g = color.g;
            sprite->b = color.b;
            if (i == 0) {
                InterfaceHealthMeterTexture(&tex);
            }
            if (i >= 44) {
                EGSTextureSetSpriteTransparency(sprite, 0);
            } else {
                EGSTextureSetSpriteTransparency(sprite, 4);
            }
        }
    }
    return ok;
}

u8 SetupMultiTextures(void *hog, s16 count, SpriteGroup *groups)
{
    SpriteGroup *group;
    u8 ok = 1;
    s32 i, j, n;

    for (i = 0; i < count; i++) {
        n = groups[i].count;
        for (j = 0; j < n; j++) {
            group = &groups[i];
            if (group->names[j][0] != '\0') {
                ok = SpriteInitTextureRam(hog, group->names[j], &group->textures[j]);
                if (ok && EGSTextureLoad(&group->textures[j]) != 0) {
                    ok = 0;
                }
            }
        }
    }
    return ok;
}

SpriteEntry *SpriteGet(s32 i)
{
    return &D_80115760[i];
}

EGSTexture *BulletTextureGet(u32 type)
{
    s32 i;

    if (type < 26) {
        i = D_80105E44[type];
    } else {
        i = 2;
    }
    if (i == 0) {
        return NULL;
    }
    return &D_80116B50[i];
}

void SpriteCreateFootFx(void *hog)
{
    SpriteInitTextureRam(hog, D_8010F864, &D_80116C58[0]);
    SpriteInitTextureRam(hog, D_8010F868, &D_80116C58[1]);
}

EGSTexture *SpriteFootFxTextureGet(s32 i)
{
    return &D_80116C58[i];
}

void SpriteInitTemporary(void)
{
    int i;

    for (i = 0; i < SPRITE_TEMP_MAX; i++) {
        D_80116CB0[i].unk28 = 0;
        D_801173E8[i] = -1;
    }
}

/* D_801173E8[i]: >0 first slot of a run of that many, 0 rest of a run, -1 free */
EGSTexture *SpriteGetTemporary(s32 count) {
    s32 i;
    s32 j;

    i = 0;
    while (i + count < SPRITE_TEMP_MAX) {
        if (D_801173E8[i] <= 0) {
            for (j = i; D_801173E8[j] < 0 && j < i + count; j++) {
            }
            if (j == i + count) {
                break;
            }
            i = j;
        }
        i += D_801173E8[i];
    }
    if (i + count > SPRITE_TEMP_MAX) {
        return NULL;
    }
    D_801173E8[i] = count;
    for (j = i + 1; j < i + count; j++) {
        D_801173E8[j] = 0;
    }
    return &D_80116CB0[i];
}

s32 SpriteDisposeTemporary(EGSTexture *tex)
{
    u32 idx = tex - D_80116CB0;
    s32 ret = 0;
    s32 i;

    if (idx >= SPRITE_TEMP_MAX) {
        return 2;
    }
    if (D_801173E8[idx] <= 0) {
        return 3;
    }
    for (i = 1; i < D_801173E8[idx]; i++) {
        if (D_801173E8[i + idx] != 0) {
            break;
        }
        D_801173E8[i + idx] = -1;
    }
    if (D_801173E8[i + idx] != 0) {
        ret = 3;
    }
    D_801173E8[idx] = -1;
    return ret;
}
