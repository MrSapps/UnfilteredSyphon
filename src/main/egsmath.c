#include "common.h"

#include "egsmath.h"
#include "egsframe.h"

const MATRIX gEGSMathIdentityMatrix = { { { 0x1000, 0, 0 }, { 0, 0x1000, 0 }, { 0, 0, 0x1000 } }, { 0, 0, 0 } };
/* .sdata */
MATRIX *pEGSMathIdentityMatrix = (MATRIX *)&gEGSMathIdentityMatrix;

int EGSMath_VectorModulus(VECTOR *v, s32 *out);
int EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR *a, VECTOR *b, s32 *out);

#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

static const VECTOR sZeroVector = { 0, 0, 0, 0 };
int EGSMath_MatrixInvert(MATRIX *in, MATRIX *out);
int EGSMathINT_Solve3LinearEquationsIn3Unknowns(s32 m[3][4], s32 *out);

int EGSMath_ValueModulus(s32 value, s32 modulus, s32 *out)
{
    s32 v;

    *out = value;
    if (modulus == 0) {
        return 1;
    }
    if (value < 0) {
        *out = value + modulus;
    } else if (value >= modulus) {
        *out = value - modulus;
    }
    v = *out;
    if (v < 0 || v >= modulus) {
        *out = v % modulus;
    }
    return 0;
}

int EGSMathFIX_ValueSquareRoot(s32 value, s32 *out)
{
    *out = SquareRoot12(value);
    return 0;
}

int EGSMathINT_ValueSquareRoot(s32 value, s32 *out)
{
    *out = SquareRoot0(value);
    return 0;
}

int EGSMath_ValueArctan(s32 y, s32 x, s32 *out)
{
    *out = ratan2(y, x);
    return 0;
}

int EGSMath_ValueSin(s32 angle, s32 *out)
{
    *out = rsin(angle);
    return 0;
}

int EGSMath_ValueCos(s32 angle, s32 *out)
{
    *out = rcos(angle);
    return 0;
}

int EGSMath_ValueTan(s32 angle, s32 *out)
{
    s32 s = rsin(angle);
    s32 c = rcos(angle);

    if (c == 0) {
        *out = 0;
        return 7;
    }
    *out = EGSMathFIXDiv(s, c);
    return 0;
}

int EGSMath_ValueRandom(s32 lo, s32 hi, s32 *out)
{
    int ret = 0;

    if (hi < lo) {
        ret = 1;
    } else {
        *out = rand() % (hi - lo + 1) + lo;
    }
    return ret;
}

int EGSMath_VectorINTtoFIX(VECTOR *in, VECTOR *out)
{
    out->vx = in->vx << 12;
    out->vy = in->vy << 12;
    out->vz = in->vz << 12;
    return 0;
}

int EGSMath_VectorFIXtoINT(VECTOR *in, VECTOR *out)
{
    out->vx = in->vx >= 0 ? in->vx >> 12 : -(-in->vx >> 12);
    out->vy = in->vy >= 0 ? in->vy >> 12 : -(-in->vy >> 12);
    out->vz = in->vz >= 0 ? in->vz >> 12 : -(-in->vz >> 12);
    return 0;
}

int EGSMath_VectorModulus(VECTOR *v, s32 *out)
{
    VECTOR sq = sZeroVector;
    VECTOR origin = sZeroVector;
    s32 ax, ay, az;

    if ((ax = ABS(v->vx)) > 0x1A20BD || (ay = ABS(v->vy)) > 0x1A20BD || (az = ABS(v->vz)) > 0x1A20BD) {
        EGSGeometry_GetFuzzyDistancePoint2Point(&origin, v, out);
        return 0;
    }
    if (ax >= 0x6883 || ay >= 0x6883 || az >= 0x6883) {
        if (v->vx != 0) {
            sq.vx = EGSMathFIXMul(v->vx, v->vx);
        }
        if (v->vy != 0) {
            sq.vy = EGSMathFIXMul(v->vy, v->vy);
        }
        if (v->vz != 0) {
            sq.vz = EGSMathFIXMul(v->vz, v->vz);
        }
        *out = SquareRoot12(sq.vx + sq.vy + sq.vz);
    } else {
        *out = SquareRoot0(v->vx * v->vx + v->vy * v->vy + v->vz * v->vz);
    }
    return 0;
}

