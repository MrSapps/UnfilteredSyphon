#!/usr/bin/env python3
"""Which main-exe TU owns which .data/.sdata/.sbss/.bss bytes.

    tools/data_owner.py egstex            # data referenced by one TU, grouped in runs
    tools/data_owner.py --section .sdata  # whole section, every address with its users

Every lui/lo16 pair and every $gp-relative access in orig/scus_942.40 .text is
attributed to the TU (yaml c/asm subsegment) containing the instruction.  Per-TU
data is laid out by psylink in link (= .text) order and, inside a TU, in
definition order, so a TU's static data shows up as one contiguous run of
addresses only it references.  Globals that sit inside such a run belong to the
TU too; .sbss/.bss globals that are *not* in a run are uninitialised commons,
which psylink appends at the end of .sbss/.bss in hash order (they belong to no
TU's section and stay `.comm` tentative definitions in C).

Names come from orig/siphon.map (globals) and config/symbols (everything else).
"""
import argparse
import re
import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x80010000
GP = 0x8010F600
SECTIONS = {  # from orig/siphon.map
    ".data": (0x800FC8E4, 0x8010F600),
    ".sdata": (0x8010F600, 0x8010FF68),
    ".sbss": (0x8010FF68, 0x801104A0),
    ".bss": (0x801104A0, 0x801377DC),
}
TEXT = (0x80014408, 0x800FC8E4)
LOADSTORE = (0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x22, 0x26, 0x2A, 0x2E, 0x32, 0x3A)


def text_tus():
    """[(vram start, name)] of the code subsegments of the main segment."""
    out = []
    for l in (ROOT / "config/scus_942.40.yaml").read_text().splitlines():
        m = re.match(r"\s*- \[(0x[0-9A-Fa-f]+), (c|asm|textbin|hasm), (\w+)\]", l)
        if m:
            out.append((int(m.group(1), 16) - 0x800 + BASE, m.group(3)))
        elif re.match(r"\s*- \[0x[0-9A-Fa-f]+, (data|sdata)", l):
            out.append((int(re.match(r"\s*- \[(0x[0-9A-Fa-f]+)", l).group(1), 16) - 0x800 + BASE, None))
            break
    return out


def names():
    n = {}
    for f in (ROOT / "config/symbols").glob("*.txt"):
        for l in f.read_text().splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+);", l)
            if m:
                n.setdefault(int(m.group(2), 16), m.group(1))
    glob = set()
    for l in (ROOT / "orig/siphon.map").read_text().splitlines():
        m = re.match(r"\s([0-9A-F]{8})\s\s(\w+)$", l)
        if m and not m.group(2).startswith("_"):
            n[int(m.group(1), 16)] = m.group(2)
            glob.add(int(m.group(1), 16))
    return n, glob


def refs():
    """{data address: set(tu)} for every data access in .text."""
    img = (ROOT / "orig/scus_942.40").read_bytes()[0x800:]
    tus = text_tus()
    out = defaultdict(set)
    ti = 0
    hi = {}
    for addr in range(TEXT[0], TEXT[1], 4):
        while ti + 1 < len(tus) and tus[ti + 1][0] <= addr:
            ti += 1
            hi = {}
        tu = tus[ti][1]
        if tu is None:
            break
        w, = struct.unpack_from("<I", img, addr - BASE)
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        tgt = None
        if op == 0x0F:
            hi[rt] = imm << 16
            continue
        if op in LOADSTORE:
            if rs == 28:
                tgt = (GP + simm) & 0xFFFFFFFF
            elif rs in hi:
                tgt = (hi[rs] + (imm if op == 0x0D else simm)) & 0xFFFFFFFF
        if tgt is not None and SECTIONS[".data"][0] <= tgt < SECTIONS[".bss"][1]:
            out[tgt].add(tu)
        # forget %hi values whose register is overwritten
        if op == 0:
            hi.pop((w >> 11) & 31, None)
        elif 0x08 <= op <= 0x0E or 0x20 <= op <= 0x26:
            hi.pop(rt, None)
        if w == 0x03E00008:  # jr $ra: end of function
            hi = {}
    return out


def section_of(a):
    for s, (lo, hi) in SECTIONS.items():
        if lo <= a < hi:
            return s
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tu", nargs="?")
    ap.add_argument("--section")
    args = ap.parse_args()
    r = refs()
    nm, glob = names()
    if args.tu:
        addrs = sorted(a for a, t in r.items() if args.tu in t)
    else:
        lo, hi = SECTIONS[args.section]
        addrs = sorted(a for a in r if lo <= a < hi)
    for a in addrs:
        users = sorted(r[a])
        kind = "global" if a in glob else "static?"
        print("%-6s %08X %-32s %-7s %s" % (section_of(a), a, nm.get(a, ""), kind, " ".join(users)))


if __name__ == "__main__":
    main()
