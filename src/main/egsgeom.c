#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_GetDistancePointToPoint);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_GetXZDistancePoint2Point);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetFuzzyXZDistancePoint2Point);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetFuzzyDistancePoint2Point);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_GetDistancePoint2Line);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetDistancePoint2Plane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetDistancePointToPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetOrientationPoint2Plane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorReflectAboutUnitNormal);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryFIX_VectorReflect);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorRotateYXZ);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorReverseRotateYXZ);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_VectorGetAngleFromXZPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryFIX_VectorGetAngleFromXZPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_VectorGetAngleFromYZPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryFIX_VectorGetAngleFromYZPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetXZAngleFromZAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetXZAngleFromXAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetXYAngleFromXAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetXYAngleFromYAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetYZAngleFromYAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorGetYZAngleFromZAxis);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_RotationMatrixToAngleYXZ);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_VectorToAngleYX);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_AngleToVectorYX);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_UnitVectorToRotationMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorToRotationMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_RayToMatrix);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ScalarInterpolate);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorInterpolate);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_AngleInterpolate);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_PointProjectOnPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorProjectOnUnitVector);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorProjectOnPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryFIX_VectorProjectOnPlaneAlongY);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_VectorAlignAlongPlane);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryFIX_PlaneGetUnitUphillGradient);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_PlaneGetY);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_FaceGetUnitNormal);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_FaceGetNonUnitNormal);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_PointProjectOnFacePlaneAlongY);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometryINT_PointProjectOnPlaneAlongY);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_TransformPoint);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ReverseTransformPoint);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_TransformVector);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ReverseTransformVector);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_TransformSegment);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ReverseTransformSegment);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_TransformLine);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ReverseTransformLine);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_TransformRay);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_ReverseTransformRay);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectUnrotBox2UnrotBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectXZUnrotBox2UnrotBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectPoint2UnrotBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectXZPoint2UnrotBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetBoundingBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetBoxPoints);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectBoundingBox2BoundingBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectXZBoundingBox2BoundingBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectPoint2BoundingBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectXZPoint2BoundingBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_GetIntersectRaySegment2UnrotBox);

INCLUDE_ASM("asm/main/nonmatchings/egsgeom", EGSGeometry_IsIntersectLineSegment2UnrotBox);
