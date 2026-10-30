#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetupSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterVAB);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterVABContinue);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterVABPartly);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterVABComplete);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterVABCompleteCallback);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReleaseXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetXACallbackFunction);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundRegisterMIDI);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ClearVABsResolvedFlag);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetupAllLoaded);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReleaseAllSoundBlocks);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReleaseOneSoundBlock);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReleaseMIDI);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ShutdownSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlaySoundVolPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlaySoundVolPanByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlaySoundVolPanByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayXADirect);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayXADirectwCB);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayXADirectByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayXADirectByIDwCB);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayMusicUnderscoreVolPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayMusicUnderscoreVolPanByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PauseMusicUnderscore);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopMusicUnderscore);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", AdjustRunningMusicVol);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetMusicUnderscoreVol);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundVol);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundVolByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundVolByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundVol);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundVolByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundVolByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PauseAllLoops);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundPanByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundPanByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundNote);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundNoteByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundNoteByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundNote);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundNoteByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetSoundNoteByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopSoundByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopSoundByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopAllSoundsInBlock);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopAllSoundsInBlockByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopAllSoundsInBlockByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReverbOn);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReverbOff);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ReverbSet);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PrepareMovieSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FadeDownMovieSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindSoundBlockByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindSoundBlockByNum);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", NextFreeVAB);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindMIDIDesc);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", NextFreeMIDI);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindMIDIByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", NextFreeXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindXAByID);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetLoopingVolume);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetVoiceVolume);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetVoiceFromField);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetVolumesFromPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetPanChange);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", AddPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetMasterVolume);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetMasterVolume);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetStereoMode);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetStereoMode);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", DoSoundTick);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", NewSoundEvent);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", FindSoundEvent);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetUnusedEvent);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", RemoveSoundEvent);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", KillAllEventsForSound);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundSeqCallback);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetMIDIRegister);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetMIDIRegister);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetGlobalExcite);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetGlobalExcite);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ClearMIDIMuteStatus);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetMIDIVolPan);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ResetMIDICallback);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetGroupMember);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetGroupMember);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetNumGroupMembers);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PrepareXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", UnprepareXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PauseXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", ContinueXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", StopXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", PlayingXA);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetXAStatus);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetXAPos);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", CDPosToLong);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", CDLongToPos);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", func_800C1380);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", func_800C13B4);

INCLUDE_ASM("asm/main/nonmatchings/snddrv", func_800C1590);
