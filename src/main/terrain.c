#include "common.h"
#include "game.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

/* terrain.c's small data, still assembled from asm */
SDATA_LOCAL(vramLoaded)
SDATA_LOCAL(gTextureCache)
SDATA_LOCAL(gTerrSectorCount)
SDATA_LOCAL(gNumTerrainDataBlocks)
SDATA_LOCAL(gTerrainCache)
SDATA_LOCAL(gTerrainCacheFreeBlocks)
SDATA_LOCAL(gTerrainCacheInactiveBlocks)
SDATA_LOCAL(gSHCount)
SDATA_LOCAL(gSectorsPerBlock)
SDATA_LOCAL(gTerrSector)
SDATA_LOCAL(gTUTime)
SDATA_LOCAL(gTerrainDirectory)
SDATA_LOCAL(gGabeSectorAnim)
SDATA_LOCAL(gCache)
SDATA_LOCAL(gVramHogCount)
SDATA_LOCAL(D_8010F9FC)
SDATA_LOCAL(D_8010FA00)

/* Terrain sector table (gSectorMap[], 0x10 bytes each). */
typedef struct SectorMapEntry {
    /* 0x00 */ char *name;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u8 numParts; /* streamed parts (sectors on disc) */
    /* 0x09 */ u8 id;
    /* 0x0A */ u8 originalId;
    /* 0x0B */ u8 unk0B[5];
} SectorMapEntry;

/*
 * Model of a terrain sector's anim. data points at the sector's first streamed part;
 * bit 31 is cleared (no longer a KSEG0 address) while the sector is not displayed.
 */
typedef struct TerrainModel {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 data;
} TerrainModel;

/* Header of a sector's first streamed part. */
typedef struct TerrainPartHeader {
    /* 0x00 */ u8 bank; /* VRAM bank in the high nibble */
    /* 0x01 */ u8 unk01;
    /* 0x02 */ s16 sector;
    /* 0x04 */ u8 unk04[0x84];
    /* 0x88 */ u32 pages; /* texture pages used, bit per page of the bank */
} TerrainPartHeader;

/* Completed stream request as passed to the callbacks. */
typedef struct StreamReq {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ s32 tag;
} StreamReq;

typedef struct TerrainRect {
    s16 x, y, w, h;
} TerrainRect;

#define VRAM_PAGES_PER_BANK 32

typedef struct TerrainAnim {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ TerrainModel *model;
} TerrainAnim;

/* Per sector runtime state (gTerrSector[], 0x3C bytes each). */
typedef struct TerrSector {
    /* 0x00 */ TerrainAnim *anim;
    /* 0x04 */ void *parts[14]; /* cache block data per streamed part */
} TerrSector;

/* Objects/generators placed in a sector (objActivationList[], generatorActivationList[]). */
typedef struct ActivationList {
    /* 0x00 */ s32 count;
    /* 0x04 */ GameObjectInfo **infos;
} ActivationList;

/* One block of the terrain stream cache (gTerrainCache[], 0x14 bytes each). */
typedef struct TerrainCacheEntry {
    /* 0x00 */ void *data;
    /* 0x04 */ TerrainAnim *owner;
    /* 0x08 */ struct SectorMapEntry *map;
    /* 0x0C */ s32 sector;
    /* 0x10 */ s32 state; /* 1: free, 0: in use, < 0: loading, > 1: unused since gTUTime */
} TerrainCacheEntry;

/* Pending sector activation requests for game objects. */
typedef struct SHRequest {
    /* 0x0 */ s16 id;
    /* 0x2 */ u8 activate;
    /* 0x3 */ u8 pad;
} SHRequest;

#define SH_REQUEST_MAX 0x7D

/* One VRAM texture page of the level's texture file (gVRAMMap[2][33]). */
typedef struct VramMapEntry {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ u8 pos[4]; /* CdlLOC */
    /* 0x8 */ u8 unk8;
    /* 0x9 */ u8 page;
    /* 0xA */ u8 page2;
    /* 0xB */ u8 unkB[5];
} VramMapEntry;

#define VRAM_MAP_PAGES 33
#define EMD_OFFSET_COUNT 33

/* Sector/object model data as loaded (EMD). Offsets are segment:offset until fixed up. */
typedef struct EMDHeader {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 fixed; /* -1 once the offsets are pointers */
    /* 0x04 */ s32 offsets[EMD_OFFSET_COUNT]; /* segment in bits 14+, offset in the low 14 */
    /* 0x88 */ u8 unk88[4];
    /* 0x8C */ u8 *segments[4]; /* 64K each */
    /* 0x9C */ s32 unk9C;        /* segment:offset like offsets[] */
} EMDHeader;

