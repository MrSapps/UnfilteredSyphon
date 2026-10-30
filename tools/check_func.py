#!/usr/bin/env python3
"""Compare built functions against the original:  tools/check_func.py Name [Name...] | --tu system"""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from tu_analyze import functions  # noqa: E402
from fingerprint import mask  # noqa: E402


def main():
    built = Path("build/scus_942.40").read_bytes()
    orig = Path("orig/scus_942.40").read_bytes()
    funcs = functions("build/scus_942.40.elf")
    addrs = [a for _, a in funcs]
    orig_addr = {}
    for line in Path("config/symbols/main.txt").read_text().splitlines():
        p = line.split()
        if len(p) >= 3:
            orig_addr[p[0]] = int(p[2].rstrip(";"), 16)
    want = sys.argv[1:]
    if want[:1] == ["--tu"]:
        src = Path(f"src/main/{want[1]}.c").read_text()
        want = re.findall(r"^\w[\w\s\*]*?\b(\w+)\([^;]*$", src, re.M)
    for name, a in funcs:
        if name not in want:
            continue
        i = addrs.index(a)
        end = addrs[i + 1] if i + 1 < len(addrs) else a + 4
        o = a - 0x8000F800
        n = (end - a) // 4
        oa = orig_addr.get(name, a) - 0x8000F800
        w = lambda img, off, k: int.from_bytes(img[off + 4 * k : off + 4 * k + 4], "little")  # noqa: E731
        diff = sum(1 for k in range(n) if w(built, o, k) != w(orig, oa, k))
        mdiff = sum(1 for k in range(n) if mask(w(built, o, k)) != mask(w(orig, oa, k)))
        status = "OK " if not diff else ("REL" if not mdiff else "BAD")
        print(f"{status} {name:40s} {a:08X} {n:4d} insns {diff} differ ({mdiff} ignoring relocs)")


if __name__ == "__main__":
    main()
