#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSEmergencyBreak);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayAddMovie);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeleteAllSprites);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeleteMovie);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayAddGLine);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeleteGLine);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayAddSprite);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeleteSprite);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayAddPoly);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeletePoly);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly3F);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly3FT);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly3G);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4F);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4FTrans);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4FT);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4G);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4GTrans);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildPoly4GT);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildLineG);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildLineF);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayBuildTile1);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayAddAnim);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDeleteAnim);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayClear);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlushClearRegions);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayClearRegion);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterSectorFaceCollisionArray);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFindSectorWithLight);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRunTextureAnimations);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", a_prim);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", iEGSDisplaySort);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayQueueInterrupt);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayCheckInterruptQueue);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", DCallback);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetupTransformLocalToScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetupTransformWorldToScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetupTransformFrameToScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySortFade);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySortFogScrim);

INCLUDE_RODATA("asm/main/nonmatchings/egsdisp", D_80013734);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySort);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlush);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFadeFull);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFadeOff);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFade);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayDraw);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraGetFrame);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraSetProjection);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayProjectionAngleToDistance);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraSetProjectionAngle);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraView);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayMovieMalloc);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayCameraMalloc);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayMalloc);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPacketAreaMalloc);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPacketAreaFree);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraCreate);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTransformWorldPointToScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetDefaultCamera);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSAnimSetReferenceAnim);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterScrimModel);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFogOn);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetFogColor);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetTerrainFog);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetFogDepth);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetAmbient);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSHAnimationInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", FindFreeHAnimation);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAStartUnique);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetHA);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetHAWithType);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAGetHipMotionArray);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAStart3);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAStart2);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAStart);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAStop);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHMDCache);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterHMD);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", InitGabeDamage);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterHMDDamage);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSFrameLinkParent);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSAnimHMDLink);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", InitPointSources);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", FinalizePointSources);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSFaceListInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSFaceListHead);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSFastInitFaceList);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterFaceCollisionAnim);

void EGSDisplaySetFlashlightFrame(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetFlashlightParameters);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetFlashTime);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashEffect);

void EGSDisplayRedFlashEffect(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayInitTextureAnimation);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayStartTextureAnimation);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayStopTextureAnimation);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayInitBillboardAnimation);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetSectorRotBoxList);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetMovieHeap);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayForceUpdate);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayStopLevel);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayReinitCameras);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EndFlash);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSAnimGunFlash);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSAnimGunFlash2);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetPointSector);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", InitDamagePolys);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterDamage);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterFootprint);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRemoveDamage);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHASetCurrentFrame);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetAlternateViewpoint);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSAnimOnScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetLoad);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPointSourceInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPointSourceFrame);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPointSourceParams);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPointSourceOn);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayPointSourceOff);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashlightInit);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashlightFrame);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashlightParams);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashlightOn);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlashlightOff);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayIsFlashlightOn);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetAmbientLight);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetCollideXZ);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", iEGSDisplayFromLetterbox);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", iEGSDisplayToLetterbox);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFromLetterbox);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayToLetterbox);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTextOverMovies);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetFuzzyFoggedOutDistance);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayFlushCamera);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayRegisterFogScrim);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySetFrameRate);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayNoSort);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayResetOTs);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", BillboardAnimate);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", FuzzyXZDistancePoint2Point);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", SortGs);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTMDSetupGTE);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTMDOnScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySortGMD);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayCalculateHMDTerrainLighting);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetHMDSector);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHMDInActiveSector);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayGetAnimSector);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHAInterpolate);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayHADoneCallback);
