#!/usr/bin/env python3
"""Compare built functions against the original.

    tools/check_func.py [-m module] Name [Name...]
    tools/check_func.py --tu <tu>          (all C-defined functions of src/*/<tu>.c)

OK = identical, REL = only relocated fields differ, BAD = real differences.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from fingerprint import mask  # noqa: E402
from modinfo import module, module_of_tu, symbol_addrs  # noqa: E402
from tu_analyze import functions  # noqa: E402


def main():
    args = sys.argv[1:]
    mod = "main"
    if args[:1] == ["-m"]:
        mod, args = args[1], args[2:]
    if args[:1] == ["--tu"]:
        mod = module_of_tu(args[1])
        src = Path(f"src/{mod}/{args[1]}.c").read_text()
        args = re.findall(r"^\w[\w\s\*]*?\b(\w+)\([^;]*$", src, re.M)
    m = module(mod)
    built = Path(m["built"]).read_bytes()
    orig = Path(m["orig"]).read_bytes()
    orig_addr = symbol_addrs(m["syms"])
    funcs = functions(m["elf"])
    addrs = [a for _, a in funcs]
    word = lambda img, off: int.from_bytes(img[off : off + 4], "little")  # noqa: E731
    for name, a in funcs:
        if name not in args:
            continue
        i = addrs.index(a)
        end = addrs[i + 1] if i + 1 < len(addrs) else a + 4
        n = (end - a) // 4
        bo, oo = a - m["base"], orig_addr.get(name, a) - m["base"]
        diff = sum(1 for k in range(n) if word(built, bo + 4 * k) != word(orig, oo + 4 * k))
        mdiff = sum(1 for k in range(n) if mask(word(built, bo + 4 * k)) != mask(word(orig, oo + 4 * k)))
        status = "OK " if not diff else ("REL" if not mdiff else "BAD")
        print(f"{status} {name:40s} {a:08X} {n:4d} insns {diff} differ ({mdiff} ignoring relocs)")


if __name__ == "__main__":
    main()
