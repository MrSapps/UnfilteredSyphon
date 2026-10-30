#include "common.h"

#include "system.h"
#include "egsframe.h"

/* a segment/line/ray: a point and a direction */
typedef struct {
    VECTOR p;
    VECTOR d;
} EGSLine;

extern MATRIX gEGSIdentityMatrix;

int EGSFrameNodeUpdate(EGSFrame *frame); /* egsmath.c */
int EGSMath_MatrixInvert(MATRIX *in, MATRIX *out);

static inline void CopyRotation(MATRIX *dst, MATRIX *src)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

/* EGS frames are y-up, GTE matrices y-down: convert between the two */
static inline MATRIX *FlipYRot(MATRIX *mat)
{
    mat->m[0][1] = -mat->m[0][1];
    mat->m[1][0] = -mat->m[1][0];
    mat->m[1][2] = -mat->m[1][2];
    mat->m[2][1] = -mat->m[2][1];
    return mat;
}

#define VECTOR_IS_ZERO(v) ((v)->vx == 0 && (v)->vy == 0 && (v)->vz == 0)

/* EGS (y-up) YXZ angles to a GTE rotation matrix (not yet y-flipped) */
static inline void AnglesToMatrix(VECTOR *rot, MATRIX *m)
{
    SVECTOR a;

    a.vx = -rot->vx;
    a.vy = rot->vy;
    a.vz = -rot->vz;
    RotMatrixYXZ(&a, m);
}

static inline int FlipY(MATRIX *mat)
{
    mat->m[0][1] = -mat->m[0][1];
    mat->m[1][0] = -mat->m[1][0];
    mat->m[1][2] = -mat->m[1][2];
    mat->m[2][1] = -mat->m[2][1];
    mat->t[1] = -mat->t[1];
    return 0;
}

int EGSMath_RotationMatrixOrthonormalizeYXZ(MATRIX *in, MATRIX *out);
MATRIX *MatrixNormal_0(MATRIX *m0, MATRIX *m1);

int EGSFrameCreate(EGSFrame **out);
int EGSFrameCreateNode(EGSFrame *frame, EGSFrame *parent);
int EGSFrameAddNode(EGSFrame *frame, EGSFrame *parent);
int EGSFrameRemoveNode(EGSFrame *frame);
int EGSFrameGetRotationMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *out);
int EGSFrameGetMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *out);
int EGSFrameRotateYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot);
int EGSFrameRevolveYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot);
int EGSFrameSetRotationYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot);
int EGSFrameSetRotationMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *m);
int EGSFrameSetMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *m);
int EGSFrameGetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *out);
int EGSFrameSetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *pos);
int EGSFrameRotationMatrixOrthonormalize(EGSFrame *frame);
int EGSFrameTransformPoint(VECTOR *in, EGSFrame *frame, EGSFrame *ref, VECTOR *out);
int EGSFrameTransformVector(VECTOR *in, EGSFrame *frame, EGSFrame *ref, VECTOR *out);
int EGSFrameSetOrientation(EGSFrame *frame, EGSFrame *ref, VECTOR *dir, s32 arg3);
int EGSFrameSetOrientation_WithReferenceUpDirection(EGSFrame *frame, EGSFrame *ref, VECTOR *dir, VECTOR *up, s32 arg4);

int EGSFrameCreate(EGSFrame **out)
{
    *out = EGSMemMalloc(sizeof(EGSFrame));
    if (*out == NULL) {
        return EGS_ERR_NOMEM;
    }
    (*out)->matrix = gEGSIdentityMatrix;
    (*out)->node = NULL;
    return 0;
}

