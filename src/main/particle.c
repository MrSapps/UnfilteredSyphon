#include "common.h"
#include "game.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

#line 46

/* A group of particles sharing an emitter (0x34 bytes, group[] has D_80110060 of them). */
typedef struct ParticleGroup {
    /* 0x00 */ u32 fade; /* 0x80000000 | fade steps when fading out */
    /* 0x04 */ u32 flags;
    /* 0x08 */ s16 x; /* emitter position */
    /* 0x0A */ s16 y;
    /* 0x0C */ s16 z;
    /* 0x0E */ s16 dx; /* group drift velocity */
    /* 0x10 */ s16 dy;
    /* 0x12 */ s16 dz;
    /* 0x14 */ s16 gobjId; /* object the particles stick to */
    /* 0x16 */ s16 unk16;
    /* 0x18 */ s16 owner; /* object id of the shooter */
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
    /* 0x20 */ s32 color;
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
    /* 0x56 */ s16 size; /* triangles: angle of the second vertex */
    /* 0x58 */ u8 unk58[4];
    /* 0x5C */ s16 unk5C; /* size / 2 + 0x800; triangles: angle of the third vertex */
    /* 0x5E */ s16 unk5E;
    /* 0x60 */ s16 unk60;
    /* 0x62 */ s16 unk62;
    /* 0x64 */ s16 unk64;
    /* 0x66 */ u8 unk66[2];
} Particle; /* size 0x68 */

#define PARTICLE_MAX 160
#define PARTICLE_FREE -0x8000

#define PARTICLE_UPDATE_NONE 10
#define PARTICLE_DRAW_NONE 5

/*
 * .sbss (0x80110030..0x80110118): every slot is 4 apart whatever its size. The original
 * assembler word-aligned each .lcomm (as aspsx 2.56 does); 2.77 and maspsx pack by size.
 * The u8/s16 slots must stay scalars (cc1 reloads array elements and struct members), so
 * padding statics fill the gaps; define LCOMM_WORD for a word-aligning aspsx (2.56).
 */
#ifdef LCOMM_WORD
#define SBSS_PAD1(n)
#define SBSS_PAD2(n)
#define SBSS_PAD3(n)
#else
#define SBSS_PAD1(n) static u8 n;
#define SBSS_PAD2(n) static s16 n;
#define SBSS_PAD3(n) static u8 n; static s16 n##_2;
#endif
static s32 D_80110030; /* wire sag amplitude */
static s32 D_80110034; /* wire swing amplitude */
static s32 D_80110038; /* wire swing phase */
static s32 D_8011003C; /* wind phases */
static s32 D_80110040;
static s32 D_80110044; /* rain wind phase */
static s32 D_80110048;
static s32 D_8011004C; /* glow pulse phase */
static s32 D_80110050; /* glow pulse counter */
static s32 D_80110054; /* screen size shift of the current triangle group */
static u8 D_80110058; SBSS_PAD3(D_80110058_pad) /* fire light fade */
static u8 D_8011005C; SBSS_PAD3(D_8011005C_pad)
static s32 D_80110060; /* number of groups */
static ParticleGroup *D_80110064; /* last shell group */
static ParticleGroup *D_80110068; /* last tracer group */
static ParticleGroup *D_8011006C; /* snow group */
static s32 D_80110070; /* groupUpdateList count */
static s32 D_80110074; /* lowest possibly free particle index */
static s32 D_80110078; /* groups in use */
static s32 D_8011007C; /* particles in use */
static s32 D_80110080; /* triangles drawn */
static Particle *D_80110084; /* last shell that bounced */
static s32 D_80110088; /* default floor height */
static EGSFrame **D_8011008C; /* camera frame */
static s32 D_80110090; /* group allocation cursor */
static ParticleGroup *D_80110094; /* group carrying the fire light */
static ParticleGroup *D_80110098;
static s32 D_8011009C;
static s32 D_801100A0; /* XZ plane emitter slope (dz << 8) / dx */
static s32 D_801100A4; /* emitter mode: 0 point, 1 mask */
static s32 D_801100A8;
static s32 D_801100AC;
static s32 D_801100B0;
static s32 D_801100B4;
static s32 D_801100B8; /* life */
static s32 D_801100BC; /* color */
static s32 D_801100C0; /* color fade */
static s32 D_801100C4; /* direction random range >> 8 */
static s32 D_801100C8; /* direction speed */
static s16 D_801100CC; SBSS_PAD2(D_801100CC_pad) /* taser wire length (dist >> 8) */
static s16 D_801100D0; SBSS_PAD2(D_801100D0_pad) /* wire segments still flying out */
static s16 D_801100D4; SBSS_PAD2(D_801100D4_pad) /* wire sag */
static s16 D_801100D8; SBSS_PAD2(D_801100D8_pad) /* initial sag amplitude */
static ParticleGroup *D_801100DC; /* taser wire group */
static GameObjectRuntime *D_801100E0;
static u8 D_801100E4; SBSS_PAD3(D_801100E4_pad) /* taser target bone */
static u8 D_801100E8; SBSS_PAD3(D_801100E8_pad) /* player's target info byte 0x60 when fired */
static u8 D_801100EC; SBSS_PAD3(D_801100EC_pad)
static s16 D_801100F0; SBSS_PAD2(D_801100F0_pad) /* glass texture mapping, set up by the pane functions: tpage */
static s16 D_801100F4; SBSS_PAD2(D_801100F4_pad) /* clut */
static s16 D_801100F8; SBSS_PAD2(D_801100F8_pad) /* texture VRAM x */
static s16 D_801100FC; SBSS_PAD2(D_801100FC_pad) /* texture VRAM y */
static s16 D_80110100; SBSS_PAD2(D_80110100_pad) /* texture width */
static s16 D_80110104; SBSS_PAD2(D_80110104_pad) /* texture height */
static s16 D_80110108; SBSS_PAD2(D_80110108_pad) /* pane origin x */
static s16 D_8011010C; SBSS_PAD2(D_8011010C_pad) /* pane origin y */
static s16 D_80110110; SBSS_PAD2(D_80110110_pad) /* pane width */
static s16 D_80110114; SBSS_PAD2(D_80110114_pad) /* pane height */
/* commons (psylink places them after every object; tentative definitions so aspsx
 * reaches them through $gp) */
ParticleGroup *group;
u8 *groupUpdateList;
s32 particleBreathFlag;
s32 particleTaserState;
u8 particleFireWaveFlag;
#line 147


static const VECTOR32 D_80012000; /* {0, 0, 0}: defined below */
static const VECTOR32 D_80012010; /* {0, 0x1000, 0} */
extern VECTOR32 D_80012030; /* {7, 7, 7} */
extern VECTOR32 D_80012050; /* {0x3FF, 0x1FF, 0x3FF} */






void ParticleUpdateFragmentMeterFrictionFloor(ParticleGroup *g);
void ParticleUpdateExplosion(ParticleGroup *g);
void ParticleDisplayRain(ParticleGroup *g);
void ParticleUpdateSnow(ParticleGroup *g);
void ParticleUpdateGlass(ParticleGroup *g);
void ParticleUpdateTracer(ParticleGroup *g);
void ParticleUpdateTaser(ParticleGroup *g);
void ParticleUpdateShell(ParticleGroup *g);
void ParticleUpdateLineMeter(ParticleGroup *g);
void ParticleUpdateGrenadeLauncher(ParticleGroup *g);
void ParticleDisplayPolyFlat(ParticleGroup *g);
void ParticleDisplayPolyWorld(ParticleGroup *g);
void ParticleDisplayPolyFlat4(ParticleGroup *g);

/* .data (0x801062C4..0x80106438) */
/* per level: number of levelFloorY entries */
static u8 D_801062C4[16] = { 7, 2, 5, 1, 3, 3, 3, 1, 1, 1, 1, 2, 2, 1, 6, 1 };
s16 levelFloorY[20][7] = {
    { 0x8BC, 0x4B4, 0xEC, 0x66, -0x1B8, -0x3F6, -0x477 },
    { 0xEC, -0x3F5, 0, 0, 0, 0, 0 },
    { -0x3F5, 0, 0, 0, 0, 0, 0 },
    { 0x6A, -0x16, -0x10A, -0x18E, -0x2CA, 0, 0 },
    { -0x9A, 0, 0, 0, 0, 0, 0 },
    { 0x9A6, 0x30F, 0x69, 0, 0, 0, 0 },
    { 0x2F0, 0x1CE, -0xD9, 0, 0, 0, 0 },
    { 0x6A, -0x19B, -0x2BC, 0, 0, 0, 0 },
    { -0x19B, -0x2BC, 0, 0, 0, 0, 0 },
    { 0x353, 0, 0, 0, 0, 0, 0 },
    { 0x6A, 0, 0, 0, 0, 0, 0 },
    { 0x150D, 0, 0, 0, 0, 0, 0 },
    { 0x78A, 0, 0, 0, 0, 0, 0 },
    { 0x613, 0x33E, 0, 0, 0, 0, 0 },
    { 0x201, 0x71, 0, 0, 0, 0, 0 },
    { 0x6A, 0, 0, 0, 0, 0, 0 },
    { 0x8EE, 0x803, 0x6F4, 0x491, 0x320, -0xB3, 0 },
    { 0x8EE, 0x803, 0x6F4, 0x491, 0x320, -0xB3, 0 },
    { -0x3F5, 0, 0, 0, 0, 0, 0 },
    { 0x8BC, 0x4B4, 0xEC, 0x66, -0x1B8, -0x3F6, -0x477 },
};
void (*updateFunc[])(ParticleGroup *) = {
    ParticleUpdateFragmentMeterFrictionFloor, ParticleUpdateExplosion, ParticleDisplayRain,
    ParticleUpdateSnow, ParticleUpdateGlass, ParticleUpdateTracer, ParticleUpdateTaser,
    ParticleUpdateShell, ParticleUpdateLineMeter, ParticleUpdateGrenadeLauncher,
};
void (*drawFunc[])(ParticleGroup *) = {
    ParticleDisplayPolyFlat, ParticleDisplayPolyWorld, NULL, NULL, ParticleDisplayPolyFlat4,
};
static VECTOR32 D_80106428 = { 0, 0, 0, 0 };
#line 164

typedef struct PointSourceLight {
    /* 0x00 */ s32 on;
    /* 0x04 */ u8 unk04[0x28];
    /* 0x2C */ EGSFrame *frame;
} PointSourceLight;
/* .bss (0x80118FC0..0x8011D1D0) */
static VECTOR32 D_80118FC0; /* tracer direction / shell velocity */
static VECTOR32 D_80118FD0; /* gun tip */
static PointSourceLight D_80118FE0[2];
static Particle D_80119040[PARTICLE_MAX];
static VECTOR32 D_8011D140; /* NPC fire position followed by the fire light */
static VECTOR32 D_8011D150; /* emitter position */
static VECTOR32 D_8011D160; /* XZ plane emitter end point */
static VECTOR32 D_8011D170; /* emitter mask size */
static VECTOR32 D_8011D180; /* direction base */
static VECTOR32 D_8011D190; /* taser tip (wire start) */
static VECTOR32 D_8011D1A0; /* wire end */
static VECTOR32 D_8011D1B0; /* wire direction */
static VECTOR32 D_8011D1C0; /* wind (x, z used) */
#line 171

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


void ParticleInitEmitterXZPlane(VECTOR32 *from, VECTOR32 *to, VECTOR32 *size) {
    VECTOR32 center;
    VECTOR32 mask;
    VECTOR32 center2;
    VECTOR32 mask2;
    s32 dx = to->vx - from->vx;
    s32 dz = to->vz - from->vz;

    if (dx == 0) {
        mask.vx = 0;
        mask.vy = size->vy;
        mask.vz = size->vz;
        center.vx = from->vx;
        center.vy = (from->vy + to->vy) >> 1;
        center.vz = (from->vz + to->vz) >> 1;
        ParticleInitEmitterMask(&center, &mask);
    } else if (dz == 0) {
        mask2.vx = size->vx;
        mask2.vy = size->vy;
        mask2.vz = 0;
        center2.vx = (from->vx + to->vx) >> 1;
        center2.vy = (from->vy + to->vy) >> 1;
        center2.vz = from->vz;
        ParticleInitEmitterMask(&center2, &mask2);
    } else {
        s32 slope = (dz << 8) / dx;

        D_801100A4 = 3;
        D_8011D150 = *from;
        D_8011D160 = *to;
        D_8011D170.vx = 0x7F;
        D_8011D170.vy = size->vy;
        D_801100A0 = slope;
    }
}

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

/* Like ParticleInitLifeColor, plus a fade that takes 3/4 of the color off over the life
 * (the step shift is the bit length of life, from the GTE leading-zero counter). */
void ParticleInitLifeColorFade(s32 life, s32 color) {
    s32 shift = 0;

    D_801100B8 = life;
    if (gSniperMode && gCurrentWeapon == 12) {
        color = ((s32)((((u32)color >> 16) & 0xFF) + ((color & 0xFF00) >> 8) + (color & 0xFF)) >> 2) << 8;
    }
    D_801100BC = color;
    __asm__ volatile("mtc2 %0, $30" : : "r"(life));
    __asm__ volatile("nop\n\tnop\n\tmfc2 %0, $31" : : "r"(shift));
    shift = 31 - shift;
    D_801100C0 = ((((color & 0xFF0000) - ((color & 0xFF0000) >> 2)) >> shift) & 0xFF0000) |
                 ((((color & 0xFF00) - ((color & 0xFF00) >> 2)) >> shift) & 0xFF00) |
                 ((((color & 0xFF) - ((color & 0xFF) >> 2)) >> shift) & 0xFF);
}

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
#ifndef _PSYQ
/* splat labels inside the .bss objects above, used by the INCLUDE_ASM functions */
__asm__(".set D_80118FC4, D_80118FC0+4\n"
        ".set D_80118FC8, D_80118FC0+8\n"
        ".set D_80118FD4, D_80118FD0+4\n"
        ".set D_80118FD8, D_80118FD0+8\n"
        ".set D_80119044, D_80119040+4\n"
        ".set D_80119048, D_80119040+8\n"
        ".set D_8011904C, D_80119040+0xC\n"
        ".set D_80119060, D_80119040+0x20\n"
        ".set D_80119064, D_80119040+0x24\n"
        ".set D_80119066, D_80119040+0x26\n"
        ".set D_8011D194, D_8011D190+4\n"
        ".set D_8011D198, D_8011D190+8\n"
        ".set D_8011D1A4, D_8011D1A0+4\n"
        ".set D_8011D1C8, D_8011D1C0+8\n");
#endif

/* Particle sprite textures (0x80105E1C..) */
typedef struct ParticleTextures {
    /* 0x00 */ void *sprite;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ u16 unk10; /* breath puffs start at this - 1 */
} ParticleTextures;
extern ParticleTextures D_80105E1C;
void EGSDisplayBuildPoly3F(void *prim, s32 color, s32 xy0, s32 xy1, s32 xy2);
void EGSDisplayBuildPoly3FT(void *prim, s32 color, s32 xy0, s32 uv0, s32 xy1, s32 uv1, s32 xy2, s32 uv2);
void EGSDisplayBuildLineF(void *prim, s32 color, s32 xy0, s32 xy1);
void EGSDisplayBuildLineG(void *prim, s32 color0, s32 xy0, s32 xy1, s32 color1);
void EGSTextureCreateSprite(void *texture, void *sprite, s32 w, s32 h, s32 u, s32 v);
void EGSDisplayAddPoly(void *display, void *prim);

/* Random velocity component: base + random spread, scaled by the direction speed.
 * (The sign test and the value each draw their own random number.) */
#define PARTICLE_RAND_VEL(base)                                                                          \
    ((base) + ((D_801100C4 & rand()) << 8) >= 0 ? ((base) + ((D_801100C4 & rand()) << 8)) >> (12 - D_801100C8) \
                                                : -(-((base) + ((D_801100C4 & rand()) << 8)) >> (12 - D_801100C8)))

#ifdef NON_MATCHING
/* r7J (54 -> 1): regalloc fixed by loop-depth ref weights (nested do-while(0)): i x4 around
 * `i = p->next` (16 refs: 25600, takes s0 first), AddPoly x4 + the Poly3F/Poly3FT calls x1 lift
 * prim (22/42: 20952) over p (82/240: 20500), so prim shares s0 with i and p gets s1.
 * Left: drawType 2 arg order, original `li a2,0x190` before `lw a1,D_801100BC` (ours after).
 * sched1 ties all four arg sets at priority 1 and takes LUID order (a0..a3); the original needs
 * the a1 load picked first (higher priority/LUID). prim = p->prim after the call, a local for
 * the color, a call-through cast, volatile load and barriers around it change nothing. */
