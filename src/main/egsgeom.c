#include "common.h"

#include "egsmath.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

/* Axis-aligned ("unrotated") box. */
typedef struct EGSBox {
    /* 0x00 */ VECTOR min;
    /* 0x10 */ VECTOR max;
} EGSBox; /* size 0x20 */

/* Segment / line / ray: a point and a second point or direction. */
typedef struct EGSSegment {
    /* 0x00 */ VECTOR p0;
    /* 0x10 */ VECTOR p1;
} EGSSegment; /* size 0x20 */

/* Plane through a point with a (unit) normal. */
typedef struct EGSPlane {
    /* 0x00 */ VECTOR point;
    /* 0x10 */ VECTOR normal;
} EGSPlane; /* size 0x20 */

/* Triangle (type 3) or quad (type 4) face with a cached normal. */
typedef struct EGSFace {
    /* 0x00 */ s32 type;
    /* 0x04 */ VECTOR v[4];
    /* 0x44 */ VECTOR normal;
    /* 0x54 */ u8 normalIsUnit;
} EGSFace;

extern MATRIX gEGSIdentityMatrix;

/* libgte */
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);

/* egsmath.c */
int EGSMathINT_VectorModulus(VECTOR *v, s32 *out);
int EGSMathFIX_VectorModulus(VECTOR *v, s32 *out);
int EGSMathFIX_VectorGetXZComponent(VECTOR *v, s32 *out);
int EGSMathFIX_VectorGetYZComponent(VECTOR *v, s32 *out);
int EGSMathFIX_VectorScaleForMultiplication(VECTOR *in, VECTOR *out);
int EGSMath_VectorApplyReverseMatrix(VECTOR *in, MATRIX *m, VECTOR *out);
int EGSMath_MatrixInvert(MATRIX *in, MATRIX *out);
int EGSMath_VectorModulus(VECTOR *v, s32 *out);
int EGSMathFIX_ValueSquareRoot(s32 value, s32 *out);

int EGSGeometryINT_VectorGetAngleFromXZPlane(VECTOR *v, s32 *out);
int EGSGeometry_UnitVectorToRotationMatrix(VECTOR *v, s32 arg1, MATRIX *m);
int EGSGeometry_FaceGetNonUnitNormal(EGSFace *face);
int EGSGeometry_ReverseTransformSegment(EGSSegment *in, MATRIX *m, EGSSegment *out);
int EGSGeometry_IsIntersectPoint2UnrotBox(VECTOR *p, EGSBox *b, s32 margin, u8 *hit);
int EGSGeometry_IsIntersectUnrotBox2UnrotBox(EGSBox *a, EGSBox *b, s32 margin, u8 *hit);
int EGSGeometry_IsIntersectXZUnrotBox2UnrotBox(EGSBox *a, EGSBox *b, s32 margin, u8 *hit);
int EGSGeometry_IsIntersectXZPoint2UnrotBox(VECTOR *p, EGSBox *b, s32 margin, u8 *hit);
int EGSGeometry_GetBoundingBox(VECTOR *pts, s32 n, EGSBox *out);

static inline void EGSMathFIX_ValueMultiply(s32 a, s32 b, s32 *out)
{
    *out = EGSMathFIXMul(a, b);
}

static inline void EGSMathFIX_VectorDot(VECTOR *a, VECTOR *b, s32 *out)
{
    *out = EGSMathFIXMul(a->vx, b->vx) + EGSMathFIXMul(a->vy, b->vy) + EGSMathFIXMul(a->vz, b->vz);
}

static inline void EGSMathFIX_VectorScale(VECTOR *v, s32 scale, VECTOR *out)
{
    out->vx = EGSMathFIXMul(v->vx, scale);
    out->vy = EGSMathFIXMul(v->vy, scale);
    out->vz = EGSMathFIXMul(v->vz, scale);
}

/* Rotation matrix for YXZ euler angles in EGS (left-handed) convention. */
static inline void EGSGeometry_AngleToMatrixYXZ(VECTOR *angle, MATRIX *m)
{
    SVECTOR r;

    r.vx = -angle->vx;
    r.vy = angle->vy;
    r.vz = -angle->vz;
    RotMatrixYXZ(&r, m);
    m->m[0][1] = -m->m[0][1];
    m->m[1][0] = -m->m[1][0];
    m->m[1][2] = -m->m[1][2];
    m->m[2][1] = -m->m[2][1];
}

static inline void EGSMathINT_VectorGetXZComponentInline(VECTOR *v, s32 *out)
{
    *out = SquareRoot0(v->vx * v->vx + v->vz * v->vz);
}

static inline void EGSMathINT_VectorGetYZComponentInline(VECTOR *v, s32 *out)
{
    *out = SquareRoot0(v->vy * v->vy + v->vz * v->vz);
}

int EGSGeometryINT_GetDistancePointToPoint(VECTOR *a, VECTOR *b, s32 *out)
{
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    EGSMathINT_VectorModulus(&d, out);
    return 0;
}

int EGSGeometryINT_GetXZDistancePoint2Point(VECTOR *a, VECTOR *b, s32 *out)
{
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    d.vy = 0;
    EGSMathINT_VectorModulus(&d, out);
    return 0;
}

int EGSGeometry_GetFuzzyXZDistancePoint2Point(VECTOR *a, VECTOR *b, s32 *out)
{
    s32 dx, dz, sum, diff, half, quarter;

    dx = ABS(a->vx - b->vx);
    dz = ABS(a->vz - b->vz);
    sum = dx + dz;
    half = sum >> 1;
    quarter = sum >> 2;
    diff = ABS(dx - dz);
    if (diff < half) {
        sum -= quarter;
    } else if (diff <= half + quarter) {
        sum -= sum >> 3;
    }
    *out = sum;
    return 0;
}

int EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR *a, VECTOR *b, s32 *out)
{
    s32 dx, dy, dz, t;

    dx = ABS(a->vx - b->vx);
    dy = ABS(a->vy - b->vy);
    dz = ABS(a->vz - b->vz);
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
    *out = dx + ((dy + dz) >> 2);
    return 0;
}

int EGSGeometryINT_GetDistancePoint2Line(VECTOR *p, EGSSegment *line, s32 *out)
{
    VECTOR d;
    VECTOR unit;
    VECTOR proj;
    VECTOR foot;
    VECTOR diff;
    s32 dot;

    d.vx = p->vx - line->p0.vx;
    d.vy = p->vy - line->p0.vy;
    d.vz = p->vz - line->p0.vz;
    GTEMath_VectorNormal(&line->p1, &unit);
    EGSMathFIX_VectorDot(&d, &unit, &dot);
    proj.vx = EGSMathFIXMul(unit.vx, dot);
    proj.vy = EGSMathFIXMul(unit.vy, dot);
    proj.vz = EGSMathFIXMul(unit.vz, dot);
    foot.vx = line->p0.vx + proj.vx;
    foot.vy = line->p0.vy + proj.vy;
    foot.vz = line->p0.vz + proj.vz;
    diff.vx = p->vx - foot.vx;
    diff.vy = p->vy - foot.vy;
    diff.vz = p->vz - foot.vz;
    EGSMathINT_VectorModulus(&diff, out);
    return 0;
}

int EGSGeometry_GetDistancePoint2Plane(VECTOR *p, EGSPlane *plane, s32 *out)
{
    VECTOR d;

    d.vx = p->vx - plane->point.vx;
    d.vy = p->vy - plane->point.vy;
    d.vz = p->vz - plane->point.vz;
    *out = EGSMathFIXMul(d.vx, plane->normal.vx) + EGSMathFIXMul(d.vy, plane->normal.vy) +
           EGSMathFIXMul(d.vz, plane->normal.vz);
    return 0;
}

int EGSGeometry_GetDistancePointToPlane(VECTOR *p, VECTOR *point, VECTOR *normal, s32 *out)
{
    VECTOR d;

    d.vx = p->vx - point->vx;
    d.vy = p->vy - point->vy;
    d.vz = p->vz - point->vz;
    *out = EGSMathFIXMul(d.vx, normal->vx) + EGSMathFIXMul(d.vy, normal->vy) +
           EGSMathFIXMul(d.vz, normal->vz);
    return 0;
}

int EGSGeometry_GetOrientationPoint2Plane(VECTOR *p, EGSPlane *plane, s32 *out)
{
    VECTOR d;
    VECTOR ns;
    VECTOR ds;

    d.vx = p->vx - plane->point.vx;
    d.vy = p->vy - plane->point.vy;
    d.vz = p->vz - plane->point.vz;
    EGSMathFIX_VectorScaleForMultiplication(&d, &ds);
    EGSMathFIX_VectorScaleForMultiplication(&plane->normal, &ns);
    *out = EGSMathFIXMul(ds.vx, ns.vx) + EGSMathFIXMul(ds.vy, ns.vy) + EGSMathFIXMul(ds.vz, ns.vz);
    *out = *out == 0 ? 0 : (*out < 0 ? -1 : 1);
    return 0;
}

int EGSGeometry_VectorReflectAboutUnitNormal(VECTOR *v, VECTOR *normal, VECTOR *out)
{
    VECTOR off;
    s32 *o = &off.vx;
    s32 dot;

    EGSMathFIX_VectorDot(v, normal, &dot);
    o[0] = EGSMathFIXMul(normal->vx, dot * 2);
    o[1] = EGSMathFIXMul(normal->vy, dot * 2);
    o[2] = EGSMathFIXMul(normal->vz, dot * 2);
    out->vx = v->vx - off.vx;
    out->vy = v->vy - off.vy;
    out->vz = v->vz - off.vz;
    return 0;
}

int EGSGeometryFIX_VectorReflect(VECTOR *v, VECTOR *normal, VECTOR *out)
{
    VECTOR unit;
    VECTOR off;
    s32 dot;

    GTEMath_VectorNormal(normal, &unit);
    EGSMathFIX_VectorDot(v, &unit, &dot);
    off.vx = EGSMathFIXMul(unit.vx, dot * 2);
    off.vy = EGSMathFIXMul(unit.vy, dot * 2);
    off.vz = EGSMathFIXMul(unit.vz, dot * 2);
    out->vx = v->vx - off.vx;
    out->vy = v->vy - off.vy;
    out->vz = v->vz - off.vz;
    return 0;
}

int EGSGeometry_VectorRotateYXZ(VECTOR *v, VECTOR *angle, VECTOR *out)
{
    MATRIX m;

    if (v->vx != 0 || v->vy != 0 || v->vz != 0) {
        EGSGeometry_AngleToMatrixYXZ(angle, &m);
        ApplyMatrixLV(&m, v, out);
        return 0;
    }
    *out = *v;
    return 0;
}

int EGSGeometry_VectorReverseRotateYXZ(VECTOR *v, VECTOR *angle, VECTOR *out)
{
    MATRIX m;

    if (v->vx != 0 || v->vy != 0 || v->vz != 0) {
        EGSGeometry_AngleToMatrixYXZ(angle, &m);
        ApplyTransposeMatrixLV(&m, v, out);
        return 0;
    }
    *out = *v;
    return 0;
}

int EGSGeometryINT_VectorGetAngleFromXZPlane(VECTOR *v, s32 *out)
{
    s32 xz;

    EGSMathINT_VectorGetXZComponentInline(v, &xz);
    *out = -ratan2(v->vy, xz);
    return 0;
}

int EGSGeometryFIX_VectorGetAngleFromXZPlane(VECTOR *v, s32 *out)
{
    s32 xz;

    EGSMathFIX_VectorGetXZComponent(v, &xz);
    *out = -ratan2(v->vy, xz);
    return 0;
}

