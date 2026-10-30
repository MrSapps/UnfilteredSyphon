#!/usr/bin/env python3
"""List the global symbols a TU's asm accesses, with the load/store widths used.

    tools/sym_widths.py <tu>

Helps pick C types for externs: lw/sw -> 32-bit, lh -> s16, lhu -> u16, lb -> s8, lbu -> u8;
'addr' means the address is taken (array/struct or function).
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

OPS = {"lw": "s32", "sw": "32", "lh": "s16", "lhu": "u16", "sh": "16",
       "lb": "s8", "lbu": "u8", "sb": "8", "addiu": "addr"}


def main():
    tu = sys.argv[1]
    files = list(Path("asm").glob(f"*/nonmatchings/{tu}/*.s"))
    seen = defaultdict(set)
    for p in files:
        for line in p.read_text().splitlines():
            m = re.search(r"\*/\s+(\w+)\s+\$\w+, %(lo|gp_rel)\((\w+)(?:\s*\+\s*0x[0-9A-Fa-f]+)?\)", line)
            if m and m.group(1) in OPS:
                seen[m.group(3)].add(OPS[m.group(1)] + ("/gp" if m.group(2) == "gp_rel" else ""))
    for sym in sorted(seen):
        print(f"{sym:36s} {' '.join(sorted(seen[sym]))}")


if __name__ == "__main__":
    main()
