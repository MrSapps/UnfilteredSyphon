#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/stream", InitStreamRequests);

INCLUDE_ASM("asm/main/nonmatchings/stream", StreamRequest);

INCLUDE_ASM("asm/main/nonmatchings/stream", DataStreamCallback);

INCLUDE_ASM("asm/main/nonmatchings/stream", DataStream);

INCLUDE_ASM("asm/main/nonmatchings/stream", StreamOverlay);

INCLUDE_ASM("asm/main/nonmatchings/stream", StreamLock);

INCLUDE_ASM("asm/main/nonmatchings/stream", StreamSynchronous);

INCLUDE_ASM("asm/main/nonmatchings/stream", OpenHogForStreaming);

INCLUDE_ASM("asm/main/nonmatchings/stream", StreamHogBlock);

INCLUDE_ASM("asm/main/nonmatchings/stream", FlushStreamReqList);

INCLUDE_ASM("asm/main/nonmatchings/stream", FlushAllStreamRequests);

INCLUDE_RODATA("asm/main/nonmatchings/stream", D_80012380);

INCLUDE_RODATA("asm/main/nonmatchings/stream", D_80012390);

INCLUDE_ASM("asm/main/nonmatchings/stream", CheckStreamRequests);

INCLUDE_ASM("asm/main/nonmatchings/stream", PauseTerrainStream);

INCLUDE_ASM("asm/main/nonmatchings/stream", UnpauseTerrainStream);

INCLUDE_ASM("asm/main/nonmatchings/stream", FlushStreamRequests);
