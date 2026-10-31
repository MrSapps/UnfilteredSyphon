#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

/* object.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_80010F60[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

typedef void (*ObjectCallback)();

/* .sdata */
static ObjectCallback D_8010F7CC = NULL; /* quip object callback */
static ObjectCallback D_8010F7D0 = NULL; /* special object callback */
static ObjectCallback D_8010F7D4 = NULL; /* special ambient callback */
static ObjectCallback D_8010F7D8 = NULL; /* ambient sound index callback */
ObjectCallback gObjectMovieDoneCallback = NULL;
u8 gNotKilledByFallingObject = 0;

/* .sbss */
static s32 D_8010FF98;
static s32 D_8010FF9C;
static s32 D_8010FFA0;
static s32 D_8010FFA4;
static s32 D_8010FFA8;
static s32 D_8010FFAC;
static s32 D_8010FFB0;
static s32 D_8010FFB4;

/* .data */
static s32 D_801052E0[3] = { 0xFFFF, 0x80FFC0, 0xFF8080 }; /* spark colours */
VECTOR32 gObjPathScaleVector = { 0x1000, 0x1000, 0x1000, 0 };
VECTOR32 gExploShakeVector = { 6, 9, 6, 0 };
VECTOR32 gExploDir = { 0, -0x1000, 0, 0 };

extern MovieInfo gObjectMovieInfo;
extern u8 gUsingSndBigFire;

ObjectCallback ObjectSetMovieDoneCallback(ObjectCallback cb);
ObjectCallback ObjectSetQuipObjectCallback(ObjectCallback cb);
ObjectCallback ObjectSetSpecialObjectCallback(ObjectCallback cb);
ObjectCallback ObjectSetSpecialAmbientCallback(ObjectCallback cb);
ObjectCallback ObjectSetAmbientSndIdxCallback(ObjectCallback cb);
void SysPlayMovie(MovieInfo *info);
void SysStopSong(s32 arg0, s32 song);
s32 SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);
void ParticleSmoke(s32 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void EGSAnimShowDetailDef(GameObjectAnim *anim);
void EGSDisplayRemoveDamage();
void ParticleFireHide(); /* (s16 id); unprototyped: EFFECTSObjDispatch passes extra args */
void MatrixNormal_0(void *in, void *out);
void SysSoundQueueXA(s16 level, s32 xa);
void SysSoundPlayQueuedXA(s32 arg0, s32 arg1);
u16 StringContactMeiXingMsg(void);
void CobjAddBulletCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3);
void CobjAddCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void CobjRemoveBulletCollisionBoxGobj(Gobj *gobj);
void CobjRemoveCollisionBoxGobj(Gobj *gobj);
void func_80148A00(Gobj *gobj);
s32 EGSMathFIXMul(s32 a, s32 b);
void ParticleFireShow(s16 id, VECTOR32 *pos, s32 arg2, s32 arg3);
s32 TerrainGetOriginalId(s32 sector);
void TerrainReplaceSector(s32 sector, s32 id);

typedef struct ActivationList {
    /* 0x00 */ s32 count;
    /* 0x04 */ GameObjectInfo **infos;
} ActivationList;

typedef struct LevelSectorInfo {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u8 links[1][15]; /* per sector: linked sectors, 0xFF = self, 0xFE = none */
} LevelSectorInfo;

typedef struct LevelData {
    /* 0x00 */ u8 unk00[0x6C];
    /* 0x6C */ LevelSectorInfo sectors;
} LevelData;

extern s32 gPlayerOffset;

void ParticleExplosion(s16 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void GameCameraShakeStart(void *state, s32 ticks, s32 arg2, void *vec, s32 arg4, void *pos);
void EGSDisplaySetFlashTime(s32 ticks);
void MovementAddGobj(Gobj *gobj);
void MovementRemoveGobj(Gobj *gobj);
void func_80149280(Gobj *gobj);
void ObjectSetLightState(Gobj *gobj, s32 state);
GameObjectRuntime *ObjectMatchToRefIDByCatID(s16 id, s16 category);
void EGSFrameTransformPoint(VECTOR32 *in, EGSFrame *frame, EGSFrame *ref, VECTOR32 *out);
void TerrainZoomAndOrient(VECTOR32 *pos, VECTOR32 *dir, s32 sector, s32 arg3);
void TerrainZoom(VECTOR32 *pos, s32 sector);
void DamageExplosion(s32 from, VECTOR32 *pos, s32 arg2);
int rand(void);

/* Animated model as listed in a display */
typedef struct ObjectDisplayAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ u16 id; /* 0x2000: game object, gameObjectsInfo index in the low 10 bits */
} ObjectDisplayAnim;

typedef struct ObjectDisplayNode {
    /* 0x00 */ ObjectDisplayAnim *anim;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ struct ObjectDisplayNode *next;
} ObjectDisplayNode;

typedef struct ObjectDisplay {
    /* 0x00 */ u8 unk00[0x8C];
    /* 0x8C */ ObjectDisplayNode *anims;
} ObjectDisplay;

void CameraGetDisplay(void *camera, ObjectDisplay **out);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);

void SysSetSongVolPan(s32 arg0, s32 song, Gobj *gobj, s32 vol, s32 pan);
void ParticleSparkShower(VECTOR32 *pos, s32 count, s32 color);
void func_80148780(Gobj *gobj, SysMsg *msg);

u8 PathHandleEx(Gobj *gobj, s32 scale, s32 orient, u8 pingPong, s16 first, s16 last, u8 *reachedEnd);
void func_80148EA8(Gobj *gobj);
static const char D_80010FD0[11]; /* "s/object.c", defined below */

extern u8 gPlayerOnFireFlag;
void func_80148938(Gobj *gobj, s16 type);

void CobjAddWindowGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3);
void CobjRemoveWindowGobj(Gobj *gobj);
void DamageGasCloud(s32 from, VECTOR32 *pos, s32 arg2);
void func_80148B1C(Gobj *gobj, s16 type);

extern u8 gAddingObjectiveViaQuip;
u8 LevelDoEOL(u8 force);
u8 LevelGetDirectionPointerFlag(void);
void LevelSetPointer(s32 id);
void GameCameraSetScrimOnFlag(u8 on);
u16 ObjectDoQuip(s32 id);
void ObjectUpdateAmbientObjectSetting(s32 id, s32 type);
void ObjectUpdateAmbientSndObjectSetting(s32 id, s32 type);

/* MoveInfo velocities */
typedef struct ObjectMove {
    /* 0x00 */ u8 unk00[0x70];
    /* 0x70 */ VECTOR32 vel;
    /* 0x80 */ u8 unk80[0x70];
    /* 0xF0 */ VECTOR32 rotVel;
} ObjectMove;

static inline u8 ObjectIsInactive(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 ret = 0;

    if (gobj == NULL || gobj->anim == NULL) {
        ret = 1;
    }
    return ret;
}



/* Gobj.unk04 as a word (damage state of multi-part objects) */
#define GOBJ_STATE(g) (*(u32 *)&(g)->unk04)

/* GameObjectHealth with the fields used here */
typedef struct GobjHealth {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 lastAttacker;
    /* 0x06 */ s16 armor;
    /* 0x08 */ s16 health;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s16 unk0C;
} GobjHealth;
#define GOBJ_HEALTH(g) ((GobjHealth *)(g)->healthInfo)

void GameCameraSetScrimScrollRate(u8 rate);
void func_801486DC(Gobj *gobj);
void DamageGOBJ(s32 arg0, s16 from, s16 to, s16 amount, s32 type);

typedef struct TerrSectorDisplay {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 lightSector;
} TerrSectorDisplay;

typedef struct TerrSectorModel {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ TerrSectorDisplay *display;
} TerrSectorModel;

typedef struct TerrSector {
    /* 0x00 */ TerrSectorModel *model;
    /* 0x04 */ u8 unk04[0x38];
} TerrSector; /* size 0x3C */

extern TerrSector *gTerrSector;
extern s32 gTerrSectorCount;

s32 EGSDisplayGetSectorLightArray(s32 sector);
void EGSDisplaySectorLight(s32 sector, s32 id, u8 on);
s32 EGSDisplayFindSectorWithLight(TerrSectorModel *model, s32 id);
void EGSAnimHideDetailDef(GameObjectAnim *anim);
void EGSDisplayKillAnimatedLight(s32 sector, s32 id);

