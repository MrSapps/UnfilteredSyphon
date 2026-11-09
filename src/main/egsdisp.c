#include "common.h"
#include "game.h"
#include "egsframe.h"
#include "scratch.h"

/* egsdisp.c's data is defined before the first function */

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

typedef struct GsOT {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT; /* size 0x14 */

typedef struct GsCOORDINATE2 {
    /* 0x00 */ u32 flg;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ MATRIX workm;
    /* 0x44 */ void *param;
    /* 0x48 */ struct GsCOORDINATE2 *super;
    /* 0x4C */ struct GsCOORDINATE2 *sub;
} GsCOORDINATE2; /* size 0x50 */

/* Display camera (gCameraArray, 0xF4 bytes). */
typedef struct EGSCamera {
    /* 0x00 */ EGSFrame *frame;
    /* 0x04 */ u16 projection;
    /* 0x06 */ u16 flags; /* 1: displayed, 2: movie, 4: text over movies, 8: game camera */
    /* 0x08 */ u8 priority;
    /* 0x09 */ u8 index;
    /* 0x0A */ u8 otShift;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ void *region;
    /* 0x10 */ u8 bgColor[3];
    /* 0x13 */ u8 unk13;
    /* 0x14 */ s32 id;
    /* 0x18 */ GsCOORDINATE2 coord;
    /* 0x68 */ MATRIX view;
    /* 0x88 */ GsCOORDINATE2 *viewCoord;
    /* 0x8C */ EGSListNode *anims;
    /* 0x90 */ EGSListNode *sprites;
    /* 0x94 */ EGSListNode *glines;
    /* 0x98 */ EGSListNode *polys;
    /* 0x9C */ void *refAnim;
    /* 0xA0 */ u16 polysSorted;
    /* 0xA2 */ u16 unkA2;
    /* 0xA4 */ u8 unkA4[0x3C];
    /* 0xE0 */ u8 ambient[3];
    /* 0xE3 */ u8 unkE3;
    /* 0xE4 */ s16 fogNear;
    /* 0xE6 */ u8 unkE6[2];
    /* 0xE8 */ s32 fogFar;
    /* 0xEC */ u8 fogColor[3];
    /* 0xEF */ u8 unkEF;
    /* 0xF0 */ u8 fogOn;
    /* 0xF1 */ u8 unkF1[3];
} EGSCamera; /* size 0xF4 */

/* Point light source (0x2C bytes; gPointSourceArray holds per-frame copies). */
typedef struct EGSPointSource {
    /* 0x00 */ EGSListNode *node;
    /* 0x04 */ EGSFrame *frame;
    /* 0x08 */ void *anim;
    /* 0x0C */ s32 color;
    /* 0x10 */ s16 pos[3];
    /* 0x16 */ s16 radius;
    /* 0x18 */ s32 count;
    /* 0x1C */ s32 sum[3];
    /* 0x28 */ void *anim2;
} EGSPointSource; /* size 0x2C */

/* Flashlight (cone light) parameters. */
typedef struct EGSFlashlight {
    /* 0x00 */ EGSListNode *node;
    /* 0x04 */ s32 headFlag;
    /* 0x08 */ EGSFrame *frame;
    /* 0x0C */ s32 range;
    /* 0x10 */ s32 width;
    /* 0x14 */ s32 shift;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 color;
    /* 0x20 */ MATRIX ws; /* world-screen matrix seen from the light */
} EGSFlashlight;

/* Display primitive packets (tag word at 4, then GPU words). */
typedef struct EGSPrim {
    /* 0x00 */ void *node;
    /* 0x04 */ u32 tag;
    /* 0x08 */ u32 len;
    /* 0x0C */ u32 code;
    /* 0x10 */ u32 data[1];
} EGSPrim;

typedef struct TILE {
    /* 0x00 */ u8 addr[3];
    /* 0x03 */ u8 len;
    /* 0x04 */ u8 r0, g0, b0, code;
    /* 0x08 */ s16 x0, y0;
    /* 0x0C */ s16 w, h;
} TILE;

/* Terrain light accumulator filled by EGSDisplayPointColor. */
typedef struct LightAccum {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s16 pos[3];
    /* 0x16 */ s16 pad;
    /* 0x18 */ s32 count;
    /* 0x1C */ s32 sum[3];
} LightAccum;

typedef struct DISPENV {
    /* 0x00 */ s16 dispX;
    /* 0x02 */ u8 dispY; /* low byte tested: 0 = upper field */
    /* 0x03 */ u8 dispYHi;
    /* 0x04 */ s16 dispW, dispH;
    /* 0x08 */ s16 screen[4];
    /* 0x10 */ u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct GsBOXF {
    /* 0x00 */ u32 attribute;
    /* 0x04 */ s16 x, y;
    /* 0x08 */ u16 w, h;
    /* 0x0C */ u8 r, g, b;
} GsBOXF;

typedef struct DR_AREA {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 code[2];
} DR_AREA;

typedef struct DR_TPAGE {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 code[1];
} DR_TPAGE;

/* Fog scrim description (gFogInfo). */
typedef struct EGSFogInfo {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s32 abr; /* 4: no scrim */
    /* 0x14 */ u8 r, g, b;
} EGSFogInfo;

typedef struct DR_MODE {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 code[2];
} DR_MODE;

typedef struct RECT {
    s16 x, y, w, h;
} RECT;

/* Sector description of a level model. */
typedef struct EGSSectorInfo {
    /* 0x00 */ s16 unk00;
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 numParts;
    /* 0x08 */ u8 unk08[8];
    /* 0x10 */ struct HMDCacheList *cache;
    /* 0x14 */ u8 unk14[0x70];
    /* 0x84 */ s32 *rotBoxes; /* count, then the boxes */
} EGSSectorInfo;

/* Model header used by the display (bounding box, flags). */
typedef struct EGSModel {
    /* 0x00 */ s32 min[4];
    /* 0x10 */ s32 max[3];
    /* 0x1C */ s32 color;
    /* 0x20 */ EGSSectorInfo *info;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ u32 flags; /* 0x400000: hierarchical anim, 0x1000000: sector, 0x2000000: single anim */
    /* 0x2C */ void *anims;
} EGSModel;

/* Hierarchical animation (gHAnimationArray, 0xAC bytes). */
typedef struct HAnimation {
    /* 0x00 */ struct HAnimNode *node; /* NULL when free */
    /* 0x04 */ s16 id;
    /* 0x06 */ s16 speed;
    /* 0x08 */ u32 flags;
    /* 0x0C */ u8 *data;
    /* 0x10 */ u8 *cur;
    /* 0x14 */ s32 frame;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ void (*callback)();
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 cbArg;
    /* 0x28 */ s32 type;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ SVECTOR saved[15];
} HAnimation; /* size 0xAC */

#define HANIM_MAX 14

typedef struct HAnimNode {
    /* 0x00 */ HAnimation *ha;
    /* 0x04 */ struct HAnimNode *prev;
    /* 0x08 */ struct HAnimNode *next;
} HAnimNode;

/* Display object (a model instance placed in a camera). */
typedef struct EGSAnim {
    /* 0x00 */ EGSListNode *node[1]; /* per camera id */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u8 flags; /* 0x10: hierarchical, 0x80: force update */
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 flags2; /* 0x10: gun flash */
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ EGSModel *model;
    /* 0x14 */ u8 unk14[2];
    /* 0x16 */ s16 side; /* +-1: OT z bias direction */
    /* 0x18 */ void *unk18; /* billboard animation / cached model / attached anim / terrain colour */
    /* 0x1C */ void **dla;
    /* 0x20 */ HAnimNode *haList;
    /* 0x24 */ SVECTOR *rot; /* per-part rotations */
    /* 0x28 */ s32 unk28;
} EGSAnim;

typedef struct HMDCacheModel {
    /* 0x00 */ u8 unk00[0x38];
    /* 0x38 */ EGSFrame *frame;
} HMDCacheModel;

typedef struct HMDCacheEntry {
    /* 0x00 */ EGSAnim *owner;
    /* 0x04 */ HMDCacheModel *model;
    /* 0x08 */ SVECTOR *rot;
    /* 0x0C */ s32 unk0C; /* number of damage values */
    /* 0x10 */ u32 values[11]; /* sorted damage ids */
} HMDCacheEntry; /* size 0x3C */

typedef struct HMDCacheList {
    /* 0x00 */ s32 count;
    /* 0x04 */ HMDCacheEntry *entries;
} HMDCacheList;

/* Billboard texture animation (0xC bytes). */
typedef struct BillboardAnim {
    /* 0x00 */ u8 numFrames;
    /* 0x01 */ u8 delay;
    /* 0x02 */ u16 frame;
    /* 0x04 */ s32 lastTick;
    /* 0x08 */ s16 *frames; /* u, v pairs */
} BillboardAnim;

#define POLY_W(p, o) (*(s32 *)((p) + (o)))
#define POLY_H(p, o) (*(u16 *)((p) + (o)))

typedef struct GsIMAGE {
    /* 0x00 */ u32 pmode;
    /* 0x04 */ s16 px, py;
    /* 0x08 */ u16 pw, ph;
    /* 0x0C */ u32 *pixel;
    /* 0x10 */ s16 cx, cy;
    /* 0x14 */ u16 cw, ch;
    /* 0x18 */ u32 *clut;
} GsIMAGE;

typedef struct EGSTexture {
    /* 0x00 */ u32 *tim;
    /* 0x04 */ void *slot;
    /* 0x08 */ u8 animated;
    /* 0x09 */ u8 loaded;
    /* 0x0C */ GsIMAGE image;
} EGSTexture;

/* POLY_FT4 behind the EGS prim header */
typedef struct PrimFT4 {
    /* 0x00 */ void *node;
    /* 0x04 */ u32 tag;
    /* 0x08 */ u32 len;
    /* 0x0C */ u32 code;
    /* 0x10 */ u32 xy0;
    /* 0x14 */ u8 u0, v0;
    /* 0x16 */ u16 clut;
    /* 0x18 */ u32 xy1;
    /* 0x1C */ u8 u1, v1;
    /* 0x1E */ u16 tpage;
    /* 0x20 */ u32 xy2;
    /* 0x24 */ u8 u2, v2;
    /* 0x26 */ u16 pad1;
    /* 0x28 */ u32 xy3;
    /* 0x2C */ u8 u3, v3;
    /* 0x2E */ u16 pad2;
} PrimFT4;

/* POLY_GT4 behind the EGS prim header */
typedef struct PrimGT4 {
    /* 0x00 */ void *node;
    /* 0x04 */ u32 tag;
    /* 0x08 */ u32 len;
    /* 0x0C */ u32 code;
    /* 0x10 */ u32 xy0;
    /* 0x14 */ u8 u0, v0;
    /* 0x16 */ u16 clut;
    /* 0x18 */ u32 c1;
    /* 0x1C */ u32 xy1;
    /* 0x20 */ u8 u1, v1;
    /* 0x22 */ u16 tpage;
    /* 0x24 */ u32 c2;
    /* 0x28 */ u32 xy2;
    /* 0x2C */ u8 u2, v2;
    /* 0x2E */ u16 pad1;
    /* 0x30 */ u32 c3;
    /* 0x34 */ u32 xy3;
    /* 0x38 */ u8 u3, v3;
    /* 0x3A */ u16 pad2;
} PrimGT4;

typedef struct EGSSprite {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u16 tpage;
    /* 0x0E */ u8 unk0E[0x16];
    /* 0x24 */ u32 pri; /* OT depth */
    /* 0x28 */ EGSListNode *node;
} EGSSprite;

typedef struct EGSFace {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ struct EGSFace *next;
    /* 0x08 */ u8 unk08[0x24];
} EGSFace; /* size 0x2C */

/* Texture animation (0x18 bytes, EGSMemMalloc'd). */
typedef struct EGSTexAnim {
    /* 0x00 */ EGSListNode *node;
    /* 0x04 */ u8 unk04;
    /* 0x05 */ u8 unk05;
    /* 0x06 */ u16 unk06;
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ void *unk10;
    /* 0x14 */ struct { s16 xy[2]; } *unk14;
} EGSTexAnim; /* size 0x18 */

/* Interrupt-queue entry (gDisplayInterruptArray). */
typedef struct DisplayInterrupt {
    /* 0x00 */ void (*func)();
    /* 0x04 */ s32 delay; /* in vsyncs */
    /* 0x08 */ s32 arg;
} DisplayInterrupt;

#define DISPLAY_IRQ_MAX 12

/* signed halving that rounds toward zero */
#define HALF(x) ((x) < 0 ? -(-(x) >> 1) : (x) >> 1)

typedef struct HMDBlock {
    /* 0x00 */ s32 size; /* bytes */
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 numVerts;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s16 m[9]; /* column-major rotation */
    /* 0x22 */ s16 t[3];
    /* 0x28 */ u8 unk28[0x10];
    /* 0x38 */ s16 parent;
    /* 0x3A */ s16 unk3A;
    /* 0x3C */ struct EGSFrame *frame;
} HMDBlock;

typedef struct HMDHeader {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 numBlocks;
    /* 0x08 */ s32 primOffset; /* in words, from words[] */
    /* 0x0C */ u8 unk0C[8];
    /* 0x14 */ s32 coordOffset;
    /* 0x18 */ u8 unk18[0xC];
    /* 0x24 */ s32 words[1];
} HMDHeader;

/* Damage decal (gDamagePolys, 0x38 bytes). */
typedef struct EGSDamagePoly {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32 active;
    /* 0x24 */ void *owner;
    /* 0x28 */ u32 uv[4]; /* uv0+clut, uv1+tpage, uv2, uv3 */
} EGSDamagePoly; /* size 0x38 */

#define DAMAGE_MAX 60

typedef struct DamageEntry {
    /* 0x00 */ void *obj;
    /* 0x04 */ void *unk04;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ s32 count;
} DamageEntry;

extern EGSCamera gCameraArray[];
extern DisplayInterrupt gDisplayInterruptArray[DISPLAY_IRQ_MAX];
extern EGSListNode *gDisplayInterruptList;
typedef struct DisplayInitFlags {
    /* 0x00 */ u8 clearIrqs; /* clear the interrupt array on the next queue */
    /* 0x01 */ u8 initFade; /* (re)build the fade prims */
} DisplayInitFlags;


/* deferred re-inits requested by EGSDisplayReinitCameras */
typedef struct DamageInitFlags {
    /* 0x00 */ u8 initGabe;
    /* 0x01 */ u8 initDamage;
} DamageInitFlags;
extern void *gFlashlightFrame;
extern u8 gFlashlightHeadFlag;
extern u8 gRearViewFlag;
s16 gHMDVertTriplets; /* common, $gp */
SDATA_LOCAL(gHMDVertTriplets)
extern s16 masterAnimCount;
extern MATRIX gEGSIdentityMatrix;
extern MATRIX GsIDMATRIX2;
extern u8 *GsOUT_PACKET_P;
extern u8 gGabeInCamera;
typedef struct FLEntry {
    /* 0x00 */ s32 x, y, z;
    /* 0x0C */ u32 angle; /* bit 31: wide beam */
} FLEntry;
extern FLEntry gFLArray[];
s32 gFLCount; /* common, $gp */
SDATA_LOCAL(gFLCount)
extern s16 gCurrentCamera;
extern VECTOR gPCGouraudLight;
extern MATRIX gCameraWSMatrix;
extern s32 gFogFlag;
u8 gRunFlag; /* common, $gp */
SDATA_LOCAL(gRunFlag)
extern s32 lastCounterTick;
extern s32 lastCounterTickSort;
extern s32 gImagePtr;
typedef struct EGSImageLevel {
    u8 *mark;
} EGSImageLevel;
typedef struct EGSImage {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ u8 *start;
    /* 0x08 */ u8 *end;
    /* 0x0C */ u8 unk0C[4];
    /* 0x10 */ EGSImageLevel levels[6]; /* depths follow at +0x18 each */
    /* 0x28 */ s32 depths[6];
    /* 0x40 */ s32 enabled;
} EGSImage; /* size 0x44 */
extern EGSImage gImages[5];
extern u32 D_80107B1C[];
extern VECTOR D_80013740; /* up vector */
s32 gSubsectorsSorted; /* common, $gp */
SDATA_LOCAL(gSubsectorsSorted)
s32 gSubsectorPolysSorted; /* common, $gp */
SDATA_LOCAL(gSubsectorPolysSorted)
extern s32 gGameTick;
extern s16 gCurrentDrawBuffer;
extern s16 gSortBuffer;
extern u8 *gPacketAreaEnd;
extern s32 gPacketAreaFull;
typedef struct Str8 {
    char s[8];
} Str8;

/* Run on the scratchpad stack (old sp saved at 0x1F800000). */
s32 gPointSourceHMD; /* common, $gp */
SDATA_LOCAL(gPointSourceHMD)
extern struct CameraAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ s16 unk14;
} gCameraAnim;
extern GsCOORDINATE2 gGabeCoord;
extern struct GsVIEW2 { MATRIX view; GsCOORDINATE2 *super; } gGabeView;
extern MATRIX gInvWSMatrix;
extern MATRIX gGabeWSMatrix;
extern MATRIX GsWSMATRIX;
extern s32 dTicks;
extern GsOT gammaOT[2];
extern s16 gDisplayBuffer;
extern void *gInitHeap;
s16 gMaxPackets; /* common, $gp */
SDATA_LOCAL(gMaxPackets)
extern void *gSortInfo;
extern HAnimation gHAnimationArray[HANIM_MAX];
extern EGSDamagePoly gDamagePolys[DAMAGE_MAX];
extern GsOT gOtbl[];
extern s32 gPacketAreaMallocSize;
extern s32 gPacketAreaSize;
extern s32 gPacketAreaSafeSize;
u8 *gOutPacket; /* common, $gp */
SDATA_LOCAL(gOutPacket)
extern u8 *D_80110234; /* gOutPacket + 4 */
extern EGSListNode *gTextureAnimationList;
extern EGSListNode *gPointSourceList;
extern EGSListNode *gFlashlightList;
extern void (*gFadeCallback)();
s32 gGabeCache; /* common, $gp */
SDATA_LOCAL(gGabeCache)
extern DamageEntry *D_8011045C; /* gGabeCache + 4 */
extern DamageEntry gGabeCacheEntry;
extern void *gDLA_npc_gun_flash[];
extern char D_80013734[];
extern u8 D_8014BAD8[]; /* initial packet area */
extern const GsBOXF D_80013724; /* video-mode border box */
extern u16 POSITION[2]; /* fade tiles, per draw buffer */
extern u16 gCameraID;
extern s32 gTerrainFog;
extern s32 gFlashEffect;
extern s32 gFlashIntensity;
extern s16 gFrameRate;
extern void *gSectorsCollidedArray;
extern u16 gLoadC;
extern u16 gLoadG;
extern u16 gLoadP;
char gErrorString[8]; /* common, $gp */
SDATA_LOCAL(gErrorString)
extern s32 gCollideXZ;
extern s32 gFineCollideXZ;
extern s32 gPointSourceCount;
extern EGSFace *gFaceCache;
extern s32 gDamagePolyPtr;
extern s16 gFadeRate;
extern s16 gFadeRGB;
extern void *gScrimModel;
void (*gHADoneCallback)(); /* egshmd.c defines it; a tentative definition here so aspsx reaches it through $gp */
s32 gpuTime; /* common, $gp */
SDATA_LOCAL(gpuTime)
struct EGSFogInfo *gFogInfo; /* common, $gp */
SDATA_LOCAL(gFogInfo)
extern u8 gFogScrimFlag;
extern s32 gAmbientLight; /* 0x00BBGGRR */
extern s32 gFlashlightIntensity;
extern s32 gFlashlightShift;
extern s32 gEmergencyRA;
extern void *gTextureCache;
extern EGSPointSource gPointSourceArray[];

EGSListNode *EGSListAdd(EGSListNode **list, void *data);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
void *EGSMemMalloc(u32 size);
void EGSMemFree(void *p);
void DrawSync(s32 mode);
s32 VSync(s32 mode);
void DrawSyncCallback(void (*func)());
void ResetGraph(s32 mode);
void GsSwapDispBuff(void);
void GsClearOt(u16 offset, u16 point, GsOT *ot);
s32 EGSDisplayClear();
s32 EGSDisplayFlush(void);
void EGSDisplayQueueInterrupt(void (*func)(), s32 delay, s32 arg);
EGSAnim *EGSDisplayGetHMDSector(EGSAnim *anim);
EGSAnim *EGSDisplayGetPointSector(s32 *point);
HAnimation *EGSDisplayHAStart3(EGSAnim *anim, s32 animId, s32 loop, s32 type, s32 speed);
void iEGSDisplayFromLetterbox(void (*done)());
void iEGSDisplayToLetterbox(void (*done)());
void EGSFaceListInit(EGSFace *faces, s16 count);
HAnimation *EGSDisplayGetHA(EGSAnim *anim, s32 id);
s32 EGSDisplayHAStop(EGSAnim *anim, s32 id);
s32 EGSDisplayProjectionAngleToDistance(s32 angle, s32 *dist);
void EGSDeviceGraphicsGetResolution(u16 *w, u16 *h);
void ClearImage(RECT *rect, u8 r, u8 g, u8 b);
void GsSetProjection(u16 h);
void EGSDisplaySetupGTE();
void GsSetView2(void *view);
void EGSDisplayRegisterDamage();
void EGSFrameGetMatrix(EGSFrame *frame, EGSFrame *ref, MATRIX *out);
void EGSGeometry_ReverseTransformPoint(VECTOR *point, MATRIX *m, VECTOR *out);
void MoveImage(RECT *rect, s32 x, s32 y);
void EGSListInit(void);
void GsSortClear(u8 r, u8 g, u8 b, GsOT *ot);
void EGSDeviceRender(void);
s32 HMDInitOT(void);
s32 EGSDisplayCameraMalloc(EGSCamera *camera, s32 packets);
void EGSHAnimationInit(void);
void EGSDisplaySetupTransformLocalToScreen(EGSCamera *camera, MATRIX *m);
void EGSDisplayOnScreen();
void EGSListFlush(EGSListNode **list);
void EndFlash(EGSAnim *anim);
void GsSetFlatLight(s32 id, VECTOR *v);
void GsSetWorkBase(void *base);
void GsSetLightMode(s32 mode);
void GsSetFogParam(void *fog);
void EGSDisplaySetGTEAmbient(s32 r, s32 g, s32 b);
void EGSDisplayCheckInterruptQueue(u8 vsync);
void EGSDisplayRunTextureAnimations(void);
void DCallback(void);
void EGSDisplaySortFade(GsOT *ot);
void EGSDisplaySortFogScrim(GsOT *ot);
s32 iEGSDisplaySort(EGSCamera *camera, u8 sortHMDs, s32 tick);
void GsSortFastSprite(EGSSprite *sp, GsOT *ot, u16 pri);
void GsSortSprite(EGSSprite *sp, GsOT *ot, u16 pri);
void GsSortGLine(void *line, GsOT *ot, u16 pri);
void InitPointSources(void *anim);
void FinalizePointSources(void);
void SortHMDs(EGSCamera *camera, EGSListNode *anims, s16 sortBuffer);
void SortEMDs(EGSCamera *camera, EGSListNode *anims, s16 sortBuffer);
void SortGs(EGSCamera *camera, EGSListNode *anims, s32 sortBuffer);
s32 GetVideoMode(void);
s32 func_80138718();
void GsDrawOt(GsOT *ot);
void GetDispEnv(DISPENV *env);
void SetDispMask(s32 mask);
void SetDrawArea(DR_AREA *p, RECT *r);
void AddPrim(void *ot, void *p);
void GsSortBoxFill(GsBOXF *box, GsOT *ot, u16 pri);
u32 EGSDisplayFindAnimBoneVertexNearAndFacing(EGSAnim *anim, s32 bone, VECTOR *in, VECTOR *out);
u32 EGSDisplayFindHMDBoneNormalFacing(EGSSectorInfo *hmd, s32 bone, VECTOR *in, EGSAnim *anim);
int EGSFrameCreateWithNode(EGSFrame **out, EGSFrame *parent);
int EGSFrameSetMatrixParent(EGSFrame *frame, MATRIX *m);
s32 EGSFrameLinkParent(EGSFrame *frame, EGSFrame *parent);
void EGSFastInitFaceList(void);
void SetDrawMode(DR_MODE *p, s32 dfe, s32 dtd, u16 tpage, RECT *tw);
void SetDrawTPage(DR_TPAGE *p, s32 dfe, s32 dtd, u16 tpage);
void EGSMiscMemcpy(void *dst, void *src, s32 words);
void EGSDisplaySortFog(struct EGSFogInfo *info, u32 *ot);
void a_prim(u32 *ot, void *prim);
void EGSDisplayPointColor(void *info, LightAccum *acc);
int EGSFrameCreate(EGSFrame **out);
int EGSFrameGetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *out);
int EGSFrameSetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR *pos);
void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
void EGSDisplayHAnimate(EGSAnim *anim, HAnimation *ha, s32 arg2);
HAnimation *FindFreeHAnimation(void);
s32 EGSDisplayHASetCurrentFrame(EGSAnim *anim, s32 id, s32 frame);
int GetTPage(int tp, int abr, int x, int y);
void EGSFrameAddNode(EGSFrame *parent, EGSFrame *frame);
void EGSDisplayFlushCamera(EGSCamera *camera);
void EGSDisplayFadeOff(void);
void InitImages(void);
void InitGabeDamage(void);
void InitDamagePolys(void);
int EGSMath_ValueTan(s32 angle, s32 *out);
int EGSMathFIXDiv(int a, int b);
u16 *EGSDisplayGetSectorLightArray(s32 info);

