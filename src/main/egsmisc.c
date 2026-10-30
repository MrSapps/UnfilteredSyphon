#include "common.h"
#include "game.h"

/* egsmisc.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FF38)
SDATA_LOCAL(D_8010FF3C)

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

#define EGS_LIST_POOL_SIZE 800

extern EGSListNode D_80122C90[EGS_LIST_POOL_SIZE]; /* node pool */
extern EGSListNode *D_8010FF38;                    /* free-node list */
extern u16 D_8010FF3C;                             /* nodes in use */

/* Collision box: min corner, then max corner (4 words each) */
typedef struct EGSBox {
    /* 0x00 */ s32 min[4];
    /* 0x10 */ s32 max[4];
} EGSBox;

typedef struct EGSMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} EGSMatrix;

typedef struct EGSObject {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ void *frame;
} EGSObject;

s32 EGSAnimGetBox(EGSObject *obj, s32 arg1, EGSBox *box);
void EGSFrameGetMatrix(void *frame, s32 arg1, EGSMatrix *mtx);
void EGSGeometry_TransformPoint(void *point, void *mtx, void *out);
s32 EGSMathFIXMul(s32 a, s32 b);

void EGSListInit(void) {
    s32 i;

    D_8010FF3C = 0;
    for (i = 0; i < EGS_LIST_POOL_SIZE; i++) {
        if (i < EGS_LIST_POOL_SIZE - 1) {
            D_80122C90[i].next = &D_80122C90[i + 1];
        } else {
            D_80122C90[i].next = NULL;
        }
        if (i != 0) {
            D_80122C90[i].prev = &D_80122C90[i - 1];
        } else {
            D_80122C90[i].prev = NULL;
        }
        D_80122C90[i].data = NULL;
    }
    D_8010FF38 = D_80122C90;
}

s32 EGSListFind(EGSListNode **list, EGSListNode *node, void *data) {
    EGSListNode *cur;

    for (cur = *list; cur != NULL; cur = cur->next) {
        if (cur == node) {
            return 1;
        }
        if (cur->data == data) {
            return 1;
        }
    }
    return 0;
}

EGSListNode *EGSListAdd(EGSListNode **list, void *data) {
    EGSListNode *node = D_8010FF38;

#line 71
    ASSERT(node != NULL);
    D_8010FF38 = node->next;
    D_8010FF38->prev = NULL;
    node->data = data;
    node->next = *list;
    node->prev = NULL;
    (*list)->prev = node;
    *list = node;
    D_8010FF3C++;
    return *list;
}

EGSListNode *EGSListAddToEnd(EGSListNode **list, EGSListNode *after, void *data) {
    EGSListNode *node = D_8010FF38;

#line 91
    ASSERT(node != NULL);
    D_8010FF38 = node->next;
    D_8010FF38->prev = NULL;
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    if (*list == NULL) {
        *list = node;
    } else {
        if (after == NULL) {
            after = *list;
            while (after->next != NULL) {
                after = after->next;
            }
        }
        after->next = node;
        node->prev = after;
    }
    D_8010FF3C++;
    return *list;
}

void EGSListRemove(EGSListNode **list, EGSListNode *node) {
    EGSListNode *next;
    EGSListNode *free;

    if (node == NULL || *list == NULL) {
        return;
    }
    next = node->next;
    if (node->prev != NULL) {
        node->prev->next = next;
    } else {
        *list = next;
    }
    if (next != NULL) {
        next->prev = node->prev;
    }
    free = D_8010FF38;
    D_8010FF38 = node;
    free->prev = node;
    node->prev = NULL;
    node->next = free;
    D_8010FF3C--;
}

void EGSListFlush(EGSListNode **list) {
    EGSListNode *node;
    EGSListNode *next;

    node = *list;
    while (node != NULL) {
        next = node->next;
        EGSListRemove(list, node);
        node = next;
    }
    *list = NULL;
}

s32 EGSListCount(EGSListNode *node) {
    s32 count = 0;

    while (node != NULL) {
        node = node->next;
        count++;
    }
    return count;
}

void EGSMiscMemset(s32 *dst, s32 unused, u32 size) {
    s32 fill = -1;

    while (size >= 4) {
        *dst++ = fill;
        size -= 4;
    }
}

s32 EGSCollideGetUntransformedBox(EGSObject *obj, EGSBox *box) {
    s32 ret;
    s32 t;

    ret = EGSAnimGetBox(obj, 0, box);
    if (ret == 0) {
        t = box->min[1];
        box->min[1] = -box->max[1];
        box->max[1] = -t;
        return 0;
    }
    return ret;
}

/* Halve rounding towards zero, written out by hand */
#define HALF(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))

s32 EGSCollideGetTransformedCenterBottom(EGSObject *obj, s32 unused, s32 *out) {
    EGSBox box;
    s32 half[3];
    s32 point[4];
    EGSMatrix mtx;

    EGSCollideGetUntransformedBox(obj, &box);
    half[0] = HALF(box.max[0] - box.min[0]);
    half[1] = 0;
    half[2] = HALF(box.max[2] - box.min[2]);
    point[0] = box.min[0] + half[0];
    point[1] = box.min[1] + half[1];
    point[2] = box.min[2] + half[2];
    EGSFrameGetMatrix(obj->frame, 0, &mtx);
    EGSGeometry_TransformPoint(point, &mtx, out);
    return 0;
}

/*
 * Turns a box into a transformed corner (row 0, by EGSGeometry_TransformPoint)
 * plus the three rotated edge vectors (rows 1..3).
 */
static s32 func_800D85C8(EGSBox *box, EGSMatrix *mtx, s32 out[4][4]) {
    s32 axes[3][4];

    axes[0][0] = mtx->m[0][0];
    axes[0][1] = mtx->m[1][0];
    axes[0][2] = mtx->m[2][0];
    axes[1][0] = mtx->m[0][1];
    axes[1][1] = mtx->m[1][1];
    axes[1][2] = mtx->m[2][1];
    axes[2][0] = mtx->m[0][2];
    axes[2][1] = mtx->m[1][2];
    axes[2][2] = mtx->m[2][2];
    out[1][0] = EGSMathFIXMul(axes[0][0], box->max[0] - box->min[0]);
    out[1][1] = EGSMathFIXMul(axes[0][1], box->max[0] - box->min[0]);
    out[1][2] = EGSMathFIXMul(axes[0][2], box->max[0] - box->min[0]);
    out[2][0] = EGSMathFIXMul(axes[1][0], box->max[1] - box->min[1]);
    out[2][1] = EGSMathFIXMul(axes[1][1], box->max[1] - box->min[1]);
    out[2][2] = EGSMathFIXMul(axes[1][2], box->max[1] - box->min[1]);
    out[3][0] = EGSMathFIXMul(axes[2][0], box->max[2] - box->min[2]);
    out[3][1] = EGSMathFIXMul(axes[2][1], box->max[2] - box->min[2]);
    out[3][2] = EGSMathFIXMul(axes[2][2], box->max[2] - box->min[2]);
    EGSGeometry_TransformPoint(box, mtx, out);
    return 0;
}

s32 EGSCollideGetTransformedBox(EGSObject *obj, s32 unused, s32 out[4][4]) {
    EGSBox box;
    EGSMatrix mtx;
    void *frame = obj->frame;

    EGSCollideGetUntransformedBox(obj, &box);
    EGSFrameGetMatrix(frame, 0, &mtx);
    func_800D85C8(&box, &mtx, out);
    return 0;
}
