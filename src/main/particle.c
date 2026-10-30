#include "common.h"
#include "game.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

/* particle.c's small data, still assembled from asm */
SDATA_LOCAL(D_80110058)
SDATA_LOCAL(D_8011005C)
SDATA_LOCAL(D_8011009C)
SDATA_LOCAL(D_80110060)
SDATA_LOCAL(D_80110070)
SDATA_LOCAL(D_80110074)
SDATA_LOCAL(D_80110078)
SDATA_LOCAL(D_8011007C)
SDATA_LOCAL(D_8011006C)
SDATA_LOCAL(D_80110064)
SDATA_LOCAL(D_80110068)
SDATA_LOCAL(D_80110098)
SDATA_LOCAL(particleFireWaveFlag)
SDATA_LOCAL(D_80110088)
SDATA_LOCAL(D_80110090)
SDATA_LOCAL(D_80110094)
SDATA_LOCAL(D_801100A4)
SDATA_LOCAL(D_801100A8)
SDATA_LOCAL(D_801100AC)
SDATA_LOCAL(D_801100B0)
SDATA_LOCAL(D_801100B4)
SDATA_LOCAL(D_801100B8)
SDATA_LOCAL(D_801100BC)
SDATA_LOCAL(D_801100C0)
SDATA_LOCAL(D_801100C4)
SDATA_LOCAL(D_801100C8)
SDATA_LOCAL(D_801100E0)
SDATA_LOCAL(D_801100E4)
SDATA_LOCAL(D_801100EC)
SDATA_LOCAL(D_8011003C)
SDATA_LOCAL(D_80110040)
SDATA_LOCAL(D_80110044)
SDATA_LOCAL(D_80110048)
SDATA_LOCAL(group)
SDATA_LOCAL(groupUpdateList)
SDATA_LOCAL(particleBreathFlag)
SDATA_LOCAL(particleTaserState)

/* A group of particles sharing an emitter (0x34 bytes, group[] has D_80110060 of them). */
typedef struct ParticleGroup {
    /* 0x00 */ u32 fade; /* 0x80000000 | fade steps when fading out */
    /* 0x04 */ u32 flags;
    /* 0x08 */ s16 x; /* emitter position */
    /* 0x0A */ s16 y;
    /* 0x0C */ s16 z;
    /* 0x0E */ u8 unk0E[8];
    /* 0x16 */ s16 unk16;
    /* 0x18 */ u8 unk18[2];
    /* 0x1A */ s16 wind;
    /* 0x1C */ s16 unk1C;
    /* 0x1E */ s16 head; /* first particle index, -1 when the group is free */
    /* 0x20 */ s16 id;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 unk23;
    /* 0x24 */ u8 unk24;
    /* 0x25 */ u8 unk25;
    /* 0x26 */ s8 unk26;
    /* 0x27 */ u8 unk27;
    /* 0x28 */ u8 index; /* own index in group[] */
    /* 0x29 */ u8 slot; /* index in groupUpdateList */
    /* 0x2A */ u8 unk2A[2];
    /* 0x2C */ s32 updateType; /* index into updateFunc[] */
    /* 0x30 */ s32 drawType; /* index into drawFunc[] */
} ParticleGroup; /* size 0x34 */

/* One particle of the pool (0x68 bytes, PARTICLE_MAX of them). */
typedef struct Particle {
    /* 0x00 */ VECTOR32 pos; /* << 12 */
    /* 0x10 */ VECTOR32 vel;
    /* 0x20 */ u8 unk20[4];
    /* 0x24 */ s16 state; /* PARTICLE_FREE when unused */
    /* 0x26 */ s16 next; /* next particle index in the group, -1 at the end */
    /* 0x28 */ u8 prim[4]; /* display primitive starts here */
    /* 0x2C */ s32 depth;
    /* 0x30 */ u8 unk30[8];
    /* 0x38 */ s32 xy0; /* screen x | y << 16 */
    /* 0x3C */ u8 unk3C[4];
    /* 0x40 */ s32 xy1;
    /* 0x44 */ u8 unk44[0xC];
    /* 0x50 */ s16 wx; /* world position */
    /* 0x52 */ s16 wy;
    /* 0x54 */ s16 wz;
    /* 0x56 */ u8 unk56[0xA];
    /* 0x60 */ s16 unk60;
    /* 0x62 */ s16 unk62;
    /* 0x64 */ s16 unk64;
    /* 0x66 */ u8 unk66[2];
} Particle; /* size 0x68 */

#define PARTICLE_MAX 160
#define PARTICLE_FREE -0x8000

#define PARTICLE_UPDATE_NONE 10
#define PARTICLE_DRAW_NONE 5

extern u8 D_80110058; /* fire light fade */
extern u8 D_8011005C;
extern s32 D_8011009C;
extern s32 D_80110060; /* number of groups */
extern s32 D_80110070; /* groupUpdateList count */
extern s32 D_80110074; /* lowest possibly free particle index */
extern s32 D_80110078; /* groups in use */
extern s32 D_8011007C; /* particles in use */
extern ParticleGroup *D_8011006C; /* snow group */
extern s32 D_80110088; /* default floor height */
extern s32 D_80110090; /* group allocation cursor */
extern ParticleGroup *D_80110094; /* group carrying the fire light */
extern s32 D_801100A4; /* emitter mode: 0 point, 1 mask */
extern s32 D_801100A8;
extern s32 D_801100AC;
extern s32 D_801100B0;
extern s32 D_801100B4;
extern s32 D_801100B8; /* life */
extern s32 D_801100BC; /* color */
extern s32 D_801100C0; /* color fade */
extern s32 D_801100C4; /* direction random range >> 8 */
extern s32 D_801100C8; /* direction speed */
extern GameObjectRuntime *D_801100E0;
extern u8 D_801100E4; /* taser target bone */
extern u8 D_801100EC;
extern s32 D_8011003C; /* wind phases */
extern s32 D_80110040;
extern s32 D_80110044; /* rain wind phase */
extern s32 D_80110048;
extern ParticleGroup *group;
extern u8 *groupUpdateList;
extern s32 particleBreathFlag;
extern s32 particleTaserState;
extern u8 particleFireWaveFlag;
extern s32 D_80110064;
extern s32 D_80110068;
extern ParticleGroup *D_80110098;