extern EGSFace gFaceList[]; /* face cache */

/* .sdata (0x8010FDA0 gLRTick before it is unreferenced and stays in the splat blob) */
s32 gGameTick = 0;
u16 gCameraID = 0;
s16 gCurrentCamera = 0;
s32 gFogFlag = 0;
s32 gFogEnd = 0;
s32 gTerrainFog = 0x107FF;
s16 gDisplayBuffer = 0;
s16 gCurrentDrawBuffer = 1;
s16 gSortBuffer = 0;
EGSListNode *gFlashlightList = NULL;
void *gFlashlightFrame = NULL;
u8 gFlashlightHeadFlag = 1;
u8 gRearViewFlag = 0;
u8 gFogScrimFlag = 0;
s32 gFlashEffect = 0;
EGSListNode *gPointSourceList = NULL;
EGSListNode *gTextureAnimationList = NULL;
s32 gTRender = 0;
s16 gFrameRate = 3;
s32 dTicks = 0;
s32 emdFlag = 0;
void *gSectorsCollidedArray = NULL;
u16 gLoadC = 0;
u16 gLoadG = 0;
u16 gLoadP = 0;
s32 gPacketAreaFull = 0;
u8 *gPacketAreaEnd = NULL;
s32 gPacketAreaSize = 0;
s32 gPacketAreaSafeSize = 0;
s32 gPacketAreaMallocSize = 0;
void *gInitHeap = NULL;
s32 gColorCollisionFaces = 0;
s32 gCollideXZ = 0x130;
s32 gFineCollideXZ = 0xA5;
s32 gPointSourceCount = 0;
EGSPointSource *gCameraPointSource = &gPointSourceArray[1];
EGSListNode *gDisplayInterruptList = NULL;
EGSFace *gFaceCache = NULL;
s32 gDamagePolyPtr = 0;
s16 gFadeRate = 0;
s16 gFadeRGB = 0;
void *gScrimModel = NULL;
void (*gFadeCallback)() = NULL;
static char D_8010FE38[8] = "GPU ERR"; /* 8-byte error messages */
static DisplayInitFlags D_8010FE40 = { 1, 1 };
static s32 D_8010FE44 = 1; /* reset the Gabe view */
static s32 D_8010FE48 = 0; /* last VSync count */
static char D_8010FE4C[8] = "PK FULL";
static char D_8010FE54[8] = "FC FULL";
static u8 *D_8010FE5C = NULL; /* gOutPacket saved by EGSDisplayMovieMalloc */
static u8 *D_8010FE60 = NULL; /* D_80110234 saved */
static s32 D_8010FE64 = 0;    /* gPacketAreaSize saved */
static DamageInitFlags D_8010FE68 = { 1, 1 }; /* deferred re-inits requested by EGSDisplayReinitCameras */
void *gOTStart = NULL;

/* .bss */
#ifdef _PSYQ
extern TILE D_801225C8[2];     /* laid out by hand after EGSDisplayInit: as .lcomms */
extern DR_MODE D_801225E8[2];  /* aspsx 2.77 would 16-align the draw areas */
extern DR_AREA D_80122600[10]; /* per OT draw areas */
#else
static TILE D_801225C8[2];
/* maspsx's 2.7x layout would 16-align the draw areas; one block, named with .set */
static DR_MODE D_801225E8_bss[12];
extern DR_MODE D_801225E8[2];
extern DR_AREA D_80122600[10];
#endif

/* .data */
VECTOR gFoo = { 0, -0xFFF, 0, 0 };

#ifndef _PSYQ
__asm__(".set D_801225E8, D_801225E8_bss\n"
        ".set D_80122600, D_801225E8_bss+0x18\n");
#endif

/* hand-written: break 0, then returns through gEmergencyRA via $at */
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSEmergencyBreak);

s32 EGSDisplayInit(void) {
    s32 i;
    s32 off;

    EGSListInit();
    EGSHAnimationInit();
    GsSortClear(0, 0, 0, &gOtbl[gDisplayBuffer]);
    EGSDeviceRender();
    EGSDeviceRender();
    gCameraID = 0;
    for (off = 9 * sizeof(GsOT); off >= 0; off -= sizeof(GsOT)) {
        ((GsOT *)((u8 *)gOtbl + off))->length = 0;
    }
    for (i = 0; i < 2; i++) {
        gammaOT[i].length = 2;
        gammaOT[i].org = EGSMemMalloc(8);
        GsClearOt(0, 0, &gammaOT[i]);
    }
    gHMDVertTriplets = 0;
    return 0;
}

#ifdef _PSYQ
__asm__("\t.bss\nD_801225C8:\t.space\t32\nD_801225E8:\t.space\t24\n"
        "D_80122600:\t.space\t120\n\t.text"); /* + 40 unreferenced bytes, as the modern block */
#endif

s32 EGSDisplayAddMovie(EGSCamera *camera) {
    if (camera->flags & 2) {
        return 0x1D;
    }
    camera->flags |= 2;
    return 0;
}

s32 EGSDisplayDeleteAllSprites(void) {
    return 0;
}

s32 EGSDisplayDeleteMovie(EGSCamera *camera) {
    camera->flags &= ~2;
    return 0;
}

s32 EGSDisplayAddGLine(EGSCamera *camera, EGSPrim *line) {
    line->data[1] = (u32)EGSListAdd(&camera->glines, line);
    return 0;
}

s32 EGSDisplayDeleteGLine(EGSCamera *camera, EGSPrim *line) {
    if (line != NULL) {
        if (line->data[1] == 0) {
            return 0;
        }
        EGSListRemove(&camera->glines, (EGSListNode *)line->data[1]);
        line->data[1] = 0;
    }
    return 0;
}

s32 EGSDisplayAddSprite(EGSCamera *camera, EGSSprite *sprite) {
    if (camera->id <= 0) {
        sprite->node = EGSListAdd(&camera->sprites, sprite);
    } else {
        sprite->node = NULL;
    }
    return 0;
}

s32 EGSDisplayDeleteSprite(EGSCamera *camera, EGSSprite *sprite) {
    if (sprite != NULL) {
        if (sprite->node == NULL) {
            return 0;
        }
        EGSListRemove(&camera->sprites, sprite->node);
        sprite->node = NULL;
    }
    return 0;
}

s32 EGSDisplayAddPoly(EGSCamera *camera, EGSPrim *poly) {
    if (camera->id <= 0) {
        poly->node = EGSListAdd(&camera->polys, poly);
    } else {
        poly->node = NULL;
    }
    return 0;
}

s32 EGSDisplayDeletePoly(EGSCamera *camera, EGSPrim *poly) {
    if (poly != NULL) {
        if (poly->node == NULL) {
            return 0;
        }
        EGSListRemove(&camera->polys, poly->node);
        poly->node = NULL;
    }
    return 0;
}

void EGSDisplayBuildPoly3F(EGSPrim *p, u32 color, u32 v0, u32 v1, u32 v2) {
    p->len = 0x04000000;
    p->tag = 0;
    p->code = color | 0x20000000;
    p->data[0] = v0;
    p->data[1] = v1;
    p->data[2] = v2;
}

void EGSDisplayBuildPoly3FT(EGSPrim *p, u32 color, u32 v0, u32 uv0, u32 v1, u32 uv1, u32 v2, u32 uv2) {
    p->len = 0x07000000;
    p->tag = 0;
    p->code = color | 0x24000000;
    p->data[0] = v0;
    p->data[1] = uv0;
    p->data[2] = v1;
    p->data[3] = uv1;
    p->data[4] = v2;
    p->data[5] = uv2;
}

void EGSDisplayBuildPoly3G(EGSPrim *p, u32 c0, u32 v0, u32 v1, u32 v2, u32 c1, u32 c2) {
    p->len = 0x06000000;
    p->tag = 0;
    p->code = c0 | 0x30000000;
    p->data[0] = v0;
    p->data[2] = v1;
    p->data[4] = v2;
    p->data[1] = c1;
    p->data[3] = c2;
}

void EGSDisplayBuildPoly4F(EGSPrim *p, u32 color, u32 v0, u32 v1, u32 v2, u32 v3) {
    p->len = 0x05000000;
    p->tag = 0;
    p->code = color | 0x28000000;
    p->data[0] = v0;
    p->data[1] = v1;
    p->data[4] = 4;
    p->data[2] = v2;
    p->data[3] = v3;
}