/* Spawns every particle of g from the emitter/direction/life/color set up by the ParticleInit* calls. */
void ParticleStart(ParticleGroup *g, s32 count) {
    void *display;
    s32 lifeShift;
    void *prim;
    s32 i;
    Particle *p;
    s16 size;
    s32 rand1 = rand() & 1;
    s32 n = count;

    i = g->head;
    g->flags = D_801100B4;
    g->unk22 = D_801100B8 >> 1;
    g->fade = D_801100C0;
    lifeShift = rand1 + 2;
    CameraGetDisplay(gCamera, &display);
    while (i >= 0) {
        p = &D_80119040[i];
        if (D_801100A4 == 1) {
            p->pos.vx = D_8011D150.vx + (D_8011D170.vx & rand());
            p->pos.vy = D_8011D150.vy + (D_8011D170.vy & rand());
            p->pos.vz = D_8011D150.vz + (D_8011D170.vz & rand());
        } else if (D_801100A4 == 0) {
            p->pos = D_8011D150;
        } else {
            p->pos.vx = D_8011D150.vx + (rand() & D_8011D170.vx) * (D_8011D160.vx - D_8011D150.vx) / D_8011D170.vx;
            p->pos.vy = D_8011D150.vy - (D_8011D170.vy & rand());
            p->pos.vz = D_8011D150.vz + (((p->pos.vx - D_8011D150.vx) * D_801100A0) >> 8);
        }
        if (D_801100C4 != 0) {
            p->vel.vx = PARTICLE_RAND_VEL(D_8011D180.vx);
            p->vel.vy = (D_8011D180.vy + ((D_801100C4 & rand()) << 8)) << D_801100C8;
            p->vel.vz = PARTICLE_RAND_VEL(D_8011D180.vz);
        } else {
            p->vel.vx = D_8011D180.vx >> (12 - D_801100C8);
            p->vel.vy = D_8011D180.vy << D_801100C8;
            p->vel.vz = D_8011D180.vz >> (12 - D_801100C8);
        }
        p->pos.vx += p->vel.vx;
        p->pos.vy += p->vel.vy >> 12;
        p->pos.vz += p->vel.vz;
        p->state = D_801100B8 + (n >> lifeShift);
        n--;
        if ((u32)g->drawType < 2) {
            p->color = D_801100BC;
            p->unk62 = rand();
            size = (D_801100AC & rand()) - (D_801100AC >> 1) + D_801100A8;
            p->size = size;
            p->unk5C = (size >> 1) + 0x800;
            p->unk64 = (D_801100B0 & rand()) - (D_801100B0 >> 1);
            prim = p->prim;
            if (g->flags & 0x8000) {
                do {
                    EGSDisplayBuildPoly3FT(prim, D_801100BC, 0x190, 0, 0x190, 0, 0x190, 0);
                } while (0);
            } else {
                do {
                    EGSDisplayBuildPoly3F(prim, D_801100BC, 0x190, 0x190, 0x190);
                } while (0);
            }
        } else if (g->drawType == 4) {
            EGSTextureCreateSprite(D_80105E1C.sprite, p->prim, 0x80, 0x80, 0, 0);
            p->color = D_801100BC;
            *(s32 *)p->prim &= ~0x40;
            goto next;
        } else if (g->drawType == 2) {
            prim = p->prim;
            EGSDisplayBuildLineF(prim, D_801100BC, 0x190, 0x190);
        } else if (g->drawType == 3) {
            prim = p->prim;
            p->color = D_801100BC;
            EGSDisplayBuildLineG(prim, 0, 0x190, 0x190, D_801100BC);
        } else {
            goto next;
        }
        do {
            do {
                do {
                    do {
                        EGSDisplayAddPoly(display, prim);
                    } while (0);
                } while (0);
            } while (0);
        } while (0);
    next:
        do {
            do {
                do {
                    do {
                        i = p->next;
                    } while (0);
                } while (0);
            } while (0);
        } while (0);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStart);
#endif

void ParticleInitBreath(void) {
    particleBreathFlag = 1;
}


s32 EGSTextureSetSpriteTransparency(void *sprite, s32 mode);

/* x >> n rounded towards zero */
#define SHIFT_TO_ZERO(x, n) ((x) >= 0 ? (x) >> (n) : -(-(x) >> (n)))

/* A puff of cold breath in front of the head (bone 0) of object id. */
void ParticleBreath(s32 id) {
    VECTOR32 dir;
    VECTOR32 pos;
    GameObjectRuntime *gobj = gameObjectsInfo[id].runtime;
    ParticleGroup *g;
    Particle *p;
    PlayerAnimView *anim;
    s32 side;

    if (id == gPlayerOffset && gSniperMode && gCurrentWeapon == 13) {
        return;
    }
    pos.vx = ((AnimFramesView *)gobj->anim)->frames[0]->pos.vx;
    pos.vy = ((AnimFramesView *)gobj->anim)->frames[0]->pos.vy;
    pos.vz = ((AnimFramesView *)gobj->anim)->frames[0]->pos.vz;
    pos.vy = -pos.vy + 8;
    dir.vx = ((AnimFramesView *)gobj->anim)->frames[0]->m[0][2];
    dir.vy = ((AnimFramesView *)gobj->anim)->frames[0]->m[1][2];
    dir.vz = ((AnimFramesView *)gobj->anim)->frames[0]->m[2][2];
    dir.vy = -dir.vy;
    ParticleInitLifeColorNoInfrared(8, 0x1E1E1E);
    D_801100B4 = 0x10;
    g = iParticleExplosion(-1, &pos, 1, 0, -1, 0xFFF, 0);
    if (g != NULL) {
        g->unk23 = 3;
        p = &D_80119040[g->head];
        anim = (PlayerAnimView *)gobj->anim;
        if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
            side = anim->unk16 == 1 ? 0x18 : 0;
        } else {
            side = -0x18;
        }
        g->unk26 = side;
        if (id == gPlayerOffset) {
            g->unk26 = side + 0x1E;
        }
        g->unk1C = 3;
        p->unk64 = D_80105E1C.unk10 - 1;
        p->vel.vx = -SHIFT_TO_ZERO(dir.vx, 10);
        p->vel.vy = -dir.vy << 2;
        p->vel.vz = -SHIFT_TO_ZERO(dir.vz, 10);
        p->pos.vx = pos.vx + (p->vel.vx << 2);
        p->pos.vy = pos.vy;
        p->pos.vz = pos.vz + (p->vel.vz << 2);
        p->state = (rand() & 7) + 12;
        p->unk60 = p->state;
        EGSTextureSetSpriteTransparency(p->prim, 1);
    }
}

void WeaponGetTip(GameObjectRuntime *gobj, VECTOR32 *tip);
s32 EGSMathFIXMul(s32 a, s32 b);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);

/* GameObjectInfo.flags24 low byte: NPC weapon */
#define GOBJINFO_WEAPON(info) (*(u8 *)&(info)->flags24)
#define GOBJ_WEAPON_IS(g, w)                                                                         \
    ((g)->id != gPlayerOffset ? (GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) != 0 &&                   \
                                 GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) == (w))                   \
                              : gCurrentWeapon == (w))

/* Frames until a shot from object id reaches target (0 for instant weapons). */
s32 ParticleDamageDelay(s32 id, VECTOR32 *target) {
    VECTOR32 tip;
    VECTOR32 dir;
    s32 dist;
    GameObjectRuntime *gobj;
    EGSFrame *gun;
    s32 delay;

    if (gSniperMode && id == gPlayerOffset && (gCurrentWeapon == 12 || gCurrentWeapon == 13 || gCurrentWeapon == 18)) {
        return 0;
    }
    gobj = gameObjectsInfo[id].runtime;
    if (gobj->npcState == 1 || gobj->npcState == 2) {
        WeaponGetTip(gobj, &tip);
    } else {
        if (GOBJ_TYPE(gobj->id) != 3) {
            return 0;
        }
        gun = ((AnimFramesView *)gobj->anim)->frames[4];
        tip.vx = gun->pos.vx;
        tip.vy = gun->pos.vy;
        tip.vz = gun->pos.vz;
        tip.vy = -tip.vy;
        dir.vx = gun->m[0][2];
        dir.vy = gun->m[1][2];
        dir.vz = gun->m[2][2];
        dir.vy = -dir.vy;
        dir.vx = EGSMathFIXMul(dir.vx, 0x100);
        dir.vy = EGSMathFIXMul(dir.vy, 0x100);
        dir.vz = EGSMathFIXMul(dir.vz, 0x100);
        tip.vx += dir.vx;
        tip.vy += dir.vy;
        tip.vz += dir.vz;
    }
    EGSGeometry_GetFuzzyDistancePoint2Point(&tip, target, &dist);
    if (!GOBJ_WEAPON_IS(gobj, 16)) {
        delay = (dist - 0x200) >> 9;
    } else {
        delay = dist >> 7;
    }
    if (delay < 0) {
        delay = 0;
    }
    return delay;
}

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


void WeaponGetTip(GameObjectRuntime *gobj, VECTOR32 *tip);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);

/* Fires the taser wire from the player's gun at the taser target (or at pos when there is none). */
void ParticleTaser(VECTOR32 *pos) {
    VECTOR32 dir;
    VECTOR32 dest;
    s32 dist;
    GameObjectRuntime *target = NULL;
    ParticleGroup *g;

    if (gTaserGobjID >= 0) {
        target = gameObjectsInfo[gTaserGobjID].runtime;
    }
    WeaponGetTip(gObjPlayer, &D_8011D190);
    D_801100E0 = gObjPlayer;
    D_801100EC = gSniperMode;
    if (target != NULL) {
        dest.vx = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vx;
        dest.vy = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vy;
        dest.vz = ((AnimFramesView *)target->anim)->frames[D_801100E4]->pos.vz;
        dest.vy = -dest.vy;
    } else {
        dest = *pos;
    }
    dir.vx = D_8011D190.vx - dest.vx;
    dir.vy = D_8011D190.vy - dest.vy;
    dir.vz = D_8011D190.vz - dest.vz;
    GTEMath_VectorNormal(&dir, &dir);
    EGSGeometry_GetFuzzyDistancePoint2Point(&D_8011D190, &dest, &dist);
    if (dist > 0xA00) {
        D_801100D4 = 0x80;
    } else {
        D_801100D4 = dist / 20;
    }
    D_8011D1B0.vx = -dir.vx >> 4;
    D_8011D1B0.vy = -dir.vy >> 4;
    D_8011D1B0.vz = -dir.vz >> 4;
    D_801100CC = dist >> 8;
    if (D_801100CC < 0) {
        D_801100CC = 0;
    }
    D_801100D0 = D_801100CC;
    ParticleInitEmitterPoint(&dest);
    ParticleInitLifeColor(1, 0xFFFFFF);
    ParticleInitDirection(&D_8011D1B0, 0, 0);
    g = ParticlePoolAllocate(32, NULL);
    if (g != NULL) {
        g->unk23 = 10;
        g->updateType = 6;
        g->unk1C = 0;
        g->wind = 0;
        g->drawType = 3;
        ParticleStart(g, 32);
        g->unk27 = 0;
        g->unk26 = -5;
        D_8011D1A0 = D_8011D190;
        particleTaserState = 1;
        D_801100DC = g;
        EGSFrameSetPosition(D_80118FE0[0].frame, 0, &D_8011D190);
        EGSDisplayPointSourceParams(&D_80118FE0[0], 100, 0xFF0000);
        if (!D_80118FE0[0].on) {
            EGSDisplayPointSourceOn(&D_80118FE0[0]);
        }
    }
    D_801100E8 = ((TargetInfo *)gObjPlayer->target)->unk60;
}


/* EGS display wrapper around a PsyQ LINE_G2 (Particle.prim) */
typedef struct EGSLineG2 {
    /* 0x00 */ void *link;
    /* 0x04 */ s32 depth;
    /* 0x08 */ u32 tag;
    /* 0x0C */ s32 rgbc0; /* r0 g0 b0 code */
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 rgb1;
    /* 0x18 */ s32 xy1;
} EGSLineG2;

void NPCSoundGobj(s32 arg0, s32 from, void *pos, s32 arg3);

