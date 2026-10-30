#include "common.h"
#include "gameobj.h"
#include "gobj.h"

/* Random XA pick list */
typedef struct XARandTable {
    /* 0x00 */ u32 used; /* bit n: tracks[n] already played */
    /* 0x04 */ s16 tracks[1]; /* -1 terminated */
} XARandTable;

/* sound.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F990)
SDATA_LOCAL(D_8010F994)
SDATA_LOCAL(D_8010F995)
SDATA_LOCAL(D_8010F996)
SDATA_LOCAL(D_8010F997)
SDATA_LOCAL(D_8010F998)
SDATA_LOCAL(D_80110188)
SDATA_LOCAL(D_80110190)
SDATA_LOCAL(D_80110194)
SDATA_LOCAL(D_80110198)
SDATA_LOCAL(D_8011019C)
SDATA_LOCAL(D_801101A0)
SDATA_LOCAL(D_801101A4)
SDATA_LOCAL(mXABlock)

extern s32 D_8010F990;
extern s8 D_8010F994[2]; /* master volumes */
extern s8 D_8010F995;
extern u8 D_8010F996;
extern u8 D_8010F997; /* stereo flag */
extern u8 D_8010F998; /* sound system initialised */
extern s32 *D_80110188; /* loaded sound data: [0] = offset (words) of the MIDI data */
extern s32 *D_80110190; /* MIDI data */
extern u8 D_80110194;  /* shooting excite active */
extern u8 D_80110198;  /* chase excite level */
extern s32 D_8011019C; /* periodic song */
extern s32 D_801101A0;
extern u8 D_801101A4;
extern s32 mXABlock;

extern s32 D_8011EB08[6]; /* sound block handles (slot 3: music) */
extern s32 D_8011EB30;    /* XA state, -3: queued */
extern s16 D_8011EB34;
extern s16 D_8011EB36;
extern s16 D_8011EB38; /* queued XA */
extern u8 gUsingSndBigFire;
extern u8 gSoundFootstep[26];
extern s16 getNPCIndex;
extern s32 npcList[6];
extern s8 mExciteLevelModTable[];
extern s8 D_80106AD0; /* excite: distance divisor */
extern s8 D_80106AD1; /* excite: no threat */
extern s8 D_80106AD2; /* excite: threat 1 */
extern s8 D_80106AD3; /* excite: low threat */
extern s8 D_80106AD4; /* excite: high threat */
extern s8 D_80106AD5; /* excite: full threat */
extern s8 D_80106AD6; /* excite: shooting bonus */

typedef struct SoundTarget {
    /* 0x00 */ u8 unk00[0xD4];
    /* 0x D4 */ s32 threat;
} SoundTarget;
typedef struct SoundSysInfo {
    s16 level;
} SoundSysInfo;
extern SoundSysInfo gSysInfo;
extern s32 xaLevelStartIndex[];
extern s32 gPlayerOffset;

typedef struct CdlLOC {
    u8 minute, second, sector, track;
} CdlLOC;
typedef struct CdlFILE {
    CdlLOC pos;
    u32 size;
    char name[16];
} CdlFILE;

