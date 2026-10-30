#include "common.h"

typedef struct LOSVec {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
} LOSVec;

typedef struct LOSMtx {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} LOSMtx;

/* Window (portal) the ray passed through; copied whole into the result */
typedef struct LOSWindow {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 data[4];
} LOSWindow; /* size 0x14 */

typedef struct LOSHitOwner {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
} LOSHitOwner;

typedef struct LOSHit {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u32 z;
    /* 0x08 */ s32 type; /* 1: game object, 2: window */
    /* 0x0C */ void *box;
    /* 0x10 */ LOSVec normal;
    /* 0x20 */ LOSVec point;
    /* 0x30 */ u8 unk30[0x18];
    /* 0x48 */ LOSHitOwner *owner;
    /* 0x4C */ LOSWindow *window;
} LOSHit; /* size 0x50 */

/* Line of sight query, as filled in by LOSVerifyHits */
typedef struct LOSQuery {
    /* 0x000 */ u8 unk000[0xE8];
    /* 0x0E8 */ u32 hitZ;
    /* 0x0EC */ u8 hit;
    /* 0x0F0 */ LOSVec hitPoint;
    /* 0x100 */ LOSVec hitNormal;
    /* 0x110 */ LOSWindow hitWindow;
} LOSQuery;

typedef struct LOSAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ LOSMtx *matrix;
} LOSAnim;

typedef struct LOSRuntime {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ LOSAnim *anim;
    /* 0x0C */ u8 unk0C[0x16];
    /* 0x22 */ u8 kind;
} LOSRuntime;

typedef struct LOSObjectInfo {
    /* 0x00 */ u8 unk00[0x34];
    /* 0x34 */ LOSRuntime *runtime;
    /* 0x38 */ u8 unk38[0x14];
} LOSObjectInfo; /* size 0x4C, gameObjectsInfo entry */

typedef struct LOSFace {
    /* 0x00 */ struct LOSFace *prev;
    /* 0x04 */ struct LOSFace *next;
} LOSFace;

typedef struct LOSFaceOwner {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ LOSFace *faces;
} LOSFaceOwner;

typedef struct LOSFaceGobj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ LOSFaceOwner *owner;
} LOSFaceGobj;

typedef struct LOSAnimFace {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s16 *v[3];
} LOSAnimFace;

extern LOSObjectInfo *gameObjectsInfo;
extern LOSHit losHitArray[];
extern LOSHit *losHitsSorted[];
extern u32 losHitCount;
extern u32 losSortedCount;
extern s32 losWindowCount;
extern s32 losWindowArray[];
extern s16 losEndPointZ;
extern LOSHit *losBlocker;
extern LOSMtx gEGSIdentityMatrix;

void GobjLOSLoop(void *query, s32 type, u8 arg2);
void LOSSetupGTE(LOSMtx *mtx);
u32 LOSCalculateZ(s32 x, s32 y, s32 z);
void LOSVerifyHits(LOSQuery *query);
void LOSSortHits(void);
void ApplyMatrixLV(LOSMtx *m, LOSVec *in, LOSVec *out);

void WindowLOSLoop(LOSQuery *query) {
    GobjLOSLoop(query, 2, 0);
    LOSVerifyHits(query);
}

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", GobjLOSLoop);

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", iLOSGeometry_IsIntersectPoint2CoplanarAnimFace);

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", LOSGeometry_GetIntersectLine2AnimFacePlane);

void LosTransformFaceToWorld(LOSAnimFace *face, LOSMtx *mtx) {
    LOSVec v;

    v.vx = face->v[0][0];
    v.vy = face->v[0][1];
    v.vz = face->v[0][2];
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[0][0] = v.vx;
    face->v[0][1] = v.vy;
    face->v[0][2] = v.vz;
    v.vx = face->v[1][0];
    v.vy = face->v[1][1];
    v.vz = face->v[1][2];
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[1][0] = v.vx;
    face->v[1][1] = v.vy;
    face->v[1][2] = v.vz;
    v.vx = face->v[2][0];
    v.vy = face->v[2][1];
    v.vz = face->v[2][2];
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[2][0] = v.vx;
    face->v[2][1] = v.vy;
    face->v[2][2] = v.vz;
}

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", LOSSortHits);

void LOSVerifyHits(LOSQuery *query) {
    u32 i;
    LOSHit *hit;
    LOSHit *first;

    losWindowCount = 0;
    LOSSortHits();
    if (losSortedCount != 0 && losHitsSorted[0]->z < losEndPointZ) {
        first = losHitsSorted[0];
        query->hit = 1;
        query->hitZ = first->z;
        query->hitWindow = *first->window;
        query->hitPoint = first->point;
        query->hitNormal = first->normal;
    }
    for (i = 0; i < losSortedCount; i++) {
        hit = losHitsSorted[i];
        if (hit->z > losEndPointZ || hit->type != 2) {
            losBlocker = hit;
            return;
        }
        if (hit->z < losEndPointZ + 20) {
            losWindowArray[losWindowCount++] = hit->window->id;
        }
    }
}

s32 AddFaceToFaceList(LOSFaceGobj *gobj, LOSFace *face) {
    face->prev = NULL;
    face->next = gobj->owner->faces;
    gobj->owner->faces = face;
    if (face->next != NULL) {
        face->next->prev = face;
    }
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", iCobjHandleCollisionBox_RegisterBox);

s32 GetFuzzyDistancePoint2Point(LOSVec *a, LOSVec *b) {
    s32 dx;
    s32 dy;
    s32 dz;
    s32 t;

    dx = a->vx - b->vx;
    if (dx < 0) {
        dx = b->vx - a->vx;
    }
    dy = a->vy - b->vy;
    if (dy < 0) {
        dy = b->vy - a->vy;
    }
    dz = a->vz - b->vz;
    if (dz < 0) {
        dz = b->vz - a->vz;
    }
    if (dx < dy) {
        t = dx;
        dx = dy;
        dy = t;
    }
    if (dx < dz) {
        t = dx;
        dx = dz;
        dz = t;
    }
    return dx + ((dy + dz) >> 2);
}

INCLUDE_ASM("asm/main/nonmatchings/gtegeo", GobjCollideLoop);
