#include "common.h"
#include "scratch.h"

typedef struct LOSVector {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
} LOSVector;

typedef struct LOSMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} LOSMatrix;

typedef struct LOSModel {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
} LOSModel;

typedef struct LOSAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags;
    /* 0x09 */ u8 unk09[3];
    /* 0x0C */ LOSMatrix *matrix;
    /* 0x10 */ LOSModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ LOSMatrix **headMatrix;
} LOSAnim;

typedef struct LOSGobj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ LOSAnim *anim;
    /* 0x0C */ void *unk0C;
    /* 0x10 */ u8 unk10[0x12];
    /* 0x22 */ u8 kind;
} LOSGobj;

/* Line of sight request */
typedef struct LOSRequest {
    /* 0x00 */ LOSVector *from;
    /* 0x04 */ LOSVector *to;
    /* 0x08 */ LOSGobj *fromObj;
    /* 0x0C */ LOSGobj *toObj;
    /* 0x10 */ LOSVector *dir;
    /* 0x14 */ void *angles;
    /* 0x18 */ s32 distance;
    /* 0x1C */ u8 *noHit;
    /* 0x20 */ LOSVector *hitPos;
    /* 0x24 */ LOSVector *hitNormal;
    /* 0x28 */ struct LOSHitInfo *hitInfo;
    /* 0x2C */ u8 *windowHit;
    /* 0x30 */ struct LOSHitInfo *windowInfo;
} LOSRequest;

s32 EGSMathFIXMul(s32 a, s32 b);
void EGSGeometry_VectorRotateYXZ(LOSVector *in, void *angles, LOSVector *out);
u8 _LOSPoint2Point(LOSRequest *req);

/* Straight ahead, rotated by the request's angles */
static const LOSVector D_80011B80 = { 0, 0, 0x1000, 0 };

void LOSGetGobjLOSTestPoint(LOSGobj *gobj, LOSVector *out) {
    if ((gobj->kind == 1 || gobj->kind == 2) && (gobj->anim->model->flags & 0x400000) &&
        !(gobj->anim->flags & 0x10) && gobj->unk0C != NULL) {
        out->vx = (*gobj->anim->headMatrix)->t[0];
        out->vy = (*gobj->anim->headMatrix)->t[1];
        out->vz = (*gobj->anim->headMatrix)->t[2];
        out->vy = -out->vy - 8;
    } else {
        out->vx = gobj->anim->matrix->t[0];
        out->vy = gobj->anim->matrix->t[1];
        out->vz = gobj->anim->matrix->t[2];
        out->vy = -out->vy;
    }
}

/* What a line-of-sight test hit (0x14 bytes) */
typedef struct LOSHitInfo {
    /* 0x00 */ void *obj;
    /* 0x04 */ u8 kind; /* 2: window */
    /* 0x05 */ u8 unk05;
    /* 0x06 */ u8 unk06;
    /* 0x07 */ u8 unk07;
    /* 0x08 */ u8 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
} LOSHitInfo;

/* State of one GTELOSLoop / WindowLOSLoop walk */
typedef struct LOSContext {
    /* 0x00 */ u16 sector;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u8 noHitPos;
    /* 0x09 */ u8 unk09[0x97];
    /* 0xA0 */ LOSGobj *ignore;
    /* 0xA4 */ LOSGobj *target;
    /* 0xA8 */ u8 unkA8;
    /* 0xAC */ LOSVector from;
    /* 0xBC */ LOSVector delta;
    /* 0xCC */ LOSVector mid;
    /* 0xDC */ s32 radius;
    /* 0xE0 */ s32 length;
    /* 0xE4 */ s32 halfLength;
    /* 0xE8 */ s32 nearest;
    /* 0xEC */ u8 hit;
    /* 0xF0 */ LOSVector hitPos;
    /* 0x100 */ LOSVector hitNormal;
    /* 0x110 */ LOSHitInfo hitInfo;
} LOSContext; /* size 0x124 */

extern u16 gGabeSector;
extern void **gCamera;
extern s32 losWindowCount;
extern void *losWindowArray[];
extern LOSHitInfo dummyCobjInfo;

s32 EGSMath_VectorModulus(LOSVector *v, s32 *out);
void GTELOSLoop(LOSContext *ctx, s32 arg1, s32 arg2);
void WindowLOSLoop(LOSContext *ctx);

#define HALF(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))

#ifdef NON_MATCHING
/* 15 lines left (r4D; ctx.ignore/ctx.target right after ctx.mid as in _LOSPoint2PointCollide).
 * Open: the original has a dead 'addiu s0,sp,0x10' in the HALF(length) branch (bltz + j layout;
 * `length < 0 ? .. : ..` and an if/else don't change it) and sets the GTELOSLoop args after the
 * hitInfo stores (see _LOSPoint2PointCollide). */