void EGSDisplayBuildPoly4FTrans(EGSPrim *p, u32 color, u32 v0, u32 v1, u32 v2, u32 v3, u32 abr) {
    p->len = 0x05000000;
    p->tag = 0;
    p->code = color | 0x28000000;
    p->data[0] = v0;
    p->data[1] = v1;
    p->data[2] = v2;
    p->data[3] = v3;
    if (abr < 4) {
        p->data[4] = abr;
        ((u8 *)&p->code)[3] |= 2;
    } else {
        p->data[4] = 4;
    }
}

void EGSDisplayBuildPoly4FT(PrimFT4 *p, u32 xy0, u32 xy1, u32 xy2, u32 xy3, u32 color, EGSTexture *tex, s32 abr) {
    s32 mode;
    s32 shift;
    s32 tpage;
    s32 py;
    s32 w;
    s32 h;
    s32 uEnd;
    s32 vEnd;
    s32 u;
    s32 v;

    p->tag = 0;
    p->len = 0x09000000;
    p->xy0 = xy0;
    p->xy1 = xy1;
    p->xy2 = xy2;
    p->code = color | 0x2C000000;
    p->xy3 = xy3;
    mode = tex->image.pmode & 3;
    shift = 0;
    if (mode == 0) {
        shift = 2;
    } else if (mode == 1) {
        shift = 1;
    }
    tpage = (u16)GetTPage(mode, abr, tex->image.px, tex->image.py);
    if (tpage < 16) {
        u = (tex->image.px - (tpage << 6)) << shift;
    } else {
        u = (tex->image.px - ((tpage - 16) << 6)) << shift;
    }
    py = tex->image.py;
    if (py < 0x100) {
        v = py;
    } else {
        v = py - 0x100;
    }
    w = (tex->image.pw << shift) - 1;
    h = tex->image.ph - 1;
    p->u0 = u;
    p->v0 = v;
    p->clut = (tex->image.cy << 6) | ((tex->image.cx >> 4) & 0x3F);
    p->v1 = v;
    p->tpage = tpage;
    p->u2 = u;
    uEnd = u + w;
    vEnd = v + h;
    p->u1 = uEnd;
    p->v2 = vEnd;
    p->u3 = uEnd;
    p->v3 = vEnd;
}

void EGSDisplayBuildPoly4G(EGSPrim *p, u32 c0, u32 v0, u32 v1, u32 v2, u32 v3, u32 c1, u32 c2, u32 c3) {
    p->len = 0x08000000;
    p->tag = 0;
    p->code = c0 | 0x38000000;
    p->data[0] = v0;
    p->data[2] = v1;
    p->data[7] = 4;
    p->data[4] = v2;
    p->data[6] = v3;
    p->data[1] = c1;
    p->data[3] = c2;
    p->data[5] = c3;
}

void EGSDisplayBuildPoly4GTrans(EGSPrim *p, u32 c0, u32 v0, u32 v1, u32 v2, u32 v3, u32 c1, u32 c2, u32 c3, u32 abr) {
    p->len = 0x08000000;
    p->tag = 0;
    p->code = c0 | 0x38000000;
    p->data[0] = v0;
    p->data[2] = v1;
    p->data[4] = v2;
    p->data[6] = v3;
    p->data[1] = c1;
    p->data[3] = c2;
    p->data[5] = c3;
    if (abr < 4) {
        p->data[7] = abr;
        ((u8 *)&p->code)[3] |= 2;
    } else {
        p->data[7] = 4;
    }
}

void EGSDisplayBuildPoly4GT(PrimGT4 *p, u32 xy0, u32 xy1, u32 xy2, u32 xy3, u32 c0, u32 c1, u32 c2, u32 c3, EGSTexture *tex, s32 abr) {
    s32 mode;
    s32 shift;
    s32 tpage;
    s32 py;
    s32 w;
    s32 h;
    s32 uEnd;
    s32 vEnd;
    s32 u;
    s32 v;

    p->xy1 = xy1;
    p->tag = 0;
    p->len = 0x0C000000;
    p->xy0 = xy0;
    p->xy2 = xy2;
    p->code = c0 | 0x3C000000;
    p->c1 = c1;
    p->c2 = c2;
    p->c3 = c3;
    p->xy3 = xy3;
    mode = tex->image.pmode & 3;
    shift = 0;
    if (mode == 0) {
        shift = 2;
    } else if (mode == 1) {
        shift = 1;
    }
    tpage = (u16)GetTPage(mode, abr, tex->image.px, tex->image.py);
    if (tpage < 16) {
        u = (tex->image.px - (tpage << 6)) << shift;
    } else {
        u = (tex->image.px - ((tpage - 16) << 6)) << shift;
    }
    py = tex->image.py;
    if (py < 0x100) {
        v = py;
    } else {
        v = py - 0x100;
    }
    w = (tex->image.pw << shift) - 1;
    h = tex->image.ph - 1;
    p->u0 = u;
    p->v0 = v;
    p->clut = (tex->image.cy << 6) | ((tex->image.cx >> 4) & 0x3F);
    p->v1 = v;
    p->tpage = tpage;
    p->u2 = u;
    uEnd = u + w;
    vEnd = v + h;
    p->u1 = uEnd;
    p->v2 = vEnd;
    p->u3 = uEnd;
    p->v3 = vEnd;
}

void EGSDisplayBuildLineG(EGSPrim *p, u32 c0, u32 v0, u32 v1, u32 c1) {
    p->len = 0x04000000;
    p->tag = 0;
    p->code = c0 | 0x50000000;
    p->data[0] = v0;
    p->data[2] = v1;
    p->data[1] = c1;
}

void EGSDisplayBuildLineF(EGSPrim *p, u32 color, u32 v0, u32 v1) {
    p->len = 0x03000000;
    p->tag = 0;
    p->code = color | 0x40000000;
    p->data[0] = v0;
    p->data[1] = v1;
}

void EGSDisplayBuildTile1(EGSPrim *p, u32 color, u32 xy) {
    p->len = 0x02000000;
    p->tag = 0;
    p->code = color | 0x68000000;
    p->data[0] = xy;
}

s32 EGSDisplayAddAnim(EGSCamera *camera, EGSAnim *anim) {
    if (camera->id != 0) {
        return 0;
    }
    if (anim == NULL) {
        return 1;
    }
    if (anim->node[0] != NULL) {
        return 0;
    }
    if ((anim->model->flags & 0x2400000) == 0x400000) {
        anim->flags |= 0x10;
    }
    anim->node[0] = EGSListAdd(&camera->anims, anim);
    return 0;
}

s32 EGSDisplayDeleteAnim(EGSCamera *camera, EGSAnim *anim) {
    s32 id = camera->id;
    HAnimation *ha;
    u32 flags;

    if (id != 0) {
        return 0;
    }
    if (anim == NULL) {
        return 1;
    }
    flags = anim->model->flags;
    if (anim->node[0] != NULL) {
        if (flags & 0x400000) {
            goto check;
            do {
                EGSDisplayHAStop(anim, ha->id);
            check:
                ha = EGSDisplayGetHA(anim, -1);
            } while (ha != NULL);
        }
        EGSListRemove(&camera->anims, anim->node[id]);
        anim->node[id] = NULL;
    }
    return 0;
}

s32 EGSDisplayClear(void) {
    RECT rect;
    u16 w;
    u16 h;

    EGSDeviceGraphicsGetResolution(&w, &h);
    rect.y = 0;
    rect.x = 0;
    rect.w = w;
    rect.h = h * 2;
    ClearImage(&rect, 0, 0, 0);
}

s32 EGSDisplayFlushClearRegions(void) {
    return 1;
}

s32 EGSDisplayClearRegion(void) {
    return 0;
}

void EGSDisplayRegisterSectorFaceCollisionArray(void *array) {
    gSectorsCollidedArray = array;
}

s32 EGSDisplayFindSectorWithLight(EGSAnim *anim, s32 light) {
    s32 info = (s32)anim->model->info;
    u16 *lights;
    s32 i;

    if ((info >> 24) != -0x80) {
        return 0x19;
    }
    lights = EGSDisplayGetSectorLightArray(info);
    if (lights != NULL) {
        for (i = 0; i < 32; i += 2) {
            if ((lights[i] & 0x7FFF) == light) {
                return 0;
            }
            if ((s16)lights[i] == -1) {
                return 9;
            }
        }
    }
    return 9;
}

void EGSDisplayRunTextureAnimations(void) {
    EGSListNode *node;
    EGSTexAnim *ta;
    RECT rect;
    s16 *xy;

    for (node = gTextureAnimationList; node != NULL; node = node->next) {
        ta = node->data;
        rect.w = ta->unk08;
        rect.h = ta->unk09;
        if (ta->unk05 == 0 || dTicks < ta->unk0C + ta->unk05) {
            return;
        }
        ta->unk0C = dTicks;
        xy = ta->unk14[(s16)ta->unk06++].xy;
        rect.x = xy[0];
        rect.y = xy[1];
        MoveImage(&rect, ((s16 *)ta->unk10)[0], ((s16 *)ta->unk10)[1]);
        if ((s16)ta->unk06 == ta->unk04) {
            ta->unk06 = 0;
        }
    }
}

/* libgpu P_TAG-style ordering-table tag: 24-bit next pointer + 8-bit word count */
typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
} OTTag;

/* addPrim: link prim in front of the OT entry. The volatile OT access keeps reorg from
 * putting the final store in the jr delay slot (resource_conflicts_p). */
void a_prim(u32 *ot, void *prim) {
    ((OTTag *)prim)->addr = ((volatile OTTag *)ot)->addr;
    ((volatile OTTag *)ot)->addr = (u32)prim;
}

/* D_80013724 (the video-mode border box) */
const GsBOXF D_80013724 = {0, -256, -240, 512, 3, 0, 0, 0};

s32 iEGSDisplaySort(EGSCamera *camera, u8 sortHMDs, s32 tick) {
    DR_TPAGE tpage;
    GsBOXF box;
    u8 *end;
    EGSListNode *sprites = camera->sprites;
    EGSListNode *polys = camera->polys;
    EGSListNode *glines = camera->glines;
    EGSListNode *anims;
    u8 skip = 0;
    u32 maxDepth = 1 << camera->otShift;
    u8 transp = camera->flags & 0x10;
    u8 *pkt;
    u8 *spkt;
    EGSSprite *sp;
    EGSPrim *prim;
    s32 len;
    s32 code;
    s32 words;
    s32 abr;

    gSubsectorsSorted = 0;
    gSubsectorPolysSorted = 0;
    gGameTick = tick;
    camera->polysSorted = 0;
    camera->unkA2 = 0;
    gPacketAreaEnd = (&gOutPacket)[gCurrentDrawBuffer] + gPacketAreaSize;
    end = gPacketAreaEnd;
    anims = camera->anims;
    if (camera->index == 0) {
        skip = gCameraArray[3].flags % 2; /* % keeps the u16 load (& 1 narrows it to lbu) */
    }
    for (; sprites != NULL && !skip; sprites = sprites->next) {
        spkt = GsOUT_PACKET_P;
        sp = sprites->data;
        if (end < spkt + 0x40) {
            gPacketAreaFull = 1;
            break;
        }
        if (sp->pri >= maxDepth) {
            sp->pri = maxDepth - 1;
        }
        if (sp->tpage < 0x20) {
            if (camera->index == 1) {
                GsSortFastSprite(sp, &gOtbl[gSortBuffer], sp->pri);
            } else {
                spkt[3] = 1;
                ((u32 *)spkt)[1] = 0xE1000200;
                a_prim((u32 *)gOtbl[gSortBuffer].org + sp->pri, spkt);
                GsOUT_PACKET_P = spkt + 8;
                GsSortSprite(sp, &gOtbl[gSortBuffer], sp->pri);
            }
        }
    }
    if (!(camera->flags & 2) && anims != NULL) {
        if (camera->index == 0) {
            InitPointSources(camera->refAnim);
        }
        if (sortHMDs) {
            SortHMDs(camera, anims, gSortBuffer);
        }
        if (camera->index == 0) {
            FinalizePointSources();
        }
        SortEMDs(camera, anims, gSortBuffer);
        SCRATCH_CALL3(SortGs, camera, anims, (u16)gSortBuffer);
    }
    camera->polysSorted += gSubsectorPolysSorted;
    for (; glines != NULL; glines = glines->next) {
        void *line = glines->data;

        if (end < GsOUT_PACKET_P + 0x20) {
            gPacketAreaFull = 1;
            break;
        }
        GsSortGLine(line, &gOtbl[gSortBuffer], 1);
    }
    pkt = GsOUT_PACKET_P;
    while (polys != NULL && !skip) {
        prim = polys->data;
        len = ((u8 *)&prim->len)[3];
        words = len + 1;
        code = ((s8 *)&prim->code)[3] & 0xFD;
        if (end < pkt + 0x28) {
            gPacketAreaFull = 1;
            break;
        }
        if ((len == 2 && code != 0x68) || (len == 3 && code != 0x40) || (len == 6 && code != 0x30) ||
            (len == 4 && code != 0x20 && code != 0x50) || code == 0 || len <= 0 || (code & 0x80)) {
            *(Str8 *)gErrorString = *(Str8 *)D_8010FE38;
            polys = polys->next;
            continue;
        }
        EGSMiscMemcpy(pkt, &prim->len, words);
        if (transp && code == 0x3C) {
            ((u32 *)pkt)[1] &= 0xFF40FF00;
            ((u32 *)pkt)[4] &= 0xFF40FF00;
            ((u32 *)pkt)[7] &= 0xFF40FF00;
            ((u32 *)pkt)[10] &= 0xFF40FF00;
        }
        if (prim->tag >= maxDepth) {
            prim->tag = maxDepth - 1;
        }
        a_prim((u32 *)gOtbl[gSortBuffer].org + prim->tag, pkt);
        pkt += words * 4;
        if ((code == 0x28 && (abr = prim->data[4]) != 4) || (code == 0x38 && (abr = prim->data[7]) != 4)) {
            SetDrawTPage(&tpage, 0, 1, GetTPage(2, abr, 0, 0));
            EGSMiscMemcpy(pkt, &tpage, 2);
            a_prim(gOtbl[gSortBuffer].org, pkt);
            pkt += 0x20;
        }
        polys = polys->next;
    }
    GsOUT_PACKET_P = pkt;
    if (GetVideoMode() != 0) {
        box = D_80013724;
        GsSortBoxFill(&box, &gOtbl[gSortBuffer], 0);
    }
    return 0;
}

void EGSDisplayQueueInterrupt(void (*func)(), s32 delay, s32 arg) {
    EGSListNode *node = gDisplayInterruptList;
    DisplayInterrupt *irq;
    s32 i;
    s32 off;

    if (D_8010FE40.clearIrqs) {
        for (off = (s32)(sizeof(gDisplayInterruptArray) - sizeof(DisplayInterrupt)); off >= 0; off -= sizeof(DisplayInterrupt)) {
            ((DisplayInterrupt *)((u8 *)gDisplayInterruptArray + off))->func = NULL;
        }
        D_8010FE40.clearIrqs = 0;
    }
    for (; node != NULL; node = node->next) {
        irq = node->data;
        if (irq->func == func && irq->arg == arg) {
            if (delay == 0) {
                EGSListRemove(&gDisplayInterruptList, node);
                irq->func = NULL;
            } else {
                irq->delay = delay;
            }
            return;
        }
    }
    if (delay != 0) {
        for (i = 0; i < DISPLAY_IRQ_MAX; i++) {
            irq = &gDisplayInterruptArray[i];
            if (irq->func == NULL) {
                break;
            }
        }
        if (i >= DISPLAY_IRQ_MAX) {
            EGSDebugAssert(1, 0, D_80013734, 0x425);
        }
        irq->func = func;
        irq->delay = delay;
        irq->arg = arg;
        EGSListAdd(&gDisplayInterruptList, irq);
    }
}

void EGSDisplayCheckInterruptQueue(u8 vsync) {
    EGSListNode *node = gDisplayInterruptList;
    EGSListNode *next;
    DisplayInterrupt *irq;
    void (*func)();
    s32 delay;

    while (node != NULL) {
        irq = node->data;
        next = node->next;
        delay = 666;
        if (vsync || irq->arg == 0xE0C00000) {
            delay = --irq->delay;
        }
        if (delay == 0) {
            func = irq->func;
            irq->func = NULL;
            EGSListRemove(&gDisplayInterruptList, node);
            func(irq->arg);
        }
        node = next;
    }
}

void DCallback(void) {
    gpuTime = VSync(1);
    DrawSyncCallback(NULL);
}