int EGSGeometryINT_VectorGetAngleFromYZPlane(VECTOR *v, s32 *out)
{
    s32 yz;

    EGSMathINT_VectorGetYZComponentInline(v, &yz);
    *out = ratan2(v->vx, yz);
    return 0;
}

int EGSGeometryFIX_VectorGetAngleFromYZPlane(VECTOR *v, s32 *out)
{
    s32 yz;

    EGSMathFIX_VectorGetYZComponent(v, &yz);
    *out = ratan2(v->vx, yz);
    return 0;
}

int EGSGeometry_VectorGetXZAngleFromZAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(v->vx, v->vz);
    return 0;
}

int EGSGeometry_VectorGetXZAngleFromXAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(-v->vz, v->vx);
    return 0;
}

int EGSGeometry_VectorGetXYAngleFromXAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(v->vy, v->vx);
    return 0;
}

int EGSGeometry_VectorGetXYAngleFromYAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(-v->vx, v->vy);
    return 0;
}

int EGSGeometry_VectorGetYZAngleFromYAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(v->vz, v->vy);
    return 0;
}

int EGSGeometry_VectorGetYZAngleFromZAxis(VECTOR *v, s32 *out)
{
    *out = ratan2(-v->vy, v->vz);
    return 0;
}

int EGSGeometry_RotationMatrixToAngleYXZ(MATRIX *m, VECTOR *angle)
{
    VECTOR c0, c1, c2;
    VECTOR a;

    c0.vx = m->m[0][0];
    c0.vy = m->m[1][0];
    c0.vz = m->m[2][0];
    c1.vx = m->m[0][1];
    c1.vy = m->m[1][1];
    c1.vz = m->m[2][1];
    c2.vx = m->m[0][2];
    c2.vy = m->m[1][2];
    c2.vz = m->m[2][2];
    EGSGeometryFIX_VectorGetAngleFromXZPlane(&c2, &angle->vx);
    if (c2.vx == 0 && c2.vz == 0) {
        angle->vy = 0;
    } else {
        angle->vy = ratan2(c2.vx, c2.vz);
    }
    a.vx = angle->vx;
    a.vy = angle->vy;
    a.vz = 0;
    EGSGeometry_VectorReverseRotateYXZ(&c0, &a, &c0);
    angle->vz = ratan2(c0.vy, c0.vx);
    return 0;
}

int EGSGeometryINT_VectorToAngleYX(VECTOR *v, VECTOR *angle)
{
    angle->vy = ratan2(v->vx, v->vz);
    EGSGeometryINT_VectorGetAngleFromXZPlane(v, &angle->vx);
    angle->vz = 0;
    return 0;
}

int EGSGeometry_AngleToVectorYX(VECTOR *angle, VECTOR *out)
{
    MATRIX m;

    EGSGeometry_AngleToMatrixYXZ(angle, &m);
    out->vx = m.m[0][2];
    out->vy = m.m[1][2];
    out->vz = m.m[2][2];
    return 0;
}

int EGSGeometry_UnitVectorToRotationMatrix(VECTOR *v, s32 roll, MATRIX *m)
{
    VECTOR x, y, z;
    MATRIX rm;
    VECTOR angle;
    s32 isZero;
    s32 zero;

    if (v == NULL) {
        return 1;
    }
    isZero = v->vx == 0 && v->vy == 0 && v->vz == 0;
    if (isZero == 1) {
        return 1;
    }
    z = *v;
    x.vy = 0;
    x.vx = z.vz;
    x.vz = -z.vx;
    zero = x.vx == 0 && x.vz == 0;
    if (zero == 1) {
        x.vz = 0x1000;
    }
    GTEMath_VectorNormal(&x, &x);
    OuterProduct12(&z, &x, &y);
    m->m[0][0] = x.vx;
    m->m[1][0] = x.vy;
    m->m[2][0] = x.vz;
    m->m[0][1] = y.vx;
    m->m[1][1] = y.vy;
    m->m[2][1] = y.vz;
    m->m[0][2] = z.vx;
    m->m[1][2] = z.vy;
    m->m[2][2] = z.vz;
    if (roll != 0) {
        angle.vx = 0;
        angle.vy = 0;
        angle.vz = roll;
        EGSGeometry_AngleToMatrixYXZ(&angle, &rm);
        MulMatrix0(m, &rm, m);
    }
    return 1;
}

int EGSGeometry_VectorToRotationMatrix(VECTOR *v, s32 arg1, MATRIX *m)
{
    VECTOR unit;

    GTEMath_VectorNormal(v, &unit);
    EGSGeometry_UnitVectorToRotationMatrix(&unit, arg1, m);
    return 1;
}

int EGSGeometry_RayToMatrix(EGSSegment *ray, s32 arg1, MATRIX *m)
{
    VECTOR unit;

    GTEMath_VectorNormal(&ray->p1, &unit);
    EGSGeometry_UnitVectorToRotationMatrix(&unit, arg1, m);
    m->t[0] = ray->p0.vx;
    m->t[1] = ray->p0.vy;
    m->t[2] = ray->p0.vz;
    return 1;
}

int EGSGeometry_ScalarInterpolate(s32 from, s32 to, s32 step, u8 mode, s32 *out, s32 *remain)
{
    s32 d = to - from;

    if (mode == 1) {
        *out = EGSMathFIXMul(d, step);
    } else {
        step = ABS(step);
        if (d >= 0) {
            if (d < step) {
                *out = d;
            } else {
                *out = step;
            }
        } else if (-d < step) {
            *out = d;
        } else {
            *out = -step;
        }
    }
    *remain = d - *out;
    return 0;
}

