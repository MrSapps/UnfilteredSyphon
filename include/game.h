#ifndef GAME_H
#define GAME_H

#include "common.h"

void EGSDebugAssert(int, int, char *file, int line);

/* Game-side assert: reports "s/<file>.c" and the line number. */
#define ASSERT(cond) \
    if (!(cond)) EGSDebugAssert(1, 0, __FILE__, __LINE__)

#endif /* GAME_H */
