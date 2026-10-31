/* Absolute addresses for rodata now emitted by compiled C (as local labels)
 * but still referenced by functions that are asm. Remove entries once no asm
 * references them. */
D_80010038 = 0x80010038; /* "s/system.c" */
D_80010044 = 0x80010044; /* "MOVIE.OVL" */
D_80010050 = 0x80010050; /* "INIT.OVL" */
mExeTickTimers = 0x801104A0; /* system.c static */
D_800FCB14 = 0x800FCB14; /* &gSysMsgTable[0].handle (system.c static), asm SysExecute */
D_8010FF68 = 0x8010FF68; /* start of .sbss (system.c static), lib_sn __SN_ENTRY_POINT */
mSysMsgQueue = 0x80110560; /* system.c static (D_80110560.queue) */
mSysMsgQueueNext = 0x80111054; /* system.c static (D_80110560.next) */
D_80111B48 = 0x80111B48; /* system.c static (D_80110560.textSprites) */
D_800FE5E8 = 0x800FE5E8; /* gPhysicsGravityAcceleration.vy (physics.c); movement.c reads it as a scalar */
D_800FE5EC = 0x800FE5EC; /* gPhysicsGravityAcceleration.vz */
D_80105E10 = 0x80105E10; /* &gMultiTextures[0].textures (sprite.c), particle.c */
D_80105E1C = 0x80105E1C; /* &gMultiTextures[1].textures */
D_80105E2C = 0x80105E2C; /* &gMultiTextures[3] */
D_801104A0 = 0x801104A0; /* start of .bss (system.c mExeTickTimers), the __bss word */
/* movie.ovl entry points called from the main exe */
MovieSetDoubleDecodeBuffers = 0x801383F8;
MovieSetStreamBufferNumSectors = 0x8013841C;
MovieOpen = 0x8013842C;
MovieClose = 0x80138A94;
gMovieOpenFlag = 0x8013BBC0; /* movie.ovl */