extern Particle D_80119040[PARTICLE_MAX];
extern VECTOR32 D_80012000; /* {0, 0, 0} */
extern VECTOR32 D_80012010; /* {0, 0x1000, 0} */
extern VECTOR32 D_80012030; /* {7, 7, 7} */
extern VECTOR32 D_80012050; /* {0x3FF, 0x1FF, 0x3FF} */
extern VECTOR32 D_8011D140; /* NPC fire position followed by the fire light */
extern VECTOR32 D_8011D150; /* emitter position */
extern VECTOR32 D_8011D170; /* emitter mask size */
extern VECTOR32 D_8011D180; /* direction base */
extern s32 D_8011D1C0; /* wind x */
extern s32 D_8011D1C8; /* wind z */

extern u8 D_801062C4[]; /* per level: number of levelFloorY entries */
extern s16 levelFloorY[][7];

extern void (*updateFunc[])(ParticleGroup *);
extern void (*drawFunc[])(ParticleGroup *);

typedef struct PointSourceLight {
    /* 0x00 */ s32 on;
    /* 0x04 */ u8 unk04[0x28];
    /* 0x2C */ EGSFrame *frame;
} PointSourceLight;
extern PointSourceLight D_80118FE0[2];

extern s32 gSniperMode;
extern s32 gCurrentWeapon;

/* View of the player's GameObjectAnim used by the fence sparks. */
typedef struct PlayerAnimView {
    /* 0x00 */ u8 unk00[0xB];
    /* 0x0B */ u8 flags0B;
    /* 0x0C */ u8 unk0C[0xA];
    /* 0x16 */ s16 unk16;
} PlayerAnimView;

/* View of GameObjectAnim's per-bone frame table. */
typedef struct AnimFramesView {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ EGSFrame **frames;
} AnimFramesView;

/* View of TargetInfo's aim position. */
typedef struct TargetPosView {
    /* 0x00 */ u8 unk00[0x74];
    /* 0x74 */ VECTOR32 pos;
} TargetPosView;

extern GameObjectRuntime *gObjPlayer;
extern s16 gTaserGobjID;

int rand(void);
int rsin(int);
void ParticleFree(ParticleGroup *g, Particle *p, s32 prev, s32 index);
void ParticleGroupFree(ParticleGroup *g);
ParticleGroup *ParticleFindGroup(s32 id);
ParticleGroup *ParticlePoolAllocate(s32 count, ParticleGroup *g);
void ParticleStart(ParticleGroup *g, s32 count);
ParticleGroup *ParticleSparkShower(VECTOR32 *pos, s32 size, s32 color);
ParticleGroup *ParticleHitWall(s32 arg0, VECTOR32 *pos, VECTOR32 *normal, s32 color);
s32 ParticleFindFloor(s32 y);
void ParticleInitLifeColorFade(s32 life, s32 color);
void ParticleInitEmitterPoint(VECTOR32 *pos);
void ParticleInitEmitterMask(VECTOR32 *pos, VECTOR32 *mask);
void ParticleInitAngle(s32 a, s32 b, s32 c);
void ParticleInitDirection(VECTOR32 *dir, s32 range, s32 speed);
void ParticleInitNegativeDirection(VECTOR32 *dir, s32 range, s32 speed);
void particleGroupDelete(ParticleGroup *g);
void particleGroupAdd(ParticleGroup *g);
void CameraGetDisplay(void *camera, void **display);
void EGSDisplayDeleteSprite(void *display, void *prim);
void EGSDisplayDeletePoly(void *display, void *prim);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);
int rcos(int);
ParticleGroup *ParticleHitSnow(s32 id, VECTOR32 *pos, s32 unused);
void ParticleGroupUpdateWind(ParticleGroup *g);
void ParticleInitLifeColorNoInfrared(s32 life, s32 color);
void ParticleInitLifeColor(s32 life, s32 color);
ParticleGroup *iParticleExplosion();
VECTOR32 *SniperGetTarget(s16 *buf);
void EGSDisplayTransformWorldToScreen(VECTOR32 *world, VECTOR32 *screen);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void EGSDisplayPointSourceOff(void *source);
void EGSDisplayPointSourceOn(void *source);
s32 func_80052D90(s32 x);
void EGSDisplayPointSourceInit(void *source);
void EGSDisplayPointSourceFrame(void *source, EGSFrame *frame);
void EGSDisplayPointSourceParams(void *source, s32 radius, s32 color);
void EGSFrameCreate(EGSFrame **frame);

void debug(void) {
}

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80011FE0);