void EGSDisplaySetupTransformLocalToScreen(EGSCamera *camera, MATRIX *m) {
    camera->coord.flg = 0;
    camera->coord.coord = camera->frame->matrix;
    GsSetProjection(camera->projection);
    GsSetView2(&camera->view);
    m->m[0][1] = -m->m[0][1];
    m->m[1][0] = -m->m[1][0];
    m->m[1][2] = -m->m[1][2];
    m->m[2][1] = -m->m[2][1];
    m->t[1] = -m->t[1];
    EGSDisplaySetupGTE(m);
    m->m[0][1] = -m->m[0][1];
    m->m[1][0] = -m->m[1][0];
    m->m[1][2] = -m->m[1][2];
    m->m[2][1] = -m->m[2][1];
    m->t[1] = -m->t[1];
}

void EGSDisplaySetupTransformWorldToScreen(EGSCamera *camera) {
    camera->coord.flg = 0;
    camera->coord.coord = camera->frame->matrix;
    GsSetProjection(camera->projection);
    GsSetView2(&camera->view);
    EGSDisplaySetupGTE(&gEGSIdentityMatrix);
}

void EGSDisplaySetupTransformFrameToScreen(EGSCamera *camera, MATRIX *src) {
    MATRIX m;

    m.m[0][0] = src->m[0][0];
    m.m[0][1] = -src->m[0][1];
    m.m[0][2] = src->m[0][2];
    m.m[1][0] = -src->m[1][0];
    m.m[1][1] = src->m[1][1];
    m.m[1][2] = -src->m[1][2];
    m.m[2][0] = src->m[2][0];
    m.m[2][1] = -src->m[2][1];
    m.m[2][2] = src->m[2][2];
    m.t[0] = src->t[0];
    m.t[1] = -src->t[1];
    m.t[2] = src->t[2];
    EGSDisplaySetupTransformLocalToScreen(camera, &m);
}

void EGSDisplaySortFade(GsOT *ot) {
    u32 *otp = ot->org;
    TILE *tile = &D_801225C8[gDisplayBuffer];
    DR_MODE *mode = &D_801225E8[gDisplayBuffer];
    s32 ambient = *(u8 *)&gAmbientLight;
    void (*callback)();
    s32 i;
    s32 c;
    TILE *t;
    DR_MODE *m;

    if (D_8010FE40.initFade) {
        D_8010FE40.initFade = 0;
        gFadeRGB = ambient;
        for (i = 0; i < 2; i++) {
            m = &D_801225E8[i];
            t = &D_801225C8[i];
            t->len = 3;
            t->code = 0x60;
            t->x0 = -0xD0;
            t->y0 = -0x88;
            t->w = 0x1A0;
            t->h = 0x110;
            t->code |= 2;
            SetDrawMode(m, 0, 1, GetTPage(1, 2, 0, 0), NULL);
        }
    }
    if (gFadeRate == 0) {
        c = ambient;
    } else {
        if ((gFadeRGB == 0xFF && gFadeRate > 0) || (gFadeRGB == ambient && gFadeRate < 0)) {
            if (gFadeCallback != NULL) {
                callback = gFadeCallback;
                gFadeCallback = NULL;
                callback();
            }
        } else {
            gFadeRGB += gFadeRate;
            if (gFadeRGB >= 0x100) {
                gFadeRGB = 0xFF;
            } else if (gFadeRGB < ambient) {
                gFadeRGB = ambient;
            }
        }
        c = gFadeRGB;
    }
    tile->r0 = c;
    tile->g0 = c;
    tile->b0 = c;
    *(u32 *)tile = (*(u32 *)tile & 0xFF000000) | (*otp & 0xFFFFFF);
    *otp = (*otp & 0xFF000000) | ((u32)tile & 0xFFFFFF);
    *(u32 *)mode = (*(u32 *)mode & 0xFF000000) | (*otp & 0xFFFFFF);
    *otp = (*otp & 0xFF000000) | ((u32)mode & 0xFFFFFF);
}

/* `gCameraArray->projection` (not `gCameraArray[0].projection`) keeps the projection load a plain
 * constant address, so cse has no sym+4 pseudo to derive &camera->view from; the position is then
 * re-read through gCameraArray[0] after GsSetView2. */
void EGSDisplaySortFogScrim(GsOT *ot) {
    MATRIX m;
    VECTOR pos;
    DR_TPAGE tpage;
    TILE tile;
    DR_MODE mode;
    u8 *pkt;
    EGSCamera *camera;

    m = gEGSIdentityMatrix;
    GsSetProjection(gCameraArray->projection);
    camera = gCameraArray;
    camera->coord.flg = 0;
    camera->coord.coord = camera->frame->matrix;
    GsSetView2(&camera->view);
    pos.vx = gCameraArray[0].frame->matrix.t[0];
    pos.vy = gCameraArray[0].frame->matrix.t[1];
    pos.vz = gCameraArray[0].frame->matrix.t[2];
    pos.vy = -pos.vy;
    m.t[0] = pos.vx;
    m.t[1] = pos.vy;
    m.t[2] = pos.vz;
    EGSDisplaySetupGTE(&m);
    EGSDisplaySortFog(gFogInfo, ot->org);
    pkt = GsOUT_PACKET_P;
    SetDrawTPage(&tpage, 0, 1, GetTPage(2, 1, 0, 0));
    EGSMiscMemcpy(pkt, &tpage, 2);
    a_prim(ot->org, pkt);
    pkt += 0x20;
    if (gFogInfo->abr != 4) {
        tile.len = 3;
        tile.code = 0x60;
        tile.x0 = -0xC0;
        tile.y0 = -0x78;
        tile.w = 0x180;
        tile.h = 0xF0;
        tile.code = 0x62;
        tile.r0 = gFogInfo->r;
        tile.g0 = gFogInfo->g;
        tile.b0 = gFogInfo->b;
        EGSMiscMemcpy(pkt, &tile, 4);
        a_prim(ot->org, pkt);
        pkt += 0x40;
        SetDrawMode(&mode, 0, 1, GetTPage(2, gFogInfo->abr, 0, 0), NULL);
        EGSMiscMemcpy(pkt, &mode, 3);
        a_prim(ot->org, pkt);
        pkt += 0x30;
    }
    GsOUT_PACKET_P = pkt;
}

INCLUDE_RODATA("asm/main/nonmatchings/egsdisp", D_80013734);

#ifdef NON_MATCHING
/* first draft: run/movie flags are kept on the stack in the original, many spill/scheduling differences.
 * r6D: the original's u8 run/movie are global-alloc spills reloaded through $t3 (first unused
 * call-used reg after block temps in t0-t2), i.e. more call-crossing pseudos competed for s-regs
 * than in this draft. */
INCLUDE_PART1("asm/main/nonmatchings/egsdisp", EGSDisplaySort)

s32 EGSDisplaySort(u8 run, s32 tick) {
    VECTOR dir;
    VECTOR up;
    VECTOR fd;
    MATRIX m;
    VECTOR c1;
    VECTOR c0;
    VECTOR c2;
    EGSAnim *ref = NULL;
    u8 movie;
    EGSListNode *node;
    EGSFlashlight *fl;
    FLEntry *e;
    EGSCamera *camera;
    GsOT *ot;
    EGSImage *img;
    EGSImageLevel *lv;
    u32 *tags;
    u8 *p;
    u8 *mark;
    s32 angle;
    s32 depth;
    s32 left;
    s32 k;
    s16 i;
    s16 load;
    u16 t;

    movie = *(u8 *)&gCameraArray[0].flags & 2;
    if (run) {
        ref = gCameraArray[0].refAnim;
    }
    gErrorString[0] = 0;
    gPacketAreaFull = 0;
    gRunFlag = run;
    lastCounterTickSort = lastCounterTick;
    if (ref != NULL) {
        up = D_80013740;
        dir.vx = ref->frame->matrix.m[0][2];
        dir.vy = ref->frame->matrix.m[1][2];
        dir.vy = 0;
        dir.vz = ref->frame->matrix.m[2][2];
        OuterProduct12(&dir, &up, &gPCGouraudLight);
        GsSetFlatLight(0, &gFoo);
        GsSetFlatLight(1, &gPCGouraudLight);
        GsSetFlatLight(2, &gFoo);
    }
    if (!movie) {
        EGSDisplayCheckInterruptQueue(run);
        movie = *(u8 *)&gCameraArray[0].flags & 2;
    }
    GsSetWorkBase((&gOutPacket)[gCurrentDrawBuffer]);
    if (!movie) {
        node = gFlashlightList;
        gFLCount = 0;
        for (; node != NULL; node = node->next) {
            fl = node->data;
            fd.vx = fl->frame->matrix.m[0][2];
            fd.vy = fl->frame->matrix.m[1][2];
            fd.vy = -fd.vy;
            fd.vz = fl->frame->matrix.m[2][2];
            if (fl->headFlag & 1) {
                fd.vx = -fd.vx;
                fd.vy = -fd.vy;
                fd.vz = -fd.vz;
            }
            angle = ratan2(fd.vz, fd.vx);
            e = &gFLArray[gFLCount++];
            e->x = fl->frame->matrix.t[0];
            e->y = fl->frame->matrix.t[1];
            e->y = -e->y;
            e->z = fl->frame->matrix.t[2];
            if (fl->color == 0xFF) {
                e->angle = (angle & 0xFFFF) | 0x80000000;
            } else {
                e->angle = angle & 0xFFFF;
            }
        }
        if (gTextureAnimationList != NULL && run) {
            EGSDisplayRunTextureAnimations();
        }
        if (D_8010FE44) {
            gGabeView.view = GsIDMATRIX2;
            D_8010FE44 = 0;
            GsInitCoordinate2(NULL, &gGabeCoord);
            gGabeView.super = &gGabeCoord;
        }
    }
    for (i = 0; i < gCameraID; i++) {
        camera = &gCameraArray[i];
        if (i != 0) {
            camera->frame->matrix = gCameraArray[0].frame->matrix;
        }
        if (camera->id > 0) {
            camera->anims = gCameraArray[0].anims;
            camera->sprites = gCameraArray[0].sprites;
            camera->polys = gCameraArray[0].polys;
        }
        gSortBuffer = gCurrentDrawBuffer + camera->index * 2;
        if (gOtbl[gSortBuffer].length == 0) {
            continue;
        }
        gCurrentCamera = i;
        if (!movie) {
            for (node = gFlashlightList; node != NULL; node = node->next) {
                fl = node->data;
                m = fl->frame->matrix;
                if (fl->headFlag & 1) {
                    c0.vx = m.m[0][0];
                    c0.vy = m.m[1][0];
                    c0.vz = m.m[2][0];
                    c2.vx = m.m[0][2];
                    c2.vy = m.m[1][2];
                    c2.vz = m.m[2][2];
                    c1.vx = m.m[0][1];
                    c1.vy = m.m[1][1];
                    c1.vz = m.m[2][1];
                    c0.vx = -c0.vx;
                    c0.vy = -c0.vy;
                    c0.vz = -c0.vz;
                    c2.vx = -c2.vx;
                    c2.vy = -c2.vy;
                    c2.vz = -c2.vz;
                    m.m[0][0] = c0.vx;
                    m.m[1][0] = c0.vy;
                    m.m[2][0] = c0.vz;
                    m.m[0][1] = c1.vx;
                    m.m[1][1] = c1.vy;
                    m.m[2][1] = c1.vz;
                    m.m[0][2] = c2.vx;
                    m.m[1][2] = c2.vy;
                    m.m[2][2] = c2.vz;
                }
                gGabeCoord.flg = 0;
                gGabeCoord.coord = m;
                GsSetView2(&gGabeView);
                fl->ws = GsWSMATRIX;
            }
        }
        if (!movie || (camera == &gCameraArray[1] && (gCameraArray[1].flags & 4))) {
            GsClearOt(0, 0, &gOtbl[gSortBuffer]);
            if (camera->id >= 0) {
                GsSetProjection(camera->projection);
                camera->coord.flg = 0;
                camera->coord.coord = camera->frame->matrix;
                GsSetView2(&camera->view);
                if (gFlashlightList != NULL || gFlashEffect > 0 || (camera->flags & 0x10)) {
                    gCameraWSMatrix = GsWSMATRIX;
                }
                gFogFlag = gTerrainFog;
                GsSetLightMode(1);
                GsSetFogParam(&gCameraArray[0].fogNear);
            }
            EGSDisplaySetGTEAmbient(camera->ambient[0] * 16, camera->ambient[1] * 16, camera->ambient[2] * 16);
            if (camera->flags & 1) {
                iEGSDisplaySort(camera, run, tick);
            }
        }
    }
    if ((u32)gGameTick < 60) {
        load = gMaxPackets;
        if (load >= 10000) {
            load = 9999;
        }
        gLoadP = load;
    } else {
        gLoadP = (GsOUT_PACKET_P - (&gOutPacket)[gCurrentDrawBuffer]) / 40;
    }
    if (!movie) {
        ot = &gammaOT[gCurrentDrawBuffer];
        left = (&gOutPacket)[gCurrentDrawBuffer] + gPacketAreaSize - GsOUT_PACKET_P;
        GsClearOt(0, 0, ot);
        if (left > 100) {
            EGSDisplaySortFade(ot);
        }
        if (gFogScrimFlag && gRunFlag && left > 5200) {
            EGSDisplaySortFogScrim(ot);
        }
    }
    t = gDisplayBuffer;
    gDisplayBuffer = gCurrentDrawBuffer;
    gCurrentDrawBuffer = t;
    if (!movie) {
        if (gOtbl[0].length != 0) {
            VSync(1);
            DrawSync(0);
            if (gCameraArray[0].flags & 0x10) {
                tags = D_80107B1C;
                for (k = 1; k < 4; k++, tags++) {
                    img = &gImages[(gImagePtr + k) % 5];
                    p = img->start;
                    if (img->enabled && p != NULL && p != img->end) {
                        lv = img->levels;
                        mark = lv->mark;
                        depth = 20;
                        for (; p < img->end; p += 0x14) {
                            if (p == mark) {
                                depth = *(s32 *)(lv + 6);
                                lv++;
                                mark = lv->mark;
                            }
                            if (depth - k <= 0) {
                                depth = k + 1;
                            }
                            ((u32 *)p)[1] = *tags;
                            AddPrim((u32 *)gOtbl[(s16)gDisplayBuffer].org + depth - k, p);
                        }
                    }
                }
            }
        }
        if (!gRunFlag) {
            VSync(2);
        } else {
            while (VSync(-1) < D_8010FE48 + (run ? gFrameRate : 2)) {
            }
        }
        D_8010FE48 = VSync(-1);
        EGSDeviceRender();
        DrawSyncCallback(DCallback);
        if (gPacketAreaFull) {
            *(Str8 *)gErrorString = *(Str8 *)D_8010FE4C;
        } else if (gFaceCache == NULL) {
            *(Str8 *)gErrorString = *(Str8 *)D_8010FE54;
        }
        dTicks++;
        return 0;
    }
    ((EGSAnim *)gCameraArray[0].refAnim)->unk28 = 0;
    DrawSync(0);
    VSync(0);
    EGSDeviceRender();
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySort);
#endif

s32 EGSDisplayFlush(void) {
    ResetGraph(1);
    EGSDisplayClear();
    GsSwapDispBuff();
    ResetGraph(1);
    EGSDisplayClear();
    GsSwapDispBuff();
}

void EGSDisplayFadeFull(void) {
    gFadeRate = 1;
    gFadeRGB = 0xFF;
    EGSDisplayFlush();
}

void EGSDisplayFadeOff(void) {
    gFadeRate = 0;
    gFadeRGB = *(u8 *)&gAmbientLight;
}

s32 EGSDisplayFade(s32 dir, u32 speed, void (*done)()) {
    if (speed == 0 || speed >= 0xFF) {
        EGSDebugAssert(1, 0, D_80013734, 0x61D);
    }
    gFadeCallback = done;
    gFadeRate = dir * speed;
    return 1;
}