/* MoveInfo fields only used here */
typedef struct MoveInfoRot {
    /* 0x00 */ u8 unk00[0xE0];
    /* 0xE0 */ VECTOR32 rotVel;
} MoveInfoRot;
extern s16 gGabeSector;
extern LevelData *gLevData;
extern ActivationList *objActivationList;

void ObjectTextureAnimationsHack(void) {
}

void ObjectInitData(void) {
    ObjectSetMovieDoneCallback(NULL);
    ObjectSetQuipObjectCallback(NULL);
    ObjectSetSpecialObjectCallback(NULL);
    ObjectSetSpecialAmbientCallback(NULL);
    ObjectSetAmbientSndIdxCallback(NULL);
    gNotKilledByFallingObject = 1;
    D_8010FF98 = 0;
    D_8010FF9C = -1;
    D_8010FFA0 = 0;
    D_8010FFA4 = -1;
    D_8010FFA8 = 0;
    D_8010FFAC = -1;
    D_8010FFB0 = 0;
    D_8010FFB4 = 0;
}

void iObjectDoMovieEx(void) {
    InterfaceForceOff();
    EGSDisplayFadeOff();
    SysStatePop();
    SysPlayMovie(&gObjectMovieInfo);
}

void ObjectDoMovieEx(char *name, s32 doneMsgId, s32 doneMsgTo, s32 doneMsgFrom) {
    sprintf(gObjectMovieInfo.name, "\\CUT\\%s", name);
    gObjectMovieInfo.halfSize = 0x28;
    gObjectMovieInfo.width = 320;
    gObjectMovieInfo.height = 240;
    gObjectMovieInfo.unk84 = 0;
    gObjectMovieInfo.doneMsgId = doneMsgId;
    gObjectMovieInfo.doneMsgFrom = doneMsgFrom;
    gObjectMovieInfo.doneMsgTo = doneMsgTo;
    gObjectMovieInfo.trueColor = 1;
    SysStall("MOVIE", 0xE, iObjectDoMovieEx);
}

static char D_8010F7F4[1] = ""; /* unreferenced .sdata literal after "MOVIE" */

ObjectCallback ObjectSetMovieDoneCallback(ObjectCallback cb) {
    ObjectCallback old = gObjectMovieDoneCallback;
    gObjectMovieDoneCallback = cb;
    return old;
}

void ObjectRemoveAndIgnoreBulletHoles(Gobj *gobj) {
    if (gobj->unk20[0] & 0x80) {
        gobj->unk20[0] &= 0x7F;
        EGSDisplayRemoveDamage();
    }
}

void ObjectReplaceSector(Gobj *gobj) {
    s32 sector;
    s32 j;
    s32 i = 0;
    ActivationList *list;
    LevelSectorInfo *sectors = &gLevData->sectors;

    do {
        sector = sectors->links[gGabeSector][i];
        if (sector != 0xFE) {
            if (sector == 0xFF) {
                sector = gGabeSector;
            }
            list = &objActivationList[sector];
            for (j = 0; j < list->count; j++) {
                if (gobj == list->infos[j]->runtime) {
                    TerrainReplaceSector(sector, TerrainGetOriginalId(sector) + 1);
                    return;
                }
            }
        }
        i++;
    } while (sector != gGabeSector);
}

s32 ObjectInvisibleMatchToRefID(s16 id, s16 type) {
    s32 i;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (ObjectIsInactive(i) && type == GOBJ_TYPE(i) && gameObjectsInfo[i].refId == id) {
            return i;
        }
    }
    return id;
}

GameObjectRuntime *ObjectMatchToRefID(s16 id, s16 type) {
    s32 i;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (!ObjectIsInactive(i) && type == GOBJ_TYPE(i) && gameObjectsInfo[i].refId == id) {
            return gameObjectsInfo[i].runtime;
        }
    }
    return gameObjectsInfo[id].runtime;
}

#ifdef NON_MATCHING
/* target hoists the gLevelGameObjCount reload out of the loop (cse2 then copies it, a3 = v1); our loop.c rates that move "not desirable" (44 insns) */
GameObjectRuntime *ObjectMatchToRefIDByCatID(s16 id, s16 category) {
    s32 i;
    Gobj *gobj;
    s32 count = gLevelGameObjCount;

    for (i = 0; i < count; i++) {
        if (!ObjectIsInactive(i)) {
            gobj = gameObjectsInfo[i].runtime;
            if (gobj != NULL && category == gobj->npcState && gameObjectsInfo[i].refId == id) {
                return gobj;
            }
        }
    }
    return gameObjectsInfo[id].runtime;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectMatchToRefIDByCatID);
#endif

/* Move the player to a teleport object. sector < 0: through a door (-1: keep the player's
 * side of the door), the player stays in the current sector. */
void ObjectTeleportPlayer(s16 id, s32 sector) {
    VECTOR32 dir;
    VECTOR32 doorDir;
    VECTOR32 pos;
    VECTOR32 doorPos;
    VECTOR32 local;
    VECTOR32 player;
    Gobj *door;
    EGSFrame *frame;
    u8 orient = 1;

    dir.vx = gameObjectsInfo[id].matrix.m[0][2];
    dir.vy = gameObjectsInfo[id].matrix.m[1][2];
    dir.vz = gameObjectsInfo[id].matrix.m[2][2];
    pos.vx = gameObjectsInfo[id].matrix.t[0];
    pos.vy = gameObjectsInfo[id].matrix.t[1];
    pos.vz = gameObjectsInfo[id].matrix.t[2];
    if (sector < 0) {
        player = gObjPlayer->move->pos;
        pos.vy = player.vy;
        if (sector < -1) {
            orient = 0;
        } else {
            door = ObjectMatchToRefIDByCatID(id, 9);
            if (id != door->id) {
                frame = door->anim->frame;
                doorDir.vx = frame->m[0][2];
                doorDir.vy = frame->m[1][2];
                doorDir.vz = frame->m[2][2];
                doorDir.vy = -doorDir.vy;
                doorPos.vx = frame->pos.vx;
                doorPos.vy = frame->pos.vy;
                doorPos.vz = frame->pos.vz;
                doorPos.vy = -doorPos.vy;
                if (((player.vx - doorPos.vx) * doorDir.vx + (player.vz - doorPos.vz) * doorDir.vz > 0) !=
                    ((pos.vx - doorPos.vx) * doorDir.vx + (pos.vz - doorPos.vz) * doorDir.vz > 0)) {
                    EGSFrameTransformPoint(&pos, NULL, door->anim->frame, &local);
                    local.vz = -local.vz;
                    EGSFrameTransformPoint(&local, door->anim->frame, NULL, &pos);
                    dir.vx = -dir.vx;
                    dir.vy = -dir.vy;
                    dir.vz = -dir.vz;
                }
            }
        }
        sector = gGabeSector;
    }
    if (orient) {
        TerrainZoomAndOrient(&pos, &dir, sector, sector);
    } else {
        TerrainZoom(&pos, sector);
    }
}

/* id is an s16 to callers (level.h); taken as s32 here, it is only narrowed where used. */
void ObjectRefMultiSYSPOSTMSG0(s32 id, s32 msg, s32 priority) {
    s16 ref = id;
    u8 more = 1;
    Gobj *gobj;

    do {
        ref = gameObjectsInfo[ref].refId;
        if (ref != -1) {
            gobj = gameObjectsInfo[ref].runtime;
            if (gobj != NULL && gobj->npcState == 4) {
                more = 0;
            }
            SysPostMessage(msg, priority, (s16)id, ref, 0, 0, 0, 0);
        } else {
            more = 0;
        }
    } while (more);
}

void ObjectBigExplosionFX(s16 id, VECTOR32 *pos, s32 radius, s32 shakeTicks, s32 flashTicks) {
    if (pos != NULL) {
        pos->vx = gameObjectsInfo[id].runtime->anim->frame->pos.vx;
        pos->vy = gameObjectsInfo[id].runtime->anim->frame->pos.vy;
        pos->vz = gameObjectsInfo[id].runtime->anim->frame->pos.vz;
        pos->vy = -pos->vy;
        ParticleExplosion(-1, pos, &gExploDir, radius, 1);
    }
    GameCameraShakeStart(NULL, shakeTicks, 1, &gExploShakeVector, 0, pos);
    EGSDisplaySetFlashTime(flashTicks);
}

