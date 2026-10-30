#include "common.h"

/*
 * snddrv.c: the game's sound driver, on top of libsnd/libspu/libcd.
 * Sound banks are VABs with a custom header and a table of SoundEntry records;
 * MIDI underscores are SEQ/SEP data; XA streams are played straight off the CD.
 */

/* The original was built with gcc's builtin abs() (inline bgez/negu); this build passes -fno-builtin. */
#define abs(x) __builtin_abs(x)

/* snddrv.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FD78)
SDATA_LOCAL(D_8010FD7C)
SDATA_LOCAL(D_8010FD7E)
SDATA_LOCAL(D_8010FD80)
SDATA_LOCAL(D_8010FD82)
SDATA_LOCAL(D_8010FD84)
SDATA_LOCAL(D_8010FD88)
SDATA_LOCAL(D_8010FD8C)
SDATA_LOCAL(D_8010FD90)
SDATA_LOCAL(D_8010FD94)
SDATA_LOCAL(D_8010FD98)
SDATA_LOCAL(D_801101E8)
SDATA_LOCAL(D_801101EC)
SDATA_LOCAL(D_801101F0)
SDATA_LOCAL(D_801101F4)
SDATA_LOCAL(D_801101F8)
SDATA_LOCAL(gAllowCueAfter)
SDATA_LOCAL(gCurrentMovieVol)
SDATA_LOCAL(gHasCD)
SDATA_LOCAL(gLastXAPan)
SDATA_LOCAL(gLastXAVol)
SDATA_LOCAL(gLoopsPaused)
SDATA_LOCAL(gMusicNumPaused)
SDATA_LOCAL(gMusicPaused)
SDATA_LOCAL(gPlayingMIDIUnderscore)
SDATA_LOCAL(gPlayingMIDIUnderscoreBlock)
SDATA_LOCAL(gRevDepth)
SDATA_LOCAL(gRevType)
SDATA_LOCAL(gReverbOn)
SDATA_LOCAL(gReverbWaiting)
SDATA_LOCAL(gSEvent)
SDATA_LOCAL(gStereoMode)
SDATA_LOCAL(gUnderscoreID)
SDATA_LOCAL(gUserXACallback)
SDATA_LOCAL(gUserXACallbackData)
SDATA_LOCAL(gUserXACallbackFrame)
SDATA_LOCAL(gVABDoneCallback)
SDATA_LOCAL(gVABDoneCallbackData_1)
SDATA_LOCAL(gVABDoneCallbackData_2)
SDATA_LOCAL(gXferingVAB)
SDATA_LOCAL(gXferingVABPtr)
SDATA_LOCAL(oldreadycallback)

/* One sound in a bank (0x18 bytes) */
typedef struct SoundEntry {
    /* 0x00 */ u16 flags; /* low 5 bits: SND_TYPE_* */
    /* 0x02 */ u8 vab;
    /* 0x03 */ u8 seq;    /* SEQ/SEP access number (SND_TYPE_SEQ) */
    /* 0x04 */ s8 sepNum; /* -1: a SEQ */
    /* 0x05 */ u8 prog;   /* group: member count */
    /* 0x06 */ u8 firstTone;
    /* 0x07 */ u8 tone;
    /* 0x08 */ u8 note;
    /* 0x09 */ u8 vol;
    /* 0x0A */ s16 pan;
    /* 0x0C */ u8 defVol;
    /* 0x0D */ u8 unk0D;
    /* 0x0E */ s16 defPan;
    /* 0x10 */ s8 numVoices; /* -1: not playing */
    /* 0x11 */ u8 playTone; /* tone keyed on */
    /* 0x12 */ u8 unk12;
    /* 0x13 */ u8 unk13;
    /* 0x14 */ s32 voices; /* bit field of SPU voices */
} SoundEntry;

#define SND_TYPE_ONESHOT 0
#define SND_TYPE_LOOP 1
#define SND_TYPE_SEQ 2
#define SND_TYPE_GROUP 0xB
#define SND_TYPE_VOICED 0xE

/* A registered sound bank (the head of the loaded .VH) */
typedef struct SoundBlock {
    /* 0x00 */ s32 id;
    /* 0x04 */ SoundEntry *sounds; /* an offset from the block until registered */
    /* 0x08 */ s32 vabId;
    /* 0x0C */ u32 numSounds; /* low byte: count; bit 31: resolved */
} SoundBlock;

/* A registered XA stream */
typedef struct XABlock {
    /* 0x00 */ s32 id;
    /* 0x04 */ char name[0x80];
    /* 0x84 */ s32 pos; /* CD sector of the file */
    /* 0x88 */ s32 offsets[1]; /* sector offsets of the later sub-files */
} XABlock;

/* A registered MIDI underscore */
typedef struct MIDIDesc {
    /* 0x00 */ s32 id;
    /* 0x04 */ s16 type; /* 0: SEQ, else SEP */
    /* 0x06 */ s16 numSeqs;
    /* 0x08 */ s32 blockId; /* the sound block holding its VAB */
    /* 0x0C */ s16 seq;     /* libsnd access number */
    /* 0x0E */ s16 unk0E;
    /* 0x10 */ u8 data[1];
} MIDIDesc;

/* A pending sound event (0x1C bytes) */
typedef struct SoundEvent {
    /* 0x00 */ s16 kind;
    /* 0x02 */ s16 id; /* -1: unused */
    /* 0x04 */ struct SoundBlock *block;
    /* 0x08 */ u16 unk08;
    /* 0x0A */ s16 step;
    /* 0x0C */ u16 unk0C;
    /* 0x0E */ s16 unk0E;
    /* 0x10 */ s16 value;
    /* 0x12 */ s16 unk12; /* pan fade: -(change) */
    /* 0x14 */ s16 unk14; /* stop the underscore when the fade ends */
    /* 0x16 */ s16 unk16;
    /* 0x18 */ struct SoundEvent *next;
} SoundEvent;

/* A sound group (0x34 bytes) */
typedef struct SoundGroup {
    /* 0x00 */ u8 num;
    /* 0x01 */ u8 unk01;
    /* 0x02 */ u8 a[16];
    /* 0x12 */ u8 b[16];
    /* 0x22 */ u8 c[16];
    /* 0x32 */ u8 unk32[2];
} SoundGroup;

/* An XA play request (0xC bytes) */
typedef struct XAQueue {
    /* 0x00 */ s32 pos;
    /* 0x04 */ s32 chan;
    /* 0x08 */ s32 arg;
} XAQueue;

/* libsnd VAB header; a SoundBlock overlays its first 0x10 bytes */
typedef struct VabHdr {
    /* 0x00 */ s32 form;
    /* 0x04 */ u32 ver;
    /* 0x08 */ u32 id;
    /* 0x0C */ u32 fsize;
    /* 0x10 */ u16 reserved0;
    /* 0x12 */ u16 ps;
    /* 0x14 */ u16 ts;
    /* 0x16 */ u16 vs;
    /* 0x18 */ u8 mvol;
    /* 0x19 */ u8 pan;
    /* 0x1A */ u8 attr1;
    /* 0x1B */ u8 attr2;
    /* 0x1C */ u32 reserved1;
} VabHdr;

typedef struct ProgAtr {
    /* 0x00 */ u8 tones;
    /* 0x01 */ u8 mvol;
    /* 0x02 */ u8 prior;
    /* 0x03 */ u8 mode;
    /* 0x04 */ u8 mpan;
    /* 0x05 */ s8 reserved0;
    /* 0x06 */ s16 attr;
    /* 0x08 */ u32 reserved1; /* index of the program's tone block */
    /* 0x0C */ u32 reserved2;
} ProgAtr;

typedef struct VagAtr {
    /* 0x00 */ u8 prior;
    /* 0x01 */ u8 mode;
    /* 0x02 */ u8 vol;
    /* 0x03 */ u8 pan;
    /* 0x04 */ u8 pad[0x1C];
} VagAtr;

typedef struct CdlLOC {
    u8 minute, second, sector, track;
} CdlLOC;

extern SoundBlock *gVabBlock[16];
extern s32 gSndMemPtr[16];
extern XABlock *gXABlock[6];
extern MIDIDesc *gMIDIDesc[6];
extern SoundEvent gEventQue[8];
extern SoundEvent *gSEvent;
extern SoundGroup D_801221E8[];
extern XAQueue D_80122528[8];
extern XAQueue *D_801101E8;
extern XAQueue *D_801101EC;
extern s16 D_801101D0[3]; /* master volumes */
extern u8 D_801101E0[];   /* MIDI registers */
extern s32 D_80107890[16]; /* MIDI mute status */

extern s32 D_8010FD78; /* global excite */
extern s16 D_8010FD7C; /* MIDI vol */
extern s16 D_8010FD7E; /* MIDI pan */
extern u32 D_8010FD84; /* XA position (sectors played) */
extern s32 D_8010FD88; /* XA prepared */
extern s32 D_8010FD8C; /* XA playing */
extern s32 D_8010FD90; /* XA paused */
extern s16 D_8010FD98; /* XA error count */
extern s32 gHasCD;
extern SoundEntry *gPlayingMIDIUnderscore; /* the underscore's sound table */
extern s16 gUnderscoreID;
extern s16 gLastXAVol;
extern s16 gLastXAPan;
extern s32 D_801101F0; /* XA: start position */
extern u32 D_801101F4; /* XA length (sectors) */
extern s32 D_801101F8; /* XA: channel */
extern s32 D_8010FD94;
extern s16 gStereoMode;
extern s16 gReverbOn;
extern s16 gReverbWaiting;
extern s16 gRevType;
extern s16 gRevDepth;
extern s16 gLoopsPaused;
extern u16 gCurrentMovieVol;
extern s32 gXferingVAB;
extern SoundBlock *gXferingVABPtr;
extern void (*gVABDoneCallback)();
extern s32 gVABDoneCallbackData_1;
extern s32 gVABDoneCallbackData_2;
extern void (*gUserXACallback)();
extern s32 gUserXACallbackFrame;
extern s32 gUserXACallbackData;
extern s32 oldreadycallback;

