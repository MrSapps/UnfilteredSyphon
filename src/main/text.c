#include "common.h"
#include "system.h"
#include "scratch.h"
#include "game.h"

SDATA_LOCAL(gTextCamera)
SDATA_LOCAL(D_8010FA48)
SDATA_LOCAL(D_8010FA4C)
SDATA_LOCAL(D_8010FA50)
SDATA_LOCAL(D_8010FAA8)
SDATA_LOCAL(D_8010FAAC)
SDATA_LOCAL(D_801101A8)
SDATA_LOCAL(D_801101AA)
SDATA_LOCAL(D_801101AC)
SDATA_LOCAL(D_801101AE)
SDATA_LOCAL(D_801101B0)

/* One displayed character: a GsSPRITE plus two words (0x2C bytes). */
typedef struct TextChar {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ u16 w;
    /* 0x0A */ u16 h;
    /* 0x0C */ u16 tpage;
    /* 0x0E */ u8 u;
    /* 0x0F */ u8 v;
    /* 0x10 */ s16 cx;
    /* 0x12 */ s16 cy;
    /* 0x14 */ u8 r;
    /* 0x15 */ u8 g;
    /* 0x16 */ u8 b;
    /* 0x17 */ u8 pad17;
    /* 0x18 */ s16 mx;
    /* 0x1A */ s16 my;
    /* 0x1C */ s16 scalex;
    /* 0x1E */ s16 scaley;
    /* 0x20 */ s32 rotate;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
} TextChar;

/* A string shown in a text window (0x1C bytes); ids are (serial << 8) | index. */
typedef struct TextString {
    /* 0x00 */ TextChar *chars;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u32 endTick; /* -1: forever */
    /* 0x0C */ u16 length;
    /* 0x0E */ u16 xOffset;
    /* 0x10 */ u8 r;
    /* 0x11 */ u8 g;
    /* 0x12 */ u8 b;
    /* 0x13 */ u8 lineHeight;
    /* 0x14 */ u8 flags; /* 4: make noise */
    /* 0x15 */ u8 serial;
    /* 0x16 */ u16 speed;
    /* 0x18 */ struct TextString *next; /* continuation (multi-line) */
} TextString;

/* A text window (0x14 bytes). */
typedef struct TextWindow {
    /* 0x00 */ u32 flags; /* 1: active, 0x10: mini */
    /* 0x04 */ u8 r;
    /* 0x05 */ u8 g;
    /* 0x06 */ u8 b;
    /* 0x07 */ s8 yAdjust;
    /* 0x08 */ s16 x;
    /* 0x0A */ s16 y;
    /* 0x0C */ s16 w;
    /* 0x0E */ s16 h;
    /* 0x10 */ EGSListNode *strings;
} TextWindow;

#define TEXT_STRING_MAX 50
#define TEXT_WINDOW_MAX 7

extern TextString D_80120BE0[TEXT_STRING_MAX];
extern TextWindow D_80121158[TEXT_WINDOW_MAX];
extern void *gTextCamera;
extern EGSListNode *D_8010FA50; /* persistent strings */
extern s32 D_8010FAA8;          /* tick of the last TextUpdate */
typedef struct TextRect {
    s16 x, y, w, h;
} TextRect;
extern TextRect D_801101A8; /* TextStringRect result */
extern s32 D_8010FA48;
extern s16 D_8010FA4C;
extern u8 D_8010FAA0[];         /* camera parameters */
extern TextChar D_8011FAB0[100];

/* A flat-shaded line primitive built by EGSDisplayBuildLineF (0x18 bytes). */
typedef struct TextLine {
    /* 0x00 */ s32 tag;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
} TextLine;
extern TextLine D_801211E4[10];
extern u32 D_801101B0;          /* tick of the next queued TextUpdate */

#define TEXT_GET_STRING(s, id)     \
    if ((id) >= TEXT_STRING_MAX) { \
        s = NULL;                  \
    } else {                       \
        s = &D_80120BE0[id];       \
    }
#define TEXT_GET_WINDOW(w, id)     \
    if ((id) >= TEXT_WINDOW_MAX) { \
        w = NULL;                  \
    } else {                       \
        w = &D_80121158[id];       \
    }

