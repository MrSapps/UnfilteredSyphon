#!/usr/bin/env python3
"""Side-by-side disassembly of a built function vs the original:  tools/diff_func.py [-m module] Name

'!!' differs, '~~' differs only in relocated fields.
"""
import sys
from pathlib import Path

import rabbitizer

sys.path.insert(0, str(Path(__file__).parent))
from fingerprint import mask  # noqa: E402
from modinfo import module, symbol_addrs  # noqa: E402
from tu_analyze import functions  # noqa: E402

rabbitizer.config.regNames_gprAbiNames = rabbitizer.Abi.O32


def main():
    args = sys.argv[1:]
    mod = "main"
    if args[:1] == ["-m"]:
        mod, args = args[1], args[2:]
    name = args[0]
    m = module(mod)
    built = Path(m["built"]).read_bytes()
    orig = Path(m["orig"]).read_bytes()
    funcs = functions(m["elf"])
    bf = dict(funcs)
    ba = bf[name]
    oa = symbol_addrs(m["syms"]).get(name, ba)
    ad = sorted(a for _, a in funcs)
    nxt = ad[ad.index(ba) + 1] if ad.index(ba) + 1 < len(ad) else ba + 0x40
    n = (nxt - ba) // 4 + 2
    word = lambda img, a: int.from_bytes(img[a - m["base"] : a - m["base"] + 4], "little")  # noqa: E731
    for i in range(n):
        x, y = word(built, ba + 4 * i), word(orig, oa + 4 * i)
        dx = rabbitizer.Instruction(x, ba + 4 * i).disassemble()
        dy = rabbitizer.Instruction(y, oa + 4 * i).disassemble()
        flag = "  " if x == y else ("~~" if mask(x) == mask(y) else "!!")
        print(f"{flag} {i*4:4X}  {dx:40s} {dy}")


if __name__ == "__main__":
    main()