#ifdef NON_MATCHING
/* logic complete; spill slots/callee-saved choice and the D_801100D0/D_801100D4 reloads differ */
/* Animates the taser wire: flies out segment by segment, then sways and crackles until retracted. */
void ParticleUpdateTaser(ParticleGroup *unused) {
    VECTOR32 screen;
    VECTOR32 pos;
    VECTOR32 cur;
    VECTOR32 step;
    VECTOR32 dir;
    s32 bright = 100;
    s32 prev = 0;
    GameObjectRuntime *target = NULL;
    s32 i = 0;
    s32 idx;
    s32 next;
    Particle *p;
    EGSLineG2 *line;
    s32 angle;
    s32 angleStep;
    s32 sag;
    s32 swing;
    s32 phase;
    s32 phase2;
    s32 phaseStep;
    s32 bx;
    s32 by;
    s32 bz;
    s32 amp;
    s32 a;
    s32 c;
    s32 flag;
    s32 floor;
    s32 count;
    ParticleGroup *g;

    if (particleTaserState != 3 && D_801100E0->healthInfo->health <= 0) {
        particleTaserState = 3;
    }
    if (gTaserGobjID >= 0) {
        target = gameObjectsInfo[gTaserGobjID].runtime;
    }
    ParticleTaserGetDestination(target, &D_8011D1A0);
    dir.vx = D_8011D190.vx - D_8011D1A0.vx;
    dir.vy = D_8011D190.vy - D_8011D1A0.vy;
    dir.vz = D_8011D190.vz - D_8011D1A0.vz;
    GTEMath_VectorNormal(&dir, &dir);
    D_8011D1B0.vx = -dir.vx >> 4;
    D_8011D1B0.vy = -dir.vy >> 4;
    D_8011D1B0.vz = -dir.vz >> 4;
    EGSDisplayPointSourceParams(&D_80118FE0[0], rand() % 64 + 0x5A, ((rand() & 0x7F) + 0x7F) << 16);
    idx = D_801100DC->head;
    cur = D_8011D190;
    if (D_801100D0 > 0) {
        D_801100D0--;
        pos.vx = D_8011D190.vx + D_8011D1B0.vx * (D_801100CC - D_801100D0);
        pos.vy = D_8011D190.vy + D_8011D1B0.vy * (D_801100CC - D_801100D0);
        pos.vz = D_8011D190.vz + D_8011D1B0.vz * (D_801100CC - D_801100D0);
        angle = 0x200;
        sag = D_801100D4 * 2;
        angleStep = ((D_801100CC - D_801100D0) << 5) / D_801100CC;
        D_80110034 = 0;
        D_80110038 = 0;
        D_80110030 = sag;
        D_801100D8 = sag;
        D_8011D1A0 = pos;
        swing = 0;
        if (particleTaserState == 3) {
            D_801100D0 = 0;
        } else if (D_801100D0 <= 0) {
            D_80110034 = D_801100D4;
            if (!(rand() & 1)) {
                D_80110034 = -D_80110034;
            }
            particleTaserState = 2;
            NPCSoundGobj(1, gPlayerOffset, &D_8011D1A0, 0x1E0);
        }
    } else {
        if (particleTaserState == 1) {
            particleTaserState = 2;
            NPCSoundGobj(1, gPlayerOffset, &D_8011D1A0, 0x1E0);
        }
        angle = 0x200;
        pos.vx = D_8011D1A0.vx;
        pos.vy = D_8011D1A0.vy;
        pos.vz = D_8011D1A0.vz;
        angleStep = 0x20;
        if (D_80110030 > 0) {
            D_80110030 -= D_80110030 >> 4;
        }
        if (D_80110034 > 0) {
            D_80110034 -= D_80110030 >> 4;
        }
        sag = (D_80110030 * rsin(D_80110038)) >> 12;
        swing = (D_80110034 * rsin(D_80110038)) >> 12;
        D_80110038 += ((D_801100D4 >> 1) << 12) / 360;
    }
    phase2 = (gSystemTick & 0x7F) * (0x80 - D_801100D4 / 2);
    phaseStep = ((D_801100D4 >> 2) << 12) / 360;
    phase = phase2;
    step.vx = (pos.vx - D_8011D190.vx) / 32;
    step.vy = (pos.vy - D_8011D190.vy) / 32;
    step.vz = (pos.vz - D_8011D190.vz) / 32;
    bx = cur.vx;
    by = cur.vy;
    bz = cur.vz;
    while (idx >= 0) {
        p = &D_80119040[idx];
        next = p->next;
        if (particleTaserState == 3) {
            if (--p->state == 0) {
                ParticleFree(D_801100DC, p, prev, idx);
                D_80110034 = 0x100;
            } else {
                prev = idx;
            }
        }
        if (p->state != PARTICLE_FREE) {
            if (i < 8) {
                amp = swing / (8 - i);
            } else {
                amp = swing;
            }
            line = (EGSLineG2 *)p->prim;
            pos = cur;
            angle += angleStep;
            bx += step.vx;
            by += step.vy;
            bz += step.vz;
            cur.vx = bx + ((amp / 16) * rsin(phase) >> 12);
            cur.vz = bz + ((amp / 16) * rcos(phase) >> 12);
            a = (sag * (rsin(angle) - rsin(0x200))) >> 12;
            c = (amp * rsin(phase2)) >> 12;
            cur.vy = by + a + c / 16;
            a += c;
            phase += phaseStep * 2;
            phase2 += phaseStep;
            c = (bright << 16) + (bright << 8) + bright;
            if (c & 0xFF000000) {
                line->rgbc0 = (c & 0xFFFFFF) | 0x50000000;
            } else {
                line->rgbc0 = c | 0x50000000;
            }
            if (a > 0) {
                bright = a + 100;
                if (bright > 200) {
                    bright = 255;
                }
            } else {
                bright = (a >> 1) + 100;
            }
            if (bright < 0) {
                bright = 0;
            }
            line->rgb1 = (bright << 16) + (bright << 8) + bright;
            EGSDisplayTransformWorldToScreen(&pos, &screen);
            if (screen.vz > 0) {
                line->xy0 = (u16)screen.vx - (screen.vy << 16);
                EGSDisplayTransformWorldToScreen(&cur, &screen);
                if (screen.vz > 0) {
                    line->xy1 = (u16)screen.vx - (screen.vy << 16);
                    c = (screen.vz >> 2) - (screen.vz >> 4) + (D_801100DC->unk26 - 16);
                    if (c < 0) {
                        line->depth = 0;
                    } else {
                        line->depth = c;
                    }
                } else {
                    line->xy1 = line->xy0 = 0x04000400;
                }
            } else {
                line->xy1 = line->xy0 = 0x04000400;
            }
            prev = idx;
        }
        i++;
        idx = next;
    }
    if (D_801100DC->head < 0) {
        particleTaserState = 0;
        return;
    }
    if (particleTaserState == 2) {
        if (target != NULL) {
            flag = gSystemTick & 1;
        } else {
            flag = D_801100D4 > 0x40;
        }
        if (D_801100E8 != 25) {
            if (!flag) {
                g = ParticleHitWall(-1, &cur, &step, 0xFFC8C8);
            } else {
                VECTOR32 zero = D_80012000;

                if (target != NULL) {
                    floor = D_8011D1A0.vy - 0x60;
                    if (D_80110088 < floor) {
                        floor = D_80110088;
                    }
                    count = (rand() & 1) + 1;
                } else {
                    floor = D_80110088;
                    count = (rand() & 3) + 3;
                }
                D_801100B4 = 0x40;
                ParticleInitEmitterPoint(&D_8011D1A0);
                ParticleInitLifeColorFade(0x18, 0xFFC8C8);
                ParticleInitDirection(&zero, 0x7FF, 0);
                g = ParticlePoolAllocate(count, NULL);
                if (g == NULL) {
                    return;
                }
                g->unk23 = 3;
                g->wind = -0x2916;
                g->unk1C = floor;
                g->updateType = 10;
                g->drawType = 0;
                ParticleStart(g, count);
            }
            if (g != NULL) {
                g->unk26 = -5;
                g->unk24 = 3;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateTaser);
#endif


/* Per-weapon info (0x20 bytes each); flags bits 11-13: tracer every n shots */
typedef struct WeaponInfo {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u8 unk04[0x1C];
} WeaponInfo;
extern WeaponInfo D_80105F58[];

/* View of NPCInfo's shot counter. */
typedef struct NPCShotView {
    /* 0x00 */ u8 unk00[0x36];
    /* 0x36 */ s16 shots;
} NPCShotView;

/* Ambient light of the object's sector (GameObjectAnim 0x1C) */
typedef struct AnimLightView {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 r;
    /* 0x06 */ s16 g;
    /* 0x08 */ s16 b;
} AnimLightView;

/* View of GameObjectAnim with the light and per-bone frames. */
typedef struct FootAnimView {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ EGSFrame **frames;
    /* 0x1C */ AnimLightView *light;
} FootAnimView;

#define GOBJINFO_FLAGS(info) (*(u32 *)&(info)->flags24)
/* Weapon held by gobj: an NPC's from its placement data, else the player's current one */
#define GOBJ_NPC_WEAPON(gobj)                                                                   \
    ((gobj)->npc != NULL                                                                        \
         ? (GOBJINFO_WEAPON(&gameObjectsInfo[(gobj)->id])                                       \
                ? GOBJINFO_WEAPON(&gameObjectsInfo[(gobj)->id])                                 \
            : (GOBJINFO_FLAGS(&gameObjectsInfo[(gobj)->id]) & 0x3000) == 0x1000 ? 19            \
            : (GOBJINFO_FLAGS(&gameObjectsInfo[(gobj)->id]) & 0x3000) == 0x2000 ? 20            \
                                                                                 : 0)           \
         : gCurrentWeapon)

#ifdef NON_MATCHING
/* callee-saved register assignment and some load scheduling differ (630 vs 634 insns) */
/* r6E (62 -> 37): shots is `__builtin_abs(load)` (abssi2 copy-then-negate: bgez/move/negu) and
 * line->state is set after wz. Left: the original keeps light9 >> 8 in light's reg (s4, sra after
 * the 2nd rand) and light9 in s6, &D_80118FD0 in s7 (one more saved reg). `light = light9 >> 8`
 * anywhere is 85 (light set twice: no sched1 birthing boost, sra floats to the top; it also lifts
 * &D_80118FC0 above gobj); a fresh `l8 = light9 >> 8` is 34 but boosted to the block end. */
/* A tracer line from gobj's gun to target (every few NPC shots), and an ejected shell for the player. */
void ParticleTracer(GameObjectRuntime *gobj, VECTOR32 *target) {
    void *display;
    s32 dist;
    s32 id = gobj->id;
    GameObjectInfo *info;
    s32 weapon;
    s32 steps;
    s32 shots;
    s32 rate;
    ParticleGroup *g;
    Particle *line;
    Particle *p;
    void *prim;
    FootAnimView *anim;
    s32 light;
    s32 tmp;
    s32 light9;
    s32 floor;
    s32 c;

    if (gSniperMode && id == gPlayerOffset && (gCurrentWeapon == 12 || gCurrentWeapon == 13 || gCurrentWeapon == 18)) {
        return;
    }
    CameraGetDisplay(gCamera, &display);
    if ((gobj->npcState == 1 || gobj->npcState == 2) && *(s32 *)gobj->anim != 0) {
        WeaponGetTip(gobj, &D_80118FD0);
    } else if (GOBJ_TYPE(id) == 3) {
        VECTOR32 dir;
        EGSFrame *gun;

        gun = ((AnimFramesView *)gobj->anim)->frames[4];
        D_80118FD0.vx = gun->pos.vx;
        D_80118FD0.vy = gun->pos.vy;
        D_80118FD0.vz = gun->pos.vz;
        D_80118FD0.vy = -D_80118FD0.vy;
        dir.vx = gun->m[0][2];
        dir.vy = gun->m[1][2];
        dir.vz = gun->m[2][2];
        dir.vy = -dir.vy;
        dir.vx = EGSMathFIXMul(dir.vx, 0x100);
        dir.vy = EGSMathFIXMul(dir.vy, 0x100);
        dir.vz = EGSMathFIXMul(dir.vz, 0x100);
        D_80118FD0.vx += dir.vx;
        D_80118FD0.vy += dir.vy;
        D_80118FD0.vz += dir.vz;
    } else {
        return;
    }
    D_80118FC0.vx = D_80118FD0.vx - target->vx;
    D_80118FC0.vy = D_80118FD0.vy - target->vy;
    D_80118FC0.vz = D_80118FD0.vz - target->vz;
    GTEMath_VectorNormal(&D_80118FC0, &D_80118FC0);
    EGSGeometry_GetFuzzyDistancePoint2Point(&D_80118FD0, target, &dist);
    D_80118FC0.vx = -D_80118FC0.vx >> 3;
    D_80118FC0.vy = -D_80118FC0.vy >> 3;
    D_80118FC0.vz = -D_80118FC0.vz >> 3;
    steps = (dist - 0x200) >> 9;
    if (gobj->npc != NULL) {
        info = &gameObjectsInfo[gobj->id];
        weapon = GOBJINFO_WEAPON(info);
        shots = __builtin_abs(((NPCShotView *)gobj->npc)->shots);
        rate = (D_80105F58[weapon                                 ? weapon
                           : (GOBJINFO_FLAGS(info) & 0x3000) == 0x1000 ? 19
                           : (GOBJINFO_FLAGS(info) & 0x3000) == 0x2000 ? 20
                                                                       : 0]
                    .flags >>
                11) &
               7;
    } else {
        rate = (D_80105F58[gCurrentWeapon].flags >> 11) & 7;
        shots = ((NPCShotView *)gobj->npc)->shots; /* NULL here */
    }
    if (steps >= 0 && id != gPlayerOffset && rate > 0 && shots % rate == 0) {
        g = ParticlePoolAllocate(1, D_80110068);
        if (g != NULL) {
            g->updateType = 5;
            g->drawType = 3;
            line = &D_80119040[g->head];
            line->vel.vx = D_80118FC0.vx;
            line->vel.vy = D_80118FC0.vy << 12;
            line->vel.vz = D_80118FC0.vz;
            D_80110068 = g;
            g->unk26 = 0;
            D_80118FD0.vx = target->vx - steps * D_80118FC0.vx;
            D_80118FD0.vy = target->vy - steps * D_80118FC0.vy;
            D_80118FD0.vz = target->vz - steps * D_80118FC0.vz;
            line->wx = D_80118FD0.vx - D_80118FC0.vx;
            line->wy = D_80118FD0.vy - D_80118FC0.vy;
            line->wz = D_80118FD0.vz - D_80118FC0.vz;
            line->state = steps + 2;
            line->pos = D_80118FD0;
            line->color = 0x96C8;
            prim = line->prim;
            if (gSniperMode && gCurrentWeapon == 12) {
                line->color = 0x9600;
            }
            EGSDisplayBuildLineG(prim, 0, 0x190, 0x190, line->color);
            EGSDisplayAddPoly(display, prim);
        }
    }
    if (id == gPlayerOffset && D_8011007C + 24 < 0x78 && !(gSniperMode && gCurrentWeapon == 12)) {
        anim = (FootAnimView *)gobj->anim;
        light = (anim->light->r + anim->light->g + anim->light->b) >> 1;
        if (light > 0x1000) {
            light = 0x1000;
        }
        {
            GOBJ_MATRIX mat; /* the shell bone's rotation, y flipped */

            D_80118FD0.vx = anim->frames[8]->pos.vx;
            D_80118FD0.vy = ((FootAnimView *)gobj->anim)->frames[8]->pos.vy;
            D_80118FD0.vz = ((FootAnimView *)gobj->anim)->frames[8]->pos.vz;
            D_80118FD0.vy = -D_80118FD0.vy + 8;
            mat.m[0][0] = ((FootAnimView *)gobj->anim)->frames[8]->m[0][0];
            mat.m[0][1] = -((FootAnimView *)gobj->anim)->frames[8]->m[0][1];
            mat.m[0][2] = ((FootAnimView *)gobj->anim)->frames[8]->m[0][2];
            mat.m[1][0] = -((FootAnimView *)gobj->anim)->frames[8]->m[1][0];
            mat.m[1][1] = ((FootAnimView *)gobj->anim)->frames[8]->m[1][1];
            mat.m[1][2] = -((FootAnimView *)gobj->anim)->frames[8]->m[1][2];
            mat.m[2][0] = ((FootAnimView *)gobj->anim)->frames[8]->m[2][0];
            mat.m[2][1] = -((FootAnimView *)gobj->anim)->frames[8]->m[2][1];
            mat.m[2][2] = ((FootAnimView *)gobj->anim)->frames[8]->m[2][2];
            D_80118FC0.vx = mat.m[0][1];
            D_80118FC0.vy = mat.m[1][1];
            D_80118FC0.vz = mat.m[2][1];
        }
        D_80118FC0.vx = -D_80118FC0.vx;
        D_80118FC0.vy = -D_80118FC0.vy;
        D_80118FC0.vz = -D_80118FC0.vz;
        tmp = D_80118FC0.vz << 3;
        light9 = light * 9;
        D_80118FC0.vy = (rand() & 0x1FFF) | 0x8000;
        D_80118FC0.vz = (rand() & 0x1FFF) - (D_80118FC0.vx << 3);
        D_80118FC0.vx = tmp + (rand() & 0x1FFF);
        g = ParticlePoolAllocate(1, D_80110064);
        if (g != NULL) {
            VECTOR32 hand;

            p = &D_80119040[g->head];
            hand.vx = ((FootAnimView *)gobj->anim)->frames[7]->pos.vx;
            hand.vy = ((FootAnimView *)gobj->anim)->frames[7]->pos.vy;
            hand.vz = ((FootAnimView *)gobj->anim)->frames[7]->pos.vz;
            hand.vy = -hand.vy;
            D_80110064 = g;
            floor = D_80110088;
            if (hand.vy + 0x20 < floor) {
                floor = hand.vy + 0x20;
            }
            g->wind = -0x2000;
            g->updateType = 7;
            g->unk1C = floor;
            g->drawType = 2;
            p->pos = D_80118FD0;
            p->state = 60;
            p->vel.vx = D_80118FC0.vx >> 12;
            p->vel.vy = D_80118FC0.vy;
            p->vel.vz = D_80118FC0.vz >> 12;
            p->unk62 = rand() & 0x7FF;
            p->unk64 = 0x100;
            p->unk60 = 7;
            c = light9 >> 10;
            EGSDisplayBuildLineF(p->prim, (c << 16) + (((light9 >> 8) - c) << 8) + (light9 >> 8), 0x190, 0x190);
            EGSDisplayAddPoly(display, p->prim);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTracer);
#endif

static const VECTOR32 D_80012000 = { 0, 0, 0, 0 };

static const VECTOR32 D_80012010 = { 0, 0x1000, 0, 0 };

#define BONE_LEFT_FOOT 1
#define BONE_RIGHT_FOOT 7

/* Two splashes kicked sideways from the player's foot (right when foot != 0). */
void ParticleFootSplash(s32 foot) {
    VECTOR32 pos;
    VECTOR32 dir = D_80012010;
    VECTOR32 mask = {15, 0, 15};
    FootAnimView *anim;
    ParticleGroup *g;
    s32 light;
    s32 color;
    s32 bone;
    s32 tmp;

    if (D_8011007C + 16 < 0x78) {
        anim = (FootAnimView *)gObjPlayer->anim;
        light = (anim->light->r + anim->light->g + anim->light->b) >> 1;
        if (light > 0x1000) {
            light = 0x1000;
        }
        light = (light >> 6) + 0x40;
        color = ((light + (light >> 2)) << 16) + (light << 8) + light;
        bone = BONE_LEFT_FOOT;
        if (foot) {
            bone = BONE_RIGHT_FOOT;
        }
        pos.vx = anim->frames[bone]->pos.vx;
        pos.vy = ((FootAnimView *)gObjPlayer->anim)->frames[bone]->pos.vy;
        pos.vz = ((FootAnimView *)gObjPlayer->anim)->frames[bone]->pos.vz;
        pos.vy = -pos.vy;
        pos.vy = D_80110088; /* the foot height is overridden by the floor */
        ParticleInitAngle(0x555, 0, 0xFF);
        ParticleInitLifeColorFade(8, color);
        dir.vx = ((FootAnimView *)gObjPlayer->anim)->frame->m[0][2];
        dir.vy = ((FootAnimView *)gObjPlayer->anim)->frame->m[1][2];
        dir.vz = ((FootAnimView *)gObjPlayer->anim)->frame->m[2][2];
        dir.vy = -dir.vy;
        tmp = dir.vx;
        dir.vx = dir.vz;
        dir.vz = -tmp;
        D_801100B4 = 0x688;
        dir.vy = (gObjPlayer->cas->flags & 8) ? 0x200 : 0x100;
        pos.vx += dir.vx >> 9;
        pos.vz += dir.vz >> 9;
        ParticleInitEmitterMask(&pos, &mask);
        ParticleInitDirection(&dir, 0x7FF, 3);
        g = ParticlePoolAllocate(8, NULL);
        if (g != NULL) {
            g->unk23 = 2;
            g->wind = -0x2916;
            g->updateType = 8;
            g->drawType = 3;
            g->unk1C = D_80110088;
            ParticleStart(g, 8);
            g->unk26 = -10;
        }
        dir.vx = -dir.vx;
        dir.vz = -dir.vz;
        pos.vx += dir.vz >> 8;
        pos.vz += dir.vx >> 8;
        ParticleInitEmitterMask(&pos, &mask);
        ParticleInitDirection(&dir, 0x7FF, 3);
        g = ParticlePoolAllocate(8, NULL);
        if (g != NULL) {
            g->unk23 = 2;
            g->wind = -0x2916;
            g->updateType = 8;
            g->drawType = 3;
            g->unk1C = D_80110088;
            ParticleStart(g, 8);
            g->unk26 = -10;
        }
    }
}

/* View of GameObjectHealth past health. */
typedef struct HealthView {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s16 health;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s16 unk0C;
} HealthView;

/* Blood (and dust or a floor splash) where object id was hit. */
ParticleGroup *ParticleHitGobj(s32 arg0, s32 id, VECTOR32 *pos, VECTOR32 *normal) {
    VECTOR32 n;
    VECTOR32 at;
    VECTOR32 view;
    GameObjectRuntime *gobj = gameObjectsInfo[id].runtime;
    s32 behind = 0;
    PlayerAnimView *anim;
    ParticleGroup *g;
    ParticleGroup *g2;
    s32 side;
    s32 update;
    s32 draw;
    s32 unk0C;

    if (gSniperMode && id == gPlayerOffset) {
        return NULL;
    }
    anim = (PlayerAnimView *)gobj->anim;
    if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
        side = anim->unk16 == 1 ? 0x18 : 0;
    } else {
        side = -0x18;
    }
    GTEMath_VectorNormal(normal, &n);
    at.vx = pos->vx + (n.vx >> 10);
    at.vy = pos->vy + (n.vy >> 10);
    at.vz = pos->vz + (n.vz >> 10);
    ParticleInitEmitterPoint(&at);
    ParticleInitAngle(0x555, 0, 0xFF);
    ParticleInitLifeColorFade(15, 0xC8);
    if (id == gPlayerOffset) {
        view.vx = (*(EGSFrame **)*gCamera)->m[0][2];
        view.vy = (*(EGSFrame **)*gCamera)->m[1][2];
        view.vz = (*(EGSFrame **)*gCamera)->m[2][2];
        view.vy = -view.vy;
        behind = 8;
        if (n.vx * view.vx + n.vz * view.vz > 0) {
            side += 0x32;
        } else {
            side -= 0x14;
        }
    }
    D_801100B4 = behind | 0x10430;
    ParticleInitDirection(&n, 0x1FFF, 1);
    g = ParticlePoolAllocate(20, NULL);
    if (g != NULL) {
        g->unk23 = 8;
        g->wind = -0x400;
        g->updateType = update = 8;
        g->drawType = draw = 3;
        g->unk1C = D_80110088;
        ParticleStart(g, 20);
        if (arg0 >= 0) {
            g->gobjId = id;
            if (id != gPlayerOffset) {
                unk0C = ((HealthView *)gobj->healthInfo)->unk0C;
                g->unk26 = side;
                if (unk0C < 0x33 && unk0C < ((HealthView *)gobj->healthInfo)->health) {
                    return g;
                }
                if (D_80110088 < at.vy) {
                    ParticleInitLifeColor(12, 0xC8);
                    D_801100B4 = 0x10490;
                    ParticleInitDirection(&n, 0x7FFF, 0);
                    g2 = ParticlePoolAllocate(8, NULL);
                    if (g2 != NULL) {
                        g2->unk23 = 3;
                        g2->wind = -0x148B;
                        g2->updateType = update;
                        g2->drawType = draw;
                        g2->unk1C = D_80110088;
                        ParticleStart(g2, 8);
                        g2->unk26 = side;
                        g2->gobjId = id;
                    }
                }
            } else {
                g->unk26 = side;
                if (!(gobj->cas->flags & 2)) {
                    return g;
                }
                ParticleInitEmitterPoint(&at);
                ParticleInitAngle(0x555, 0, 0xFF);
                ParticleInitLifeColorFade(8, 0x80);
                D_801100B4 = 0x20228;
                ParticleInitDirection(&n, 0x1FF, 0);
                g2 = ParticlePoolAllocate(6, NULL);
                if (g2 != NULL) {
                    g2->unk23 = 6;
                    g2->wind = -0x10;
                    g2->unk1C = 0;
                    g2->updateType = 10;
                    g2->drawType = 0;
                    ParticleStart(g2, 6);
                    g2->unk26 = side;
                    g2->gobjId = id;
                }
            }
        }
    }
    return g;
}

/* Sparks (and dust when the object is in CAS state 2) off object id's flak jacket. */
ParticleGroup *ParticleHitFlakJacket(s32 arg0, s32 id, VECTOR32 *pos, VECTOR32 *normal) {
    VECTOR32 at;
    VECTOR32 n;
    VECTOR32 view;
    GameObjectRuntime *gobj = gameObjectsInfo[id].runtime;
    s32 behind = 0;
    PlayerAnimView *anim;
    ParticleGroup *g;
    ParticleGroup *dust;
    s32 side;

    if (gSniperMode && id == gPlayerOffset) {
        return NULL;
    }
    GTEMath_VectorNormal(normal, &n);
    at.vx = pos->vx + (n.vx >> 10);
    at.vy = pos->vy + (n.vy >> 10);
    at.vz = pos->vz + (n.vz >> 10);
    ParticleInitEmitterPoint(&at);
    ParticleInitAngle(0x555, 0, 0xFF);
    ParticleInitLifeColorFade(8, 0x406080);
    anim = (PlayerAnimView *)gobj->anim;
    if (!(anim->flags0B & 0x20) && anim->unk16 != -1) {
        side = anim->unk16 == 1 ? 0x18 : 0;
    } else {
        side = -0x18;
    }
    if (id == gPlayerOffset) {
        view.vx = (*(EGSFrame **)*gCamera)->m[0][2];
        view.vy = (*(EGSFrame **)*gCamera)->m[1][2];
        view.vz = (*(EGSFrame **)*gCamera)->m[2][2];
        view.vy = -view.vy;
        behind = 8;
        if (n.vx * view.vx + n.vz * view.vz > 0) {
            side += 0x32;
        } else {
            side -= 0x14;
        }
    }
    D_801100B4 = behind | 0x10420;
    ParticleInitDirection(&n, 0x1FFF, 2);
    g = ParticlePoolAllocate(8, NULL);
    if (g != NULL) {
        g->unk23 = 5;
        g->wind = -0x40;
        g->updateType = 8;
        g->unk1C = 0;
        g->drawType = 3;
        ParticleStart(g, 8);
        if (arg0 >= 0) {
            g->gobjId = id;
            g->unk24 = 3;
            g->unk26 = side;
            if (!(gobj->cas->flags & 2)) {
                return g;
            }
            ParticleInitEmitterPoint(&at);
            ParticleInitAngle(0x555, 0, 0xFF);
            ParticleInitLifeColorFade(5, 0x808080);
            D_801100B4 = behind | 0x20220;
            ParticleInitDirection(&n, 0x1FF, 0);
            dust = ParticlePoolAllocate(6, NULL);
            if (dust != NULL) {
                dust->unk23 = 6;
                dust->wind = -0x10;
                dust->unk1C = 0;
                dust->updateType = 10;
                dust->drawType = 0;
                ParticleStart(dust, 6);
                dust->unk26 = side;
                dust->gobjId = id;
            }
        }
    }
    return g;
}

/* Texture animation strip (gMultiTextures[], 0xC bytes each) */
/* libgs GsIMAGE */
typedef struct ParticleImage {
    /* 0x00 */ u32 pmode;
    /* 0x04 */ s16 px, py;
    /* 0x08 */ u16 pw, ph;
    /* 0x0C */ u32 *pixel;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u16 cw, ch;
    /* 0x18 */ u32 *clut;
} ParticleImage;

/* One animation frame (EGSTexture) */
typedef struct ParticleTexture {
    /* 0x00 */ u32 *tim;
    /* 0x04 */ void *slot;
    /* 0x08 */ u8 animated;
    /* 0x09 */ u8 loaded;
    /* 0x0C */ ParticleImage image;
    /* 0x28 */ s32 unk28;
} ParticleTexture; /* size 0x2C */

/* Particle display primitive (Particle + 0x28) as an EGSSprite (GsSPRITE) */
typedef struct ParticleGsSprite {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x, y;
    /* 0x08 */ u16 w, h;
    /* 0x0C */ u16 tpage;
    /* 0x0E */ u8 u, v;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u8 r, g, b;
    /* 0x18 */ s16 mx, my;
    /* 0x1C */ s16 scalex, scaley;
    /* 0x20 */ s32 rotate;
} ParticleGsSprite;

typedef struct MultiTexture {
    /* 0x00 */ s16 frames;
    /* 0x02 */ u8 unk02[6];
    /* 0x08 */ ParticleTexture *tex;
} MultiTexture;

extern MultiTexture gMultiTextures[];

ParticleGroup *iParticleExplosion(s32 unused, VECTOR32 *pos, s32 count, s32 type, s32 id, s32 size, s32 range) {
    VECTOR32 mask;
    VECTOR32 cam;
    VECTOR32 dir = D_80012000;
    ParticleGroup *g;
    Particle *p;
    s32 i;
    s32 next;
    s32 base;
    s32 mode;
    s32 scale;
    s32 size64;

    cam.vx = (*(EGSFrame **)*gCamera)->pos.vx;
    cam.vy = (*(EGSFrame **)*gCamera)->pos.vy;
    cam.vz = (*(EGSFrame **)*gCamera)->pos.vz;
    cam.vy = -cam.vy;
    if (id < 0) {
        dir.vx = pos->vx - cam.vx;
        dir.vy = pos->vy - cam.vy;
        dir.vz = pos->vz - cam.vz;
        GTEMath_VectorNormal(&dir, &dir);
        range = 0x3FF;
        dir.vx = -dir.vx >> 5;
        dir.vy = -dir.vy >> 5;
        dir.vz = -dir.vz >> 5;
    }
    size64 = size << 6;
    mask.vz = mask.vy = mask.vx = (size64 - size) >> 12;
    ParticleInitEmitterMask(pos, &mask);
    ParticleInitDirection(&dir, range, 0);
    g = ParticlePoolAllocate(count, NULL);
    if (g != NULL) {
        g->unk23 = 0x80;
        g->wind = -size << 8;
        g->unk1C = 0;
        g->updateType = 1;
        g->drawType = 4;
        ParticleStart(g, count);
        i = g->head;
        scale = (size64 + (size << 5)) >> 12;
        g->unk23 = scale;
        g->unk25 = 0;
        if (type == 0) {
            if (id < 0) {
                g->unk1C = 2;
                base = gMultiTextures[2].frames - 1;
            } else {
                g->unk1C = 2;
                base = gMultiTextures[2].frames - 5;
            }
            mode = 1;
        } else {
            if (type == 3) {
                g->unk1C = 1;
                base = gMultiTextures[1].frames - 1;
                mode = 1;
                g->wind = 0;
            } else {
                g->unk1C = type == 1 ? 3 : 4;
                base = gMultiTextures[g->unk1C].frames - 1;
                mode = 3;
            }
        }
        for (; i >= 0; i = next) {
            p = &D_80119040[i];
            next = p->next;
            if (p->pos.vy < pos->vy) {
                p->pos.vy = pos->vy * 2 - p->pos.vy;
            }
            p->vel.vy = p->vel.vy < 0 ? -p->vel.vy : p->vel.vy;
            p->state = (rand() & 7) + base;
            p->unk60 = p->state;
            EGSTextureSetSpriteTransparency(p->prim, mode);
            p->unk64 = base;
            p->unk62 = g->unk1C;
        }
        g->x = D_8011D150.vx;
        g->y = D_8011D150.vy;
        g->z = D_8011D150.vz;
        g->id = id;
        g->fade = 0;
    }
    return g;
}

ParticleGroup *ParticleExplosion(s32 id, VECTOR32 *pos, s32 unused, s32 size, s32 big) {
    VECTOR32 v;
    ParticleGroup *g;
    s32 objId;
    s32 onObject = 0;
    s32 count;
    s32 i;

    objId = id;
    if (id == GOBJ_ID_666) {
        objId = -1;
    }
    ParticleInitLifeColorNoInfrared(16, 0x808080);
    D_801100B4 = 16;
    count = 1;
    if (big) {
        count = 3;
    }
    if (objId >= 0 && GOBJ_TYPE(objId) == 0x6C) {
        onObject = 1;
        objId = -1;
    }
    g = iParticleExplosion(objId, pos, count, 0, 0, size, 0);
    if (g != NULL) {
        g->fade = 0x80000F00;
        if (onObject) {
            D_80119040[g->head].pos.vx = pos->vx;
            D_80119040[g->head].pos.vy = pos->vy;
            D_80119040[g->head].pos.vz = pos->vz;
            D_80119040[g->head].vel.vx = 0;
            D_80119040[g->head].vel.vy = -0x8000;
            D_80119040[g->head].vel.vz = 0;
        }
        if (big) {
            SysPlaySong(0, 0, 0, (s32)pos);
            g->flags |= 0x80000;
            if (id == GOBJ_ID_666 && D_8011007C + 24 < 0x78) {
                ParticleInitLifeColorNoInfrared(16, 0xFFFFFF);
                D_801100B4 = 0x810;
                for (i = 0; i < (rand() & 1) + 2; i++) {
                    g = iParticleExplosion(-1, pos, 9, 0, GOBJ_ID_666, 0x100, 0);
                    if (g != NULL) {
                        v.vx = (rand() & 0xFFF) - 0x7FF;
                        v.vz = (rand() & 0xFFF) - 0x7FF;
                        v.vy = 0;
                        GTEMath_VectorNormal(&v, &v);
                        g->dx = v.vx >> 2;
                        g->dz = v.vz >> 2;
                        v.vx = (*(EGSFrame **)*gCamera)->pos.vx;
                        v.vy = (*(EGSFrame **)*gCamera)->pos.vy;
                        v.vz = (*(EGSFrame **)*gCamera)->pos.vz;
                        v.vy = -v.vy;
                        v.vx -= pos->vx;
                        v.vy -= pos->vy;
                        v.vz -= pos->vz;
                        GTEMath_VectorNormal(&v, &v);
                        g->dx += v.vx >> 1;
                        g->dz += v.vz >> 1;
                        g->dx >>= 1;
                        g->dz >>= 1;
                        g->dy = ((rand() & 0x1FF) + 700) * 2;
                        g->wind = -0x2916;
                        g->fade = 0x800007FF;
                    }
                }
            }
        }
    }
    return g;
}

void ParticleSmoke(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4) {
    ParticleInitLifeColorNoInfrared(16, 0x808080);
    D_801100B4 = 16;
    iParticleExplosion(a0, a1, 2, a4, -1, a3, 0);
}

ParticleGroup *ParticleFire(s32 a0, s32 a1, s32 a2, s32 color, s32 r, s32 gr, s32 b) {
    s32 flag = 1;
    ParticleGroup *g;

    if (r == 0) {
        flag = gr == 0;
    }
    if (color < 0) {
        ParticleInitLifeColorNoInfrared(16, 0x808080);
    } else {
        ParticleInitLifeColorNoInfrared(16, color);
    }
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

typedef struct PathPoint {
    s16 x, y, z, pad;
} PathPoint;

typedef struct PathData {
    /* 0x00 */ s32 numPoints;
    /* 0x04 */ PathPoint *points;
} PathData;

extern s32 gLevelGameObjCount;
u8 LevelGetPathInfo(s32 id, s32 *velocity, PathData **path);

void ParticleFireShow(s16 id, VECTOR32 *pos, s32 size, s32 type) {
    s32 velocity;
    PathData *path = NULL;
    ParticleGroup *g;
    s32 div;
    s32 kind;
    s32 life;
    s32 i;
    Particle *p;

    if (id < gLevelGameObjCount) {
        LevelGetPathInfo(id, &velocity, &path);
    }
    if (path != NULL) {
        VECTOR32 v;

        ParticleInitLifeColorNoInfrared(16, 0x808080);
        v.vx = path->points[0].x;
        v.vy = path->points[0].y;
        v.vz = path->points[0].z;
        kind = 0;
        D_801100B4 = 0x41010;
        if (gSysInfo.level != 18 || type != 0) {
            div = 9;
            if (gSysInfo.level == 18) {
                size >>= 1;
                kind = -16;
            }
            g = iParticleExplosion(-1, &v, 9, type, id, size >> 1, 0);
        } else {
            g = iParticleExplosion(-1, &v, 2, 0, 0, size + size / 2, 0);
            g->unk27 = (rand() & 0xF) + (rand() & 7);
            div = 2;
            g->fade = !(id & 1) ? 0x801F7F7F : 0x80007F7F;
        }
        if (g != NULL) {
            v.vx = (path->points[1].x - v.vx) >> 4;
            v.vy = (path->points[1].y - v.vy) << 8;
            v.vz = (path->points[1].z - v.vz) >> 4;
            life = 1;
            for (i = g->head; i >= 0; i = p->next) {
                p = &D_80119040[i];
                p->vel = v;
                p->unk60 = 18;
                p->state = life;
                life += p->unk60 / div;
            }
            g->dx = v.vx;
            g->dy = v.vy >> 12;
            g->dz = v.vz;
            g->wind = 0;
            if (kind) {
                g->unk26 = kind;
            }
        }
    } else {
        ParticleInitLifeColorNoInfrared(16, 0x808080);
        D_801100B4 = 0x1010;
        g = iParticleExplosion(-1, pos, 2, type, id, size, 0);
        if (g != NULL) {
            g->unk26 = 16;
        }
    }
}

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

s32 ParticleNPCFireMove(u16 id, VECTOR32 *pos) {
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

void WeaponGetTip(GameObjectRuntime *gobj, VECTOR32 *tip);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);

void ParticleGrenadeLauncher(s32 id, VECTOR32 *target) {
    VECTOR32 dir;
    VECTOR32 tip;
    s32 dist;
    ParticleGroup *g;
    Particle *p;
    s32 steps;

    WeaponGetTip(gameObjectsInfo[id].runtime, &tip);
    dir.vx = target->vx - tip.vx;
    dir.vy = target->vy - tip.vy;
    dir.vz = target->vz - tip.vz;
    GTEMath_VectorNormal(&dir, &dir);
    EGSGeometry_GetFuzzyDistancePoint2Point(&tip, target, &dist);
    steps = dist / 128;
    if (steps <= 0) {
        steps = 1;
    }
    dir.vx <<= 7;
    dir.vy <<= 7;
    dir.vz <<= 7;
    g = ParticlePoolAllocate(1, NULL);
    if (g != NULL) {

        g->x = target->vx;
        g->y = target->vy;
        g->z = target->vz;
        g->unk16 = -1;
        g->updateType = 9;
        g->drawType = 5;
        g->owner = id;
        g->unk27 = steps;
        p = &D_80119040[g->head];
        p->pos.vx = tip.vx << 12;
        p->pos.vy = tip.vy << 12;
        p->pos.vz = tip.vz << 12;
        p->vel = dir;
        p->state = 1;
        EGSFrameSetPosition(D_80118FE0[0].frame, 0, &tip);
        EGSDisplayPointSourceParams(&D_80118FE0[0], 100, 0xFFFF);
        if (!D_80118FE0[0].on) {
            EGSDisplayPointSourceOn(&D_80118FE0[0]);
        }
        g->wind = dist >> 5;
        p->unk62 = 0;
        p->unk64 = 0x800 / steps;
    }
}

void ParticleUpdateGrenadeLauncher(ParticleGroup *g) {
    Particle *p = &D_80119040[g->head];
    VECTOR32 v;
    s32 s;
    s32 color;
    s16 angle;

    p->pos.vx += p->vel.vx;
    p->pos.vy += p->vel.vy;
    p->pos.vz += p->vel.vz;
    angle = p->unk62 + p->unk64;
    p->unk62 = angle;
    s = g->wind * rsin(angle);
    v.vx = p->pos.vx >> 12;
    v.vy = (p->pos.vy + s) >> 12;
    v.vz = p->pos.vz >> 12;
    color = (((rand() & 0x7F) + 0x20) << 8) + 0xFF;
    ParticleHitWall(-1, &v, &p->vel, color);
    EGSDisplayPointSourceParams(&D_80118FE0[0], 100, color);
    EGSFrameSetPosition(D_80118FE0[0].frame, 0, &v);
}

void EGSDisplaySetFlashTime(s32 ticks);

#ifdef NON_MATCHING
/* sched only: the original issues pos.vx += vel.vx >> 2 and the particleFireWaveFlag store before the m[1][2] load (6 insns).
 * Statement reorders of the vel loads/flag store make it much worse (30+); iCamDir/iCamPos inline helpers change nothing.
 * r4B: the sb must precede the `lw *D_8011008C` reload for m[2][2] (two non-struct accesses: sched keeps
 * their order), so the flag store comes before `vel.vz = ...` in the source; doing only that gets the sb
 * right but the vz pointer reload then lands in $v0 instead of reusing $a2 (5 lines).
 * r4E: in that variant the D_8011008C pseudo (79, block-local, REG_EQUIV mem) dies in the vz
 * `*D` load (insn 78) but local-alloc does not tie the load's dest (102) to it; the remaining
 * diff there is that tie plus the `vel.vx >> 2` sra placement. Seven other orders of the
 * vel/pos.vx/flag statements are all 29+ lines.
 * r4I: the vz frame reload ties to a2 in the original because v0 is busy with D_80110088 (pos.vy)
 * in its sched1 order; moving `pos.vy = D_80110088` (or a local copy of it) earlier in the source
 * does not change ours (5). Empty do-while(0) sched barriers between the statements are 34-43
 * lines: the LOOP_END note also ends the cse1 block, so *D_8011008C/pos.vx stop being CSE'd.
 * r6E (sched1 dumps): with the flag store before vel.vz (5 lines) the pos.vx addu (and its sra 18)
 * is birthing-boosted (single-set pseudo) and sinks next to the pos.vx store; the original keeps
 * it right after the vel.vx extension, so its pseudo is not single-set. A user var for pos.vx
 * (x = (*D)->pos.vx; pos.vx = x; ... x += vel.vx >> 2; pos.vx = x;) stops the boost but
 * shifts local-alloc (21); the D_80110088 load must also issue between the vz frame reload and
 * its lhu (it is boosted and LUID-tied with the lhu: needs a lower LUID, but moving pos.vy =
 * D_80110088 earlier is 32+). */
void ParticleFireWaveDeath(void) {
    VECTOR32 pos;
    VECTOR32 vel;
    VECTOR32 step;
    ParticleGroup *g;
    Particle *p;
    s32 i;
    s32 n;
    s32 life;

    if (!particleFireWaveFlag) {
        pos.vx = (*D_8011008C)->pos.vx;
        pos.vy = (*D_8011008C)->pos.vy;
        pos.vz = (*D_8011008C)->pos.vz;
        pos.vy = -pos.vy;
        vel.vx = (*D_8011008C)->m[0][2];
        vel.vy = (*D_8011008C)->m[1][2];
        vel.vz = (*D_8011008C)->m[2][2];
        vel.vy = -vel.vy;
        pos.vx += vel.vx >> 2;
        particleFireWaveFlag = 1;
        pos.vy = D_80110088;
        pos.vz += vel.vz >> 2;
        ParticleInitLifeColorNoInfrared(16, 0x808080);
        D_801100B4 = 0x3010;
        step.vx = -(vel.vz >> 3);
        step.vz = vel.vx >> 3;
        pos.vx -= step.vx;
        pos.vz -= step.vz;
        vel.vx = -vel.vx >> 7;
        vel.vz = -vel.vz >> 7;
        vel.vy = -vel.vy << 5;
        for (n = 0; n < 3; n++) {
            pos.vx += step.vx;
            pos.vz += step.vz;
            g = iParticleExplosion(-1, &pos, 3, -1, 0, 0x800, 0);
            if (g != NULL) {
                life = 1;
                for (i = g->head; i >= 0; i = p->next) {
                    p = &D_80119040[i];
                    p->vel = vel;
                    p->unk60 = 12;
                    p->state = life;
                    life += (s16)(p->unk60 / 3);
                }
                g->x = n - 1;
                g->wind = 0;
                g->unk26 = -0x7F;
                g->fade = 0x8000FF00;
            }
        }
        EGSDisplaySetFlashTime(100);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireWaveDeath);
#endif

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

s32 ParticleFindFloor(s32 y) {
    s32 floor;
    s32 i;
    s32 count;
    s16 *floors;

    if ((u16)gSysInfo.level < 16) {
        count = D_801062C4[gSysInfo.level];
        floor = y;
        i = 0;
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

ParticleGroup *ParticleGlassPoint(s32 unused, s32 id, VECTOR32 *pos) {
    VECTOR32 dir = D_80012010;
    GameObjectRuntime *gobj = gameObjectsInfo[id].runtime;
    ParticleGroup *g;
    ParticleGroup *flash;

    if (GOBJ_TYPE(id) == 0x4D) {
        ParticleInitEmitterPoint(pos);
        ParticleInitAngle(0x200, 0xFF, 0xFF);
        ParticleInitLifeColor(12, 0x204080);
        D_801100B4 = 0x282;
        ParticleInitDirection(&dir, 0x7FFF, 2);
        g = ParticlePoolAllocate(8, NULL);
        if (g != NULL) {
            g->unk23 = 4;
            g->unk1C = pos->vy - 16;
            g->wind = -0x2916;
            g->updateType = 10;
            g->drawType = 0;
            ParticleStart(g, 8);
        }
        dir.vy = 0x2000;
        ParticleInitAngle(0x555, 0, 0xFF);
        ParticleInitLifeColor(7, 0x6080C0);
        ParticleInitDirection(&dir, 0xFFF, 1);
        g = ParticlePoolAllocate(8, NULL);
        if (g != NULL) {
            g->unk23 = 5;
            g->unk1C = pos->vy - 16;
            g->wind = -0x2916;
            g->updateType = 10;
            g->drawType = 0;
            ParticleStart(g, 8);
            g->unk24 = 3;
        }
    } else if (GOBJ_TYPE(id) == 0x55) {
        VECTOR32 mask = { 7, 0x3F, 7 };
        s32 floor;

        dir.vy = 0x4000;
        ParticleInitEmitterMask(pos, &mask);
        ParticleInitAngle(0x555, 0xFF, 0xFF);
        ParticleInitLifeColor(10, 0x102040);
        D_801100B4 = 0x80;
        floor = pos->vy - 0x40;
        if (D_80110088 < floor) {
            floor = D_80110088;
        }
        ParticleInitDirection(&dir, 0x7FFF, 1);
        g = ParticlePoolAllocate(20, NULL);
        if (g != NULL) {
            g->unk23 = 5;
            g->wind = -0x2916;
            g->unk1C = floor;
            g->updateType = 10;
            g->drawType = 0;
            ParticleStart(g, 20);
            g->unk26 = -10;
        }
        ParticleInitLifeColor(10, 0xA152A);
        g = ParticlePoolAllocate(20, NULL);
        if (g != NULL) {
            g->unk23 = 5;
            g->wind = -0x2916;
            g->unk1C = floor;
            g->updateType = 10;
            g->drawType = 0;
            ParticleStart(g, 20);
            g->unk24 = 3;
            g->unk26 = -10;
        }
    } else {
        s32 floor;

        ParticleInitEmitterPoint(pos);
        floor = ParticleFindFloor(pos->vy);
        ParticleInitAngle(0x200, 0xFF, 0xFF);
        ParticleInitLifeColor(12, 0xEF9F9F);
        D_801100B4 = 0x202;
        ParticleInitDirection(&dir, 0x7FFF, 0);
        g = ParticlePoolAllocate(16, NULL);
        if (g != NULL) {
            g->unk23 = 8;
            g->wind = -0x2916;
            g->unk1C = floor;
            g->updateType = 4;
            g->drawType = 0;
            ParticleStart(g, 16);
            g->flags |= 2;
        }
        if (gobj->flags0 & 2) {
            ParticleInitAngle(0x555, 0, 0xFF);
            ParticleInitLifeColorFade(8, 0xFFFF);
            D_801100B4 = 2;
            ParticleInitDirection(&dir, 0xFFFF, 1);
            flash = ParticlePoolAllocate((rand() & 0xF) + 15, NULL);
            if (flash != NULL) {
                flash->unk23 = 3;
                flash->wind = -0x148B;
                flash->unk1C = floor;
                flash->updateType = 10;
                flash->drawType = 0;
                ParticleStart(flash, (rand() & 0xF) + 15);
            }
        }
    }
    return g;
}



/* Texture coordinate of one POLY_FT3 vertex plus the clut/tpage halfword that follows it */
typedef struct GlassUV {
    /* 0x00 */ u8 u;
    /* 0x01 */ u8 v;
    /* 0x02 */ u16 attr;
} GlassUV;

/* Textured triangle as built by the display code (POLY_FT3 behind an 8-byte header) */
typedef struct GlassPoly {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ GlassUV uv0; /* attr: clut */
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ GlassUV uv1; /* attr: tpage */
    /* 0x20 */ u8 unk20[4];
    /* 0x24 */ GlassUV uv2;
} GlassPoly;

void particleGlassTexture(GlassPoly *poly, s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2) {
    GlassUV *uv;
    s32 page;
    s32 u;
    s32 v;

    u = D_801100F8 + (x0 - D_80110108) * D_80110100 / D_80110110;
    v = D_801100FC + D_80110104 - (y0 - D_8011010C) * D_80110104 / D_80110114;
    uv = &poly->uv0;
    page = D_801100F0 & 0x1F;
    uv->u = (u - (page < 16 ? page << 6 : (page - 16) << 6)) * 2;
    uv->v = v;
    uv->attr = D_801100F4;
    u = D_801100F8 + (x1 - D_80110108) * D_80110100 / D_80110110;
    v = D_801100FC + D_80110104 - (y1 - D_8011010C) * D_80110104 / D_80110114;
    uv = &poly->uv1;
    uv->u = (u - (page < 16 ? page << 6 : (page - 16) << 6)) * 2;
    uv->v = v;
    uv->attr = D_801100F0;
    u = D_801100F8 + (x2 - D_80110108) * D_80110100 / D_80110110;
    v = D_801100FC + D_80110104 - (y2 - D_8011010C) * D_80110104 / D_80110114;
    uv = &poly->uv2;
    uv->u = (u - (page < 16 ? page << 6 : (page - 16) << 6)) * 2;
    uv->v = v;
}

/* View of a Particle used as a glass shard */
typedef struct GlassShard {
    /* 0x00 */ VECTOR32 pos; /* centre */
    /* 0x10 */ u8 unk10[0x18];
    /* 0x28 */ GlassPoly poly;
    /* 0x50 */ s16 corner[3][3]; /* per corner: angle, radius, offset along the pane normal */
    /* 0x62 */ s16 spin;
} GlassShard;

int ratan2(int y, int x);
int SquareRoot0(int a);

void ParticleBuildGlass(GlassShard *shard, VECTOR32 *a, VECTOR32 *b, VECTOR32 *c, s32 plane, s32 textured) {
    s32 tx, ty, tz;
    s32 cx, cy, cz;
    s32 x0, y0, z0;
    s32 x1, y1, z1;
    s32 x2, y2, z2;
    s32 scale;

    tx = (a->vx + (b->vx >> 1) + c->vx) >> 1;
    ty = (a->vy + (b->vy >> 1) + c->vy) >> 1;
    tz = (a->vz + (b->vz >> 1) + c->vz) >> 1;
    shard->pos.vx = cx = (a->vx + (b->vx >> 1) + tx) >> 1;
    shard->pos.vy = cy = (a->vy + (b->vy >> 1) + ty) >> 1;
    shard->pos.vz = cz = (a->vz + (b->vz >> 1) + tz) >> 1;
    x0 = a->vx - cx;
    y0 = a->vy - cy;
    z0 = a->vz - cz;
    x1 = x0 + b->vx;
    y1 = y0 + b->vy;
    z1 = z0 + b->vz;
    scale = (rand() & 0xF) + 8;
    x2 = (tx - cx) * scale >> 3;
    y2 = (ty - cy) * scale >> 3;
    z2 = (tz - cz) * scale >> 3;
    if (plane == 3) {
        if (textured) {
            particleGlassTexture(&shard->poly, cx + x0, cy + y0, cx + x1, cy + y1, cx + x2, cy + y2);
        }
        shard->corner[0][0] = ratan2(y0, x0);
        shard->corner[0][1] = SquareRoot0(y0 * y0 + x0 * x0);
        shard->corner[0][2] = z0;
        shard->corner[1][0] = ratan2(y1, x1);
        shard->corner[1][1] = SquareRoot0(y1 * y1 + x1 * x1);
        shard->corner[1][2] = z1;
        shard->corner[2][0] = ratan2(y2, x2);
        shard->corner[2][1] = SquareRoot0(y2 * y2 + x2 * x2);
        shard->corner[2][2] = z2;
    } else if (plane == 6) {
        if (textured) {
            particleGlassTexture(&shard->poly, cz + z0, cy + y0, cz + z1, cy + y1, cz + z2, cy + y2);
        }
        shard->corner[0][0] = ratan2(y0, z0);
        shard->corner[0][1] = SquareRoot0(y0 * y0 + z0 * z0);
        shard->corner[0][2] = x0;
        shard->corner[1][0] = ratan2(y1, z1);
        shard->corner[1][1] = SquareRoot0(y1 * y1 + z1 * z1);
        shard->corner[1][2] = x1;
        shard->corner[2][0] = ratan2(y2, z2);
        shard->corner[2][1] = SquareRoot0(y2 * y2 + z2 * z2);
        shard->corner[2][2] = x2;
    }
    shard->spin = 0;
}

/* Bit-length mask of |x| via the GTE leading-zero counter (the mfc2 result is an input
 * operand, as in ParticleInitLifeColorFade, so the shift folds to 0).
 * shift = 0 at function scope: the pseudo spans blocks, gets no hard reg and is reloaded
 * from its REG_EQUIV constant (into v1) right before the one asm that uses it, so mtc2 and
 * mfc2 must be one asm statement for the `li v1,0` to land before the mtc2. */
s32 func_80052D90(s32 x) {
    s32 shift = 0;
    s32 ret = 0;

    if (x < 0) {
        x = -x;
    }
    if (x > 0) {
        __asm__ volatile("mtc2 %1, $30\n\tnop\n\tnop\n\tmfc2 %0, $31" : : "r"(shift), "r"(x));
        ret = 0x7FFFFFFF >> shift;
    }
    return ret;
}

u16 GetTPage(int tp, int abr, int x, int y);
void ParticleInitEmitterXZPlane(VECTOR32 *min, VECTOR32 *max, VECTOR32 *size);

/* glass object type that is not pushed out along its normal */
#define GOBJ_TYPE_GLASS_FIXED 0x31

ParticleGroup *ParticleGlassPaneWall(s32 shooter, s32 id, VECTOR32 *min, VECTOR32 *max, VECTOR32 *dir) {
    VECTOR32 center;
    VECTOR32 size;
    VECTOR32 push = D_80012000;
    VECTOR32 delta;
    VECTOR32 pos;
    VECTOR32 edge;
    s32 plane;
    s32 rows;
    s32 cols;
    s32 textured = 0;
    s32 stepX;
    s32 stepY;
    s32 stepZ;
    s32 spin;
    s32 dx, dy, dz;
    s32 floor;
    ParticleGroup *g;
    ParticleGroup *dust;
    Particle *p;
    s32 i;
    s32 n;
    s32 r;
    s32 s;
    s32 s2;
    GameObjectRuntime *gobj;

    if (gSysInfo.level == 0) {
        if ((id >= 9 && id <= 18) || (id >= 0x9D && id <= 0xA0)) {
            D_801100F8 = 0x240;
            D_801100FC = 0xC0;
            D_80110100 = 0xF;
            D_80110104 = 0x3F;
            D_801100F0 = GetTPage(1, 1, 0x240, 0xC0);
            textured = 0x8000;
            D_801100F4 = 0x79B0;
        } else if ((id >= 0x21 && id <= 0x24) || (id >= 0xE9 && id <= 0xED)) {
            D_801100F8 = 0x1B0;
            D_801100FC = 0xC0;
            D_80110100 = 0xF;
            D_80110104 = 0x3F;
            D_801100F0 = GetTPage(1, 1, 0x1B0, 0xC0);
            textured = 0x8000;
            D_801100F4 = 0x7A30;
        }
    }
    dx = __builtin_abs(max->vx - min->vx);
    dy = max->vy - min->vy;
    if (GOBJ_TYPE(id) == GOBJ_TYPE_GLASS_FIXED) {
        floor = dy > 0 ? min->vy : max->vy;
    } else {
        floor = dy > 0 ? min->vy : max->vy;
        floor = ParticleFindFloor(floor);
    }
    dz = __builtin_abs(max->vz - min->vz);
    size.vx = func_80052D90(dx);
    size.vy = func_80052D90(dy);
    size.vz = func_80052D90(dz);
    center.vx = (min->vx + max->vx) >> 1;
    center.vy = (min->vy + max->vy) >> 1;
    center.vz = (min->vz + max->vz) >> 1;
    if (shooter >= 0) {
        gobj = gameObjectsInfo[shooter].runtime;
        pos.vx = gobj->anim->frame->pos.vx;
        pos.vy = gobj->anim->frame->pos.vy;
        pos.vz = gobj->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        delta.vx = pos.vx - center.vx;
        delta.vy = pos.vy - center.vy;
        delta.vz = pos.vz - center.vz;
    } else {
        delta = *dir;
    }
    if (dz < dx) {
        plane = 3;
        if (GOBJ_TYPE(id) != GOBJ_TYPE_GLASS_FIXED) {
            push.vz = delta.vz > 0 ? -0x4000 : 0x4000;
        }
        if (textured) {
            D_80110110 = dx;
            D_80110114 = dy;
            D_80110108 = min->vx;
            D_8011010C = min->vy;
        }
    } else {
        plane = 6;
        if (GOBJ_TYPE(id) != GOBJ_TYPE_GLASS_FIXED) {
            push.vx = delta.vx > 0 ? -0x4000 : 0x4000;
        }
        if (textured) {
            D_80110110 = dz;
            D_80110114 = dy;
            D_80110108 = min->vz;
            D_8011010C = min->vy;
        }
    }
    rows = (rand() & 1) + 2;
    cols = rows - (rand() & 1);
    if (cols < 2) {
        cols = 1;
    }
    ParticleInitEmitterXZPlane(min, max, &size);
    ParticleInitAngle(0x555, 0, 0);
    if (textured == 0) {
        ParticleInitLifeColor(1, 0xEF9F9F);
    } else {
        ParticleInitLifeColor(1, 0x3F3F3F);
    }
    D_801100B4 = textured | 0x202;
    ParticleInitDirection(&push, 0x1FF, 0);
    g = ParticlePoolAllocate((rows + cols) * 2, NULL);
    if (g != NULL) {
        g->unk23 = 16;
        g->wind = -0x2916;
        g->updateType = 4;
        g->unk1C = floor;
        g->drawType = 1;
        ParticleStart(g, (rows + cols) * 2);
    }
    if (D_8011007C + 12 < 0x78) {
        ParticleInitEmitterMask(&center, &size);
        ParticleInitAngle(0x200, 3, 0x1F);
        D_801100B4 = 0x282;
        ParticleInitDirection(&push, 0, 0);
        dust = ParticlePoolAllocate(6, NULL);
        if (dust != NULL) {
            dust->unk23 = size.vy >> 3;
            dust->wind = -0x2996;
            dust->unk1C = floor;
            dust->updateType = 10;
            dust->drawType = 0;
            ParticleStart(dust, 6);
        }
        dust = ParticlePoolAllocate(6, NULL);
        if (dust != NULL) {
            dust->unk23 = size.vy >> 3;
            dust->wind = -0x2816;
            dust->unk1C = floor;
            dust->updateType = 10;
            dust->drawType = 0;
            ParticleStart(dust, 6);
        }
    }
    if (g != NULL && (i = g->head) >= 0) {
        g->unk25 = plane;
        spin = (rand() & 0x3F) - 0x1F;
        stepY = (max->vy - min->vy) / rows;
        pos.vy = min->vy;
        for (n = 0; n < rows; n++) {
            p = &D_80119040[i];
            r = (rand() & 7) + 7;
            pos.vx = min->vx;
            pos.vz = min->vz;
            edge.vx = 0;
            edge.vz = 0;
            edge.vy = r * stepY >> 4;
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, plane, textured);
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            s = spin >> (r & 1);
            p->unk64 = s;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
            p = &D_80119040[i];
            pos.vx = max->vx;
            pos.vz = max->vz;
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, plane, textured);
            pos.vy += stepY;
            p->unk64 = s;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
        }
        stepX = (max->vx - min->vx) / cols;
        stepZ = (max->vz - min->vz) / cols;
        pos.vx = min->vx;
        pos.vz = min->vz;
        for (n = 0; n < cols; n++) {
            p = &D_80119040[i];
            pos.vy = min->vy;
            r = (rand() & 7) + 7;
            edge.vy = 0;
            edge.vx = r * stepX >> 4;
            edge.vz = r * stepZ >> 4;
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, plane, textured);
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            s2 = spin >> (r & 1);
            p->unk64 = s2;
            pos.vy = max->vy;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
            p = &D_80119040[i];
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, plane, textured);
            pos.vx += stepX;
            pos.vz += stepZ;
            p->unk64 = s2;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
        }
    }
    return g;
}

/* glass object type with a textured ceiling pane */
#define GOBJ_TYPE_GLASS_CEILING 0x56

ParticleGroup *ParticleGlassPaneCeiling(s32 unused, s32 id, VECTOR32 *min, VECTOR32 *max) {
    VECTOR32 center;
    VECTOR32 size;
    s32 velocity;
    PathData *path;
    s32 rows;
    s32 cols;
    s32 textured;
    s32 stepX;
    s32 stepY;
    s32 stepZ;
    s32 spin;
    s32 dx, dy, dz;
    s32 floor;
    ParticleGroup *g;
    ParticleGroup *dust;
    Particle *p;
    s32 i;
    s32 n;
    s32 r;
    s32 s;
    s32 s2;

    path = NULL;
    LevelGetPathInfo(id, &velocity, &path);
    textured = 0;
    if (path != NULL) {
        floor = path->points[1].y;
    } else {
        floor = ParticleFindFloor(min->vy);
    }
    if (GOBJ_TYPE(id) == GOBJ_TYPE_GLASS_CEILING) {
        textured = 0x8000;
        floor = -0x1BC;
    }
    dx = __builtin_abs(max->vx - min->vx);
    dy = __builtin_abs(max->vy - min->vy);
    dz = __builtin_abs(max->vz - min->vz);
    size.vx = func_80052D90(dx);
    size.vy = func_80052D90(dy);
    size.vz = func_80052D90(dz);
    center.vx = (min->vx + max->vx) >> 1;
    center.vy = (min->vy + max->vy) >> 1;
    center.vz = (min->vz + max->vz) >> 1;
    if (textured) {
        D_80110110 = dx;
        D_80110114 = dz;
        D_80110108 = min->vx;
        D_8011010C = min->vz;
    }
    rows = (rand() & 3) + 1;
    cols = (rand() & 3) + 1;
    ParticleInitEmitterMask(&center, &size);
    ParticleInitAngle(0x555, 0, 0);
    ParticleInitLifeColor(1, 0xEF9F9F);
    D_801100B4 = textured | 0x202;
    ParticleInitDirection(&D_80106428, 0, 0);
    g = ParticlePoolAllocate((rows + cols) * 2, NULL);
    if (g != NULL) {
        g->unk23 = 16;
        g->wind = -0x2916;
        g->updateType = 4;
        g->unk1C = floor;
        g->drawType = 1;
        ParticleStart(g, (rows + cols) * 2);
    }
    if (D_8011007C + 12 < 0x78) {
        ParticleInitEmitterMask(&center, &size);
        ParticleInitAngle(0x200, 3, 0x1F);
        D_801100B4 = 0x282;
        dust = ParticlePoolAllocate(6, NULL);
        if (dust != NULL) {
            dust->unk23 = size.vx >> 4;
            dust->wind = -0x2996;
            dust->unk1C = floor;
            dust->updateType = 10;
            dust->drawType = 0;
            ParticleStart(dust, 6);
        }
        dust = ParticlePoolAllocate(6, NULL);
        if (dust != NULL) {
            dust->unk23 = size.vx >> 3;
            dust->unk1C = floor;
            dust->wind = -0x2816;
            dust->updateType = 10;
            dust->drawType = 0;
            ParticleStart(dust, 6);
        }
    }
    if (g != NULL && (i = g->head) >= 0) {
        VECTOR32 pos;
        VECTOR32 edge;

        g->unk25 = 3;
        spin = (rand() & 0x3F) - 0x1F;
        stepY = (max->vy - min->vy) / rows;
        stepX = (max->vx - min->vx) / rows;
        pos.vx = min->vx;
        pos.vy = min->vy;
        for (n = 0; n < rows; n++) {
            p = &D_80119040[i];
            pos.vz = min->vz;
            r = (rand() & 7) + 7;
            edge.vz = 0;
            edge.vx = r * stepX >> 4;
            edge.vy = r * stepY >> 4;
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, 3, textured);
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            s = spin >> (r & 1);
            p->unk64 = s;
            pos.vz = max->vz;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
            p = &D_80119040[i];
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, 3, textured);
            pos.vx += stepX;
            pos.vy += stepY;
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            p->unk64 = s;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
        }
        stepY = (max->vy - min->vy) / cols;
        stepZ = (max->vz - min->vz) / cols;
        pos.vy = min->vy;
        pos.vz = min->vz;
        for (n = 0; n < cols; n++) {
            p = &D_80119040[i];
            pos.vx = min->vx;
            r = (rand() & 7) + 7;
            edge.vx = 0;
            edge.vy = r * stepY >> 4;
            edge.vz = r * stepZ >> 4;
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, 6, textured);
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            s2 = spin >> (r & 1);
            p->unk64 = s2;
            pos.vx = max->vx;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
            p = &D_80119040[i];
            ParticleBuildGlass((GlassShard *)p, &pos, &edge, &center, 6, textured);
            pos.vy += stepY;
            pos.vz += stepZ;
            p->vel.vy = ((rand() & 0xF) + 1) << 11;
            p->unk64 = s2;
            i = D_80119040[i].next;
            if (i < 0) {
                return g;
            }
        }
    }
    return g;
}

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

void ParticleGroupUpdateWind(ParticleGroup *g) {
    s32 c = rcos(D_8011003C);
    s32 s = rsin(D_80110040);
    s32 angle = (s >> 5) + 0x200;
    s32 bias;
    s32 mag = (s + (bias = c + 0x4000)) >> 6;

    c = rcos(angle);
    s = rsin(angle);
    D_8011003C += 3;
    D_80110040 += 4;
    D_8011D1C0.vx = (mag * c) >> 12;
    D_8011D1C0.vz = (mag * s) >> 12;
}

void ParticleGroupUpdateRain(ParticleGroup *g) {
    s32 a = rsin(D_80110044);
    s32 b = rsin(D_80110048);
    s32 bias;

    g->wind = (a >> 3) + (bias = (b >> 3) - 0x6000);
    D_80110044 += 0xC0;
    D_80110048 += 0x100;
    ParticleGroupUpdateWind(g);
}

void ParticleUpdateFragmentMeterFrictionFloor(ParticleGroup *g) {
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    s32 range;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (--p->state == 0) {
            ParticleFree(g, p, prev, i);
        } else {
            p->pos.vx += p->vel.vx;
            p->pos.vy += p->vel.vy >= 0 ? p->vel.vy >> 12 : -(-p->vel.vy >> 12);
            p->pos.vz += p->vel.vz;
            p->vel.vy += g->wind;
            p->vel.vx -= (p->vel.vx + 1) >> 4;
            p->vel.vz -= (p->vel.vz + 1) >> 4;
            if (p->pos.vy < g->unk1C) {
                p->vel.vy = -p->vel.vy >> ((rand() & 1) + 2);
                range = func_80052D90(p->vel.vy >> 11);
                p->vel.vx = (rand() & range) - (range >> 1);
                p->vel.vz = (rand() & range) - (range >> 1);
                p->pos.vy = g->unk1C;
            }
            p->unk62 += p->unk64;
            prev = i;
        }
        i = next;
    }
}

