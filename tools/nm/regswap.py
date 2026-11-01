#!/usr/bin/env python3
"""List NON_MATCHING functions whose diff vs the original is registers only
(same length, same opcodes/immediates once register fields are masked)."""
import subprocess, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from score import compile, nm_funcs, built_words, orig_insns, maskw


def regmask(w):
    x = maskw(w)
    op = x >> 26
    if op == 0:                      # SPECIAL: rs rt rd
        return x & ~(0x1F << 21 | 0x1F << 16 | 0x1F << 11)
    if op in (2, 3):
        return x
    return x & ~(0x1F << 21 | 0x1F << 16)


srcs = sys.argv[1:] or sorted(set(subprocess.run(["grep", "-rl", "#ifdef NON_MATCHING", "src"], capture_output=True, text=True).stdout.split()))
for src in srcs:
    fs = nm_funcs(src)
    if not fs:
        continue
    o = compile(src)
    if not o:
        continue
    for d, n in fs:
        a = built_words(o, n)
        b = orig_insns(f"{d}/{n}.s")
        b = b[:len(a)] if len(b) > len(a) and all(len(x) == 8 for x in b) else b
        if not a or len(a) != len(b):
            continue
        ra = [regmask(w) for w in a]
        rb = [regmask(w) for w in b]
        if ra == rb:
            nd = sum(1 for x, y in zip(a, b) if maskw(x) != maskw(y))
            print(nd, len(a), n, src, flush=True)
