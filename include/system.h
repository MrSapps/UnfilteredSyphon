#ifndef SYSTEM_H
#define SYSTEM_H

#include "game.h"

#define SYS_STATE_STACK_MAX 10
#define SYS_MSG_QUEUE_MAX 100
#define SYS_MSG_PRIORITY_COUNT 5
#define SYS_LEVEL_COUNT 20

/* SysPostMessage priority that dispatches immediately instead of queueing. */
#define SYS_MSG_IMMEDIATE 5

/* Special message endpoints (otherwise a gameObjectsInfo index). */
#define SYS_MSG_GLOBAL 0xFFFF    /* handled through gSysMsgTable */
#define SYS_MSG_LEVEL 0xFFFE     /* handled through the level overlay's table */
#define SYS_MSG_LEVEL_RAW 0xFFFD /* level handler, header only */
#define SYS_MSG_OBJ_666 0x29A    /* dispatched straight to gDispatchFunctions[0x29A] */

typedef struct SysMsg {
    /* 0x00 */ s16 id;
    /* 0x02 */ s16 priority;
    /* 0x04 */ s32 from;
    /* 0x08 */ s32 to;
    /* 0x0C */ s32 data[4];
} SysMsg; /* size 0x1C */

typedef struct SysMsgQueue {
    /* 0x000 */ s32 count;
    /* 0x004 */ SysMsg msgs[SYS_MSG_QUEUE_MAX];
} SysMsgQueue; /* size 0xAF4 */

typedef void (*SysMsgHandler)(SysMsg *msg);

/* One entry per message id, in gSysMsgTable and the level overlay's table. */
typedef struct SysMsgDesc {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ SysMsgHandler handle;
    /* 0x08 */ void (*onPost)(SysMsg *msg, s32 d0, s32 d1, s32 d2);
} SysMsgDesc; /* size 0xC */

extern SysMsgDesc gSysMsgTable[];

typedef struct SysInfo {
    /* 0x00 */ s16 level;
    /* 0x02 */ s16 unk02;
    /* 0x04 */ SysMsgDesc *levelMsgTable; /* message table of the loaded level overlay */
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
} SysInfo; /* size 0x10 */

extern SysInfo gSysInfo;
extern SysMsgHandler gDispatchFunctions[];

#define SYS_EXE_TICK_TIMER_MAX 16

typedef struct ExeTickTimer {
    /* 0x00 */ void (*callback)(s32 arg);
    /* 0x04 */ s32 ticks;
    /* 0x08 */ s32 arg;
} ExeTickTimer; /* size 0xC */

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

/* Movie playback state (system.c static). */
typedef struct SysMovie {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 doneMsgId; /* message posted when the movie ends (0x21: end of level) */
    /* 0x02 */ u8 unk02[2];
    /* 0x04 */ s32 doneMsgFrom;
    /* 0x08 */ s32 doneMsgTo;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ u8 playing;
} SysMovie;

/* system.c statics, still defined in the asm .bss */
extern ExeTickTimer mExeTickTimers[SYS_EXE_TICK_TIMER_MAX];
extern EGSListNode *exeTickTimerList;
extern SysMovie mSysMovie;
extern char tcOverlay[];
extern u8 D_801377E0[]; /* load address of texture-cache/menu/movie overlays */
extern u8 gStallLoaded;
extern u8 gStallFaded;
extern u8 gPlayerRadioFlag;
extern SysMsgQueue mSysMsgQueue;
extern SysMsgQueue mSysMsgQueueNext;

extern int gSystemState;
extern int mSystemStateDepth;
extern int mSystemStateStack[];
extern u8 gSysHaltState;
extern s32 gSystemTick;
/* In .sdata, but referenced with absolute addressing, so not declared as a scalar here. */
extern u8 gContinueLevel[];
extern u8 gLevelReady;
extern u8 gInEOLState;
extern u8 D_8010F622;
extern u8 D_80145BF0[]; /* load address of INIT.OVL */

void SysHalt(u8 mode);
void SysStatePush(int state);
void SysStatePop(void);
void SysStopSound(void);
void SysStopAllSongs(void);
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);
void SysLoadOverlay(char *name, void *addr, s32 arg2);
void SysLoadTextureCacheOverlay(char *name, s32 arg1);
void func_80015A64(int state, int push);

void SysSoundSetMasterVolume(s32 channel, s32 vol);
s32 SysSoundGetMasterVolume(s32 channel);
void GameSysSetLetterBoxMode(s32 mode);
void InterfaceForceOff(void);
void EGSDisplayFadeOff(void);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
void func_801445A4(void);
void func_801470B0(void);

void UnpauseTerrainStream(void);
void PauseTerrainStream(void);
void TerrainUpdateActivation(s32);
void GameSysTableHook(void);
void SYSCasInit(void);
void SYSCameraInit(void);
void SYSMovementInit(void);
void SYSGameInit(void);
void ShadowInit(void);

void EGSDebugInit(s32);
void EGSMemInit(u32 top);
s32 EGSDeviceGraphicsInit(s32 w, s32 h, s32);
void EGSDeviceCounterInitSystemClock(void);
void EGSDeviceCdInit(void);
void EGSDeviceControllerInit(void);
void EGSDeviceControllerVibrate(int, int, int);
void EGSDeviceRender(void);
void EGSDisplayInit();
void EGSDisplayReinitCameras(void);
void EGSMiscMemcpy(void *dst, void *src, s32 words);
void BreakDraw(void);

#endif /* SYSTEM_H */
