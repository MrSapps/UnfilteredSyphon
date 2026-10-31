#include "common.h"
#include "game.h"

/*
 * HOG archive: a 0x800-byte directory sector followed by the block data.
 * Offsets in the header are relative to the header itself.
 */
typedef struct HogHeader {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 numBlocks;
    /* 0x008 */ s32 offsetTable; /* -> s32 blockOffset[numBlocks] (relative to dataStart) */
    /* 0x00C */ s32 names;       /* -> packed NUL-terminated block names */
    /* 0x010 */ s32 dataStart;   /* size of the directory */
    /* 0x014 */ u8 unk14[0x7F8 - 0x14];
    /* 0x7F8 */ s32 sector;      /* filled by EGSFileReadHogDirectory */
    /* 0x7FC */ s32 fileSize;
} HogHeader; /* size 0x800 */

typedef struct EGSHog {
    /* 0x00 */ HogHeader *header;
    /* 0x04 */ s32 unk4;
} EGSHog; /* size 0x8 */

typedef struct CdlFILE {
    /* 0x00 */ u8 pos[4];
    /* 0x04 */ u32 size;
    /* 0x08 */ char name[16];
} CdlFILE;

#define HOG_NAMES(hdr) ((char *)(hdr) + (hdr)->names)
#define HOG_OFFSETS(hdr) ((s32 *)((u8 *)(hdr) + (hdr)->offsetTable))
#define HOG_DATA(hdr) ((u8 *)(hdr) + (hdr)->dataStart)

/* .sdata: only the literal "" (D_8010FF64), shared by the three path checks */

void *EGSMemMalloc(u32 size);
void EGSMemFree(void *ptr);
s32 EGSFileOpen(char *path, s32 *file);
s32 EGSFileOpenPC(s32 arg0, char *path, s32 *file);
s32 EGSFileGetBufferSize(s32 file, u32 *size);
s32 EGSFileRead(s32 file, void *buf, u32 size, s32 *nread);
s32 EGSFileRewind(s32 file);
s32 EGSFileClose(s32 *file);
int strcmp(const char *, const char *);
int CdPosToInt(void *pos);

s32 EGSHogOpen_Common(s32 file, s32 unused, EGSHog **hog, HogHeader *buffer, u32 bufferSize) {
    u32 size;
    s32 nread;
    s32 err;

    if ((*hog = EGSMemMalloc(sizeof(EGSHog))) == NULL) {
        return 3;
    }
    err = EGSFileGetBufferSize(file, &size);
    if (err != 0) {
        EGSFileClose(&file);
        goto free_hog;
    }
    if (buffer != NULL) {
#line 216
        ASSERT(size <= bufferSize);
        (*hog)->header = buffer;
    } else {
        (*hog)->header = EGSMemMalloc(size);
        if ((*hog)->header == NULL) {
            return 3;
        }
    }
    err = EGSFileRead(file, (*hog)->header, size, &nread);
    if (err != 0) {
        EGSFileClose(&file);
        EGSMemFree((*hog)->header); /* cross-jumped with the free below */
        goto free_hog;
    }
    err = EGSFileClose(&file);
    if (err == 0) {
        return 0;
    }
    EGSMemFree((*hog)->header);
free_hog:
    EGSMemFree(*hog);
    return err;
}

s32 EGSHogOpenDirectoryOnly(char *path, s32 unused, HogHeader **out) {
    s32 file;
    s32 nread;
    HogHeader *buf;
    s32 size;

    buf = EGSMemMalloc(0x800);
#line 254
    ASSERT(buf != NULL);
    if (path == NULL) {
        return 1;
    }
    if (EGSFileOpen(path, &file) != 0) {
        return 4;
    }
    if (EGSFileRead(file, buf, 0x800, &nread) == 0) {
        size = buf->dataStart;
        if ((u32)size < 0x800) {
            size = 0x800;
        }
        if (size <= 0x800) {
            goto done;
        }
        EGSMemFree(buf);
        buf = EGSMemMalloc(size);
#line 280
        ASSERT(buf != NULL);
        if (EGSFileRewind(file) == 0 && EGSFileRead(file, buf, size, &nread) == 0) {
            goto done;
        }
    }
    EGSFileClose(&file);
    return 4;
done:
    *out = buf;
    EGSFileClose(&file);
    return 0;
}

s32 EGSHogOpenWithBuffer(char *path, s32 unused, EGSHog **hog, HogHeader *buffer, u32 bufferSize) {
    s32 file;
    s32 err;

    if (path == NULL || path == "" || hog == NULL) {
        return 1;
    }
    err = EGSFileOpen(path, &file);
    if (err != 0) {
        return err;
    }
    return EGSHogOpen_Common(file, unused, hog, buffer, bufferSize);
}

