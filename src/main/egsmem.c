#include "common.h"
#include "game.h"
#include "system.h"

SDATA_LOCAL(gEndOfMemory)
SDATA_LOCAL(gStaticChunk)
SDATA_LOCAL(gStaticChunkCurrent)
SDATA_LOCAL(gStaticChunkEnd)

extern u8 *gEndOfMemory;
extern u8 *gStaticChunk;        /* lowest usable address of the static heap */
extern u8 *gStaticChunkCurrent; /* heap grows down from the top */
extern u8 *gStaticChunkEnd;

#ifdef NON_MATCHING
/* GCC folds base + (top - base) back to top; the original keeps the
 * subu/addu pair. */
void EGSMemInit(u32 top) {
    u32 size;

    gEndOfMemory = (u8 *)top;
    size = top - (u32)D_80145BF0;
    gStaticChunk = D_80145BF0;
    gStaticChunkCurrent = D_80145BF0 + size;
    gStaticChunkEnd = D_80145BF0 + size;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsmem", EGSMemInit);
#endif

void EGSMemRegisterVariableOverlayEnd(void *end) {
#line 71
    ASSERT(((u32)end & 3) == 0);
    gStaticChunk = end;
}

u32 EGSMemAvail(void) {
    return gStaticChunkCurrent - gStaticChunk;
}

void EGSMemDumpHeap(void) {
}

void *EGSMemMalloc(u32 size) {
    u8 *cur = gStaticChunkCurrent;
    u8 *ptr;

    if (size == 0) {
        return NULL;
    }
#line 93
    ASSERT(cur != NULL);
    ptr = cur - size;
    ptr -= (u32)ptr & 3;
#line 100
    ASSERT(ptr >= gStaticChunk);
    gStaticChunkCurrent = ptr;
    return ptr;
}

void EGSMemFree(void *ptr) {
#line 108
    ASSERT(gStaticChunkCurrent != NULL);
    gStaticChunkCurrent = ptr;
}

void EGSMemSetHeap(void *heap) {
    gStaticChunkCurrent = heap;
}

void *EGSMemGetHeap(void) {
    return gStaticChunkCurrent;
}
