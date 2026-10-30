#include "common.h"
#include "system.h"
#include "game.h"

/* EGS streaming movie player (MDEC via libpress, CD streaming via libcd's St* calls). */

typedef struct MovieRect {
    s16 x, y, w, h;
} MovieRect;

/* STR sector header */
typedef struct StHeader {
    /* 0x00 */ u16 status;
    /* 0x02 */ u16 type;    /* 0x8001: video */
    /* 0x04 */ u16 sector;  /* sector index within the frame */
    /* 0x06 */ u16 sectors; /* sectors per frame */
    /* 0x08 */ u32 frame;
    /* 0x0C */ u32 frameSize;
    /* 0x10 */ u16 width;
    /* 0x12 */ u16 height;
} StHeader;

typedef struct DrawEnv {
    /* 0x00 */ MovieRect clip;
} DrawEnv;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
/* the asserts here carry "bin/movie.c", not the usual "s/..." path */
#define ASSERT_MOVIE(cond) if (!(cond)) EGSDebugAssert(1, 0, D_801377E4, __LINE__)

/* Player state (movie.ovl .data/.bss, still defined in asm) */
extern s32 D_8013BB80;          /* EGS file handle */
extern s32 D_8013BB84;          /* screen x */
extern s32 D_8013BB88;          /* screen y */
extern s32 D_8013BB8C;          /* last frame, -1 for all */
extern void (*D_8013BB90)();    /* done callback */
extern void *D_8013BB94;         /* work buffer */
extern s32 D_8013BB98;          /* work buffer size */
extern s32 D_8013BB9C;          /* number of decode buffers */
extern u32 *D_8013BBA0;         /* ring buffer */
extern u8 *D_8013BBA4[2];       /* decode buffers */
extern u8 *D_8013BBAC[2];       /* slice buffers */
extern s32 D_8013BBB4;          /* ring buffer sectors */
extern s32 D_8013BBB8;          /* current decode buffer */
extern s32 D_8013BBBC;          /* current slice buffer */
extern u8 gMovieOpenFlag;
extern u8 D_8013BBC1;           /* first frame */
extern u8 D_8013BBC2;
extern u8 D_8013BBC3;           /* frame drawn */
extern u8 D_8013BBC4;           /* end of stream */
extern u8 D_8013BBC5;           /* paused */
extern u8 D_8013BBC6;           /* loop */
extern u8 D_8013BBC7;           /* needs restart */
extern s32 D_8013BBC8;          /* colour mode (1: 24-bit) */
extern u32 D_8013BBCC;          /* frame count */
extern u32 D_8013BBD0;          /* current frame */
extern u32 D_8013CBA4;          /* decode buffer size */
extern s32 D_8013CBA8;          /* slice size in words */
extern MovieRect D_8013CBAC;    /* movie rect on screen */
extern MovieRect D_8013CBB4;    /* current slice rect */
extern u32 *gHuffmanTable;
extern s32 StCdIntrFlag;
extern char D_801377E4[];       /* "bin/movie.c" */

s32 EGSFileRewind(s32 file);
void EGSFileStreamPlay(s32 file, s32 arg1, s32 arg2);
void EGSFileStreamPause(s32 on);
void EGSDeviceGraphicsGetDrawingEnvironment(DrawEnv **out);
void EGSDeviceGraphicsGetColorMode(s32 *mode);
void StSetMask(s32 arg0, s32 arg1, s32 arg2);
void StClearRing(void);
void StUnSetRing(void);
void StSetRing(u32 *ring, s32 sectors);
void StSetStream(s32 trueColor, s32 start, s32 end, void *func1, void *func2);
s32 StGetNext(u32 **addr, StHeader **header);
s32 StFreeRing(u32 *base);
void StCdInterrupt(void);
void DecDCTReset(s32 mode);
s32 DecDCTBufSize(u32 *bs);
void DecDCTvlc2(u32 *bs, u32 *buf, u32 *table);
void DecDCTvlcBuild(u32 *table);
void DecDCTin(u32 *buf, s32 mode);
void DecDCTout(u32 *buf, s32 size);
void DecDCToutCallback(void (*func)(void));
void LoadImage(MovieRect *rect, u32 *p);
s32 func_801379E0();
s32 func_80137A50(void);
void func_801380A0(void);
s32 func_80137F80();
s32 func_80138004();
s32 MovieWaitTillFinishDrawFrame(void);
s32 MovieRewind(void);
s32 MoviePause(void);

/* Rewind the stream to the first frame. */
s32 func_801379E0() {
    s32 err = EGSFileRewind(D_8013BB80);

    if (err != 0) {
        MovieClose();
        return err;
    }
    StSetMask(1, 1, D_8013BB8C);
    StClearRing();
    D_8013BBC7 = 1;
    return 0;
}