void func_80082C4C(void);
void func_80082CE8(TextString *s);
void EGSTextureSetSpriteTransparency(TextChar *sprite, s32 on);
u16 TextAddString(u8 window, char *str, s32 ticks, TextChar *sprites);
u16 func_80083F1C();
u32 func_800857B8(TextWindow *w);
void EGSCameraCreate(s32 arg0, void *arg1, s32 arg2, void *arg3, s32 arg4, s32 arg5, s32 arg6, void **out);
void EGSDisplayBuildLineF(TextLine *line, s32 arg1, s32 arg2, s32 arg3);
void iTextGetWindowOffsets(TextWindow *w, s16 *x, s16 *y);
void EGSListAddToEnd(EGSListNode **list, EGSListNode *after, void *data);
void TextUpdate(void);
void TextSpriteInitialize(TextChar *sprites, s32 count);
TextRect *TextStringRect(u16 id, s32 all);
void func_80082E70();
void EGSDisplayDeleteSprite(void *camera, TextChar *sprite);
void func_80085024(TextString *s); /* also called from hotbox.c */
static void func_800863E8(TextString *s);

INCLUDE_RODATA("asm/main/nonmatchings/text", D_800123D8);

INCLUDE_RODATA("asm/main/nonmatchings/text", D_80012448);

INCLUDE_RODATA("asm/main/nonmatchings/text", D_800124F8);

INCLUDE_RODATA("asm/main/nonmatchings/text", D_80012504);

INCLUDE_RODATA("asm/main/nonmatchings/text", D_80012510);

void TextInitialize(s32 createCamera) {
    s32 params[4] = { 0, 0, 0, 0 };
    TextWindow *w;
    TextString *s;
    s32 i;

    w = D_80121158;
    s = D_80120BE0;
    if (createCamera) {
        EGSCameraCreate(0, params, 1, D_8010FAA0, 4, 2, 0, &gTextCamera);
        ((u16 *)gTextCamera)[3] |= 1;
    }
    D_8010FA48 = 0;
    D_8010FA4C = 0;
    D_801101B0 = -1;
    for (i = 0; i < 6; i++, w++) {
        w->y = 0;
        w->x = 0;
        w->h = 0;
        w->w = 0;
        w->strings = NULL;
        w->flags = 0;
        w->b = 0x80;
        w->g = 0x80;
        w->r = 0x80;
        w->yAdjust = 0;
    }
    w->x = -0x7A;
    w->y = 0x50;
    w->w = 0xD2;
    w->h = 0x3C;
    w->strings = NULL;
    w->b = 0x80;
    w->g = 0x80;
    w->r = 0x80;
    w->yAdjust = 0;
    w[1].strings = NULL; /* past the end: lands in D_801211E4[0] */
    w->flags |= 4;
    for (i = 0; i < TEXT_STRING_MAX; i++, s++) {
        s->length = 0;
        s->endTick = 0;
        *(s32 *)s->unk04 = 0;
        s->chars = NULL;
        s->next = NULL;
        s->flags = 0;
    }
    D_8010FA50 = NULL;
    TextSpriteInitialize(D_8011FAB0, 100);
    for (i = 0; i < 10; i++) {
        EGSDisplayBuildLineF(&D_801211E4[i], 0, 0x4000400, 0x4000400);
        D_801211E4[i].tag = 0;
    }
}

void TextSpriteInitialize(TextChar *sprites, s32 count) {
    s32 unused[1]; /* unreferenced; the original frame has a 4-byte local */

    for (count--; count != -1; count--) {
        sprites->attribute = 0x1000000;
        sprites->h = 8;
        sprites->tpage = 0xD;
        sprites->cx = 0x300;
        sprites->mx = 0;
        sprites->my = 0;
        sprites->cy = 0x1E3;
        sprites->r = 0;
        sprites->g = 0;
        sprites->b = 0;
        sprites->rotate = 0;
        sprites->scalex = 0x1000;
        sprites->scaley = 0x1000;
        sprites->unk24 = 0;
        sprites->unk28 = 0;
        EGSTextureSetSpriteTransparency(sprites, 1);
        sprites++;
    }
}

u8 TextAddWindow(s16 x, s16 y, s16 w, s16 h) {
    TextWindow *win;
    EGSListNode *node;
    EGSListNode *next;
    s32 i;

    for (win = D_80121158, i = 0; i < 6; i++, win++) {
        if (win->x == x && win->y == y && win->w == w && win->h == h) {
            win->flags |= 3;
            if (win->strings != NULL) {
                for (node = win->strings; node != NULL;) {
                    if (gSystemTick >= ((TextString *)node->data)->endTick) {
                        next = node->next;
                        func_800863E8(node->data);
                        EGSListRemove(&win->strings, node);
                        node = next;
                    } else {
                        node = node->next;
                    }
                }
            }
            win->yAdjust = 0;
            return i;
        }
    }
    win = D_80121158;
    i = 0;
    while ((win->flags & 1) && i < 6) {
        win++;
        i++;
    }
    if (i >= 6) {
        return 0xFF;
    }
    win->r = win->g = win->b = 0x80;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->yAdjust = 0;
    win->flags = 3;
    return i;
}

