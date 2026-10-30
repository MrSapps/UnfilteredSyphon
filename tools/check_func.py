#!/usr/bin/env python3
"""Compare built functions against the original:  tools/check_func.py Name [Name...] | --tu system"""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from tu_analyze import functions  # noqa: E402


def main():
    built = Path("build/scus_942.40").read_bytes()
    orig = Path("orig/scus_942.40").read_bytes()
    funcs = functions("build/scus_942.40.elf")
    addrs = [a for _, a in funcs]
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
        diff = sum(1 for k in range(n) if built[o + 4 * k : o + 4 * k + 4] != orig[o + 4 * k : o + 4 * k + 4])
        print(f"{'OK ' if not diff else 'BAD'} {name:40s} {a:08X} {n:4d} insns {diff} differ")


if __name__ == "__main__":
    main()