/* libsnd / libspu / libcd / libapi */
void SsInit(void);
void SsSetTableSize(void *table, s16 s_max, s16 t_max);
void SsSetMVol(s16 left, s16 right);
void SsSetSerialAttr(s8 s_num, s8 attr, s8 mode);
void SsSetSerialVol(s8 s_num, s16 left, s16 right);
void SsUtReverbOff(void);
s16 SsUtSetReverbType(s16 type);
void SsUtSetReverbDepth(s16 ldepth, s16 rdepth);
void SsSetTickMode(s32 mode);
void SsStart(void);
void SsEnd(void);
void SsQuit(void);
void SsSetStereo(void);
void SsSetMono(void);
void SsVabClose(s16 vab);
s16 SsVabTransCompleted(s16 mode);
s16 SsVabTransBodyPartly(u8 *addr, u32 bufsize, s16 vab);
void SpuClearReverbWorkArea(s32 mode);
s32 CdControlF(u8 com, u8 *param);
s32 CdControlB(u8 com, u8 *param, u8 *result);
s32 CdReadyCallback(s32 func);

SoundBlock *FindSoundBlockByID(s32 id);
SoundBlock *SoundRegisterVABPartly(SoundBlock *vh, u8 *vb, s32 size);
s16 PlaySoundVolPan(SoundBlock *block, s16 snd, s16 vol, s16 pan);
void StopSound(SoundBlock *block, s16 snd);
void StopAllSoundsInBlock(SoundBlock *block);
void ReleaseAllSoundBlocks(void);
void UnprepareXA(void);
void StopXA(void);
void ClearMIDIMuteStatus(void);
void SetStereoMode(s16 mode);
void SoundSeqCallback(s32 a, s32 b, s32 c);
static void func_800C1380(void);
static void func_800C1590(s32 com, s32 pos);
CdlLOC *CDLongToPos(u32 pos, CdlLOC *loc);
s16 GetSoundVol(SoundBlock *block, s16 snd, s16 which);
s16 GetSoundNote(SoundBlock *block, s16 snd, s16 which);
static void func_800C13B4(s32 status);
extern u8 gSeqTable[];
s16 AddPan(s16 pan, s16 add);
s32 PlayingXA(void);
s32 GetXAPos(void);
s16 SsVoiceCheck(s32 voice, s32 vab_pro, s16 note);
s32 SsVoKeyOn(s32 vab_pro, s32 pitch, u16 volL, u16 volR);
void SsUtChangeADSR(s16 vc, s16 vab, s16 prog, s16 old_note, u16 adsr1, u16 adsr2);
void SsUtKeyOffV(s16 voice);
void SsSeqSetVol(s16 access_num, s16 voll, s16 volr);
void SsSeqPlay(s16 access_num, s8 play_mode, s16 l_count);
void SsSepPlay(s16 access_num, s16 seq_num, s8 play_mode, s16 l_count);
MIDIDesc *FindMIDIDesc(s32 id);
SoundBlock *FindSoundBlockByNum(s32 num);
void ReverbSet(s16 type, s16 depth);
void SsSeqSetCrescendo(s16 access_num, s16 vol, s32 v_time);
void SsSepSetCrescendo(s16 access_num, s16 seq_num, s16 vol, s32 v_time);
void SetLoopingVolume(SoundEntry *s, s16 vol, s16 pan);
void SsUtChangePitch(s16 voice, s16 vab, s16 prog, s16 old_note, s16 old_fine, s16 new_note, s16 new_fine);
s16 GetPanChange(s16 flags, s16 from, s16 to);
SoundEvent *GetUnusedEvent(void);
void ResetMIDICallback(void);
s16 SsIsEos(s16 access_num, s16 seq_num);
void SsSepPause(s16 access_num, s16 seq_num);
void SsSepReplay(s16 access_num, s16 seq_num);
s32 EnterCriticalSection(void);
void ExitCriticalSection(void);
extern SoundBlock *gPlayingMIDIUnderscoreBlock;
extern s32 gMusicPaused;
extern s32 gMusicNumPaused;
extern u8 gMusicPID[]; /* SEPs paused by PauseMusicUnderscore */
void PlayXA(s32 pos, s32 chan, s32 force, s32 arg3);
void PrepareXA(void);
extern s32 gAllowCueAfter;
s32 CanIHaveTheCDPlease(s32 pos, s16 chan, s32 arg);
s32 CdGetSector(void *madr, s32 size);
extern s16 D_80122588[]; /* XA sector header buffer */
void AdjustRunningMusicVol(void);
void SsSeqClose(s16 access_num);
void SsSepClose(s16 access_num);
void SsUtAllKeyOff(s16 mode);
void StopMusicUnderscore(void);
s32 FindMIDIByID(s16 id);
s32 NextFreeMIDI(void);
s16 SsSeqOpen(u8 *addr, s16 vab);
s16 SsSepOpen(u8 *addr, s16 vab, s16 seq_num);
void SsSetMarkCallback(s16 access_num, s16 seq_num, void (*func)());
void SsSepSetVol(s16 access_num, s16 seq_num, s16 voll, s16 volr);
void GetVolumesFromPan(s16 vol, s16 pan, s16 *left, s16 *right, s16 which);
void SetMIDIVolPan(s16 vol, s16 pan);
void PauseMusicUnderscore(s32 pause);
s32 WhereIsThisFilePlease(char *name);
XABlock *FindXAByID(s32 id);
s32 NextFreeXA(void);
s32 PlayXADirect(XABlock *xa, s16 vol, s16 pan, s16 a3, s16 arg4, s32 arg5);
void PlayMusicUnderscoreVolPan(SoundBlock *block, s16 snd, s16 vol, s16 pan);
void SetSoundVol(SoundBlock *block, s16 snd, s16 vol, s16 a3, s16 arg4);
void SetSoundPan(SoundBlock *block, s16 snd, s16 pan, s16 a3, s16 arg4);
void SetSoundNote(SoundBlock *block, s16 snd, s16 note, s16 a3, s16 arg4);
void KillAllEventsForSound(SoundBlock *block, s16 snd);
SoundEvent *FindSoundEvent(s16 kind, SoundBlock *block, s16 id);
SoundEvent *NewSoundEvent(s16 kind, SoundBlock *block, s16 id);
SoundEvent *RemoveSoundEvent(SoundEvent *ev);
void GetVoiceVolume(SoundBlock *block, s16 prog, s16 tone, s16 vol, s16 pan, s16 *left, s16 *right);
s16 GetVoiceFromField(s32 field, s16 n);
void ReverbOn(void);
void ReverbOff(void);
void SsVoKeyOff(s32 vab_pro, s32 pitch);
void SsSeqStop(s16 access_num);
void SsSepStop(s16 access_num, s16 seq_num);
void SsUtReverbOn(void);
void SsUtSetDetVVol(s16 vc, s16 voll, s16 volr);
s16 SsVabTransBody(u8 *addr, s16 vab);
s16 SsVabOpenHeadSticky(u8 *addr, s16 vab, u32 sbaddr);

s32 SetupSound(s32 hasCD) {
    s16 i;

    SsInit();
    gHasCD = hasCD;
    SsSetTableSize(gSeqTable, 1, 0x12);
    for (i = 0; i < 8; i++) {
        gEventQue[i].id = -1;
    }
    for (i = 0; i < 16; i++) {
        gVabBlock[i] = NULL;
    }
    for (i = 0; i < 6; i++) {
        gMIDIDesc[i] = NULL;
    }
    for (i = 0; i < 3; i++) {
        D_801101D0[i] = 0x7F;
    }
    SsSetMVol(0x7F, 0x7F);
    SsSetSerialAttr(0, 0, 1);
    SsSetSerialVol(0, 0x7F, 0x7F);
    gReverbOn = 0;
    SsUtReverbOff();
    SsUtSetReverbType(0);
    SsUtSetReverbDepth(0, 0);
    ClearMIDIMuteStatus();
    SsSetTickMode(3);
    SsStart();
    SetStereoMode(1);
    return 1;
}

SoundBlock *SoundRegisterVAB(SoundBlock *vh, u8 *vb) {
    return SoundRegisterVABPartly(vh, vb, 0);
}

SoundBlock *SoundRegisterVABContinue(SoundBlock *block, u8 *vb, s32 size) {
    s16 ret = SsVabTransBodyPartly(vb, size, block->vabId);

    if (ret == -1) {
        gXferingVAB = ret;
        gXferingVABPtr = NULL;
        return NULL;
    }
    gXferingVABPtr = block;
    gXferingVAB = block->vabId;
    if (ret == block->vabId) {
        return block;
    }
    return NULL;
}

SoundBlock *SoundRegisterVABPartly(SoundBlock *vh, u8 *vb, s32 size) {
    SoundEntry saved[69]; /* the sound table, saved while libsnd parses the header */
    s32 i;
    s32 vab;
    s32 offset;
    s32 id;
    SoundEntry *sounds;
    s32 n;
    s32 ret;
    s32 type;

    if (vh->vabId == -1) {
        for (ret = 8; ret < 16; ret++) {
            if (gVabBlock[ret] == NULL) {
                break;
            }
        }
        if (ret == 16) {
            return NULL;
        }
        vh->vabId = ret;
    }
    vab = vh->vabId;
    if (gSndMemPtr[vab] == 0) {
        return NULL;
    }
    offset = (s32)vh->sounds;
    n = (u8)vh->numSounds;
    id = vh->id;
    sounds = (SoundEntry *)((u8 *)vh + offset);
    for (i = 0; i < n; i++) {
        saved[i] = sounds[i];
    }
    vh->id = 0x56414270; /* "pBAV" */
    vh->sounds = (SoundEntry *)7;
    ret = SsVabOpenHeadSticky((u8 *)vh, vh->vabId, gSndMemPtr[vh->vabId]);
    if (ret == -1) {
        return NULL;
    }
    vh->vabId = ret;
    if (size == 0) {
        ret = SsVabTransBody(vb, ret);
    } else {
        ret = SsVabTransBodyPartly(vb, size, ret);
    }
    gXferingVABPtr = vh;
    vh->id = id;
    vh->sounds = sounds;
    gXferingVAB = vh->vabId;
    for (i = 0; i < n; i++) {
        sounds[i] = saved[i];
        type = sounds[i].flags & 0x1F;
        if (!(type == 7 || type == 8) && !(type == 9 || type == 10)) {
            sounds[i].vab = vab;
            if (type != SND_TYPE_SEQ) {
                sounds[i].seq = sounds[i].tone;
            }
        }
    }
    if (ret == -1) {
        vh->sounds = (SoundEntry *)offset;
        gXferingVAB = ret;
        gXferingVABPtr = NULL;
        return NULL;
    }
    if (size == 0 || ret >= 0) {
        return vh;
    }
    return NULL;
}

s16 SoundRegisterVABComplete(void) {
    s16 done = SsVabTransCompleted(0);

    if (done != 0) {
        s32 vab = gXferingVAB;
        if (vab != -1) {
            gXferingVAB = -1;
            gVabBlock[vab] = gXferingVABPtr;
        }
    }
    return done;
}