s32 EGSDisplayDraw(void) {
    u16 res[2];
    DISPENV env;
    RECT clear;
    GsBOXF box;
    GsOT *gamma = &gammaOT[gDisplayBuffer];
    u8 cleared;
    EGSCamera *camera;
    GsOT *ot;
    RECT *r;
    RECT *r2;
    DR_AREA *area;
    s32 ret;
    s32 i;
    s32 pri;
    s16 idx;
    u8 lace;

    ResetGraph(1);
    GsSwapDispBuff();
    EGSDeviceGraphicsGetResolution(&res[0], &res[1]);
    SetDispMask(1);
    if (gCameraArray[0].flags & 2) {
        ret = func_80138718();
        if (ret != 0) {
            return ret;
        }
        if (gCameraArray[1].flags & 4) {
            GsDrawOt(&gOtbl[(s16)(gDisplayBuffer + 2)]);
        }
    } else {
        cleared = 0;
        GetDispEnv(&env);
        lace = env.dispY;
        for (pri = 0; pri < 5; pri++) {
            for (i = 0; i < gCameraID; i++) {
                camera = &gCameraArray[i];
                idx = gDisplayBuffer + camera->index * 2;
                ot = &gOtbl[idx];
                if (ot->length == 0 || camera->priority != pri) {
                    continue;
                }
                r = camera->region;
                if (r->w == 0 || r->h == 0) {
                    continue;
                }
                if (lace == 0 && r->y < 0xF0) {
                    r->y += 0xF0;
                } else if (r->y >= 0xF0) {
                    ((RECT *)camera->region)->y -= 0xF0;
                }
                if (!(camera->flags & 1)) {
                    continue;
                }
                area = &D_80122600[idx];
                SetDrawArea(area, camera->region);
                AddPrim((u8 *)ot->org + ((4 << ot->length) - 4), area);
                if (!cleared) {
                    clear = *(RECT *)camera->region;
                    if (clear.h != 0xF0) {
                        clear.h = 0xF0;
                        clear.y = (clear.y >= 0xF0) ? 0xF0 : 0;
                    }
                    ClearImage(&clear, camera->bgColor[0], camera->bgColor[1], camera->bgColor[2]);
                    cleared = 1;
                } else if (camera->flags & 8) {
                    box.attribute = 0;
                    box.x = ((RECT *)camera->region)->x - POSITION[0];
                    r2 = camera->region;
                    if (r2->y >= 0xF0) {
                        r2->y -= 0xF0;
                    }
                    box.y = ((RECT *)camera->region)->y - POSITION[1];
                    box.w = ((RECT *)camera->region)->w;
                    box.h = ((RECT *)camera->region)->h;
                    box.r = camera->bgColor[0];
                    box.g = camera->bgColor[1];
                    box.b = camera->bgColor[2];
                    GsSortBoxFill(&box, ot, (1 << ot->length) - 2);
                }
                GsDrawOt(ot);
            }
        }
    }
    if (!(gCameraArray[0].flags & 2) && (gFadeRGB != 0 || gFogScrimFlag)) {
        GsDrawOt(gamma);
    }
    return 0;
}

s32 EGSCameraGetFrame(EGSFrame **out) {
    *out = gCameraArray[0].frame;
    return 0;
}

s32 EGSCameraSetProjection(u16 projection) {
    gCameraArray[0].projection = projection;
    return 0;
}

s32 EGSDisplayProjectionAngleToDistance(s32 angle, s32 *dist) {
    u16 w;
    u16 h;
    s32 tan;
    s32 err;

    if (angle >= 0x800) {
        return 1;
    }
    EGSDeviceGraphicsGetResolution(&w, &h);
    err = EGSMath_ValueTan(HALF(angle), &tan);
    if (err != 0) {
        return err;
    }
    *dist = EGSMathFIXDiv(HALF((s16)w), tan);
    return 0;
}

s32 EGSCameraSetProjectionAngle(EGSCamera *camera, s32 angle) {
    s32 dist;
    s32 err;

    err = EGSDisplayProjectionAngleToDistance(angle, &dist);
    if (err != 0) {
        return err;
    }
    camera->projection = dist;
    return 0;
}

s32 EGSCameraView(void) {
    return 0;
}

s32 EGSDisplayMovieMalloc(u8 *buf, s32 size) {
    u8 *start = (u8 *)((u32)(buf + size + 7) & ~7);
    u8 *end = D_80110234 + gPacketAreaSize;
    u32 newSize = (u32)(end - start - 8) >> 1;

    if (buf == NULL) {
        if (D_8010FE64 == 0) {
            return 0;
        }
        gPacketAreaSize = D_8010FE64;
        D_8010FE64 = 0;
        gOutPacket = D_8010FE5C;
        D_80110234 = D_8010FE60;
        return 1;
    }
    if ((u32)end < (u32)start) {
        return 0;
    }
    D_8010FE5C = gOutPacket;
    gOutPacket = start;
    D_8010FE60 = D_80110234;
    D_8010FE64 = gPacketAreaSize;
    gPacketAreaSize = newSize;
    D_80110234 = (u8 *)((u32)(start + newSize + 3) & ~3);
    return newSize;
}

s32 EGSDisplayCameraMalloc(EGSCamera *camera, s32 packets) {
    s32 first = camera->index * 2;
    s32 shift = camera->otShift;
    u32 size = 0;
    s32 i;

    if (gOtbl[first].length != 0) {
        return 0;
    }
    for (i = first; i < first + 2; i++) {
        size = 4 << shift;
        gOtbl[i].length = shift;
        gOtbl[i].org = EGSMemMalloc(size);
        GsClearOt(0, 0, &gOtbl[i]);
    }
    if (packets > 0) {
        s32 bytes = packets << 6;

        D_80110234 = D_8014BAD8;
        gOutPacket = D_8014BAD8 + bytes;
        gPacketAreaSize = bytes;
        gPacketAreaSafeSize = bytes;
        gInitHeap = gOutPacket + bytes;
    }
    return size * 2;
}

s32 EGSDisplayMalloc(u32 size) {
    s32 i;
    u32 total;
    s32 half;
    s32 packets;

    gInitHeap = NULL;
    total = (gHMDVertTriplets + 4) * 0x24;
    gSortInfo = EGSMemMalloc(total);
    for (i = 0; i < gCameraID; i++) {
        total += EGSDisplayCameraMalloc(&gCameraArray[i], 0);
    }
    total += HMDInitOT();
    half = (s32)(size - total - 0x800) >> 1;
    packets = half / 40;
    gPacketAreaSafeSize = gPacketAreaSize = half & ~7;
    D_80110234 = EGSMemMalloc(half);
    gOutPacket = EGSMemMalloc(half);
    gMaxPackets = packets;
    return packets;
}

u32 *EGSDisplayPacketAreaMalloc(s32 size) {
    s32 half;
    s32 i;
    u8 *out;
    u32 *p;

    DrawSync(0);
    size = (size + 8) & ~7;
    half = size >> 1;
    if (gPacketAreaSize < half) {
        EGSDebugAssert(1, 0, D_80013734, 0x752);
    }
    if (gPacketAreaMallocSize != 0) {
        EGSDebugAssert(1, 0, D_80013734, 0x753);
    }
    gPacketAreaMallocSize = half;
    gPacketAreaSize -= half;
    gPacketAreaSafeSize -= half;
    out = gOutPacket + gPacketAreaSize;
    p = (u32 *)(out + gPacketAreaSize);
    D_80110234 = out;
    {
        u32 *q = p;

        for (i = 0; i < size >> 2; i++) {
            *q++ = 0;
        }
    }
    return p;
}

void EGSDisplayPacketAreaFree(void) {
    s32 size = gPacketAreaMallocSize;

    if (size != 0) {
        gPacketAreaMallocSize = 0;
        gPacketAreaSize += size;
        gPacketAreaSafeSize += size;
        D_80110234 = gOutPacket + gPacketAreaSize;
        DrawSync(0);
    }
}

#ifdef NON_MATCHING
/* 7 diff lines (r7K, was 8). The do {} while (0) (sched barrier) keeps sched1 from sinking the
 * birthing-boosted param copies down to the EGSFrameCreate call, so their live lengths grow and
 * pos/dist/region/priority/otShift get s2-s6 as in the original. r7K: `s32 id; u16 index = id;`
 * (a mode-changing copy survives cse; a same-mode u16/s16 copy is merged by cse2) plus an
 * s32-param inline doing the (s16) cast gives the original's plain move into s0. Left: the (s16)
 * extension lands in v1 (original a1, so v0/v1/a0 were all busy there at local-alloc; the la of
 * gCameraArray is pinned after the chain by the loop-note barrier) and parent's a0 copy is
 * scheduled later than in the original. r8Q: original sll v0/sra a1 means the sra dest was NOT tied to the
 * sll temp (local-alloc combine_regs refuses when the dest already has a qty or dies twice); reusing `id`/`err`
 * as the extension dest unties it (sll v0; sra v1) but costs 15-21 lines elsewhere. a1 also needs v1/a0 busy.
 * r9W: the a0/a1 param copies are a never-scheduled leading run in sched1 (sched.c skips reg-source SETs
 * up to the first note), so a0 can't stay live; moving gCameraID's increment after GetCam (v1 live) is 22. */
static inline EGSCamera *GetCam(s32 i) { return &gCameraArray[(s16)i]; }
s32 EGSCameraCreate(EGSFrame *parent, VECTOR *pos, u16 dist, void *region, u16 priority, u8 otShift, s32 unused, EGSCamera **out) {
    s32 id;
    u16 index;
    EGSCamera *camera;
    VECTOR p;
    s32 err;

    id = gCameraID++;
    index = id;
    do {} while (0);
    camera = GetCam(index);

    err = EGSFrameCreate(&camera->frame);
    if (err != 0) {
        return err;
    }
        camera->id = -1;
        camera->projection = dist;
        camera->priority = priority;
        camera->region = region;
        camera->index = index;
        camera->otShift = otShift;
        camera->flags = 0;
        camera->anims = NULL;
        camera->sprites = NULL;
        camera->glines = NULL;
        camera->polys = NULL;
        camera->refAnim = NULL;
        camera->bgColor[0] = camera->bgColor[1] = camera->bgColor[2] = 0;
        camera->fogOn = 0;
        camera->ambient[0] = 0x80;
        camera->ambient[1] = 0x80;
        camera->ambient[2] = 0x80;
        camera->view = GsIDMATRIX2;
        GsInitCoordinate2(NULL, &camera->coord);
        camera->viewCoord = &camera->coord;
        EGSFrameGetPosition(camera->frame, parent, &p);
        p.vx += pos->vx;
        p.vy += pos->vy;
        p.vz += pos->vz;
        EGSFrameSetPosition(camera->frame, parent, &p);
        *out = camera;
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSCameraCreate);
#endif

s32 EGSDisplayTransformWorldPointToScreen(VECTOR *point, EGSCamera *camera, VECTOR *out) {
    MATRIX m;
    s32 v;
    s16 w;
    s16 h;

    EGSDeviceGraphicsGetResolution(&w, &h);
    (void)&v; /* the original keeps v in a stack slot */
    EGSFrameGetMatrix(camera->frame, NULL, &m);
    EGSGeometry_ReverseTransformPoint(point, &m, out);
    if (w == 0 || out->vz == 0) {
        return 7;
    }
    v = EGSMathFIXDiv(out->vx, out->vz);
    out->vx = EGSMathFIXMul(v, camera->projection);
    v = EGSMathFIXDiv(out->vy, out->vz);
    v = v * (h << 2) / (w * 3);
    out->vy = EGSMathFIXMul(v, camera->projection);
    return 1;
}

EGSCamera *EGSDisplayGetDefaultCamera(void) {
    return gCameraArray;
}

void EGSAnimSetReferenceAnim(void *anim) {
    gCameraArray[0].refAnim = anim;
}

void EGSDisplayRegisterScrimModel(void *model) {
    gScrimModel = model;
}

s32 EGSDisplayFogOn(EGSCamera *camera) {
    camera->fogOn = 1;
    return 0;
}

s32 EGSDisplaySetFogColor(EGSCamera *camera, u8 *rgb) {
    camera->fogColor[0] = camera->bgColor[0] = rgb[0];
    camera->fogColor[1] = camera->bgColor[1] = rgb[1];
    camera->fogColor[2] = camera->bgColor[2] = rgb[2];
    return 0;
}

void EGSDisplaySetTerrainFog(s16 start, s32 shift) {
    gTerrainFog = (shift << 16) + start;
}

s32 EGSDisplaySetFogDepth(EGSCamera *camera, s16 fogNear, s32 fogFar) {
    camera->fogNear = fogNear;
    camera->fogFar = fogFar;
    return 0;
}

s32 EGSDisplaySetAmbient(EGSCamera *camera, u8 r, u8 g, u8 b) {
    camera->ambient[0] = r;
    camera->ambient[1] = g;
    camera->ambient[2] = b;
    return 0;
}

void EGSHAnimationInit(void) {
    s32 off;

    for (off = (HANIM_MAX - 1) * sizeof(HAnimation); off >= 0; off -= sizeof(HAnimation)) {
        ((HAnimation *)((u8 *)gHAnimationArray + off))->node = NULL;
    }
}

HAnimation *FindFreeHAnimation(void) {
    s32 i;

    for (i = 0; i < HANIM_MAX; i++) {
        if (gHAnimationArray[i].node == NULL) {
            return &gHAnimationArray[i];
        }
    }
    return NULL;
}

void EGSDisplayHAStartUnique(EGSAnim *anim, s32 animId, u8 flags) {
    if (anim->haList != NULL) {
        EGSDisplayHAStop(anim, anim->haList->ha->id);
    }
    EGSDisplayHAStart3(anim, animId, flags, 0, 30);
}

HAnimation *EGSDisplayGetHA(EGSAnim *anim, s32 id) {
    HAnimation *ha;
    HAnimNode *node;

    do {
        for (node = anim->haList; node != NULL; node = node->next) {
            ha = node->ha;
            if (id < 0) {
                return ha;
            }
            if (ha->id == id) {
                return ha;
            }
        }
    } while (0);
    return NULL;
}

HAnimation *EGSDisplayGetHAWithType(EGSAnim *anim, s32 type) {
    HAnimNode *node;
    HAnimation *ha;

    do {
        for (node = anim->haList; node != NULL; node = node->next) {
            ha = node->ha;
            if (type < 0) {
                return ha;
            }
            if (ha->type == type) {
                return ha;
            }
        }
    } while (0);
    return NULL;
}

s32 *EGSDisplayHAGetHipMotionArray(HAnimation *ha) {
    u8 *data = ha->data;
    s32 tag = ((s32 *)data)[-1];
    u8 *hip;

    if ((tag & 0xFFFF) != 0xEFEF) {
        return NULL;
    }
    hip = data - (((tag >> 8) & 0xFF00) + ((u32)tag >> 24));
    if (*hip != 0xEA) {
        return NULL;
    }
    return (s32 *)(hip + 4);
}

HAnimation *EGSDisplayHAStart3(EGSAnim *anim, s32 animId, s32 loop, s32 type, s32 speed) {
    EGSModel *model = anim->model;
    HAnimNode *node;
    HAnimNode *next;
    HAnimation *ha;
    HAnimation *cur;
    u8 *data;

    if (model->flags & 0x02000000) {
        data = model->anims;
    } else {
        data = ((u8 **)model->anims)[animId];
    }
    node = anim->haList;
    if (data[0] == 0xEA) {
        data += data[1] + (data[2] << 8);
    }
    for (; node != NULL; node = next) {
        cur = node->ha;
        next = node->next;
        if (cur->data == data) {
            return NULL;
        }
        if (cur->type == type || type == 0 || cur->type == 0) {
            EGSDisplayHAStop(anim, cur->id);
        }
    }
    ha = FindFreeHAnimation();
    if (ha == NULL) {
        EGSDebugAssert(1, 0, D_80013734, 0x887);
    }
    ha->node = (HAnimNode *)EGSListAdd((EGSListNode **)&anim->haList, ha);
    ha->flags = 0x40000000;
    if (loop) {
        ha->flags = 0x50000000;
    }
    ha->data = data;
    ha->cur = data;
    ha->frame = -1;
    ha->unk18 = -1;
    ha->id = animId;
    ha->speed = speed;
    ha->unk30 = 0;
    ha->unk20 = 0;
    ha->callback = NULL;
    ha->cbArg = 0;
    ha->type = type;
    ha->unk2C = loop;
    EGSDisplayHASetCurrentFrame(anim, animId, 1);
    return ha;
}

void EGSDisplayHAStart2(void *hmd, s32 anim, s32 flags, s32 frame) {
    EGSDisplayHAStart3(hmd, anim, flags, frame, 30);
}

void EGSDisplayHAStart(void *hmd, s32 anim, s32 flags) {
    EGSDisplayHAStart3(hmd, anim, flags, 0, 30);
}

s32 EGSDisplayHAStop(EGSAnim *anim, s32 id) {
    HAnimation *ha = EGSDisplayGetHA(anim, id);

    if (ha == NULL) {
        return 0x2D;
    }
    if (ha->unk20 < 0 && ha->callback != NULL) {
        ha->callback(anim, 0x80000000, ha->cbArg);
    }
    EGSListRemove((EGSListNode **)&anim->haList, ha->node);
    ha->id = -1;
    ha->node = NULL;
    ha->unk2C = -1;
    ha->flags &= 0xAFFFFFFF;
    return 0;
}

void EGSDisplayHMDCache(EGSAnim *anim, s32 add) {
    HMDCacheList *list = anim->model->info->cache;
    EGSAnim *obj = anim;
    HMDCacheEntry *e;
    s32 i;

    if (list == NULL) {
        EGSDebugAssert(1, 0, D_80013734, 0x8C4);
    }
    for (i = 0; i < list->count; i++) {
        e = &list->entries[i];
        if (add) {
            if (e->owner == NULL) {
                e->owner = anim;
                obj->unk18 = e->model;
                obj->rot = e->rot;
                e->unk0C = 0;
                EGSFrameAddNode(e->model->frame, anim->frame);
                return;
            }
        } else if (e->owner == anim) {
            e->owner = NULL;
            obj->unk18 = NULL;
            obj->rot = NULL;
            return;
        }
    }
    EGSDebugAssert(1, 0, D_80013734, 0x8DD);
}