void ParticleInit(void) {
    s32 i;

    D_80110060 = 40;
    if (gSysInfo.level == 15 || gSysInfo.level == 1) {
        D_80110060 = 80;
    }
    group = EGSMemMalloc(D_80110060 * sizeof(ParticleGroup));
#line 378
    ASSERT(group != NULL);
    groupUpdateList = EGSMemMalloc(D_80110060 + 1);
#line 380
    ASSERT(groupUpdateList != NULL);
    for (i = 0; i < D_80110060; i++) {
        group[i].head = -1;
        group[i].unk24 = 0xFF;
        group[i].unk27 = 0;
        group[i].unk25 = 0;
        group[i].unk26 = 0;
        group[i].id = -1;
        group[i].unk16 = 0;
        group[i].index = i;
        groupUpdateList[i] = 0xFF;
    }
    groupUpdateList[D_80110060] = 0xFF;
    D_80110070 = 0;
    D_80110074 = 0;
    for (i = 0; i < PARTICLE_MAX; i++) {
        D_80119040[i].state = PARTICLE_FREE;
    }
    D_80110090 = 0;
    D_80110078 = 0;
    D_8011007C = 0;
    D_80110088 = 0;
    particleBreathFlag = 0;
    particleFireWaveFlag = 0;
    D_80110094 = NULL;
    D_80110098 = NULL;
    particleTaserState = 0;
    D_80110068 = 0;
    D_80110064 = 0;
    D_8011006C = NULL;
    for (i = 0; i < 2; i++) {
        EGSDisplayPointSourceInit(&D_80118FE0[i]);
        EGSFrameCreate(&D_80118FE0[i].frame);
        EGSDisplayPointSourceFrame(&D_80118FE0[i], D_80118FE0[i].frame);
        EGSDisplayPointSourceParams(&D_80118FE0[i], 100, 0x20FFFFFF);
    }
    SysPostMessage(0x10, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
}

void particleGroupAdd(ParticleGroup *g) {
    groupUpdateList[D_80110070] = g->index;
    g->slot = D_80110070;
    D_80110070++;
    D_80110078++;
}

void particleGroupDelete(ParticleGroup *g) {
    D_80110078--;
    D_80110070--;
    groupUpdateList[g->slot] = groupUpdateList[D_80110070];
    group[groupUpdateList[D_80110070]].slot = g->slot;
    groupUpdateList[D_80110070] = 0xFF;
}

void ParticleFree(ParticleGroup *g, Particle *p, s32 prev, s32 index) {
    void *display;

    CameraGetDisplay(gCamera, &display);
    if (g->updateType != 9) {
        if (g->drawType != 4) {
            EGSDisplayDeletePoly(display, p->prim);
        } else if (g->unk25) {
            EGSDisplayDeleteSprite(display, p->prim);
        }
    }
    p->state = PARTICLE_FREE;
    D_8011007C--;
    if (index == g->head) {
        g->head = p->next;
        if (g->head < 0) {
            /* divides by the size twice */
            D_80110090 = (g - group) / sizeof(ParticleGroup);
            g->unk16 = 0;
            g->unk27 = 0;
            g->unk25 = 0;
            g->unk26 = 0;
            g->drawType = PARTICLE_DRAW_NONE;
            if (D_80110094 == g) {
                D_80110094 = NULL;
            }
            g->id = -1;
            g->unk24 = 0xFF;
            g->flags = 0;
            particleGroupDelete(g);
        }
    } else {
        D_80119040[prev].next = p->next;
    }
    p->next = -1;
    if (index < D_80110074) {
        D_80110074 = index;
    }
}

void ParticleGroupFree(ParticleGroup *g) {
    s32 i;
    s32 next;
    Particle *p;

    for (i = g->head; i >= 0; i = next) {
        p = &D_80119040[i];
        next = p->next;
        ParticleFree(g, p, 0, i);
    }
}

ParticleGroup *ParticlePoolAllocate(s32 count, ParticleGroup *g) {
    ParticleGroup *found = NULL;
    s32 n = 0;
    s32 first = -1;
    s32 last = 0;
    s32 i;

    if (g == NULL) {
        if (D_80110078 < D_80110060) {
            for (i = 0; i < D_80110060; i++) {
                if (++D_80110090 >= D_80110060) {
                    D_80110090 = 0;
                }
                if (group[D_80110090].head == -1) {
                    found = &group[D_80110090];
                    break;
                }
            }
        }
    } else {
        found = g;
    }
    if (found != NULL) {
        if (count > 8 && D_8011007C + count > 0x80) {
            count >>= 2;
        }
        if (count > 3 && D_8011007C + count > 0x78) {
            count >>= 1;
        }
        for (i = D_80110074; i < PARTICLE_MAX; i++) {
            if (D_80119040[i].state == PARTICLE_FREE) {
                if (first < 0) {
                    first = i;
                } else {
                    D_80119040[last].next = i;
                }
                n++;
                D_8011007C++;
                last = i;
                if (n == count) {
                    break;
                }
            }
        }
        if (first >= 0) {
            if (g == NULL) {
                D_80119040[last].next = -1;
                particleGroupAdd(found);
            } else {
                D_80119040[last].next = found->head;
            }
            found->head = first;
            if (D_80110074 < last) {
                D_80110074 = last;
            }
        } else {
            return NULL;
        }
    }
    return found;
}

void ParticleInitEmitterPoint(VECTOR32 *pos) {
    D_801100A4 = 0;
    D_8011D150 = *pos;
}

void ParticleInitEmitterMask(VECTOR32 *pos, VECTOR32 *mask) {
    D_801100A4 = 1;
    D_8011D170 = *mask;
    D_8011D150.vx = pos->vx - (D_8011D170.vx >> 1);
    D_8011D150.vy = pos->vy - (D_8011D170.vy >> 1);
    D_8011D150.vz = pos->vz - (D_8011D170.vz >> 1);
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitEmitterXZPlane);

void ParticleInitAngle(s32 a, s32 b, s32 c) {
    D_801100A8 = a;
    D_801100AC = b;
    D_801100B0 = c;
}

void ParticleInitLifeColorNoInfrared(s32 life, s32 color) {
    D_801100B8 = life;
    D_801100BC = color;
    D_801100C0 = 0;
}

void ParticleInitLifeColor(s32 life, s32 color) {
    D_801100B8 = life;
    if (gSniperMode && gCurrentWeapon == 12) {
        /* infrared scope: grey of the same brightness */
        D_801100BC = ((s32)((((u32)color >> 16) & 0xFF) + ((color & 0xFF00) >> 8) + (color & 0xFF)) >> 2) << 8;
    } else {
        D_801100BC = color;
    }
    D_801100C0 = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitLifeColorFade);

void ParticleInitDirection(VECTOR32 *dir, s32 range, s32 speed) {
    s32 half = range >> 1;

    D_8011D180.vx = dir->vx - half;
    D_8011D180.vy = dir->vy - half;
    D_801100C4 = range >> 8;
    D_801100C8 = speed;
    D_8011D180.vz = dir->vz - half;
}

void ParticleInitNegativeDirection(VECTOR32 *dir, s32 range, s32 speed) {
    s32 half = range >> 1;

    D_8011D180.vx = -dir->vx - half;
    D_8011D180.vy = -dir->vy - half;
    D_801100C4 = range >> 8;
    D_801100C8 = speed;
    D_8011D180.vz = -dir->vz - half;
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStart);

void ParticleInitBreath(void) {
    particleBreathFlag = 1;
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleBreath);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDamageDelay);