int EGSMathFIX_VectorModulus(VECTOR *v, s32 *out)
{
    EGSMath_VectorModulus(v, out);
    return 0;
}

int EGSMathINT_VectorModulus(VECTOR *v, s32 *out)
{
    EGSMath_VectorModulus(v, out);
    return 0;
}

int EGSMathINT_VectorScaleToLength(VECTOR *v, s32 length, VECTOR *out)
{
    s32 modulus;

    EGSMathINT_VectorModulus(v, &modulus);
    if (modulus != 0) {
        out->vx = v->vx * length / modulus;
        out->vy = v->vy * length / modulus;
        out->vz = v->vz * length / modulus;
        return 0;
    }
    return 7;
}

int EGSMathFIX_VectorScaleToLength(VECTOR *v, s32 length, VECTOR *out)
{
    s32 unit[3];
    s32 *u = unit;
    s32 modulus;

    EGSMathFIX_VectorModulus(v, &modulus);
    if (modulus != 0) {
        u[0] = EGSMathFIXDiv(v->vx, modulus);
        u[1] = EGSMathFIXDiv(v->vy, modulus);
        u[2] = EGSMathFIXDiv(v->vz, modulus);
        out->vx = EGSMathFIXMul(unit[0], length);
        out->vy = EGSMathFIXMul(unit[1], length);
        out->vz = EGSMathFIXMul(unit[2], length);
        return 0;
    }
    return 7;
}

int EGSMathFIX_VectorClipToLength(VECTOR *v, s32 length, VECTOR *out)
{
    s32 modulus;

    if (length == 0) {
        out->vx = out->vy = out->vz = 0;
    } else {
        EGSMath_VectorModulus(v, &modulus);
        length = length >= 0 ? length : -length;
        if (length < modulus) {
            out->vx = EGSMathFIXDiv(v->vx, modulus);
            out->vy = EGSMathFIXDiv(v->vy, modulus);
            out->vz = EGSMathFIXDiv(v->vz, modulus);
            out->vx = EGSMathFIXMul(out->vx, length);
            out->vy = EGSMathFIXMul(out->vy, length);
            out->vz = EGSMathFIXMul(out->vz, length);
        } else {
            *out = *v;
        }
    }
    return 0;
}

int EGSMathINT_VectorClipToLength(VECTOR *v, s32 length, VECTOR *out)
{
    s32 modulus;

    if (length == 0) {
        out->vx = out->vy = out->vz = 0;
    } else {
        EGSMath_VectorModulus(v, &modulus);
        length = length >= 0 ? length : -length;
        if (length < modulus) {
            out->vx = EGSMathFIXDiv(v->vx, modulus);
            out->vy = EGSMathFIXDiv(v->vy, modulus);
            out->vz = EGSMathFIXDiv(v->vz, modulus);
            out->vx = EGSMathFIXMul(out->vx, length);
            out->vy = EGSMathFIXMul(out->vy, length);
            out->vz = EGSMathFIXMul(out->vz, length);
        } else {
            *out = *v;
        }
    }
    return 0;
}

int EGSMath_VectorScaleByPowerOf2(VECTOR *in, s32 shift, VECTOR *out)
{
    out->vx = in->vx << shift;
    out->vy = in->vy << shift;
    out->vz = in->vz << shift;
    return 0;
}

int EGSMath_VectorScaleDownByPowerOf2(VECTOR *in, s32 shift, VECTOR *out)
{
    out->vx = in->vx >= 0 ? in->vx >> shift : -(-in->vx >> shift);
    out->vy = in->vy >= 0 ? in->vy >> shift : -(-in->vy >> shift);
    out->vz = in->vz >= 0 ? in->vz >> shift : -(-in->vz >> shift);
    return 0;
}