int EGSGeometry_VectorInterpolate(VECTOR *from, VECTOR *to, VECTOR *step, u8 mode, VECTOR *out, VECTOR *remain)
{
    EGSGeometry_ScalarInterpolate(from->vx, to->vx, step->vx, mode, &out->vx, &remain->vx);
    EGSGeometry_ScalarInterpolate(from->vy, to->vy, step->vy, mode, &out->vy, &remain->vy);
    EGSGeometry_ScalarInterpolate(from->vz, to->vz, step->vz, mode, &out->vz, &remain->vz);
    return 0;
}

int EGSGeometry_AngleInterpolate(s32 from, s32 to, s32 step, u8 mode, s32 *out, s32 *remain)
{
    s32 d = to - from;

    if (d > 0x800) {
        d -= 0x1000;
    } else if (d < -0x800) {
        d += 0x1000;
    }
    if (mode == 1) {
        *out = EGSMathFIXMul(d, step);
    } else {
        step = ABS(step);
        if (d >= 0) {
            if (d < step) {
                *out = d;
            } else {
                *out = step;
            }
        } else if (-d < step) {
            *out = d;
        } else {
            *out = -step;
        }
    }
    *remain = d - *out;
    return 0;
}

int EGSGeometry_PointProjectOnPlane(VECTOR *p, EGSPlane *plane, VECTOR *out)
{
    VECTOR off;
    s32 *o = &off.vx;
    s32 dist;

    EGSGeometry_GetDistancePoint2Plane(p, plane, &dist);
    o[0] = EGSMathFIXMul(plane->normal.vx, dist);
    o[1] = EGSMathFIXMul(plane->normal.vy, dist);
    o[2] = EGSMathFIXMul(plane->normal.vz, dist);
    out->vx = p->vx - off.vx;
    out->vy = p->vy - off.vy;
    out->vz = p->vz - off.vz;
    return 0;
}

int EGSGeometry_VectorProjectOnUnitVector(VECTOR *v, VECTOR *unit, VECTOR *out)
{
    s32 dot;

    EGSMathFIX_VectorDot(v, unit, &dot);
    out->vx = EGSMathFIXMul(unit->vx, dot);
    out->vy = EGSMathFIXMul(unit->vy, dot);
    out->vz = EGSMathFIXMul(unit->vz, dot);
    return 0;
}

int EGSGeometry_VectorProjectOnPlane(VECTOR *v, VECTOR *normal, VECTOR *out)
{
    VECTOR off;
    s32 *o = &off.vx;
    s32 dot;

    EGSMathFIX_VectorDot(v, normal, &dot);
    o[0] = EGSMathFIXMul(normal->vx, dot);
    o[1] = EGSMathFIXMul(normal->vy, dot);
    o[2] = EGSMathFIXMul(normal->vz, dot);
    out->vx = v->vx - off.vx;
    out->vy = v->vy - off.vy;
    out->vz = v->vz - off.vz;
    return 0;
}

int EGSGeometryFIX_VectorProjectOnPlaneAlongY(VECTOR *v, VECTOR *normal, VECTOR *out)
{
    VECTOR yAxis = { 0, 0x1000, 0, 0 };
    VECTOR side;
    VECTOR dir;
    s32 tmp;

    OuterProduct12(&yAxis, v, &side);
    OuterProduct12(&side, normal, &dir);
    out->vx = v->vx;
    out->vz = v->vz;
    if (dir.vx != 0 || dir.vz != 0) {
        if (ABS(dir.vx) >= ABS(dir.vz)) {
            EGSMathFIX_ValueMultiply(out->vx, dir.vy, &tmp);
            out->vy = EGSMathFIXDiv(tmp, dir.vx);
        } else {
            EGSMathFIX_ValueMultiply(out->vz, dir.vy, &tmp);
            out->vy = EGSMathFIXDiv(tmp, dir.vz);
        }
    } else {
        out->vy = 0;
    }
    return 0;
}

int EGSGeometry_VectorAlignAlongPlane(VECTOR *v, VECTOR *normal, VECTOR *out)
{
    VECTOR d;
    VECTOR proj;
    s32 *o = &proj.vx;
    s32 dot;
    s32 len;

    EGSMathFIX_VectorDot(v, normal, &dot);
    o[0] = EGSMathFIXMul(normal->vx, dot);
    o[1] = EGSMathFIXMul(normal->vy, dot);
    o[2] = EGSMathFIXMul(normal->vz, dot);
    d.vx = v->vx - proj.vx;
    d.vy = v->vy - proj.vy;
    d.vz = v->vz - proj.vz;
    if (d.vx == 0 && d.vy == 0 && d.vz == 0) {
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return 1;
    }
    GTEMath_VectorNormal(&d, &d);
    EGSMath_VectorModulus(v, &len);
    out->vx = EGSMathFIXMul(d.vx, len);
    out->vy = EGSMathFIXMul(d.vy, len);
    out->vz = EGSMathFIXMul(d.vz, len);
    return 0;
}

int EGSGeometryFIX_PlaneGetUnitUphillGradient(VECTOR *normal, VECTOR *out)
{
    s32 sq;
    s32 tmp;

    EGSMathFIX_ValueMultiply(normal->vy, normal->vy, &sq);
    EGSMathFIX_ValueSquareRoot(0x1000 - sq, &out->vy);
    if (out->vy == 0) {
        out->vx = 0;
        out->vz = 0;
    } else {
        EGSMathFIX_ValueMultiply(-normal->vx, normal->vy, &tmp);
        out->vx = EGSMathFIXDiv(tmp, out->vy);
        EGSMathFIX_ValueMultiply(-normal->vz, normal->vy, &tmp);
        out->vz = EGSMathFIXDiv(tmp, out->vy);
    }
    return 0;
}