#ifdef NON_MATCHING
/* regalloc: prev and p swap s0/s1; min-loop head lands in a0 */
/* r6E: p (128 refs) is allocated first and so always gets s0; prev (7 refs) can only reach s0 if
 * p is split or loses priority. A separate block-local pointer for the min loop is worse (113). */
/* Explosion fireballs: animated sprites respawned around the (drifting) group centre */
void ParticleUpdateExplosion(ParticleGroup *g) {
    VECTOR32 cam;
    VECTOR32 dir;
    Particle *p;
    s32 prev = 0;
    s32 set = 0;
    s32 i = g->head;
    u32 fade = g->fade;
    s32 next;
    s32 j;
    s32 min;
    s32 c;
    s32 range;
    s32 spread;
    s32 offset;
    s32 frame;
    s32 mode;
    s32 shift;
    s32 tpage;
    s32 vy;
    s32 drag;
    ParticleGsSprite *sprite;
    ParticleTexture *tex;

    if ((s32)fade < 0) {
        if (fade & 0xFF0000) {
            set = 3;
            g->fade = fade - 0x10000;
        } else if (fade & 0xFF00) {
            set = 2;
            if ((fade & 0xFF00) != 0xFF00) {
                g->fade = fade - 0x100;
            }
            if (!(g->flags & 0x800)) {
                if ((g->id & 0x3FFF) < 0xFFF && D_80110094 != g) {
                    D_80110094 = g;
                    D_8011009C = g->unk23;
                    D_8011D140 = D_80119040[i].pos;
                }
                if (g->id == 0) {
                    D_8011D140 = D_80119040[i].pos;
                }
            }
        } else {
            set = 3;
            if ((fade & 0xFF) != 0xFF && fade != 0x80000000) {
                g->fade = fade - 1;
            }
            if (D_80110094 == g) {
                D_80110094 = NULL;
            }
        }
    }
    if (g->flags & 0x800) {
        g->x += g->dx >= 0 ? g->dx >> 6 : -(-g->dx >> 6);
        g->y += g->dy >= 0 ? g->dy >> 6 : -(-g->dy >> 6);
        g->z += g->dz >= 0 ? g->dz >> 6 : -(-g->dz >> 6);
        g->dy += g->wind >> 7;
        if (g->y < D_80110088) {
            g->fade = 0x80000000;
            g->y = D_80110088;
        }
        min = 100;
        for (c = j = g->head; c >= 0; c = p->next) {
            p = &D_80119040[c];
            if (--p->state < min) {
                j = c;
                min = p->state;
            }
        }
        D_80119040[j].state = 0;
        c = (g->y - D_80110088) >> 1;
        if (c >= 0x100) {
            c = 0xFF;
        }
        D_80119040[j].color = (c << 16) | (c << 8) | c;
    }
    while (i >= 0) {
        p = &D_80119040[i];
        p->state--;
        next = p->next;
        if (g->id >= 0) {
            if (p->state > 0) {
                goto show;
            }
            if (g->fade == 0x80000000) {
                ParticleFree(g, p, prev, i);
                i = next;
                continue;
            }
            if ((s32)g->fade < 0) {
                p->unk62 = set;
                if (set == 3) {
                    EGSTextureSetSpriteTransparency(p->prim, 3);
                    p->unk64 = gMultiTextures[3].frames - 1;
                } else {
                    if (!(g->flags & 0x800)) {
                        EGSTextureSetSpriteTransparency(p->prim, 1);
                    } else {
                        EGSTextureSetSpriteTransparency(p->prim, 3);
                    }
                    p->unk64 = gMultiTextures[set].frames - 5;
                }
                if (!(g->fade & 0xFFFF00)) {
                    g->unk23 -= g->unk23 >> 4;
                }
            }
            p->state = p->unk60 = p->unk64 + (rand() & 7);
            range = g->unk23 * 127;
            spread = range >> 7;
            offset = range >> 8;
            p->pos.vx = (spread & rand()) - offset + g->x;
            p->pos.vz = (spread & rand()) - offset + g->z;
            if (g->flags & 0x2000) {
                cam.vx = (*D_8011008C)->pos.vx;
                cam.vy = (*D_8011008C)->pos.vy;
                cam.vz = (*D_8011008C)->pos.vz;
                cam.vy = -cam.vy;
                dir.vx = (*D_8011008C)->m[0][2];
                dir.vy = (*D_8011008C)->m[1][2];
                dir.vz = (*D_8011008C)->m[2][2];
                dir.vy = -dir.vy;
                p->pos.vx = cam.vx + (dir.vx >> 4);
                p->pos.vz = cam.vz + (dir.vz >> 4);
                p->pos.vy = cam.vy + (dir.vy >> 4);
                if (g->x < 0) {
                    p->pos.vx -= dir.vz >> 5;
                    p->pos.vz += dir.vx >> 5;
                }
                if (g->x > 0) {
                    p->pos.vx += dir.vz >> 5;
                    p->pos.vz -= dir.vx >> 5;
                }
                p->vel.vx = (rand() & 0xF) - 7;
                p->vel.vy = ((rand() & 0xF) - 7) << 12;
                p->vel.vz = (rand() & 0xF) - 7;
            } else if (g->flags & 0x40000) {
                if (g->unk1C != 1) {
                    p->pos.vy = (spread & rand()) - offset + g->y;
                    spread = range >> 9;
                    range >>= 10;
                    p->vel.vx = g->dx + (spread & rand()) - range;
                    p->vel.vy = (g->dy + (spread & rand()) - range) << 12;
                    p->vel.vz = g->dz + (spread & rand()) - range;
                }
            } else {
                range >>= 9;
                p->pos.vy = (spread & rand()) + g->y;
                p->vel.vx = (offset & rand()) - range;
                p->vel.vy = (offset & rand()) << 10;
                p->vel.vz = (offset & rand()) - range;
            }
        } else if (p->state <= 0) {
            ParticleFree(g, p, prev, i);
            i = next;
            continue;
        }
    show:
        frame = p->unk64 * p->state / p->unk60;
        sprite = (ParticleGsSprite *)p->prim;
        if ((s32)g->fade >= 0) {
            set = g->unk1C;
        } else {
            set = p->unk62;
        }
        tex = &gMultiTextures[set].tex[p->unk64 - frame];
        mode = tex->image.pmode & 3;
        shift = 2 - mode;
        tpage = GetTPage(mode, 0, tex->image.px, tex->image.py) & 0x1F;
        if (tpage < 16) {
            sprite->u = (tex->image.px - tpage * 64) << shift;
        } else {
            sprite->u = (tex->image.px - (tpage - 16) * 64) << shift;
        }
        sprite->v = tex->image.py;
        sprite->tpage = tpage;
        sprite->w = tex->image.pw << shift;
        sprite->h = tex->image.ph;
        sprite->mx = sprite->w >> 1;
        sprite->my = sprite->h >> 1;
        sprite->cx = tex->image.cx;
        sprite->cy = tex->image.cy;
        sprite->attribute |= 0x80000000 | (mode << 24);
        if (!(g->flags & 0x800)) {
            p->pos.vx += p->vel.vx;
            p->pos.vy += (vy = p->vel.vy + g->wind) >= 0 ? vy >> 12 : -(-vy >> 12);
            p->pos.vz += p->vel.vz;
            if (g->flags & 0x10) {
                drag = (g->flags & 0x1000) ? 4 : 2;
                p->vel.vx -= (p->vel.vx + 1) >> drag;
                p->vel.vy -= (p->vel.vy + 1) >> drag;
                p->vel.vz -= (p->vel.vz + 1) >> drag;
            }
        }
        prev = i;
        i = next;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateExplosion);
#endif

/* GPU code word: r, g, b and the primitive code in the top byte */
typedef union ParticleRGBC {
    u32 word;
    struct {
        u8 r, g, b, code;
    } c;
} ParticleRGBC;

/* Particle display primitive (Particle + 0x28) drawn as a flat triangle */
typedef struct ParticlePolyF3 {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ ParticleRGBC rgbc0;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
    /* 0x18 */ s32 xy2;
} ParticlePolyF3;


/* Same layout as libgte MATRIX (egsmath.h conflicts with this file's prototypes) */
typedef struct CameraMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x14 */ s32 t[3];
} CameraMatrix;