int EGSMathFIX_VectorScaleForMultiplication(VECTOR *in, VECTOR *out)
{
    s32 ax, ay, az, max;

    ax = ABS(in->vx);
    ay = ABS(in->vy);
    az = ABS(in->vz);
    max = MAX(ax, MAX(ay, az));
    if (max <= 0x1000) {
        out->vx = in->vx << 8;
        out->vy = in->vy << 8;
        out->vz = in->vz << 8;
    } else if (max <= 0x40000) {
        out->vx = in->vx << 2;
        out->vy = in->vy << 2;
        out->vz = in->vz << 2;
    } else if (max <= 0x1000000) {
        out->vx = in->vx >> 4;
        out->vy = in->vy >> 4;
        out->vz = in->vz >> 4;
    } else {
        out->vx = in->vx >> 11;
        out->vy = in->vy >> 11;
        out->vz = in->vz >> 11;
    }
    return 0;
}

int EGSMath_VectorHalf(VECTOR *in, VECTOR *out)
{
    out->vx = in->vx >= 0 ? in->vx >> 1 : -(-in->vx >> 1);
    out->vy = in->vy >= 0 ? in->vy >> 1 : -(-in->vy >> 1);
    out->vz = in->vz >= 0 ? in->vz >> 1 : -(-in->vz >> 1);
    return 0;
}

int EGSMath_VectorNegate(VECTOR *in, VECTOR *out)
{
    out->vx = -in->vx;
    out->vy = -in->vy;
    out->vz = -in->vz;
    return 0;
}

int EGSMath_VectorAdd3(VECTOR *a, VECTOR *b, VECTOR *c, VECTOR *out)
{
    VECTOR sum;

    sum.vx = a->vx + b->vx;
    sum.vy = a->vy + b->vy;
    sum.vz = a->vz + b->vz;
    out->vx = sum.vx + c->vx;
    out->vy = sum.vy + c->vy;
    out->vz = sum.vz + c->vz;
    return 0;
}

int EGSMath_VectorApplyRotationMatrix(VECTOR *in, MATRIX *m, VECTOR *out)
{
    MATRIX *id = pEGSMathIdentityMatrix;

    if (((s32 *)m)[0] == ((s32 *)id)[0] && ((s32 *)m)[1] == ((s32 *)id)[1] &&
        ((s32 *)m)[2] == ((s32 *)id)[2] && ((s32 *)m)[3] == ((s32 *)id)[3] &&
        m->m[2][2] == id->m[2][2]) {
        *out = *in;
    } else {
        ApplyMatrixLV(m, in, out);
    }
    return 0;
}

int EGSMath_VectorApplyReverseRotationMatrix(VECTOR *in, MATRIX *m, VECTOR *out)
{
    MATRIX *id = pEGSMathIdentityMatrix;

    if (((s32 *)m)[0] == ((s32 *)id)[0] && ((s32 *)m)[1] == ((s32 *)id)[1] &&
        ((s32 *)m)[2] == ((s32 *)id)[2] && ((s32 *)m)[3] == ((s32 *)id)[3] &&
        m->m[2][2] == id->m[2][2]) {
        *out = *in;
    } else {
        ApplyTransposeMatrixLV(m, in, out);
    }
    return 0;
}

int EGSMath_VectorApplyMatrix(VECTOR *in, MATRIX *m, VECTOR *out)
{
    ApplyMatrixLV(m, in, out);
    out->vx += m->t[0];
    out->vy += m->t[1];
    out->vz += m->t[2];
    return 0;
}

int EGSMath_VectorApplyReverseMatrix(VECTOR *in, MATRIX *m, VECTOR *out)
{
    MATRIX inv;

    EGSMath_MatrixInvert(m, &inv);
    ApplyMatrixLV(&inv, in, out);
    out->vx += inv.t[0];
    out->vy += inv.t[1];
    out->vz += inv.t[2];
    return 0;
}

int EGSMathINT_VectorGetXZComponent(VECTOR *v, s32 *out)
{
    *out = SquareRoot0(v->vx * v->vx + v->vz * v->vz);
    return 0;
}

int EGSMathFIX_VectorGetXZComponent(VECTOR *v, s32 *out)
{
    s32 sq[2];
    s32 *p = sq;

    p[0] = EGSMathFIXMul(v->vx, v->vx);
    p[1] = EGSMathFIXMul(v->vz, v->vz);
    *out = SquareRoot12(sq[0] + sq[1]);
    return 0;
}

int EGSMathINT_VectorGetXYComponent(VECTOR *v, s32 *out)
{
    *out = SquareRoot0(v->vx * v->vx + v->vy * v->vy);
    return 0;
}