void ParticleTaserRetract(void) {
    if (particleTaserState != 3) {
        particleTaserState = 3;
        if (D_80118FE0[0].on) {
            EGSDisplayPointSourceOff(&D_80118FE0[0]);
        }
    }
}

void ParticleTaserSetTarget(s32 arg0, s32 target) {
    if (target < 0) {
        D_801100E4 = 13;
    } else {
        D_801100E4 = target;
    }
}

void ParticleTaserGetDestination(GameObjectRuntime *target, VECTOR32 *out) {
    s16 tmp[4];

    if (D_801100EC) {
        *out = *SniperGetTarget(tmp);
    } else if (target != NULL) {
        out->vx = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vx;
        out->vy = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vy;
        out->vz = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vz;
        out->vy = -out->vy;
    } else {
        *out = ((TargetPosView *)D_801100E0->target)->pos;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTaser);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateTaser);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTracer);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012000);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012010);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFootSplash);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitGobj);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitFlakJacket);

INCLUDE_ASM("asm/main/nonmatchings/particle", iParticleExplosion);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleExplosion);

void ParticleSmoke(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4) {
    ParticleInitLifeColorNoInfrared(16, 0x808080);
    D_801100B4 = 16;
    iParticleExplosion(a0, a1, 2, a4, -1, a3, 0);
}

#ifdef NON_MATCHING
/* flag/a1 callee-saved registers swapped, life constant scheduled late */
ParticleGroup *ParticleFire(s32 a0, s32 a1, s32 a2, s32 color, s32 r, s32 gr, s32 b) {
    s32 flag = 1;
    ParticleGroup *g;

    if (r == 0) {
        flag = gr == 0;
    }

    ParticleInitLifeColorNoInfrared(16, color < 0 ? 0x808080 : color);
    D_801100B4 = 16;
    g = iParticleExplosion(a0, a1, 2, flag, GOBJ_ID_666, a2, 0);
    if (g != NULL) {
        if (r >= 0xFF) {
            r = 0xFE;
        }
        if (gr >= 0xFF) {
            gr = 0xFE;
        }
        if (b >= 0xFF) {
            b = 0xFE;
        }
        g->fade = 0x80000000 | ((r & 0xFF) << 16) | ((gr & 0xFF) << 8) | (b & 0xFF);
    }
    return g;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFire);
#endif

ParticleGroup *ParticleFindGroup(s32 id) {
    s32 i;
    ParticleGroup *found = NULL;

    for (i = 0; i < D_80110060; i++) {
        if (group[i].head >= 0 && group[i].id == id) {
            found = &group[i];
            break;
        }
    }
    return found;
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireShow);

void ParticleFireHide(s16 id) {
    ParticleGroup *g = ParticleFindGroup(id);
    s32 i;
    s32 next;
    Particle *p;

    if (g != NULL) {
        for (i = g->head; i >= 0; i = next) {
            p = &D_80119040[i];
            next = p->next;
            ParticleFree(g, p, 0, i);
        }
        g->id = -1;
    }
}

s32 ParticleSmokeFade(s16 id) {
    s32 ret = 0;
    ParticleGroup *g = ParticleFindGroup(id);

    if (g != NULL) {
        g->fade = (rand() & 0x1F) + 10 | 0x80000000;
        ret = 1;
    }
    return ret;
}

s32 ParticleFireFade(s16 id) {
    s32 ret = 0;
    ParticleGroup *g = ParticleFindGroup(id);

    if (g != NULL) {
        s32 r = rand();
        g->fade = ((r & 0x1F) + 10) << 8 | ((rand() & 0x1F) + 10) | 0x80000000;
        ret = 1;
    }
    return ret;
}

void ParticleNPCFireShow(s32 id, s32 a1, s32 a2) {
    s32 obj = id & 0xFFF;
    s32 taser = obj == gTaserGobjID;
    ParticleGroup *g;
    GameObjectRuntime *gobj;
    PlayerAnimView *anim;

    ParticleInitLifeColorNoInfrared(16, 0x808080);
    D_801100B4 = 16;
    g = iParticleExplosion(-1, a1, 2, taser, (s16)(id | 0x4000), a2, 0);
    if (g != NULL) {
        gobj = gameObjectsInfo[obj].runtime;
        if (taser == 1) {
            g->fade = 0x8001FF00;
        }
        g->unk27 = rand() & 0xF;
        anim = (PlayerAnimView *)gobj->anim;
        if (anim != NULL) {
            if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
                if (anim->unk16 == 1) {
                    g->unk26 = 0x27;
                } else {
                    g->unk26 = 0xF;
                }
            } else {
                g->unk26 = -9;
            }
        }
    }
}