/* Particle display primitive (Particle + 0x28) drawn as a scaled sprite */
typedef struct ParticleSprite {
    /* 0x00 */ u32 tag;
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ u8 unk08[0xC];
    /* 0x14 */ u8 r;
    /* 0x15 */ u8 g;
    /* 0x16 */ u8 b;
    /* 0x17 */ u8 unk17[5];
    /* 0x1C */ s16 w;
    /* 0x1E */ s16 h;
    /* 0x20 */ u8 unk20[4];
    /* 0x24 */ s32 z;
} ParticleSprite;

typedef struct {
    s16 x, y;
} ParticleScreenPos;
extern ParticleScreenPos POSITION; /* screen centre */

void EGSDisplayAddSprite(void *display, void *prim);

#define CAMERA_MATRIX (*(CameraMatrix **)*gCamera)
#define SPRITE(p) ((ParticleSprite *)(p)->prim)

/* Glows: camera-facing sprites sized by the projected height of the group.
 * The delete/add loops use their own block-local pointers (e, f) and the main loop q
 * (`i = q->next`, as in ParticleDisplayPolyWorld). The do-while(0) around the tag store
 * raises sprite's loop-weighted refs so it outranks q for s0. */
static inline s16 ScreenOX(void) { return POSITION.x - 0xC0; }
static inline s16 ScreenOY(void) { return POSITION.y - 0x78; }
#define SCREEN_OX ScreenOX()
#define SCREEN_OY ScreenOY()
static inline s16 ScreenY(s32 vy) { vy = -vy; return vy - ScreenOY(); }
void ParticleDisplayPolyFlat4(ParticleGroup *g) {
    VECTOR32 screen;
    void *display[2];
    VECTOR32 pos;
    VECTOR32 up;
    CameraMatrix m;
    void *display2[6];
    s32 i;
    s32 height;
    s32 infrared;
    s32 prev;
    s32 added;
    s32 scale;
    s32 top;
    s32 w;
    s32 depth;
    s32 bias;
    Particle *p;
    Particle *q;
    Particle *e;
    Particle *f;
    ParticleSprite *sprite;

    CameraGetDisplay(gCamera, display);
    i = g->head;
    height = g->unk23;
    infrared = ((u8 *)display[0])[6] & 0x10;
    m.m[0][0] = CAMERA_MATRIX->m[0][0];
    m.m[0][1] = -CAMERA_MATRIX->m[0][1];
    m.m[0][2] = CAMERA_MATRIX->m[0][2];
    m.m[1][0] = -CAMERA_MATRIX->m[1][0];
    m.m[1][1] = CAMERA_MATRIX->m[1][1];
    m.m[1][2] = -CAMERA_MATRIX->m[1][2];
    m.m[2][0] = CAMERA_MATRIX->m[2][0];
    m.m[2][1] = -CAMERA_MATRIX->m[2][1];
    m.m[2][2] = CAMERA_MATRIX->m[2][2];
    prev = 0;
    added = 0;
    height *= 16;
    up.vx = m.m[0][1];
    up.vy = m.m[1][1];
    up.vz = m.m[2][1];
    p = &D_80119040[i];
    pos.vx = p->pos.vx + ((up.vx * height) >> 12);
    pos.vy = p->pos.vy + ((up.vy * height) >> 12);
    pos.vz = p->pos.vz + ((up.vz * height) >> 12);
    EGSDisplayTransformWorldToScreen(&pos, &screen);
    scale = 0;
    if (screen.vz > 0 && (u32)(screen.vx + 0xFF) < 0x1FF) {
        scale = screen.vy;
        EGSDisplayTransformWorldToScreen(&p->pos, &screen);
        if (screen.vy >= scale) {
            scale = 0;
        } else {
            top = scale - screen.vy;
            scale = top << 6;
            if ((s32)g->fade >= 0 && g->unk1C == 3) {
                scale = top << 7;
            }
            if (scale > 0x7FFF) {
                scale = 0x7FFF;
            }
        }
    }
    if (g->unk25 == 0) {
        if (scale != 0) {
            added = 1;
            g->unk25 = 1;
        }
    } else if (scale == 0) {
        CameraGetDisplay(gCamera, display2);
        for (i = g->head; i >= 0; i = e->next) {
            e = &D_80119040[i];
            EGSDisplayDeleteSprite(display2[0], e->prim);
        }
        g->unk25 = 0;
        return;
    }
    while (i >= 0) {
        q = &D_80119040[i];
        sprite = SPRITE(q);
        if (i != g->head) {
            EGSDisplayTransformWorldToScreen(&q->pos, &screen);
        }
        if (screen.vz <= 0) {
            if (g->id < 0) {
                ParticleFree(g, q, prev, i);
                goto next;
            }
        } else {
            if (g->unk1C == 1) {
                D_80110050++;
                D_8011004C += (rcos(gSystemTick) + 0x2000) >> 6;
                SPRITE(q)->w = scale + ((scale * rcos(D_8011004C * ((D_80110050 + 1) & 1))) >> 14);
                SPRITE(q)->h = scale + ((scale * rcos(D_8011004C * ((D_80110050 + 1) & 3))) >> 15);
                SPRITE(q)->x = screen.vx - SCREEN_OX;
                SPRITE(q)->y = ScreenY(screen.vy);
            } else {
                w = scale;
                if ((s32)g->fade < 0 && q->unk62 == 3 && w < 0x4000) {
                    w *= 2;
                }
                sprite->x = screen.vx - SCREEN_OX;
                sprite->y = ScreenY(screen.vy);
                sprite->w = w;
                sprite->h = w;
            }
            depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
            if (depth >= 0) {
                sprite->z = depth;
            } else {
                sprite->z = 0;
            }
            do {
                sprite->tag &= 0x7FFFFFFF;
            } while (0);
            if (infrared) {
                sprite->r = 0;
                sprite->g = (u32)(((u8 *)&q->color)[2] + ((u8 *)&q->color)[1] + ((u8 *)&q->color)[0]) >> 2;
                sprite->b = 0;
            } else {
                sprite->r = q->color;
                sprite->g = (u32)q->color >> 8;
                sprite->b = ((u16 *)&q->color)[1];
            }
        }
        prev = i;
    next:
        i = q->next;
        D_80110080++;
    }
    if (added) {
        for (i = g->head; i >= 0; i = f->next) {
            f = &D_80119040[i];
            EGSDisplayAddSprite(display[0], f->prim);
        }
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012050);

/* Falling glass: shatters into shards when it reaches the floor */
void ParticleUpdateGlass(ParticleGroup *g) {
    VECTOR32 mask = { 0x1F, 0x1F, 0x1F };
    VECTOR32 pos;
    VECTOR32 screen;
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    s32 j;
    s32 angle;
    s32 depth;
    s32 bias;
    Particle *p;
    Particle *q;
    ParticleGroup *shards;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (p->pos.vy < g->unk1C) {
            if (!(i & 1) && D_8011007C + 12 < PARTICLE_MAX - 40) {
                p->vel.vy = ~p->vel.vy >> 3;
                pos.vx = p->pos.vx;
                pos.vy = g->unk1C;
                pos.vz = p->pos.vz;
                ParticleInitEmitterMask(&pos, &mask);
                ParticleInitAngle(0x555, 3, 0);
                ParticleInitLifeColor(8, 0xEF9F9F);
                D_801100B4 = 0x322;
                ParticleInitDirection(&p->vel, 0x7FF, 0);
                shards = ParticlePoolAllocate(6, NULL);
                if (shards != NULL) {
                    shards->unk23 = 4;
                    shards->wind = -0x2916;
                    shards->unk1C = 0;
                    shards->updateType = PARTICLE_UPDATE_NONE;
                    shards->drawType = 0;
                    ParticleStart(shards, 6);
                    j = shards->head;
                    shards->wind = 0;
                    while (j >= 0) {
                        angle = rand();
                        q = &D_80119040[j];
                        q->vel.vx = rcos(angle) >> 8;
                        q->vel.vy = 0;
                        q->vel.vz = rsin(angle) >> 8;
                        EGSDisplayTransformWorldToScreen(&q->pos, &screen);
                        depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
                        if (depth >= 0) {
                            q->depth = depth;
                        } else {
                            q->depth = 0;
                        }
                        j = q->next;
                    }
                }
            }
            ParticleFree(g, p, prev, i);
        } else {
            prev = i;
            p->pos.vy += p->vel.vy >= 0 ? p->vel.vy >> 12 : -(-p->vel.vy >> 12);
            p->unk62 += p->unk64;
            p->unk5E -= 3;
            p->pos.vx += p->vel.vx;
            p->pos.vz += p->vel.vz;
            p->vel.vy += g->wind;
        }
        i = next;
    }
}