s32 func_80137A50(void) {
    EGSFileStreamPlay(D_8013BB80, 0, 0);
    StSetMask(0, 1, D_8013BB8C);
    D_8013BBC7 = 0;
    return 0;
}

#ifdef NON_MATCHING
/* register allocation (hdr/y swapped) and MIN result stores */
/* Work out the on-screen rect and carve the slice and decode buffers out of the work buffer. */
s32 func_80137A98(StHeader *hdr, s32 x, s32 y, u8 *buf, s32 bufSize, s32 numDecode, s32 trueColor,
                  MovieRect *rect, MovieRect *slice, u8 **slices, s32 *sliceSize, u8 **decode, s32 *decodeSize) {
    DrawEnv *env;
    MovieRect clip;
    s32 i;

    EGSDeviceGraphicsGetDrawingEnvironment(&env);
    clip = env->clip;
    rect->w = MIN(hdr->width, MAX(0, clip.w - x));
    if (trueColor == 1) {
        rect->w = rect->w * 24 / 16;
    } else {
        rect->w = rect->w;
    }
    rect->h = MIN(hdr->height, MAX(0, clip.h - y));
    slice->w = 16;
    if (trueColor == 1) {
        slice->w = 24;
    }
    slice->h = rect->h / 16 * 16;
    if (trueColor == 1) {
        *sliceSize = hdr->height * 12;
    } else {
        *sliceSize = hdr->height * 8;
    }
#line 213
    ASSERT_MOVIE(buf != NULL);
    for (i = 0; i < 2; i++) {
        slices[i] = buf;
        bufSize -= *sliceSize * 4;
        buf += *sliceSize * 4;
        if (bufSize < 0) {
            goto fail;
        }
    }
    *decodeSize = hdr->sectors * 1710;
    for (i = 0; i < numDecode; i++) {
        decode[i] = buf;
        bufSize -= *decodeSize * 4;
        buf += *decodeSize * 4;
        if (bufSize < 0) {
            goto fail;
        }
    }
    return 0;
fail:
    MovieClose();
    return 0x1F;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", func_80137A98);
#endif

#ifdef NON_MATCHING
/* 5 insns short: StGetNext retry loop shape */
/* Wait for the next frame from the CD ring buffer and check it is in sequence. */
s32 func_80137D80(u32 **addr, StHeader **hdr) {
    u32 tries;
    s32 ret;
    u32 frame;

    if (D_8013BBD0 == D_8013BBCC && !D_8013BBC1) {
        D_8013BBC4 = 1;
    }
    frame = 0;
    if (!D_8013BBC4) {
        ret = 1;
        tries = 0;
        while (tries++ < 300000 && (ret = StGetNext(addr, hdr)) == 1) {
        }
        if (ret == 1 || (*hdr)->type != 0x8001 || (*hdr)->sector != 0) {
            D_8013BBC4 = 1;
        }
        frame = 0;
        if (!D_8013BBC4) {
            if (!D_8013BBC1) {
                if (D_8013BBCC < (*hdr)->frame || D_8013BBD0 >= (*hdr)->frame) {
                    D_8013BBC4 = 1;
                }
            } else if (D_8013BB8C > 0) {
                D_8013BBCC = D_8013BB8C;
            } else {
                D_8013BBCC = ((u32 *)D_8013BB80)[1] / ((*hdr)->sectors << 11);
            }
            frame = 0;
            if (!D_8013BBC4) {
                frame = (*hdr)->frame;
            }
        }
    }
    D_8013BBD0 = frame;
    return 0;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", func_80137D80);
#endif

s32 func_80137F80(u32 *frame, u32 *size) {
    if (D_8013CBA4 < DecDCTBufSize(frame)) {
        StFreeRing(frame);
        return 0x1F;
    }
    DecDCTvlc2(frame, size, gHuffmanTable);
    return StFreeRing(frame) == 1 ? 0x26 : 0;
}

s32 func_80138004(u32 *bs, u32 *buf, s32 trueColor, s32 *index, u32 **slices) {
    DecDCTReset(1);
    DecDCTin(bs, trueColor == 1);
    DecDCTout(slices[*index], buf);
    *index = *index == 0;
    return 0;
}

/* DecDCTout callback: upload the finished slice and decode the next one. */
void func_801380A0(void) {
    s32 next;

    if (D_8013BBC8 == 1 && StCdIntrFlag == D_8013BBC8) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    if (D_8013CBB4.x + D_8013CBB4.w * 2 <= D_8013CBAC.x + D_8013CBAC.w) {
        DecDCTout(D_8013BBAC[D_8013BBBC], D_8013CBA8);
    } else {
        DrawSync(0);
        D_8013BBC3 = 1;
    }
    next = D_8013BBBC == 0;
    D_8013BBBC = next;
    LoadImage(&D_8013CBB4, D_8013BBAC[next]);
    D_8013CBB4.x += D_8013CBB4.w;
}

s32 func_801381BC(void) {
    u32 *addr;
    StHeader *hdr;
    s32 err = func_80137D80(&addr, &hdr);

    if (err == 0) {
        if (D_8013BBC4) {
            return 0;
        }
        if (D_8013BBC1) {
            err = func_80137A98(hdr, D_8013BB84, D_8013BB88, D_8013BB94, D_8013BB98, D_8013BB9C, D_8013BBC8,
                                &D_8013CBAC, &D_8013CBB4, D_8013BBAC, &D_8013CBA8, D_8013BBA4, &D_8013CBA4);
            if (err != 0) {
                goto fail;
            }
        }
        err = func_80137F80(addr, D_8013BBA4[D_8013BBB8]);
        if (err == 0) {
            return 0;
        }
    }
fail:
    MovieClose();
    return err;
}

s32 func_801382F4(void) {
    DrawEnv *env;
    MovieRect clip;
    s32 err;

    EGSDeviceGraphicsGetDrawingEnvironment(&env);
    clip = env->clip;
    D_8013CBB4.x = D_8013CBAC.x = clip.x + D_8013BB84;
    D_8013CBB4.y = D_8013CBAC.y = clip.y + D_8013BB88;
    D_8013BBC3 = 0;
    err = func_80138004(D_8013BBA4[D_8013BBB8], D_8013CBA8, D_8013BBC8, &D_8013BBBC, D_8013BBAC);
    if (err != 0) {
        MovieClose();
        return err;
    }
    return 0;
}

s32 MovieSetLoopFlag(u8 loop) {
    D_8013BBC6 = loop;
    return 0;
}

s32 MovieSetDoubleDecodeBuffers(u8 on) {
    D_8013BB9C = (on == 1) ? 2 : 1;
    return 1;
}

s32 MovieSetStreamBufferNumSectors(s32 sectors) {
    D_8013BBB4 = sectors;
    return 0;
}

s32 MovieOpen(char *path, s32 x, s32 y, s32 lastFrame, void (*done)(void), void *buf, s32 bufSize) {
    s32 err;
    s32 ringSize;

    if (path == NULL) {
        return 1;
    }
    if (EGSFileOpen(path, &D_8013BB80) != 0) {
        return 5;
    }
    gHuffmanTable = (u32 *)((u8 *)buf + bufSize);
    if (x < 0 || y < 0) {
        return 1;
    }
    if (gMovieOpenFlag) {
        err = MovieClose();
        if (err != 0) {
            MovieClose();
            return err;
        }
    }
    D_8013BB84 = x;
    D_8013BB88 = y;
    D_8013BB8C = lastFrame;
    if (lastFrame <= 0) {
        D_8013BB8C = -1;
    }
    D_8013BB90 = done;
    D_8013BB94 = buf;
    D_8013BB98 = bufSize;
    D_8013BBB8 = 0;
    D_8013BBC1 = 1;
    D_8013BBC2 = 1;
    D_8013BBC3 = 1;
    D_8013BBC4 = 0;
    D_8013BBC5 = 0;
    D_8013BBC7 = 1;
    EGSDeviceGraphicsGetColorMode(&D_8013BBC8);
#line 517
    ASSERT_MOVIE(D_8013BB94 != NULL);
    DecDCTvlcBuild(gHuffmanTable);
    D_8013BBA0 = D_8013BB94;
    ringSize = D_8013BBB4 << 11;
    D_8013BB94 = (u8 *)D_8013BB94 + ringSize;
    D_8013BB98 -= ringSize;
    if (D_8013BB98 < 0) {
        MovieClose();
        return 0x1F;
    }
    StSetRing(D_8013BBA0, D_8013BBB4);
    StClearRing();
    StSetStream(D_8013BBC8 == 1, 1, D_8013BB8C, NULL, NULL);
    DecDCTReset(0);
    DecDCToutCallback(func_801380A0);
    gMovieOpenFlag = 1;
    return 0;
}

s32 MovieRewind(void) {
    s32 err;

    if (!gMovieOpenFlag) {
        return 0x1C;
    }
    D_8013BBC2 = 1;
    D_8013BBC3 = 1;
    D_8013BBC4 = 0;
    D_8013BBC5 = 0;
    D_8013BBC7 = 1;
    err = func_801379E0(D_8013BB80);
    if (err != 0) {
        MovieClose();
        return err;
    }
    DecDCTReset(0);
    return 0;
}

s32 MoviePlayNextFrame(void) {
    s32 err;
    s32 tries;

    if (!gMovieOpenFlag) {
        return 0x1C;
    }
    if (D_8013BBC7 && !D_8013BBC5) {
        err = func_80137A50();
        if (err != 0) {
            goto fail;
        }
        D_8013BBC7 = 0;
    }
    if (D_8013BBC2 && !D_8013BBC5) {
        tries = 0;
        while (1) {
            D_8013BBC4 = 0;
            err = func_801381BC();
            if (err != 0) {
                goto fail;
            }
            if (!D_8013BBC4) {
                break;
            }
            err = func_801379E0(D_8013BB80);
            if (err != 0) {
                goto fail;
            }
            err = func_80137A50();
            if (err != 0) {
                goto fail;
            }
            tries++;
            if (!D_8013BBC4) {
                break;
            }
            if (tries >= 10) {
                return 4;
            }
        }
        D_8013BBC2 = 0;
        D_8013BBC1 = 0;
    }
    err = func_801382F4();
    if (err != 0) {
        goto fail;
    }
    if (!D_8013BBC5) {
        err = func_801381BC();
        if (err != 0) {
            goto fail;
        }
    }
    err = MovieWaitTillFinishDrawFrame();
    if (err != 0) {
        goto fail;
    }
    if (D_8013BBC4) {
        err = MovieWaitTillFinishDrawFrame();
        if (err != 0) {
            goto fail;
        }
        err = MovieRewind();
        if (err != 0) {
            goto fail;
        }
        if (!D_8013BBC6) {
            err = MoviePause();
            if (err != 0) {
                goto fail;
            }
        }
        D_8013BBC4 = 0;
        if (D_8013BB90 != NULL && !D_8013BBC6) {
            D_8013BB90 = NULL;
            MovieClose();
        }
    }
    return 0;
fail:
    MovieClose();
    return err;
}

#ifdef NON_MATCHING
/* return value kept in v1 and copied, the original builds it in v0 */
s32 MovieWaitTillFinishDrawFrame(void) {
    u32 i = 0;
    s32 err;

    if (!gMovieOpenFlag) {
        return 0x1C;
    }
    err = 0;
    if (!D_8013BBC3) {
        while (i++ <= 1000000 && !D_8013BBC3) {
        }
        err = 0;
        if (!D_8013BBC3) {
            D_8013BBC3 = 1;
            err = 0x27;
        }
    }
    return err;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", MovieWaitTillFinishDrawFrame);
#endif

#ifdef NON_MATCHING
/* return value kept in v1 and copied, the original builds it in v0 */
s32 MoviePause(void) {
    if (!gMovieOpenFlag) {
        return 0x1C;
    }
    if (!D_8013BBC5) {
        StSetMask(1, 0, D_8013BB8C);
        EGSFileStreamPause(1);
        D_8013BBC5 = 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", MoviePause);
#endif

#ifdef NON_MATCHING
/* return value kept in v1 and copied, the original builds it in v0 */
s32 MovieUnpause(void) {
    s32 err = 0x1C;

    if (gMovieOpenFlag) {
        if (D_8013BBC5) {
            D_8013BBC7 = 1;
            D_8013BBC5 = 0;
            return 0;
        }
        err = 0;
    }
    return err;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", MovieUnpause);
#endif

s32 MovieClose(void) {
    if (gMovieOpenFlag) {
        MoviePause();
        StUnSetRing();
        DecDCTReset(0);
        DecDCToutCallback(NULL);
        EGSFileClose(&D_8013BB80);
        gMovieOpenFlag = 0;
    }
    return 0;
}

#ifdef NON_MATCHING
/* return value kept in v1 and copied, the original builds it in v0 */
s32 EGSMovieWaitTillFinishDrawFrame(void) {
    u32 i = 0;
    s32 err;

    if (!gMovieOpenFlag) {
        return 0x1C;
    }
    err = 0;
    if (!D_8013BBC3) {
        while (i++ <= 1000000 && !D_8013BBC3) {
        }
        err = 0;
        if (!D_8013BBC3) {
            D_8013BBC3 = 1;
            err = 0x27;
        }
    }
    return err;
}
#else
INCLUDE_ASM("asm/movie/nonmatchings/movie", EGSMovieWaitTillFinishDrawFrame);
#endif

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_801377E4);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_801377F0);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_8013780C);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_8013781C);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_8013782C);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_80137840);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", jtbl_80137868);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_801378E8);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_80137910);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_80137934);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_80137960);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_80137990);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_801379B0);

INCLUDE_RODATA("asm/movie/nonmatchings/movie", D_801379C0);