void EGSDisplayRegisterHMD(HMDHeader *hmd) {
    s32 i;
    s32 verts = 0;
    s32 n = hmd->numBlocks;
    HMDBlock *blk = (HMDBlock *)(hmd->primOffset * 4 + (s32)hmd + 0x24);

    for (i = 0; i < n; i++) {
        verts += blk->numVerts;
        blk = (HMDBlock *)((u8 *)blk + blk->size);
    }
    if (gHMDVertTriplets < verts) {
        gHMDVertTriplets = verts;
    }
    if (hmd->numBlocks == 15) {
        s32 *coords = (s32 *)((u8 *)hmd + hmd->coordOffset);
        s32 *coords2;

        coords[0x1A4 / 4] = -0x33;
        coords[0x1B4 / 4] = -0xA;
        coords2 = (s32 *)((u8 *)hmd + hmd->coordOffset);
        coords2[0x1C4 / 4] = -0x14;
        coords2[0x1D4 / 4] = 0x14;
    }
}

/* gGabeCache/D_8011045C are really one HMDCacheList {count, entries}: the stores must be
 * struct (MEM_IN_STRUCT) accesses so sched keeps them ahead of the ref->unk18 load. */
void InitGabeDamage(void) {
    EGSAnim *ref;

    ((HMDCacheList *)&gGabeCache)->count = 1;
    ref = gCameraArray[0].refAnim;
    ((HMDCacheList *)&D_8011045C)->count = (s32)&gGabeCacheEntry;
    gGabeCacheEntry.count = 0;
    gGabeCacheEntry.unk04 = ref->unk18;
    D_8011045C->obj = ref;
}

s32 EGSDisplayRegisterHMDDamage(EGSAnim *anim, s32 bone, VECTOR *in, VECTOR *out, u8 byVertex) {
    EGSSectorInfo *hmd = anim->model->info;
    HMDCacheList *list = hmd->cache;
    HMDCacheEntry *e;
    MATRIX *m;
    u32 id;
    u32 found;
    s32 i;
    s32 j;
    s32 n;
    s32 k;

    if (D_8010FE68.initGabe) {
        InitGabeDamage();
        D_8010FE68.initGabe = 0;
    }
    if (anim == gCameraArray[0].refAnim) {
        list = (HMDCacheList *)&gGabeCache;
    }
    GsSetProjection(300);
    if (list == NULL) {
        return 0;
    }
    for (i = 0; i < list->count; i++) {
        e = &list->entries[i];
        if (e->model == anim->unk18) {
            m = &((EGSFrame **)e->model)[bone]->matrix;
            found = byVertex ? EGSDisplayFindAnimBoneVertexNearAndFacing(anim, bone, in, out)
                             : EGSDisplayFindHMDBoneNormalFacing(hmd, bone, in, anim);
            if (found == 0) {
                return 0;
            }
            id = found;
            j = 0;
            n = e->unk0C;
            if (!byVertex) {
                ApplyMatrixLV(m, in, out);
                out->vx += m->t[0];
                out->vy += m->t[1];
                out->vz += m->t[2];
                out->vy = -out->vy;
            }
            if (n == 10) {
                return 0;
            }
            for (; j < n; j++) {
                if (id == e->values[j]) {
                    return 0;
                }
                if (id < e->values[j]) {
                    break;
                }
            }
            for (k = n; j < k; k--) {
                e->values[k] = e->values[k - 1];
            }
            e->values[j] = id;
            e->unk0C++;
            return 1;
        }
    }
    return 0;
}

s32 EGSFrameLinkParent(EGSFrame *frame, EGSFrame *parent) {
    frame->node->parent = parent;
    frame->node->next = parent->node->child;
    parent->node->child = frame;
    return 0;
}

/* model arrives as void * and is copied into the typed local: combine folds the a1 param copy
 * into that local's set, which ends sched1's untouchable run of leading hard-reg param copies
 * (sched.c schedule_block, b == 0), so the anims/copies copies get scheduled like the original. */
void EGSAnimHMDLink(EGSAnim *anim, void *model, void *anims, s32 copies) {
    HMDHeader *hmd = model;
    EGSFrame *root = anim->frame;
    MATRIX m;
    HMDCacheList *list;
    HMDCacheEntry *entries;
    HMDCacheEntry *e;
    EGSFrame **frames;
    HMDBlock *blk;
    EGSFrame *parent;
    s32 i;
    s32 j;

    anim->model->info = (EGSSectorInfo *)hmd;
    anim->model->flags |= 0x400000;
    anim->unk18 = NULL;
    anim->rot = NULL;
    if (copies == 0 || ((EGSSectorInfo *)hmd)->cache == NULL) {
        list = EGSMemMalloc(8);
        entries = EGSMemMalloc(copies * sizeof(HMDCacheEntry));
        if (copies != 0) {
            list->count = copies;
            list->entries = entries;
            ((EGSSectorInfo *)hmd)->cache = list;
        }
        for (i = 0; copies != 0 ? i < copies : i <= 0; i++) {
            frames = EGSMemMalloc(hmd->numBlocks * 4);
            if (copies != 0) {
                e = &entries[i];
                e->owner = NULL;
                e->model = (HMDCacheModel *)frames;
                e->rot = EGSMemMalloc(hmd->numBlocks * 8);
            } else {
                anim->unk18 = frames;
                anim->rot = EGSMemMalloc(hmd->numBlocks * 8);
            }
            blk = (HMDBlock *)(hmd->primOffset * 4 + (s32)hmd + 0x24);
            for (j = 0; j < hmd->numBlocks; j++) {
                m.m[0][0] = blk->m[0];
                m.m[1][0] = blk->m[1];
                m.m[2][0] = blk->m[2];
                m.m[0][1] = blk->m[3];
                m.m[1][1] = blk->m[4];
                m.m[2][1] = blk->m[5];
                m.m[0][2] = blk->m[6];
                m.m[1][2] = blk->m[7];
                m.m[2][2] = blk->m[8];
                m.t[0] = blk->t[0];
                m.t[1] = blk->t[1];
                m.t[2] = blk->t[2];
                EGSFrameCreateWithNode(&frames[j], NULL);
                blk->frame = frames[j];
                EGSFrameSetMatrixParent(frames[j], &m);
                blk = (HMDBlock *)((s32)blk + blk->size);
            }
            blk = (HMDBlock *)(hmd->primOffset * 4 + (s32)hmd + 0x24);
            for (j = 0; j < hmd->numBlocks; j++) {
                if (blk->parent >= 0) {
                    parent = frames[blk->parent];
                } else {
                    parent = root;
                }
                EGSFrameLinkParent(frames[j], parent);
                blk = (HMDBlock *)((s32)blk + blk->size);
            }
        }
    } else {
        anim->unk18 = NULL;
        anim->rot = NULL;
    }
    anim->model->anims = anims;
    anim->haList = NULL;
}

void InitPointSources(void *anim) {
    EGSListNode *node = gPointSourceList;
    EGSPointSource *p;
    EGSPointSource *ps;
    s32 cnt;
    EGSFrame *frame;
    s16 *color;
    s32 i;

    for (i = 0; i < gPointSourceCount; i++) {
        p = &gPointSourceArray[i];
        if (p->anim2 != NULL) {
            cnt = p->count;
            color = (s16 *)((EGSAnim *)p->anim2)->dla;
            if (cnt != 0 && (i != 0 || gGabeInCamera)) {
                color[2] = p->sum[0] / cnt * 8 + 500;
                color[3] = p->sum[1] / cnt * 8 + 500;
                color[4] = p->sum[2] / cnt * 8 + 500;
            }
        }
    }
    gPointSourceArray[0].anim = anim;
    gPointSourceCount = 2;
    gPointSourceArray[0].anim2 = anim;
    gPointSourceArray[1].anim = &gCameraAnim;
    gPointSourceArray[1].anim2 = NULL;
    gCameraAnim.unk14 = 0;
    gCameraAnim.frame = gCameraArray[0].frame;
    while (node != NULL && gPointSourceCount + 1 < 10) {
        ps = node->data;
        frame = ps->frame;
        if (frame == NULL) {
            EGSDebugAssert(1, 0, D_80013734, 0xA1B);
        }
        ps->pos[0] = frame->matrix.t[0];
        ps->pos[1] = frame->matrix.t[1];
        ps->pos[2] = frame->matrix.t[2];
        ps->anim = NULL;
        ps->anim2 = NULL;
        gPointSourceArray[gPointSourceCount++] = *ps;
        node = node->next;
    }
    gPointSourceHMD = gPointSourceCount;
}

/* volatile casts reproduce the frame reloads (real cause unknown); the per-branch
 * `ps->frame = f` (cross-jumped later) gives f the refs to outrank t in global-alloc */
void FinalizePointSources(void) {
    VECTOR v;
    EGSPointSource *ps;
    EGSAnim *a2;
    EGSAnim *a;
    EGSFrame *f;
    s32 *t;
    s32 i;

    for (i = 0; i < gPointSourceCount; i++) {
        ps = &gPointSourceArray[i];
        a2 = ps->anim2;
        a = ps->anim;
        if (i != 1 && a2 == NULL) {
            continue;
        }
        if (i == 1) {
            t = ((volatile EGSAnim *)a)->frame->matrix.t;
            ((volatile EGSAnim *)a)->unk28 = 0;
            f = ((volatile EGSAnim *)a)->frame;
            ps->frame = f;
        } else {
            t = ((volatile EGSAnim *)a2)->frame->matrix.t;
            f = ((volatile EGSAnim *)a2)->frame;
            ((volatile EGSAnim *)a2)->unk28 = 0;
            ps->frame = f;
        }
        ps->color = 0;
        ps->pos[0] = t[0];
        ps->pos[1] = t[1] + ((i != 0) ? 0x80 : 0x28);
        ps->pos[2] = t[2];
        ps->count = 0;
        ps->sum[2] = 0;
        ps->sum[1] = 0;
        ps->sum[0] = 0;
        if (i != 1) {
            ps->radius = gCollideXZ;
            if (i != 0) {
                ps->radius = gCollideXZ - 0x8C;
            }
        } else {
            ps->radius = gCollideXZ + 0x40;
        }
        if (a2 != NULL && (a2->flags2 & 0x10)) {
            ps->color = a2->model->color;
            if (i != 0) {
                v.vx = (*(MATRIX **)a2->unk18)->m[0][2];
                v.vy = (*(MATRIX **)a2->unk18)->m[1][2];
                v.vz = (*(MATRIX **)a2->unk18)->m[2][2];
                v.vy = -v.vy;
                ps->pos[0] -= v.vx >> 5;
                ps->pos[2] -= v.vz >> 5;
            }
        }
    }
    gPointSourceArray[gPointSourceCount].frame = NULL;
    EGSFastInitFaceList();
}

void EGSFaceListInit(EGSFace *faces, s16 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (i < count - 1) {
            faces[i].next = &faces[i + 1];
        } else {
            faces[i].next = NULL;
        }
    }
}

void *EGSFaceListHead(void **list) {
    while (*list != NULL) {
        list = *list;
    }
    return list;
}

void EGSFastInitFaceList(void) {
    gFaceCache = gFaceList;
    EGSFaceListInit(gFaceList, 0x96);
}

void EGSDisplayRegisterFaceCollisionAnim(void *anim) {
    gPointSourceCount = 1;
    gPointSourceArray[0].anim = anim;
    gPointSourceArray[0].anim2 = anim;
}

void EGSDisplaySetFlashlightFrame(void) {
}

void EGSDisplaySetFlashlightParameters(s16 intensity, s16 shift, s32 shift2) {
    s32 level = intensity;

    if (level > 0x80) {
        level = 0x80;
    }
    gFlashlightIntensity = level;
    gFlashlightShift = (shift2 << 16) + shift;
}

void EGSDisplaySetFlashTime(s32 effect) {
    gFlashEffect = effect;
    gFlashIntensity = 0x78;
}

void EGSDisplayFlashEffect(s32 effect, s32 intensity) {
    gFlashEffect = effect;
    gFlashIntensity = intensity;
}

void EGSDisplayRedFlashEffect(void) {
}

EGSTexAnim *EGSDisplayInitTextureAnimation(void *a, void *b, s32 c, s32 d, s32 e) {
    EGSTexAnim *ta = EGSMemMalloc(sizeof(EGSTexAnim));

    ta->unk08 = 0x10;
    ta->unk10 = a;
    ta->unk14 = b;
    ta->unk04 = c;
    ta->unk09 = 0x40;
    if (e >= 0) {
        ta->unk06 = e;
    } else {
        ta->unk06 = 0;
    }
    ta->unk05 = d;
    ta->unk0C = 0;
    ta->node = EGSListAdd(&gTextureAnimationList, ta);
    return ta;
}

s32 EGSDisplayStartTextureAnimation(EGSTexAnim *ta) {
    if (ta != NULL) {
        ta->node = EGSListAdd(&gTextureAnimationList, ta);
        return 1;
    }
    return 0;
}

s32 EGSDisplayStopTextureAnimation(EGSTexAnim *ta) {
    if (ta == NULL) {
        return 0;
    }
    if (ta->node == NULL) {
        return 0;
    }
    EGSListRemove(&gTextureAnimationList, ta->node);
    return 1;
}

void EGSDisplayInitBillboardAnimation(EGSAnim *anim, s16 *src, s32 count) {
    BillboardAnim *ba = EGSMemMalloc(sizeof(BillboardAnim));
    s16 *frames = EGSMemMalloc(count * 4);
    s32 i;

    for (i = 0; i < count; i++) {
        s16 x = src[i * 2];
        s16 y = src[i * 2 + 1];
        frames[i * 2] = x - ((x >> 6) << 6);
        frames[i * 2 + 1] = y & 0xFF;
    }
    ba->frames = frames;
    ba->numFrames = count;
    ba->frame = rand() % count;
    ba->delay = rand() % 3 + 4;
    ba->lastTick = 0;
    anim->model->flags |= 0x10000000;
    anim->unk18 = ba;
}

void EGSDisplayGetSectorRotBoxList(EGSAnim *anim, s32 *count, void **boxes) {
    EGSSectorInfo *info = anim->model->info;
    s32 *list = NULL;

    if (info != (EGSSectorInfo *)-1) {
        list = info->rotBoxes;
    }
    if (list == NULL || anim->node[0] == NULL) {
        *count = 0;
        *boxes = NULL;
    } else {
        *count = *list;
        *boxes = list + 1;
    }
}

void *EGSDisplayGetMovieHeap(void) {
    return gTextureCache;
}

void EGSDisplayForceUpdate(EGSAnim *anim) {
    anim->flags |= 0x80;
}

void EGSDisplayStopLevel(void) {
    s32 i;

    gFogScrimFlag = 0;
    gTextureAnimationList = NULL;
    gCameraArray[0].refAnim = NULL;
    gFlashlightList = NULL;
    gPointSourceList = NULL;
    gFlashEffect = 0;
    gCameraArray[0].flags &= ~1;
    for (i = 0; i < 3; i++) {
        EGSDisplayFlushCamera(&gCameraArray[i]);
    }
    EGSDisplayFadeOff();
}

void EGSDisplayReinitCameras(void) {
    s32 i;

    gCameraID = 0;
    gHMDVertTriplets = 0;
    gDisplayInterruptList = NULL;
    gTextureAnimationList = NULL;
    gFlashlightList = NULL;
    gFlashlightFrame = NULL;
    gFlashlightHeadFlag = 1;
    gFlashEffect = 0;
    gPointSourceList = NULL;
    gScrimModel = NULL;
    gRearViewFlag = 0;
    gFogScrimFlag = 0;
    for (i = (s32)(sizeof(gDisplayInterruptArray) - sizeof(DisplayInterrupt)); i >= 0; i -= sizeof(DisplayInterrupt)) {
        ((DisplayInterrupt *)((u8 *)gDisplayInterruptArray + i))->func = NULL;
    }
    gPointSourceCount = 0;
    masterAnimCount = -1;
    InitGabeDamage();
    InitImages();
    InitDamagePolys();
}

void EndFlash(EGSAnim *anim) {
    *anim->dla = NULL;
    anim->flags2 &= ~0x10;
}

void EGSAnimGunFlash(EGSAnim *anim, s32 time) {
    if (*anim->dla == NULL) {
        *anim->dla = gDLA_npc_gun_flash;
        anim->flags2 |= 0x10;
        EGSDisplayQueueInterrupt(EndFlash, time, (s32)anim);
        anim->model->color = 0x304050;
    }
}

void EGSAnimGunFlash2(EGSAnim *anim, s32 time, u32 color) {
    EGSAnimGunFlash(anim, time);
    anim->model->color = color & 0xFFFFFF;
}