void iElectronicStopShockSnd(s32 handle) {
    SysStopSong(1, handle);
}

void ObjectElectronicDestruction(Gobj *gobj) {
    VECTOR32 pos;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    ParticleSmoke(-1, &pos, &gExploDir, 0x599, 1);
    if (gobj->anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(gobj->anim);
    }
    SysExeTickQueueTimer(iElectronicStopShockSnd, 10, (s16)SysPlaySong(1, 0x18, gobj, 0));
}

void func_8002BE44(Gobj *gobj, s32 *count, s32 *handle) {
    VECTOR32 pos;

    if (!gUsingSndBigFire) {
        pos.vx = gobj->anim->frame->pos.vx;
        pos.vy = gobj->anim->frame->pos.vy;
        pos.vz = gobj->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        (*count)++;
    }
}

void func_8002BECC(Gobj *gobj, s32 *count, s32 handle) {
    if (!gUsingSndBigFire) {
        if (*count > 0) {
            (*count)--;
        }
    }
}

void ObjectSetLights(s32 state) {
    s32 i;
    Gobj *gobj;
    s16 type;

    for (i = 0; i < gLevelGameObjCount; i++) {
        gobj = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (gobj != NULL && gobj->healthInfo->health > 0 && gobj->npcState == 4 && type != 0x14 &&
            type != 0x28 && type != 0x6D && type != 0x34) {
            ObjectSetLightState(gobj, state);
        }
    }
}

#ifdef NON_MATCHING
/* Branch layout: the original places both flag tests before the on/off bodies, and lays out the sector lookup differently */
static inline TerrSector *ObjectGetTerrSector(s32 sector) {
    TerrSector *ts;

    if (sector < gTerrSectorCount && sector >= 0) {
        ts = &gTerrSector[sector];
    } else {
        ts = NULL;
    }
    return ts;
}

/* Switch a light object on/off and update the lighting of the player's sector and its neighbours. */
void ObjectSetLightState(Gobj *gobj, s32 on) {
    s32 ref = gameObjectsInfo[gobj->id].refId;
    u8 *links = gLevData->sectors.links[gGabeSector];
    s32 i;
    u8 *p;
    TerrSector *ts;
    TerrSectorModel *model;
    s16 type;

    if (on) {
        if (gobj->flags0 & 2) {
            return;
        }
    } else if (!(gobj->flags0 & 2)) {
        return;
    }
    if (on) {
        gobj->flags0 |= 2;
        gobj->anim->frames = NULL;
        gobj->anim->unk00[9] = 0x17;
        if (gobj->anim->unk00[0xA] & 8) {
            EGSAnimHideDetailDef(gobj->anim);
        }
    } else {
        gobj->flags0 &= ~2;
        gobj->anim->unk00[9] = 0x40;
        if (gobj->anim->unk00[0xA] & 8) {
            EGSAnimShowDetailDef(gobj->anim);
        }
    }
    for (i = -1, p = links - 1;; i++, p++) {
        if (i == -1) {
            ts = ObjectGetTerrSector(gGabeSector);
        } else {
            ts = ObjectGetTerrSector(*p);
        }
        if (ts == NULL) {
            break;
        }
        model = ts->model;
        if (EGSDisplayFindSectorWithLight(model, ref) == 0) {
            type = GOBJ_TYPE(gobj->id);
            if (type != 0x14 && type != 0x28 && type != 0x6D && type != 0x34) {
                EGSDisplaySectorLight(model->display->lightSector, ref, on);
            } else {
                EGSDisplayKillAnimatedLight(model->display->lightSector, ref);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetLightState);
#endif

s32 ObjectSetSectorLighting(s32 sector) {
    TerrSector *ts;
    s32 lightSector;
    s32 i;
    Gobj *gobj;
    s32 type;
    s16 ref;

    if (sector < gTerrSectorCount && sector >= 0) {
        ts = &gTerrSector[sector];
    } else {
        ts = NULL;
    }
    lightSector = ts->model->display->lightSector;
    if (EGSDisplayGetSectorLightArray(lightSector)) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            gobj = gameObjectsInfo[i].runtime;
            if (gobj != NULL && gobj->npcState == 4 && !(gobj->flags0 & 2)) {
                type = GOBJ_TYPE(i);
                ref = gameObjectsInfo[gobj->id].refId;
                if (type != 0x14 && type != 0x28 && type != 0x6D && type != 0x34) {
                    EGSDisplaySectorLight(lightSector, ref, 0);
                } else {
                    EGSDisplayKillAnimatedLight(lightSector, ref);
                }
            }
        }
    }
    return 9;
}

/* An object of the given type within 0x80 of gobj: displayed ones first, then all. */
Gobj *iObjectFindNearbyLight(Gobj *gobj, s32 type) {
    VECTOR32 pos;
    VECTOR32 other;
    ObjectDisplay *display;
    s32 dist;
    ObjectDisplayNode *node;
    EGSFrame *frame;
    u16 id;
    Gobj *found;
    s32 i;
    s16 idx;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    CameraGetDisplay(gCamera, &display);
    for (node = display->anims; node != NULL; node = node->next) {
        id = node->anim->id;
        frame = node->anim->frame;
        if (id & 0x2000) {
            idx = id & 0x3FF;
            if (gobj->id != idx && GOBJ_TYPE(idx) == type) {
                found = gameObjectsInfo[idx].runtime;
                other.vx = frame->pos.vx;
                other.vy = frame->pos.vy;
                other.vz = frame->pos.vz;
                other.vy = -other.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&other, &pos, &dist);
                if (dist < 0x80) {
                    return found;
                }
            }
        }
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        found = gameObjectsInfo[i].runtime;
        if (found != NULL && i != gobj->id && GOBJ_TYPE(i) == type) {
            other.vx = found->anim->frame->pos.vx;
            other.vy = found->anim->frame->pos.vy;
            other.vz = found->anim->frame->pos.vz;
            other.vy = -other.vy;
            EGSGeometry_GetFuzzyDistancePoint2Point(&other, &pos, &dist);
            if (dist < 0x80) {
                return found;
            }
        }
    }
    return NULL;
}

u16 ObjectDoQuip(s32 id) {
    u16 msg = 0xFFFF;
    s32 xa = gameObjectsInfo[id].pathStart;

    if (xa > 100) {
        SysSoundQueueXA(gSysInfo.level, xa - 100);
        SysSoundPlayQueuedXA(-1, gPlayerOffset);
    } else if (xa > 0) {
        SysSoundQueueXA(gSysInfo.level, xa);
        msg = StringContactMeiXingMsg();
    }
    return msg;
}

ObjectCallback ObjectSetQuipObjectCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7CC;
    D_8010F7CC = cb;
    return old;
}

ObjectCallback ObjectSetSpecialObjectCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D0;
    D_8010F7D0 = cb;
    return old;
}

ObjectCallback ObjectSetSpecialAmbientCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D4;
    D_8010F7D4 = cb;
    return old;
}

static inline void ObjectSetAmbient2(s32 id, s32 path, GameObjectInfo *other) {
    GameObjectInfo *info = &gameObjectsInfo[id];

    if (path == other->pathStart) {
        info->flags24 |= 0x20;
    } else {
        info->flags24 &= ~0x20;
    }
}

#ifdef NON_MATCHING
/* count reload and per-branch info pointer copy differ (see ObjectUpdateAmbientSndObjectSetting for the matching pattern) */
void ObjectUpdateAmbientObjectSetting(s32 id, s32 type) {
    s32 i;
    s32 path = gameObjectsInfo[id].pathStart;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (i != GOBJ_ID_666 ? type == gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type
                             : type == GOBJ_ID_666) {
            ObjectSetAmbient2((s16)i, path, &gameObjectsInfo[i]);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectUpdateAmbientObjectSetting);
#endif

static inline void ObjectAmbientSndOn(s32 id, s32 path, GameObjectInfo *other) {
    GameObjectInfo *info = &gameObjectsInfo[id];

    if (path == other->pathStart) {
        info->flags24 |= 0x20;
    }
}

static inline void ObjectAmbientSndOff(s32 id, s32 path, GameObjectInfo *other) {
    GameObjectInfo *info = &gameObjectsInfo[id];

    if (path == other->pathStart) {
        info->flags24 &= ~0x20;
    }
}

/* Ambient sound objects come in pairs (0x68/0x69): the ones on the path switch on, the others off. */
void ObjectUpdateAmbientSndObjectSetting(s32 id, s32 type) {
    s32 i;
    s32 other = 0x68;
    s32 path = gameObjectsInfo[id].pathStart;

    if (type == 0x68) {
        other = 0x69;
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        if (i != GOBJ_ID_666 ? type == gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type : type == GOBJ_ID_666) {
            ObjectAmbientSndOn((s16)i, path, &gameObjectsInfo[i]);
        } else if (i != GOBJ_ID_666 ? other == gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type : other == GOBJ_ID_666) {
            ObjectAmbientSndOff((s16)i, path, &gameObjectsInfo[i]);
        }
    }
}

ObjectCallback ObjectSetAmbientSndIdxCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D8;
    D_8010F7D8 = cb;
    return old;
}