/* View of GameObjectAnim's bounding box */
typedef struct SnowAnimView {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ struct {
        u8 unk00[4];
        s16 x, y, z;
    } *box;
} SnowAnimView;

/* Particle display primitive (Particle + 0x28) drawn as a gouraud line */
typedef struct ParticleLine {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ ParticleRGBC rgbc0;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ u32 rgb1;
    /* 0x18 */ s32 xy1;
} ParticleLine;

/* Rain: falling gouraud lines in a box around the camera, with a splash at the floor */
void ParticleDisplayRain(ParticleGroup *g) {
    VECTOR32 cam;
    VECTOR32 old;
    VECTOR32 dir;
    VECTOR32 screen;
    VECTOR32 player;
    GameObjectRuntime *gobj = gameObjectsInfo[gPlayerOffset].runtime;
    s32 i = g->head;
    SnowAnimView *anim;
    s32 size;
    s32 fall;
    s32 top;
    s32 windX;
    s32 windZ;
    s32 floor;
    s32 c;
    u32 color;
    u32 head;
    u32 tail;
    s32 odd;
    s32 width;
    s32 x;
    s32 d;
    s32 depth;
    s32 height;
    Particle *p;
    ParticleLine *line;

    ParticleGroupUpdateRain(g);
    anim = (SnowAnimView *)gobj->anim;
    size = (anim->box->x + anim->box->y + anim->box->z) >> 1;
    windX = D_8011D1C0.vx >> 3;
    fall = g->wind >> 8;
    windZ = D_8011D1C0.vz >> 3;
    if (size > 0x1000) {
        size = 0x1000;
    }
    c = (size >> 6) + 0x10;
    color = (c << 16) + (c << 8) + c;
    if (color & 0xFF000000) {
        head = (color & 0xFFFFFF) | 0x50000000;
    } else {
        head = color | 0x50000000;
    }
    c >>= 1;
    tail = (c << 16) + (c << 8) + c;
    if (gSniperMode && gCurrentWeapon == 12) {
        head &= 0xFF00FF00;
        tail &= 0xFF00;
    }
    player.vx = ((AnimFramesView *)gobj->anim)->frames[13]->pos.vx;
    player.vy = ((AnimFramesView *)gobj->anim)->frames[13]->pos.vy;
    player.vz = ((AnimFramesView *)gobj->anim)->frames[13]->pos.vz;
    top = player.vy = -player.vy;
    floor = ParticleFindFloor(D_80110088);
    cam.vx = (*D_8011008C)->pos.vx;
    cam.vy = (*D_8011008C)->pos.vy;
    cam.vz = (*D_8011008C)->pos.vz;
    cam.vy = -cam.vy;
    dir.vx = (*D_8011008C)->m[0][2];
    dir.vy = (*D_8011008C)->m[1][2];
    dir.vz = (*D_8011008C)->m[2][2];
    dir.vy = -dir.vy;
    cam.vx += dir.vx >> 3;
    cam.vz += dir.vz >> 3;
    while (i >= 0) {
        p = &D_80119040[i];
        line = (ParticleLine *)p->prim;
        odd = i & 1;
        if (p->state <= odd) {
            p->pos.vx = cam.vx + (rand() & 0x3FF) - 0x1FF;
            p->pos.vy = top - (height = (i & 0x1F) - 0x104);
            p->pos.vz = cam.vz + (rand() & 0x3FF) - 0x1FF;
            p->state = 3;
            line->rgbc0.word = head;
            line->rgb1 = tail;
        }
        if (p->pos.vx < cam.vx - 0x1FF) {
            p->pos.vx += 0x3FF;
        }
        if (cam.vx + 0x1FF < p->pos.vx) {
            p->pos.vx -= 0x3FF;
        }
        if (p->pos.vz < cam.vz - 0x1FF) {
            p->pos.vz += 0x3FF;
        }
        if (cam.vz + 0x1FF < p->pos.vz) {
            p->pos.vz -= 0x3FF;
        }
        D_80110080++;
        old = p->pos;
        p->pos.vy += fall;
        if (p->pos.vy <= floor) {
            width = (4 - p->state) << 1;
            if (p->state == 3) {
                p->pos.vx = old.vx + windX * (floor - old.vy) / fall;
                p->pos.vz = old.vz + windZ * (floor - old.vy) / fall;
                if (!odd) {
                    line->rgb1 = head & 0xFFFFFF;
                    if (tail & 0xFF000000) {
                        line->rgbc0.word = (tail & 0xFFFFFF) | 0x50000000;
                    } else {
                        line->rgbc0.word = tail | 0x50000000;
                    }
                }
            }
            p->pos.vy = floor;
            EGSDisplayTransformWorldToScreen(&p->pos, &screen);
            if (screen.vz > 0) {
                width >>= (cam.vy - floor) >> 8;
                line->xy0 = -(screen.vy << 16) + (u16)(screen.vx - width);
                line->xy1 = -(screen.vy << 16) + (u16)(screen.vx + width);
                {
                    s32 bias; /* separate temp keeps (unk26 - 16) from reassociating */

                    depth = (screen.vz >> 2) - (screen.vz >> 4) + (bias = g->unk26 - 16);
                }
                if (depth >= 0) {
                    line->z = depth;
                } else {
                    line->z = 0;
                }
            } else {
                line->xy0 = line->xy1 = 0x4000400;
            }
            p->state--;
        } else {
            p->pos.vx += windX;
            p->pos.vz += windZ;
            EGSDisplayTransformWorldToScreen(&old, &screen);
            if (screen.vz > 0) {
                x = screen.vx;
                p->xy1 = -(screen.vy << 16) + (u16)x;
                EGSDisplayTransformWorldToScreen(&p->pos, &screen);
                if (screen.vz > 0 && (d = screen.vx - x, (d >= 0 ? d : -d) < 0x40)) {
                    p->xy0 = -(screen.vy << 16) + (u16)screen.vx;
                    {
                        s32 bias;

                        depth = (screen.vz >> 2) - (screen.vz >> 4) + (bias = g->unk26 - 16);
                    }
                    if (depth >= 0) {
                        p->depth = depth;
                    } else {
                        p->depth = 0;
                    }
                } else {
                    line->xy0 = line->xy1 = 0x4000400;
                }
            } else {
                p->xy0 = p->xy1 = 0x4000400;
            }
        }
        line->rgbc0.c.code |= 2;
        i = p->next;
    }
}