EGSAnim *EGSDisplayGetPointSector(s32 *point) {
    EGSListNode *node;
    EGSAnim *anim;
    EGSModel *model;
    s32 id;

    for (node = gCameraArray[0].anims; node != NULL; node = node->next) {
        anim = node->data;
        model = anim->model;
        if (model->flags & 0x1000000) {
            id = model->info->id;
            if (id != -1 && id != 0xF0) {
                if (point[0] > model->min[0] && point[0] < model->max[0] && point[2] > model->min[2] &&
                    point[2] < model->max[2]) {
                    return anim;
                }
            }
        }
    }
    return NULL;
}

void InitDamagePolys(void) {
    s32 off;

    for (off = (DAMAGE_MAX - 1) * sizeof(EGSDamagePoly); off >= 0; off -= sizeof(EGSDamagePoly)) {
        ((EGSDamagePoly *)((u8 *)gDamagePolys + off))->active = 0;
    }
    gDamagePolyPtr = 0;
}

/* v = py & 0xFF is a single set (`v = py; v &= 0xFF` gets combined and leaves a USE at the block
 * label, so v dies twice and loses its local tie); the uv[0] high part goes in a single-death `t`
 * so the or/<< 16 chain ties into one reg, and uv[2]/uv[3] use their own `x`. */
void EGSDisplayRegisterDamage(VECTOR *pos, VECTOR *dir, VECTOR *normal, EGSTexture *tex, s32 owner) {
    VECTOR a;
    VECTOR b;
    EGSDamagePoly *d;
    s32 shift;
    s32 u;
    s32 w;
    u16 py;
    s16 h;
    s32 v;
    s32 t;
    s32 x;
    s32 s;

    d = &gDamagePolys[gDamagePolyPtr++];
    shift = 9;
    u = tex->image.px & 0x3F;
    w = tex->image.pw * 2 - 1;
    py = tex->image.py;
    h = tex->image.ph - 1;
    if (owner == 666) {
        shift = 8;
    }
    if (D_8010FE68.initDamage) {
        InitDamagePolys();
        D_8010FE68.initDamage = 0;
    }
    if (gDamagePolyPtr == DAMAGE_MAX) {
        gDamagePolyPtr = 0;
    }
    d->active = 1;
    d->owner = (void *)owner;
    t = ((tex->image.cy << 6) | ((tex->image.cx >> 4) & 0x3F)) << 16;
    v = py & 0xFF;
    d->uv[0] = t + (v << 8) + u * 2;
    x = ((v + h) << 8) + u * 2;
    s = ((((py >> 4) & 0x10) | (((tex->image.px & 0x3FF) >> 6) | 0x80) | ((py & 0x200) * 4)) << 16) + (v << 8) + u * 2 +
               (s16)w;
    d->uv[2] = x;
    d->uv[3] = x + (s16)w;
    d->uv[1] = s;
    OuterProduct12(dir, normal, &a);
    if (a.vx == 0 && a.vy == 0 && a.vz == 0) {
        OuterProduct12(dir, pos, &a);
    }
    GTEMath_VectorNormal(&a, &a);
    OuterProduct12(&a, dir, &b);
    GTEMath_VectorNormal(&b, &b);
    a.vx >>= shift;
    a.vy >>= shift;
    a.vz >>= shift;
    b.vx >>= shift;
    b.vy >>= shift;
    b.vz >>= shift;
    d->v[0].vx = pos->vx - a.vx + b.vx;
    d->v[0].vy = -(pos->vy - a.vy + b.vy);
    d->v[0].vz = pos->vz - a.vz + b.vz;
    d->v[1].vx = pos->vx + a.vx + b.vx;
    d->v[1].vy = -(pos->vy + a.vy + b.vy);
    d->v[1].vz = pos->vz + a.vz + b.vz;
    d->v[2].vx = pos->vx + a.vx - b.vx;
    d->v[2].vy = -(pos->vy + a.vy - b.vy);
    d->v[2].vz = pos->vz + a.vz - b.vz;
    d->v[3].vx = pos->vx - a.vx - b.vx;
    d->v[3].vy = -(pos->vy - a.vy - b.vy);
    d->v[3].vz = pos->vz - a.vz - b.vz;
}

void EGSDisplayRegisterFootprint(EGSFrame *frame, s32 kind, void *owner) {
    VECTOR dir;
    VECTOR pos;

    pos.vx = frame->matrix.t[0];
    pos.vy = frame->matrix.t[1];
    pos.vz = frame->matrix.t[2];
    pos.vy = -pos.vy;
    dir.vx = frame->matrix.m[0][2];
    dir.vy = frame->matrix.m[1][2];
    dir.vz = frame->matrix.m[2][2];
    dir.vy = -dir.vy;
    dir.vx = -dir.vx;
    dir.vy = -dir.vy;
    dir.vz = -dir.vz;
    EGSDisplayRegisterDamage(&pos, owner, &dir, kind, 666);
}

void EGSDisplayRemoveDamage(void *owner) {
    s32 i;

    for (i = 0; i < DAMAGE_MAX; i++) {
        if (gDamagePolys[i].owner == owner) {
            gDamagePolys[i].active = 0;
        }
    }
}

s32 EGSDisplayHASetCurrentFrame(EGSAnim *anim, s32 id, s32 frame) {
    HAnimation *ha = EGSDisplayGetHA(anim, id);
    EGSSectorInfo *info = anim->model->info;
    u8 *p;
    u8 b;
    u32 mask;
    s32 i;
    s32 f;

    if (ha == NULL) {
        return 0x2D;
    }
    p = ha->data;
    for (;;) {
        if ((*p >> 4) != 0xF) {
            return 0;
        }
        f = p[1];
        if (f == frame) {
            ha->cur = p;
            ha->frame = f;
            return 0;
        }
        if (f == 0xFC) {
            return 0x2E;
        }
        p += 2;
        {
            u32 hi = p[0];
            mask = p[1] | (hi << 8);
        }
        p += 2;
        for (i = 0; i < info->numParts; i++, mask >>= 1) {
            b = *p;
            if (mask & 1) {
                if (!(b & 0x80)) {
                    p += 2;
                } else if (b & 0x40) {
                    p += 4;
                } else if (!(b & 0x20)) {
                    p += 3;
                } else {
                    p += 6;
                }
                if (f == 1 || i == 0) {
                    p += 3;
                }
            }
        }
    }
}

void EGSDisplaySetAlternateViewpoint(MATRIX *m) {
    gGabeCoord.flg = 0;
    m->m[0][2] = -m->m[0][2];
    m->m[1][2] = -m->m[1][2];
    m->m[2][2] = -m->m[2][2];
    m->m[0][0] = -m->m[0][0];
    m->m[1][0] = -m->m[1][0];
    m->m[2][0] = -m->m[2][0];
    gGabeCoord.coord = *m;
    GsSetView2(&gGabeView);
    EGSDisplaySetupGTE(&gEGSIdentityMatrix);
    gInvWSMatrix = GsWSMATRIX;
    m->m[0][2] = -m->m[0][2];
    m->m[1][2] = -m->m[1][2];
    m->m[2][2] = -m->m[2][2];
    m->m[0][0] = -m->m[0][0];
    m->m[1][0] = -m->m[1][0];
    m->m[2][0] = -m->m[2][0];
    gGabeCoord.coord = *m;
    GsSetView2(&gGabeView);
    gGabeWSMatrix = GsWSMATRIX;
    EGSDisplaySetupGTE(&gEGSIdentityMatrix);
}

void EGSAnimOnScreen(EGSAnim *anim) {
    GsSetProjection(gCameraArray[0].projection);
    EGSDisplaySetupGTE(anim->frame);
    EGSDisplayOnScreen(0x2000, 0xF0);
}

void EGSDisplayGetLoad(u16 *c, u16 *g, u16 *p, char **err) {
    *c = gLoadC;
    *g = gLoadG;
    *p = gLoadP;
    *err = gErrorString;
}

void EGSDisplayPointSourceInit(EGSPointSource *ps) {
    ps->node = NULL;
    ps->anim = NULL;
    ps->color = 0;
    ps->radius = 0;
}

void EGSDisplayPointSourceFrame(EGSPointSource *ps, void *frame) {
    ps->frame = frame;
}

void EGSDisplayPointSourceParams(EGSPointSource *ps, s16 radius, s32 color) {
    ps->radius = radius;
    ps->color = color;
}

s32 EGSDisplayPointSourceOn(EGSPointSource *ps) {
    if (ps->node != NULL) {
        return 0;
    }
    if (ps->frame != NULL) {
        ps->node = EGSListAdd(&gPointSourceList, ps);
        return 1;
    }
    return 0;
}

s32 EGSDisplayPointSourceOff(EGSPointSource *ps) {
    if (ps->node == NULL) {
        return 0;
    }
    EGSListRemove(&gPointSourceList, ps->node);
    ps->node = NULL;
    return 1;
}

void EGSDisplayFlashlightInit(EGSFlashlight *fl) {
    fl->range = 0x50;
    fl->width = 0xE;
    fl->node = NULL;
    fl->frame = NULL;
    fl->headFlag = 0;
    fl->shift = 6;
    fl->unk18 = 0;
    fl->color = 0xFFFFFF;
}

void EGSDisplayFlashlightFrame(EGSFlashlight *fl, void *frame, u8 headFlag) {
    fl->headFlag = headFlag != 0;
    fl->frame = frame;
}

void EGSDisplayFlashlightParams(EGSFlashlight *fl, s32 range, s32 width, s32 shift) {
    fl->range = range;
    fl->width = width;
    fl->shift = shift;
}

s32 EGSDisplayFlashlightOn(EGSFlashlight *fl) {
    if (fl->node != NULL) {
        return 0;
    }
    if (fl->frame != NULL) {
        fl->node = EGSListAdd(&gFlashlightList, fl);
        return 1;
    }
    return 0;
}

s32 EGSDisplayFlashlightOff(EGSFlashlight *fl) {
    if (fl->node == NULL) {
        return 0;
    }
    EGSListRemove(&gFlashlightList, fl->node);
    fl->node = NULL;
    return 1;
}

s32 EGSDisplayIsFlashlightOn(EGSFlashlight *fl) {
    return fl->node != NULL;
}

void EGSDisplaySetAmbientLight(s32 r, s32 g, s32 b) {
    gAmbientLight = (b << 16) + (g << 8) + r;
}

void EGSDisplaySetCollideXZ(s32 xz, s32 fineXZ) {
    gCollideXZ = xz;
    gFineCollideXZ = fineXZ;
}

void iEGSDisplayFromLetterbox(void (*done)()) {
    s32 i;
    u8 busy = 0;
    RECT *r;

    for (i = 0; i < 3; i++) {
        r = gCameraArray[i].region;
        if (done == NULL) {
            while (r->h < 0xF0) {
                r->h += 4;
                r->y -= 2;
            }
        } else if (r->h < 0xF0) {
            busy = 1;
            r->h += 4;
            r->y -= 2;
        }
    }
    if (done != NULL) {
        if (busy) {
            EGSDisplayQueueInterrupt(iEGSDisplayFromLetterbox, 2, (s32)done);
        } else {
            done();
        }
    }
}

void iEGSDisplayToLetterbox(void (*done)()) {
    s32 i;
    u8 busy = 0;
    RECT *r;

    for (i = 0; i < 3; i++) {
        r = gCameraArray[i].region;
        if (done == NULL) {
            while (r->h > 0xA0) {
                r->h -= 4;
                r->y += 2;
            }
        } else if (r->h > 0xA0) {
            busy = 1;
            r->h -= 4;
            r->y += 2;
        }
    }
    if (done != NULL) {
        if (busy) {
            EGSDisplayQueueInterrupt(iEGSDisplayToLetterbox, 2, (s32)done);
        } else {
            done();
        }
    }
}

void EGSDisplayFromLetterbox(void (*done)()) {
    EGSDisplayQueueInterrupt(iEGSDisplayToLetterbox, 0, (s32)done);
    iEGSDisplayFromLetterbox(done);
}

void EGSDisplayToLetterbox(void (*done)()) {
    EGSDisplayQueueInterrupt(iEGSDisplayFromLetterbox, 0, (s32)done);
    iEGSDisplayToLetterbox(done);
}

void EGSDisplayTextOverMovies(u8 on) {
    if (on) {
        gCameraArray[1].flags |= 4;
    } else {
        gCameraArray[1].flags &= ~4;
    }
}

u32 EGSDisplayGetFuzzyFoggedOutDistance(void) {
    u32 start = gTerrainFog & 0xFFFF;
    u32 shift = ((u32)gTerrainFog >> 16) - 1;
    u32 dist = start + ((0x1000 - start) >> shift);

    if (start >= 0x1000) {
        return 15000;
    }
    return dist + (dist >> 3);
}

void EGSDisplayFlushCamera(EGSCamera *camera) {
    EGSListFlush(&camera->anims);
    EGSListFlush(&camera->sprites);
    EGSListFlush(&camera->glines);
    EGSListFlush(&camera->polys);
}

void EGSDisplayRegisterFogScrim(void *info) {
    gFogInfo = info;
    gFogScrimFlag = 1;
}

void EGSDisplaySetFrameRate(s16 rate) {
    gFrameRate = rate;
}

void EGSDisplayNoSort(void) {
    DrawSync(0);
}

void EGSDisplayResetOTs(void) {
    s32 i;

    for (i = 0; i < gCameraID * 2; i++) {
        gOtbl[i].length = 0;
    }
}

#ifndef _PSYQ
/*
 * The original reads dTicks with lui/lw, not gp (maybe this was its own TU, dTicks extern
 * there). maspsx gp-converts every symbol defined in this file's .sdata, so reach it through
 * an asm name it doesn't recognise ("dTicks+0+0"; GNU as expands that to lui/lw). Under ccpsx
 * this stays INCLUDE_ASM until aspsx's handling of that operand has been checked.
 */