void ParticleNPCFireHide(s32 id) {
    ParticleGroup *g = ParticleFindGroup((s16)(id | 0x4000));
    s32 i;
    s32 next;
    Particle *p;

    if (g != NULL) {
        for (i = g->head; i >= 0; i = next) {
            p = &D_80119040[i];
            next = p->next;
            ParticleFree(g, p, 0, i);
        }
        g->id = -1;
    }
}

void ParticleNPCFireFade(s32 id) {
    ParticleGroup *g = ParticleFindGroup((s16)(id | 0x4000));

    if (g != NULL && ((s32)g->fade >= 0 || (g->fade & 0xFF00) == 0xFF00)) {
        if ((id & 0xFFF) == gPlayerOffset) {
            g->fade = 0x8000001F;
        } else {
            s32 r = rand();
            g->fade = ((r & 0x1F) + 10) << 8 | ((rand() & 0x1F) + 10) | 0x80000000;
        }
    }
}

#ifdef NON_MATCHING
/* prologue: id copy scheduled before the pos copy */
s32 ParticleNPCFireMove(s32 id, VECTOR32 *pos) {
    s32 notFound = 0;
    ParticleGroup *g;
    s32 i;
    Particle *p;
    PlayerAnimView *anim;
    s32 kind;

    g = ParticleFindGroup((s16)(id | 0x4000));
    if (g != NULL) {
        i = g->head;
        if (g == D_80110094) {
            D_8011D140 = *pos;
        }
        for (; i >= 0; i = p->next) {
            p = &D_80119040[i];
            p->pos.vx += (pos->vx - g->x) >> 2;
            p->pos.vy += (pos->vy - g->y) >> 2;
            p->pos.vz += (pos->vz - g->z) >> 2;
        }
        anim = (PlayerAnimView *)gameObjectsInfo[id & 0xFFF].runtime->anim;
        if (anim != NULL) {
            if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
                if (anim->unk16 == 1) {
                    kind = 0x27;
                } else {
                    kind = 0xF;
                }
            } else {
                kind = -9;
            }
            g->unk26 = kind;
        }
        g->x = pos->vx;
        g->y = pos->vy;
        g->z = pos->vz;
    } else {
        notFound = 1;
    }
    return notFound;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleNPCFireMove);
#endif

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGrenadeLauncher);

#ifdef NON_MATCHING
/* load scheduling of the angle/velocity update differs */
void ParticleUpdateGrenadeLauncher(ParticleGroup *g) {
    Particle *p = &D_80119040[g->head];
    VECTOR32 v;
    s32 s;
    s32 color;
    s16 angle;

    angle = p->unk62 + p->unk64;
    p->unk62 = angle;
    p->pos.vx += p->vel.vx;
    p->pos.vy += p->vel.vy;
    p->pos.vz += p->vel.vz;
    s = g->wind * rsin(angle);
    v.vx = p->pos.vx >> 12;
    v.vy = (p->pos.vy + s) >> 12;
    v.vz = p->pos.vz >> 12;
    color = (((rand() & 0x7F) + 0x20) << 8) + 0xFF;
    ParticleHitWall(-1, &v, &p->vel, color);
    EGSDisplayPointSourceParams(&D_80118FE0[0], 100, color);
    EGSFrameSetPosition(D_80118FE0[0].frame, 0, &v);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateGrenadeLauncher);
#endif

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireWaveDeath);

ParticleGroup *ParticleHitWall(s32 arg0, VECTOR32 *pos, VECTOR32 *normal, s32 color) {
    VECTOR32 n;
    ParticleGroup *g;

    GTEMath_VectorNormal(normal, &n);
    ParticleInitEmitterPoint(pos);
    ParticleInitLifeColorFade((gSystemTick & 1) | 4, color);
    D_801100B4 = 32;
    ParticleInitNegativeDirection(&n, 0x7FFF, 0);
    g = ParticlePoolAllocate(8, NULL);
    if (g != NULL) {
        g->unk23 = 3;
        g->updateType = 8;
        g->unk1C = 0;
        g->wind = 0;
        g->drawType = 3;
        ParticleStart(g, 8);
    }
    return g;
}

void ParticleFuse(VECTOR32 *pos) {
    VECTOR32 dir = D_80012000;
    VECTOR32 mask = D_80012030;
    s32 c;
    ParticleGroup *g;

    ParticleInitEmitterMask(pos, &mask);
    c = (rand() & 0x3F) + 100;
    ParticleInitLifeColorFade(3, ((c << 16) + (c << 8)) | c);
    D_801100B4 = 0x430;
    ParticleInitDirection(&dir, 0x1FF, 0);
    g = ParticlePoolAllocate(4, NULL);
    if (g != NULL) {
        g->unk23 = 8;
        g->unk1C = 0;
        g->wind = 0;
        g->updateType = 10;
        g->drawType = 0;
        ParticleStart(g, 4);
        g->unk24 = 3;
    }
}