s32 EGSFileSearchCD(CdlFILE *file, char *name);
s32 CDPosToLong(CdlFILE *file);
int printf(const char *fmt, ...);
void SetStereoMode(s32 flag);
void SetMasterVolume(s32 which, s16 vol);
void ReverbOff(void);
void ReleaseMIDI(s32 *midi);
void ReleaseXA(s32 block);
void ReleaseOneSoundBlock(s32 block);
void StopMusicUnderscore(void);
void StopSound(s32 block, s16 id);
void StopAllSoundsInBlock(s32 block);
void PauseMusicUnderscore(u8 pause);
void PauseAllLoops(u8 pause);
s32 PlayingXA(void);
void StopXA(void);
void UnprepareXA(void);
void UnpauseTerrainStream(void);
void SetGlobalExcite(u8 level);
u8 GetGlobalExcite(void);
void SetMIDIRegister(s32 reg, s32 value);
void EGSDisplayQueueInterrupt(void (*fn)(), s32 arg1, s32 arg2);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, s32 arg);
void iSndXACB(void);
void iSndExciteShootCB(void);
static void func_8006B6A0(void);
void SysLoadData(char *name, void **out, s32 arg2);
void *EGSMemGetHeap(void);
void EGSMemSetHeap(void *heap);
s32 SoundRegisterVAB(void *vh, void *vb);
s16 SoundRegisterVABComplete(void);
void SetupAllLoaded(s32 arg0, s32 arg1);
void EGSDebugAssert(int, int, char *file, int line);
void EGSHogOpen(s32 name, s32 arg1, void ***hog);
s32 EGSHogGetBlockCount(void **hog);
void EGSHogGetBlock(void **hog, s32 index, void **out);
void EGSHogGetBlockName(void **hog, s32 index, char **out);
void EGSHogGetBlockByName(s32 hog, char *name, void **out);
char *strcpy(char *dst, const char *src);
int strlen(const char *s);
int strcmp(const char *a, const char *b);
s32 SoundRegisterXA(void *xh);
void SoundRegisterMIDI(s32 *midi);
void EGSMemFree(void *p);
void ReverbOn(void);
void SoundSetupFootsteps(void);
void SysSetMusicExciteLevel(u8 level);

/* The bank file names; the .VB name is copied over the .VH one in place. */
typedef struct SoundFileName {
    char s[20];
} SoundFileName;
/*
 * These strings sit before SoundSetupFootsteps' jump table in .rodata, so they stay
 * INCLUDE_RODATA (and __FILE__ is not used for the assert) to keep that order.
 */
extern char D_80012224[]; /* "s/sound.c" */
extern char D_80012230[]; /* "MUSIC.VB" */
extern char D_8001223C[]; /* "INGAME.XH" */
extern char D_8010F99C[]; /* sound bank names (.sdata) */
extern char D_8010F9A4[];
extern char D_8010F9AC[];
extern char D_80012248[]; /* "\COMMON\BEEPSX.VH;1" */
extern char D_8001225C[]; /* "\COMMON\BEEPSX.VB;1" */
void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);
void SysPlayXA(s32 track, GameObjectRuntime *src, s32 arg2);
void SysSoundPlayRandXA(XARandTable *table);
u8 StreamLock(void);
s32 PlayXADirect(s32 block, s16 track, s16 vol, s16 pan, s32 arg4, s32 arg5);
s32 GetSoundNote(s32 block, s16 id, s32 arg2);
void SetSoundNote(s32 block, s16 id, s16 note, s32 arg3, s32 arg4);
int rand(void);
s32 GetSoundVol(s32 block, s16 id, s32 arg2);
void SetSoundPan(s32 block, s16 id, s16 pan, s32 arg3, s32 arg4);
void SetSoundVol(s32 block, s16 id, s16 vol, s32 arg3, s32 arg4);
s32 PlaySoundVolPan(s32 block, s16 id, s16 vol, s16 pan);
void PlayMusicUnderscoreVolPan(s32 block, s16 id, s32 vol, s32 pan);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void EGSFrameTransformPoint(VECTOR32 *in, s32 arg1, void *frame, VECTOR32 *out);
int ratan2(int y, int x);
void iSndCalcVolPan(s32 type, s32 song, GameObjectRuntime *src, VECTOR32 *pos, s16 *volOut, s16 *panOut);

extern XARandTable D_80106A88; /* random XA table for SoundPlayXACASAnimCB */

typedef struct SoundCamera {
    void **display; /* *display: the camera frame */
} SoundCamera;
extern SoundCamera *gCamera;

typedef struct SoundFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 t[3];
} SoundFrame;

typedef struct SoundAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ SoundFrame *frame;
} SoundAnim;

typedef struct SoundNPC {
    /* 0x00 */ u8 unk00[0x51];
    /* 0x51 */ u8 unk51;
} SoundNPC;

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_800121E4);

