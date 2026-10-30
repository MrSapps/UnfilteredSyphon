#include "common.h"

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

INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPoint2Point);

#ifdef NON_MATCHING
/* Scheduling only: the original stores each EGSMathFIXMul result after the next
 * call's argument loads (looks like an extra pre-reload scheduling pass). */
u8 _LOSPointAlongDirection(LOSRequest *req) {
    LOSVector to;
    LOSVector step;

    step.vx = EGSMathFIXMul(req->dir->vx, req->distance);
    step.vy = EGSMathFIXMul(req->dir->vy, req->distance);
    step.vz = EGSMathFIXMul(req->dir->vz, req->distance);
    to.vx = req->from->vx + step.vx;
    to.vy = req->from->vy + step.vy;
    to.vz = req->from->vz + step.vz;
    req->to = &to;
    return _LOSPoint2Point(req);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPointAlongDirection);
#endif

#ifdef NON_MATCHING
/* Scheduling only: the original stores each EGSMathFIXMul result after the next
 * call's argument loads (looks like an extra pre-reload scheduling pass). */
u8 _LOSPointAlongAngleDirectionYXZ(LOSRequest *req) {
    LOSVector to;
    LOSVector dir = D_80011B80;

    EGSGeometry_VectorRotateYXZ(&dir, req->angles, &dir);
    dir.vx = EGSMathFIXMul(dir.vx, req->distance);
    dir.vy = EGSMathFIXMul(dir.vy, req->distance);
    dir.vz = EGSMathFIXMul(dir.vz, req->distance);
    to.vx = req->from->vx + dir.vx;
    to.vy = req->from->vy + dir.vy;
    to.vz = req->from->vz + dir.vz;
    req->to = &to;
    req->toObj = (LOSGobj *)-1;
    return _LOSPoint2Point(req);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPointAlongAngleDirectionYXZ);
#endif

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

#ifdef NON_MATCHING
/* Scheduling only: the original stores each EGSMathFIXMul result after the next
 * call's argument loads (looks like an extra pre-reload scheduling pass). */
u8 _LOSGobjAlongDirection(LOSRequest *req) {
    LOSVector from;
    LOSVector to;
    LOSVector step;

    if (req->fromObj == NULL || req->fromObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    step.vx = EGSMathFIXMul(req->dir->vx, req->distance);
    step.vy = EGSMathFIXMul(req->dir->vy, req->distance);
    step.vz = EGSMathFIXMul(req->dir->vz, req->distance);
    to.vx = from.vx + step.vx;
    to.vy = from.vy + step.vy;
    to.vz = from.vz + step.vz;
    req->from = &from;
    req->to = &to;
    return _LOSPoint2Point(req);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSGobjAlongDirection);
#endif

#ifdef NON_MATCHING
/* Scheduling only: the original stores each EGSMathFIXMul result after the next
 * call's argument loads (looks like an extra pre-reload scheduling pass). */
u8 _LOSGobjAlongAngleDirectionYXZ(LOSRequest *req) {
    LOSVector from;
    LOSVector to;
    LOSVector dir = D_80011B80;

    if (req->fromObj == NULL || req->fromObj->anim == NULL) {
        return 0;
    }
    LOSGetGobjLOSTestPoint(req->fromObj, &from);
    EGSGeometry_VectorRotateYXZ(&dir, req->angles, &dir);
    dir.vx = EGSMathFIXMul(dir.vx, req->distance);
    dir.vy = EGSMathFIXMul(dir.vy, req->distance);
    dir.vz = EGSMathFIXMul(dir.vz, req->distance);
    to.vx = from.vx + dir.vx;
    to.vy = from.vy + dir.vy;
    to.vz = from.vz + dir.vz;
    req->from = &from;
    req->to = &to;
    return _LOSPoint2Point(req);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/los", _LOSGobjAlongAngleDirectionYXZ);
#endif

INCLUDE_ASM("asm/main/nonmatchings/los", _LOSPoint2PointCollide);
