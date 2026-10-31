#!/usr/bin/env python3
"""Wrap a C function in #ifdef NON_MATCHING ... #else INCLUDE_ASM ... #endif.

    tools/nonmatch.py src/main/pickup.c UseObject "regalloc: a3 vs s2 for the call arg"

The function must start at column 0 and end with a '}' line at column 0.
"""
import re
import sys
from pathlib import Path


def main():
    path, name = Path(sys.argv[1]), sys.argv[2]
    reason = sys.argv[3] if len(sys.argv) > 3 else ""
    text = path.read_text()
    asm_dir = re.search(r'INCLUDE_ASM\("([^"]+)"', text)
    if asm_dir:
        asm_dir = asm_dir.group(1)
    else:
        mod, tu = path.parent.name, path.stem
        asm_dir = f"asm/{mod}/nonmatchings/{tu}"
    m = re.search(rf"^(?:static )?[A-Za-z_][^\n;]*\b{name}\([^;{{]*\)\s*{{", text, re.M)
    if not m:
        sys.exit(f"{name}: definition not found")
    start = m.start()
    # include an immediately preceding comment block
    before = text[:start]
    cm = re.search(r"(/\*(?:(?!\*/).)*\*/\n)$", before, re.S)
    if cm:
        start = cm.start()
    end = text.index("\n}\n", m.end()) + 3
    body = text[start:end]
    if "#ifdef NON_MATCHING" in text[max(0, start - 40) : start]:
        sys.exit(f"{name}: already NON_MATCHING")
    note = f"/* {reason} */\n" if reason else ""
    wrapped = f"#ifdef NON_MATCHING\n{note}{body}#else\nINCLUDE_ASM(\"{asm_dir}\", {name});\n#endif\n"
    path.write_text(text[:start] + wrapped + text[end:])


if __name__ == "__main__":
    main()