s32 WhereIsThisFilePlease(char *name) {
    CdlFILE file;

    s32 pos;

    if (EGSFileSearchCD(&file, name)) {
        pos = CDPosToLong(&file);
    } else {
        printf("WhereIsThisFilePlease: Error finding file ->%s\n", name);
        pos = 0;
    }
    return pos;
}

s32 CanIHaveTheCDPlease(void) {
    return 1;
}

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_80012224);

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_80012230);

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_8001223C);

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_80012248);

INCLUDE_RODATA("asm/main/nonmatchings/sound", D_8001225C);

/* Footstep sound per surface material (26 entries) for the current level */
#define FOOTSTEPS_FILL(sound)               \
    for (i = 0; i < 26; i++) {              \
        gSoundFootstep[i] = (sound);        \
    }

void SoundSetupFootsteps(void) {
    s32 i;

    switch (gSysInfo.level) {
    case 0:
    case 1:
    case 2:
    case 19:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 3:
        FOOTSTEPS_FILL(0x34);
        gSoundFootstep[12] = 0x34;
        gSoundFootstep[20] = 0x28;
        gSoundFootstep[11] = 0x2B;
        gSoundFootstep[14] = 0x2B;
        break;
    case 4:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 5:
    case 6:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 7:
    case 8:
    case 9:
    case 10:
        FOOTSTEPS_FILL(0x3A);
        break;
    case 11:
    case 12:
    case 13:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 14:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 15:
        FOOTSTEPS_FILL(0x2B);
        break;
    case 16:
    case 17:
        FOOTSTEPS_FILL(0x28);
        break;
    case 18:
        FOOTSTEPS_FILL(0x2E);
        break;
    }
}

void SysInitSound(s32 name, s32 arg1) {
    char buf[32];
    void **hog;
    void *vb;
    char *blockName;
    void *vh;
    void *xh;
    s32 count;
    s32 i;
    s32 bank = 0;

    if (name == 0) {
        return;
    }
    EGSHogOpen(name, arg1, &hog);
    D_80110188 = (s32 *)*hog + *(s32 *)*hog;
    count = EGSHogGetBlockCount(hog);
    if (count >= 10) {
        EGSDebugAssert(1, 0, D_80012224, 359);
    }
    for (i = 0; i < count; i++) {
        EGSHogGetBlock(hog, i, &vb);
        EGSHogGetBlockName(hog, i, &blockName);
        strcpy(buf, blockName);
        buf[(u8)strlen(buf) - 1] = 'H';
        EGSHogGetBlockByName(D_8010F990, buf, &vh);
        if (!strcmp(blockName, D_8010F99C)) {
            bank = 1;
        } else if (!strcmp(blockName, D_8010F9A4)) {
            bank = 0;
        } else if (!strcmp(blockName, D_8010F9AC)) {
            bank = 2;
        } else if (!strcmp(blockName, D_80012230)) {
            bank = 3;
        }
        D_8011EB08[bank] = SoundRegisterVAB(vh, vb);
        while (!SoundRegisterVABComplete()) {
        }
    }
    EGSHogGetBlockByName(D_8010F990, D_8001223C, &xh);
    mXABlock = SoundRegisterXA(xh);
    EGSMemFree(D_80110188);
    if (*D_80110188 != 0) {
        D_80110190 = D_80110188 + *D_80110188;
        SoundRegisterMIDI(D_80110190);
    } else {
        D_80110190 = NULL;
    }
    SetupAllLoaded(3, 30);
    ReverbOn();
    D_80110194 = 0;
    D_80110198 = 0;
    D_8010F998 = 1;
    gUsingSndBigFire = 0;
    SoundSetupFootsteps();
    SetMasterVolume(0, D_8010F994[0]);
    SetMasterVolume(1, D_8010F995);
    SetMasterVolume(2, (s8)D_8010F996);
    SetStereoMode(D_8010F997);
    SysSetMusicExciteLevel(1);
}

