#ifndef SYSTEM_H
#define SYSTEM_H

#include "game.h"

#define SYS_STATE_STACK_MAX 10

extern int gSystemState;
extern int mSystemStateDepth;
extern int mSystemStateStack[];
extern u8 gSysHaltState;

void SysStatePush(int state);
void SysStatePop(void);
void SysStopSound(void);
void SysStopAllSongs(void);
void UnpauseTerrainStream(void);
void EGSDeviceControllerVibrate(int, int, int);
void func_80015A64(int state, int push);

#endif /* SYSTEM_H */