int EGSFrameCreateWithNode(EGSFrame **out, EGSFrame *parent)
{
    int ret;

    ret = EGSFrameCreate(out);
    if (ret != 0) {
        return ret;
    }
    ret = EGSFrameCreateNode(*out, parent);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

int EGSFrameCreateNode(EGSFrame *frame, EGSFrame *parent)
{
    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    if (frame->node != NULL) {
        return 1;
    }
    frame->node = EGSMemMalloc(sizeof(EGSFrameNode));
    if (frame->node == NULL) {
        return EGS_ERR_NOMEM;
    }
    frame->node->local = gEGSIdentityMatrix;
    frame->node->parent = NULL;
    frame->node->next = NULL;
    frame->node->child = NULL;
    frame->node->dirty = 0;
    EGSFrameAddNode(frame, parent);
    return 0;
}

int EGSFrameAddNode(EGSFrame *frame, EGSFrame *parent)
{
    MATRIX m;
    int ret;
    int err;

    if (frame == NULL && parent == NULL) {
        return EGS_ERR_NULL;
    }
    if (parent != NULL && parent->node == NULL) {
        ret = EGSFrameCreateNode(parent, NULL);
        if (ret != 0) {
            return ret;
        }
    }
    if (frame == NULL) {
        return 0;
    }
    if (frame->node == NULL) {
        err = EGSFrameCreateNode(frame, parent);
        if (err != 0) {
            return err;
        }
    } else {
        if (frame->node->parent != NULL) {
            EGSFrameRemoveNode(frame);
        }
        frame->node->parent = parent;
        if (parent != NULL) {
            frame->node->next = parent->node->child;
            parent->node->child = frame;
        }
        if (frame->node->dirty == 0) {
            /* keep the world placement: convert it to a y-down local matrix */
            m.m[0][0] = frame->matrix.m[0][0];
            m.m[0][1] = -frame->matrix.m[0][1];
            m.m[0][2] = frame->matrix.m[0][2];
            m.m[1][0] = -frame->matrix.m[1][0];
            m.m[1][1] = frame->matrix.m[1][1];
            m.m[1][2] = -frame->matrix.m[1][2];
            m.m[2][0] = frame->matrix.m[2][0];
            m.m[2][1] = -frame->matrix.m[2][1];
            m.m[2][2] = frame->matrix.m[2][2];
            m.t[0] = frame->matrix.t[0];
            m.t[1] = -frame->matrix.t[1];
            m.t[2] = frame->matrix.t[2];
            EGSFrameSetMatrix(frame, NULL, &m);
        }
    }
    return 0;
}

#ifdef NON_MATCHING
/* loop entry not threaded past the cur!=frame test; prev in a2 not a1 */
int EGSFrameRemoveNode(EGSFrame *frame)
{
    EGSFrameNode *pnode;
    EGSFrameNode *node;
    EGSFrame *cur;
    EGSFrame *prev;

    if (frame == NULL) {
        return 0;
    }
    if (frame->node == NULL) {
        return EGS_ERR_NONODE;
    }
    if (frame->node->parent == NULL) {
        return 0;
    }
    pnode = frame->node->parent->node;
    if (pnode == NULL) {
        return EGS_ERR_NONODE;
    }
    cur = pnode->child;
    if (cur == NULL) {
        return EGS_ERR_NONODE;
    }
    if (frame == cur) {
        pnode->child = frame->node->next;
    } else {
        while (cur != NULL && cur != frame) {
            prev = cur;
            node = cur->node;
            if (node == NULL) {
                return EGS_ERR_NONODE;
            }
            cur = node->next;
        }
        if (prev->node == NULL) {
            return EGS_ERR_NONODE;
        }
        if (prev->node->next == NULL) {
            return EGS_ERR_NONODE;
        }
        prev->node->next = frame->node->next;
    }
    frame->node->parent = NULL;
    frame->node->dirty = 1;
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsframe", EGSFrameRemoveNode);
#endif

int EGSFrameUpdateNode(EGSFrame *frame)
{
    return EGSFrameNodeUpdate(frame);
}

int EGSFrameGetRotationMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *out)
{
    MATRIX inv;

    if (ref == frame) {
        CopyRotation(out, &gEGSIdentityMatrix);
    } else if (ref == NULL) {
        CopyRotation(out, &frame->matrix);
    } else if (frame == NULL) {
        TransposeMatrix(&ref->matrix, out);
        out->t[0] = ref->matrix.t[0];
        out->t[1] = ref->matrix.t[1];
        out->t[2] = ref->matrix.t[2];
    } else {
        TransposeMatrix(&ref->matrix, &inv);
        inv.t[0] = ref->matrix.t[0];
        inv.t[1] = ref->matrix.t[1];
        inv.t[2] = ref->matrix.t[2];
        MulMatrix0(&inv, &frame->matrix, out);
    }
    out->t[0] = 0;
    out->t[1] = 0;
    out->t[2] = 0;
    return FlipY(out);
}

int EGSFrameGetMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *out)
{
    MATRIX inv;

    if (ref == frame) {
        *out = gEGSIdentityMatrix;
    } else if (ref == NULL) {
        *out = frame->matrix;
    } else if (frame == NULL) {
        EGSMath_MatrixInvert(&ref->matrix, out);
    } else {
        EGSMath_MatrixInvert(&ref->matrix, &inv);
        CompMatrixLV(&inv, &frame->matrix, out);
    }
    return FlipY(out);
}

int EGSFrameRotateYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot)
{
    MATRIX cur;
    MATRIX rm;
    int ret;

    ret = EGSFrameGetRotationMatrix(frame, ref, &cur);
    AnglesToMatrix(rot, &rm);
    MulMatrix0(FlipYRot(&rm), &cur, &cur);
    if (ret == 0) {
        ret = EGSFrameSetRotationMatrix(frame, ref, &cur);
    }
    EGSFrameRotationMatrixOrthonormalize(frame);
    return ret;
}

int EGSFrameRevolveYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot)
{
    MATRIX cur;
    MATRIX rm;
    int ret;

    ret = EGSFrameGetMatrix(frame, ref, &cur);
    AnglesToMatrix(rot, &rm);
    CompMatrixLV(FlipYRot(&rm), &cur, &cur);
    if (ret == 0) {
        ret = EGSFrameSetMatrix(frame, ref, &cur);
    }
    EGSFrameRotationMatrixOrthonormalize(frame);
    return ret;
}

int EGSFrameRotateLocalYXZ(EGSFrame *frame, VECTOR *rot)
{
    return EGSFrameRotateYXZ(frame, frame, rot);
}

int EGSFrameRotateParentYXZ(EGSFrame *frame, VECTOR *rot)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameRotateYXZ(frame, NULL, rot);
    } else {
        ret = EGSFrameRotateYXZ(frame, frame->node->parent, rot);
    }
    return ret;
}

int EGSFrameRevolveParentYXZ(EGSFrame *frame, VECTOR *rot)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameRevolveYXZ(frame, NULL, rot);
    } else {
        ret = EGSFrameRevolveYXZ(frame, frame->node->parent, rot);
    }
    return ret;
}

int EGSFrameSetRotationLocalYXZ(EGSFrame *frame, VECTOR *rot)
{
    return EGSFrameSetRotationYXZ(frame, frame, rot);
}

int EGSFrameSetRotationParentYXZ(EGSFrame *frame, VECTOR *rot)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameSetRotationYXZ(frame, NULL, rot);
    } else {
        ret = EGSFrameSetRotationYXZ(frame, frame->node->parent, rot);
    }
    return ret;
}

int EGSFrameSetRotationYXZ(EGSFrame *frame, EGSFrame *ref, VECTOR *rot)
{
    MATRIX m;

    AnglesToMatrix(rot, &m);
    return EGSFrameSetRotationMatrix(frame, ref, FlipYRot(&m));
}

int EGSFrameSetRotationMatrixParent(EGSFrame *frame, MATRIX *m)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameSetRotationMatrix(frame, NULL, m);
    } else {
        ret = EGSFrameSetRotationMatrix(frame, frame->node->parent, m);
    }
    return ret;
}

int EGSFrameSetRotationMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *m)
{
    MATRIX inv;
    MATRIX tmp;
    EGSFrame *parent;

    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    FlipY(m);
    if (frame->node != NULL) {
        parent = frame->node->parent;
        if (ref == parent) {
            frame->node->local.m[0][0] = m->m[0][0];
            frame->node->local.m[0][1] = m->m[0][1];
            frame->node->local.m[0][2] = m->m[0][2];
            frame->node->local.m[1][0] = m->m[1][0];
            frame->node->local.m[1][1] = m->m[1][1];
            frame->node->local.m[1][2] = m->m[1][2];
            frame->node->local.m[2][0] = m->m[2][0];
            frame->node->local.m[2][1] = m->m[2][1];
            frame->node->local.m[2][2] = m->m[2][2];
        } else if (ref == NULL && parent != NULL) {
            TransposeMatrix(&parent->matrix, &inv);
            inv.t[0] = frame->node->parent->matrix.t[0];
            inv.t[1] = frame->node->parent->matrix.t[1];
            inv.t[2] = frame->node->parent->matrix.t[2];
            MulMatrix0(&inv, m, &frame->node->local);
        } else if (ref != NULL && parent == NULL) {
            MulMatrix0(&ref->matrix, m, &frame->node->local);
        } else {
            MulMatrix0(&ref->matrix, m, &tmp);
            TransposeMatrix(&frame->node->parent->matrix, &inv);
            inv.t[0] = frame->node->parent->matrix.t[0];
            inv.t[1] = frame->node->parent->matrix.t[1];
            inv.t[2] = frame->node->parent->matrix.t[2];
            MulMatrix0(&inv, &tmp, &frame->node->local);
        }
        frame->node->dirty = 1;
    } else if (ref != NULL) {
        MulMatrix0(&ref->matrix, m, &frame->matrix);
    } else {
        frame->matrix.m[0][0] = m->m[0][0];
        frame->matrix.m[0][1] = m->m[0][1];
        frame->matrix.m[0][2] = m->m[0][2];
        frame->matrix.m[1][0] = m->m[1][0];
        frame->matrix.m[1][1] = m->m[1][1];
        frame->matrix.m[1][2] = m->m[1][2];
        frame->matrix.m[2][0] = m->m[2][0];
        frame->matrix.m[2][1] = m->m[2][1];
        frame->matrix.m[2][2] = m->m[2][2];
    }
    return FlipY(m);
}

int EGSFrameSetMatrixParent(EGSFrame *frame, MATRIX *m)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameSetMatrix(frame, NULL, m);
    } else {
        ret = EGSFrameSetMatrix(frame, frame->node->parent, m);
    }
    return ret;
}

int EGSFrameSetMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *m)
{
    MATRIX inv;
    MATRIX tmp;
    EGSFrame *parent;

    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    FlipY(m);
    if (frame->node != NULL) {
        parent = frame->node->parent;
        if (ref == parent) {
            frame->node->local = *m;
        } else if (ref == NULL && parent != NULL) {
            EGSMath_MatrixInvert(&parent->matrix, &inv);
            CompMatrixLV(&inv, m, &frame->node->local);
        } else if (ref != NULL && parent == NULL) {
            CompMatrixLV(&ref->matrix, m, &frame->node->local);
        } else {
            CompMatrixLV(&ref->matrix, m, &tmp);
            EGSMath_MatrixInvert(&frame->node->parent->matrix, &inv);
            CompMatrixLV(&inv, &tmp, &frame->node->local);
        }
        frame->node->dirty = 1;
    } else if (ref != NULL) {
        CompMatrixLV(&ref->matrix, m, &frame->matrix);
    } else {
        frame->matrix = *m;
    }
    return FlipY(m);
}

int EGSFrameRotationMatrixOrthonormalizeYXZ(EGSFrame *frame)
{
    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    if (frame->node != NULL) {
        EGSMath_RotationMatrixOrthonormalizeYXZ(&frame->node->local, &frame->node->local);
        frame->node->dirty = 1;
    } else {
        EGSMath_RotationMatrixOrthonormalizeYXZ(&frame->matrix, &frame->matrix);
    }
    return 0;
}

int EGSFrameRotationMatrixOrthonormalize(EGSFrame *frame)
{
    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    if (frame->node != NULL) {
        MatrixNormal_0(&frame->node->local, &frame->node->local);
        frame->node->dirty = 1;
    } else {
        MatrixNormal_0(&frame->matrix, &frame->matrix);
    }
    return 0;
}

int EGSFrameSetIdentityRotationMatrix(EGSFrame *frame, EGSFrame *ref)
{
    return EGSFrameSetRotationMatrix(frame, ref, &gEGSIdentityMatrix);
}

int EGSFrameSetIdentityMatrix(EGSFrame *frame, EGSFrame *ref)
{
    return EGSFrameSetMatrix(frame, ref, &gEGSIdentityMatrix);
}

