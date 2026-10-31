#!/usr/bin/env python3
"""Replace INCLUDE_ASM lines with C definitions.

    tools/put_c.py src/main/pickup.c funcs.c

funcs.c holds one or more top-level C function definitions (each starting at
column 0 and ending with a column-0 '}'), optionally preceded by a comment.
Each replaces the INCLUDE_ASM line for the function of the same name.
"""
import re
import sys
from pathlib import Path


def split_defs(text):
    defs = {}
    for m in re.finditer(r"((?:/\*(?:(?!\*/).)*\*/\n)?(?:static )?[A-Za-z_][^\n;{]*?\b(\w+)\([^;{]*\)\s*\{.*?\n\}\n)", text, re.S):
        defs[m.group(2)] = m.group(1)
    return defs


def main():
    path = Path(sys.argv[1])
    src = path.read_text()
    for name, body in split_defs(Path(sys.argv[2]).read_text()).items():
        pat = re.compile(rf'^INCLUDE_ASM\("[^"]+", {name}\);\n', re.M)
        if not pat.search(src):
            print(f"{name}: no INCLUDE_ASM line, skipped")
            continue
        src = pat.sub(lambda _: body, src, count=1)
        print(f"{name}: replaced")
    path.write_text(src)


if __name__ == "__main__":
    main()