ParticleGroup *ParticleHitSnow(s32 id, VECTOR32 *pos, s32 unused) {
    s32 big = 0;
    ParticleGroup *g = NULL;
    VECTOR32 mask = D_80012030;
    VECTOR32 dir = D_80012010;
    VECTOR32 *pMask = &mask;
    VECTOR32 *pDir = &dir;
    s32 speed;
    s32 count;

    if (pos->vy >= D_80110088 - 0x40) {
        if (id != GOBJ_ID_666) {
            big = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == 3;
        }
        ParticleInitEmitterPoint(pos);
        ParticleInitLifeColor(16, 0xA08282);
        D_801100B4 = 0x692;
        speed = big + 3;
        ParticleInitDirection(pDir, 0x1FFF, speed);
        count = big * 8 + 8;
        g = ParticlePoolAllocate(count, NULL);
        if (g != NULL) {
            g->unk23 = 3;
            g->wind = -0x1000;
            g->updateType = 8;
            g->unk1C = 0;
            g->drawType = 3;
            ParticleStart(g, count);
        }
        ParticleInitEmitterMask(pos, pMask);
        D_801100B4 = 0x610;
        ParticleInitDirection(pDir, 0x1FFF, speed);
        ParticleInitLifeColorFade(4, 0xFFFFFF);
        g = ParticlePoolAllocate(count, NULL);
        if (g != NULL) {
            g->unk23 = 3;
            g->updateType = 8;
            g->unk1C = 0;
            g->wind = 0;
            g->drawType = 3;
            ParticleStart(g, count);
        }
    }
    return g;
}

void ParticleHitWallChopper(s32 id, VECTOR32 *pos) {
    ParticleHitSnow(id, pos, 0);
}

ParticleGroup *ParticleHitChopper(s32 arg0, VECTOR32 *pos, VECTOR32 *dir) {
    ParticleGroup *g;

    ParticleInitEmitterPoint(pos);
    ParticleInitLifeColorFade(20, 0x50B4FF);
    D_801100B4 = 16;
    ParticleInitNegativeDirection(dir, 0x7FFF, 3);
    g = ParticlePoolAllocate(20, NULL);
    if (g != NULL) {
        g->unk23 = 3;
        g->wind = -0x1000;
        g->updateType = 8;
        g->unk1C = 0;
        g->drawType = 3;
        ParticleStart(g, 20);
    }
    return g;
}

#ifdef NON_MATCHING
/* empty-count test is blez instead of beqz, registers differ */
s32 ParticleFindFloor(s32 y) {
    s32 floor;
    s32 i;
    u8 count;
    s16 *floors;

    if ((u16)gSysInfo.level < 16) {
        floor = y;
        i = 0;
        count = D_801062C4[gSysInfo.level];
        floors = levelFloorY[gSysInfo.level];
        for (; i < count; i++) {
            if (floors[i] - 0x80 < y) {
                floor = floors[i] - 0x60;
                break;
            }
        }
    } else {
        floor = D_80110088;
    }
    return floor;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFindFloor);
#endif

ParticleGroup *ParticleSparkShower(VECTOR32 *pos, s32 size, s32 color) {
    ParticleGroup *g = NULL;
    VECTOR32 dir = D_80012010;
    VECTOR32 mask;
    s32 floor;

    size--;
    mask.vz = size;
    mask.vy = size;
    mask.vx = size;
    if (D_8011007C + 16 < 0x78) {
        floor = ParticleFindFloor(pos->vy);
        ParticleInitEmitterMask(pos, &mask);
        ParticleInitLifeColorFade((rand() & 0x1F) + 4, color);
        D_801100B4 = 0x450;
        ParticleInitDirection(&dir, ((rand() & 3) << 15) | 0x7FFF, 0);
        g = ParticlePoolAllocate((rand() & 7) + 1, NULL);
        if (g != NULL) {
            g->unk23 = 2;
            g->wind = -0x2916;
            g->updateType = 8;
            g->unk1C = floor;
            g->drawType = 3;
            ParticleStart(g, (rand() & 7) + 1);
            g->unk26 = -20;
        }
    }
    return g;
}

void ParticleSparkShowerTime(VECTOR32 *pos, s32 size, s32 color, s32 count) {
    s32 i;
    ParticleGroup *g;

    for (i = 0; i < count; i++) {
        g = ParticleSparkShower(pos, size, color);
        if (g != NULL) {
            g->unk27 = i;
        }
    }
}

ParticleGroup *ParticleSparkSphere(VECTOR32 *pos, s32 color) {
    VECTOR32 dir = D_80012000;
    ParticleGroup *g;

    D_801100B4 = 0x4090;
    ParticleInitEmitterPoint(pos);
    ParticleInitAngle(0x555, 0, 0xFF);
    ParticleInitLifeColorFade(40, color);
    ParticleInitDirection(&dir, ((rand() & 3) << 15) | 0x7FFF, 0);
    g = ParticlePoolAllocate((rand() & 0xF) + 4, NULL);
    if (g != NULL) {
        g->unk23 = 8;
        g->wind = -0x148B;
        g->updateType = 10;
        g->drawType = 0;
        g->unk1C = D_80110088;
        ParticleStart(g, (rand() & 0xF) + 4);
        g->unk24 = 3;
    }
    return g;
}