void SoundRegisterVABCompleteCallback(void (*cb)(), s32 data1, s32 data2) {
    gVABDoneCallback = cb;
    gVABDoneCallbackData_1 = data1;
    gVABDoneCallbackData_2 = data2;
}

XABlock *SoundRegisterXA(XABlock *xa) {
    s16 i;

    if (gHasCD != 0) {
        i = NextFreeXA();
        if (i != -1) {
            gXABlock[i] = xa;
            xa->pos = WhereIsThisFilePlease(xa->name);
            return xa;
        }
    }
    return NULL;
}

void ReleaseXA(XABlock *xa) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (gXABlock[i] == xa) {
            gXABlock[i] = NULL;
            return;
        }
    }
}

void SetXACallbackFunction(void (*cb)(), s32 frame, s32 data) {
    gUserXACallback = cb;
    gUserXACallbackFrame = frame;
    gUserXACallbackData = data;
}

s32 SoundRegisterMIDI(MIDIDesc *midi) {
    SoundBlock *block = FindSoundBlockByID(midi->blockId);
    s32 vab;
    s16 slot;
    s32 i;

    if (block == NULL) {
        return 0;
    }
    vab = block->vabId;
    slot = NextFreeMIDI();
    if (slot == -1) {
        return 0;
    }
    gMIDIDesc[slot] = midi;
    if (midi->type == 0) {
        gMIDIDesc[slot]->seq = SsSeqOpen(midi->data, vab);
        SsSetMarkCallback(gMIDIDesc[slot]->seq, 0, SoundSeqCallback);
        return 1;
    }
    gMIDIDesc[slot]->seq = SsSepOpen(midi->data, vab, midi->numSeqs);
    for (i = 0; i < midi->numSeqs; i++) {
        SsSetMarkCallback(gMIDIDesc[slot]->seq, i, SoundSeqCallback);
    }
    return 1;
}

void ClearVABsResolvedFlag(SoundBlock *block) {
    block->numSounds &= 0x7FFFFFFF;
}

#ifdef NON_MATCHING
/* regalloc: s4/s5 swapped in loop 1; target copies type to a3 for the 7/9/10 tests */
void SetupAllLoaded(s16 revType, s16 revDepth) {
    s32 n;
    s32 i;
    s16 j;
    u8 count;
    MIDIDesc *m;
    SoundBlock *other;
    s32 type;
    u8 idx;
    s32 byID;

    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL && !(gVabBlock[i]->numSounds & 0x80000000)) {
            n = gVabBlock[i]->numSounds & 0xFF;
            for (j = 0; j < n; j++) {
                if ((gVabBlock[i]->sounds[j].flags & 0x1F) == SND_TYPE_SEQ) {
                    m = FindMIDIDesc(gVabBlock[i]->sounds[j].voices);
                    if (m != NULL) {
                        gVabBlock[i]->sounds[j].seq = m->seq;
                    } else {
                        gVabBlock[i]->sounds[j].seq = 0xFF;
                    }
                }
            }
        }
    }
    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL) {
            count = gVabBlock[i]->numSounds;
            for (j = 0; j < count; j++) {
                type = gVabBlock[i]->sounds[j].flags & 0x1F;
                if ((s32)gVabBlock[i]->numSounds < 0 && ((s16)gVabBlock[i]->sounds[j].flags >> 5) != 0) {
                    type = 9;
                    if ((s16)gVabBlock[i]->sounds[j].flags & 0x8000) {
                        type = 10;
                    }
                    gVabBlock[i]->sounds[j].voices = ((s16)gVabBlock[i]->sounds[j].flags >> 11) & 0xF;
                    gVabBlock[i]->sounds[j].vab = (gVabBlock[i]->sounds[j].flags >> 5) & 0x3F;
                }
                if ((byID = (type == 7 || type == 8)) || type == 9 || type == 10) {
                    idx = gVabBlock[i]->sounds[j].vab;
                    if (byID) {
                        other = FindSoundBlockByID(gVabBlock[i]->sounds[j].voices);
                    } else {
                        other = FindSoundBlockByNum(gVabBlock[i]->sounds[j].voices);
                    }
                    if (other != NULL) {
                        gVabBlock[i]->sounds[j].vab = other->vabId;
                        gVabBlock[i]->sounds[j].seq = other->sounds[idx].seq;
                        gVabBlock[i]->sounds[j].sepNum = other->sounds[idx].sepNum;
                        gVabBlock[i]->sounds[j].prog = other->sounds[idx].prog;
                        gVabBlock[i]->sounds[j].firstTone = other->sounds[idx].firstTone;
                        if (type == 7 || type == 9) {
                            gVabBlock[i]->sounds[j].tone = other->sounds[idx].tone;
                            gVabBlock[i]->sounds[j].note = other->sounds[idx].note;
                            gVabBlock[i]->sounds[j].vol = other->sounds[idx].vol;
                            gVabBlock[i]->sounds[j].pan = other->sounds[idx].pan;
                            gVabBlock[i]->sounds[j].defVol = other->sounds[idx].defVol;
                            gVabBlock[i]->sounds[j].defPan = other->sounds[idx].defPan;
                            gVabBlock[i]->sounds[j].unk12 = other->sounds[idx].unk12;
                            if (type == 9) {
                                gVabBlock[i]->sounds[j].flags = (other->sounds[idx].flags & 0x1F) | (idx << 5) |
                                                                (gVabBlock[i]->sounds[j].voices << 11);
                            } else {
                                gVabBlock[i]->sounds[j].flags = other->sounds[idx].flags;
                            }
                        } else if (type == 10) {
                            gVabBlock[i]->sounds[j].flags = (other->sounds[idx].flags & 0x1F) | (idx << 5) |
                                                            (gVabBlock[i]->sounds[j].voices << 11) | 0x8000;
                        } else {
                            gVabBlock[i]->sounds[j].flags = other->sounds[idx].flags;
                        }
                    }
                } else {
                    gVabBlock[i]->sounds[j].vab = gVabBlock[i]->vabId;
                }
                if ((gVabBlock[i]->sounds[j].flags & 0x1F) != SND_TYPE_SEQ && (s32)gVabBlock[i]->numSounds >= 0) {
                    gVabBlock[i]->sounds[j].seq = gVabBlock[i]->sounds[j].tone;
                }
            }
            gVabBlock[i]->numSounds |= 0x80000000;
        }
    }
    ReverbSet(revType, revDepth);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetupAllLoaded);
#endif

void ReleaseAllSoundBlocks(void) {
    s16 j;
    s32 i;

    if (gPlayingMIDIUnderscore != NULL) {
        StopMusicUnderscore();
    }
    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL) {
            StopAllSoundsInBlock(gVabBlock[i]);
        }
    }
    i = 0;
    while (gMIDIDesc[i] != NULL) {
        if (gMIDIDesc[i]->type == 0) {
            SsSeqStop(gMIDIDesc[i]->seq);
            SsSetMarkCallback(gMIDIDesc[i]->seq, 0, NULL);
            SsSeqClose(gMIDIDesc[i]->seq);
        } else {
            for (j = 0; j < gMIDIDesc[i]->numSeqs; j++) {
                SsSepStop(gMIDIDesc[i]->seq, j);
                SsSetMarkCallback(gMIDIDesc[i]->seq, j, NULL);
            }
            SsSepClose(gMIDIDesc[i]->seq);
        }
        gMIDIDesc[i] = NULL;
    }
    SsUtAllKeyOff(0);
    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL) {
            SsVabClose(gVabBlock[i]->vabId);
            gVabBlock[i] = NULL;
        }
    }
    SsEnd();
    SsStart();
}

void ReleaseOneSoundBlock(SoundBlock *block) {
    StopAllSoundsInBlock(block);
    SsVabClose(block->vabId);
    gVabBlock[block->vabId] = NULL;
}

void ReleaseMIDI(MIDIDesc *midi) {
    s16 i = 0;
    s16 j;

    while (gMIDIDesc[i] != midi && i < 6) {
        i++;
    }
    if (i == 6) {
        return;
    }
    if (gMIDIDesc[i]->type == 0) {
        SsSeqStop(gMIDIDesc[i]->seq);
        SsSeqClose(gMIDIDesc[i]->seq);
    } else {
        for (j = 0; j < gMIDIDesc[i]->numSeqs; j++) {
            SsSepStop(gMIDIDesc[i]->seq, j);
            SsSetMarkCallback(gMIDIDesc[i]->seq, j, NULL);
        }
        SsSepClose(gMIDIDesc[i]->seq);
    }
    for (; i < 5; i++) {
        gMIDIDesc[i] = gMIDIDesc[i + 1];
    }
    gMIDIDesc[i] = NULL;
}

void ShutdownSound(void) {
    gReverbOn = 0;
    gRevType = 0;
    gRevDepth = 0;
    SsUtReverbOff();
    SsUtSetReverbType(0);
    SsUtSetReverbDepth(0, 0);
    SpuClearReverbWorkArea(gRevType);
    ReleaseAllSoundBlocks();
    UnprepareXA();
    SsEnd();
    SsQuit();
}

