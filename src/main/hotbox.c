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
    /* 0x0A */ u8 unk0A[0x22];
} TextChar;

typedef struct TextString {
    /* 0x00 */ TextChar *chars;
    /* 0x04 */ u8 unk04[8];
    /* 0x0C */ u16 count;
    /* 0x0E */ s16 x;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ u8 flags; /* 0x40 right-justify, 0x08 centre */
    /* 0x15 */ u8 unk15[3];
    /* 0x18 */ struct TextString *next;
} TextString;

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
    if (player->healthInfo->maxHealth <= 0) {
        return 0;
    }
    box = *event;
    flags = box->flags;
    type = flags & 0xFF;
    if (type == 0x14) {
        if (gSysInfo.level != 0xE) {
            if (gObjPlayer->healthInfo->maxHealth > 0 && !gPlayerOnFireFlag) {
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

INCLUDE_ASM("asm/main/nonmatchings/hotbox", func_80082F20);