int EGSFrameGetOrientation(EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetRotationMatrix(frame, ref, &m);
    out->vx = m.m[0][2];
    out->vy = m.m[1][2];
    out->vz = m.m[2][2];
    return ret;
}

int EGSFrameGetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX inv;
    MATRIX inv2;

    if (ref == frame) {
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
    } else if (ref == NULL) {
        out->vx = frame->matrix.t[0];
        out->vy = frame->matrix.t[1];
        out->vz = frame->matrix.t[2];
    } else if (frame == NULL) {
        EGSMath_MatrixInvert(&ref->matrix, &inv);
        out->vx = inv.t[0];
        out->vy = inv.t[1];
        out->vz = inv.t[2];
    } else {
        out->vx = frame->matrix.t[0];
        out->vy = frame->matrix.t[1];
        out->vz = frame->matrix.t[2];
        EGSMath_MatrixInvert(&ref->matrix, &inv2);
        ApplyMatrixLV(&inv2, out, out);
        out->vx += inv2.t[0];
        out->vy += inv2.t[1];
        out->vz += inv2.t[2];
    }
    out->vy = -out->vy;
    return 0;
}

int EGSFrameSetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *pos)
{
    VECTOR local;
    VECTOR world;
    int ret;

    if (frame->node != NULL) {
        ret = EGSFrameTransformPoint(pos, ref, frame->node->parent, &local);
        local.vy = -local.vy;
        frame->node->local.t[0] = local.vx;
        frame->node->local.t[1] = local.vy;
        frame->node->local.t[2] = local.vz;
        frame->node->dirty = 1;
    } else {
        ret = EGSFrameTransformPoint(pos, ref, NULL, &world);
        world.vy = -world.vy;
        frame->matrix.t[0] = world.vx;
        frame->matrix.t[1] = world.vy;
        frame->matrix.t[2] = world.vz;
    }
    return ret;
}

int EGSFrameSetPositionLocal(EGSFrame *frame, VECTOR *pos)
{
    return EGSFrameSetPosition(frame, frame, pos);
}

int EGSFrameSetPositionParent(EGSFrame *frame, VECTOR *pos)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameSetPosition(frame, NULL, pos);
    } else {
        ret = EGSFrameSetPosition(frame, frame->node->parent, pos);
    }
    return ret;
}

int EGSFrameMove(EGSFrame *frame, EGSFrame *ref, VECTOR *delta)
{
    VECTOR pos;
    int ret;

    ret = EGSFrameGetPosition(frame, ref, &pos);
    pos.vx += delta->vx;
    pos.vy += delta->vy;
    pos.vz += delta->vz;
    if (ret == 0) {
        ret = EGSFrameSetPosition(frame, ref, &pos);
    }
    return ret;
}

int EGSFrameMoveLocal(EGSFrame *frame, VECTOR *delta)
{
    return EGSFrameMove(frame, frame, delta);
}

int EGSFrameMoveParent(EGSFrame *frame, VECTOR *delta)
{
    int ret;

    if (frame->node == NULL) {
        ret = EGSFrameMove(frame, NULL, delta);
    } else {
        ret = EGSFrameMove(frame, frame->node->parent, delta);
    }
    return ret;
}

int EGSFrameGetLocalX(EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;

    EGSFrameGetRotationMatrix(frame, ref, &m);
    out->vx = m.m[0][0];
    out->vy = m.m[1][0];
    out->vz = m.m[2][0];
    return 0;
}

int EGSFrameGetLocalY(EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;

    EGSFrameGetRotationMatrix(frame, ref, &m);
    out->vx = m.m[0][1];
    out->vy = m.m[1][1];
    out->vz = m.m[2][1];
    return 0;
}

int EGSFrameGetLocalZ(EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;

    EGSFrameGetRotationMatrix(frame, ref, &m);
    out->vx = m.m[0][2];
    out->vy = m.m[1][2];
    out->vz = m.m[2][2];
    return 0;
}