int EGSGeometryINT_PlaneGetY(VECTOR *origin, VECTOR *e0, VECTOR *e1, VECTOR *p, s32 *y)
{
    s32 ox = origin->vx, oy = origin->vy, oz = origin->vz;
    s32 ax = e0->vx, ay = e0->vy, az = e0->vz;
    s32 bx = e1->vx, by = e1->vy, bz = e1->vz;
    s32 px = p->vx, pz = p->vz;
    s32 det = ax * bz - az * bx;
    s32 u, v;

    if (det == 0) {
        *y = p->vy;
        return 7;
    }
    u = -bx * (pz - oz) + bz * (px - ox);
    v = ax * (pz - oz) - az * (px - ox);
    *y = oy + (ay * u + by * v) / det;
    return 0;
}

int EGSGeometry_FaceGetUnitNormal(EGSFace *face)
{
    int ret = EGSGeometry_FaceGetNonUnitNormal(face);

    if (ret == 0) {
        GTEMath_VectorNormal(&face->normal, &face->normal);
        face->normalIsUnit = 1;
    }
    return ret;
}

int EGSGeometry_FaceGetNonUnitNormal(EGSFace *face)
{
    VECTOR e0, e1;
    VECTOR s0, s1;

    if (face->type != 3 && face->type != 4) {
        return 1;
    }
    e0.vx = face->v[2].vx - face->v[1].vx;
    e0.vy = face->v[2].vy - face->v[1].vy;
    e0.vz = face->v[2].vz - face->v[1].vz;
    e1.vx = face->v[0].vx - face->v[1].vx;
    e1.vy = face->v[0].vy - face->v[1].vy;
    e1.vz = face->v[0].vz - face->v[1].vz;
    EGSMathFIX_VectorScaleForMultiplication(&e0, &s0);
    EGSMathFIX_VectorScaleForMultiplication(&e1, &s1);
    OuterProduct12(&s0, &s1, &face->normal);
    face->normalIsUnit = 0;
    return 0;
}

#ifdef NON_MATCHING
/* sched: product/mult order differs (n stores and p reloads match). Original issues e1.vz*e0.vx right after
 * e1.vz (2nd mult); ours delays it past n.vy's mults. Plain n.vx stores and all 64 operand-order swaps don't help. */
int EGSGeometryINT_PointProjectOnFacePlaneAlongY(VECTOR *p, EGSFace *face, u8 *ok, s32 *y)
{
    VECTOR o;
    VECTOR e0;
    VECTOR e1;
    VECTOR n;
    u8 hit;

    o = face->v[1];
    e0.vx = face->v[2].vx - face->v[1].vx;
    e0.vy = face->v[2].vy - face->v[1].vy;
    e0.vz = face->v[2].vz - face->v[1].vz;
    e1.vx = face->v[0].vx - face->v[1].vx;
    e1.vy = face->v[0].vy - face->v[1].vy;
    e1.vz = face->v[0].vz - face->v[1].vz;
    ((s32 *)&n)[0] = e1.vx * e0.vz - e1.vz * e0.vx;
    ((s32 *)&n)[1] = (p->vz - o.vz) * e1.vx - (p->vx - o.vx) * e1.vz;
    ((s32 *)&n)[2] = (p->vx - o.vx) * e0.vz - (p->vz - o.vz) * e0.vx;
    if (n.vx != 0) {
        *y = o.vy + (n.vy * e0.vy + n.vz * e1.vy) / n.vx;
        hit = 1;
    } else {
        hit = 0;
    }
    *ok = hit;
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_PointProjectOnFacePlaneAlongY);
#endif

#ifdef NON_MATCHING
/* sched: product/mult order differs; plane basis selection matches */
int EGSGeometryINT_PointProjectOnPlaneAlongY(VECTOR *p, EGSPlane *plane, u8 *ok, s32 *y)
{
    VECTOR o;
    VECTOR e0;
    VECTOR e1;
    VECTOR n;
    s32 ax, ay, az, max;
    u8 hit;

    o = plane->point;
    n = plane->normal;
    ax = ABS(n.vx);
    ay = ABS(n.vy);
    az = ABS(n.vz);
    max = MAX(ax, MAX(ay, az));
    if (max == ax) {
        e0.vz = 0;
        e1.vy = 0;
        e0.vx = -n.vy;
        e0.vy = n.vx;
        e1.vx = -n.vz;
        e1.vz = n.vx;
    } else if (max == ay) {
        e0.vx = 0;
        e1.vz = 0;
        e0.vy = -n.vz;
        e0.vz = n.vy;
        e1.vx = n.vy;
        e1.vy = -n.vx;
    } else {
        e0.vy = 0;
        e1.vx = 0;
        e0.vx = n.vz;
        e0.vz = -n.vx;
        e1.vy = n.vz;
        e1.vz = -n.vy;
    }
    ((s32 *)&n)[0] = e1.vx * e0.vz - e1.vz * e0.vx;
    ((s32 *)&n)[1] = (p->vz - o.vz) * e1.vx - (p->vx - o.vx) * e1.vz;
    ((s32 *)&n)[2] = (p->vx - o.vx) * e0.vz - (p->vz - o.vz) * e0.vx;
    if (n.vx != 0) {
        *y = o.vy + (n.vy * e0.vy + n.vz * e1.vy) / n.vx;
        hit = 1;
    } else {
        hit = 0;
    }
    *ok = hit;
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_PointProjectOnPlaneAlongY);
#endif

int EGSGeometry_TransformPoint(VECTOR *in, MATRIX *m, VECTOR *out)
{
    ApplyMatrixLV(m, in, out);
    out->vx += m->t[0];
    out->vy += m->t[1];
    out->vz += m->t[2];
    return 0;
}

int EGSGeometry_ReverseTransformPoint(VECTOR *in, MATRIX *m, VECTOR *out)
{
    EGSMath_VectorApplyReverseMatrix(in, m, out);
    return 0;
}

int EGSGeometry_TransformVector(VECTOR *in, MATRIX *m, VECTOR *out)
{
    ApplyMatrixLV(m, in, out);
    return 0;
}

int EGSGeometry_ReverseTransformVector(VECTOR *in, MATRIX *m, VECTOR *out)
{
    ApplyTransposeMatrixLV(m, in, out);
    return 0;
}