static s32 D_8010F944 = 0x50; /* snow brightness, follows the player's size */
static s32 D_8010F948 = 0;    /* .sdata, unreferenced (taken as this file's) */


/* Snow flakes: a box of flat triangles that follows the camera */
void ParticleUpdateSnow(ParticleGroup *g) {
    VECTOR32 center;
    VECTOR32 dir;
    VECTOR32 screen;
    s32 i = g->head;
    s32 size;
    s32 delta;
    s32 fall;
    s32 floor;
    s32 depth;
    s32 bias;
    s32 height;
    s32 c;
    s32 angle;
    s32 x;
    s32 y;
    u32 color;
    Particle *p;
    ParticlePolyF3 *poly;
    SnowAnimView *anim;

    ParticleGroupUpdateRain(g);
    anim = (SnowAnimView *)gameObjectsInfo[gPlayerOffset].runtime->anim;
    size = (anim->box->x + anim->box->y + anim->box->z) >> 1;
    fall = (g->wind >> 12) + (g->wind >> 13);
    if (size > 0x1000) {
        size = 0x1000;
    }
    delta = (size >> 7) + 0x20;
    delta -= D_8010F944;
    if (delta > 8) {
        delta = 8;
    } else if (delta < -8) {
        delta = -8;
    }
    D_8010F944 += delta;
    floor = 0;
    if (D_80110088 >= 0) {
        floor = levelFloorY[gSysInfo.level][0];
    }
    center.vx = (*D_8011008C)->pos.vx;
    center.vy = (*D_8011008C)->pos.vy;
    center.vz = (*D_8011008C)->pos.vz;
    center.vy = -center.vy;
    dir.vx = (*D_8011008C)->m[0][2];
    dir.vy = (*D_8011008C)->m[1][2];
    dir.vz = (*D_8011008C)->m[2][2];
    dir.vy = -dir.vy;
    center.vx += (dir.vx >> 2) - (dir.vx >> 4);
    center.vz += (dir.vz >> 2) - (dir.vz >> 4);
    while (i >= 0) {
        p = &D_80119040[i];
        if (p->pos.vy < floor) {
            p->pos.vx = center.vx + (rand() & 0x3FF) - 0x1FF;
            p->pos.vy = floor + (height = i * 4 + 500);
            p->pos.vz = center.vz + (rand() & 0x3FF) - 0x1FF;
        }
        if (p->pos.vx < center.vx - 0x1FF) {
            p->pos.vx += 0x3FF;
        }
        if (center.vx + 0x1FF < p->pos.vx) {
            p->pos.vx -= 0x3FF;
        }
        if (p->pos.vz < center.vz - 0x1FF) {
            p->pos.vz += 0x3FF;
        }
        if (center.vz + 0x1FF < p->pos.vz) {
            p->pos.vz -= 0x3FF;
        }
        p->pos.vx += (D_8011D1C0.vx - i + 40) >> 4;
        p->pos.vy += fall - ((i - 40) >> 4);
        p->pos.vz += (D_8011D1C0.vz - i + 40) >> 4;
        angle = (s16)(p->unk62 += p->unk64);
        EGSDisplayTransformWorldToScreen(&p->pos, &screen);
        poly = (ParticlePolyF3 *)p->prim;
        depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
        if (depth >= 0) {
            poly->z = depth;
        } else {
            poly->z = 0;
        }
        c = (D_8010F944 * ((0x7FB - screen.vz) >> 3)) >> 6;
        color = (c << 16) + (c << 8) + c;
        x = screen.vx + (rcos(angle) >> 11);
        y = (screen.vy + (rsin(angle) >> 11)) << 16;
        poly->xy0 = (u16)x - y;
        x = screen.vx + (rcos(angle + p->size) >> 11);
        y = (screen.vy + (rsin(angle + p->size) >> 11)) << 16;
        poly->xy1 = (u16)x - y;
        x = screen.vx + (rcos(angle + p->unk5C) >> 11);
        y = (screen.vy + (rsin(angle + p->unk5C) >> 11)) << 16;
        poly->xy2 = (u16)x - y;
        if (color & 0xFF000000) {
            poly->rgbc0.word = (color & 0xFFFFFF) | 0x20000000;
        } else {
            poly->rgbc0.word = color | 0x20000000;
        }
        poly->rgbc0.c.code |= 2;
        i = p->next;
        D_80110080++;
    }
}

/* Triangle vertex in the particle's local frame: polar position plus an offset */
typedef struct ParticleTriVertex {
    /* 0x00 */ s16 angle;
    /* 0x02 */ s16 radius;
    /* 0x04 */ s16 offset;
} ParticleTriVertex;

/* View of a Particle as a world-space triangle (fields from 0x50 reused) */
typedef struct ParticleTriView {
    /* 0x00 */ u8 unk00[0x50];
    /* 0x50 */ ParticleTriVertex verts[3];
} ParticleTriView;

/* Particle display primitive (Particle + 0x28) drawn as a textured triangle */
typedef struct ParticlePolyFT3 {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ ParticleRGBC rgbc0;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ u32 uv0;
    /* 0x18 */ s32 xy1;
    /* 0x1C */ u32 uv1;
    /* 0x20 */ s32 xy2;
} ParticlePolyFT3;

#define TRI(p) ((ParticleTriView *)(p))

/* World-space triangles (debris) spinning in the XY (type 3) or ZY (type 6) plane */
void ParticleDisplayPolyWorld(ParticleGroup *g) {
    VECTOR32 screen;
    VECTOR32 v;
    VECTOR32 w;
    Particle *p;
    s32 angle;
    s32 i;
    s32 prev;
    u32 color;
    s32 xy2;
    s32 xy0;
    s32 xy1;
    s32 depth;
    s32 bias;
    ParticlePolyF3 *poly;
    ParticlePolyFT3 *polyT;

    prev = 0;
    i = g->head;
    xy2 = 0x7FFFFFFF;

    while (i >= 0) {
        p = &D_80119040[i];
        angle = p->unk62;
        if (p->state > 0) {
            if (p->state < g->unk22) {
                p->color -= g->fade;
            }
            /* do-while(0) keeps its loop notes: a sched barrier that keeps the color load ahead
             * of the unk25 test (else the birthing boost sinks it and reorg steals li 3) */
            do {
                color = p->color;
            } while (0);
            if (g->unk25 == 3) {
                v.vx = p->pos.vx + ((TRI(p)->verts[0].radius * rcos(angle + TRI(p)->verts[0].angle)) >> 12);
                v.vy = p->pos.vy + ((TRI(p)->verts[0].radius * rsin(angle + TRI(p)->verts[0].angle)) >> 12);
                v.vz = p->pos.vz + TRI(p)->verts[0].offset;
                EGSDisplayTransformWorldToScreen(&v, &screen);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                    goto done;
                }
                xy0 = (u16)screen.vx - (screen.vy << 16);
                v.vx = p->pos.vx + ((TRI(p)->verts[1].radius * rcos(angle + TRI(p)->verts[1].angle)) >> 12);
                v.vy = p->pos.vy + ((TRI(p)->verts[1].radius * rsin(angle + TRI(p)->verts[1].angle)) >> 12);
                v.vz = p->pos.vz + TRI(p)->verts[1].offset;
                EGSDisplayTransformWorldToScreen(&v, &screen);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                    goto done;
                }
                xy1 = (u16)screen.vx - (screen.vy << 16);
                v.vx = p->pos.vx + ((TRI(p)->verts[2].radius * rcos(angle + TRI(p)->verts[2].angle)) >> 12);
                v.vy = p->pos.vy + ((TRI(p)->verts[2].radius * rsin(angle + TRI(p)->verts[2].angle)) >> 12);
                v.vz = p->pos.vz + TRI(p)->verts[2].offset;
                EGSDisplayTransformWorldToScreen(&v, &screen);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                    goto done;
                }
                prev = i;
                xy2 = (u16)screen.vx - (screen.vy << 16);
            } else if (g->unk25 == 6) {
                w.vz = p->pos.vz + ((TRI(p)->verts[0].radius * rcos(angle + TRI(p)->verts[0].angle)) >> 12);
                w.vy = p->pos.vy + ((TRI(p)->verts[0].radius * rsin(angle + TRI(p)->verts[0].angle)) >> 12);
                w.vx = p->pos.vx + TRI(p)->verts[0].offset;
                EGSDisplayTransformWorldToScreen(&w, &screen);
                xy0 = (u16)screen.vx - (screen.vy << 16);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                    goto done;
                }
                w.vz = p->pos.vz + ((TRI(p)->verts[1].radius * rcos(angle + TRI(p)->verts[1].angle)) >> 12);
                w.vy = p->pos.vy + ((TRI(p)->verts[1].radius * rsin(angle + TRI(p)->verts[1].angle)) >> 12);
                w.vx = p->pos.vx + TRI(p)->verts[1].offset;
                EGSDisplayTransformWorldToScreen(&w, &screen);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                    goto done;
                }
                xy1 = (u16)screen.vx - (screen.vy << 16);
                w.vz = p->pos.vz + ((TRI(p)->verts[2].radius * rcos(angle + TRI(p)->verts[2].angle)) >> 12);
                w.vy = p->pos.vy + ((TRI(p)->verts[2].radius * rsin(angle + TRI(p)->verts[2].angle)) >> 12);
                w.vx = p->pos.vx + TRI(p)->verts[2].offset;
                EGSDisplayTransformWorldToScreen(&w, &screen);
                if (screen.vz <= 0) {
                    ParticleFree(g, p, prev, i);
                } else {
                    prev = i;
                    xy2 = (u16)screen.vx - (screen.vy << 16);
                }
            }
        done:
            if (xy2 != 0x7FFFFFFF) {
                if (g->flags & 0x8000) {
                    polyT = (ParticlePolyFT3 *)p->prim;
                    if (color & 0xFF000000) {
                        polyT->rgbc0.word = (color & 0xFFFFFF) | 0x24000000;
                    } else {
                        polyT->rgbc0.word = color | 0x24000000;
                    }
                    if (g->flags & 0x200) {
                        polyT->rgbc0.c.code |= 2;
                    }
                    polyT->xy0 = xy0;
                    polyT->xy1 = xy1;
                    polyT->xy2 = xy2;
                    depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
                    if (depth >= 0) {
                        polyT->z = depth;
                    } else {
                        polyT->z = 0;
                    }
                } else {
                    poly = (ParticlePolyF3 *)p->prim;
                    if (color & 0xFF000000) {
                        poly->rgbc0.word = (color & 0xFFFFFF) | 0x20000000;
                    } else {
                        poly->rgbc0.word = color | 0x20000000;
                    }
                    if (g->flags & 0x200) {
                        poly->rgbc0.c.code |= 2;
                    }
                    poly->xy0 = xy0;
                    poly->xy1 = xy1;
                    poly->xy2 = xy2;
                    depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
                    if (depth >= 0) {
                        poly->z = depth;
                    } else {
                        poly->z = 0;
                    }
                }
            }
        }
        i = p->next;
        D_80110080++;
    }
}