int EGSMathFIX_VectorGetXYComponent(VECTOR *v, s32 *out)
{
    s32 sq[2];
    s32 *p = sq;

    p[0] = EGSMathFIXMul(v->vx, v->vx);
    p[1] = EGSMathFIXMul(v->vy, v->vy);
    *out = SquareRoot12(sq[0] + sq[1]);
    return 0;
}

int EGSMathINT_VectorGetYZComponent(VECTOR *v, s32 *out)
{
    *out = SquareRoot0(v->vy * v->vy + v->vz * v->vz);
    return 0;
}

int EGSMathFIX_VectorGetYZComponent(VECTOR *v, s32 *out)
{
    s32 sq[2];
    s32 *p = sq;

    p[0] = EGSMathFIXMul(v->vy, v->vy);
    p[1] = EGSMathFIXMul(v->vz, v->vz);
    *out = SquareRoot12(sq[0] + sq[1]);
    return 0;
}

int EGSMath_MatrixMultiply(MATRIX *a, MATRIX *b, MATRIX *out)
{
    CompMatrixLV(a, b, out);
    return 0;
}

int EGSMath_RotationMatrixMultiply(MATRIX *a, MATRIX *b, MATRIX *out)
{
    MulMatrix0(a, b, out);
    return 0;
}

int EGSMath_RotationMatrixOrthonormalizeYXZ(MATRIX *in, MATRIX *out)
{
    VECTOR x, y, z;

    x.vx = in->m[0][0];
    x.vy = in->m[1][0];
    x.vz = in->m[2][0];
    y.vx = in->m[0][1];
    y.vy = in->m[1][1];
    y.vz = in->m[2][1];
    z.vx = in->m[0][2];
    z.vy = in->m[1][2];
    z.vz = in->m[2][2];
    GTEMath_VectorNormal(&y, &y);
    OuterProduct12(&x, &y, &z);
    GTEMath_VectorNormal(&z, &z);
    OuterProduct12(&y, &z, &x);
    out->m[0][0] = x.vx;
    out->m[1][0] = x.vy;
    out->m[2][0] = x.vz;
    out->m[0][1] = y.vx;
    out->m[1][1] = y.vy;
    out->m[2][1] = y.vz;
    out->m[0][2] = z.vx;
    out->m[1][2] = z.vy;
    out->m[2][2] = z.vz;
    return 0;
}

int EGSMath_RotationMatrixTranspose(MATRIX *in, MATRIX *out)
{
    TransposeMatrix(in, out);
    out->t[0] = in->t[0];
    out->t[1] = in->t[1];
    out->t[2] = in->t[2];
    return 0;
}

int EGSMath_RotationMatrixInvert(MATRIX *in, MATRIX *out)
{
    TransposeMatrix(in, out);
    out->t[0] = in->t[0];
    out->t[1] = in->t[1];
    out->t[2] = in->t[2];
    return 0;
}

int EGSMath_MatrixInvert(MATRIX *in, MATRIX *out)
{
    VECTOR t;

    TransposeMatrix(in, out);
    out->t[0] = in->t[0];
    out->t[1] = in->t[1];
    out->t[2] = in->t[2];
    t.vx = in->t[0];
    t.vy = in->t[1];
    t.vz = in->t[2];
    t.vx = -t.vx;
    t.vy = -t.vy;
    t.vz = -t.vz;
    ApplyMatrixLV(out, &t, &t);
    out->t[0] = t.vx;
    out->t[1] = t.vy;
    out->t[2] = t.vz;
    return 0;
}

int EGSMath_RotationMatrixCopy(MATRIX *in, MATRIX *out)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out->m[i][j] = in->m[i][j];
        }
    }
    return 0;
}

int EGSMath_MatrixCopy(MATRIX *in, MATRIX *out)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out->m[i][j] = in->m[i][j];
        }
    }
    for (i = 0; i < 3; i++) {
        out->t[i] = in->t[i];
    }
    return 0;
}