s16 PlaySoundVolPan(SoundBlock *block, s16 snd, s16 vol, s16 pan) {
    SoundEntry *sounds = block->sounds;
    s16 left, right;
    s16 i;
    s16 ret = snd;

    if (snd == -1) {
        return -1;
    }
    if ((u8)block->numSounds - 1 < snd) {
        return -1;
    }
    if (gVabBlock[block->vabId] == NULL) {
        return -1;
    }
    if (gVabBlock[sounds[snd].vab] == NULL) {
        return -1;
    }
    if ((sounds[snd].flags & 0x1F) == SND_TYPE_GROUP) {
        for (i = 0; i < sounds[snd].prog; i++) {
            if (sounds[i + snd + 1].numVoices == -1) {
                return PlaySoundVolPan(block, snd + (s16)(i + 1), vol, pan);
            }
            if ((sounds[i + snd + 1].flags & 0x1F) == SND_TYPE_VOICED &&
                SsVoiceCheck(GetVoiceFromField(sounds[snd].voices, i), (sounds[snd].vab << 8) | sounds[snd].prog, sounds[snd].tone) != 0) {
                return PlaySoundVolPan(block, snd + (s16)(i + 1), vol, pan);
            }
        }
        return -1;
    }
    if (vol != -2) {
        if (vol == -1) {
            sounds[snd].vol = sounds[snd].defVol;
        } else {
            sounds[snd].vol = vol;
        }
    }
    if (pan != -2) {
        if (pan == -1) {
            sounds[snd].pan = sounds[snd].defPan;
        } else {
            sounds[snd].pan = pan;
        }
    }
    if (gReverbWaiting != 0) {
        ReverbOn();
    }
    switch (sounds[snd].flags & 0x1F) {
    case SND_TYPE_ONESHOT:
        GetVolumesFromPan(sounds[snd].vol, sounds[snd].pan, &left, &right, sounds[snd].unk12);
        if (left != 0 || right != 0) {
            EnterCriticalSection();
            SsVoKeyOn((sounds[snd].vab << 8) | sounds[snd].prog, (sounds[snd].tone << 8) | sounds[snd].note, left, right);
            ExitCriticalSection();
        }
        break;
    case SND_TYPE_VOICED:
        GetVolumesFromPan(sounds[snd].vol, sounds[snd].pan, &left, &right, sounds[snd].unk12);
        if (left != 0 || right != 0) {
            if (sounds[snd].numVoices != -1) {
                for (i = 0; i < sounds[snd].numVoices; i++) {
                    if (SsVoiceCheck(GetVoiceFromField(sounds[snd].voices, i), (sounds[snd].vab << 8) | sounds[snd].prog, sounds[snd].tone) == 0) {
                        SsUtChangeADSR(GetVoiceFromField(sounds[snd].voices, i), sounds[snd].vab, sounds[snd].prog, sounds[snd].tone, 0, 0);
                        SsUtKeyOffV(GetVoiceFromField(sounds[snd].voices, i));
                    }
                }
            }
            EnterCriticalSection();
                sounds[snd].voices = SsVoKeyOn((sounds[snd].vab << 8) | sounds[snd].prog, (sounds[snd].tone << 8) | sounds[snd].note, left, right);
            ExitCriticalSection();
            sounds[snd].numVoices = 0;
            for (i = 0; i < 24; i++) {
                if ((sounds[snd].voices >> i) & 1) {
                    sounds[snd].numVoices++;
                }
            }
            if (sounds[snd].numVoices == 0) {
                sounds[snd].numVoices = -1;
            }
        }
        break;
    case SND_TYPE_LOOP:
        gLoopsPaused = 0;
        if (sounds[snd].numVoices != -1) {
            return -1;
        }
        GetVolumesFromPan(sounds[snd].vol, sounds[snd].pan, &left, &right, sounds[snd].unk12);
        EnterCriticalSection();
        sounds[snd].voices = SsVoKeyOn((sounds[snd].vab << 8) | sounds[snd].prog, (sounds[snd].tone << 8) | sounds[snd].note, 1, 1);
        ExitCriticalSection();
        sounds[snd].numVoices = 0;
        sounds[snd].playTone = sounds[snd].tone;
        for (i = 0; i < 24; i++) {
            if ((sounds[snd].voices >> i) & 1) {
                sounds[snd].numVoices++;
            }
        }
        if (sounds[snd].numVoices == 0) {
            sounds[snd].numVoices = -1;
        }
        SetLoopingVolume(&sounds[snd], left, right);
        break;
    case SND_TYPE_SEQ:
        GetVolumesFromPan(sounds[snd].vol, sounds[snd].pan, &left, &right, sounds[snd].unk12);
        if (left != 0 || right != 0) {
            if (sounds[snd].sepNum == -1) {
                SsSeqSetVol(sounds[snd].seq, left, right);
                SsSeqPlay(sounds[snd].seq, 1, sounds[snd].prog);
            } else {
                SsSepSetVol(sounds[snd].seq, sounds[snd].sepNum, left, right);
                SsSepPlay(sounds[snd].seq, sounds[snd].sepNum, 1, sounds[snd].prog);
            }
            break;
        }
    case 7:
    case 8:
    case 9:
    case 10:
        ret = -1;
        break;
    }
    return ret;
}

s16 PlaySoundVolPanByID(s32 id, s16 snd, s16 vol, s16 pan) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block == NULL) {
        return -1;
    }
    return PlaySoundVolPan(block, snd, vol, pan);
}

s16 PlaySoundVolPanByNum(u32 num, s16 snd, s16 vol, s16 pan) {
    if (num < 16 && gVabBlock[num] != NULL) {
        return PlaySoundVolPan(gVabBlock[num], snd, vol, pan);
    }
    return -1;
}

s32 PlayXADirect(XABlock *xa, s16 track, s16 vol, s16 pan, s16 allowCue, s32 arg5) {
    s16 left, right;
    s16 file = track / 15;
    s16 chan = track - file * 15;
    s32 *offsets;
    s32 pos;
    s32 v;

    if (file != 0) {
        offsets = xa->offsets;
        pos = xa->pos + offsets[file];
    } else {
        pos = xa->pos;
    }
    if (gHasCD != 0 && CanIHaveTheCDPlease(pos, track, arg5) != 0) {
        gLastXAVol = vol;
        gLastXAPan = pan;
        v = vol * 60 / 100;
        if (v >= 0x80) {
            v = 0x7F;
        }
        GetVolumesFromPan(v, pan, &left, &right, 2);
        if (left != 0 || right != 0) {
            SsSetSerialVol(0, left, right);
            PrepareXA();
            PlayXA(pos, chan, gAllowCueAfter, arg5);
            gAllowCueAfter = allowCue;
            if (gReverbWaiting != 0) {
                ReverbOn();
            }
        }
        return 1;
    }
    return 0;
}

s32 PlayXADirectwCB(XABlock *xa, s16 vol, s16 pan, s16 a3, s16 arg4, s32 arg5, void (*cb)()) {
    if (PlayXADirect(xa, vol, pan, a3, arg4, arg5) != 0) {
        gUserXACallback = cb;
        gUserXACallbackFrame = -1;
        return 1;
    }
    return 0;
}

s32 PlayXADirectByID(s32 id, s16 vol, s16 pan, s16 a3, s16 arg4, s32 arg5) {
    XABlock *xa = FindXAByID(id);

    if (xa == NULL) {
        return 0;
    }
    return PlayXADirect(xa, vol, pan, a3, arg4, arg5);
}

s32 PlayXADirectByIDwCB(s32 id, s16 vol, s16 pan, s16 a3, s16 arg4, s32 arg5, void (*cb)()) {
    if (PlayXADirectByID(id, vol, pan, a3, arg4, arg5) != 0) {
        gUserXACallback = cb;
        gUserXACallbackFrame = -1;
        return 1;
    }
    return 0;
}

void PlayMusicUnderscoreVolPan(SoundBlock *block, s16 snd, s16 vol, s16 pan) {
    SoundEntry *sounds;

    if (gPlayingMIDIUnderscore != NULL) {
        StopMusicUnderscore();
    }
    sounds = block->sounds;
    if (vol != -2) {
        if (vol == -1) {
            sounds[snd].vol = sounds[snd].defVol;
        } else {
            sounds[snd].vol = vol;
        }
    }
    if (pan != -2) {
        if (pan == -1) {
            sounds[snd].pan = sounds[snd].defPan;
        } else {
            sounds[snd].pan = pan;
        }
    }
    ResetMIDICallback();
    SetMIDIVolPan(sounds[snd].vol, sounds[snd].pan);
    gPlayingMIDIUnderscore = sounds;
    gPlayingMIDIUnderscoreBlock = block;
    gUnderscoreID = snd;
    gMusicPaused = 0;
    gMusicNumPaused = 0;
    if (PlaySoundVolPan(block, snd, sounds[snd].vol, sounds[snd].pan) == -1) {
        gMusicPaused = 1;
    }
}

void PlayMusicUnderscoreVolPanByID(s32 id, s16 snd, s16 vol, s16 pan) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        PlayMusicUnderscoreVolPan(block, snd, vol, pan);
    }
}

void PauseMusicUnderscore(s32 pause) {
    s32 idx;
    s32 i;

    if (gPlayingMIDIUnderscore != NULL) {
        if (gPlayingMIDIUnderscore[gUnderscoreID].sepNum != -1 && gMusicPaused != pause) {
            if (pause) {
                gMusicNumPaused = 0;
                idx = FindMIDIByID(gPlayingMIDIUnderscore[gUnderscoreID].seq);
                for (i = 0; i < gMIDIDesc[idx]->numSeqs; i++) {
                    if (SsIsEos(gPlayingMIDIUnderscore[gUnderscoreID].seq, i)) {
                        gMusicPaused = 1;
                        gMusicPID[gMusicNumPaused] = i;
                        gMusicNumPaused++;
                        SsSepPause(gPlayingMIDIUnderscore[gUnderscoreID].seq, i);
                    }
                }
            } else {
                gMusicPaused = 0;
                if (gMusicNumPaused != 0) {
                    EnterCriticalSection();
                    for (i = 0; i < gMusicNumPaused; i++) {
                        SsSepReplay(gPlayingMIDIUnderscore[gUnderscoreID].seq, gMusicPID[i]);
                    }
                    gMusicNumPaused = 0;
                    ExitCriticalSection();
                } else if (PlaySoundVolPan(gPlayingMIDIUnderscoreBlock, gUnderscoreID, gPlayingMIDIUnderscore[gUnderscoreID].vol,
                                           gPlayingMIDIUnderscore[gUnderscoreID].pan) == -1) {
                    gMusicPaused = 1;
                }
            }
        }
    }
}

void StopMusicUnderscore(void) {
    s32 i;
    s32 idx;

    if (gPlayingMIDIUnderscore != NULL) {
        if (gPlayingMIDIUnderscore[gUnderscoreID].sepNum != -1) {
            i = 0;
            idx = FindMIDIByID(gPlayingMIDIUnderscore[gUnderscoreID].seq);
            for (; i < gMIDIDesc[idx]->numSeqs; i++) {
                SsSepStop(gPlayingMIDIUnderscore[gUnderscoreID].seq, i);
            }
            gPlayingMIDIUnderscore = NULL;
        }
    }
}

void AdjustRunningMusicVol(void) {
    s16 left, right;
    s32 idx;
    s32 i;

    if (gPlayingMIDIUnderscore != NULL) {
        SetMIDIVolPan(gPlayingMIDIUnderscore[gUnderscoreID].vol, gPlayingMIDIUnderscore[gUnderscoreID].pan);
        GetVolumesFromPan(gPlayingMIDIUnderscore[gUnderscoreID].vol, gPlayingMIDIUnderscore[gUnderscoreID].pan, &left, &right,
                          gPlayingMIDIUnderscore[gUnderscoreID].unk12);
        idx = FindMIDIByID(gPlayingMIDIUnderscore[gUnderscoreID].seq);
        if (idx != -1) {
            for (i = 0; i < gMIDIDesc[idx]->numSeqs; i++) {
                SsSepSetVol(gPlayingMIDIUnderscore[gUnderscoreID].seq, i, left, right);
            }
        }
        if (left == 0 && right == 0) {
            PauseMusicUnderscore(1);
        } else {
            PauseMusicUnderscore(0);
        }
    }
}

