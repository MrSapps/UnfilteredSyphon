#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"
#include "gameobj.h"

/* Hooks level overlays install from their init function. */
void DamageSetDamagedCallback(void (*cb)(s16 id, s16 by));
void DamageSetDestroyedCallback(void (*cb)(s16 id, s16 by));
s32 (*LevelSetEOLCallback(s32 (*cb)()))(); /* cb returns nonzero to allow the end of level */
void *UseSetBombCallback(void (*cb)()); /* returns the previous callback */
void *UseSetLevSpecObjCallback(void (*cb)()); /* returns the previous callback */
void *UseSetSwitchCallback(void (*cb)()); /* returns the previous callback */
void *UseSetComputerCallback(void (*cb)()); /* returns the previous callback */
void ObjectSetQuipObjectCallback(void (*cb)());
void ObjectSetSpecialObjectCallback(void (*cb)());
void *SetLockedStringCallback(void (*cb)()); /* returns the previous callback */
void *SetUseStringCallback(void (*cb)()); /* returns the previous callback */

void NPCCopDamagedCallback(s16 id, s16 by);
void ObjectBombDamagedCallback(s16 id, s16 by);
void NPCCopKilledWashington(s16 id, s16 by, s32 refId, s32 arg3, s32 arg4);
void ObjectBombDeath(s32 reason, s32 xa, s32 secondary, s32 id);
void ObjectBigExplosionFX(s16 id, void *pos, s32 radius, s32 arg3, s32 arg4);
void DamageRadius(s16 id, void *pos, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6);
void NPCHazmatStart(s32 id);
void SysPlayXA(s32 track, s32 arg1, s32 arg2);
void InterfaceCountdownAbort(void);

/* Two lists: the primary objectives (flags 0x10/0x14/0x18) and a second list (flags 0x20/0x24) */
typedef struct LevelObjectives {
    /* 0x00 */ s32 primaryCount;
    /* 0x04 */ char **primaryTexts;
    /* 0x08 */ s32 count;
    /* 0x0C */ char **texts;
    /* 0x10 */ u32 completed; /* bit per objective */
    /* 0x14 */ u32 unk14;     /* bit per objective */
    /* 0x18 */ u32 unk18;     /* bit per objective */
    /* 0x1C */ u32 unk1C;     /* bit per objective */
    /* 0x20 */ u32 unk20;     /* bit per objective */
    /* 0x24 */ u32 unk24;     /* bit per objective */
} LevelObjectives;

extern LevelObjectives *gObjectives;
extern u8 gMissionFailed;
void LevelObjectiveComplete(s16 objective);
void LevelObjectiveAdd(s32 objective, s32 arg1);
void WeaponAdd(s32 weapon, s32 count);
void WeaponRemove(s32 weapon);
u16 TextStatusMessage(void *msg, s32 ticks); /* returns the text string id */
void InterfaceStartCountdown(s32 seconds, s32 arg1, void (*done)());
void SysPlaySong(s32 arg0, s32 song, s32 arg2, s32 arg3);

int ObjectInvisibleMatchToRefID(s16 id, s32 type);
extern s32 gPlayerFlashlight;

void InterfaceViralRadarInit(void);
void SaveObjectivesNTimersReinit(void *info, s32 arg1);
void SysStopSong(s32 arg0, s32 song);
GameObjectRuntime *ObjectMatchToRefID(s16 id, s32 type);
extern u8 gUsingSndBigFire;
extern s32 gLevelInnocentsKilled;
extern s16 gDefaultLevelDmgFX;
extern void (*gSysMovieDoneCallback)(void);

/* NPC state hanging off GameObjectRuntime.npc */
typedef struct NPCInfo {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 flags;
    /* 0x24 */ u8 unk24[0x18];
    /* 0x3C */ s32 casAction; /* CAS action in progress, 0 when done */
    /* 0x40 */ u8 unk40[7];
    /* 0x47 */ u8 nameMsgId;
    /* 0x48 */ u8 unk48;
} NPCInfo;

extern s16 gLevelSpecificNpcID;
extern s16 gLevelSpecificNpcID2;
extern void *gLevelSpecificNpcName;
extern void *gLevelSpecificNpc2Name;
extern u8 gLevelAlerted;
extern void *gLevelAlertedMsg;
extern void *gPhaganMsg;
extern void *gMeiXingMsg;
extern void *gObjectivesIncompleteMsg;
extern void *gRemainingMsg;
extern void *gCheckpointMsg;
extern void *gElectronicallyLockedMsg;
extern void *gLockCodeRequiredMsg;
extern s32 gLevelKeycards;
extern s32 gLevelKeycardsTaken;
extern u32 gWeaponsAvailable[];
void npcSetLevelAlert(void);
void WeaponSetWeaponFiredCallback(void (*cb)());
void LevelMissionFailed(s16 reason, s32 xa, u8 secondary);
void StringMissionFailedMsg();
void LevelObjectivesIncomplete(void *msg);
void DamageDestroyImmediately(s32 by, s32 id);
void ObjectDoMovieEx(char *name, s32 arg1, s16 arg2, s16 arg3);
s32 LevelMissionCtrMsg(char *msg, s32 extraTicks);
void ObjectTeleportPlayer(s16 id, s32 arg1);
void GameCameraRequestCutToBehind(void);
void InterfaceSetAddLevelSpecificNPCCallback(void (*cb)());
void *NPCSetCopDeactivateCallback(void (*cb)()); /* returns the previous callback */
void ObjectSetMovieDoneCallback(void (*cb)());
void CASRequestAction(s32 id, s32 action);
void StringObjectiveObjectsRemaining2(void *buf, void *fmt, void *remaining, void *arg3, s32 done, s32 total);
void StringInteractObjectsRemaining(void *fmt, s32 left);
void ObjectRefMultiSYSPOSTMSG0(s16 id, s32 msg, s32 arg2);
void SaveCreateContinue(void);
void *PickupSetKeyCallback(void (*cb)()); /* returns the previous callback */

extern s32 gPlayerOffset; /* object id of the player */
extern u8 gDeviceDetonated;

#endif /* LEVEL_H */