/* m: a1 x + b1 y = c1 ; a2 x + b2 y = c2 -> out: {det, detX, det, detY} */
/* m: a1 x + b1 y = c1 ; a2 x + b2 y = c2 -> out: {det, detX, det, detY} */
/* m: a1 x + b1 y = c1 ; a2 x + b2 y = c2 -> out: {det, detX, det, detY} */
/* m: a1 x + b1 y = c1 ; a2 x + b2 y = c2 -> out: {det, detX, det, detY} */
/* m: a1 x + b1 y = c1 ; a2 x + b2 y = c2 -> out: {det, detX, det, detY} */
int EGSMathINT_Solve2LinearEquationsIn2Unknowns(s32 m[2][3], s32 out[4])
{
    s32 a1 = m[0][0];
    s32 b1 = m[0][1];
    s32 c1 = m[0][2];
    s32 a2 = m[1][0];
    s32 b2 = m[1][1];
    s32 c2 = m[1][2];
    s32 b1a2 = b1 * a2;
    s32 b2a1 = b2 * a1;
    s32 c2b1 = c2 * b1;
    s32 c1b2 = c1 * b2;
    s32 c1a2 = c1 * a2;
    s32 c2a1 = c2 * a1;

    out[0] = out[2] = b1a2 - b2a1;
    out[1] = c2b1 - c1b2;
    out[3] = c1a2 - c2a1;
    return out[0] == 0;
}

int EGSMathINT_ValueGetScaleForMultiplication(s32 value, s32 *shift)
{
    value = value >= 0 ? value : -value;
    if (value <= 0x4000) {
        *shift = 0;
    } else if (value <= 0x40000) {
        *shift = 4;
    } else if (value <= 0x400000) {
        *shift = 8;
    } else if (value <= 0x4000000) {
        *shift = 12;
    } else {
        *shift = 17;
    }
    return 0;
}

static inline void EGSMathINT_Swap(s32 *a, s32 *b)
{
    s32 t;

    t = *a;
    *a = *b;
    *b = t;
}

/*
 * Solve the 3x3 system m (row i: m[i][0]*x + m[i][1]*y + m[i][2]*z = m[i][3])
 * by elimination, rescaling the intermediates to avoid overflow. The result
 * is left as fractions in out[0..5]; returns 1 if the system is degenerate.
 */
int EGSMathINT_Solve3LinearEquationsIn3Unknowns(s32 m[3][4], s32 *out)
{
    s32 i0, i1, i2;
    s32 shift;
    s32 e1, e2, e3;
    s32 f1, f2, f3;
    s32 g1, g2;
    s32 z, y, w;

    /* rows: i0 has a nonzero x coefficient, i1 a nonzero y one if possible */
    i0 = 0;
    i1 = 1;
    i2 = 2;
    if (m[0][0] == 0) {
        if (m[1][0] != 0) {
            EGSMathINT_Swap(&i0, &i1);
        } else {
            EGSMathINT_Swap(&i0, &i2);
        }
    }
    if (m[i1][1] == 0) {
        EGSMathINT_Swap(&i1, &i2);
    }
    e1 = m[i0][1] * m[i1][0] - m[i0][0] * m[i1][1];
    e2 = m[i0][2] * m[i1][0] - m[i0][0] * m[i1][2];
    e3 = m[i0][3] * m[i1][0] - m[i0][0] * m[i1][3];
    f1 = m[i0][1] * m[i2][0] - m[i0][0] * m[i2][1];
    f2 = m[i0][2] * m[i2][0] - m[i0][0] * m[i2][2];
    f3 = m[i0][3] * m[i2][0] - m[i0][0] * m[i2][3];
    EGSMathINT_ValueGetScaleForMultiplication(
        MAX(ABS(e1), MAX(ABS(e2), MAX(ABS(e3), MAX(ABS(f1), MAX(ABS(f2), ABS(f3)))))),
        &shift);
    if (shift != 0) {
        e1 >>= shift;
        e2 >>= shift;
        e3 >>= shift;
        f1 >>= shift;
        f2 >>= shift;
        f3 >>= shift;
    }
    g1 = e2 * f1 - e1 * f2;
    g2 = e3 * f1 - e1 * f3;
    EGSMathINT_ValueGetScaleForMultiplication(MAX(ABS(g1), ABS(g2)), &shift);
    if (shift != 0) {
        g1 >>= shift;
        g2 >>= shift;
    }
    z = e1 * g1;
    y = e3 * g1 - e2 * g2;
    w = e1 * g2;
    EGSMathINT_ValueGetScaleForMultiplication(MAX(ABS(z), MAX(ABS(y), ABS(w))), &shift);
    if (shift != 0) {
        z >>= shift;
        y >>= shift;
        w >>= shift;
    }
    out[0] = m[i0][0] * z;
    out[1] = m[i0][3] * z - m[i0][2] * w - m[i0][1] * y;
    out[2] = z;
    out[3] = y;
    out[4] = g1;
    out[5] = g2;
    if (out[0] == 0 || out[2] == 0 || out[4] == 0) {
        return 1;
    }
    return 0;
}