void SysInitMenuSoundBank(void) {
    void *vh;
    void *vb;
    void *heap;

    SysLoadData(D_80012248, &vh, 0);
    heap = EGSMemGetHeap();
    *(SoundFileName *)D_80012248 = *(SoundFileName *)D_8001225C;
    SysLoadData(D_80012248, &vb, 0);
    if (vb != NULL && vh != NULL) {
        D_8011EB08[5] = SoundRegisterVAB(vh, vb);
        while (!SoundRegisterVABComplete()) {
        }
    }
    SetupAllLoaded(3, 30);
    EGSMemSetHeap(heap);
}

void SysSoundSetStereoFlag(s32 flag) {
    SetStereoMode(flag);
    D_8010F997 = flag;
}

u8 SysSoundGetStereoFlag(void) {
    return D_8010F997;
}

void SysSoundSetMasterVolume(s32 which, s32 vol) {
    D_8010F994[which] = vol;
    SetMasterVolume(which, vol);
}

s8 SysSoundGetMasterVolume(s32 which) {
    return D_8010F994[which];
}

void SysStopSound(void) {
    s32 i;

    if (D_8010F998) {
        ReverbOff();
        if (D_80110190) {
            ReleaseMIDI(D_80110190);
        }
        ReleaseXA(mXABlock);
        for (i = 0; i < 4; i++) {
            ReleaseOneSoundBlock(D_8011EB08[i]);
        }
        D_8010F998 = 0;
    }
}

void iSndCalcVolPan(s32 type, s32 song, GameObjectRuntime *src, VECTOR32 *pos, s16 *volOut, s16 *panOut) {
    VECTOR32 eye;
    VECTOR32 at;
    VECTOR32 local;
    s32 dist;
    s16 vol;
    s16 pan;
    s16 base;
    s32 angle;
    SoundFrame *frame;

    if ((src != NULL && src->id != gPlayerOffset) || pos != NULL) {
        if (pos != NULL) {
            at.vx = pos->vx;
            at.vy = pos->vy;
            at.vz = pos->vz;
        } else {
            frame = ((SoundAnim *)src->anim)->frame;
            at.vx = frame->t[0];
            at.vy = frame->t[1];
            at.vz = frame->t[2];
            at.vy = -at.vy;
        }
        eye.vx = ((SoundFrame *)*gCamera->display)->t[0];
        eye.vy = ((SoundFrame *)*gCamera->display)->t[1];
        eye.vz = ((SoundFrame *)*gCamera->display)->t[2];
        eye.vy = -eye.vy;
        EGSGeometry_GetFuzzyDistancePoint2Point(&eye, &at, &dist);
        if (type == 4) {
            base = 0x7F;
        } else {
            base = GetSoundVol(D_8011EB08[type], song, -1);
        }
        dist -= 0x280;
        if (dist < 0) {
            dist = 0;
        }
        vol = base - base * dist / 7040;
        if (vol < 0) {
            vol = 0;
        }
        EGSFrameTransformPoint(&at, 0, *gCamera->display, &local);
        angle = ratan2(local.vx, local.vz) * 360;
        pan = angle < 0 ? -(-angle >> 12) : angle >> 12;
        if (pan < 0) {
            pan += 360;
        }
    } else if (type == 4) {
        vol = (s8)D_8010F996;
        pan = 0;
    } else {
        vol = -1;
        pan = -1;
    }
    *volOut = vol;
    *panOut = pan;
}

s16 SysPlaySong(s32 type, s32 song, s32 arg2, s32 arg3) {
    s32 handle = -1;
    s16 vol;
    s16 pan;

    if (type == 3) {
        PlayMusicUnderscoreVolPan(D_8011EB08[3], song, -2, -2);
    } else {
        iSndCalcVolPan(type, song, (GameObjectRuntime *)arg2, (VECTOR32 *)arg3, &vol, &pan);
        if (type == 5 && song == 3) {
            vol = 20;
        }
        handle = PlaySoundVolPan(D_8011EB08[type], song, vol, pan);
    }
    return handle;
}