int EGSFrameGetLocalAxes(EGSFrame *frame, EGSFrame *ref, VECTOR *x, VECTOR *y, VECTOR *z)
{
    MATRIX m;

    EGSFrameGetRotationMatrix(frame, ref, &m);
    x->vx = m.m[0][0];
    x->vy = m.m[1][0];
    x->vz = m.m[2][0];
    y->vx = m.m[0][1];
    y->vy = m.m[1][1];
    y->vz = m.m[2][1];
    z->vx = m.m[0][2];
    z->vy = m.m[1][2];
    z->vz = m.m[2][2];
    return 0;
}

int EGSFrameSetLocalX(EGSFrame *frame, EGSFrame *ref, VECTOR *axis)
{
    VECTOR local;
    VECTOR world;
    int ret;

    if (frame->node != NULL) {
        ret = EGSFrameTransformVector(axis, ref, frame->node->parent, &local);
        local.vy = -local.vy;
        frame->node->local.m[0][0] = local.vx;
        frame->node->local.m[1][0] = local.vy;
        frame->node->local.m[2][0] = local.vz;
        frame->node->dirty = 1;
    } else {
        ret = EGSFrameTransformVector(axis, ref, NULL, &world);
        world.vy = -world.vy;
        frame->matrix.m[0][0] = world.vx;
        frame->matrix.m[1][0] = world.vy;
        frame->matrix.m[2][0] = world.vz;
    }
    return ret;
}

int EGSFrameSetLocalY(EGSFrame *frame, EGSFrame *ref, VECTOR *axis)
{
    VECTOR local;
    VECTOR world;
    int ret;

    if (frame->node != NULL) {
        ret = EGSFrameTransformVector(axis, ref, frame->node->parent, &local);
        local.vx = -local.vx;
        local.vz = -local.vz;
        frame->node->local.m[0][1] = local.vx;
        frame->node->local.m[1][1] = local.vy;
        frame->node->local.m[2][1] = local.vz;
        frame->node->dirty = 1;
    } else {
        ret = EGSFrameTransformVector(axis, ref, NULL, &world);
        world.vx = -world.vx;
        world.vz = -world.vz;
        frame->matrix.m[0][1] = world.vx;
        frame->matrix.m[1][1] = world.vy;
        frame->matrix.m[2][1] = world.vz;
    }
    return ret;
}

int EGSFrameSetLocalZ(EGSFrame *frame, EGSFrame *ref, VECTOR *axis)
{
    VECTOR local;
    VECTOR world;
    int ret;

    if (frame->node != NULL) {
        ret = EGSFrameTransformVector(axis, ref, frame->node->parent, &local);
        local.vy = -local.vy;
        frame->node->local.m[0][2] = local.vx;
        frame->node->local.m[1][2] = local.vy;
        frame->node->local.m[2][2] = local.vz;
        frame->node->dirty = 1;
    } else {
        ret = EGSFrameTransformVector(axis, ref, NULL, &world);
        world.vy = -world.vy;
        frame->matrix.m[0][2] = world.vx;
        frame->matrix.m[1][2] = world.vy;
        frame->matrix.m[2][2] = world.vz;
    }
    return ret;
}

int EGSFrameSetLocalAxes(EGSFrame *frame, EGSFrame *ref, VECTOR *x, VECTOR *y, VECTOR *z)
{
    MATRIX m;

    m.m[0][0] = x->vx;
    m.m[1][0] = x->vy;
    m.m[2][0] = x->vz;
    m.m[0][1] = y->vx;
    m.m[1][1] = y->vy;
    m.m[2][1] = y->vz;
    m.m[0][2] = z->vx;
    m.m[1][2] = z->vy;
    m.m[2][2] = z->vz;
    return EGSFrameSetRotationMatrix(frame, ref, &m);
}

int EGSFrameLookAt(EGSFrame *frame, EGSFrame *target, s32 arg2)
{
    VECTOR from;
    VECTOR to;
    VECTOR dir;
    int ret;

    ret = EGSFrameGetPosition(frame, NULL, &from);
    if (ret == 0) {
        ret = EGSFrameGetPosition(target, NULL, &to);
    }
    dir.vx = to.vx - from.vx;
    dir.vy = to.vy - from.vy;
    dir.vz = to.vz - from.vz;
    if (ret == 0) {
        ret = EGSFrameSetOrientation(frame, NULL, &dir, arg2);
    }
    return ret;
}

