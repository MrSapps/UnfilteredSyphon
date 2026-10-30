#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/terrain", VramSwap);

INCLUDE_ASM("asm/main/nonmatchings/terrain", AckVram);

INCLUDE_RODATA("asm/main/nonmatchings/terrain", D_8001233C);

INCLUDE_ASM("asm/main/nonmatchings/terrain", InitStreaming);

INCLUDE_RODATA("asm/main/nonmatchings/terrain", D_80012360);

INCLUDE_ASM("asm/main/nonmatchings/terrain", SetupTerrain);

INCLUDE_ASM("asm/main/nonmatchings/terrain", GameSysFuzzyDistPt2Pt);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FindBadCacheEntries);

INCLUDE_ASM("asm/main/nonmatchings/terrain", AllocateFromTerrainCache);

INCLUDE_ASM("asm/main/nonmatchings/terrain", StreamVramPage);

INCLUDE_ASM("asm/main/nonmatchings/terrain", SectorActivated);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FixupTerrain);

INCLUDE_ASM("asm/main/nonmatchings/terrain", ClearTerrainCachePending);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainPartCallback);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FlushedStreamCallback);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainStreamCallback);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FindSectorInNewMap);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainUpdateActivation);

INCLUDE_ASM("asm/main/nonmatchings/terrain", SHReq);

INCLUDE_ASM("asm/main/nonmatchings/terrain", GSActivateGameObject);

INCLUDE_ASM("asm/main/nonmatchings/terrain", GSDeactivateGameObject);

INCLUDE_ASM("asm/main/nonmatchings/terrain", DeactivateCurrentTerrain);

INCLUDE_ASM("asm/main/nonmatchings/terrain", ActivateSector);

INCLUDE_ASM("asm/main/nonmatchings/terrain", DeactivateSector);

INCLUDE_ASM("asm/main/nonmatchings/terrain", GSAddAnim);

INCLUDE_ASM("asm/main/nonmatchings/terrain", GSDeleteAnim);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FlushSectorFromCache);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainReplaceSector);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainGetSectorIdByName);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainGetOriginalId);

INCLUDE_ASM("asm/main/nonmatchings/terrain", func_8008198C);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainZoom);

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainZoomAndOrient);

INCLUDE_ASM("asm/main/nonmatchings/terrain", FixupEMDGameObject);