void SetMusicUnderscoreVol(SoundBlock *block, s16 snd, s16 vol, s16 time, s16 mode) {
    SoundEntry *sounds;
    s16 diff;
    s16 stop;
    s16 d;
    SoundEvent *ev;

    if (snd != -1 && block != NULL) {
        sounds = block->sounds;
        if (vol == -1) {
            vol = sounds[snd].defVol;
        }
        stop = (vol == -3);
        if (stop) {
            vol = 0;
        }
        if (gPlayingMIDIUnderscore == NULL) {
            sounds[snd].vol = vol;
            return;
        }
        if (time == 0) {
            RemoveSoundEvent(FindSoundEvent(10, block, snd));
            sounds[snd].vol = vol;
            AdjustRunningMusicVol();
            if (stop) {
                StopMusicUnderscore();
            }
            return;
        }
        diff = vol - sounds[snd].vol;
        if (diff != 0) {
            ev = FindSoundEvent(10, block, snd);
            if (mode == 3 && ev != NULL) {
                d = vol - ev->value;
                if (d == 0) {
                    sounds[snd].vol = vol;
                    AdjustRunningMusicVol();
                    if (stop) {
                        StopMusicUnderscore();
                    }
                    RemoveSoundEvent(ev);
                    return;
                }
                time = abs(diff * time / d);
                if (time <= 0) {
                    time = 1;
                }
            }
            if (ev == NULL) {
                ev = NewSoundEvent(10, block, snd);
            }
            if (time > abs(diff)) {
                ev->unk08 = abs(time / diff);
                ev->unk0E = 1;
                ev->unk0C = ev->unk08;
                if (vol >= sounds[snd].vol) {
                    ev->step = 1;
                } else {
                    ev->step = -1;
                }
            } else {
                ev->step = diff / time;
                ev->unk0E = 0;
            }
            ev->value = vol;
            if (stop) {
                ev->unk14 = 1;
            } else {
                ev->unk14 = 0;
            }
        }
    }
}

#ifdef NON_MATCHING
/* regalloc: target gives snd/time/mode/diff/stop stack homes (frame 0x70 vs 0x60); structure matches */
void SetSoundVol(SoundBlock *block, s16 snd, s16 vol, s16 time, s16 mode) {
    SoundEntry *sounds;
    SoundEntry *s;
    SoundEvent *ev = NULL;
    s16 left, right;
    s16 stop;
    s16 diff;
    s16 d;
    s16 i;
    s32 t;
    s32 type;
    s32 master;
    s32 newVol, oldVol;

    if (snd != -1 && (u8)block->numSounds - 1 >= snd) {
        sounds = block->sounds;
        if (vol == -1) {
            vol = sounds[snd].defVol;
        }
        stop = (vol == -3);
        if (stop) {
            vol = 0;
        }
        s = &sounds[snd];
        type = s->flags & 0x1F;
        if (type == SND_TYPE_GROUP) {
            for (i = 0; i < s->prog; i++) {
                if (sounds[i + snd + 1].numVoices == -1) {
                    SetSoundVol(block, snd + (s16)(i + 1), vol, 0, 2);
                }
            }
            return;
        }
        switch (type) {
        case SND_TYPE_ONESHOT:
        case SND_TYPE_VOICED:
            sounds[snd].vol = vol;
            break;
        case SND_TYPE_LOOP:
            gLoopsPaused = 0;
            if (s->numVoices == -1) {
                s->vol = vol;
                return;
            }
            if (time == 0) {
                RemoveSoundEvent(FindSoundEvent(4, block, snd));
                s->vol = vol;
                GetVolumesFromPan(s->vol, s->pan, &left, &right, s->unk12);
                SetLoopingVolume(s, left, right);
            } else {
                diff = vol - s->vol;
                if (diff == 0) {
                    return;
                }
                ev = FindSoundEvent(4, block, snd);
                if (mode == 3) {
                    if (ev != NULL) {
                        d = vol - ev->value;
                        if (d == 0) {
                            s->vol = vol;
                            GetVolumesFromPan(s->vol, s->pan, &left, &right, s->unk12);
                            SetLoopingVolume(s, left, right);
                            RemoveSoundEvent(ev);
                            goto done;
                        }
                        time = abs(diff * time / d);
                        if (time <= 0) {
                            time = 1;
                        }
                    }
                }
                if (ev == NULL) {
                    ev = NewSoundEvent(4, block, snd);
                }
                if (time > abs(diff)) {
                    ev->unk08 = abs(time / diff);
                    ev->unk0E = 1;
                    ev->unk0C = ev->unk08;
                    if (vol >= sounds[snd].vol) {
                        ev->step = 1;
                    } else {
                        ev->step = -1;
                    }
                } else {
                    ev->step = diff / time;
                    ev->unk0E = 0;
                }
                ev->value = vol;
                if (stop) {
                    ev->unk14 = 1;
                } else {
                    ev->unk14 = 0;
                }
                return;
            }
        done:
            if (stop) {
                StopSound(block, snd);
            }
            break;
        case SND_TYPE_SEQ:
            master = D_801101D0[s->unk12];
            newVol = vol * master / 127;
            oldVol = s->vol * master / 127;
            if (newVol != oldVol) {
                if (s->sepNum == -1) {
                    SsSeqSetCrescendo(s->seq, newVol - oldVol, time);
                } else {
                    SsSepSetCrescendo(s->seq, s->sepNum, newVol - oldVol, time);
                }
                sounds[snd].vol = vol;
                if (stop) {
                    ev = NewSoundEvent(9, block, snd);
                    ev->unk0E = 1;
                    ev->unk0C = time;
                }
            }
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/snddrv", SetSoundVol);
#endif

void SetSoundVolByID(s32 id, s16 snd, s16 vol, s16 a3, s16 arg4) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        SetSoundVol(block, snd, vol, a3, arg4);
    }
}

void SetSoundVolByNum(u32 num, s16 snd, s16 vol, s16 a3, s16 arg4) {
    if (num < 16 && gVabBlock[num] != NULL) {
        SetSoundVol(gVabBlock[num], snd, vol, a3, arg4);
    }
}

s16 GetSoundVol(SoundBlock *block, s16 snd, s16 which) {
    SoundEntry *s;

    if (snd == -1) {
        return -1;
    }
    if ((u8)block->numSounds - 1 >= snd) {
        s = block->sounds;
        if ((s[snd].flags & 0x1F) == SND_TYPE_GROUP) {
            snd++;
            which = -1;
        }
        if (which != -2) {
            if (which == -1) {
                return s[snd].defVol;
            }
            return 0;
        }
        return s[snd].vol;
    }
    return 0;
}

s16 GetSoundVolByID(s32 id, s16 snd, s16 which) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block == NULL) {
        return 0;
    }
    return GetSoundVol(block, snd, which);
}

s16 GetSoundVolByNum(u32 num, s16 snd, s16 which) {
    if (num < 16 && gVabBlock[num] != NULL) {
        return GetSoundVol(gVabBlock[num], snd, which);
    }
    return -1;
}

void PauseAllLoops(s32 pause) {
    u16 saved[3];
    SoundEntry *sounds;
    s32 i, j;
    s32 type;
    u8 n;

    if (pause != gLoopsPaused) {
        if (pause) {
            for (i = 0; i < 3; i++) {
                saved[i] = D_801101D0[i];
                D_801101D0[i] = 0;
            }
        }
        for (i = 0; i < 16; i++) {
            if (gVabBlock[i] != NULL) {
                n = gVabBlock[i]->numSounds;
                sounds = gVabBlock[i]->sounds;
                for (j = 0; j < n; j++) {
                    type = sounds[j].flags & 0x1F;
                    if ((type == SND_TYPE_LOOP || type == 3) && sounds[j].numVoices != -1) {
                        SetSoundVol(gVabBlock[i], j, sounds[j].vol, 0, 2);
                    }
                }
            }
        }
        if (pause) {
            for (i = 0; i < 3; i++) {
                D_801101D0[i] = saved[i];
            }
        }
        gLoopsPaused = pause;
    }
}

void SetSoundPan(SoundBlock *block, s16 snd, s16 pan, s16 dir, s16 time) {
    SoundEntry *sounds;
    SoundEvent *ev;
    s16 left, right;
    s16 vl, vr;
    s16 i;
    s16 change;
    s16 voice;
    s32 t;
    s32 type;

    if (snd != -1 && (u8)block->numSounds - 1 >= snd) {
        sounds = block->sounds;
        if (pan == -1) {
            pan = sounds[snd].defPan;
        }
        type = sounds[snd].flags & 0x1F;
        if (type == SND_TYPE_GROUP) {
            for (i = 0; i < sounds[snd].prog; i++) {
                if (sounds[i + snd + 1].numVoices == -1) {
                    SetSoundPan(block, snd + (s16)(i + 1), pan, 1, 0);
                }
            }
        } else {
            switch (type) {
            case SND_TYPE_ONESHOT:
            case SND_TYPE_VOICED:
                sounds[snd].pan = pan;
                break;
            case SND_TYPE_LOOP:
                if (sounds[snd].numVoices == -1) {
                    sounds[snd].pan = pan;
                    return;
                }
                if (time == 0) {
                    sounds[snd].pan = pan;
                    GetVolumesFromPan(sounds[snd].vol, pan, &left, &right, sounds[snd].unk12);
                    for (i = 0; i < sounds[snd].numVoices; i++) {
                        voice = GetVoiceFromField(sounds[snd].voices, i);
                        GetVoiceVolume(gVabBlock[sounds[snd].vab], sounds[snd].prog, i, left, right, &vl, &vr);
                        SsUtSetDetVVol(voice, vl, vr);
                    }
                } else {
                    change = GetPanChange(dir, sounds[snd].pan, pan);
                    if (change != 0) {
                        ev = NewSoundEvent(3, block, snd);
                        if (abs(change) < time) {
                            ev->step = dir;
                            ev->unk08 = abs(time / change);
                            ev->unk0E = 1;
                            ev->unk0C = ev->unk08;
                        } else {
                            ev->step = change / time;
                            ev->unk0E = 0;
                        }
                        ev->value = pan;
                        ev->unk12 = -change;
                    }
                }
                break;
            case SND_TYPE_SEQ:
                if (pan != sounds[snd].pan) {
                    change = GetPanChange(dir, sounds[snd].pan, pan);
                    ev = NewSoundEvent(2, block, snd);
                    if (time > abs(change)) {
                        ev->step = dir;
                        ev->unk08 = abs(time / change);
                        ev->unk0E = 1;
                        ev->unk0C = ev->unk08;
                    } else {
                        ev->step = change / time;
                        ev->unk0E = 0;
                    }
                    ev->value = pan;
                    ev->unk12 = -change;
                }
                break;
            }
        }
    }
}