void SysStopSong(s32 type, s32 song) {
    if (type == 3) {
        StopMusicUnderscore();
    } else {
        StopSound(D_8011EB08[type], song);
    }
    if (song == 7) {
        gUsingSndBigFire = 0;
    }
}

void SysStopAllSongs(void) {
    s32 i;

    if (D_8010F998) {
        for (i = 0; i < 6; i++) {
            if (i == 3) {
                StopMusicUnderscore();
            } else {
                StopAllSoundsInBlock(D_8011EB08[i]);
            }
        }
        gUsingSndBigFire = 0;
    }
}

void SysSetSongVolPan(s32 type, s32 song, GameObjectRuntime *src, s32 newVol, s32 newPan) {
    s16 vol;
    s16 pan;

    iSndCalcVolPan(type, song, src, NULL, &vol, &pan);
    if (newVol != -1) {
        vol = newVol;
    }
    if (pan != -1) {
        pan = newPan;
    }
    SetSoundPan(D_8011EB08[type], song, pan, 0, 0);
    SetSoundVol(D_8011EB08[type], song, vol, 0, 0);
}

void SysPauseSound(u8 pause) {
    PauseMusicUnderscore(pause);
    PauseAllLoops(pause);
}

u8 SysSoundIsXAPlaying(void) {
    return PlayingXA();
}

void SndSysCheckXADoneCallback(void (*done)(void)) {
    if (SysSoundIsXAPlaying()) {
        SysExeTickQueueTimer(SndSysCheckXADoneCallback, 1, (s32)done);
    } else if (done != NULL) {
        done();
    }
}

#ifdef NON_MATCHING
/* The original keeps &D_8011EB30 in $s1 (one lui/addiu, then 0($s1) accesses); cc1 here
 * folds the pointer back into direct %hi/%lo accesses. */