/* Blow up a breakable object: explosion damage, then (for falling kinds) knock it into the air. */
void func_8002CAB0(Gobj *gobj, s16 type, s32 from, u8 *intact) {
    VECTOR32 pos;
    VECTOR32 vel;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    DamageExplosion(from, &pos, 0);
    if (type == 0x3D || type == 0x66) {
        gobj->flags0 |= 8;
        MovementAddGobj(gobj);
        vel.vz = 0;
        vel.vx = 0;
        vel.vy = ((rand() % 10) * 327 + 0xCCC) << 5;
        ((ObjectMove *)gobj->move)->vel.vx += vel.vx;
        ((ObjectMove *)gobj->move)->vel.vy += vel.vy;
        ((ObjectMove *)gobj->move)->vel.vz += vel.vz;
        vel.vx = (((rand() % 15) << 12) / 360) << 12;
        vel.vy = 0;
        vel.vz = (((rand() % 15) << 12) / 360) << 12;
        ((ObjectMove *)gobj->move)->rotVel.vx += vel.vx;
        ((ObjectMove *)gobj->move)->rotVel.vy += vel.vy;
        ((ObjectMove *)gobj->move)->rotVel.vz += vel.vz;
        ObjectRemoveAndIgnoreBulletHoles(gobj);
        *intact = 0;
        SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
    }
}

void func_8002CD34(Gobj *gobj) {
    VECTOR32 offset;
    VECTOR32 pos;
    VECTOR32 firePos;
    VECTOR32 dir;
    struct EGSFrame *frame = gobj->anim->frame;
    s16 *m = (s16 *)frame;

    pos.vx = frame->pos.vx;
    pos.vy = frame->pos.vy;
    pos.vz = frame->pos.vz;
    pos.vy = -pos.vy;
    dir.vx = m[2];
    dir.vy = m[5];
    dir.vz = m[8];
    dir.vy = -dir.vy;
    offset.vx = EGSMathFIXMul(dir.vx, 0xC0);
    offset.vy = EGSMathFIXMul(dir.vy, 0xC0);
    offset.vz = EGSMathFIXMul(dir.vz, 0xC0);
    firePos.vx = pos.vx - offset.vx;
    firePos.vy = pos.vy - offset.vy;
    firePos.vz = pos.vz - offset.vz;
    ParticleFireShow(gobj->id, &firePos, 0x400, 1);
}

void func_8002CE20(Gobj *gobj) {
    VECTOR32 offset;
    VECTOR32 pos;
    VECTOR32 firePos;
    VECTOR32 dir;
    struct EGSFrame *frame;
    s16 *m;
    s32 count;

    if (gobj->flags1 & 0x7F) {
        frame = gobj->anim->frame;
        m = (s16 *)frame;
        pos.vx = frame->pos.vx;
        pos.vy = frame->pos.vy;
        pos.vz = frame->pos.vz;
        pos.vy = -pos.vy;
        dir.vx = m[2];
        dir.vy = m[5];
        dir.vz = m[8];
        dir.vy = -dir.vy;
        offset.vx = EGSMathFIXMul(dir.vx, 0xC0);
        offset.vy = EGSMathFIXMul(dir.vy, 0xC0);
        offset.vz = EGSMathFIXMul(dir.vz, 0xC0);
        firePos.vx = pos.vx - offset.vx;
        firePos.vy = pos.vy - offset.vy;
        firePos.vz = pos.vz - offset.vz;
        ParticleFireShow(gobj->id, &firePos, 0x400, 0);
        func_8002BE44(gobj, &D_8010FFA8, &D_8010FFAC);
        count = (gobj->flags1 & 0x7F) - 1;
        gobj->flags1 = (gobj->flags1 & 0x80) | (count & 0x7F);
    }
}

void func_8002CF58(Gobj *gobj) {
    if (!(gobj->flags1 & 0x7F)) {
        ParticleFireHide(gobj->id);
        func_8002BECC(gobj, &D_8010FFA8, D_8010FFAC);
        gobj->flags1 = (gobj->flags1 & 0x80) | 1;
    }
}

void func_8002CFC4(Gobj *gobj, u32 state) {
    GameObjectAnim *anim = gobj->anim;
    s32 ref = gameObjectsInfo[gobj->id].refId;
    Gobj *target;

    GOBJ_STATE(gobj) = state;
    if (anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(anim);
    }
    if (ref != -1) {
        target = gameObjectsInfo[ref].runtime;
        if (target->flags0 & 2) {
            SysPostMessage(0x14, 5, gobj->id, ref, 0, 0, 0, 0);
        }
        if (GOBJ_STATE(target) < state) {
            GOBJ_STATE(target) = state;
            if (GOBJ_TYPE(ref) == 0x34 && state == 0) {
                if (target->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(target->anim);
                }
                target->flags0 |= 0x80;
                GOBJ_HEALTH(target)->health = 0;
            }
            GOBJ_HEALTH(target)->unk0C = 0;
            GOBJ_HEALTH(target)->lastAttacker = gobj->id;
            DamageGOBJ(-1, gobj->id, target->id, GOBJ_HEALTH(target)->health, 0xF);
        }
    }
}

void func_8002D190(Gobj *gobj, s16 type) {
    GameObjectHealth *armor = gobj->healthInfo;
    s32 state;
    s32 old;
    Gobj *main;
    Gobj *part;
    struct EGSFrame *frame;
    s16 *m;
    VECTOR32 dir;
    VECTOR32 pos;
    s32 hp = armor->armor;
    s32 third = hp / 3;
    s32 twoThirds = hp * 2 / 3;

    if (armor->health < third) {
        state = 2;
    } else if (armor->health < twoThirds) {
        state = 1;
    } else {
        state = 0;
    }
    if (type == 0x2C) {
        main = gobj;
        part = ObjectMatchToRefID(main->id, 0x38);
        part->healthInfo->health = main->healthInfo->health;
    } else {
        main = gameObjectsInfo[gameObjectsInfo[gobj->id].refId].runtime;
        part = gobj;
        main->healthInfo->health = part->healthInfo->health;
    }
    old = GOBJ_STATE(main);
    if (state != old) {
        frame = main->anim->frame;
        m = (s16 *)frame;
        GOBJ_STATE(part) = state;
        GOBJ_STATE(main) = state;
        pos.vx = frame->pos.vx;
        pos.vy = frame->pos.vy;
        pos.vz = frame->pos.vz;
        pos.vy = -pos.vy;
        dir.vx = m[2];
        dir.vy = m[5];
        dir.vz = m[8];
        dir.vy = -dir.vy;
        if (old == 0) {
            func_8002CD34(main);
        }
    }
}


typedef struct ObjectKick {
    s32 up;
    s32 unk04;
    s32 spin;
} ObjectKick;

/* Destroy a two-part object (type 0x2C with its 0x38 part): explosions at both ends,
 * then throw the main part into the air with a random spin. */
