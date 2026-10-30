#ifndef SYSTEM_H
#define SYSTEM_H

#include "game.h"

#define SYS_STATE_STACK_MAX 10
#define SYS_MSG_QUEUE_MAX 100
#define SYS_MSG_PRIORITY_COUNT 5
#define SYS_LEVEL_COUNT 20

/* System states (gSystemState / state stack). */
#define SYS_STATE_GAME 0
#define SYS_STATE_MOVIE 3
#define SYS_STATE_TITLE 4
#define SYS_STATE_MENU 7
#define SYS_STATE_STALL 9
#define SYS_STATE_COUNT 10

/* SysPostMessage priority that dispatches immediately instead of queueing. */
#define SYS_MSG_IMMEDIATE 5

/* Special message endpoints (otherwise a gameObjectsInfo index). */
#define SYS_MSG_GLOBAL 0xFFFF    /* handled through gSysMsgTable */
#define SYS_MSG_LEVEL 0xFFFE     /* handled through the level overlay's table */
#define SYS_MSG_LEVEL_RAW 0xFFFD /* level handler, header only */
#define SYS_MSG_OBJ_666 0x29A    /* dispatched straight to gDispatchFunctions[0x29A] */

typedef struct SysMsg {
    /* 0x00 */ u16 id;
    /* 0x02 */ u16 priority;
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
    /* 0x08 */ void (*onPost)(SysMsg *msg, s32 d0, s32 d1, s32 d2, s32 d3);
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

/* Movie request; SysPlayMovie copies the caller's descriptor into gMovieInfo. */
typedef struct MovieInfo {
    /* 0x00 */ char name[0x80]; /* bare name -> \\MOVIES\\<name>.STR;1, or a full path */
    /* 0x80 */ s16 width;
    /* 0x82 */ s16 height;
    /* 0x84 */ s16 unk84;     /* passed to MovieOpen */
    /* 0x86 */ s16 halfSize;  /* selects the smaller decode buffer */
    /* 0x88 */ u8 trueColor;  /* 24-bit colour mode */
    /* 0x89 */ u8 doneMsgId;  /* message posted when the movie ends (0x21: end of level) */
    /* 0x8A */ u8 unk8A[2];
    /* 0x8C */ s32 doneMsgFrom;
    /* 0x90 */ s32 doneMsgTo;
    /* 0x94 */ s32 bufferSize;
    /* 0x98 */ u8 playing;
    /* 0x99 */ u8 unk99[3];
} MovieInfo; /* size 0x9C */

/* system.c statics, still defined in the asm .bss */
extern ExeTickTimer mExeTickTimers[SYS_EXE_TICK_TIMER_MAX];
extern EGSListNode *exeTickTimerList;
extern MovieInfo gMovieInfo;
extern u8 gUsePCFiles;
extern char D_8010F608[]; /* "" (.sdata) */
extern char tcOverlay[];
extern u8 D_801377E0[]; /* load address of texture-cache/menu/movie overlays */
extern void (*gStallCallback)(void);
extern void (*gSysMovieDoneCallback)(void);
/* TODO: string literals; addressed directly until system.c's data moves to C */
extern char D_8010F634[]; /* "%s.OVL" (.sdata) */
extern char D_8010F63C[]; /* "MENU" (.sdata) */
extern char D_80010078[]; /* "\\BIN\\%s;1", owned by asm SysLoadOverlay */
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
void SysLoadOverlay(char *name, u8 *addr, u8 viaBuffer);
void SysLoadTextureCacheOverlay(char *name, u8 arg1);

/* EGS file API; open/read return 0 on success. */
s32 EGSFileOpen(char *path, s32 *file);
void EGSFileGetBufferSize(s32 file, u32 *size);
s32 EGSFileRead(s32 file, void *buf, u32 size, s32 *nread);
void EGSFileClose(s32 *file);
void *EGSMemMalloc(u32 size);
void EGSMemRegisterVariableOverlayEnd(void *end);
void EGSListAdd(EGSListNode **list, void *data);
int sprintf(char *, const char *, ...);
int DrawSync(int);
int SysIsPaused(void);

extern char *gLevelNames[];
extern u8 D_80140FE0[]; /* load address of level overlays */

#define ABS(x) ((x) < 0 ? -(x) : (x))

void SysSoundSetMasterVolume(s32 channel, s32 vol);
s32 SysSoundGetMasterVolume(s32 channel);
void GameSysSetLetterBoxMode(s32 mode);
void InterfaceForceOff(void);
void EGSDisplayFadeOff(void);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
void func_801445A4(void);
void EGSDisplayFade(s32 dir, s32 speed, void (*done)(void));
void SysStopXA(void);
void SysStreamTextureCacheOverlay(char *name, void (*done)(s32));
void StreamOverlay(char *path, void *dst, s32 size, u32 tag, void (*done)(s32));
void StallFadeCallback(void);
void SysPauseSound(s32);
void StreamSynchronous(void);
void EGSDisplayStopLevel(void);
void EGSMemSetHeap(void *heap);
void EGSDisplayResetOTs(void);
void EGSDisplayMalloc(u32 size);
u32 EGSMemAvail(void);
void func_8001593C(void);
void SysStall(char *overlay, s32 fadeSpeed, void (*callback)(void));
extern void *gLevelRestartHeap;
extern void *gObjPlayer;
void StallOverlayCallback(void);
void EGSDisplayMovieMalloc(void *buf, s32 size);
void SysSetMusicExciteLevel(s32 level);
void EGSDisplayFlush(void);
void EGSDeviceGraphicsSetResolution(s32 w, s32 h);
void EGSDisplayAddMovie(void *camera, s32 arg1);
void PrepareMovieSound(void);
u8 StreamLock(void);
u8 SysSoundIsXAPlaying(void);
void SysStopMovie(void);
void SysPlayMovieCB(void);
void EGSDisplayFadeFull(void);
void EGSDeviceGraphicsSetColorMode(s32 mode);
s32 EGSFileSearchCD(void *pos, char *path);
/* movie.ovl */
void MovieSetStreamBufferNumSectors(s32 sectors);
void MovieSetDoubleDecodeBuffers(s32);
s32 MovieOpen(char *path, s32 arg1, s32 arg2, s32 arg3, void (*done)(void), void *buf, s32 bufSize);
void MovieClose(void);
extern void **gCamera;
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
