#ifndef LEVEL_H
#define LEVEL_H

#include "common.h"
#include "gameobj.h"

/* Hooks level overlays install from their init function. */
void DamageSetDamagedCallback(void (*cb)(s16 id, s16 by));
void DamageSetDestroyedCallback(void (*cb)(s16 id, s16 by));
void LevelSetEOLCallback(void (*cb)());
void UseSetBombCallback(void (*cb)());
void UseSetLevSpecObjCallback(void (*cb)());
void UseSetSwitchCallback(void (*cb)());
void ObjectSetQuipObjectCallback(void (*cb)());
void ObjectSetSpecialObjectCallback(void (*cb)());
void SetLockedStringCallback(void (*cb)());
void SetUseStringCallback(void (*cb)());

void NPCCopDamagedCallback(s16 id, s16 by);
void ObjectBombDamagedCallback(s16 id, s16 by);
void NPCCopKilledWashington(s16 id, s16 by, s32 refId, s32 arg3, s32 arg4);
void ObjectBombDeath(s32 arg0, s32 arg1, s32 arg2, s32 id);
void ObjectBigExplosionFX(s16 id, void *pos, s32 radius, s32 arg3, s32 arg4);
void DamageRadius(s16 id, void *pos, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6);
void NPCHazmatStart(s16 id);
void SysPlayXA(s32 track, s32 arg1, s32 arg2);
void InterfaceCountdownAbort(void);

typedef struct LevelObjectives {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 count;
    /* 0x0C */ u8 unk0C[4];
    /* 0x10 */ u32 completed; /* bit per objective */
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ s32 unk18;
    /* 0x1C */ u8 unk1C[8];
    /* 0x24 */ u32 unk24; /* bit per objective */
} LevelObjectives;

extern LevelObjectives *gObjectives;
extern u8 gMissionFailed;
void LevelObjectiveComplete(s32 objective);
void LevelObjectiveAdd(s32 objective, s32 arg1);
void WeaponAdd(s32 weapon, s32 count);
void WeaponRemove(s32 weapon);
void TextStatusMessage(void *msg, s32 arg1);
void InterfaceStartCountdown(s32 seconds, s32 arg1, void (*done)());
void SysPlaySong(s32 arg0, s32 song, s32 arg2, s32 arg3);

int ObjectInvisibleMatchToRefID(s16 id, s32 type);
extern u8 gPlayerFlashlight;

void InterfaceViralRadarInit(void);
void SaveObjectivesNTimersReinit(void *info, s32 arg1);
void SysStopSong(s32 arg0, s32 song);
GameObjectRuntime *ObjectMatchToRefID(s16 id, s32 type);
extern u8 gUsingSndBigFire;
extern s32 gLevelInnocentsKilled;
extern s16 gDefaultLevelDmgFX;
extern void (*gSysMovieDoneCallback)(void);

extern s32 gPlayerOffset; /* object id of the player */
extern u8 gDeviceDetonated;

#endif /* LEVEL_H */