int EGSGeometry_TransformSegment(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    ApplyMatrixLV(m, &in->p0, &out->p0);
    out->p0.vx += m->t[0];
    out->p0.vy += m->t[1];
    out->p0.vz += m->t[2];
    ApplyMatrixLV(m, &in->p1, &out->p1);
    return 0;
}

int EGSGeometry_ReverseTransformSegment(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    MATRIX inv;

    EGSMath_MatrixInvert(m, &inv);
    ApplyMatrixLV(&inv, &in->p0, &out->p0);
    out->p0.vx += inv.t[0];
    out->p0.vy += inv.t[1];
    out->p0.vz += inv.t[2];
    ApplyMatrixLV(&inv, &in->p1, &out->p1);
    return 1;
}

int EGSGeometry_TransformLine(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    ApplyMatrixLV(m, &in->p0, &out->p0);
    out->p0.vx += m->t[0];
    out->p0.vy += m->t[1];
    out->p0.vz += m->t[2];
    ApplyMatrixLV(m, &in->p1, &out->p1);
    return 0;
}

int EGSGeometry_ReverseTransformLine(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    EGSGeometry_ReverseTransformSegment(in, m, out);
    return 0;
}

int EGSGeometry_TransformRay(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    ApplyMatrixLV(m, &in->p0, &out->p0);
    out->p0.vx += m->t[0];
    out->p0.vy += m->t[1];
    out->p0.vz += m->t[2];
    ApplyMatrixLV(m, &in->p1, &out->p1);
    return 0;
}

int EGSGeometry_ReverseTransformRay(EGSSegment *in, MATRIX *m, EGSSegment *out)
{
    EGSGeometry_ReverseTransformSegment(in, m, out);
    return 0;
}

int EGSGeometry_IsIntersectUnrotBox2UnrotBox(EGSBox *a, EGSBox *b, s32 margin, u8 *hit)
{
    if (b->max.vx + margin >= a->min.vx && a->max.vx >= b->min.vx - margin &&
        b->max.vy + margin >= a->min.vy && a->max.vy >= b->min.vy - margin &&
        b->max.vz + margin >= a->min.vz && a->max.vz >= b->min.vz - margin) {
        *hit = 1;
    } else {
        *hit = 0;
    }
    return 0;
}

int EGSGeometry_IsIntersectXZUnrotBox2UnrotBox(EGSBox *a, EGSBox *b, s32 margin, u8 *hit)
{
    if (b->max.vx + margin >= a->min.vx && a->max.vx >= b->min.vx - margin &&
        b->max.vz + margin >= a->min.vz && a->max.vz >= b->min.vz - margin) {
        *hit = 1;
    } else {
        *hit = 0;
    }
    return 0;
}

int EGSGeometry_IsIntersectPoint2UnrotBox(VECTOR *p, EGSBox *b, s32 margin, u8 *hit)
{
    if (p->vx >= b->min.vx - margin && b->max.vx + margin >= p->vx &&
        p->vy >= b->min.vy - margin && b->max.vy + margin >= p->vy &&
        p->vz >= b->min.vz - margin && b->max.vz + margin >= p->vz) {
        *hit = 1;
    } else {
        *hit = 0;
    }
    return 0;
}

int EGSGeometry_IsIntersectXZPoint2UnrotBox(VECTOR *p, EGSBox *b, s32 margin, u8 *hit)
{
    if (p->vx >= b->min.vx - margin && b->max.vx + margin >= p->vx &&
        p->vz >= b->min.vz - margin && b->max.vz + margin >= p->vz) {
        *hit = 1;
    } else {
        *hit = 0;
    }
    return 0;
}

int EGSGeometry_GetBoundingBox(VECTOR *pts, s32 n, EGSBox *out)
{
    EGSBox box;
    s32 i;

    if (pts == NULL || n <= 0 || out == NULL) {
        return 1;
    }
    box.min = *pts;
    box.max = *pts;
    for (i = 1; i < n; i++) {
        box.min.vx = MIN(pts[i].vx, box.min.vx);
        box.min.vy = MIN(pts[i].vy, box.min.vy);
        box.min.vz = MIN(pts[i].vz, box.min.vz);
        box.max.vx = MAX(pts[i].vx, box.max.vx);
        box.max.vy = MAX(pts[i].vy, box.max.vy);
        box.max.vz = MAX(pts[i].vz, box.max.vz);
    }
    *out = box;
    return 0;
}

int EGSGeometry_GetBoxPoints(EGSBox *box, SVECTOR *pts, MATRIX *m)
{
    VECTOR size;
    MATRIX sm;
    VECTOR axis;
    s32 i;

    if (box == NULL || pts == NULL) {
        return 1;
    }
    pts[0].vx = box->min.vx;
    pts[0].vy = box->min.vy;
    pts[0].vz = box->min.vz;
    if (m != NULL) {
        ApplyMatrixSV(m, &pts[0], &pts[0]);
        pts[0].vx += m->t[0];
        pts[0].vy += m->t[1];
        pts[0].vz += m->t[2];
        size.vx = box->max.vx - box->min.vx;
        size.vy = box->max.vy - box->min.vy;
        size.vz = box->max.vz - box->min.vz;
        sm = *m;
        ScaleMatrix(&sm, &size);
    } else {
        sm = gEGSIdentityMatrix;
        size.vx = box->max.vx - box->min.vx;
        size.vy = box->max.vy - box->min.vy;
        size.vz = box->max.vz - box->min.vz;
        sm.m[0][0] = size.vx;
        sm.m[1][1] = size.vy;
        sm.m[2][2] = size.vz;
    }
    for (i = 1; i < 8; i++) {
        pts[i] = pts[0];
        if (i & 1) {
            axis.vx = sm.m[0][0];
            axis.vy = sm.m[1][0];
            axis.vz = sm.m[2][0];
            pts[i].vx += axis.vx;
            pts[i].vy += axis.vy;
            pts[i].vz += axis.vz;
        }
        if (i & 2) {
            axis.vx = sm.m[0][1];
            axis.vy = sm.m[1][1];
            axis.vz = sm.m[2][1];
            pts[i].vx += axis.vx;
            pts[i].vy += axis.vy;
            pts[i].vz += axis.vz;
        }
        if (i & 4) {
            axis.vx = sm.m[0][2];
            axis.vy = sm.m[1][2];
            axis.vz = sm.m[2][2];
            pts[i].vx += axis.vx;
            pts[i].vy += axis.vy;
            pts[i].vz += axis.vz;
        }
    }
    return 0;
}

