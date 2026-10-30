#!/usr/bin/env python3
"""Side-by-side disassembly of a built function vs the original:  tools/diff_func.py Name"""
import struct
import sys
from pathlib import Path

import rabbitizer

sys.path.insert(0, str(Path(__file__).parent))
from tu_analyze import functions  # noqa: E402

rabbitizer.config.regNames_gprAbiNames = rabbitizer.Abi.O32


def words(img, addr, n):
    o = addr - 0x8000F800
    return [struct.unpack_from("<I", img, o + 4 * i)[0] for i in range(n)]


def main():
    name = sys.argv[1]
    built = Path("build/scus_942.40").read_bytes()
    orig = Path("orig/scus_942.40").read_bytes()
    bf = dict((n, a) for n, a in functions("build/scus_942.40.elf"))
    of = {}
    for line in Path("config/symbols/main.txt").read_text().splitlines():
        p = line.split()
        if len(p) >= 3:
            of[p[0]] = int(p[2].rstrip(";"), 16)
    ba = bf[name]
    oa = of.get(name, ba)
    ad = sorted(a for _, a in functions("build/scus_942.40.elf"))
    n = (ad[ad.index(ba) + 1] - ba) // 4 + 2
    for i, (x, y) in enumerate(zip(words(built, ba, n), words(orig, oa, n))):
        dx = rabbitizer.Instruction(x, ba + 4 * i).disassemble()
        dy = rabbitizer.Instruction(y, oa + 4 * i).disassemble()
        print(f"{'  ' if x == y else '!!'} {i*4:4X}  {dx:40s} {dy}")


if __name__ == "__main__":
    main()
