#include "common.h"

void ObjectTextureAnimationsHack(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectInitData);

INCLUDE_ASM("asm/main/nonmatchings/object", iObjectDoMovieEx);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectDoMovieEx);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetMovieDoneCallback);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectRemoveAndIgnoreBulletHoles);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectReplaceSector);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectInvisibleMatchToRefID);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectMatchToRefID);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectMatchToRefIDByCatID);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectTeleportPlayer);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectRefMultiSYSPOSTMSG0);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectBigExplosionFX);

INCLUDE_ASM("asm/main/nonmatchings/object", iElectronicStopShockSnd);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectElectronicDestruction);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002BE44);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002BECC);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetLights);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetLightState);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetSectorLighting);

INCLUDE_ASM("asm/main/nonmatchings/object", iObjectFindNearbyLight);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectDoQuip);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetQuipObjectCallback);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetSpecialObjectCallback);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetSpecialAmbientCallback);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectUpdateAmbientObjectSetting);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectUpdateAmbientSndObjectSetting);

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetAmbientSndIdxCallback);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CAB0);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CD34);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CE20);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CF58);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CFC4);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002D190);

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010F60);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002D328);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002D790);

void ANIMATEDObjDispatch(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/object", EFFECTSObjDispatch);

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010FD0);

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010FDC);

INCLUDE_ASM("asm/main/nonmatchings/object", EFFECTS2ObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", FIREObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", GENObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", GLASSObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", LASERObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", LIGHTObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", ROTATINGObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", SPECIALObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", TGTObjDispatch);
