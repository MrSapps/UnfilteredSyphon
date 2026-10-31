#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

#define SYS_STATE_HALTED 2

typedef struct PadState {
    /* 0x0 */ u8 status; /* 0xFF: no pad */
    /* 0x1 */ u8 unk1[3];
    /* 0x4 */ u16 buttons; /* active low */
} PadState;

void ResetCallback(void);
void EGSDeviceSetVideoType(s32 type);
void MainLoop(void);
void SysInit(u8 haltState);
void SysExecute(void);
void SysStallCheck(void);
void StreamSynchronous(void);
void CheckStreamRequests(s32 arg0);
void EGSDeviceControllerPoll(void);
s32 EGSDeviceControllerRead(PadState **out, u32 port);
void EGSDisplaySort(s32 run, s32 tick);
void EGSDisplayNoSort(void);
void TextUpdate(PadState *pad);
u16 TextReplaceString(u16 id, char *str, s32 arg2);
void TextRemoveString(u16 id);
void TextRemoveWindowNow(u8 window);
void TextSetStringIntensity(u16 id, s32 r, s32 g, s32 b);
void func_80144000(void); /* title.ovl */
void func_8013F480(void); /* menu.ovl MenuExecute */

extern u8 gMovieOpenFlag;  /* movie.ovl */
extern u16 D_8014BAD0;     /* pause message text id */
extern u8 gSpareTextWindow;
/* .sdata (docs/data_migration.md); MainLoop reads the "" with lui/addiu */
static char D_8010F600[] = "";  /* "" */
static s32 D_8010F604 = 0;      /* state the display was last sorted for */

/* cc1 inserts the __main() constructor call itself */
int main(void) {
    ResetCallback();
    EGSDeviceSetVideoType(0);
    MainLoop();
    return 0;
}

#ifdef NON_MATCHING
/* pad bit tests: the original keeps &pad->buttons and the pad pointer in swapped registers (2 insns) */
void MainLoop(void) {
    PadState *pad;
    u32 *words;
    s32 bit;
    s32 misalign;
    s32 state;
    s32 blink;
    u8 paused;

    gSystemTick = 0;
    for (;;) {
        do {
            if (gSystemState == SYS_STATE_HALTED) {
                if (gSysHaltState == 2) {
                    mSystemStateDepth = 1;
                    gSysHaltState = 4;
                } else {
                    SysStatePop();
                }
            }
            if (gSysHaltState == 5) {
                SysStatePush(4);
            } else {
                SysInit(gSysHaltState);
            }
        } while (gSystemState == SYS_STATE_HALTED);
        do {
            gSystemTick++;
            EGSDeviceControllerPoll();
            switch (gSystemState) {
            case 3:
                if (!gMovieOpenFlag) {
                    SysStopMovie();
                } else {
                    SysPostMessage(4, 5, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
                }
                break;
            case 0:
            case 5:
                SysExecute();
                gObjPlayer->move->unk100[0] = 0;
                break;
            case 4:
                func_80144000();
                break;
            case 7:
                func_8013F480();
                state = gSystemState;
                if (state != 7) {
                    gSystemState = 0;
                    StreamSynchronous();
                    gSystemState = state;
                } else {
                    CheckStreamRequests(0);
                }
                break;
            case 8:
                EGSDeviceControllerRead(&pad, 0);
                if (pad->status != 0xFF && pad->buttons != 0) {
                    words = (u32 *)&pad->buttons;
                    bit = 11;
                    misalign = (u32)words & 3;
                    if (misalign) {
                        bit = misalign * 8 + 11;
                        words = (u32 *)((u32)words & ~3);
                    }
                    if (!(words[bit >> 5] & (1 << (bit & 0x1F)))) {
                        words = (u32 *)&pad->buttons;
                        bit = 8;
                        if (misalign) {
                            bit = misalign * 8 + 8;
                            words = (u32 *)((u32)words & ~3);
                        }
                        if (!(words[bit >> 5] & (1 << (bit & 0x1F)))) {
                            TextRemoveString(TextReplaceString(D_8014BAD0, D_8010F600, misalign));
                            TextRemoveWindowNow(gSpareTextWindow);
                            D_8014BAD0 = 0xFFFF;
                            gSpareTextWindow = 0xFF;
                            EGSDisplayMalloc(EGSMemAvail());
                            SysStatePop();
                            goto blink;
                        }
                    }
                }
                TextUpdate(pad);
            blink:
                if (!(gSystemTick & 0xF) && D_8014BAD0 != 0xFFFF) {
                    blink = (u32)(gSystemTick & 0x10) >> 4;
                    TextSetStringIntensity(D_8014BAD0, (blink * 200) & 0xF8, (blink * 100) & 0xFF,
                                           (blink * 100) & 0xFF);
                }
                break;
            case 9:
                gObjPlayer->move->unk100[0] = 1;
                CheckStreamRequests(0);
                SysStallCheck();
                break;
            }
            if (D_8010F604 == gSystemState) {
                paused = SysIsPaused();
                EGSDisplaySort(paused == 0, paused ? 0x29A : gExeTick);
            } else {
                EGSDisplayNoSort();
                if (gSystemState == 6) {
                    SysStatePop();
                } else {
                    D_8010F604 = gSystemState;
                }
                if (gObjPlayer != NULL && gObjPlayer->anim != NULL) {
                    gObjPlayer->anim->faceList = NULL;
                }
            }
        } while (gSystemState != SYS_STATE_HALTED);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/main", MainLoop);

INCLUDE_RODATA("asm/main/nonmatchings/main", jtbl_80010000);
#endif