void func_8002D328(Gobj *gobj, s16 type, s32 from) {
    VECTOR32 v;
    VECTOR32 pos;
    VECTOR32 at;
    VECTOR32 dir;
    Gobj *main;
    Gobj *part;
    ObjectKick *kick;
    s32 i;
    s16 ref;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    if (type == 0x2C) {
        main = gobj;
        part = ObjectMatchToRefID(main->id, 0x38);
        part->healthInfo->health = main->healthInfo->health;
        part->flags0 |= 0x80;
    } else {
        part = gobj;
        main = gameObjectsInfo[gameObjectsInfo[part->id].refId].runtime;
        main->healthInfo->health = part->healthInfo->health;
        main->flags0 |= 0x80;
    }
    if (GOBJ_STATE(main) != 0) {
        ParticleFireHide(main->id);
    }
    GOBJ_STATE(part) = 3;
    func_8002CFC4(main, 3);
    dir.vx = main->anim->frame->m[0][2];
    dir.vy = main->anim->frame->m[1][2];
    dir.vz = main->anim->frame->m[2][2];
    dir.vy = -dir.vy;
    v.vx = EGSMathFIXMul(dir.vx, 0xC0);
    v.vy = EGSMathFIXMul(dir.vy, 0xC0);
    v.vz = EGSMathFIXMul(dir.vz, 0xC0);
    at.vx = pos.vx + v.vx;
    at.vy = pos.vy + v.vy;
    at.vz = pos.vz + v.vz;
    ParticleExplosion(-1, &at, &gExploDir, 0x1000, 0);
    at.vx = pos.vx - v.vx;
    at.vy = pos.vy - v.vy;
    at.vz = pos.vz - v.vz;
    ParticleExplosion(-1, &at, &gExploDir, 0x1000, 0);
    DamageExplosion(from, &pos, 0);
    MovementAddGobj(main);
    main->flags0 |= 8;
    ObjectRemoveAndIgnoreBulletHoles(main);
    ObjectRemoveAndIgnoreBulletHoles(part);
    ref = gameObjectsInfo[main->id].refId;
    if (ref != -1) {
        ObjectRemoveAndIgnoreBulletHoles(gameObjectsInfo[ref].runtime);
    }
    {
        ObjectKick kicks[8] = {
            { 1, 0, 0x93000 },  { 1, 0, -0x93000 },      { 2, 0, 0xDB000 },  { 2, 0, 0x94000 },
            { 2, -0x4A000, 0x4A000 }, { 2, 0x4A000, -0x4A000 }, { 2, 0, -0x94000 }, { 2, 0, -0xDB000 },
        };

        i = (rand() & 7) * sizeof(ObjectKick);
        kick = (ObjectKick *)((u8 *)kicks + i);
        v.vz = 0;
        v.vx = 0;
        v.vy = kick->up * 0x1EB80;
        ((ObjectMove *)main->move)->vel.vx += v.vx;
        ((ObjectMove *)main->move)->vel.vy += v.vy;
        ((ObjectMove *)main->move)->vel.vz += v.vz;
        v.vx = 0;
        v.vy = 0;
        v.vz = kick->spin;
        ((ObjectMove *)main->move)->rotVel.vx += v.vx;
        ((ObjectMove *)main->move)->rotVel.vy += v.vy;
        ((ObjectMove *)main->move)->rotVel.vz += v.vz;
    }
    SysPostMessage(10, 4, main->id, main->id, 0, 0, 0, 0);
}

void func_8002D790(Gobj *gobj) {
    s16 *m = (s16 *)gobj->anim->frame;

    m[1] = 0;
    m[7] = 0;
    m[3] = 0;
    m[5] = 0;
    MatrixNormal_0(m, m);
}

void ANIMATEDObjDispatch(void) {
}

void EFFECTSObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;

    switch (msg->id) {
    case 6:
        gobj->flags0 |= 0x40;
        if (type != 0x75) {
            pos.vx = gobj->anim->frame->pos.vx;
            pos.vy = gobj->anim->frame->pos.vy;
            pos.vz = gobj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            if (type == 0x36) {
                ParticleFireShow(gobj->id, &pos, 0xC00, 1);
            } else if (type == 0x53) {
                ParticleFireShow(gobj->id, &pos, 0x1000, 2);
            }
        } else {
            if (D_8010FFB4 == 0) {
                GameCameraSetScrimScrollRate(gobj->flags1);
            }
            D_8010FFB4++;
        }
        break;
    case 5:
        gobj->flags0 &= ~0x40;
        if (type != 0x75) {
            ParticleFireHide(gobj->id, type, gobj); /* extra args only match register use */
        } else if (--D_8010FFB4 <= 0) {
            GameCameraSetScrimScrollRate(0);
            D_8010FFB4 = 0;
        }
        break;
    case 2:
        func_801486DC(gobj);
        break;
    }
}

/* first used by LASERObjDispatch below, but the original emitted it here */
static const char D_80010FD0[11] = "s/object.c";

