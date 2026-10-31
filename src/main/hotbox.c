#include "common.h"
#include "gameobj.h"

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

extern s32 gSystemTick;

void EGSDisplayGetSectorRotBoxList(void *sector, u32 *count, RotBox **boxes);
void NPCInitHotBoxGenerator(s32 id);
u8 SysIsPaused(void);
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);

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

static const s32 D_800123A8[4] = { 1, 1, 1, 0 };

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