int EGSGeometry_IsIntersectBoundingBox2BoundingBox(VECTOR *pts0, s32 n0, VECTOR *pts1, s32 n1, s32 margin, u8 *hit)
{
    EGSBox box0, box1;

    EGSGeometry_GetBoundingBox(pts0, n0, &box0);
    EGSGeometry_GetBoundingBox(pts1, n1, &box1);
    EGSGeometry_IsIntersectUnrotBox2UnrotBox(&box0, &box1, margin, hit);
    return 0;
}

int EGSGeometry_IsIntersectXZBoundingBox2BoundingBox(VECTOR *pts0, s32 n0, VECTOR *pts1, s32 n1, s32 margin, u8 *hit)
{
    EGSBox box0, box1;

    EGSGeometry_GetBoundingBox(pts0, n0, &box0);
    EGSGeometry_GetBoundingBox(pts1, n1, &box1);
    EGSGeometry_IsIntersectXZUnrotBox2UnrotBox(&box0, &box1, margin, hit);
    return 0;
}

int EGSGeometry_IsIntersectPoint2BoundingBox(VECTOR *p, void *a1, void *a2, s32 margin, u8 *hit)
{
    EGSBox box;

    EGSGeometry_GetBoundingBox(a1, a2, &box);
    EGSGeometry_IsIntersectPoint2UnrotBox(p, &box, margin, hit);
    return 0;
}

int EGSGeometry_IsIntersectXZPoint2BoundingBox(VECTOR *p, void *a1, void *a2, s32 margin, u8 *hit)
{
    EGSBox box;

    EGSGeometry_GetBoundingBox(a1, a2, &box);
    EGSGeometry_IsIntersectXZPoint2UnrotBox(p, &box, margin, hit);
    return 0;
}

#ifdef NON_MATCHING
/* regalloc: abs(margin) s0 vs v1, t/divisor/quotient regs in hit-point blocks */
int EGSGeometry_GetIntersectRaySegment2UnrotBox(EGSSegment *ray, EGSBox *box, s32 margin, u8 *hit, VECTOR *point, VECTOR *normal)
{
    EGSSegment seg;
    EGSBox b;
    VECTOR end;
    VECTOR pair[2];
    EGSBox bb;
    VECTOR hp;
    u8 found;
    s32 am;

    seg = *ray;
    b = *box;
    end.vx = seg.p0.vx + seg.p1.vx;
    end.vy = seg.p0.vy + seg.p1.vy;
    end.vz = seg.p0.vz + seg.p1.vz;
    point->vx = point->vy = point->vz = 0;
    pair[0] = seg.p0;
    pair[1] = end;
    EGSGeometry_GetBoundingBox(pair, 2, &bb);
    EGSGeometry_IsIntersectUnrotBox2UnrotBox(&bb, box, margin, hit);
    am = ABS(margin);
    if (*hit == 0) {
        return 0;
    }
    *hit = 0;
    if (box->min.vx + am < seg.p0.vx && seg.p0.vx < box->max.vx - am &&
        box->min.vy + am < seg.p0.vy && seg.p0.vy < box->max.vy - am &&
        box->min.vz + am < seg.p0.vz && seg.p0.vz < box->max.vz - am) {
        return 0;
    }

    if (seg.p1.vx > 0 && b.min.vx + margin >= seg.p0.vx && end.vx >= b.min.vx - margin) {
        hp.vx = b.min.vx;
        normal->vx = -0x1000;
        normal->vy = 0;
        normal->vz = 0;
        found = 1;
    } else if (seg.p1.vx < 0 && seg.p0.vx >= b.max.vx - margin && b.max.vx + margin >= end.vx) {
        hp.vx = b.max.vx;
        normal->vx = 0x1000;
        normal->vy = 0;
        normal->vz = 0;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vx - seg.p0.vx;
        hp.vy = seg.p1.vy * t / seg.p1.vx + seg.p0.vy;
        hp.vz = seg.p1.vz * t / seg.p1.vx + seg.p0.vz;
        if (hp.vy >= b.min.vy - margin && b.max.vy + margin >= hp.vy &&
            hp.vz >= b.min.vz - margin && b.max.vz + margin >= hp.vz) {
            goto found_hit;
        }
    }

    if (seg.p1.vy > 0 && b.min.vy + margin >= seg.p0.vy && end.vy >= b.min.vy - margin) {
        hp.vy = b.min.vy;
        normal->vx = 0;
        normal->vy = -0x1000;
        normal->vz = 0;
        found = 1;
    } else if (seg.p1.vy < 0 && seg.p0.vy >= b.max.vy - margin && b.max.vy + margin >= end.vy) {
        hp.vy = b.max.vy;
        normal->vx = 0;
        normal->vy = 0x1000;
        normal->vz = 0;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vy - seg.p0.vy;
        hp.vx = seg.p1.vx * t / seg.p1.vy + seg.p0.vx;
        hp.vz = seg.p1.vz * t / seg.p1.vy + seg.p0.vz;
        if (hp.vx >= b.min.vx - margin && b.max.vx + margin >= hp.vx &&
            hp.vz >= b.min.vz - margin && b.max.vz + margin >= hp.vz) {
            goto found_hit;
        }
    }

    if (seg.p1.vz > 0 && b.min.vz + margin >= seg.p0.vz && end.vz >= b.min.vz - margin) {
        hp.vz = b.min.vz;
        normal->vx = 0;
        normal->vy = 0;
        normal->vz = -0x1000;
        found = 1;
    } else if (seg.p1.vz < 0 && seg.p0.vz >= b.max.vz - margin && b.max.vz + margin >= end.vz) {
        hp.vz = b.max.vz;
        normal->vx = 0;
        normal->vy = 0;
        normal->vz = 0x1000;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vz - seg.p0.vz;
        hp.vx = seg.p1.vx * t / seg.p1.vz + seg.p0.vx;
        hp.vy = seg.p1.vy * t / seg.p1.vz + seg.p0.vy;
        if (hp.vx >= b.min.vx - margin && b.max.vx + margin >= hp.vx &&
            hp.vy >= b.min.vy - margin && b.max.vy + margin >= hp.vy) {
            goto found_hit;
        }
    }
    return 0;

found_hit:
    *hit = 1;
    *point = hp;
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetIntersectRaySegment2UnrotBox);
#endif