ParticleGroup *ParticleWood(s32 arg0, VECTOR32 *pos, VECTOR32 *normal, s32 color) {
    VECTOR32 n;
    ParticleGroup *g;

    ParticleInitEmitterPoint(pos);
    GTEMath_VectorNormal(normal, &n);
    ParticleInitEmitterPoint(pos);
    ParticleInitLifeColorFade(5, color);
    D_801100B4 = 32;
    ParticleInitNegativeDirection(&n, 0x7FFF, 0);
    g = ParticlePoolAllocate(8, NULL);
    if (g != NULL) {
        g->unk23 = 3;
        g->updateType = 8;
        g->unk1C = 0;
        g->wind = 0;
        g->drawType = 3;
        ParticleStart(g, 8);
        ParticleInitAngle(0x555, 0, 0xFF);
        ParticleInitLifeColorFade(10, color);
        ParticleInitNegativeDirection(&n, ((rand() & 3) << 16) | 0xFFF, 0);
        g = ParticlePoolAllocate((rand() & 3) + 1, NULL);
        if (g != NULL) {
            g->unk23 = 10;
            g->wind = -0x2916;
            g->unk1C = 0;
            g->updateType = 10;
            g->drawType = 0;
            ParticleStart(g, (rand() & 3) + 1);
            g->unk24 = 3;
        }
    }
    return g;
}

void ParticleSparkFence(VECTOR32 *pos) {
    VECTOR32 dir;
    VECTOR32 target;
    s32 kind;
    ParticleGroup *g;
    PlayerAnimView *anim = (PlayerAnimView *)gObjPlayer->anim;

    if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
        if (anim->unk16 == 1) {
            kind = 0x34;
        } else {
            kind = 0x1C;
        }
    } else {
        kind = 4;
    }
    target = *(VECTOR32 *)gObjPlayer->move;
    target.vy = pos->vy;
    dir.vx = target.vx - pos->vx;
    dir.vy = target.vy - pos->vy;
    dir.vz = target.vz - pos->vz;
    GTEMath_VectorNormal(&dir, &dir);
    g = ParticleHitWall(-1, pos, &dir, 0xFFC8C8);
    if (g != NULL) {
        g->unk26 = kind;
    }
}

void ParticleElectricalSparkShow(void) {
}

void ParticleElectricalSparkHide(void) {
}

ParticleGroup *ParticleElectricalSpark(s32 arg0, VECTOR32 *pos, s32 color) {
    VECTOR32 dir = D_80012000;
    s32 count;
    s32 life;
    ParticleGroup *g;

    ParticleInitEmitterPoint(pos);
    count = (rand() & 7) + 1;
    life = (rand() & 7) + 5;
    ParticleInitAngle(0x555, 0, 0xFF);
    ParticleInitLifeColorFade(life, color);
    D_801100B4 = 32;
    ParticleInitDirection(&dir, 0x7FF, 0);
    g = ParticlePoolAllocate(count, NULL);
    if (g != NULL) {
        g->unk23 = 8;
        g->wind = -0x2000;
        g->updateType = 8;
        g->unk1C = 0;
        g->drawType = 3;
        ParticleStart(g, count);
    }
    return g;
}

void ParticleGunFlash(void) {
}

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012030);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPoint);

INCLUDE_ASM("asm/main/nonmatchings/particle", particleGlassTexture);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleBuildGlass);

INCLUDE_ASM("asm/main/nonmatchings/particle", func_80052D90);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPaneWall);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPaneCeiling);

void ParticleStartRain(void) {
    VECTOR32 mask = D_80012050;
    VECTOR32 pos;
    VECTOR32 dir = D_80012000;
    ParticleGroup *g;

    pos = *(VECTOR32 *)gameObjectsInfo[gPlayerOffset].runtime->move;
    pos.vy += 0xFF;
    ParticleInitEmitterMask(&pos, &mask);
    ParticleInitLifeColor(1, 0x202020);
    D_801100B4 = 0x200;
    ParticleInitDirection(&dir, 0, 0);
    g = ParticlePoolAllocate(80, NULL);
    if (g != NULL) {
        g->wind = -0x2916;
        g->updateType = 2;
        g->unk23 = 0;
        g->unk1C = 0;
        g->drawType = 3;
        ParticleStart(g, 80);
    }
    g->drawType = PARTICLE_DRAW_NONE;
}

void ParticleStartSnow(void) {
    VECTOR32 mask;
    VECTOR32 pos;
    VECTOR32 dir;
    ParticleGroup *g;

    if (D_8011006C == NULL) {
        mask = D_80012050;
        dir = D_80012000;
        pos = *(VECTOR32 *)gameObjectsInfo[gPlayerOffset].runtime->move;
        D_801100B4 = 0x200;
        pos.vy += 0xFF;
        ParticleInitEmitterMask(&pos, &mask);
        ParticleInitAngle(0x555, 0x3FF, 0xFF);
        ParticleInitLifeColor(3, 0xFFFFFF);
        ParticleInitDirection(&dir, 0, 0);
        g = ParticlePoolAllocate(80, NULL);
        if (g != NULL) {
            g->unk23 = 8;
            g->wind = -0xA46;
            g->unk1C = 0;
            g->updateType = 3;
            g->drawType = 0;
            ParticleStart(g, 80);
            g->drawType = PARTICLE_DRAW_NONE;
            D_8011006C = g;
        }
    }
}

void ParticleStopSnow(void) {
    if (D_8011006C) {
        ParticleGroupFree(D_8011006C);
        D_8011006C = NULL;
    }
}

#ifdef NON_MATCHING
/* rcos+0x4000 reassociated onto the rsin term */
void ParticleGroupUpdateWind(ParticleGroup *g) {
    s32 c = rcos(D_8011003C);
    s32 s = rsin(D_80110040);
    s32 angle = (s >> 5) + 0x200;
    s32 mag = (s + (c + 0x4000)) >> 6;

    c = rcos(angle);
    s = rsin(angle);
    D_80110040 += 4;
    D_8011003C += 3;
    D_8011D1C0 = (mag * c) >> 12;
    D_8011D1C8 = (mag * s) >> 12;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGroupUpdateWind);
#endif

