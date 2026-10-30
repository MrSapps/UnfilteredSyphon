#include "common.h"

/* shadow.c's small data, still assembled from asm */
SDATA_LOCAL(mShadowRequestList)

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

typedef struct ShadowRect {
    s16 x, y, w, h;
} ShadowRect;

/* One shadow polygon (0x30 bytes); data is nonzero while it is in the display list */
typedef struct ShadowPoly {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[7];
    /* 0x0F */ u8 flags;
    /* 0x10 */ u8 unk10[0x20];
} ShadowPoly;

#define SHADOW_POOL_SIZE 6
#define SHADOW_POLYS 4

typedef struct Shadow {
    /* 0x00 */ ShadowPoly polys[SHADOW_POLYS];
} Shadow; /* size 0xC0 */

typedef struct ShadowModel {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
} ShadowModel;

typedef struct ShadowBox {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ s16 z;
} ShadowBox;

typedef struct ShadowAnim {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ ShadowModel *model;
    /* 0x14 */ u8 unk14[8];
    /* 0x1C */ ShadowBox *box;
} ShadowAnim;

typedef struct ShadowGobj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ ShadowAnim *anim;
} ShadowGobj;

extern EGSListNode *mShadowRequestList;
extern Shadow mShadowPool[SHADOW_POOL_SIZE];
extern u8 mShadowTexture[];
extern void **gCamera;

void EGSListAdd(EGSListNode **list, void *data);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
void EGSDisplayDeletePoly(void *camera, void *poly);
void LoadImage(ShadowRect *rect, void *data);
typedef struct ShadowVec {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
} ShadowVec;

/* Shadow caster state hanging off the object at 0x1A8 */
typedef struct ShadowCaster {
    /* 0x000 */ u8 unk000[0x108];
    /* 0x108 */ u8 matrix[0x14];
    /* 0x11C */ s32 pos[3];
} ShadowCaster;

typedef struct ShadowObjState {
    /* 0x000 */ ShadowVec pos;
    /* 0x010 */ u8 unk010[0x11C];
    /* 0x12C */ s32 groundY;
    /* 0x130 */ u8 unk130[0x78];
    /* 0x1A8 */ ShadowCaster *caster;
} ShadowObjState;

typedef struct ShadowObj {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ ShadowObjState *state;
} ShadowObj;

extern s32 gSniperMode;
extern void *gSniperCam;

void EGSDisplaySetupTransformLocalToScreen(void *camera, void *matrix);
void EGSDisplayTransformWorldToScreen(ShadowVec *in, ShadowVec *out);
u8 func_800918C0(ShadowObj *obj, ShadowVec *center, ShadowVec *points, s32 count); /* still asm */
u8 func_800915DC(ShadowObj *obj, u8 *rgb);                                           /* still asm */
static void func_800916CC(Shadow *shadow, ShadowVec *center, ShadowVec *points, s32 count, u8 *rgb);

/* Texture description handed to EGSDisplayBuildPoly4FT */
typedef struct ShadowTex {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ s32 mode;
    /* 0x10 */ ShadowRect rect;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ s16 clutX;
    /* 0x1E */ s16 clutY;
} ShadowTex;

void EGSDisplayBuildPoly4FT(Shadow *shadow, s32 xy0, s32 xy1, s32 xy2, s32 xy3, s32 color, ShadowTex *tex, s32 arg7);
void EGSDisplayAddPoly(void *camera, void *poly);
static u8 func_80092468(ShadowObj *obj, Shadow *shadow, s32 count);

INCLUDE_ASM("asm/main/nonmatchings/shadow", ShadowInit);

INCLUDE_ASM("asm/main/nonmatchings/shadow", func_800915DC);