typedef struct EMDOwner {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s32 unk10;
} EMDOwner;

/* Camera as seen by the display: 0x14 is its index into an anim's per-camera table. */
typedef struct DisplayCamera {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 index;
} DisplayCamera;

/* Level data header (gLevData). */
typedef struct LevData {
    /* 0x00 */ u8 unk00[0x6C];
    /* 0x6C */ u8 unk6C[0xC];
    /* 0x78 */ s32 sectorCount;
    /* 0x7C */ u8 unk7C[4];
    /* 0x80 */ u8 sectorNeighbors[1][15]; /* per sector, 0xFF terminated, 0xFE marks far sectors */
} LevData;

typedef struct Vec3 {
    s32 x, y, z;
} Vec3;

extern u8 vramLoaded;
extern void *gTextureCache;
extern s32 gTerrSectorCount;
extern s32 gNumTerrainDataBlocks;
extern TerrainCacheEntry *gTerrainCache;
extern s32 gTerrainCacheFreeBlocks;
extern s32 gTerrainCacheInactiveBlocks;
extern s32 gSHCount;
extern s32 gTUTime;
extern TerrSector *gTerrSector;
extern ActivationList *objActivationList;
extern ActivationList *generatorActivationList;
extern u8 mFaceCollisionSectors[];
extern SHRequest gSHArray[];
extern SectorMapEntry *gSectorMap;
extern s16 gGabeSector;
extern LevData *gLevData;
extern VramMapEntry gVRAMMap[2][33];
extern DisplayCamera *gSniperCam;
extern s32 gSniperMode;
extern char D_80012360[]; /* "s/terrain.c", still owned by asm SetupTerrain */
extern s16 gVramHogCount;
extern TerrainRect D_8010F9E0; /* one texture page */
extern char gVramFileName[];
extern char D_8010F9D0[]; /* "" */
extern char D_8010F9F0[]; /* "1" */
void InitStreamRequests(void);
void HotboxInit(TerrainAnim *anim);
void InitBxv(TerrainModel *model);
void ObjectSetSectorLighting(s32 sector);
extern s32 gSectorsPerBlock;
void CdIntToPos(s32 sector, void *pos);
void EGSDisplayGetSectorBox(EMDHeader *emd, s32 arg1);
void LoadImage(TerrainRect *rect, void *pixels);
void SectorActivated(s32 sector);

/* ASSERT against the asm-owned file name string. */
#define TASSERT(cond, line) \
    if (!(cond)) EGSDebugAssert(1, 0, D_80012360, line)

s32 CdPosToInt(void *pos);
void StreamRequest(void *cache, s32 pos, s32 size, s32 arg3, void (*done)(), s32 tag);
void VramSwap();
void SHReq(s32 id, u8 activate);
u8 FindSectorInNewMap(s16 sector, u8 *map, u8 includeFar);
void DeactivateSector(s32 sector);
void ActivateSector(s32 sector, u8 arg1);
void FlushSectorFromCache(s32 sector);
void FlushStreamRequests(void);
void GSDeleteAnim(void *anim);
void GSAddAnim(void *anim);
void DeactivateCurrentTerrain(void);
void JoystickGobjTeleport(Gobj *gobj, s32 x, s32 y, s32 z);
void func_8008198C(s32 sector);
void EGSDisplayAddAnim(void *camera, void *anim);
void EGSDisplayDeleteAnim(void *camera, void *anim);

