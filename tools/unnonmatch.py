#!/usr/bin/env python3
"""Undo tools/nonmatch.py: drop the NON_MATCHING wrapper around a function.

    tools/unnonmatch.py src/movie/movie.c MoviePause
"""
import re
import sys
from pathlib import Path


def main():
    path, name = Path(sys.argv[1]), sys.argv[2]
    s = path.read_text()
    tail = re.search(r'#else\nINCLUDE_ASM\("[^"]+", %s\);\n#endif\n' % name, s)
    if not tail:
        sys.exit(f"{name}: no NON_MATCHING block")
    start = s.rindex("#ifdef NON_MATCHING\n", 0, tail.start())
    body = s[start + len("#ifdef NON_MATCHING\n"):tail.start()]
    body = re.sub(r"^/\*[^\n]*\*/\n", "", body, count=1)  # the reason comment
    path.write_text(s[:start] + body + s[tail.end():])


if __name__ == "__main__":
    main()