/* View of MoveInfo's velocity */
typedef struct MoveVelView {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ VECTOR32 vel; /* << 12 */
} MoveVelView;

/* Shared particle step; returns 0 when the particle died */
s32 ParticleUpdateMovement(ParticleGroup *g, Particle *p) {
    u32 flags = g->flags;
    s32 alive = 1;
    MoveVelView *move;
    s16 life;
    s32 half;
    s32 quarter;
    s32 r0;
    s32 r1;
    s32 vy;

    if (!(flags & 0x80)) {
        p->state--;
    }
    if (p->state == 0) {
        goto dead;
    }
    if (p->state < g->unk22) {
        p->color -= g->fade;
    }
    if (g->flags & 0x20000) {
        move = (MoveVelView *)gameObjectsInfo[g->gobjId].runtime->move;
        p->pos.vx += move->vel.vx >> 12;
        p->pos.vy += move->vel.vy >> 12;
        p->pos.vz += move->vel.vz >> 12;
    }
    p->pos.vy += p->vel.vy >= 0 ? p->vel.vy >> 12 : -(-p->vel.vy >> 12);
    p->pos.vx += p->vel.vx;
    p->pos.vz += p->vel.vz;
    if (flags & 0x4000) {
        p->pos.vx += D_8011D1C0.vx >> 5;
        p->pos.vz += D_8011D1C0.vz >> 5;
    }
    p->vel.vy += g->wind;
    if (flags & 0x10) {
        p->vel.vx -= (p->vel.vx + 1) >> 4;
        p->vel.vz -= (p->vel.vz + 1) >> 4;
    }
    if (!(flags & 0x20) && p->pos.vy < g->unk1C) {
        if (flags & 0x80) {
        dead:
            alive = 0;
        } else if (flags & 0x40) {
            life = p->state;
            { s32 rnd = rand() & life;
            /* empty do-while(0): its loop notes are a sched barrier that keeps the and ahead of the vel.vx load */
            do {
            } while (0);
            vy = p->vel.vx;
            half = life >> 1;
            p->vel.vx = (vy >> 1) + rnd - half; }
            p->vel.vz = (p->vel.vz >> 1) + (rand() & life) - half;
            r0 = rand();
            r1 = rand() & half;
            quarter = life >> 2;
            vy = p->vel.vy >> ((r0 & 1) + 1);
            vy = vy >= 0 ? vy : -vy;
            vy += r1;
            vy -= quarter;
            p->vel.vy = vy;
            p->pos.vy = g->unk1C;
        } else {
            p->pos.vy = g->unk1C;
            p->vel.vx = 0;
            p->vel.vy = 0;
            p->vel.vz = 0;
        }
    }
    return alive;
}


void ParticleUpdateLineMeter(ParticleGroup *g) {
    VECTOR32 screen;
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    ParticleLine *line;
    MoveVelView *move;
    s32 depth;
    s32 bias;
    u32 color;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (g->flags & 0x10000) {
            move = (MoveVelView *)gameObjectsInfo[g->gobjId].runtime->move;
            p->pos.vx += move->vel.vx >> 13;
            p->pos.vy += move->vel.vy >> 13;
            p->pos.vz += move->vel.vz >> 13;
        }
        EGSDisplayTransformWorldToScreen(&p->pos, &screen);
        line = (ParticleLine *)p->prim;
        if (screen.vz <= 0 || !ParticleUpdateMovement(g, p)) {
            goto free;
        }
        line->xy1 = (u16)screen.vx - (screen.vy << 16);
        depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
        if (depth >= 0) {
            line->z = depth;
        } else {
            line->z = 0;
        }
        EGSDisplayTransformWorldToScreen(&p->pos, &screen);
        if (screen.vz <= 0) {
        free:
            ParticleFree(g, p, prev, i);
            i = next;
        } else {
            color = p->color;
            if ((g->flags & 2) && !((gSystemTick + i) & 0xF)) {
                color = 0xFFFFFF;
            }
            line->xy0 = (u16)screen.vx - (screen.vy << 16);
            if (color & 0xFF000000) {
                line->rgbc0.word = (color & 0xFFFFFF) | 0x50000000;
            } else {
                line->rgbc0.word = color | 0x50000000;
            }
            if (g->flags & 0x400) {
                line->rgb1 = (u32)(p->color & 0xFEFEFE) >> 1;
            } else {
                line->rgb1 = p->color;
            }
            prev = i;
            if (g->flags & 0x200) {
                line->rgbc0.c.code |= 2;
            }
            i = next;
        }
    }
}

s32 getShift(s32 n) {
    s32 shift = 0;

    while (n > 0) {
        n >>= 1;
        shift++;
    }
    return shift - 1;
}

/* Spinning flat triangles (debris), scaled by the projected size of the group's first particle */
void ParticleDisplayPolyFlat(ParticleGroup *g) {
    VECTOR32 top;
    VECTOR32 screen;
    VECTOR32 center;
    s32 i = g->head;
    s32 prev;
    s32 next;
    s32 size = g->unk23;
    s32 depth;
    s32 bias;
    s32 color;
    s32 angle;
    s32 x;
    s32 y;
    Particle *p;
    Particle *first = &D_80119040[i];
    ParticlePolyF3 *poly;

    EGSDisplayTransformWorldToScreen(&first->pos, &screen);
    top.vx = first->pos.vx;
    prev = 0;
    top.vy = first->pos.vy + size;
    size = screen.vy;
    top.vz = first->pos.vz;
    EGSDisplayTransformWorldToScreen(&top, &screen);
    size = screen.vy - size;
    if (g->unk24 < size) {
        size = g->unk24;
    }
    D_80110054 = getShift(size);
    while (i >= 0) {
        p = &D_80119040[i];
        angle = p->unk62;
        next = p->next;
        if (g->updateType == PARTICLE_UPDATE_NONE && !ParticleUpdateMovement(g, p)) {
            ParticleFree(g, p, prev, i);
            goto cont;
        }
        poly = (ParticlePolyF3 *)p->prim;
        if (p->color & 0xFF000000) {
            poly->rgbc0.word = (p->color & 0xFFFFFF) | 0x20000000;
        } else {
            /* do-while(0) keeps its loop notes, so flow.c weights refs inside it by loop
             * depth 2: 0x20000000 refs 7 -> 8 outranks next, which is spilled as in the original */
            do {
                poly->rgbc0.word = p->color | 0x20000000;
            } while (0);
        }
        if ((g->flags & 2) && !((gSystemTick + i) & 0xF)) {
            color = 0xFFFFFF;
            if (gSniperMode && gCurrentWeapon == 12) {
                color = 0xFF00;
            }
            poly->rgbc0.word = color | 0x20000000;
        }
        EGSDisplayTransformWorldToScreen(&p->pos, &center);
        if (center.vz <= 0) {
        free:
            ParticleFree(g, p, prev, i);
        } else {
            if (!(g->flags & 0x100)) {
                depth = (center.vz >> 2) - (center.vz >> 4) + (bias = g->unk26 - 16);
                if (depth >= 0) {
                    poly->z = depth;
                } else {
                    poly->z = 0;
                }
            }
            x = center.vx + (rcos(angle) >> (12 - D_80110054));
            y = center.vy + (rsin(angle) >> (12 - D_80110054));
            poly->xy0 = -(y << 16) + (u16)x;
            x = center.vx + (rcos(angle + p->size) >> (12 - D_80110054));
            y = center.vy + (rsin(angle + p->size) >> (12 - D_80110054));
            poly->xy1 = -(y << 16) + (u16)x;
            x = center.vx + (rcos(angle + p->unk5C) >> (12 - D_80110054));
            y = center.vy + (rsin(angle + p->unk5C) >> (12 - D_80110054));
            poly->xy2 = -(y << 16) + (u16)x;
            if (g->flags & 0x200) {
                poly->rgbc0.c.code |= 2;
            }
            prev = i;
        }
    cont:
        i = next;
        D_80110080++;
    }
}

void ParticleUpdateTracer(ParticleGroup *g) {
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    VECTOR32 screen;
    VECTOR32 world;
    s32 depth;
    s32 bias;
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
                    depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
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

/* Particle display primitive (Particle + 0x28) drawn as a flat line */
typedef struct ParticleLineF2 {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ ParticleRGBC rgbc0;
    /* 0x10 */ s32 xy0;
    /* 0x14 */ s32 xy1;
} ParticleLineF2;


/* Spent shell casings: a spinning line segment that bounces on the floor */
void ParticleUpdateShell(ParticleGroup *g) {
    VECTOR32 screen;
    VECTOR32 tip;
    s32 prev = 0;
    s32 i = g->head;
    s32 next;
    Particle *p;
    ParticleLineF2 *line;
    s32 depth;
    s32 bias;
    s16 energy;
    s32 half;

    while (i >= 0) {
        p = &D_80119040[i];
        next = p->next;
        if (--p->state == 0) {
            goto free;
        }
        line = (ParticleLineF2 *)p->prim;
        p->unk62 += p->unk64;
        tip.vx = p->pos.vx + (rcos(p->unk62) >> 10);
        tip.vy = p->pos.vy;
        tip.vz = p->pos.vz + (rsin(p->unk62) >> 10);
        p->unk62 += p->unk64;
        EGSDisplayTransformWorldToScreen(&p->pos, &screen);
        if (screen.vz > 0) {
            line->xy1 = (u16)screen.vx - (screen.vy << 16);
            EGSDisplayTransformWorldToScreen(&tip, &screen);
            if (screen.vz > 0) {
                line->xy0 = (u16)screen.vx - (screen.vy << 16);
                depth = (screen.vz >> 2) - (screen.vz >> 4);
                depth += (bias = g->unk26 - 16);
                if (depth >= 0) {
                    line->z = depth;
                } else {
                    line->z = 0;
                }
            }
        }
        if (screen.vz <= 0) {
        free:
            ParticleFree(g, p, prev, i);
            i = next;
        } else {
            p->pos.vx += p->vel.vx;
            p->vel.vy += g->wind;
            p->pos.vy += p->vel.vy >> 12;
            p->pos.vz += p->vel.vz;
            if (p->pos.vy < g->unk1C) {
                energy = p->unk60;
                half = energy >> 1;
                p->unk60 = half;
                if (energy < 2) {
                    p->vel.vx = 0;
                    p->vel.vy = 0;
                    p->vel.vz = 0;
                    p->unk64 = 0;
                    p->pos.vy = g->unk1C;
                } else {
                    p->pos.vy = g->unk1C * 2 - p->pos.vy;
                    if ((rand() & 7) < 2) {
                        p->vel.vx = 0;
                        p->vel.vz = 0;
                    }
                    p->vel.vx += -(p->vel.vx >> 2) + (rand() & energy) - half;
                    p->vel.vz += -(p->vel.vz >> 2) + (rand() & energy) - half;
                    p->vel.vy = (-p->vel.vy >> 2) + (rand() & (energy * 4)) - energy * 2;
                    p->unk64 = (rand() & 0xFF) - 0x7F;
                    D_80110084 = p;
                }
            }
            prev = i;
            i = next;
        }
    }
    if (g->head < 0) {
        D_80110064 = 0;
    }
}

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

#include "scratch.h"

extern void *gSniperCam;
extern s32 gPlayerOffset;

void ParticleBreath(s32 gobj);
void DamagePacketUpdate(void);
void DamageExplosion(s32 id, VECTOR32 *pos, s32 arg2);
void WeaponGetTip(GameObjectRuntime *gobj, VECTOR32 *tip);
void EGSDisplaySetupTransformWorldToScreen(EGSFrame **display);
void ParticleUpdateGrenadeLauncher(ParticleGroup *g);
void ssUpdateAndDisplay(ParticleGroup *g);
void ParticleUpdateFireLight(void);

/* View of the player's per-bone frame table (GameObjectAnim + 0x18) */
typedef struct TaserAnimView {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ EGSFrame **frames;
} TaserAnimView;

void ParticleUpdate(void) {
    VECTOR32 pos;
    VECTOR32 base;
    s32 i;
    s32 index;
    s32 id;
    s32 floor;
    ParticleGroup *g;
    void **camera;

    D_80110080 = 0;
    D_80110084 = NULL;
    if (particleBreathFlag && gObjPlayer->healthInfo->health > 0 &&
        (!gSniperMode || (gCurrentWeapon != 12 && gCurrentWeapon != 13 && gCurrentWeapon != 18)) &&
        !((gExeTick + gPlayerOffset * 4) & 0x1F)) {
        ParticleBreath(gPlayerOffset);
    }
    DamagePacketUpdate();
    ParticleUpdateFireLight();
    if (D_80110078 > 0) {
        for (i = 0; groupUpdateList[i] != 0xFF; i++) {
            g = &group[groupUpdateList[i]];
            if (g->head >= 0) {
                if (g->unk27 == 0) {
                    if (g->unk16 != 0) {
                        if (g->updateType == 9) {
                            id = g->owner;
                            pos.vx = g->x;
                            pos.vy = g->y;
                            pos.vz = g->z;
                            ParticleFree(g, &D_80119040[g->head], 0, g->head);
                            i--;
                            if (D_80118FE0[0].on) {
                                EGSDisplayPointSourceOff(&D_80118FE0[0]);
                            }
                            DamageExplosion(id, &pos, 1);
                        }
                        g->unk16 = 0;
                    }
                } else if (g->updateType == 9) {
                    ParticleUpdateGrenadeLauncher(g);
                }
            }
        }
        if (particleTaserState) {
            if (!gSniperMode) {
                WeaponGetTip(gObjPlayer, &D_8011D190);
            } else {
                D_8011D190.vx = ((TaserAnimView *)gObjPlayer->anim)->frames[11]->pos.vx;
                D_8011D190.vy = ((TaserAnimView *)gObjPlayer->anim)->frames[11]->pos.vy;
                D_8011D190.vz = ((TaserAnimView *)gObjPlayer->anim)->frames[11]->pos.vz;
                D_8011D190.vy = -D_8011D190.vy;
                base.vx = ((TaserAnimView *)gObjPlayer->anim)->frames[5]->pos.vx;
                base.vy = ((TaserAnimView *)gObjPlayer->anim)->frames[5]->pos.vy;
                base.vz = ((TaserAnimView *)gObjPlayer->anim)->frames[5]->pos.vz;
                base.vy = -base.vy;
                D_8011D190.vx += D_8011D190.vx - base.vx;
                D_8011D190.vz += D_8011D190.vz - base.vz;
                D_8011D190.vy -= 0x40;
            }
            EGSFrameSetPosition(D_80118FE0[0].frame, 0, &D_8011D1A0);
        }
        floor = ((MoveInfo *)gameObjectsInfo[gPlayerOffset].runtime->move)->unk12C;
        if (floor > -0x7D00) {
            D_80110088 = floor;
        }
        if (gCurrentWeapon == 13 && gSniperMode) {
            camera = &gSniperCam;
        } else {
            camera = gCamera;
        }
        CameraGetDisplay(camera, (void **)&D_8011008C);
        EGSDisplaySetupTransformWorldToScreen(D_8011008C);
        for (i = 0; groupUpdateList[i] != 0xFF;) {
            index = groupUpdateList[i];
            g = &group[index];
            if (g->head >= 0) {
                if (g->unk27) {
                    g->unk27--;
                } else {
                    SCRATCH_CALL1(ssUpdateAndDisplay, g);
                }
            }
            if (index == groupUpdateList[i]) {
                i++;
            }
        }
        if (D_80110084) {
            SysPlaySong(2, 0x17, 0, (s32)D_80110084);
        }
    }
    SysPostMessage(0x10, 4, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
}

/* $gp commons: let maspsx use $gp for them (kept after the last function, in .text) */
SDATA_LOCAL(group)
SDATA_LOCAL(groupUpdateList)