/* Sparking electrical object: keeps re-posting message 10 to itself while active. */
void EFFECTS2ObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    VECTOR32 pos;
    VECTOR32 at;
    s32 color;

    switch (msg->id) {
    case 10:
        if (gobj->flags0 & 0x40) {
            pos.vx = gobj->anim->frame->pos.vx;
            pos.vy = gobj->anim->frame->pos.vy;
            pos.vz = gobj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            if (rand() & 1) {
                color = gExeTick ^ 1;
                color &= 1;
                SysSetSongVolPan(1, D_8010FF9C, gobj, -1, -1);
                ParticleSparkShower(&pos, 0x10, D_801052E0[color]);
            }
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        gobj->flags0 |= 0x40;
        at.vx = gobj->anim->frame->pos.vx;
        at.vy = gobj->anim->frame->pos.vy;
        at.vz = gobj->anim->frame->pos.vz;
        at.vy = -at.vy;
        if (D_8010FF98 != 0) {
            SysStopSong(1, D_8010FF9C);
        }
        D_8010FF9C = (s16)SysPlaySong(1, 0x18, gobj, 0);
        D_8010FF98++;
        SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        break;
    case 5:
        gobj->flags0 &= ~0x40;
        if (D_8010FF98 > 0 && --D_8010FF98 == 0) {
            SysStopSong(1, D_8010FF9C);
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = gobj->unk04;
        break;
    case 42:
        gobj->unk04 = *(s32 *)msg->data[0];
        break;
    case 2:
        func_80148780(gobj, msg);
        break;
    }
}

/* Fire objects: show the flames, sets the player on fire on contact (message 26). */
void FIREObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;
    s32 size;
    s32 kind;

    switch (msg->id) {
    case 10:
        if (!gUsingSndBigFire && (gobj->flags0 & 0x40)) {
            pos.vx = gobj->anim->frame->pos.vx;
            pos.vy = gobj->anim->frame->pos.vy;
            pos.vz = gobj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        CobjAddCollisionBoxGobj(gobj, 1, 0, 0, 1);
        pos.vx = gobj->anim->frame->pos.vx;
        pos.vy = gobj->anim->frame->pos.vy;
        pos.vz = gobj->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        gobj->flags0 = (gobj->flags0 & 0x7F) | 0x40;
        kind = 0;
        switch (type) {
        case 0x30:
            size = 0x999;
            break;
        case 0x25:
            size = 0x800;
            break;
        case 0x27:
            size = 0x4CC;
            break;
        case 0x5A:
            size = 0xC8;
            kind = 3;
            break;
        default:
            return;
        }
        ParticleFireShow(gobj->id, &pos, size, kind);
        func_8002BE44(gobj, &D_8010FFA0, &D_8010FFA4);
        if (!gUsingSndBigFire) {
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        break;
    case 5:
        CobjRemoveCollisionBoxGobj(gobj);
        gobj->unk04 = 0;
        gobj->flags0 &= ~0x40;
        if (type == 0x30 || type == 0x25 || type == 0x27 || type == 0x5A) {
            ParticleFireHide(gobj->id);
            func_8002BECC(gobj, &D_8010FFA0, D_8010FFA4);
        }
        break;
    case 26:
        if (gObjPlayer->healthInfo->health > 0 && !gPlayerOnFireFlag) {
            SysPostMessage(13, 4, GOBJ_ID_666, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 42:
        if (gobj->healthInfo->health > 0) {
            break;
        }
    case 20:
        gobj->flags23 |= 2;
        gobj->healthInfo->health = -1;
        break;
    case 2:
        func_80148938(gobj, type);
        break;
    }
}

void GENObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        CobjAddCollisionBoxGobj(gobj, 1, 0, 1, 0);
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(gobj);
        CobjRemoveCollisionBoxGobj(gobj);
        break;
    case 2:
        func_80148A00(gobj);
        break;
    }
}

/* Breakable glass and similar objects (windows, gas canisters). */
void GLASSObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;
    VECTOR32 player;

    switch (msg->id) {
    case 6:
        if (!(gobj->anim->unk00[0xA] & 8) && type != 0x56) {
            CobjAddWindowGobj(gobj, 1, 0, 0);
        } else {
            CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        }
        if (!(gobj->flags0 & 1)) {
            CobjAddCollisionBoxGobj(gobj, 1, 0, type != 0x56 || gSysInfo.level != 6, 1);
        }
        break;
    case 5:
        if (!(gobj->anim->unk00[0xA] & 8) && type != 0x56) {
            CobjRemoveWindowGobj(gobj);
        } else {
            CobjRemoveBulletCollisionBoxGobj(gobj);
        }
        if (!(gobj->flags0 & 1)) {
            CobjRemoveCollisionBoxGobj(gobj);
        }
        break;
    case 26:
        if (gobj->healthInfo->health > 0) {
            if (type == 0x20) {
                SysPostMessage(0x1A, 4, gobj->id, gPlayerOffset, 0, 0, 0, 0);
            } else if (type != 0x55) {
                pos.vx = gobj->anim->frame->pos.vx;
                pos.vy = gobj->anim->frame->pos.vy;
                pos.vz = gobj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                player.vx = gObjPlayer->anim->frame->pos.vx;
                player.vy = gObjPlayer->anim->frame->pos.vy;
                player.vz = gObjPlayer->anim->frame->pos.vz;
                player.vy = -player.vy;
                if (type == 0x2D ||
                    ((type == 0x37 || pos.vy < player.vy + 0x69) &&
                     ((gObjPlayer->cas->flags & 8) || gObjPlayer->cas->active == 8 || (gObjPlayer->cas->flags & 0x400)))) {
                    DamageGOBJ(-1, msg->from, gobj->id, gobj->healthInfo->health, 0xF);
                }
            }
        }
        break;
    case 42:
        if (gobj->healthInfo->health > 0) {
            break;
        }
        gobj->flags1 |= 0x80;
    case 13:
        if (gobj->healthInfo->health <= 0 && !(gobj->flags0 & 0x80)) {
            gobj->flags0 |= 0x80;
            if (!(gobj->anim->unk00[0xA] & 8)) {
                gobj->flags23 |= 2;
            } else {
                EGSAnimShowDetailDef(gobj->anim);
            }
            if (type == 0x70) {
                pos.vx = gobj->anim->frame->pos.vx;
                pos.vy = gobj->anim->frame->pos.vy;
                pos.vz = gobj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                DamageGasCloud(msg->from, &pos, 0);
            }
        }
        break;
    case 2:
        func_80148B1C(gobj, type);
        break;
    }
}

/* Laser tripwire: moves along its path and hurts the player on contact (message 26). */
void LASERObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    u8 done;

    switch (msg->id) {
    case 10:
        if (gobj->move != NULL && (gobj->flags0 & 0x40)) {
            if (PathHandleEx(gobj, (s32)&gObjPathScaleVector, 1, 1, -1, -1, &done) != 1) {
                EGSDebugAssert(1, 0, D_80010FD0, 0x8BD);
            }
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        if (gobj->healthInfo->health > 0) {
            CobjAddCollisionBoxGobj(gobj, 1, 0, 0, 1);
            gobj->flags0 = (gobj->flags0 & 0x7F) | 0x40;
            if (gobj->move != NULL) {
                SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
            }
            SysPlaySong(0, 0xA, gobj, 0);
            D_8010FFB0++;
        }
        break;
    case 5:
        D_8010FFB0--;
        if (gobj->healthInfo->health > 0) {
            CobjRemoveCollisionBoxGobj(gobj);
            gobj->flags0 &= ~0x40;
        }
        if (D_8010FFB0 <= 0) {
            SysStopSong(0, 0xA);
            D_8010FFB0 = 0;
        }
        break;
    case 26:
        if (gobj->healthInfo->health > 0 && gObjPlayer->healthInfo->health > 0) {
            DamageGOBJ(-1, gobj->id, gPlayerOffset, gObjPlayer->healthInfo->armor + gObjPlayer->healthInfo->health,
                       0x11);
        }
        break;
    case 42:
        if (gobj->healthInfo->health > 0) {
            break;
        }
    case 20:
    case 21:
        if (!(gobj->anim->unk00[0xA] & 8)) {
            gobj->flags23 |= 2;
        } else {
            EGSAnimShowDetailDef(gobj->anim);
        }
        gobj->healthInfo->health = -1;
        break;
    case 2:
        func_80148EA8(gobj);
        break;
    }
}

/* MoveInfo velocity as seen by the falling light */
typedef struct LightMoveView {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ VECTOR32 vel;
} LightMoveView;

extern u32 D_801061C0; /* gWeaponInfo[0x13] word 0x14: bits 6..15 = blast damage */
void ParticleGlassPoint(s16 id, s16 gobjId, VECTOR32 *pos);
void ParticleFire(s16 id, VECTOR32 *pos, s32 arg2, s32 color, s32 arg4, s32 arg5, s32 arg6);
s32 DamageRadius(s16 id, VECTOR32 *pos, s32 damage, s32 inner, s32 outer, s32 kind, u8 self);
s32 PathGetNumPoints(Gobj *gobj);
void PathGetPoint(Gobj *gobj, s32 index, VECTOR32 *out);
s32 EGSDisplayGetAnimSector(GameObjectAnim *anim);
void func_80148F20(Gobj *gobj, s16 type);

static inline u8 LightHasLanded(MoveInfo *move) {
    if (!((move->unk100[8] != 0 && (move->collFlags & 0x100000)) || move->unk100[1] == 0)) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vx < -0x2000 || ((LightMoveView *)move)->vel.vx > 0x2000) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vy < -0x2000 || ((LightMoveView *)move)->vel.vy > 0x2000) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vz < -0x2000 || ((LightMoveView *)move)->vel.vz > 0x2000) {
        return 0;
    }
    return 1;
}