#ifdef NON_MATCHING
/* regalloc: t/divisor/quotient regs swapped in the three hit-point blocks; rest matches */
int EGSGeometry_IsIntersectLineSegment2UnrotBox(EGSSegment *segment, EGSBox *box, s32 margin, u8 *hit)
{
    EGSSegment seg;
    EGSBox b;
    VECTOR end;
    VECTOR pair[2];
    EGSBox bb;
    VECTOR hp;
    u8 found;

    seg = *segment;
    b = *box;
    margin = ABS(margin);
    end.vx = seg.p0.vx + seg.p1.vx;
    end.vy = seg.p0.vy + seg.p1.vy;
    end.vz = seg.p0.vz + seg.p1.vz;
    pair[0] = seg.p0;
    pair[1] = end;
    EGSGeometry_GetBoundingBox(pair, 2, &bb);
    EGSGeometry_IsIntersectUnrotBox2UnrotBox(&bb, box, margin, hit);
    if (*hit == 0) {
        return 0;
    }
    *hit = 0;
    EGSGeometry_IsIntersectPoint2UnrotBox(&seg.p0, box, margin, hit);
    if (*hit == 1) {
        return 0;
    }
    EGSGeometry_IsIntersectPoint2UnrotBox(&end, box, margin, hit);
    if (*hit == 1) {
        return 0;
    }

    if (seg.p1.vx > 0 &&
        ((b.min.vx + margin >= seg.p0.vx && end.vx >= b.min.vx - margin) ||
         (b.min.vx + margin >= end.vx && seg.p0.vx >= b.min.vx - margin))) {
        hp.vx = b.min.vx;
        found = 1;
    } else if (seg.p1.vx < 0 &&
               ((seg.p0.vx >= b.max.vx - margin && b.max.vx + margin >= end.vx) ||
                (end.vx >= b.max.vx - margin && b.max.vx + margin >= seg.p0.vx))) {
        hp.vx = b.max.vx;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vx - seg.p0.vx;
        hp.vy = seg.p1.vy * t / seg.p1.vx + seg.p0.vy;
        hp.vz = seg.p1.vz * t / seg.p1.vx + seg.p0.vz;
        if (hp.vy >= b.min.vy - margin && b.max.vy + margin >= hp.vy &&
            hp.vz >= b.min.vz - margin && b.max.vz + margin >= hp.vz) {
            *hit = 1;
            return 0;
        }
    }

    if (seg.p1.vy > 0 &&
        ((b.min.vy + margin >= seg.p0.vy && end.vy >= b.min.vy - margin) ||
         (b.min.vy + margin >= end.vy && seg.p0.vy >= b.min.vy - margin))) {
        hp.vy = b.min.vy;
        found = 1;
    } else if (seg.p1.vy < 0 &&
               ((seg.p0.vy >= b.max.vy - margin && b.max.vy + margin >= end.vy) ||
                (end.vy >= b.max.vy - margin && b.max.vy + margin >= seg.p0.vy))) {
        hp.vy = b.max.vy;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vy - seg.p0.vy;
        hp.vx = seg.p1.vx * t / seg.p1.vy + seg.p0.vx;
        hp.vz = seg.p1.vz * t / seg.p1.vy + seg.p0.vz;
        if (hp.vx >= b.min.vx - margin && b.max.vx + margin >= hp.vx &&
            hp.vz >= b.min.vz - margin && b.max.vz + margin >= hp.vz) {
            *hit = 1;
            return 0;
        }
    }

    if (seg.p1.vz > 0 &&
        ((b.min.vz + margin >= seg.p0.vz && end.vz >= b.min.vz - margin) ||
         (b.min.vz + margin >= end.vz && seg.p0.vz >= b.min.vz - margin))) {
        hp.vz = b.min.vz;
        found = 1;
    } else if (seg.p1.vz < 0 &&
               ((seg.p0.vz >= b.max.vz - margin && b.max.vz + margin >= end.vz) ||
                (end.vz >= b.max.vz - margin && b.max.vz + margin >= seg.p0.vz))) {
        hp.vz = b.max.vz;
        found = 1;
    } else {
        found = 0;
    }
    if (found == 1) {
        s32 t = hp.vz - seg.p0.vz;
        hp.vx = seg.p1.vx * t / seg.p1.vz + seg.p0.vx;
        hp.vy = seg.p1.vy * t / seg.p1.vz + seg.p0.vy;
        if (hp.vx >= b.min.vx - margin && b.max.vx + margin >= hp.vx &&
            hp.vy >= b.min.vy - margin && b.max.vy + margin >= hp.vy) {
            *hit = 1;
            return 0;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectLineSegment2UnrotBox);
#endif