int EGSFrameLookAt_WithReferenceUpDirection(EGSFrame *frame, EGSFrame *target, VECTOR *up, s32 arg3)
{
    VECTOR from;
    VECTOR to;
    VECTOR dir;
    int ret;

    ret = EGSFrameGetPosition(frame, NULL, &from);
    if (ret == 0) {
        ret = EGSFrameGetPosition(target, NULL, &to);
    }
    dir.vx = to.vx - from.vx;
    dir.vy = to.vy - from.vy;
    dir.vz = to.vz - from.vz;
    if (ret == 0) {
        ret = EGSFrameSetOrientation_WithReferenceUpDirection(frame, NULL, &dir, up, arg3);
    }
    return ret;
}

/* points the frame's z axis along dir, then rolls it by roll around z */
int EGSFrameSetOrientation(EGSFrame *frame, EGSFrame *ref, VECTOR *dir, s32 roll)
{
    VECTOR x;
    VECTOR y;
    VECTOR z;
    MATRIX m;
    MATRIX rm;
    VECTOR angles;

    if (frame == NULL || dir == NULL || VECTOR_IS_ZERO(dir) == 1) {
        return 1;
    }
    GTEMath_VectorNormal(dir, &z);
    x.vx = z.vz;
    x.vy = 0;
    x.vz = -z.vx;
    if (VECTOR_IS_ZERO(&x) == 1) {
        x.vz = 0x1000;
    }
    GTEMath_VectorNormal(&x, &x);
    OuterProduct12(&z, &x, &y);
    m.m[0][0] = x.vx;
    m.m[1][0] = x.vy;
    m.m[2][0] = x.vz;
    m.m[0][1] = y.vx;
    m.m[1][1] = y.vy;
    m.m[2][1] = y.vz;
    m.m[0][2] = z.vx;
    m.m[1][2] = z.vy;
    m.m[2][2] = z.vz;
    if (roll != 0) {
        angles.vx = 0;
        angles.vy = 0;
        angles.vz = roll;
        AnglesToMatrix(&angles, &rm);
        MulMatrix0(&m, FlipYRot(&rm), &m);
    }
    return EGSFrameSetRotationMatrix(frame, ref, &m);
}

/* as EGSFrameSetOrientation, with the x axis taken perpendicular to up */
int EGSFrameSetOrientation_WithReferenceUpDirection(EGSFrame *frame, EGSFrame *ref, VECTOR *dir, VECTOR *up, s32 roll)
{
    VECTOR x;
    VECTOR y;
    VECTOR z;
    MATRIX m;
    MATRIX rm;
    VECTOR angles;

    if (frame == NULL || dir == NULL || VECTOR_IS_ZERO(dir) == 1) {
        return 1;
    }
    GTEMath_VectorNormal(dir, &z);
    OuterProduct12(up, &z, &x);
    if (VECTOR_IS_ZERO(&x) == 1) {
        x.vz = 0x1000;
    }
    GTEMath_VectorNormal(&x, &x);
    OuterProduct12(&z, &x, &y);
    m.m[0][0] = x.vx;
    m.m[1][0] = x.vy;
    m.m[2][0] = x.vz;
    m.m[0][1] = y.vx;
    m.m[1][1] = y.vy;
    m.m[2][1] = y.vz;
    m.m[0][2] = z.vx;
    m.m[1][2] = z.vy;
    m.m[2][2] = z.vz;
    if (roll != 0) {
        angles.vx = 0;
        angles.vy = 0;
        angles.vz = roll;
        AnglesToMatrix(&angles, &rm);
        MulMatrix0(&m, FlipYRot(&rm), &m);
    }
    return EGSFrameSetRotationMatrix(frame, ref, &m);
}

int EGSFrameLookAtPoint(EGSFrame *frame, EGSFrame *ref, VECTOR *point, s32 arg3)
{
    VECTOR pos;
    VECTOR dir;
    int ret;

    ret = EGSFrameGetPosition(frame, ref, &pos);
    dir.vx = point->vx - pos.vx;
    dir.vy = point->vy - pos.vy;
    dir.vz = point->vz - pos.vz;
    if (ret == 0) {
        ret = EGSFrameSetOrientation(frame, ref, &dir, arg3);
    }
    return ret;
}