void SetSoundPanByID(s32 id, s16 snd, s16 pan, s16 a3, s16 arg4) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        SetSoundPan(block, snd, pan, a3, arg4);
    }
}

void SetSoundPanByNum(u32 num, s16 snd, s16 pan, s16 a3, s16 arg4) {
    if (num < 16 && gVabBlock[num] != NULL) {
        SetSoundPan(gVabBlock[num], snd, pan, a3, arg4);
    }
}

void SetSoundNote(SoundBlock *block, s16 snd, s16 note, s16 time, s16 mode) {
    SoundEntry *sounds;
    SoundEvent *ev;
    s32 i;
    s32 diff;
    s32 target;
    s32 d;
    s32 t;
    s32 type;

    if (snd != -1 && (u8)block->numSounds - 1 >= snd) {
        sounds = block->sounds;
        type = sounds[snd].flags & 0x1F;
        if (type == SND_TYPE_GROUP) {
            for (i = 0; i < sounds[snd].prog; i++) {
                if (sounds[i + snd + 1].numVoices == -1) {
                    SetSoundNote(block, snd + (s16)(i + 1), note, 0, 2);
                }
            }
            return;
        }
        if (note == -1) {
            note = sounds[snd].seq;
        }
        if (note >= 0x80) {
            note = 0x7F;
        }
        switch (type) {
        case SND_TYPE_ONESHOT:
        case SND_TYPE_VOICED:
            sounds[snd].tone = note;
            break;
        case SND_TYPE_LOOP:
            if (sounds[snd].numVoices == -1) {
                sounds[snd].tone = note;
                return;
            }
            ev = FindSoundEvent(0, block, snd);
            if (time == 0) {
                RemoveSoundEvent(ev);
                sounds[snd].tone = note;
                for (i = 0; i < sounds[snd].numVoices; i++) {
                    SsUtChangePitch(GetVoiceFromField(sounds[snd].voices, i), sounds[snd].vab, sounds[snd].prog, sounds[snd].playTone, 0, sounds[snd].tone, sounds[snd].note);
                }
            } else {
                target = note * 127;
                diff = target - (sounds[snd].tone * 127 + sounds[snd].note);
                if (diff == 0) {
                    RemoveSoundEvent(ev);
                    return;
                }
                if (mode == 3 && ev != NULL) {
                    d = target - ((ev->value >> 8) * 127 + (ev->value & 0xFF));
                    if (d != 0) {
                        time = abs(diff * time / d);
                        if (time <= 0) {
                            time = 1;
                        }
                    } else {
                        time = 1;
                    }
                }
                if (ev == NULL) {
                    ev = NewSoundEvent(0, block, snd);
                }
                if (time > abs(diff)) {
                    ev->unk08 = abs(time / diff);
                    ev->unk0E = 1;
                    ev->unk0C = ev->unk08;
                    if (note >= sounds[snd].tone) {
                        ev->step = 1;
                    } else {
                        ev->step = -1;
                    }
                } else {
                    ev->step = diff / time;
                    ev->unk0E = 0;
                }
                ev->value = sounds[snd].note | (note << 8);
            }
            break;
        case SND_TYPE_SEQ:
            break;
        }
    }
}

void SetSoundNoteByID(s32 id, s16 snd, s16 note, s16 a3, s16 arg4) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        SetSoundNote(block, snd, note, a3, arg4);
    }
}

void SetSoundNoteByNum(u32 num, s16 snd, s16 note, s16 a3, s16 arg4) {
    if (num < 16 && gVabBlock[num] != NULL) {
        SetSoundNote(gVabBlock[num], snd, note, a3, arg4);
    }
}

s16 GetSoundNote(SoundBlock *block, s16 snd, s16 which) {
    SoundEntry *s;

    if (snd == -1) {
        return -1;
    }
    if ((u8)block->numSounds - 1 >= snd) {
        s = block->sounds;
        if ((s[snd].flags & 0x1F) == SND_TYPE_GROUP) {
            snd++;
            which = -1;
        }
        if (which != -2) {
            if (which == -1) {
                return s[snd].seq;
            }
            return 0;
        }
        return s[snd].tone;
    }
    return 0;
}

s16 GetSoundNoteByID(s32 id, s16 snd, s16 which) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block == NULL) {
        return 0;
    }
    return GetSoundNote(block, snd, which);
}

s16 GetSoundNoteByNum(u32 num, s16 snd, s16 which) {
    if (num < 16 && gVabBlock[num] != NULL) {
        return GetSoundNote(gVabBlock[num], snd, which);
    }
    return -1;
}

void StopSound(SoundBlock *block, s16 snd) {
    SoundEntry *sounds;

    if (snd != -1 && (u8)block->numSounds - 1 >= snd) {
        sounds = block->sounds;
        switch (sounds[snd].flags & 0x1F) {
        case SND_TYPE_ONESHOT:
            break;
        case SND_TYPE_LOOP:
            if (sounds[snd].numVoices != -1) {
                SsVoKeyOff((sounds[snd].vab << 8) | sounds[snd].prog, (sounds[snd].playTone << 8) | sounds[snd].note);
                sounds[snd].numVoices = -1;
                KillAllEventsForSound(block, snd);
            }
            break;
        case SND_TYPE_SEQ:
            if (sounds[snd].sepNum == -1) {
                SsSeqStop(sounds[snd].seq);
            } else {
                SsSepStop(sounds[snd].seq, sounds[snd].sepNum);
            }
            break;
        }
    }
}

void StopSoundByID(s32 id, s16 snd) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        StopSound(block, snd);
    }
}

void StopSoundByNum(u32 num, s16 snd) {
    if (num < 16 && gVabBlock[num] != NULL) {
        StopSound(gVabBlock[num], snd);
    }
}

void StopAllSoundsInBlock(SoundBlock *block) {
    s16 i;
    s32 n = (u8)block->numSounds;

    for (i = 0; i < n; i++) {
        StopSound(block, i);
    }
}

void StopAllSoundsInBlockByID(s32 id) {
    SoundBlock *block = FindSoundBlockByID(id);

    if (block != NULL) {
        StopAllSoundsInBlock(block);
    }
}

void StopAllSoundsInBlockByNum(s32 num) {
    if (gVabBlock[num] != NULL) {
        StopAllSoundsInBlock(gVabBlock[num]);
    }
}

void ReverbOn(void) {
    gReverbOn = 1;
    gReverbWaiting = 0;
    SsUtSetReverbDepth(gRevDepth, gRevDepth);
}

void ReverbOff(void) {
    gReverbOn = 0;
    gReverbWaiting = 0;
    SsUtSetReverbDepth(0, 0);
}

void ReverbSet(s16 type, s16 depth) {
    if (type != -1) {
        if (type != gRevType) {
            SsUtReverbOff();
            SsUtSetReverbDepth(0, 0);
            SsUtSetReverbType(type);
            SpuClearReverbWorkArea(type);
            gReverbWaiting = 1;
            SsUtReverbOn();
            gRevType = type;
        }
        gRevDepth = depth;
        if (gReverbWaiting == 0) {
            ReverbOn();
        }
    } else {
        ReverbOff();
    }
}

void PrepareMovieSound(void) {
    RemoveSoundEvent(FindSoundEvent(0xB, NULL, 100));
    SsSetSerialAttr(0, 0, 1);
    SsSetSerialVol(0, 0x7F, 0x7F);
    gCurrentMovieVol = 0x7F;
}

void FadeDownMovieSound(s32 rate) {
    SoundEvent *ev = FindSoundEvent(0xB, NULL, 100);
    s32 min;
    s32 step;

    if (rate == 0) {
        RemoveSoundEvent(ev);
        SsSetSerialVol(0, 0, 0);
        return;
    }
    min = -0x7F;
    if (ev == NULL) {
        ev = NewSoundEvent(0xB, NULL, 100);
    }
    if (rate > -min) {
        step = rate / min;
        ev->unk0E = 1;
        ev->unk08 = abs(step);
        ev->step = -1;
        ev->unk0C = ev->unk08;
    } else {
        ev->unk0E = 0;
        ev->step = min / rate;
    }
    ev->value = 0;
}

SoundBlock *FindSoundBlockByID(s32 id) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL && gVabBlock[i]->id == id) {
            return gVabBlock[i];
        }
    }
    return NULL;
}

SoundBlock *FindSoundBlockByNum(s32 num) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL && gVabBlock[i]->vabId == num) {
            return gVabBlock[i];
        }
    }
    return NULL;
}

s32 NextFreeVAB(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] == NULL) {
            return i;
        }
    }
    return -1;
}

MIDIDesc *FindMIDIDesc(s32 id) {
    MIDIDesc **p = gMIDIDesc;

    while (*p != NULL) {
        if ((*p)->id == id) {
            return *p;
        }
        p++;
    }
    return NULL;
}

s32 NextFreeMIDI(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (gMIDIDesc[i] == NULL) {
            return i;
        }
    }
    return -1;
}

s32 FindMIDIByID(s16 id) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (gMIDIDesc[i]->seq == id) {
            return i;
        }
    }
    return -1;
}

s32 NextFreeXA(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (gXABlock[i] == NULL) {
            return i;
        }
    }
    return -1;
}

XABlock *FindXAByID(s32 id) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (gXABlock[i]->id == id) {
            return gXABlock[i];
        }
    }
    return NULL;
}

void SetLoopingVolume(SoundEntry *s, s16 vol, s16 pan) {
    s16 voice;
    s16 left, right;
    s16 i;

    for (i = 0; i < s->numVoices; i++) {
        voice = GetVoiceFromField(s->voices, i);
        GetVoiceVolume(gVabBlock[s->vab], s->prog, s->firstTone + i, vol, pan, &left, &right);
        SsUtSetDetVVol(voice, left, right);
    }
}