u8 iTextRemoveWindow(TextWindow *w) {
    EGSListNode *node;

    if (w >= &D_80121158[6]) {
        return 0;
    }
    if (w->strings != NULL) {
        for (node = w->strings; node != NULL; node = node->next) {
            func_80085024(node->data);
        }
        func_80082C4C();
    } else {
        w->flags &= ~1;
    }
    return 1;
}

s32 TextRemoveWindow(u8 id) {
    TextWindow *w;
    s32 ret;

    TEXT_GET_WINDOW(w, id);
    ret = 0;
    if (w != NULL) {
        ret = iTextRemoveWindow(w);
    }
    return ret;
}

s32 TextRemoveWindowNow(u8 id) {
    TextWindow *w;

    TEXT_GET_WINDOW(w, id);
    if (w == NULL) {
        return 0;
    }
    while (w->strings != NULL) {
        func_800863E8(w->strings->data);
        EGSListRemove(&w->strings, w->strings);
    }
    w->flags = 0;
    return 1;
}

void TextSetWindowJustification(u8 id, s32 just) {
    TextWindow *w;

    TEXT_GET_WINDOW(w, id);
    if (w != NULL) {
        switch (just) {
            case 0:
                w->flags |= 4;
                w->flags &= ~8;
                break;
            case -1:
                w->flags &= ~4;
                w->flags &= ~8;
                break;
            case 1:
                w->flags &= ~4;
                w->flags |= 8;
                break;
        }
    }
}

void TextSetWindowMini(u8 id, u8 on) {
    TextWindow *w;

    TEXT_GET_WINDOW(w, id);
    if (w != NULL) {
        if (on) {
            w->flags |= 0x10;
        } else {
            w->flags &= ~0x10;
        }
    }
}

void TextSetWindowColor(u8 id, u8 r, u8 g, u8 b) {
    TextWindow *w;

    TEXT_GET_WINDOW(w, id);
    if (w != NULL) {
        w->r = r;
        w->g = g;
        w->b = b;
    }
}