static void func_800916CC(Shadow *shadow, ShadowVec *center, ShadowVec *points, s32 count, u8 *rgb) {
    ShadowTex tex;
    ShadowVec unused; /* allocated but never used */
    s32 z;
    s32 i;

    if (count == 4) {
        i = 0;
        if (points[0].vz > 0 && points[1].vz > 0 && points[2].vz > 0 && points[3].vz > 0) {
            tex.mode = 2;
            tex.rect.x = 0x300;
            tex.rect.y = 0xA0;
            tex.rect.w = 0x10;
            tex.rect.h = 0x20;
            tex.clutX = 0x300;
            tex.clutY = 0x1E3;
            EGSDisplayBuildPoly4FT(shadow, points[0].vx - (points[0].vy << 16), points[1].vx - (points[1].vy << 16),
                                   points[3].vx - (points[3].vy << 16), points[2].vx - (points[2].vy << 16),
                                   ((rgb[2] >> 2) << 16) + ((rgb[1] >> 2) << 8) + (rgb[0] >> 2), &tex, 2);
            shadow->polys[0].flags |= 2;
            z = (center->vz - 0x80) * 3 / 16;
            if (z < 0) {
                z = 0;
            }
            shadow->polys[0].z = z;
            if (shadow->polys[0].data == 0) {
                EGSDisplayAddPoly(*gCamera, &shadow->polys[0]);
            }
            i = 1;
        }
        for (; i < SHADOW_POLYS; i++) {
            if (shadow->polys[i].data != 0) {
                EGSDisplayDeletePoly(*gCamera, &shadow->polys[i]);
            }
        }
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/shadow", D_80013360);

INCLUDE_ASM("asm/main/nonmatchings/shadow", func_800918C0);

static u8 func_80092468(ShadowObj *obj, Shadow *shadow, s32 count) {
    ShadowVec center;
    ShadowVec pos;
    ShadowVec screenCenter;
    ShadowVec points[8];
    ShadowVec screenPoints[8];
    u8 rgb[4];
    ShadowCaster *caster;
    void *camera;
    s32 i;

    if (obj != NULL && obj->state != NULL && obj->state->caster != NULL && func_800918C0(obj, &center, points, count)) {
        if (!func_800915DC(obj, rgb)) {
            return 0;
        }
        pos = obj->state->pos;
        pos.vy = obj->state->groundY;
        caster = obj->state->caster;
        caster->pos[0] = pos.vx;
        caster->pos[1] = pos.vy;
        caster->pos[2] = pos.vz;
        if (gSniperMode != 2 || (camera = gSniperCam) == NULL) {
            camera = *gCamera;
        }
        EGSDisplaySetupTransformLocalToScreen(camera, caster->matrix);
        EGSDisplayTransformWorldToScreen(&center, &screenCenter);
        if (screenCenter.vz > 0) {
            for (i = 0; i < count; i++) {
                EGSDisplayTransformWorldToScreen(&points[i], &screenPoints[i]);
            }
            func_800916CC(shadow, &screenCenter, screenPoints, count, rgb);
            return 1;
        }
    }
    return 0;
}

static void func_80092620(ShadowPoly *polys) {
    s32 i;

    for (i = 0; i < SHADOW_POLYS; i++) {
        if (polys[i].data != 0) {
            EGSDisplayDeletePoly(*gCamera, &polys[i]);
        }
    }
}

void ShadowProcess(void) {
    ShadowRect rect;
    EGSListNode *node;
    EGSListNode *next;
    Shadow *shadow;
    s32 count;

    rect.x = 0x300;
    rect.y = 0xA0;
    rect.w = 0x10;
    rect.h = 0x20;
    LoadImage(&rect, mShadowTexture);
    node = mShadowRequestList;
    count = 0;
    if (node != NULL) {
        shadow = mShadowPool;
        do {
            next = node->next;
            if (count < SHADOW_POOL_SIZE && func_80092468(node->data, shadow, SHADOW_POLYS)) {
                shadow++;
                count++;
            }
            EGSListRemove(&mShadowRequestList, node);
            node = next;
        } while (node != NULL);
    }
    for (; count < SHADOW_POOL_SIZE; count++) {
        func_80092620(mShadowPool[count].polys);
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/shadow", D_80013414);

INCLUDE_ASM("asm/main/nonmatchings/shadow", ShadowCalculateProjectionMatrix);

void ShadowRequestDraw(void *gobj) {
    EGSListAdd(&mShadowRequestList, gobj);
}
