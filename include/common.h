#ifndef COMMON_H
#define COMMON_H

/*
 * cc1 2.7.2 with -G8 emits every function after all file-scope asm, so a plain
 * __asm__ INCLUDE_ASM would land out of order. Wrap each include in a dummy
 * function that maspsx strips again (see tools/maspsx/README.md).
 */
#if !defined(M2CTX) && !defined(PERMUTER)
#define INCLUDE_ASM(FOLDER, NAME)                                  \
    void __maspsx_include_asm_hack_##NAME() {                     \
        __asm__(".text # maspsx-keep\n"                            \
                "\t.align\t2 # maspsx-keep\n"                     \
                "\t.set noreorder # maspsx-keep\n"                 \
                "\t.set noat # maspsx-keep\n"                      \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n" \
                "\t.set reorder # maspsx-keep\n"                   \
                "\t.set at # maspsx-keep\n");                      \
    }
#define INCLUDE_RODATA(FOLDER, NAME)                               \
    void __maspsx_include_asm_hack_rodata_##NAME() {              \
        __asm__(".section .rodata # maspsx-keep\n"                 \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n" \
                ".text # maspsx-keep\n");                          \
    }
#endif

#include "include_asm.h"

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* COMMON_H */