u8 _LOSPoint2Point(LOSRequest *req) {
    LOSVector from;
    LOSVector delta;
    LOSVector half;
    LOSVector mid;
    LOSVector cam;
    LOSContext ctx;
    s32 length;
    s32 halfLength;

    from = *req->from;
    delta.vx = req->to->vx - req->from->vx;
    delta.vy = req->to->vy - req->from->vy;
    delta.vz = req->to->vz - req->from->vz;
    EGSMath_VectorModulus(&delta, &length);
    halfLength = HALF(length);
    half.vx = HALF(delta.vx);
    half.vy = HALF(delta.vy);
    half.vz = HALF(delta.vz);
    mid.vx = from.vx + half.vx;
    mid.vy = from.vy + half.vy;
    mid.vz = from.vz + half.vz;
    cam.vx = (*(LOSMatrix **)*gCamera)->t[0];
    cam.vy = (*(LOSMatrix **)*gCamera)->t[1];
    cam.vz = (*(LOSMatrix **)*gCamera)->t[2];
    cam.vy = -cam.vy;

    ctx.unk04 = 0;
    ctx.sector = gGabeSector;
    ctx.noHitPos = req->hitPos == NULL;
    ctx.from = from;
    ctx.delta = delta;
    ctx.mid = mid;
    ctx.ignore = req->fromObj;
    ctx.target = req->toObj;
    ctx.radius = halfLength + 0xF00;
    ctx.length = length;
    ctx.halfLength = halfLength;
    ctx.unkA8 = 1;
    ctx.nearest = 0x7FFFFFFF;
    ctx.hit = 0;
    ctx.hitPos = *req->to;
    if (req->toObj != NULL) {
        ctx.hitInfo.obj = req->toObj;
        ctx.hitInfo.kind = 2;
    } else {
        ctx.hitInfo.obj = NULL;
        ctx.hitInfo.kind = 0;
    }
    ctx.hitInfo.unk05 = 0;
    ctx.hitInfo.unk06 = 0;
    ctx.hitInfo.unk07 = 0;
    ctx.hitInfo.unk0C = 0;
    ctx.hitInfo.unk08 = 0;
    ctx.hitInfo.unk10 = 0;
    GTELOSLoop(&ctx, 0, 0);
    if (req->noHit != NULL) {
        *req->noHit = ctx.hit != 1;
    }
    if (req->hitPos != NULL) {
        *req->hitPos = ctx.hitPos;
        if (req->hitNormal != NULL && ctx.hit == 1) {
            *req->hitNormal = ctx.hitNormal;
        }
        if (req->hitInfo != NULL) {
            *req->hitInfo = ctx.hitInfo;
        }
    }
    if (req->windowHit != NULL && req->windowInfo != NULL) {
        ctx.hit = 0;
        ctx.hitInfo.obj = NULL;
        ctx.hitInfo.kind = 0;
        ctx.hitInfo.unk05 = 0;
        ctx.hitInfo.unk06 = 0;
        ctx.hitInfo.unk07 = 0;
        ctx.hitInfo.unk0C = 0;
        ctx.hitInfo.unk08 = 0;
        ctx.hitInfo.unk10 = 0;
        SCRATCH_CALL1(WindowLOSLoop, &ctx);
        *req->windowHit = losWindowCount != 0;
        dummyCobjInfo.obj = losWindowArray[0];
        dummyCobjInfo.kind = 2;
        *req->windowInfo = dummyCobjInfo;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPoint2Point);
#endif

/* The first product is held in a temp until the next call's arguments are loaded. */
u8 _LOSPointAlongDirection(LOSRequest *req) {
    LOSVector to;
    LOSVector step;
    s32 x;
    s32 y;
    s32 dist;

    x = EGSMathFIXMul(req->dir->vx, req->distance);
    y = req->dir->vy;
    dist = req->distance;
    step.vx = x;
    step.vy = EGSMathFIXMul(y, dist);
    step.vz = EGSMathFIXMul(req->dir->vz, req->distance);
    to.vx = req->from->vx + step.vx;
    to.vy = req->from->vy + step.vy;
    to.vz = req->from->vz + step.vz;
    req->to = &to;
    return _LOSPoint2Point(req);
}

/* The first product is held in a temp until the next call's arguments are loaded. */
u8 _LOSPointAlongAngleDirectionYXZ(LOSRequest *req) {
    LOSVector to;
    LOSVector dir = D_80011B80;
    s32 x;
    s32 dist;

    EGSGeometry_VectorRotateYXZ(&dir, req->angles, &dir);
    x = EGSMathFIXMul(dir.vx, req->distance);
    dist = req->distance;
    dir.vx = x;
    dir.vy = EGSMathFIXMul(dir.vy, dist);
    dir.vz = EGSMathFIXMul(dir.vz, req->distance);
    to.vx = req->from->vx + dir.vx;
    to.vy = req->from->vy + dir.vy;
    to.vz = req->from->vz + dir.vz;
    req->to = &to;
    req->toObj = (LOSGobj *)-1;
    return _LOSPoint2Point(req);
}

u8 _LOSGobj2Gobj(LOSRequest *req) {
    LOSVector from;
    LOSVector to;

    if (req->fromObj == NULL || req->toObj == NULL || req->fromObj->anim == NULL || req->toObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    LOSGetGobjLOSTestPoint(req->toObj, &to);
    req->from = &from;
    req->to = &to;
    return _LOSPoint2Point(req);
}

u8 _LOSGobj2Point(LOSRequest *req) {
    LOSVector from;

    if (req->fromObj == NULL || req->fromObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    req->from = &from;
    return _LOSPoint2Point(req);
}

/* The first product is held in a temp until the next call's arguments are loaded. */
u8 _LOSGobjAlongDirection(LOSRequest *req) {
    LOSVector from;
    LOSVector to;
    LOSVector step;
    s32 x;
    s32 y;
    s32 dist;

    if (req->fromObj == NULL || req->fromObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    x = EGSMathFIXMul(req->dir->vx, req->distance);
    y = req->dir->vy;
    dist = req->distance;
    step.vx = x;
    step.vy = EGSMathFIXMul(y, dist);
    step.vz = EGSMathFIXMul(req->dir->vz, req->distance);
    to.vx = from.vx + step.vx;
    to.vy = from.vy + step.vy;
    to.vz = from.vz + step.vz;
    req->from = &from;
    req->to = &to;
    return _LOSPoint2Point(req);
}

/* The first product is held in a temp until the next call's arguments are loaded. */
u8 _LOSGobjAlongAngleDirectionYXZ(LOSRequest *req) {
    LOSVector from;
    LOSVector to;
    LOSVector dir = D_80011B80;
    s32 x;
    s32 dist;

    if (req->fromObj == NULL || req->fromObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    EGSGeometry_VectorRotateYXZ(&dir, req->angles, &dir);
    x = EGSMathFIXMul(dir.vx, req->distance);
    dist = req->distance;
    dir.vx = x;
    dir.vy = EGSMathFIXMul(dir.vy, dist);
    dir.vz = EGSMathFIXMul(dir.vz, req->distance);
    to.vx = from.vx + dir.vx;
    to.vy = from.vy + dir.vy;
    to.vz = from.vz + dir.vz;
    req->from = &from;
    req->to = &to;
    return _LOSPoint2Point(req);
}

#ifdef NON_MATCHING
/* 4 lines left (r4D): ctx.ignore/ctx.target right after ctx.mid fixed the req loads (they
 * must precede the ctx.unkA8/ctx.hit sb's: QImode stores conflict with every load). Still open:
 * sched1 hoists the a0/a1 GTELOSLoop arg sets above the hitInfo stores (sched2 then keeps that
 * LUID order), the original has them after the stores with a2 in the jal slot. */
u8 _LOSPoint2PointCollide(LOSRequest *req) {
    LOSVector from;
    LOSVector delta;
    LOSVector half;
    LOSVector mid;
    LOSContext ctx;
    s32 length;
    s32 halfLength;

    from = *req->from;
    delta.vx = req->to->vx - req->from->vx;
    delta.vy = req->to->vy - req->from->vy;
    delta.vz = req->to->vz - req->from->vz;
    EGSMath_VectorModulus(&delta, &length);
    halfLength = HALF(length);
    half.vx = HALF(delta.vx);
    half.vy = HALF(delta.vy);
    half.vz = HALF(delta.vz);
    mid.vx = from.vx + half.vx;
    mid.vy = from.vy + half.vy;
    mid.vz = from.vz + half.vz;

    ctx.unk04 = 0;
    ctx.sector = gGabeSector;
    ctx.noHitPos = req->hitPos == NULL && req->hitNormal == NULL && req->hitInfo == NULL;
    ctx.from = from;
    ctx.delta = delta;
    ctx.mid = mid;
    ctx.ignore = req->fromObj;
    ctx.target = req->toObj;
    ctx.radius = halfLength + 0xF00;
    ctx.length = length;
    ctx.halfLength = halfLength;
    ctx.unkA8 = 1;
    ctx.nearest = 0x7FFFFFFF;
    ctx.hit = 0;
    ctx.hitPos = *req->to;
    if (req->toObj != NULL) {
        ctx.hitInfo.obj = req->toObj;
        ctx.hitInfo.kind = 2;
    } else {
        ctx.hitInfo.obj = NULL;
        ctx.hitInfo.kind = 0;
    }
    ctx.hitInfo.unk05 = 0;
    ctx.hitInfo.unk06 = 0;
    ctx.hitInfo.unk07 = 0;
    ctx.hitInfo.unk0C = 0;
    ctx.hitInfo.unk08 = 0;
    ctx.hitInfo.unk10 = 0;
    GTELOSLoop(&ctx, 1, 0);
    if (req->noHit != NULL) {
        *req->noHit = ctx.hit != 1;
    }
    if (req->hitPos != NULL) {
        *req->hitPos = ctx.hitPos;
    }
    if (req->hitNormal != NULL && ctx.hit == 1) {
        *req->hitNormal = ctx.hitNormal;
    }
    if (req->hitInfo != NULL) {
        *req->hitInfo = ctx.hitInfo;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPoint2PointCollide);
#endif