void TextActivateAllWindows(u8 active) {
    TextWindow *w;
    EGSListNode *node;
    TextString *s;
    TextChar *ch;
    s32 i;

    for (w = D_80121158, i = 0; i < TEXT_WINDOW_MAX; i++, w++) {
        if (w->flags & 1) {
            if (active) {
                w->flags |= 2;
            } else {
                w->flags &= ~2;
                for (node = w->strings; node != NULL; node = node->next) {
                    TextChar *c;

                    s = node->data;
                    c = &s->chars[s->length];
                    while (s->chars < c--) {
                        c->b = 0;
                        c->g = 0;
                        c->r = 0;
                    }
                }
            }
        }
    }
    if (!active) {
        EGSListNode *pnode;
        TextString *ps;
        TextLine *line;

        for (pnode = D_8010FA50; pnode != NULL; pnode = pnode->next) {
            ps = pnode->data;
            ch = &ps->chars[ps->length];
            ps->flags |= 0x10;
            while (ps->chars < ch--) {
                ch->b = 0;
                ch->g = 0;
                ch->r = 0;
            }
        }
        for (i = 0; i < 10; i++) {
            line = &D_801211E4[i];
            line->xy1 = 0x4000400;
            D_801211E4[i].xy0 = 0x4000400;
        }
    } else {
        EGSListNode *mnode;

        for (mnode = D_8010FA50; mnode != NULL; mnode = mnode->next) {
            ((TextString *)mnode->data)->flags &= ~0x10;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/text", func_80083F1C);

#ifdef NON_MATCHING
/* Only the final add's operand order and result register (a3 in the original) differ. */
void iTextGetWindowOffsets(TextWindow *w, s16 *x, s16 *y) {
    EGSListNode *node;
    s32 lines;
    s32 scale;

    lines = 0;
    for (node = w->strings; node != NULL; node = node->next) {
        lines += ((TextString *)node->data)->lineHeight;
    }
    scale = 8;
    if (w != NULL && (w->flags & 0x10)) {
        scale = 6;
    }
    *y = lines * scale + w->y - w->yAdjust;
    *x = w->x;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/text", iTextGetWindowOffsets);
#endif

u16 TextAddString(u8 window, char *str, s32 ticks, TextChar *sprites) {
    TextWindow *w;
    TextString *s;
    s16 x;
    s16 y;
    u16 id;

    TEXT_GET_WINDOW(w, window);
    if (w == NULL) {
        return 0xFFFF;
    }
    iTextGetWindowOffsets(w, &x, &y);
    id = func_80083F1C(w, str, ticks, sprites, &x, &y);
    if (id != 0xFFFF) {
        TEXT_GET_STRING(s, (u8)id);
        s->next = NULL;
        EGSListAddToEnd(&w->strings, NULL, s);
        if (w->flags & 4) {
            s->flags |= 8;
            s->xOffset += (s16)w->w / 2;
            func_80082CE8(s);
        } else if (w->flags & 8) {
            s->flags |= 0x40;
            s->xOffset += w->w;
            func_80082CE8(s);
        }
        func_80082C4C();
    }
    return id;
}

INCLUDE_ASM("asm/main/nonmatchings/text", TextAddStringMulti);

#ifdef NON_MATCHING
/* Only the order of the gSystemTick / length loads in the default-duration branch differs. */
u16 TextStatusMessage(char *msg, s32 ticks) {
    TextString *s;
    u16 id;

    id = TextAddString(6, msg, -1, NULL);
    TEXT_GET_STRING(s, (u8)id);
    if (s != NULL) {
        if (ticks != 0) {
            s->endTick = gSystemTick + ticks;
        } else {
            s->endTick = gSystemTick + 0x18 + (s->length << 1);
        }
    }
    return id;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/text", TextStatusMessage);
#endif

u16 TextAddPersistentString(char *str, TextChar *sprites, s16 x, s16 y) {
    TextString *s;
    TextString *t;
    u16 id;

    if (sprites == NULL) {
        return 0xFFFF;
    }
    id = func_80083F1C(0, str, -1, sprites, &x, &y);
    if (id != 0xFFFF) {
        func_80082C4C();
        TEXT_GET_STRING(s, (u8)id);
        EGSListAdd(&D_8010FA50, s);
        TEXT_GET_STRING(t, (u8)id);
        t->flags &= 0x7F;
    }
    return id;
}

u16 TextAddMarker(u16 marker, char *str, s32 ticks, TextChar *sprites) {
    EGSListNode *node;
    TextString *s;
    s16 x;
    s16 y;
    u16 id;

    x = 0;
    y = 300;
    for (node = D_8010FA50; node != NULL; node = node->next) {
        s = node->data;
        if (s->flags & 2) {
            if (s->speed == marker) {
                return 0xFFFF;
            }
            x++;
        }
    }
#line 1741
    ASSERT(x < 4);
    x = 0;
    id = func_80083F1C(NULL, str, ticks, sprites, &x, &y);
    if (id != 0xFFFF) {
        TEXT_GET_STRING(s, (u8)id);
        func_80082C4C();
        EGSListAdd(&D_8010FA50, s);
        s->flags = (s->flags & 0x7F) | 2;
        func_80082CE8(s);
        s->speed = marker;
        s->r = 0xE0;
        s->g = 0xE0;
        s->b = 0xE0;
    }
    return id;
}

void TextRemoveString(u8 id) {
    TextString *s;

    TEXT_GET_STRING(s, id);
    if (s != NULL) {
        func_80085024(s);
    }
}

void func_80085024(TextString *s) {
    for (; s != NULL; s = s->next) {
        s->endTick = gSystemTick;
    }
    func_80082C4C();
}

s32 TextIsStringShowing(u16 id) {
    TextString *s;
    s32 showing;

    if (id != 0xFFFF) {
        s = &D_80120BE0[(u8)id];
        showing = s->serial == (id >> 8);
        if (s->endTick != -1) {
            showing &= gSystemTick < s->endTick + 8;
        }
    } else {
        showing = 0;
    }
    return showing;
}

u16 TextSetStringSpeed(u8 id, u16 speed) {
    TextString *s;
    u16 old;

    TEXT_GET_STRING(s, id);
    old = 0;
    if (s != NULL) {
        old = s->speed;
        s->speed = speed;
    }
    return old;
}

void TextStringMakeNoise(u8 id, u8 on) {
    TextString *s;

    TEXT_GET_STRING(s, id);
    if (s != NULL) {
        if (on) {
            s->flags |= 4;
        } else {
            s->flags &= ~4;
        }
    }
}

void TextMoveString(u8 id, s32 x, s32 y) {
    TextString *s;

    TEXT_GET_STRING(s, id);
    if (s != NULL) {
        func_80082E70(s, x, y);
    }
}

void TextSetStringJustification(u8 id, s32 just) {
    TextString *s;

    TEXT_GET_STRING(s, id);
    if (s != NULL) {
        if (just == 0) {
            s->flags = (s->flags | 8) & ~0x40;
        } else if (just == 1) {
            s->flags = (s->flags | 0x40) & ~8;
        } else {
            s->flags &= ~0x48;
        }
        func_80082CE8(s);
    }
}

s32 TextStringLength(u8 id, s32 all) {
    TextString *s;
    s32 len;

    TEXT_GET_STRING(s, id);
    if (all) {
        len = 0;
        for (; s != NULL; s = s->next) {
            len += s->length;
        }
    } else {
        len = s->length;
    }
    return len;
}

TextRect *TextStringRect(u16 id, s32 all) {
    TextString *s;
    TextChar *ch;
    s32 i;
    s32 lineHeight;

    TEXT_GET_STRING(s, (u8)id);
    if (s == NULL) {
        return NULL;
    }
    lineHeight = 8;
    if (s->flags & 0x80) {
        lineHeight = 6;
    }
    ch = s->chars;
    D_801101A8.x = ch->x;
    D_801101A8.y = ch->y;
    D_801101A8.w = ch->w;
    D_801101A8.h = lineHeight;
    if (all && s->next != NULL) {
        TextStringRect((u16)(s->next - D_80120BE0), 1);
    }
    for (i = 0; i < s->length; i++, ch++) {
        if (D_801101A8.x > ch->x) {
            D_801101A8.w += D_801101A8.x - ch->x;
            D_801101A8.x = ch->x;
        }
        if (D_801101A8.y > ch->y) {
            D_801101A8.h += D_801101A8.y - ch->y;
            D_801101A8.y = ch->y;
        }
        if (D_801101A8.w + D_801101A8.x < ch->x + ch->w) {
            D_801101A8.w = ch->x + ch->w - D_801101A8.x;
        }
        if (D_801101A8.h + D_801101A8.y < ch->y + lineHeight) {
            D_801101A8.h = ch->y + lineHeight - D_801101A8.y;
        }
    }
    return &D_801101A8;
}

u32 TextChangeStringDuration(u8 id, s32 ticks) {
    TextString *s;
    u32 end;

    TEXT_GET_STRING(s, id);
    if (s == NULL) {
        return 0;
    }
    end = gSystemTick + ticks;
    if (end < gSystemTick) {
        s->endTick = -1;
    } else {
        s->endTick = end;
    }
    func_80082C4C();
    return s->endTick;
}

u8 TextComputeChecksum(u8 *str) {
    s32 sum;

    sum = 0;
    while (*str != 0) {
        sum += *str++;
    }
    return sum;
}

u16 TextFindString(s32 checksum) {
    TextString *s;
    s32 i;

    for (i = 0; i < TEXT_STRING_MAX; i++) {
        TEXT_GET_STRING(s, (u8)i);
        if ((s->flags & 1) && gSystemTick < s->endTick && s->serial == checksum) {
            return i | (checksum << 8);
        }
    }
    return 0xFFFF;
}

void TextUpdate(void) {
    u32 next;
    u32 t;
    u8 i;
    TextWindow *w;

    next = -1;
    if (gSystemTick == 0 || D_8010FAA8 != gSystemTick) {
        w = D_80121158;
        i = 0;
        D_8010FAA8 = gSystemTick;
        D_801101B0 = next;
        for (; i < TEXT_WINDOW_MAX; i++, w++) {
            if ((w->flags & 2) && w->flags != 0) {
                SPAD_ENTER();
                t = func_800857B8(w);
                SPAD_LEAVE();
                if (t < next) {
                    next = t;
                }
            }
        }
        SPAD_ENTER();
        t = func_800857B8(NULL);
        SPAD_LEAVE();
        if (t < next) {
            next = t;
        }
        if (!(u8)SysIsPaused()) {
            if (next == 0) {
                SysPostMessage(0x1F, 4, SYS_MSG_LEVEL, SYS_MSG_LEVEL, 0, 0, 0, 0);
                D_801101B0 = gSystemTick + 1;
            } else if (next < D_801101B0) {
                EGSDisplayQueueInterrupt(TextUpdate, next - gSystemTick + 1, 0xE0C00000);
                D_801101B0 = next;
            }
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/text", func_800857B8);

void TextSetStringIntensity(u8 id, u8 r, u8 g, u8 b) {
    TextString *s;

    TEXT_GET_STRING(s, id);
    if (s != NULL) {
        s->r = r;
        s->g = g;
        s->b = b;
        func_80082C4C();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/text", TextReplaceString);

/* Fade a string chain's characters by a quarter. */
static void func_80085F24(TextString *s) {
    TextChar *ch;
    s32 n;

    for (; s != NULL; s = s->next) {
        n = s->length;
        ch = s->chars;
        while (n > 0) {
            n--;
            ch->r -= ch->r >> 2;
            ch->g -= ch->g >> 2;
            ch->b -= ch->b >> 2;
            ch++;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/text", func_80085F98);

#ifdef NON_MATCHING
/*
 * Move each character's colour 1/8 of the way (at least 1) towards the string's.
 * Same length; register allocation and scheduling of the three divisions differ.
 */
static void func_800862B0(TextString *s) {
    TextChar *ch;
    s32 i;
    s32 dr;
    s32 dg;
    s32 db;

    ch = s->chars;
    for (i = 0; i < s->length; i++, ch++) {
        dr = (s->r - ch->r) / 8;
        dg = (s->g - ch->g) / 8;
        db = (s->b - ch->b) / 8;
        if (dr != 0) {
            ch->r += dr;
        } else if (ch->r < s->r) {
            ch->r++;
        } else if (ch->r > s->r) {
            ch->r--;
        }
        if (dg != 0) {
            ch->g += dg;
        } else if (ch->g < s->g) {
            ch->g++;
        } else if (ch->g > s->g) {
            ch->g--;
        }
        if (db != 0) {
            ch->b += db;
        } else if (ch->b < s->b) {
            ch->b++;
        } else if (ch->b > s->b) {
            ch->b--;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/text", func_800862B0);
#endif

static void func_800863E8(TextString *s) {
    s32 n;
    TextChar *ch;

    if (s != NULL) {
        n = s->length;
        ch = s->chars;
        if (s->next != NULL) {
            func_800863E8(s->next);
        }
        for (; n > 0; n--) {
            ch->b = 0;
            ch->g = 0;
            ch->r = 0;
            EGSDisplayDeleteSprite(gTextCamera, ch++);
        }
        s->chars = NULL;
        s->flags = 0;
        s->next = NULL;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/text", func_80086474);

/* Glyphs (u, v, width) of the special characters, in the font rodata above. */
extern u8 D_800124F8[];
extern u8 D_800124FE[];
extern u8 D_80012501[];
extern u8 D_80012504[];
extern u8 D_80012507[];
extern u8 D_8001250A[];
extern u8 D_8001250D[];
extern u8 D_80012510[];
extern u8 D_80012519[];

#ifdef NON_MATCHING
/*
 * Set up ch for the escape code at str; returns its advance, 0 if unknown.
 * Same code except that the original keeps glyph in a1 (moving ch to a2 on entry).
 */
static s32 func_80086710(u8 *str, TextChar *ch) {
    u8 *glyph;

    glyph = NULL;
    switch (str[0]) {
        case 'c':
        case 'o':
            glyph = D_80012507;
            break;
        case 's':
            switch (str[1]) {
                case 'q':
                    glyph = D_8001250D;
                    break;
                case 'e':
                    glyph = D_80012510;
                    break;
                case 't':
                    glyph = D_80012519;
                    break;
            }
            break;
        case 'x':
            glyph = D_8001250A;
            break;
        case 't':
            glyph = D_80012504;
            break;
        case 'l':
            switch (str[1]) {
                case '1':
                    glyph = D_800124FE;
                    break;
                case '2':
                    glyph = D_800124F8;
                    break;
            }
            break;
        case 'r':
            switch (str[1]) {
                case '1':
                    glyph = D_80012501;
                    break;
                case '2':
                    glyph = D_800124F8 + 3;
                    break;
            }
            break;
    }
    if (glyph == NULL) {
        return 0;
    }
    ch->w = glyph[2];
    ch->h = 8;
    ch->u = glyph[0];
    ch->v = glyph[1];
    return glyph[2] + 2;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/text", func_80086710);
#endif
