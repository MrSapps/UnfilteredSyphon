#ifndef EGSMATH_H
#define EGSMATH_H

#include "common.h"

/* PsyQ libgte layouts */
#ifndef PSYQ_GTE_TYPES
#define PSYQ_GTE_TYPES
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;
#endif

/* libgte / libmath */
int rsin(int a);
int rcos(int a);
int ratan2(int y, int x);
int rand(void);
int SquareRoot0(int a);
int SquareRoot12(int a);
MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
MATRIX *CompMatrixLV(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
VECTOR *ApplyTransposeMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
void OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2);
void GTEMath_VectorNormal(VECTOR *in, VECTOR *out);

/* egs fixed point helpers (20.12) */
int EGSMathFIXMul(int a, int b);
int EGSMathFIXDiv(int a, int b);

#endif /* EGSMATH_H */