s32 EGSHogOpen(char *path, s32 unused, EGSHog **hog) {
    s32 file;
    s32 err;

    if (path == NULL || path == "" || hog == NULL) {
        return 1;
    }
    err = EGSFileOpen(path, &file);
    if (err != 0) {
        return err;
    }
    return EGSHogOpen_Common(file, unused, hog, NULL, 0);
}

s32 EGSHogOpenPC(s32 arg0, char *path, s32 unused, EGSHog **hog) {
    s32 file;
    s32 err;

    if (path == NULL || path == "" || hog == NULL) {
        return 1;
    }
    err = EGSFileOpenPC(arg0, path, &file);
    if (err != 0) {
        return err;
    }
    return EGSHogOpen_Common(file, unused, hog, NULL, 0);
}

s32 EGSHogClose(EGSHog **hog) {
    if (hog == NULL || *hog == NULL || (*hog)->header == NULL) {
        return 1;
    }
    EGSMemFree((*hog)->header);
    EGSMemFree(*hog);
    *hog = NULL;
    return 0;
}

s32 EGSHogGetBlockCount(EGSHog *hog) {
    if (hog == NULL || hog->header == NULL) {
        return 1;
    }
    return hog->header->numBlocks;
}

s32 EGSHogGetBlock(EGSHog *hog, s32 id, void **out) {
    HogHeader *hdr;

    if (hog == NULL || (hdr = hog->header) == NULL || out == NULL) {
        return 1;
    }
    if (id < 0 || id >= hdr->numBlocks) {
        return 14;
    }
    *out = HOG_DATA(hdr) + HOG_OFFSETS(hdr)[id];
    return 0;
}

s32 EGSHogGetBlockByName(EGSHog *hog, char *name, void **out) {
    s32 i;
    s32 ofs;

    if (hog == NULL || hog->header == NULL || name == NULL || out == NULL) {
        return 1;
    }
    ofs = 0;
    for (i = 0; i < hog->header->numBlocks; i++) {
        if (strcmp(name, HOG_NAMES(hog->header) + ofs) == 0) {
            break;
        }
        while (HOG_NAMES(hog->header)[ofs++] != 0) {
        }
    }
    if (i >= hog->header->numBlocks) {
        return 5;
    }
    *out = HOG_DATA(hog->header) + HOG_OFFSETS(hog->header)[i];
    return 0;
}

s32 EGSHogGetIdByName(EGSHog *hog, char *name) {
    s32 i;
    s32 ofs;

    ofs = 0;
    for (i = 0; i < hog->header->numBlocks; i++) {
        if (strcmp(name, HOG_NAMES(hog->header) + ofs) == 0) {
            break;
        }
        while (HOG_NAMES(hog->header)[ofs++] != 0) {
        }
    }
    if (i >= hog->header->numBlocks) {
        return -1;
    }
    return i;
}

s32 EGSHogGetBlockName(EGSHog *hog, s32 id, char **out) {
    s32 i;
    s32 ofs;

    if (hog == NULL || hog->header == NULL || out == NULL) {
        return 1;
    }
    if (id < 0 || id >= hog->header->numBlocks) {
        return 14;
    }
    ofs = 0;
    for (i = 0; i < id; i++) {
        while (HOG_NAMES(hog->header)[ofs++] != 0) {
        }
    }
    *out = HOG_NAMES(hog->header) + ofs;
    return 0;
}

u8 EGSHogGetBlockInfoFromHeader(HogHeader *hdr, char *name, u32 *offset, u32 *size) {
    u32 i;
    char *names;
    s32 *offsets;

    i = 0;
    names = HOG_NAMES(hdr);
    offsets = HOG_OFFSETS(hdr);
    for (i = 0; i < hdr->numBlocks; i++) {
        if (strcmp(name, names) == 0) {
            break;
        }
        while (*names++ != 0) {
        }
    }
    if (i >= hdr->numBlocks) {
        return 0;
    }
    *offset = hdr->dataStart + offsets[i];
    if (i == hdr->numBlocks - 1) {
        *size = hdr->fileSize - offsets[i] - 0x800;
    } else {
        *size = offsets[i + 1] - offsets[i];
    }
    return 1;
}

s32 EGSFileReadHogDirectory(CdlFILE *file, HogHeader *hdr) {
    s32 nread;

    if (EGSFileRead((s32)file, hdr, 0x800, &nread) != 0) {
        return 0;
    }
#line 597
    ASSERT(hdr->dataStart == 0x800);
    hdr->sector = CdPosToInt(file->pos);
    hdr->fileSize = file->size;
    return 1;
}
