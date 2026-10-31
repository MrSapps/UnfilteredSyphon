#include "common.h"
#include "gameobj.h"

SDATA_LOCAL(D_801101B0)
SDATA_LOCAL(gTextCamera)

/* Rotated box of a display sector (0x48 bytes) */
typedef struct RotBox {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 unk01[0x43];
    /* 0x44 */ s32 data; /* type 0x10: generator id | 0x80000000, type 0x12: self-relative offset */
} RotBox;

/* Hotbox (trigger box) as seen by HotboxCallback */
typedef struct Hotbox {
    /* 0x00 */ u32 flags; /* low byte: type, 0x200: player inside */
    /* 0x04 */ u8 unk04[0x40];
    /* 0x44 */ s32 data;
} Hotbox;

typedef struct HotboxFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 x, y, z;
} HotboxFrame;

typedef struct HotboxAnim {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ HotboxFrame **frames;
} HotboxAnim;

typedef struct {
    s32 vx, vy, vz;
} HotboxVector;

/* One glyph of a text string (0x2C bytes) */
typedef struct TextChar {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ u16 w;
    /* 0x0A */ u8 unk0A[0xA];
    /* 0x14 */ u8 r;
    /* 0x15 */ u8 g;
    /* 0x16 */ u8 b;
    /* 0x17 */ u8 unk17[0x15];
} TextChar;

typedef struct TextString {
    /* 0x00 */ TextChar *chars;
    /* 0x04 */ u32 startTick;
    /* 0x08 */ u32 endTick;
    /* 0x0C */ u16 count;
    /* 0x0E */ s16 x;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ u8 flags; /* 0x40 right-justify, 0x08 centre */
    /* 0x15 */ u8 unk15;
    /* 0x16 */ u16 marker; /* markers: gameObjectsInfo index of the labelled object */
    /* 0x18 */ struct TextString *next;
} TextString;

/* Flat line primitive (text.c's TextLine, 0x18 bytes); markers use two each. */
typedef struct TextLine {
    /* 0x00 */ s32 tag;
    /* 0x04 */ u8 unk04[8];
    /* 0x0C */ u32 color;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
} TextLine;
extern TextLine D_801211E4[];

#define HALF(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))

/* Bounding box at the start of a model. */
typedef struct HotboxModel {
    /* 0x00 */ HotboxVector min;
    /* 0x0C */ s32 pad0C;
    /* 0x10 */ HotboxVector max;
} HotboxModel;

typedef struct HotboxNPC {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 flags; /* 0x200: show marker */
} HotboxNPC;

extern void **gCamera;
extern void *gTextCamera;
u8 LevelGameObjectHasModel(s32 id);
void EGSFrameTransformVector(HotboxVector *in, void *frame, s32 arg2, HotboxVector *out);
void CameraGetDisplay(void *camera, void **display);
void EGSDisplaySetupTransformWorldToScreen(void *display);
void EGSDisplayTransformWorldToScreen(HotboxVector *in, HotboxVector *out);
void EGSDisplayAddPoly(void *camera, void *poly);
void EGSDisplayDeletePoly(void *camera, void *poly);
void func_80085024(TextString *s);
void func_80082E70(TextString *text, s32 x, s32 y);

extern struct {
    s16 level;
} gSysInfo;
extern u8 gLevelReady;
extern u8 gElectricFenceFlag;
extern u8 gPlayerOnFireFlag;
extern GameObjectRuntime *gObjPlayer;
extern s32 gPlayerOffset;
int rand(void);
void ParticleSparkFence(HotboxVector *pos, s32 color);
void DamageGOBJ(s32 a, s32 b, s32 id, u8 damage, s32 type);
void CASRequestAction(s32 id, s32 action);
void GameCameraSetModeFixed(s32 on, s32 arg1, s32 camera, s32 arg3, s32 arg4);
void NPCEnableGenerator(s32 id);
void printf();

extern u32 D_801101B0; /* tick of the last text redraw request, -1 = none */
extern s32 gSystemTick;