void iSndXACB(void) {
    s32 *state = &D_8011EB30;
    GameObjectRuntime *src;

    if (*state == -1) {
        if (!PlayingXA()) {
            if (D_8011EB34 != -1 && D_8011EB36 != -1) {
                SysPostMessage(D_8011EB34, 4, 0xFFFF, D_8011EB36, 0, 0, 0, 0);
            }
            UnprepareXA();
            UnpauseTerrainStream();
            D_8011EB34 = -1;
            D_8011EB36 = -1;
            D_8011EB38 = -1;
        } else {
            *state = -1;
            EGSDisplayQueueInterrupt(iSndXACB, 1, 0xE0C00000);
        }
    } else if (*state == -2) {
        src = NULL;
        if (D_8011EB36 != -1) {
            src = gameObjectsInfo[D_8011EB36].runtime;
        }
        SysPlayXA(D_8011EB38, src, 1);
    } else if (*state == -3) {
        *state = -1;
        D_8011EB34 = -1;
        D_8011EB36 = -1;
        D_8011EB38 = -1;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/sound", iSndXACB);
#endif

void SysPlayXA(s32 track, GameObjectRuntime *src, s32 queue) {
    s16 vol;
    s16 pan;

    iSndCalcVolPan(4, -1, src, NULL, &vol, &pan);
    D_8011EB38 = track;
    if (StreamLock() || (PlayingXA() && queue)) {
        if (PlayXADirect(mXABlock, track, vol, pan, 1, 1)) {
            D_8011EB30 = -1;
        }
    } else if (queue) {
        D_8011EB30 = -2;
        D_8011EB36 = src != NULL ? src->id : -1;
    }
    EGSDisplayQueueInterrupt(iSndXACB, 5, 0xE0C00000);
}

void SysStopXA(void) {
    StopXA();
    UnprepareXA();
    UnpauseTerrainStream();
    D_8011EB34 = -1;
    D_8011EB36 = -1;
}

void SysSoundQueueXA(s32 level, s32 index) {
    if (level == -1) {
        D_8011EB38 = index;
    } else {
        D_8011EB38 = xaLevelStartIndex[level] + index - 1;
    }
    D_8011EB30 = -3;
    D_8011EB34 = -1;
    D_8011EB36 = -1;
    EGSDisplayQueueInterrupt(iSndXACB, 100, 0);
}

#ifdef NON_MATCHING
/* Scheduling/register allocation only: the original keeps msg in $a2 and loads D_8011EB38
 * before the D_8011EB34/36 stores. */
s32 SysSoundPlayQueuedXA(s32 msg, s32 id) {
    if (D_8011EB30 != -3) {
        if (msg != -1) {
            SysPostMessage(msg, 4, 0xFFFF, id, 0, 0, 0, 0);
        }
        D_8011EB34 = -1;
        D_8011EB36 = -1;
        return 0;
    }
    D_8011EB34 = msg;
    D_8011EB36 = id;
    SysPlayXA(D_8011EB38, gameObjectsInfo[id].runtime, 1);
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/sound", SysSoundPlayQueuedXA);
#endif

s32 SysSoundIsXAQueued(void) {
    return D_8011EB30 == -3;
}

void SysSoundPlayRandXA(XARandTable *table) {
    s32 avail[32];
    s32 count = 0;
    s32 i;
    s32 pick;

    for (i = 0; table->tracks[i] != -1 && i < 32; i++) {
        if (!(table->used & (1 << i))) {
            avail[count++] = i;
        }
    }
    pick = rand() % count;
    table->used |= 1 << avail[pick];
    SysPlayXA(table->tracks[avail[pick]], NULL, 0);
    if (count == 1) {
        table->used = 0;
    }
}

void SoundPlayXACASAnimCB(s32 id, s32 event) {
    GameObjectRuntime *rt;
    s32 chance;

    if (event == 0x55) {
        chance = rand() & 0x63;
        rt = gameObjectsInfo[(s16)id].runtime;
        if (chance < 10 && rt->npc != NULL && !((SoundNPC *)rt->npc)->unk51) {
            SysSoundPlayRandXA(&D_80106A88);
        }
    }
}

#ifdef NON_MATCHING
/* cc1 folds the -1 into the note before adding rand() % 3; the original adds (rand() % 3 - 1)
 * to the sign-extended note. */
void SysPlayWpnSnd(s32 type, s32 song, s32 src, s32 variant) {
    s32 id = song + variant;
    s16 note;

    note = GetSoundNote(D_8011EB08[type], id, -1);
    SetSoundNote(D_8011EB08[type], id, note + (rand() % 3 - 1), 0, 0);
    SysPlaySong(type, id, src, 0);
    SetSoundNote(D_8011EB08[type], id, -1, 0, 0);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/sound", SysPlayWpnSnd);
#endif

void SysSetMusicExciteLevel(u8 level) {
    SetGlobalExcite(level);
}

void SysUpdateMusicExciteLevel(s32 level) {
    s32 cur = GetGlobalExcite();

    if (level < cur) {
        cur -= 5;
        if (cur < level) {
            cur = level;
        }
    } else if (cur < level) {
        cur = level;
    }
    SetGlobalExcite(cur);
}

#ifdef NON_MATCHING
/* Register allocation only: the NPC frame/threat and excite/total pairs get swapped
 * registers ($a0/$v1, $a0/$t0). */
/* Next active NPC id from npcList, -1 when done */
#define SOUND_NEXT_NPC(id)                          \
    for ((id) = -1, n = getNPCIndex; n < 6; n++) {  \
        (id) = npcList[n];                          \
        if ((id) >= 0) {                            \
            getNPCIndex = n + 1;                    \
            break;                                  \
        }                                           \
    }

void SoundProcessThreat(GameObjectRuntime *player) {
    VECTOR32 me;
    VECTOR32 them;
    s32 dists[4]; /* nearest first; the fifth entry overruns into dist */
    s32 dist = -1;
    s32 count = 0;
    s32 minDist = 0x5340;
    s32 maxThreat = 0;
    s32 id;
    s32 n;
    s32 m;
    s32 threat;
    s32 excite;
    s32 level;
    s32 total;
    s32 i;
    s32 v;
    SoundFrame *frame;
    GameObjectRuntime *rt;

    frame = ((SoundAnim *)player->anim)->frame;
    me.vx = frame->t[0];
    me.vy = frame->t[1];
    me.vz = frame->t[2];
    me.vy = -me.vy;
    getNPCIndex = 0;
    for (id = -1, m = 0; m < 6; m++) {
        id = npcList[m];
        if (id >= 0) {
            getNPCIndex = m + 1;
            break;
        }
    }
    while (id >= 0) {
        rt = gameObjectsInfo[id].runtime;
        frame = ((SoundAnim *)rt->anim)->frame;
        them.vx = frame->t[0];
        them.vy = frame->t[1];
        them.vz = frame->t[2];
        them.vy = -them.vy;
        threat = ((SoundTarget *)rt->target)->threat;
        if (maxThreat < threat) {
            maxThreat = threat;
        }
        if (threat > 0 && rt->healthInfo->health > 0) {
            EGSGeometry_GetFuzzyDistancePoint2Point(&me, &them, &dist);
            if (dist < minDist) {
                minDist = dist;
                dists[count] = dists[0];
                dists[0] = dist;
            } else {
                dists[count] = dist;
            }
            if (count < 5) {
                count++;
            }
        }
        SOUND_NEXT_NPC(id);
    }
    excite = 0;
    if (maxThreat == 0) {
        excite = D_80106AD1;
    } else if (maxThreat == 1) {
        excite = D_80106AD2;
    } else if (maxThreat >= 0x547 && maxThreat < 0xAB8) {
        excite = D_80106AD3;
    } else if (maxThreat >= 0xAB8 && maxThreat < 0x1000) {
        excite = D_80106AD4;
    } else if (maxThreat == 0x1000) {
        excite = D_80106AD5;
    }
    total = 0;
    for (i = 0; i < count; i++) {
        v = ((0xC80 - dists[i]) >> 5) / D_80106AD0;
        if (v < 0) {
            v = 0;
        }
        total += mExciteLevelModTable[i] * v / D_80106AD0;
    }
    level = excite + total;
    if (D_80110194) {
        level += D_80106AD6;
    }
    if (D_80110198 == 1) {
        level = 100;
        SetMIDIRegister(5, 1);
        D_80110198 = 2;
    } else if (D_80110198 == 2) {
        level = 100;
    }
    if (level > 100) {
        level = 100;
    } else if (level <= 0) {
        level = 1;
    }
    SysUpdateMusicExciteLevel(level);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/sound", SoundProcessThreat);
#endif

void iSndExciteShootCB(void) {
    D_80110194 = 0;
}

void SysSoundSetShootingExciteLevel(void) {
    D_80110194 = 1;
    EGSDisplayQueueInterrupt(iSndExciteShootCB, 10, 0);
}

void SysSoundSetChaseExciteLevel(u8 level) {
    D_80110198 = level;
    if (level == 0) {
        SetMIDIRegister(5, 0);
    }
}

/* Periodic song timer: alternately plays and stops D_8011019C/D_801101A0. */
static void func_8006B6A0(void) {
    s32 delay;
    s32 r;

    if (D_801101A4) {
        r = rand() % 360;
    } else {
        r = rand() % 40;
    }
    if (D_801101A4) {
        delay = r + 180;
    } else {
        delay = r + 60;
    }
    if (D_801101A4 == 1) {
        SysStopSong(D_8011019C, D_801101A0);
    } else {
        SysPlaySong(D_8011019C, D_801101A0, 0, 0);
        if (rand() % 10 >= 6) {
            delay = rand() % 20 + 5;
            D_801101A4 = 1;
        }
    }
    SysExeTickQueueTimer(func_8006B6A0, delay, 0);
    D_801101A4 ^= 1;
}

void SysSoundPlayPeriodicSong(s32 song, s32 arg1) {
    D_8011019C = song;
    D_801101A0 = arg1;
    D_801101A4 = 0;
    SysExeTickQueueTimer(func_8006B6A0, 60, 0);
}
