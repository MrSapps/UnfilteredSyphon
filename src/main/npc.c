#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInitData);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInit);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPlayRandXA);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInitHostageTaker);

INCLUDE_ASM("asm/main/nonmatchings/npc", npcHostageSetActivation);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickHostageTarget);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCShootHostage);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickNPCTarget);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDamaged);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSound);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSoundFromPC);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSoundGobj);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRegPCSound);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGobjInit);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPutFireOnNPCShow);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPutFireOnNPCHide);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPutFireOnNPCMove);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDeadContactTheLiving);

INCLUDE_ASM("asm/main/nonmatchings/npc", npcRequestKneel);

INCLUDE_ASM("asm/main/nonmatchings/npc", npcRequestKneelRelease);

INCLUDE_ASM("asm/main/nonmatchings/npc", npcGrenadeLanded);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPointIQ);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickNextPoint);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRethinkNextPoint);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRecordPlayerSighting);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetAwarenessLevel);

INCLUDE_ASM("asm/main/nonmatchings/npc", npcSetLevelAlert);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCViewConeCheck);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetShooting);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDebugGetNoDamageFlag);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetMoveMode);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetChaseStopFlag);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDoNoDamage);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPutAllOnRadar);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetLOSFirstSeen);

INCLUDE_ASM("asm/main/nonmatchings/npc", doShootingLogic);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCShoot);

INCLUDE_ASM("asm/main/nonmatchings/npc", getPointActionVector);

INCLUDE_ASM("asm/main/nonmatchings/npc", getPointActionLink);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPathDeltaToAction);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCKneel);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCStand);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSwitch);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCOpenDoor);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCOpenDoorMessage);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCHazmatDone);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetGrenadeLink);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCThrowGrenade);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRollTowardPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRollAwayFromPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCNavigateActionPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCWalkToPoint);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCStrafeTowardPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCStrafeAwayFromPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", doPointAction);

INCLUDE_RODATA("asm/main/nonmatchings/npc", D_80012070);

INCLUDE_ASM("asm/main/nonmatchings/npc", initPointAction);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRandomActions);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCIQ);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCFaceDirection);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCIQEveryFrame);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGenDispatch);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCFindGobjIDForGenerator);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCFindGeneratorPath);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetNumActive);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCreateGenerator);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGenerate);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCheckGenerators);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDeactivateGenerated);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInitTerrainGenerator);

void NPCInitHotBoxGenerator(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDisableGenerator);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCEnableGenerator);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCTryToDeactivate);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRemoveDead);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCActivationOK);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDoAction);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCKill);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDropGun);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDoDamage);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCPickAimer);

void NPCAim(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCAimAll);

INCLUDE_ASM("asm/main/nonmatchings/npc", LOSOverride);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDropPickupStuff);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosUpdateOtherStuff);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCTakeShot);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDoShootingSchedule);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosToPCProcess);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCLosCheck);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCInitSlot);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCUninitSlot);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCListAdd);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCListDelete);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCActivate);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDeactivate);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCRestore);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCAddGobjToRadar);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDeleteGobjFromRadar);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCopDeactivateWashingtonCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", func_80065D90);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCHazmatStart);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCHazmatStop);

INCLUDE_ASM("asm/main/nonmatchings/npc", func_80065F5C);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCopDamagedCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCopKilledWashington);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCDamagedByPC);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCGetCamera);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCCombatModeCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetCopDeactivateCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetInteractEnteredRangeCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetInteractExitedRangeCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", NPCSetInteractOtherCallback);

INCLUDE_ASM("asm/main/nonmatchings/npc", func_800664F8);

INCLUDE_ASM("asm/main/nonmatchings/npc", func_80066714);

INCLUDE_ASM("asm/main/nonmatchings/npc", func_800667C4);