/* Lights: flickering lamps (0x6B, 0x6F), shootable lamps, falling lamps (0x6D). */
void LIGHTObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;
    VECTOR32 other;
    s32 dist;
    ActivationList *list;
    s32 i;
    Gobj *obj;
    u8 done;
    u8 landed;

    switch (msg->id) {
    case 10:
        if (type == 0x6B || type == 0x6F) {
            if ((*(u32 *)gobj & 0xC8) == 0x48 && gobj->healthInfo->health > 0) {
                if (--gobj->flags1 == 0) {
                    gobj->flags1 = gobj->unk04;
                    ObjectSetLightState(gobj, ((gobj->flags0 >> 1) ^ 1) & 1);
                    if (gobj->anim->unk00[0xA] & 8) {
                        if (!(gobj->flags0 & 2)) {
                            EGSAnimShowDetailDef(gobj->anim);
                        } else {
                            EGSAnimHideDetailDef(gobj->anim);
                        }
                    }
                }
                SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
            }
        } else if (type == 0x6D) {
            done = 0;
            pos = gobj->move->pos;
            list = &objActivationList[gobj->unk04];
            for (i = 0; i < list->count; i++) {
                obj = list->infos[i]->runtime;
                if (obj->healthInfo->health > 0) {
                    other.vx = obj->anim->frame->pos.vx;
                    other.vy = obj->anim->frame->pos.vy;
                    other.vz = obj->anim->frame->pos.vz;
                    other.vy = -other.vy;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &other, &dist);
                    if (dist < 0x60) {
                        DamageGOBJ(-1, msg->from, obj->id, obj->healthInfo->health, 0x11);
                    }
                }
            }
            if ((gExeTick & 3) == (gobj->id & 3)) {
                VECTOR32 at;
                VECTOR32 dir = {0, 0, 0, 0};

                at.vx = gobj->anim->frame->pos.vx;
                at.vy = gobj->anim->frame->pos.vy;
                at.vz = gobj->anim->frame->pos.vz;
                at.vy = -at.vy;
                ParticleExplosion(-1, &at, &dir, 0x800, 0);
            }
            landed = LightHasLanded(gobj->move);
            if (landed) {
                VECTOR32 hit;

                if (gobj->flags0 & 2) {
                    ObjectSetLightState(gobj, 0);
                }
                gobj->flags0 &= ~8;
                MovementRemoveGobj(gobj);
                hit.vx = gobj->anim->frame->pos.vx;
                hit.vy = gobj->anim->frame->pos.vy;
                hit.vz = gobj->anim->frame->pos.vz;
                hit.vy = -hit.vy;
                ParticleGlassPoint(-1, gobj->id, &hit);
                ParticleFire(-1, &hit, 0x1000, -1, 0, 0xA, 0x1E);
                DamageRadius(-1, &hit, (D_801061C0 >> 6) & 0x3FF, 0x140, 0x140, 0x11, 1);
                GameCameraShakeStart(NULL, 5, 1, &gExploShakeVector, 0, &hit);
                gobj->flags23 |= 2;
                done = 1;
            }
            if (!done) {
                SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
            }
        }
        break;
    case 6:
        gobj->flags0 |= 0x40;
        if (type != 0x6F) {
            if (type != 0x34) {
                CobjAddWindowGobj(gobj, 1, 0, 0);
            } else {
                CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
            }
            if (type == 0x5E) {
                CobjAddCollisionBoxGobj(gobj, 1, 0, 1, 1);
            }
        }
        if ((type == 0x6B || type == 0x6F) && (*(u32 *)gobj & 0xC8) == 0x48 && gobj->healthInfo->health > 0) {
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        break;
    case 5:
        gobj->flags0 &= ~0x40;
        if (type != 0x6F) {
            if (type != 0x34) {
                CobjRemoveWindowGobj(gobj);
            } else {
                CobjRemoveBulletCollisionBoxGobj(gobj);
            }
            if (type == 0x5E) {
                CobjRemoveCollisionBoxGobj(gobj);
            }
        }
        break;
    case 20:
    case 21:
        if (gobj->healthInfo->health > 0) {
            if (gobj->flags0 & 2) {
                ObjectSetLightState(gobj, 0);
            }
            if (type != 0x6F) {
                if (!(gobj->flags0 & 2)) {
                    DamageGOBJ(-1, msg->from, gobj->id, gobj->healthInfo->health, 0xF);
                }
            } else {
                gobj->flags0 &= ~8;
                if (gobj->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(gobj->anim);
                }
            }
        }
        break;
    case 26:
        if (gobj->healthInfo->health > 0 && type == 0x5E) {
            DamageGOBJ(-1, msg->from, gobj->id, gobj->healthInfo->health, 0xF);
        }
        break;
    case 41:
        if (gobj->healthInfo->health > 0 && type == 0x6B) {
            *(s32 *)msg->data[0] = gobj->flags0 & 8;
        }
        break;
    case 42:
        if (gobj->healthInfo->health > 0) {
            if (type == 0x6B) {
                if (*(s32 *)msg->data[0] != 0) {
                    gobj->flags0 |= 8;
                } else if (gobj->anim->unk00[0xA] & 8) {
                    ObjectSetLightState(gobj, 0);
                    EGSAnimShowDetailDef(gobj->anim);
                }
            }
            break;
        }
        gobj->flags1 |= 0x80;
    case 13:
        if (gobj->healthInfo->health <= 0 && !(gobj->flags0 & 0x80)) {
            if (type != 0x34) {
                gobj->flags0 |= 0x80;
                if (type != 0x6D || msg->id != 13) {
                    if (gobj->flags0 & 2) {
                        ObjectSetLightState(gobj, 0);
                    }
                }
                if (type == 0x6B) {
                    gobj->flags0 &= ~8;
                }
                if (type == 0x6D && msg->id != 42) {
                    if (!(gobj->flags0 & 8)) {
                        VECTOR32 at;

                        gobj->flags0 |= 8;
                        MovementAddGobj(gobj);
                        PathGetPoint(gobj, PathGetNumPoints(gobj) - 1, &at);
                        gobj->move->unk12C = at.vy;
                        gobj->unk04 = EGSDisplayGetAnimSector(gobj->anim);
                        SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
                    }
                } else {
                    gobj->flags23 |= 2;
                }
            } else if (msg->id != 42) {
                if (gobj->flags0 & 2) {
                    ObjectSetLightState(gobj, 0);
                }
                if (gobj->unk04 == 0) {
                    gobj->unk04 = 1;
                    if (gobj->anim->unk00[0xA] & 8) {
                        EGSAnimShowDetailDef(gobj->anim);
                    }
                    if (gobj->unk04 == 0) {
                        GOBJ_HEALTH(gobj)->health = GOBJ_HEALTH(gobj)->armor;
                    } else {
                        gobj->flags0 |= 0x80;
                    }
                }
            } else {
                if (gobj->flags0 & 2) {
                    ObjectSetLightState(gobj, 0);
                }
                gobj->flags0 |= 0x80;
                if (gobj->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(gobj->anim);
                }
            }
        } else {
            gobj->flags23 |= 2;
        }
        break;
    case 2:
        func_80148F20(gobj, type);
        break;
    }
}

void ROTATINGObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    VECTOR32 rotVel;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        gobj->flags0 &= 0x7F;
        MovementAddGobj(gobj);
        rotVel.vy = 0;
        rotVel.vx = 0;
        rotVel.vz = 0x2AAAAA;
        ((MoveInfoRot *)gobj->move)->rotVel = rotVel;
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(gobj);
        MovementRemoveGobj(gobj);
        break;
    case 2:
        func_80149280(gobj);
        break;
    }
}

/* Special trigger objects (quips, ambient sounds, end of level, pointers...). Message 18: triggered. */
void SPECIALObjDispatch(SysMsg *msg) {
    s32 id = msg->to;
    GameObjectInfo *info = &gameObjectsInfo[(s16)msg->to];
    s32 type;
    s32 kind;
    s32 song;
    s32 path;
    VECTOR32 pos;

    switch (msg->id) {
    case 18:
        type = GOBJ_TYPE(id);
        switch (type) {
        case 0x5D:
            if (!(info->flags24 & 0x20) && (D_8010F7CC == NULL || ((u8(*)())D_8010F7CC)((s16)id, msg))) {
                gAddingObjectiveViaQuip = 0;
                info->flags24 |= 0x20;
                ObjectDoQuip(id);
            }
            break;
        case 0x5F:
        case 0x22:
            if (!(info->flags24 & 0x20)) {
                info->flags24 |= 0x20;
                if (D_8010F7D0 != NULL) {
                    D_8010F7D0((s16)id);
                }
            }
            break;
        case 0x69:
            song = gameObjectsInfo[id].pathStart - 1;
            if (song < 0 && D_8010F7D8 != NULL) {
                song = ((s32(*)())D_8010F7D8)((s16)id, msg);
            }
            if (song >= 0) {
                ObjectUpdateAmbientSndObjectSetting(id, type);
                SysStopSong(0, song);
            }
            break;
        case 0x68:
            song = gameObjectsInfo[id].pathStart - 1;
            if (song < 0 && D_8010F7D8 != NULL) {
                song = ((s32(*)())D_8010F7D8)((s16)id, msg);
            }
            if (song >= 0) {
                ObjectUpdateAmbientSndObjectSetting(id, type);
                pos.vx = gameObjectsInfo[(s16)id].matrix.t[0];
                pos.vy = gameObjectsInfo[(s16)id].matrix.t[1];
                pos.vz = gameObjectsInfo[(s16)id].matrix.t[2];
                SysPlaySong(0, song, NULL, (s32)&pos);
            }
            break;
        case 0xE:
            if (!(info->flags24 & 0x20) && LevelDoEOL(0)) {
                info->flags24 |= 0x20;
            }
            break;
        case 0x73:
            ObjectUpdateAmbientObjectSetting(id, 0x73);
            if (D_8010F7D4 != NULL) {
                D_8010F7D4((s16)id, 0x73);
            }
            break;
        case 0x18:
            if (LevelGetDirectionPointerFlag()) {
                LevelSetPointer(id);
            }
            break;
        case 0x74:
            path = gameObjectsInfo[id].pathStart;
            ObjectUpdateAmbientObjectSetting(id, 0x74);
            if (path != 1) {
                GameCameraSetScrimOnFlag(1);
            } else {
                GameCameraSetScrimOnFlag(0);
            }
            break;
        }
        break;
    case 45:
        kind = GOBJ_TYPE(id);
        if ((kind == 0x5F || kind == 0x22) && gObjectMovieDoneCallback != NULL && (info->flags24 & 0x20)) {
            gObjectMovieDoneCallback((s16)id, msg);
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = (s16)info->flags24;
        break;
    case 42:
        kind = GOBJ_TYPE(id);
        info->flags24 = *(u8 *)msg->data[0];
        if ((info->flags24 & 0x20) && ((u32)(kind - 0x68) < 2 || kind == 0x74)) {
            SysPostMessage(18, 3, id, id, 0, 0, 0, 0);
        }
        break;
    case 2:
        info->flags24 &= ~0x20;
        break;
    }
}

void DamageGOBJHierarchy(s16 id, s16 from);
void func_80149BD8(Gobj *gobj, s16 type);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
extern Gobj *gObjPlayer;

static inline u8 TGTHasSettled(MoveInfo *move) {
    if (!((move->unk100[8] != 0 && (move->collFlags & 0x100000)) || move->unk100[1] == 0)) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vx < -0x2000 || ((LightMoveView *)move)->vel.vx > 0x2000) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vy < -0x2000 || ((LightMoveView *)move)->vel.vy > 0x2000) {
        return 0;
    }
    if (((LightMoveView *)move)->vel.vz < -0x2000 || ((LightMoveView *)move)->vel.vz > 0x2000) {
        return 0;
    }
    if (move->unkA0.vx < -0x2000 || move->unkA0.vx > 0x2000) {
        return 0;
    }
    if (move->unkA0.vy < -0x2000 || move->unkA0.vy > 0x2000) {
        return 0;
    }
    if (move->unkA0.vz < -0x2000 || move->unkA0.vz > 0x2000) {
        return 0;
    }
    return 1;
}