void EGSDisplayGetSectorRotBoxList(void *sector, u32 *count, RotBox **boxes);
void NPCInitHotBoxGenerator(s32 id);
u8 SysIsPaused(void);
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);
void EGSDisplayQueueInterrupt(void (*fn)(void), s32 arg1, s32 arg2);
void TextUpdate(void);

void HotboxInit(void *sector) {
    u32 count;
    RotBox *boxes;
    u32 i;

    EGSDisplayGetSectorRotBoxList(sector, &count, &boxes);
    for (i = 0; i < count; i++) {
        RotBox *box = &boxes[i];
        s32 type = box->type;

        if (type == 0x10 && (box->data & 0x80000000)) {
            NPCInitHotBoxGenerator(box->data & 0x7FFFFFFF);
        }
        if (type == 0x12) {
            box->data = (s32)((u8 *)box + (box->data + 0x44));
        }
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/hotbox", D_800123A8);

s32 HotboxCallback(Hotbox **event) {
    GameObjectRuntime *player = gameObjectsInfo[gPlayerOffset].runtime;
    Hotbox *box;
    u32 flags;
    u32 type;
    s32 spark;
    s32 spark2;
    HotboxVector pos;

    if (!gLevelReady) {
        return 0;
    }
    if (player->healthInfo->health <= 0) {
        return 0;
    }
    box = *event;
    flags = box->flags;
    type = flags & 0xFF;
    if (type == 0x14) {
        if (gSysInfo.level != 0xE) {
            if (gObjPlayer->healthInfo->health > 0 && !gPlayerOnFireFlag) {
                SysPostMessage(0xD, 4, 0x29A, gPlayerOffset, 0, 0, 0, 0);
            }
            return 1;
        }
        if (!gElectricFenceFlag) {
            return 1;
        }
        if (gSystemTick & 3) {
            return 1;
        }
        spark = rand() % 15;
        pos.vx = ((HotboxAnim *)player->anim)->frames[spark]->x;
        pos.vy = ((HotboxAnim *)player->anim)->frames[spark]->y;
        pos.vz = ((HotboxAnim *)player->anim)->frames[spark]->z;
        pos.vy = -pos.vy;
        ParticleSparkFence(&pos, 0xFF8080);
        spark2 = rand() % 15;
        if (spark != spark2) {
            pos.vx = ((HotboxAnim *)player->anim)->frames[spark2]->x;
            pos.vy = ((HotboxAnim *)player->anim)->frames[spark2]->y;
            pos.vz = ((HotboxAnim *)player->anim)->frames[spark2]->z;
            pos.vy = -pos.vy;
            ParticleSparkFence(&pos, 0xFF8080);
        }
        DamageGOBJ(-1, -1, player->id, box->data, 0x10);
        return 1;
    }
    if (type == 0x11 || type == 0x18) {
        if (flags & 0x200) {
            box->flags = flags & ~0x200;
            CASRequestAction(player->id, 0x37);
            return 1;
        }
        box->flags = flags | 0x200;
        if (type == 0x11) {
            CASRequestAction(player->id, 0x35);
            return 1;
        }
        printf("NO_CLIMB_DOWN\n");
        CASRequestAction(player->id, 0x36);
        return 1;
    }
    if (type == 0x12) {
        if (flags & 0x200) {
            box->flags = flags & ~0x200;
            GameCameraSetModeFixed(0, 0, box->data, 0, 1);
        } else {
            box->flags = flags | 0x200;
            GameCameraSetModeFixed(1, 0, box->data, 0, 1);
        }
        return 1;
    }
    if (type == 0x10) {
        if (flags & 0x200) {
            box->flags = flags & ~0x200;
            if (box->data >= 0) {
                SysPostMessage(0x13, 4, 0xFFFE, box->data, 0, 0, 0, 0);
            }
        } else {
            box->flags = flags | 0x200;
            if (box->data < 0) {
                NPCEnableGenerator(box->data & 0x7FFFFFFF);
                return 1;
            }
            SysPostMessage(0x12, 4, 0xFFFE, box->data, 0, 0, 0, 0);
        }
    }
    return 1;
}

/* rest of D_800123C5 (the tail of the string above is referenced from text.c) */
const u8 D_800123C8[16] = {
    0x38, 0x10, 0x06, 0x20, 0x18, 0x05, 0x28, 0x18, 0x05, 0x30, 0x18, 0x06, 0x38, 0x18, 0x04, 0x20,
};

/* global: called from text.c */
void func_80082C4C(void) {
    if (gSystemTick + 1 < D_801101B0) {
        if (SysIsPaused()) {
            return;
        }
        if (D_801101B0 != -1) {
            EGSDisplayQueueInterrupt(TextUpdate, 0, 0xE0C00000);
        }
        SysPostMessage(0x1F, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
        D_801101B0 = gSystemTick + 1;
    }
}

/* Justify each line (run of glyphs with increasing x) of a string. */
/* global: called from text.c */
void func_80082CE8(TextString *text) {
    TextChar *start = text->chars;
    TextChar *end;
    s32 offset;

    for (end = start; start < &text->chars[text->count]; end++) {
        while (end[1].x > end->x && end < &text->chars[text->count - 1]) {
            end++;
        }
        if (text->flags & 0x40) {
            offset = text->x - (end->x + end->w);
        } else if (text->flags & 8) {
            offset = text->x - (((end->x + end->w) - start->x) / 2 + start->x);
        } else {
            offset = text->x - start->x;
        }
        if (offset != 0) {
            for (; start <= end; start++) {
                start->x += offset;
            }
        } else {
            start = end + 1;
        }
    }
}

/* global: called from text.c */
void func_80082E70(TextString *text, s32 x, s32 y) {
    s32 dy;
    s32 i;

    text->x = x;
    dy = y - text->chars[0].y;
    func_80082CE8(text);
    text->chars[0].y = y;
    for (; text != NULL; text = text->next) {
        for (i = 1; i < text->count; i++) {
            text->chars[i].y += dy;
        }
    }
}

#ifdef NON_MATCHING
/* regalloc of width/room/arrow (a1/a0/a2 vs a2/a1/a3), (x - 6) + arrow reassociated, &line[1] not kept in s0 */
/*
 * Place marker string s (index-th marker) over the object it labels: project
 * the object (or the top of its model) to the screen, move the string there
 * clamped to the screen edges, and draw the two-line pointer from it to the
 * object while the string is shown. Off-screen markers are parked below.
 */
void func_80082F20(TextString *s, s32 index) {
    HotboxVector pos;
    HotboxVector at;
    HotboxVector head;
    void *display;
    TextChar *chars;
    GameObjectRuntime *gobj;
    GameObjectInfo *info;
    HotboxModel *model;
    TextLine *line;
    s32 width;
    s32 room;
    s32 arrow;
    s32 x;
    s32 t;
    u32 color;

    model = NULL;
    chars = s->chars;
    gobj = gameObjectsInfo[s->marker].runtime;
    line = &D_801211E4[index * 2];
    if (LevelGameObjectHasModel(gobj->id)) {
        model = (HotboxModel *)gobj->anim->model;
        pos.vx = ((HotboxFrame *)gobj->anim->frame)->x;
        pos.vy = ((HotboxFrame *)gobj->anim->frame)->y;
        pos.vz = ((HotboxFrame *)gobj->anim->frame)->z;
        pos.vy = -pos.vy;
        if (gobj->npcState == 2) {
            if (!(((HotboxNPC *)gobj->npc)->flags & 0x200)) {
                func_80085024(s);
                return;
            }
            head.vz = 0;
            head.vy = 0;
            head.vx = 0;
        } else {
            head.vx = model->min.vx + model->max.vx;
            head.vy = model->min.vy + model->max.vy;
            head.vz = model->min.vz + model->max.vz;
            head.vx = HALF(head.vx);
            head.vy = HALF(head.vy);
            head.vz = HALF(head.vz);
            if (head.vx != 0 || head.vy != 0 || head.vz != 0) {
                head.vy = -head.vy;
                EGSFrameTransformVector(&head, gobj->anim->frame, 0, &head);
                CameraGetDisplay(gCamera, &display);
                EGSDisplaySetupTransformWorldToScreen(display);
            }
            gobj = NULL;
            pos.vx += head.vx;
            pos.vy += head.vy;
            pos.vz += head.vz;
        }
    } else {
        info = &gameObjectsInfo[gobj->id];
        pos.vx = info->matrix.t[0];
        pos.vy = info->matrix.t[1];
        gobj = NULL;
        pos.vz = info->matrix.t[2];
    }
    EGSDisplayTransformWorldToScreen(&pos, &pos);
    if (pos.vz > 0) {
        if (gobj != NULL) {
            head.vx = ((HotboxFrame *)gobj->anim->frame)->x;
            head.vy = ((HotboxFrame *)gobj->anim->frame)->y;
            head.vz = ((HotboxFrame *)gobj->anim->frame)->z;
            head.vy = -head.vy;
            head.vy -= model->min.vy;
            EGSDisplayTransformWorldToScreen(&head, &head);
        } else {
            head.vz = pos.vz;
            head.vx = pos.vx;
            head.vy = pos.vy;
        }
        width = chars[s->count - 1].x + chars[s->count - 1].w - chars[0].x;
        if (head.vz < 0) {
            head.vz = 0;
        }
        if (head.vx + (t = width + 8) > 0x98) {
            /* off the right edge: right-justify and shorten the pointer */
            room = 0xA0 - pos.vx;
            arrow = room * 6 / (width + 16);
            at.vx = 0x98;
            if (arrow < 0) {
                arrow = 0;
            }
            x = width * room / (width + 16);
            if (x < 0) {
                x = 0;
            }
            x = 0x98 - x;
            s->flags |= 0x40;
        } else {
            s->flags &= ~0x40;
            if (head.vx + 8 < -0x98) {
                at.vx = -0x98;
            } else {
                at.vx = head.vx + 8;
            }
            x = at.vx;
            arrow = 6;
        }
        if (head.vy + 8 > 0x5A) {
            at.vy = 0x5A;
        } else if (head.vy + 8 < -0x5A) {
            at.vy = -0x5A;
        } else {
            at.vy = head.vy + 8;
        }
        if (head.vy > 0x52) {
            head.vy = (pos.vy < 0x52) ? 0x52 : pos.vy;
        }
        if (s->endTick > gSystemTick) {
            line[0].xy0 = (u16)head.vx | (-head.vy << 16);
            line[0].xy1 = (x & 0xFFFF) | (-at.vy << 16);
            line[1].xy0 = ((x + arrow) & 0xFFFF) | (-at.vy << 16);
            line[1].xy1 = ((x - 6 + arrow) & 0xFFFF) | (-at.vy << 16);
            color = ((chars[0].b << 16) + (chars[0].g << 8) + chars[0].r) | 0x40000000;
            line[1].color = color;
            line[0].color = color;
            if (line[0].tag == 0) {
                EGSDisplayAddPoly(gTextCamera, &line[0]);
                EGSDisplayAddPoly(gTextCamera, &line[1]);
            }
        } else if (line[0].tag != 0) {
            EGSDisplayDeletePoly(gTextCamera, &line[0]);
            EGSDisplayDeletePoly(gTextCamera, &line[1]);
        }
        at.vy += 8;
    } else {
        if (gSystemTick - s->startTick < 600 && s->endTick < (u32)-1) {
            s->endTick++;
        }
        at.vy = 300;
        if (line[0].tag != 0) {
            EGSDisplayDeletePoly(gTextCamera, &line[0]);
            EGSDisplayDeletePoly(gTextCamera, &line[1]);
        }
    }
    func_80082E70(s, at.vx, -at.vy);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/hotbox", func_80082F20);
#endif