int EGSFrameLookAtPoint_WithReferenceUpDirection(EGSFrame *frame, EGSFrame *ref, VECTOR *point, VECTOR *up, s32 arg4)
{
    VECTOR pos;
    VECTOR dir;
    int ret;

    ret = EGSFrameGetPosition(frame, ref, &pos);
    dir.vx = point->vx - pos.vx;
    dir.vy = point->vy - pos.vy;
    dir.vz = point->vz - pos.vz;
    if (ret == 0) {
        ret = EGSFrameSetOrientation_WithReferenceUpDirection(frame, ref, &dir, up, arg4);
    }
    return ret;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

static inline void iFixMul(s32 a, s32 b, s32 *out) {
    *out = EGSMathFIXMul(a, b);
}

#define CLAMP_FIX(v) MAX(MIN(v, 0x7FFF), -0x7FFF)

#ifdef NON_MATCHING
/* prologue only: original saves/copies s1 (scale) before s0 (m) */
int EGSFrameScale(MATRIX *m, VECTOR *scale) {
    s32 t;

    iFixMul(m->m[0][0], scale->vx, &t);
    m->m[0][0] = CLAMP_FIX(t);
    iFixMul(m->m[1][0], scale->vx, &t);
    m->m[1][0] = CLAMP_FIX(t);
    iFixMul(m->m[2][0], scale->vx, &t);
    m->m[2][0] = CLAMP_FIX(t);
    iFixMul(m->m[0][1], scale->vy, &t);
    m->m[0][1] = CLAMP_FIX(t);
    iFixMul(m->m[1][1], scale->vy, &t);
    m->m[1][1] = CLAMP_FIX(t);
    iFixMul(m->m[2][1], scale->vy, &t);
    m->m[2][1] = CLAMP_FIX(t);
    iFixMul(m->m[0][2], scale->vz, &t);
    m->m[0][2] = CLAMP_FIX(t);
    iFixMul(m->m[1][2], scale->vz, &t);
    m->m[1][2] = CLAMP_FIX(t);
    iFixMul(m->m[2][2], scale->vz, &t);
    m->m[2][2] = CLAMP_FIX(t);
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsframe", EGSFrameScale);
#endif

int EGSFrameTransformPoint(VECTOR *in, EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetMatrix(frame, ref, &m);
    ApplyMatrixLV(&m, in, out);
    out->vx += m.t[0];
    out->vy += m.t[1];
    out->vz += m.t[2];
    return ret;
}

int EGSFrameTransformVector(VECTOR *in, EGSFrame *frame, EGSFrame *ref, VECTOR *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetRotationMatrix(frame, ref, &m);
    ApplyMatrixLV(&m, in, out);
    return ret;
}

int EGSFrameTransformSegment(EGSLine *in, EGSFrame *frame, EGSFrame *ref, EGSLine *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetMatrix(frame, ref, &m);
    ApplyMatrixLV(&m, &in->p, &out->p);
    out->p.vx += m.t[0];
    out->p.vy += m.t[1];
    out->p.vz += m.t[2];
    ApplyMatrixLV(&m, &in->d, &out->d);
    return ret;
}

int EGSFrameTransformLine(EGSLine *in, EGSFrame *frame, EGSFrame *ref, EGSLine *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetMatrix(frame, ref, &m);
    ApplyMatrixLV(&m, &in->p, &out->p);
    out->p.vx += m.t[0];
    out->p.vy += m.t[1];
    out->p.vz += m.t[2];
    ApplyMatrixLV(&m, &in->d, &out->d);
    return ret;
}

int EGSFrameTransformRay(EGSLine *in, EGSFrame *frame, EGSFrame *ref, EGSLine *out)
{
    MATRIX m;
    int ret;

    ret = EGSFrameGetMatrix(frame, ref, &m);
    ApplyMatrixLV(&m, &in->p, &out->p);
    out->p.vx += m.t[0];
    out->p.vy += m.t[1];
    out->p.vz += m.t[2];
    ApplyMatrixLV(&m, &in->d, &out->d);
    return ret;
}
