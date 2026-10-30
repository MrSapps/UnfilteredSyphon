#ifndef EGSFRAME_H
#define EGSFRAME_H

#include "egsmath.h"

/*
 * Full layout of an EGS frame (0x24 bytes, EGSMemMalloc'd).
 * NOTE: gobj.h has a partial `struct EGSFrame` (pos at 0x14 == matrix.t);
 * don't include both headers in one TU.
 */
struct EGSFrameNode;

typedef struct EGSFrame {
    /* 0x00 */ MATRIX matrix; /* world (composed) matrix */
    /* 0x20 */ struct EGSFrameNode *node; /* hierarchy info, NULL if not linked */
} EGSFrame; /* size 0x24 */

/* Hierarchy node of a frame (0x30 bytes, EGSMemMalloc'd). */
typedef struct EGSFrameNode {
    /* 0x00 */ MATRIX local; /* matrix relative to the parent */
    /* 0x20 */ EGSFrame *parent;
    /* 0x24 */ EGSFrame *next; /* next sibling */
    /* 0x28 */ EGSFrame *child; /* first child */
    /* 0x2C */ u8 dirty; /* world matrix needs recomputing */
} EGSFrameNode; /* size 0x30 */

/* error codes */
#define EGS_OK 0
#define EGS_ERR_NOMEM 3
#define EGS_ERR_NULL 0x18
#define EGS_ERR_NONODE 0x2B

#endif /* EGSFRAME_H */