/* Targets: falling and breakable objects (0x2C falling, 0x3D/0x66 falling debris, ...). */
void TGTObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s16 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;
    VECTOR32 at;
    VECTOR32 hit;
    s32 dist;
    u8 intact;
    u8 done;
    Gobj *ref;
    Gobj *other;
    s16 rid;

    switch (msg->id) {
    case 10:
        if (!gUsingSndBigFire && (*(u32 *)gobj & 0x48) == 0x40 && type == 0x2C) {
            pos.vx = gobj->anim->frame->pos.vx;
            pos.vy = gobj->anim->frame->pos.vy;
            pos.vz = gobj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        } else if (gobj->move != NULL && (gobj->flags0 & 8)) {
            done = 0;
            if (type == 0x3D || type == 0x66) {
                if ((gExeTick & 3) == (gobj->id & 3)) {
                    at.vx = gobj->anim->frame->pos.vx;
                    at.vy = gobj->anim->frame->pos.vy;
                    at.vz = gobj->anim->frame->pos.vz;
                    at.vy = -at.vy;
                    ParticleExplosion(-1, &at, &gExploDir, 0x800, 0);
                }
                pos = ((LightMoveView *)gobj->move)->vel;
                if (pos.vy <= 0) {
                    hit.vx = gobj->anim->frame->pos.vx;
                    hit.vy = gobj->anim->frame->pos.vy;
                    hit.vz = gobj->anim->frame->pos.vz;
                    hit.vy = -hit.vy;
                    ParticleExplosion(-1, &hit, &gExploDir, 0x1000, 0);
                    done = 1;
                    MovementRemoveGobj(gobj);
                    gobj->flags23 |= 2;
                    gobj->flags0 &= ~8;
                }
            } else if (type == 0x2C) {
                pos.vx = gobj->anim->frame->pos.vx;
                pos.vy = gobj->anim->frame->pos.vy;
                pos.vz = gobj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                if ((gExeTick & 3) == (gobj->id & 3)) {
                    ParticleExplosion(-1, &pos, &gExploDir, 0x1000, 0);
                }
                at = gObjPlayer->move->pos;
                EGSGeometry_GetFuzzyXZDistancePoint2Point(&pos, &at, &dist);
                if ((u32)dist < 0x100 && gObjPlayer->healthInfo->health > 0 && !gPlayerOnFireFlag) {
                    SysPostMessage(13, 4, GOBJ_ID_666, gPlayerOffset, 0, 0, 0, 0);
                }
                if (TGTHasSettled(gobj->move)) {
                    ref = ObjectMatchToRefID(gobj->id, 0x38);
                    gobj->flags0 &= ~8;
                    gobj->unk20[0] |= 0x80;
                    ref->unk20[0] |= 0x80;
                    rid = gameObjectsInfo[gobj->id].refId;
                    if (rid != -1) {
                        other = gameObjectsInfo[rid].runtime;
                        other->unk20[0] |= 0x80;
                    } else {
                        other = NULL;
                    }
                    MovementRemoveGobj(gobj);
                    gobj->flags0 &= ~8;
                    func_8002CE20(gobj);
                    func_8002D790(gobj);
                    done = 1;
                    if (ref->id != gobj->id) {
                        func_8002D790(ref);
                    }
                    if (other != NULL) {
                        func_8002D790(other);
                    }
                }
            }
            if (!done) {
                SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
            }
        }
        break;
    case 6:
        gobj->flags0 |= 0x40;
        if ((*(u32 *)gobj & 0x88) == 0x80 && type == 0x2C) {
            func_8002CE20(gobj);
            SysPostMessage(10, 4, gobj->id, gobj->id, 0, 0, 0, 0);
        }
        CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        CobjAddCollisionBoxGobj(gobj, 1, 0, 1, type == 0x2C);
        break;
    case 5:
        gobj->flags0 &= ~0x40;
        if ((*(u32 *)gobj & 0x88) == 0x80 && type == 0x2C) {
            func_8002CF58(gobj);
        }
        CobjRemoveBulletCollisionBoxGobj(gobj);
        CobjRemoveCollisionBoxGobj(gobj);
        break;
    case 26:
        if ((gobj->flags0 & 8) && type == 0x2C && gNotKilledByFallingObject) {
            DamageGOBJ(-1, gobj->id, gPlayerOffset, gObjPlayer->healthInfo->armor + gObjPlayer->healthInfo->health,
                       0x11);
            gNotKilledByFallingObject = 0;
        }
        break;
    case 13:
        if (gobj->healthInfo->health <= 0) {
            if (!(gobj->flags0 & 0x80)) {
                intact = 1;
                gobj->flags0 |= 0x80;
                if (type == 0x45) {
                    intact = 0;
                    ((void (*)())ObjectElectronicDestruction)(gobj, msg->from);
                } else if (type == 0x21 || type == 0x66 || type == 0x67 || type == 0x3D) {
                    func_8002CAB0(gobj, type, msg->from, &intact);
                } else if (type == 0x2C || type == 0x38) {
                    intact = 0;
                    func_8002D328(gobj, type, msg->from);
                } else if (type == 0x44) {
                    rid = gameObjectsInfo[gobj->id].refId;
                    if (rid != -1) {
                        gameObjectsInfo[rid].runtime->flags1 &= ~7;
                        gameObjectsInfo[rid].runtime->flags1 |= 1;
                        SysPostMessage(10, 4, gobj->id, rid, 0, 0, 0, 0);
                    }
                    intact = 0;
                } else {
                    if (type == 0x42) {
                        gameObjectsInfo[(s16)gameObjectsInfo[gobj->id].refId].runtime->healthInfo->health = 1;
                    }
                    DamageGOBJHierarchy(gobj->id, msg->from);
                }
                if (intact) {
                    if (!(gobj->anim->unk00[0xA] & 8)) {
                        gobj->flags23 |= 2;
                        ObjectRemoveAndIgnoreBulletHoles(gobj);
                    } else {
                        EGSAnimShowDetailDef(gobj->anim);
                    }
                }
            }
        } else if (type == 0x2C || type == 0x38) {
            func_8002D190(gobj, type);
        } else if (type != 0x21 && !(type == 0x66 || type == 0x67)) {
            DamageGOBJHierarchy(gobj->id, msg->from);
        }
        break;
    case 42:
        if (gobj->healthInfo->health <= 0) {
            gobj->flags0 |= 0x80;
            if (!(gobj->anim->unk00[0xA] & 8)) {
                gobj->flags23 |= 2;
                ObjectRemoveAndIgnoreBulletHoles(gobj);
            } else {
                EGSAnimShowDetailDef(gobj->anim);
            }
        }
        break;
    case 2:
        func_80149BD8(gobj, type);
        break;
    }
}
