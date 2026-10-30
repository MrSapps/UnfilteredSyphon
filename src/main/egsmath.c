#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueModulus);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_ValueSquareRoot);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_ValueSquareRoot);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueArctan);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueSin);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueCos);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueTan);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_ValueRandom);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorINTtoFIX);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorFIXtoINT);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorModulus);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorModulus);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorModulus);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorScaleToLength);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorScaleToLength);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorClipToLength);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorClipToLength);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorScaleByPowerOf2);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorScaleDownByPowerOf2);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorScaleForMultiplication);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorHalf);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorNegate);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorAdd3);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorApplyRotationMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorApplyReverseRotationMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorApplyMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorApplyReverseMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorGetXZComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorGetXZComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorGetXYComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorGetXYComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_VectorGetYZComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathFIX_VectorGetYZComponent);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_MatrixMultiply);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_RotationMatrixMultiply);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_RotationMatrixOrthonormalizeYXZ);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_RotationMatrixTranspose);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_RotationMatrixInvert);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_MatrixInvert);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_RotationMatrixCopy);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_MatrixCopy);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_Solve2LinearEquationsIn2Unknowns);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_ValueGetScaleForMultiplication);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_Solve3LinearEquationsIn3Unknowns);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_SolveLinear2VectorEquationForParameters);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMathINT_SolveLinear3VectorEquationForParameters);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", EGSMath_VectorIsEqual);

INCLUDE_ASM("asm/main/nonmatchings/egsmath", func_800D503C);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSZeroVector);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorX);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorY);

INCLUDE_RODATA("asm/main/nonmatchings/egsmath", gEGSUnitVectorZ);