#ifdef NON_MATCHING
/* regalloc/scheduling: param copies land in different temps (t4/t5 vs t3/a2) */
void GetVoiceVolume(SoundBlock *block, s16 prog, s16 tone, s16 vol, s16 pan, s16 *left, s16 *right) {
    VabHdr *hdr = (VabHdr *)block;
    s16 level;
    s16 pos;
    ProgAtr *progs;
    VagAtr *tones;
    u32 l, r;

    if (pan < vol) {
        level = vol;
        if (vol == 0) {
            vol = 1;
        }
        pos = (pan << 6) / vol;
    } else {
        level = pan;
        if (pan == 0) {
            pan = 1;
        }
        pos = 0x7F - vol * 63 / pan;
    }
    progs = (ProgAtr *)(hdr + 1);
    r = (u32)(hdr->mvol * 129 * progs[prog].mvol) / 127;
    tones = (VagAtr *)((u8 *)progs + (progs[prog].reserved1 * 0x200 + 0x800));
    r = (u32)(r * tones[tone].vol) / 127;
    r = (u32)(r * level) / 127;
    if (hdr->pan <= 64) {
        l = r;
        r = r * hdr->pan >> 6;
    } else {
        l = r * (0x7F - hdr->pan) >> 6;
    }
    if (progs[prog].mpan <= 64) {
        r = r * progs[prog].mpan >> 6;
    } else {
        l = l * (0x7F - progs[prog].mpan) >> 6;
    }
    if (tones[tone].pan <= 64) {
        r = r * tones[tone].pan >> 6;
    } else {
        l = l * (0x7F - tones[tone].pan) >> 6;
    }
    if (pos <= 64) {
        r = r * pos >> 6;
    } else {
        l = l * (0x7F - pos) >> 6;
    }
    *left = l;
    *right = r;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/snddrv", GetVoiceVolume);
#endif

s16 GetVoiceFromField(s32 field, s16 n) {
    s32 count = -1;
    s32 i;

    for (i = 0; i < 24; i++) {
        if ((field >> i) & 1) {
            count++;
            if (count == n) {
                return i;
            }
        }
    }
    return -1;
}

void GetVolumesFromPan(s16 vol, s16 pan, s16 *left, s16 *right, s16 which) {
    s16 v = vol * D_801101D0[which] / 127;
    s32 rear;
    s16 p;
    s16 lr;

    if (v == 0) {
        *left = *right = 0;
        return;
    }
    p = pan;
    rear = 0;
    if (pan > 90 && pan < 270) {
        rear = 90 - abs(pan - 180);
        pan = 180 - pan;
        p = pan;
        if (pan < 0) {
            p = pan + 360;
        }
    }
    if (p > 90) {
        p -= 360;
    }
    lr = (p + 91) * 127 / 180;
    if (gStereoMode == 0) {
        if (rear != 0) {
            *left = *right = v * 95 / 100;
        } else {
            *left = *right = v;
        }
    } else {
        if (lr == 64) {
            *left = v;
            *right = v;
        } else if (lr < 64) {
            *left = v;
            *right = v * lr / 64;
        } else {
            *left = v * (127 - lr) / 64;
            *right = v;
        }
        if (rear != 0) {
            *left -= *left / 4 * rear / 90;
            *right -= *right / 4 * rear / 90;
        }
    }
    if (*left == 0) {
        *left = 1;
    }
    if (*right == 0) {
        *right = 1;
    }
}

s16 GetPanChange(s16 flags, s16 from, s16 to) {
    s16 d;

    if (flags < 0) {
        s32 t = to;
        s32 f = from;
        if (t > f) {
            d = t - (s16)(f + 360); /* angles wrap as s16 */
        } else {
            d = t - f;
        }
    } else if (to < from) {
        d = (s16)(to + 360) - from;
    } else {
        d = to - from;
    }
    return d;
}

#ifdef NON_MATCHING
/* target computes (s16)(pan+add) into v0 then returns 1; the dead sum is optimized away here (permuter best 5 needs an artificial use) */
s16 AddPan(s16 pan, s16 add) {
    pan += add;
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/snddrv", AddPan);
#endif

s16 GetMasterVolume(s32 which) {
    return D_801101D0[which];
}

void SetMasterVolume(s32 which, s16 vol) {
    s16 left, right;
    s16 xaVol;
    s32 i;
    s16 j;

    D_801101D0[which] = vol;
    if (vol >= 0x80) {
        D_801101D0[which] = 0x7F;
    }
    if (gPlayingMIDIUnderscore != NULL && which == gPlayingMIDIUnderscore[gUnderscoreID].unk12) {
        AdjustRunningMusicVol();
    }
    if (which == 2) {
        xaVol = gLastXAVol * 60 / 100;
        if (xaVol >= 0x80) {
            xaVol = 0x7F;
        }
        GetVolumesFromPan(xaVol, gLastXAPan, &left, &right, which);
        SsSetSerialVol(0, left, right);
    }
    for (i = 0; i < 16; i++) {
        if (gVabBlock[i] != NULL) {
            s16 n = (u8)gVabBlock[i]->numSounds;
            SoundEntry *sounds = gVabBlock[i]->sounds;

            for (j = 0; j < n; j++) {
                s32 type = sounds[j].flags & 0x1F;
                if ((type == SND_TYPE_LOOP || type == 3) && sounds[j].numVoices != -1) {
                    SetSoundVol(gVabBlock[i], j, sounds[j].vol, 0, 2);
                }
            }
        }
    }
}

void SetStereoMode(s16 mode) {
    gStereoMode = mode;
    if (mode == 1) {
        SsSetStereo();
    } else {
        SsSetMono();
    }
}

s16 GetStereoMode(void) {
    return gStereoMode;
}

void DoSoundTick(void) {
    SoundEvent *ev;
    SoundEntry *sounds;
    s16 left, right;
    s32 tone;
    s32 note;
    s32 target;
    s16 v;
    s16 i;
    void (*cb)();
    s32 frame;
    s32 pos;

    ev = gSEvent;
    if (ev == NULL) {
        goto done;
    }
loop:
    {
        if (ev->unk0E == 1) {
            ev->unk0C--;
            if ((s16)ev->unk0C != -1) {
                goto done;
            }
            if (ev->unk0E == 1) {
                ev->unk0C = ev->unk08;
            }
        }
        sounds = ev->block->sounds;
        switch (ev->kind) {
        case 0:
            if (sounds[ev->id].numVoices != -1) {
                tone = sounds[ev->id].tone;
                note = sounds[ev->id].note + ev->step;
                target = (s16)ev->value >> 8;
                if (tone == target) {
                    if ((ev->step < 0 && note < (ev->value & 0xFF)) || (ev->step > 0 && note > (ev->value & 0xFF))) {
                        note = (u8)ev->value;
                    }
                } else {
                    if (note >= 0x80) {
                        tone++;
                        note = 0;
                    }
                    if (note < 0) {
                        tone--;
                        note = (tone != target) ? 0x7F : 0;
                    }
                }
                for (i = 0; i < sounds[ev->id].numVoices; i++) {
                    SsUtChangePitch(GetVoiceFromField(sounds[ev->id].voices, i), sounds[ev->id].vab, sounds[ev->id].prog,
                                    sounds[ev->id].playTone, 0, (s16)tone, (s16)note);
                }
                sounds[ev->id].tone = tone;
                sounds[ev->id].note = note;
            } else {
                sounds[ev->id].tone = (u16)ev->value >> 8;
                sounds[ev->id].note = ev->value;
            }
            if (sounds[ev->id].tone == ((s16)ev->value >> 8) && sounds[ev->id].note == (ev->value & 0xFF)) {
                goto remove;
            }
            goto next;
        case 9:
            StopSound(ev->block, ev->id);
            goto remove;
        case 2:
            sounds[ev->id].pan = AddPan(sounds[ev->id].pan, ev->step);
            ev->unk12 += ev->step;
            if ((ev->step > 0 && ev->unk12 > 0) || (ev->step < 0 && ev->unk12 < 0)) {
                sounds[ev->id].pan = ev->value;
            }
            GetVolumesFromPan(sounds[ev->id].vol, sounds[ev->id].pan, &left, &right, sounds[ev->id].unk12);
            if (sounds[ev->id].sepNum == -1) {
                SsSeqSetVol(sounds[ev->id].seq, left, right);
            } else {
                SsSepSetVol(sounds[ev->id].seq, sounds[ev->id].sepNum, left, right);
            }
        pancheck:
            if (sounds[ev->id].pan == ev->value) {
                goto remove;
            }
            goto next;
        case 3:
            if (sounds[ev->id].numVoices != -1) {
                sounds[ev->id].pan = AddPan(sounds[ev->id].pan, ev->step);
                ev->unk12 += ev->step;
                if ((ev->step > 0 && ev->unk12 > 0) || (ev->step < 0 && ev->unk12 < 0)) {
                    sounds[ev->id].pan = ev->value;
                }
                GetVolumesFromPan(sounds[ev->id].vol, sounds[ev->id].pan, &left, &right, sounds[ev->id].unk12);
                SetLoopingVolume(sounds + ev->id, left, right);
            } else {
                sounds[ev->id].pan = ev->value;
            }
            goto pancheck;
        case 4:
            if (sounds[ev->id].numVoices != -1) {
                v = sounds[ev->id].vol + ev->step;
                if ((ev->step > 0 && v > ev->value) || (ev->step < 0 && v < ev->value)) {
                    v = ev->value;
                }
                GetVolumesFromPan(v, sounds[ev->id].pan, &left, &right, sounds[ev->id].unk12);
                sounds[ev->id].vol = v;
                SetLoopingVolume(sounds + ev->id, left, right);
            } else {
                sounds[ev->id].vol = ev->value;
            }
            if (sounds[ev->id].vol != ev->value) {
                goto next;
            }
            if (ev->unk14 == 1) {
                StopSound(ev->block, ev->id);
            }
            goto remove;
        case 10:
            if (gPlayingMIDIUnderscore != NULL) {
                v = sounds[ev->id].vol + ev->step;
                if ((ev->step > 0 && v > ev->value) || (ev->step < 0 && v < ev->value)) {
                    v = ev->value;
                }
                sounds[ev->id].vol = v;
                AdjustRunningMusicVol();
            } else {
                sounds[ev->id].vol = ev->value;
            }
            if (sounds[ev->id].vol != ev->value) {
                goto next;
            }
            if (ev->unk14 == 1) {
                StopMusicUnderscore();
            }
            goto remove;
        case 11:
            v = gCurrentMovieVol + ev->step;
            if ((ev->step > 0 && v > ev->value) || (ev->step < 0 && v < ev->value)) {
                v = ev->value;
            }
            gCurrentMovieVol = v;
            SsSetSerialVol(0, v, v);
            if (v != ev->value) {
                goto next;
            }
        remove:
            ev = RemoveSoundEvent(ev);
            break;
        case 1:
        next:
            ev = ev->next;
            break;
        }
        if (ev != NULL) {
            goto loop;
        }
    }
done:
    if (gXferingVAB != -1 && SoundRegisterVABComplete() != 0 && gVABDoneCallback != NULL) {
        gVABDoneCallback(gVABDoneCallbackData_1, gVABDoneCallbackData_2);
        gVABDoneCallback = NULL;
    }
    if (gUserXACallback != NULL) {
        cb = gUserXACallback;
        frame = gUserXACallbackFrame;
        if (frame == -1) {
            if (!PlayingXA()) {
                goto fire;
            }
            if (gUserXACallbackFrame == frame) {
                return;
            }
        }
        if (PlayingXA()) {
            pos = GetXAPos();
            if (pos >= gUserXACallbackFrame) {
                gUserXACallback = NULL;
                cb(gUserXACallbackFrame, pos, gUserXACallbackData);
            }
        } else {
        fire:
            gUserXACallback = NULL;
            cb(gUserXACallbackFrame, -1, gUserXACallbackData);
        }
    }
}

SoundEvent *NewSoundEvent(s16 kind, SoundBlock *block, s16 id) {
    SoundEvent *ev;

    if (gSEvent == NULL) {
        gSEvent = &gEventQue[0];
        gEventQue[0].next = NULL;
        gEventQue[0].kind = kind;
        gEventQue[0].id = id;
        gEventQue[0].block = block;
        return &gEventQue[0];
    }
    ev = gSEvent;
    if (ev->id == id && ev->kind == kind && ev->block == block) {
        return ev;
    }
    while (ev->next != NULL) {
        ev = ev->next;
        if (ev != NULL && ev->id == id && ev->kind == kind && ev->block == block) {
            return ev;
        }
    }
    ev->next = GetUnusedEvent();
    if (ev->next != NULL) {
        ev->next->next = NULL;
        ev->next->kind = kind;
        ev->next->id = id;
        ev->next->block = block;
    }
    return ev->next;
}

SoundEvent *FindSoundEvent(s16 kind, SoundBlock *block, s16 id) {
    SoundEvent *ev;

    if (gSEvent != NULL) {
        ev = gSEvent;
        if (ev->id == id && ev->kind == kind && ev->block == block) {
            return ev;
        }
        while (ev->next != NULL) {
            ev = ev->next;
            if (ev != NULL && ev->id == id && ev->kind == kind && ev->block == block) {
                return ev;
            }
        }
    }
    return NULL;
}

SoundEvent *GetUnusedEvent(void) {
    s16 i;

    for (i = 0; i < 8; i++) {
        if (gEventQue[i].id == -1) {
            return &gEventQue[i];
        }
    }
    return NULL;
}

SoundEvent *RemoveSoundEvent(SoundEvent *ev) {
    SoundEvent *p;
    SoundEvent *next;

    if (ev == NULL) {
        return NULL;
    }
    if (gSEvent == ev) {
        ev->id = -1;
        gSEvent = ev->next;
        return gSEvent;
    }
    p = gSEvent;
    while ((next = p->next) != ev) {
        p = next;
    }
    p->next = ev->next;
    ev->id = -1;
    return p->next;
}

void KillAllEventsForSound(SoundBlock *block, s16 snd) {
    SoundEntry *sounds = block->sounds;
    SoundEvent *ev;

    if ((sounds[snd].flags & 0x1F) == SND_TYPE_LOOP) {
        ev = FindSoundEvent(4, block, snd);
        if (ev != NULL) {
            sounds[snd].vol = ev->value;
            RemoveSoundEvent(ev);
        }
        ev = FindSoundEvent(3, block, snd);
        if (ev != NULL) {
            sounds[snd].pan = ev->value;
            RemoveSoundEvent(ev);
        }
        ev = FindSoundEvent(0, block, snd);
        if (ev != NULL) {
            sounds[snd].tone = (u16)ev->value >> 8;
            sounds[snd].note = ev->value;
            RemoveSoundEvent(ev);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/snddrv", SoundSeqCallback);

void SetMIDIRegister(s32 reg, s16 value) {
    if (value >= 0x80) {
        value = 0x7F;
    }
    if (value < 0) {
        value = 0;
    }
    D_801101E0[reg] = value;
}

u8 GetMIDIRegister(s32 reg) {
    return D_801101E0[reg];
}

void SetGlobalExcite(u8 level) {
    if (level >= 0x80) {
        level = 0x7F;
    }
    D_8010FD78 = level;
}

u8 GetGlobalExcite(void) {
    return D_8010FD78;
}

void ClearMIDIMuteStatus(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        D_80107890[i] = 0;
    }
}

void SetMIDIVolPan(s16 vol, s16 pan) {
    D_8010FD7C = vol;
    D_8010FD7E = pan;
}

void ResetMIDICallback(void) {
    SoundSeqCallback(-1, 0, 0);
}

void GetGroupMember(s32 group, s32 i, s32 *a, s32 *b, s32 *c) {
    *a = D_801221E8[group].a[i];
    *b = D_801221E8[group].b[i];
    *c = D_801221E8[group].c[i];
}

void SetGroupMember(s32 group, s32 i, u8 a, u8 b, s32 c) {
    D_801221E8[group].a[i] = a;
    D_801221E8[group].b[i] = b;
    D_801221E8[group].c[i] = c;
}

u8 GetNumGroupMembers(s32 group) {
    return D_801221E8[group].num;
}

void PrepareXA(void) {
    u8 param;
    s32 old;

    if (D_8010FD88 == 0) {
        old = CdReadyCallback((s32)func_800C13B4);
        if (old != (s32)func_800C13B4) {
            oldreadycallback = old;
        }
        param = 0xC8;
        CdControlB(0xE, &param, 0);
        func_800C1380();
        D_8010FD8C = 0;
        D_8010FD90 = 0;
        D_8010FD88 = 1;
    }
}

void UnprepareXA(void) {
    u8 param;

    if (D_8010FD88 != 0) {
        if (D_8010FD8C != 0) {
            StopXA();
        }
        CdReadyCallback(oldreadycallback);
        param = 0x80;
        CdControlF(0xE, &param);
        D_8010FD88 = 0;
    }
}

void PlayXA(s32 pos, s32 chan, s32 force, s32 arg3) {
    u8 param[2];

    if ((D_8010FD8C == 0 && D_8010FD90 == 0) || force != 0) {
        if (D_8010FD8C == 0 && D_8010FD90 == 0) {
            D_801101F4 = 0x40;
            param[0] = 1;
            D_801101F0 = pos;
            D_8010FD84 = 0;
            D_801101F8 = param[1] = chan;
            D_8010FD94 = 0;
            CdControlF(0xD, param);
            func_800C1590(6, D_801101F0);
        } else {
            D_801101EC->pos = pos;
            D_801101EC->chan = chan;
            D_801101EC->arg = arg3;
            D_801101EC++;
            if (D_801101EC > &D_80122528[7]) {
                D_801101EC = D_80122528;
            }
        }
    }
}

void PauseXA(void) {
    if (D_8010FD8C != 0) {
        CdControlF(9, 0);
        D_8010FD90 = 1;
        D_8010FD8C = 0;
    }
}

void ContinueXA(void) {
    if (D_8010FD90 != 0) {
        func_800C1590(6, D_8010FD84);
    }
}

void StopXA(void) {
    D_8010FD90 = 0;
    D_8010FD8C = 0;
    CdControlF(9, 0);
    SsSetSerialVol(0, 0, 0);
    func_800C1380();
}

s32 PlayingXA(void) {
    return D_8010FD8C;
}

s32 GetXAStatus(void) {
    return D_8010FD88;
}

s32 GetXAPos(void) {
    return D_8010FD84;
}

s32 CDPosToLong(CdlLOC *loc) {
    u8 m = loc->minute;
    u8 s = loc->second;
    u8 f = loc->sector;

    return ((((m >> 4) * 10 + (m & 0xF)) * 60 + ((s >> 4) * 10 + (s & 0xF))) * 75 + ((f >> 4) * 10 + (f & 0xF))) - 150;
}

CdlLOC *CDLongToPos(u32 pos, CdlLOC *loc) {
    u32 sec, frame, min;

    pos += 150;
    sec = pos / 75;
    frame = pos % 75;
    min = sec / 60;
    sec = sec % 60;
    loc->sector = ((frame / 10) << 4) + (frame % 10);
    loc->second = ((sec / 10) << 4) + (sec % 10);
    loc->minute = ((min / 10) << 4) + (min % 10);
    return loc;
}

static void func_800C1380(void) {
    s32 i;

    for (i = 7 * sizeof(XAQueue); i >= 0; i -= sizeof(XAQueue)) {
        *(s32 *)((u8 *)D_80122528 + i) = 0;
    }
    D_801101E8 = D_80122528;
    D_801101EC = D_80122528;
}

static void func_800C13B4(s32 status) {
    u8 param[2];

    if (status == 1) {
        if (D_8010FD8C == 0) {
            CdControlF(9, NULL);
            return;
        }
        D_8010FD84 += 0x20;
        if (D_8010FD84 > D_801101F4) {
            if (D_801101E8->pos != 0) {
                if (CanIHaveTheCDPlease(D_801101E8->pos, D_801101E8->chan, D_801101E8->arg) == 0) {
                    CdControlF(9, NULL);
                    D_8010FD8C = 0;
                    func_800C1380();
                    return;
                }
                D_801101F4 = 0x40;
                D_8010FD84 = 0;
                D_8010FD94 = 0;
                D_801101F0 = D_801101E8->pos;
                param[0] = 1;
                D_801101F8 = param[1] = D_801101E8->chan;
                CdControlF(0xD, param);
                func_800C1590(6, D_801101F0);
                D_801101E8->pos = 0;
                D_801101E8++;
                if (D_801101E8 > &D_80122528[7]) {
                    D_801101E8 = D_80122528;
                }
            } else {
                CdControlF(9, NULL);
                D_8010FD8C = 0;
            }
        } else if (D_8010FD94 == 0) {
            D_8010FD94 = status;
            CdGetSector(D_80122588, 0x10);
            D_801101F4 = D_80122588[D_801101F8];
        }
    } else if (status == 5) {
        D_8010FD98++;
        if (D_8010FD98 >= 6) {
            CdControlF(9, NULL);
            D_8010FD8C = 0;
            func_800C1380();
            D_8010FD98 = 0;
        }
    }
}

static void func_800C1590(s32 com, s32 pos) {
    CdlLOC loc;

    CDLongToPos(pos, &loc);
    D_8010FD84 = 0;
    D_8010FD98 = 0;
    CdControlF(com, (u8 *)&loc);
    D_8010FD90 = 0;
    D_8010FD8C = 1;
}
