/* Absolute addresses for rodata now emitted by compiled C (as local labels)
 * but still referenced by functions that are asm. Remove entries once no asm
 * references them. */
D_80010038 = 0x80010038; /* "s/system.c" */
D_80010044 = 0x80010044; /* "MOVIE.OVL" */
D_80010050 = 0x80010050; /* "INIT.OVL" */
mExeTickTimers = 0x801104A0; /* system.c static */
/* movie.ovl entry points called from the main exe */
MovieSetDoubleDecodeBuffers = 0x801383F8;
MovieSetStreamBufferNumSectors = 0x8013841C;
MovieOpen = 0x8013842C;
MovieClose = 0x80138A94;