#ifdef NON_MATCHING
/* page map addressing and an 8 byte larger frame */
void VramSwap(StreamReq *req) {
    TerrainRect rect = D_8010F9E0;
    s32 tag = req->tag;
    s32 page = tag & 0x1F;
    s32 bank = tag >> 5;
    s32 other;
    u8 *map;

    if ((tag & 0xF) < 6) {
        other = page + 6;
    } else {
        other = page - 6;
    }
    map = gTexturePageMap;
    map[bank * VRAM_PAGES_PER_BANK + page] = 1;
    /* the same page in the other bank is gone */
    if (bank == 0) {
        map += VRAM_PAGES_PER_BANK;
    }
    map[page] = 0;
    gTexturePageMap[other] = 0;
    gTexturePageMap[VRAM_PAGES_PER_BANK + other] = 0;
    if ((page & 0xF) < 6) {
        page += 6;
    }
    if (page >= 0x1C) {
        rect.h -= 0x20;
    }
    rect.x = (page & 0xF) << 6;
    if (page >= 0x10) {
        rect.y = 0x100;
    } else {
        rect.y = 0;
    }
    LoadImage(&rect, gTextureCache);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/terrain", VramSwap);
#endif

void AckVram(void) {
    vramLoaded = 1;
}

INCLUDE_RODATA("asm/main/nonmatchings/terrain", D_8001233C);

#ifdef NON_MATCHING
/* loop strength reduction differs (page map / gVRAMMap pointers); 8 insns longer */
void InitStreaming(void) {
    u8 pos[0x50];
    u8 *map;
    s32 file;
    s32 sector;
    s32 bank;
    s32 i;

    InitStreamRequests();
    for (bank = 0; bank < 2; bank++) {
        map = &gTexturePageMap[bank * VRAM_PAGES_PER_BANK];
        for (i = VRAM_PAGES_PER_BANK - 1; i >= 0; i--) {
            map[i] = 0;
        }
        sprintf(gVramFileName, "\\%s\\VRAM%s.HOG;1", gLevelNames[gSysInfo.level], bank ? D_8010F9F0 : D_8010F9D0);
        file = EGSFileSearchCD(pos, gVramFileName);
        if (bank == 0) {
            TASSERT(file != 0, 0xE7);
        }
        if (file != 0) {
            gVramHogCount++;
            sector = CdPosToInt((void *)file) + 1;
            for (i = 0; i < VRAM_MAP_PAGES; i++, sector += 0x10) {
                gVRAMMap[bank][i].unk0 = 0;
                CdIntToPos(sector, gVRAMMap[bank][i].pos);
                gVRAMMap[bank][i].page = i;
                gVRAMMap[bank][i].page2 = i;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/terrain", InitStreaming);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/terrain", D_80012360);

INCLUDE_ASM("asm/main/nonmatchings/terrain", SetupTerrain);

/* Cheap 3D distance: largest axis plus a quarter of the other two. */
s32 GameSysFuzzyDistPt2Pt(Vec3 *a, Vec3 *b) {
    s32 big, mid, small, t;

    big = a->x - b->x;
    if (big < 0) {
        big = b->x - a->x;
    }
    mid = a->y - b->y;
    if (mid < 0) {
        mid = b->y - a->y;
    }
    small = a->z - b->z;
    if (small < 0) {
        small = b->z - a->z;
    }
    if (big < mid) {
        t = big;
        big = mid;
        mid = t;
    }
    if (big < small) {
        t = big;
        big = small;
        small = t;
    }
    return big + ((mid + small) >> 2);
}

/* Drops inactive cached sectors that are not near the player's new sector. */
void FindBadCacheEntries(void) {
    s32 i;
    s32 sector;
    u8 *neighbors = gLevData->sectorNeighbors[gGabeSector];

    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        TerrainCacheEntry *entry = &gTerrainCache[i];
        sector = entry->sector;
        if (entry->state <= 0 && sector != gGabeSector && !FindSectorInNewMap(sector, neighbors, 1)) {
            DeactivateSector(sector);
        }
    }
}

/* Reserve cache blocks for a sector's parts. Returns blocks reserved, 0 if busy, -1 if cached. */
s32 AllocateFromTerrainCache(TerrainAnim *owner, SectorMapEntry *map, TerrainCacheEntry **out) {
    u8 found = 0;
    s32 needed = map->numParts / gSectorsPerBlock;
    s32 n = 0;
    s32 remaining;
    s32 oldest;
    s32 i;
    TerrainCacheEntry *entry;

    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        TerrainCacheEntry *e = &gTerrainCache[i];
        if (e->owner == owner) {
            if (e->state <= 0) {
                return 0;
            }
            if (e->state >= 2) {
                found = 1;
                e->state = 0;
                gTerrainCacheInactiveBlocks--;
            }
        }
    }
    if (found) {
        return -1;
    }
    if (gTerrainCacheInactiveBlocks < needed) {
        FindBadCacheEntries();
        if (gTerrainCacheInactiveBlocks < needed) {
            __asm__ volatile(".word 0x00003FCD"); /* break 0, 255: out of cache */
        }
    }
    while (gTerrainCacheFreeBlocks < needed) {
        oldest = 0x7FFFFFFF;
        for (i = 0; i < gNumTerrainDataBlocks; i++) {
            TerrainCacheEntry *e = &gTerrainCache[i];
            if (e->state >= 2 && e->state < oldest) {
                oldest = e->state;
            }
        }
        for (i = 0; i < gNumTerrainDataBlocks; i++) {
            entry = &gTerrainCache[i];
            if (entry->state == oldest) {
                TASSERT(entry->state != -1, 0x2F3);
                entry->state = 1;
                entry->owner = NULL;
                entry->sector = -1;
                gTerrainCacheFreeBlocks++;
            }
        }
    }
    remaining = needed;
    for (i = 0; remaining > 0; i++) {
        if (i >= gNumTerrainDataBlocks) {
            return n;
        }
        {
            TerrainCacheEntry *e = &gTerrainCache[i];
            if (e->state == 1) {
                remaining--;
                e->owner = owner;
                e->map = map;
                e->sector = map->id;
                e->state = -1;
                out[n++] = e;
                gTerrainCacheFreeBlocks--;
                gTerrainCacheInactiveBlocks--;
            }
        }
    }
    return n;
}

void StreamVramPage(s32 page, s32 bank) {
    StreamRequest(gTextureCache, CdPosToInt(gVRAMMap[bank][page].pos), 0x10, 0, VramSwap, page + bank * 32);
    SysLoadTextureCacheOverlay(0, 0);
}

#ifdef NON_MATCHING
/* page map base: the original adds the bank offset to the address the other way round */
void SectorActivated(s32 sector) {
    TerrSector *ts = &gTerrSector[sector];
    TerrainAnim *anim = ts->anim;
    TerrainPartHeader *part = ts->parts[0];
    u32 pages = part->pages;
    s32 bank;
    u8 *map;
    s32 i;

    anim->model->data = (s32)part;
    bank = part->bank >> 4;
    part->sector = sector;
    TASSERT(bank < gVramHogCount, 0x33A);
    for (i = 0, map = &gTexturePageMap[bank * VRAM_PAGES_PER_BANK]; i < VRAM_PAGES_PER_BANK;
         map++, i++, pages >>= 1) {
        if ((pages & 1) && *map == 0) {
            StreamVramPage(i, bank);
        }
    }
    GSAddAnim(anim);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/terrain", SectorActivated);
#endif

void FixupTerrain(void) {
    TerrainCacheEntry *entry;
    TerrainAnim *owner;
    s32 state;
    s32 sector;
    s32 i;

    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        entry = &gTerrainCache[i];
        owner = entry->owner;
        state = entry->state;
        sector = entry->sector;
        if (state == 0 && owner->model->data == 0) {
            SectorActivated(sector);
        }
        if (state >= 2 && owner->model->data == 0) {
            owner->model->data = (s32)gTerrSector[sector].parts[0] & 0x7FFFFFFF;
        }
    }
    DeactivateCurrentTerrain();
}

void ClearTerrainCachePending(s32 sector) {
    s32 i;

    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        if (gTerrainCache[i].sector == sector) {
            TASSERT(gTerrainCache[i].state < 0, 0x364);
            if (gTerrainCache[i].state == 1) {
                gTerrainCacheFreeBlocks--;
            }
            gTerrainCache[i].state = 0;
        }
    }
}

void TerrainPartCallback(s32 *req) {
    req[6] = 0;
}

void FlushedStreamCallback(s32 *req) {
    req[6] = 0;
}

void TerrainStreamCallback(StreamReq *req) {
    s32 sector = req->tag;
    TerrSector *ts = &gTerrSector[sector];
    EMDHeader *emd = ts->parts[0];
    TerrainAnim *anim = ts->anim;
    u32 off;
    s32 seg;
    s32 i;

    for (i = 0; i < 4; i++) {
        emd->segments[i] = ts->parts[i];
    }
    for (i = 0; i < EMD_OFFSET_COUNT; i++) {
        off = emd->offsets[i];
        seg = off / (gSectorsPerBlock << 11);
        if (off == -1) {
            continue;
        }
        if (off != 0) {
            TASSERT(seg < 4, 0x397);
            emd->offsets[i] = (s32)(emd->segments[seg] + off % (gSectorsPerBlock << 11));
        }
    }
    off = emd->unk9C;
    seg = off / (gSectorsPerBlock << 11);
    TASSERT(seg < 4, 0x39F);
    emd->unk9C = (s32)(emd->segments[seg] + off % (gSectorsPerBlock << 11));
    SectorActivated(sector);
    HotboxInit(anim);
    EGSDisplayGetSectorBox(emd, (s32)anim->model);
    InitBxv(anim->model);
    ObjectSetSectorLighting(sector);
    ClearTerrainCachePending(sector);
}

u8 FindSectorInNewMap(s16 sector, u8 *map, u8 includeFar) {
    s32 i;
    s16 c;

    for (i = 0;; i++) {
        c = *map;
        if (c == 0xFF || i == 14 || (!includeFar && c == 0xFE)) {
            return 0;
        }
        if (c == sector) {
            return 1;
        }
        map++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/terrain", TerrainUpdateActivation);

void SHReq(s32 id, u8 activate) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    SHRequest *req;
    s32 i;

    if (!activate && (gobj->flags23 & 1)) {
        return;
    }
    for (i = 0; i < gSHCount; i++) {
        SHRequest *p = &gSHArray[i];
        if (p->id == id) {
            p->activate = activate;
            return;
        }
    }
    req = &gSHArray[gSHCount++];
    req->activate = activate;
    req->id = id;
    TASSERT(gSHCount != SH_REQUEST_MAX, 0x569);
}

void GSActivateGameObject(Gobj *gobj) {
    SHReq(gobj->id, 1);
}

void GSDeactivateGameObject(Gobj *gobj) {
    if (!(gobj->flags23 & 1)) {
        SHReq(gobj->id, 0);
    }
}

void DeactivateCurrentTerrain(void) {
    u8 *lev = gLevData->unk6C; /* the original indexes from 0x6C */
    ActivationList *list;
    GameObjectRuntime *rt;
    s32 sector;
    s32 i, n, j;

    i = 0;
    n = 0;
    do {
        sector = ((u8(*)[15])(lev + 0x14))[gGabeSector][i];
        if (sector == 0xFF || n == 14) {
            sector = gGabeSector;
        }
        if (sector != 0xFE) {
            list = &objActivationList[sector];
            DeactivateSector(sector);
            for (j = 0; j < list->count; j++) {
                rt = list->infos[j]->runtime;
                if (rt != NULL && rt->id != gPlayerOffset && *(s32 *)rt->anim != 0) {
                    SHReq(rt->id, 0);
                }
            }
            mFaceCollisionSectors[sector] = 0;
        }
        i++;
        n++;
    } while (sector != gGabeSector);
}

extern u8 gUsePCFiles;

/* Sizes (in disc sectors) of up to four streamed parts. */
typedef struct PartSizes {
    s16 n[4];
} PartSizes;

/* Start streaming a sector into the cache (or just show it if it is still cached). */
void ActivateSector(s32 sector, u8 tag) {
    TerrSector *ts = &gTerrSector[sector];
    TerrainAnim *anim = ts->anim;
    SectorMapEntry *map;
    TerrainCacheEntry *blocks[4];
    PartSizes sizes;
    s32 count;
    s32 last;
    s32 pos;
    s32 i;

    if (((s8 *)&anim->model->data)[3] != -0x80) {
        map = &gSectorMap[sector];
        count = AllocateFromTerrainCache(anim, map, blocks);
        if (count == 0) {
            return;
        }
        last = count - 1;
        if (count < 0) {
            anim->model->data |= 0x80000000;
            SectorActivated(sector);
        } else {
            {
                PartSizes init;

                init.n[0] = gSectorsPerBlock;
                init.n[1] = gSectorsPerBlock;
                init.n[2] = gSectorsPerBlock;
                init.n[3] = gSectorsPerBlock;
                sizes = init;
            }
            pos = CdPosToInt(map->unk04);
            for (i = 0; i < count; i++) {
                ts->parts[i] = blocks[i]->data;
            }
            for (i = last; i > 0; i--) {
                if ((u8 *)blocks[i - 1]->data + gSectorsPerBlock * 0x800 == blocks[i]->data) {
                    sizes.n[i - 1] += sizes.n[i];
                    sizes.n[i] = 0;
                    if (i == last) {
                        last = i - 1;
                        count--;
                    }
                }
            }
            i = 0;
            while (i < count) {
                if (sizes.n[i] != 0) {
                    StreamRequest(blocks[i]->data, pos, sizes.n[i], tag,
                                  i < last ? (void (*)())TerrainPartCallback : (void (*)())TerrainStreamCallback,
                                  sector);
                }
                i++;
                pos += gUsePCFiles ? gSectorsPerBlock << 11 : gSectorsPerBlock;
            }
        }
    } else {
        SectorActivated(sector);
    }
}

void DeactivateSector(s32 sector) {
    TerrainAnim *anim = gTerrSector[sector].anim;
    TerrainCacheEntry *entry;
    TerrainModel *model;
    s32 i;

    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        entry = &gTerrainCache[i];
        if (entry->owner == anim) {
            if (entry->state == 0) {
                /* both arms are cross-jumped; the test only steers register allocation */
                if (sector) {
                    entry->state = gTUTime;
                    gTerrainCacheInactiveBlocks++;
                } else {
                    entry->state = gTUTime;
                    gTerrainCacheInactiveBlocks++;
                }
            } else if (entry->state < 0) {
                entry->owner = NULL;
                entry->sector = -1;
                entry->state = 1;
                gTerrainCacheInactiveBlocks++;
                gTerrainCacheFreeBlocks++;
            }
        }
    }
    FlushStreamRequests();
    model = anim->model;
    if ((model->data >> 24) == -0x80) {
        model->data &= 0x7FFFFFFF;
    }
    GSDeleteAnim(anim);
}

void GSAddAnim(void *anim) {
    DisplayCamera *sniper = gSniperCam;

    EGSDisplayAddAnim(*gCamera, anim);
    if (gSniperMode == 2) {
        EGSDisplayAddAnim(sniper, anim);
    }
}

void GSDeleteAnim(void *anim) {
    DisplayCamera *sniper = gSniperCam;

    EGSDisplayDeleteAnim(*gCamera, anim);
    if (((void **)anim)[sniper->index] != NULL) {
        EGSDisplayDeleteAnim(sniper, anim);
    }
}

void FlushSectorFromCache(s32 sector) {
    TerrainAnim *anim = gTerrSector[sector].anim;
    TerrainCacheEntry *entry;
    s32 i;

    anim->model->data = -1;
    for (i = 0; i < gNumTerrainDataBlocks; i++) {
        entry = &gTerrainCache[i];
        if (entry->owner == anim) {
            TASSERT(entry->state >= 2, 0x665);
            entry->owner = NULL;
            entry->sector = -1;
            entry->state = 1;
            gTerrainCacheFreeBlocks++;
        }
    }
}

void TerrainReplaceSector(s32 sector, s32 with) {
    DeactivateSector(sector);
    FlushSectorFromCache(sector);
    gSectorMap[sector] = gSectorMap[with];
    gSectorMap[sector].id = sector;
    objActivationList[sector] = objActivationList[with];
    generatorActivationList[sector] = generatorActivationList[with];
    ActivateSector(sector, 1);
}

s32 TerrainGetSectorIdByName(char *name) {
    SectorMapEntry *entry;
    s32 i;

    for (i = 0; i < gTerrSectorCount; i++) {
        entry = &gSectorMap[i];
        if (strcmp(entry->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

u8 TerrainGetOriginalId(s32 sector) {
    return gSectorMap[sector].originalId;
}

void func_8008198C(s32 sector) {
    TASSERT(sector < gLevData->sectorCount, 0x68F);
    if (gGabeSector != sector) {
        DeactivateCurrentTerrain();
        gGabeSector = sector;
        TerrainUpdateActivation(1);
    }
    PauseTerrainStream();
    UnpauseTerrainStream();
}

void TerrainZoom(s32 x, s32 sector) {
    JoystickGobjTeleport(gObjPlayer, x, 0, 0);
    func_8008198C(sector);
}

void TerrainZoomAndOrient(s32 x, s32 y, s32 sector) {
    JoystickGobjTeleport(gObjPlayer, x, y, 0);
    func_8008198C(sector);
}

void FixupEMDGameObject(EMDHeader *emd, EMDOwner *owner) {
    s32 off;
    EMDHeader *hdr = emd;
    s32 seg;
    s32 i;

    if (emd->fixed == -1) {
        EGSDisplayGetSectorBox(hdr, owner->unk10);
        return;
    }
    emd->fixed = -1;
    for (i = 3; i >= 0; i--) {
        emd->segments[i] = (u8 *)emd + i * 0x10000;
    }
    for (i = 0; i < EMD_OFFSET_COUNT; i++) {
        off = hdr->offsets[i];
        seg = (u32)off >> 14;
        if (off == -1) {
            continue;
        }
        if (off != 0 && off > 0) {
            TASSERT(seg < 4, 0x6C7);
            hdr->offsets[i] = (s32)(hdr->segments[seg] + (off & 0x3FFF));
        }
    }
    EGSDisplayGetSectorBox(hdr, owner->unk10);
}