/* v[0] = a * v[1] + b * v[2]: solve using the two best conditioned axes */
int EGSMathINT_SolveLinear2VectorEquationForParameters(VECTOR *v, s32 *out)
{
    s32 m[2][3];
    s32 dx, dy, dz;
    int i;

    dx = v[0].vx - v[1].vx;
    if (dx < 0) {
        dx = v[1].vx - v[0].vx;
    }
    dy = v[0].vy - v[1].vy;
    if (dy < 0) {
        dy = v[1].vy - v[0].vy;
    }
    dz = v[0].vz - v[1].vz;
    if (dz < 0) {
        dz = v[1].vz - v[0].vz;
    }
    if (dx < dy) {
        if (dz < dx) {
            for (i = 0; i < 3; i++) {
                m[0][i] = v[i].vx;
                m[1][i] = v[i].vy;
            }
        } else {
            for (i = 0; i < 3; i++) {
                m[0][i] = v[i].vy;
                m[1][i] = v[i].vz;
            }
        }
    } else if (dz < dy) {
        for (i = 0; i < 3; i++) {
            m[0][i] = v[i].vx;
            m[1][i] = v[i].vy;
        }
    } else {
        for (i = 0; i < 3; i++) {
            m[0][i] = v[i].vx;
            m[1][i] = v[i].vz;
        }
    }
    return EGSMathINT_Solve2LinearEquationsIn2Unknowns(m, out);
}

int EGSMathINT_SolveLinear3VectorEquationForParameters(VECTOR *v, s32 *out)
{
    s32 m[3][4];
    int i;

    for (i = 0; i < 4; i++) {
        m[0][i] = v[i].vx;
        m[1][i] = v[i].vy;
        m[2][i] = v[i].vz;
    }
    return EGSMathINT_Solve3LinearEquationsIn3Unknowns(m, out);
}

#ifdef NON_MATCHING
/* original copies a to a2 and returns straight in v0; regalloc */
s32 EGSMath_VectorIsEqual(VECTOR *a, VECTOR *b) {
    if (a != NULL && b != NULL && a->vx == b->vx && a->vy == b->vy) {
        return a->vz == b->vz;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorIsEqual);
#endif

/*
 * Recomputes frame->matrix from the parent chain and flags the children dirty.
 * Not in siphon.map, but called from egsframe.c (EGSFrameUpdateNode), so it can't
 * be static here (the egsmath/egsframe TU boundary is probably at this function).
 */
int EGSFrameNodeUpdate(EGSFrame *frame)
{
    EGSFrameNode *node;
    EGSFrame *parent;
    EGSFrame *child;
    int ret;

    if (frame == NULL) {
        return EGS_ERR_NULL;
    }
    node = frame->node;
    if (node == NULL) {
        return EGS_ERR_NONODE;
    }
    parent = node->parent;
    if (parent != NULL) {
        ret = EGSFrameNodeUpdate(parent);
        if (ret != 0) {
            return ret;
        }
        if (frame->node->dirty == 1) {
            CompMatrixLV(&parent->matrix, &frame->node->local, &frame->matrix);
        }
    } else if (node->dirty == 1) {
        frame->matrix = node->local;
    }
    frame->node->dirty = 0;
    child = frame->node->child;
    while (child != NULL) {
        if (child->node == NULL) {
            return EGS_ERR_NONODE;
        }
        child->node->dirty = 1;
        child = child->node->next;
    }
    return 0;
}

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSZeroVector);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorX);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorY);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorZ);