extern s32 dTicksNoGp[] __asm__("dTicks+0+0");
#define POLY_SH(p, o) (*(s16 *)((p) + (o)))
void BillboardAnimate(EGSAnim *anim) {
    BillboardAnim *ba = anim->unk18;
    u8 *poly;
    s16 *xy;
    s32 u;
    s32 vb;
    s32 vt;

    if (ba->delay != 0 && dTicksNoGp[0] >= ba->lastTick + ba->delay) {
        ba->lastTick = dTicksNoGp[0];
        poly = (u8 *)anim->model->info;
        xy = &ba->frames[(s16)ba->frame++ * 2];
        u = xy[0] * 2;
        vb = (xy[1] + 0x1F) << 8;
        vt = xy[1] << 8;
        POLY_W(poly, 0x28) = vb + u + 0x1F;
        POLY_W(poly, 0x44) = vt + u;
        POLY_W(poly, 0x20) = (POLY_SH(poly, 0x22) << 16) + vb + u;
        POLY_W(poly, 0x24) = (POLY_SH(poly, 0x26) << 16) + vt + u;
        POLY_W(poly, 0x3C) = (POLY_SH(poly, 0x3E) << 16) + vt + u + 0x1F;
        POLY_W(poly, 0x40) = (POLY_SH(poly, 0x42) << 16) + vb + u + 0x1F;
        if ((s16)ba->frame == ba->numFrames) {
            ba->frame = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", BillboardAnimate);
#endif

s32 FuzzyXZDistancePoint2Point(VECTOR *a, VECTOR *b) {
    s32 dx;
    s32 dz;
    s32 sum;
    s32 diff;
    s32 half;
    s32 quarter;

    dx = a->vx - b->vx;
    if (dx < 0) {
        dx = b->vx - a->vx;
    }
    dz = a->vz - b->vz;
    sum = dx + dz;
    if (dz < 0) {
        dz = b->vz - a->vz;
        sum = dx + dz;
    }
    diff = dx - dz;
    half = sum >> 1;
    quarter = sum >> 2;
    if (diff < 0) {
        diff = dz - dx;
    }
    if (diff < half) {
        sum -= quarter;
    } else if (half + quarter >= diff) {
        sum -= sum >> 3;
    }
    return sum;
}

#ifdef NON_MATCHING
/* r7K first C draft, 228/863 masked diff lines (control flow follows the original). The original
 * reaches gTerrainFog/gFogFlag/gRunFlag/gFLCount/gGameTick/dTicks/gInfraredColor* with lui/lw
 * (only gOTStart through $gp), like BillboardAnimate: it was built where those were not small
 * data of the TU. Most call-crossing values live in stack slots reloaded through t1-t3 (slot order
 * 0x90.. = declaration order below). Left: anim gets s2 (orig s1, shared with sortBuffer), the
 * sortBuffer*0x14 / camera spill schedule at entry, and reload-register (t1-t3) choices that stop
 * jump2 cross-jumping the bias stores (-72/-10). A while loop whose run test does its own
 * advance (as the original layout suggests) scores worse (274). */
extern s32 gInfraredColorNoGp[] __asm__("gInfraredColor+0+0");
extern s32 gInfraredColorSnifferNoGp[] __asm__("gInfraredColorSniffer+0+0");
extern s32 gTerrainFogNoGp[] __asm__("gTerrainFog+0+0");
extern s32 gFogFlagNoGp[] __asm__("gFogFlag+0+0");
extern u8 gRunFlagNoGp[] __asm__("gRunFlag+0+0");
extern s32 gFLCountNoGp[] __asm__("gFLCount+0+0");
extern s32 gGameTickNoGp[] __asm__("gGameTick+0+0");
extern s32 dTicksNoGp2[] __asm__("dTicks+0+0");
extern u16 D_8012DB22;
extern EGSListNode *D_8012D8CC;
void EGSDisplayTMDSetupGTE(EGSFrame *frame);
s32 EGSDisplayTMDOnScreen(s32 dist, s32 size);
void EGSDisplaySortGMD(void *tmd, u8 flags, u32 *ot, s8 attr);
void DoSortHMD(void *hmd, s32 flags, u32 *ot, EGSAnim *anim);
void EGSDisplayAnimateHMD(EGSAnim *anim);
void EGSFrameUpdateHierarchy(EGSFrame *frame);
void EGSFrameSetRotationMatrix(EGSFrame *frame, EGSFrame *parent, MATRIX *m);
void EGSGeometry_UnitVectorToRotationMatrix(VECTOR *v, s32 arg1, MATRIX *m);
s32 EGSDisplayCalculateHMDTerrainLighting(EGSListNode *node, EGSAnim *anim, SVECTOR *out);
void BillboardAnimate(EGSAnim *anim);

/* GTE far (back) colour: RBK, GBK, BBK */
#define gte_SetBackColor(r, g, b) \
    __asm__ volatile("ctc2 %0, $13\n\tctc2 %1, $14\n\tctc2 %2, $15" : : "r"(r), "r"(g), "r"(b))

/* Attached light of an attached anim (EGSAnim.dla viewed as HMDSortLight in egshmd.c) */
typedef struct SortGsLight {
    /* 0x00 */ void *dla;
    /* 0x04 */ s16 r, g, b;
} SortGsLight;

void SortGs(EGSCamera *camera, EGSListNode *anims, s32 sortBuffer) {
    VECTOR pos;
    MATRIX rot;
    MATRIX view;
    VECTOR dir;
    VECTOR side;
    VECTOR n;
    EGSModel *model;
    u32 mflags;
    u32 *ot;
    s32 index;
    s32 fogNear;
    s32 fogFar;
    s32 fogRange;
    s32 fogOn;
    EGSAnim *gabe;
    u8 run;
    s32 flags;
    u8 sniffer;
    u8 boost;
    s16 bias;
    s32 far;
    s32 range;
    u8 attr;
    s32 dist;
    EGSAnim *att;
    EGSAnim *anim;
    EGSFrame *f;
    FLEntry *e;
    SortGsLight *light;
    void *tmd;
    s32 z;
    s32 d;
    s32 a;
    s32 sh;
    s32 v;
    s32 i;
    u8 wide;
    s16 angle;
    s32 sum;
    s32 sumG;
    s32 sumB;
    s32 lev;
    s32 r;
    s16 g;
    s16 b;
    s32 c;
    s32 cap;
    s32 t;
    u8 fb;

    sniffer = 0;
    fogNear = gTerrainFogNoGp[0] & 0xFFFF;
    fogFar = fogNear + ((0x1000 - fogNear) >> (gTerrainFogNoGp[0] >> 16));
    fogRange = fogFar - fogNear;
    flags = camera->flags;
    ot = gOtbl[sortBuffer].org;
    index = camera->index;
    fogOn = gFogFlagNoGp[0];
    gabe = gPointSourceArray[0].anim;
    run = gRunFlagNoGp[0];
    if (flags & 0x10) {
        sniffer = gInfraredColorNoGp[0] == gInfraredColorSnifferNoGp[0];
    }
    boost = 0;
    if (index == 0 && !(flags & 0x10)) {
        boost = D_8012DB22 & 1;
    }
    if ((u32)gGameTickNoGp[0] < 9) {
        return;
    }
    gOTStart = ot;
    if (camera->anims != NULL) {
        f = gCameraArray[0].frame;
        view.m[0][0] = f->matrix.m[0][0];
        view.m[0][1] = -f->matrix.m[0][1];
        view.m[0][2] = f->matrix.m[0][2];
        view.m[1][0] = -f->matrix.m[1][0];
        view.m[1][1] = f->matrix.m[1][1];
        view.m[1][2] = -f->matrix.m[1][2];
        view.m[2][0] = f->matrix.m[2][0];
        view.m[2][1] = -f->matrix.m[2][1];
        view.m[2][2] = f->matrix.m[2][2];
        view.t[0] = f->matrix.t[0];
        view.t[1] = -f->matrix.t[1];
        view.t[2] = f->matrix.t[2];
        dir.vx = view.m[0][0];
        dir.vy = view.m[1][0];
        dir.vz = view.m[2][0];
        dir.vx = -dir.vx;
        dir.vy = -dir.vy;
        dir.vz = -dir.vz;
        dir.vy = 0;
        GTEMath_VectorNormal(&dir, &dir);
        side.vy = 0x1000;
        side.vx = -dir.vz;
        side.vz = dir.vx;
        GTEMath_VectorNormal(&side, &n);
        EGSGeometry_UnitVectorToRotationMatrix(&n, 0, &rot);
    }
    while (anims != NULL) {
        anim = anims->data;
        model = anim->model;
        mflags = model->flags;
        if (!(mflags & 0x6000000) || (anim->flags & 8)) {
            goto next;
        }
        sum = 0;
        sumG = 0;
        sumB = 0;
        bias = 0;
        far = fogFar;
        range = fogRange;
        att = NULL;
        attr = anim->unk09;
        if ((mflags & 0x400000) && (anim->flags2 & 0x40)) {
            dist = 20000;
        } else if (mflags & 0x10000) {
            dist = 2000;
            att = anim->unk18;
            if (att != NULL) {
                dist = 4500;
            } else if (model->unk24 != 0) {
                f = anim->frame;
                if (f->node == NULL) {
                    f->matrix.m[0][0] = rot.m[0][0];
                    anim->frame->matrix.m[0][1] = -rot.m[0][1];
                    anim->frame->matrix.m[0][2] = rot.m[0][2];
                    anim->frame->matrix.m[1][0] = -rot.m[1][0];
                    anim->frame->matrix.m[1][1] = rot.m[1][1];
                    anim->frame->matrix.m[1][2] = -rot.m[1][2];
                    anim->frame->matrix.m[2][0] = rot.m[2][0];
                    anim->frame->matrix.m[2][1] = -rot.m[2][1];
                    anim->frame->matrix.m[2][2] = rot.m[2][2];
                } else {
                    EGSFrameSetRotationMatrix(f, NULL, &rot);
                }
            }
        } else {
            dist = 4500;
        }
        for (i = 0; i < gFLCountNoGp[0]; i++) {
            e = &gFLArray[i];
            angle = *(s16 *)&e->angle;
            pos.vx = anim->frame->matrix.t[0];
            pos.vy = anim->frame->matrix.t[1];
            wide = e->angle >> 31;
            pos.vy = -pos.vy;
            pos.vz = anim->frame->matrix.t[2];
            d = FuzzyXZDistancePoint2Point(&pos, (VECTOR *)e);
            if (!wide ? d < 0x821 : d < 0x501) {
                pos.vx -= e->x;
                pos.vy -= e->y;
                pos.vz -= e->z;
                a = ratan2(pos.vz, pos.vx) - angle;
                if (a < 0) {
                    a += 0x1000;
                }
                if (a > 0x800) {
                    a = 0x1000 - a;
                }
                if (a < 600) {
                    sh = d >> 9;
                    if (sh < 2) {
                        sh = 2;
                    }
                    v = (600 - a) >> sh;
                    if (!wide) {
                        sum += v;
                        sumG += v;
                        sumB += v;
                    } else {
                        sum = v >> 2;
                        sumG += v >> 4;
                        sumB += v >> 4;
                    }
                }
            }
        }
        if (anim->unk09 & 0x80) {
            far += far >> 1;
            range = far - fogNear;
        }
        EGSDisplayTMDSetupGTE(anim->frame);
        if (anim->flags2 & 0x20) {
            z = EGSDisplayTMDOnScreen(dist, 0x400);
        } else {
            z = EGSDisplayTMDOnScreen(dist, index ? 0x8C : 0x190);
        }
        if (!(anim->flags2 & 0x40)) {
            if (z == 0 || (fogOn && far < z)) {
                goto next;
            }
        }
        if (anim->unk09 & 0x10) {
            lev = (anim->unk09 & 0xF) << 8;
        } else {
            lev = 0xC00;
        }
        lev += sum * 16;
        r = 0xFFF;
        if (lev < 0x1000) {
            r = lev;
        }
        if (flags & 0x10) {
            b = 0;
            r = 0;
            g = 0x3FF;
        } else if (anim->unk09 & 0x40) {
            c = (s32)anim->unk18;
            r = c >> 16;
            if (c == 0) {
                EGSDisplayCalculateHMDTerrainLighting(D_8012D8CC, anim, (SVECTOR *)&n);
                c = ((((SVECTOR *)&n)->vx >> 4) << 16) + ((((SVECTOR *)&n)->vy >> 4) << 8) +
                    (((SVECTOR *)&n)->vz >> 4);
                anim->unk18 = (void *)c;
                if (c == 0) {
                    anim->unk18 = (void *)1;
                }
                r = c >> 16;
            }
            g = (c >> 8) & 0xFF;
            b = c & 0xFF;
            if (sum != 0) {
                r = r + sum < 0x100 ? r + sum : 0xFF;
                g = g + sumG < 0x100 ? g + sumG : 0xFF;
                b = b + sumB < 0x100 ? b + sumB : 0xFF;
            }
            r <<= 4;
            g <<= 4;
            b <<= 4;
        } else {
            b = r;
            if (sum >= 31 && sumB < sum) {
                t = r >> 6;
                cap = sum < 0x100 ? sum : 0xFF;
                if (t < cap) {
                    t = cap;
                }
                r = t << 4;
                b = 0;
                g = 0;
            } else {
                g = b;
            }
        }
        if (fogOn && fogNear < z) {
            t = far - z;
            r = (s16)r * t / range;
            g = g * t / range;
            gte_SetBackColor(r, g, b * t / range);
        } else {
            gte_SetBackColor(r, g, b);
        }
        anim->flags2 &= 0x7F;
        if (att != NULL) {
            light = (SortGsLight *)att->dla;
            if (!run || att->node[0] == NULL) {
                goto next;
            }
            if (((flags & 0x10) || (D_8012DB22 & 1)) && att == gabe) {
                goto next;
            }
            anim->flags2 = (anim->flags2 & 0xF) | (att->flags2 & 0xF0) | 0x80;
            if (att->unk09 & 0x20) {
                attr |= 0x20;
            }
            if (!(flags & 0x10)) {
                gte_SetBackColor(light->r, light->g, light->b);
            }
            fb = att->unk0B;
            if (fb & 0x10) {
                bias = -72;
            } else if (fb & 0x20) {
                bias = -24;
            } else {
                d = 24;
                if (att == gabe) {
                    d = 12;
                }
                if (att->side == 1) {
                    bias = d;
                } else if (att->side == -1) {
                    bias = -d;
                }
                if (z < 60 && bias < 0) {
                    bias = 0;
                }
            }
        } else if (anim->side == 1) {
            bias = gabe->side * 12;
        } else {
            bias = -10;
        }
        gOtbl[sortBuffer].org = ot + bias;
        if (mflags & 0x10000000) {
            BillboardAnimate(anim);
        } else if (!(mflags & 0x10000)) {
            anim->side = 0;
        } else if (anim->flags & 0x20) {
            EGSFrameUpdateHierarchy(anim->frame);
        }
        if (mflags & 0x400000) {
            EGSDisplayAnimateHMD(anim);
            DoSortHMD(model->info, camera->flags << 16, gOtbl[sortBuffer].org, anim);
        } else {
            if (anim->flags2 & 4) {
                tmd = (void *)model->unk24;
            } else {
                tmd = model->info;
            }
            if ((mflags & 0x10000) && att == NULL && model->unk24 != 0) {
                t = dTicksNoGp2[0] & 0xF;
                if (t >= 8) {
                    c = 0xFFF - (15 - t) * 250;
                } else {
                    c = 0xFFF - t * 250;
                }
                if (!(flags & 0x10)) {
                    gte_SetBackColor(c, c, c);
                }
                tmd = (void *)model->unk24;
            }
            if (boost) {
                gte_SetBackColor(300, 300, 300);
            }
            if (sniffer) {
                attr |= 0x20;
            }
            EGSDisplaySortGMD(tmd, anim->flags2, gOtbl[sortBuffer].org, attr);
        }
        if (bias != 0) {
            gOtbl[sortBuffer].org = ot;
        }
    next:
        anims = anims->next;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", SortGs);
#endif

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTMDSetupGTE);

/* EGSDisplayTMDSetupGTE / TMDOnScreen / SortGMD are hand-written GTE asm (add/sub/addi,
 * $t8/$t9 temps, no $ra save, saves at 0($sp)) */
INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplayTMDOnScreen);

INCLUDE_ASM("asm/main/nonmatchings/egsdisp", EGSDisplaySortGMD);

/* The original keeps a dead ret=0 in s2 but returns a literal 0 on both paths: s2 needs a use that survives
 * flow/global-alloc but emits no code. Tried (r5A..r9W): REG_EQUIV single-set ret, dead counters, in-loop
 * `ret = ret`, ret placed after the loop's first branch (not hoisted -> real addu), combine-folded uses
 * (`ret & count`, `ret != 0`: ret is set twice so combine's nonzero_bits is unknown). */
s32 EGSDisplayCalculateHMDTerrainLighting(EGSListNode *node, EGSAnim *anim, SVECTOR *out) {
    LightAccum acc;
    SVECTOR unused;
    VECTOR p;
    EGSModel *model;
    EGSFrame *frame;
    s32 count;
    s32 ret;

    frame = anim->frame;
    acc.pos[0] = frame->matrix.t[0];
    acc.pos[1] = frame->matrix.t[1];
    acc.pos[2] = frame->matrix.t[2];
    acc.count = acc.sum[0] = acc.sum[1] = acc.sum[2] = 0;
    EGSFrameGetPosition(anim->frame, NULL, &p);
    ret = 0;
    for (; node != NULL; node = node->next) {
        ret = 0;
        model = ((EGSAnim *)node->data)->model;
        if ((model->flags & 0x1000000) && p.vx > model->min[0] && p.vx < model->max[0] && p.vz > model->min[2] &&
            p.vz < model->max[2]) {
            EGSDisplayPointColor(model->info, &acc);
        }
    }
    count = acc.count;
    if (count != 0) {
        out->vx = acc.sum[0] / count * 16;
        out->vy = acc.sum[1] / count * 16;
        out->vz = acc.sum[2] / count * 16;
        /* FAKE: an empty asm use of ret stands in for whatever kept ret (s2) live in the original;
         * the in-loop `ret = 0` is hoisted and turned into a no-op `s2 = s2` deleted in final. */
        __asm__("" : : "r"(ret));
        return 0;
    }
    out->vx = out->vy = out->vz = 450;
    return 0;
}

EGSAnim *EGSDisplayGetHMDSector(EGSAnim *anim) {
    VECTOR pos;

    pos.vx = anim->frame->matrix.t[0];
    pos.vy = anim->frame->matrix.t[1];
    pos.vz = anim->frame->matrix.t[2];
    pos.vy = -pos.vy;
    return EGSDisplayGetPointSector(&pos.vx);
}

s32 EGSDisplayHMDInActiveSector(EGSAnim *anim) {
    EGSAnim *sector = EGSDisplayGetHMDSector(anim);

    if (sector != NULL && sector->node[0] != NULL) {
        return 1;
    }
    return 0;
}

s16 EGSDisplayGetAnimSector(EGSAnim *anim) {
    EGSAnim *sector = EGSDisplayGetHMDSector(anim);

    if (sector != NULL && sector->node[0] != NULL) {
        return sector->model->info->id;
    }
    return -1;
}

void EGSDisplayHAInterpolate(EGSAnim *anim, HAnimation *ha, s32 steps) {
    EGSAnim *a = anim;
    s32 count = a->model->info->numParts;
    s32 i;
    SVECTOR tmp;

    for (i = 0; i < count; i++) {
        ha->saved[i] = a->rot[i];
    }
    for (i = 0; i < steps; i++) {
        EGSDisplayHAnimate(anim, ha, 0);
    }
    for (i = 0; i < count; i++) {
        tmp = a->rot[i];
        a->rot[i] = ha->saved[i];
        ha->saved[i] = tmp;
    }
    ha->frame = 1;
    ha->cur = ha->data;
    ha->flags |= steps + 0x20000000;
}

void EGSDisplayHADoneCallback(void (*callback)()) {
    gHADoneCallback = callback;
}