#ifdef NON_MATCHING
/* addiu -0x6000 lands on the wrong addend */
void ParticleGroupUpdateRain(ParticleGroup *g) {
    s32 a;

    a = rsin(D_80110044);
    g->wind = (a >> 3) + ((rsin(D_80110048) >> 3) - 0x6000);
    D_80110044 += 0xC0;
    D_80110048 += 0x100;
    ParticleGroupUpdateWind(g);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGroupUpdateRain);
#endif

#ifdef NON_MATCHING
/* velocity loads scheduled above the pos.vy store */
void ParticleUpdateFragmentMeterFrictionFloor(ParticleGroup *g) {
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    s32 range;
    s32 half;
    s32 vx;
    s32 vz;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (--p->state == 0) {
            ParticleFree(g, p, prev, i);
        } else {
            p->pos.vx += p->vel.vx;
            p->pos.vy += p->vel.vy >= 0 ? p->vel.vy >> 12 : -(-p->vel.vy >> 12);
            p->pos.vz += p->vel.vz;
            vx = p->vel.vx;
            vz = p->vel.vz;
            vx -= (vx + 1) >> 4;
            vz -= (vz + 1) >> 4;
            p->vel.vy += g->wind;
            p->vel.vx = vx;
            p->vel.vz = vz;
            if (p->pos.vy < g->unk1C) {
                p->vel.vy = -p->vel.vy >> ((rand() & 1) + 2);
                range = func_80052D90(p->vel.vy >> 11);
                half = range >> 1;
                p->vel.vx = (rand() & range) - half;
                p->vel.vz = (rand() & range) - half;
                p->pos.vy = g->unk1C;
            }
            p->unk62 += p->unk64;
            prev = i;
        }
        i = next;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateFragmentMeterFrictionFloor);
#endif

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateExplosion);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyFlat4);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012050);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateGlass);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayRain);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateSnow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyWorld);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateMovement);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateLineMeter);

s32 getShift(s32 n) {
    s32 shift = 0;

    while (n > 0) {
        n >>= 1;
        shift++;
    }
    return shift - 1;
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyFlat);

#ifdef NON_MATCHING
/* depth: the -16 is folded onto the shift difference instead of the group offset */
void ParticleUpdateTracer(ParticleGroup *g) {
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    VECTOR32 screen;
    VECTOR32 world;
    s32 depth;
    s32 dy;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (--p->state == 0) {
            ParticleFree(g, p, prev, i);
        } else {
            world.vx = p->wx;
            world.vy = p->wy;
            world.vz = p->wz;
            EGSDisplayTransformWorldToScreen(&p->pos, &screen);
            if (screen.vz > 0) {
                p->xy1 = (u16)screen.vx - (screen.vy << 16);
                EGSDisplayTransformWorldToScreen(&world, &screen);
                if (screen.vz > 0) {
                    p->xy0 = (u16)screen.vx - (screen.vy << 16);
                    depth = g->unk26 - 16;
                    depth += (screen.vz >> 2) - (screen.vz >> 4);
                    if (depth >= 0) {
                        p->depth = depth;
                    } else {
                        p->depth = 0;
                    }
                } else {
                    p->xy1 = 0x4000400;
                    p->xy0 = 0x4000400;
                }
            } else {
                p->xy1 = 0x4000400;
                p->xy0 = 0x4000400;
            }
            p->pos.vx += p->vel.vx;
            p->wx += p->vel.vx;
            dy = p->vel.vy >= 0 ? p->vel.vy >> 12 : -(-p->vel.vy >> 12);
            p->pos.vy += dy;
            p->wy += dy;
            p->pos.vz += p->vel.vz;
            p->wz += p->vel.vz;
            prev = i;
        }
        i = next;
    }
    if (g->head < 0) {
        D_80110068 = 0;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateTracer);
#endif

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateShell);

void ssUpdateAndDisplay(ParticleGroup *g) {
    if (g->updateType != PARTICLE_UPDATE_NONE) {
        updateFunc[g->updateType](g);
    }
    if (g->drawType != PARTICLE_DRAW_NONE && drawFunc[g->drawType] != NULL) {
        drawFunc[g->drawType](g);
    }
}

void ParticleUpdateFireLight(void) {
    s32 r;
    s32 color;

    if (D_80110094 != D_80110098) {
        if (D_80110094 != NULL) {
            if (D_80110098 == NULL) {
                if (!D_80118FE0[1].on) {
                    EGSDisplayPointSourceOn(&D_80118FE0[1]);
                }
                D_80110058 = 4;
            }
        } else {
            D_80110058 = 16;
        }
        D_80110098 = D_80110094;
    }
    if (D_80110094 != NULL) {
        if (D_80110058) {
            D_80110058--;
        }
        r = (4 - D_80110058) * 32 + (rand() & 0x1F);
        EGSDisplayPointSourceParams(&D_80118FE0[1], D_8011009C * 2 + 100, ((r / 2 + (rand() & 0x1F)) << 8) + r);
        EGSFrameSetPosition(D_80118FE0[1].frame, 0, &D_8011D140);
        D_8011005C = (D_80110094->flags >> 19) & 1;
    } else {
        if (D_80110058) {
            r = D_80110058 * 8 + 15;
            color = (((u32)r >> 1) << 8) + r;
            if (D_8011005C) {
                color |= 0x80000000;
            }
            D_80110058--;
            EGSDisplayPointSourceParams(&D_80118FE0[1], D_8011009C * 2 + 100, color);
            if (D_80110058) {
                return;
            }
        }
        if (D_80118FE0[1].on) {
            EGSDisplayPointSourceOff(&D_80118FE0[1]);
            D_80110098 = NULL;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdate);
