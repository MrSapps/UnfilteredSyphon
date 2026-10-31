#!/usr/bin/env python3
"""Compare two SN OBJ files, ignoring the source file records (aspsx writes the
absolute path of the source, which depends on the directory it ran in).

usage: psyq_objeq.py A.obj B.obj [C.obj D.obj ...]     exit 0 = all pairs equal
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psyq_obj import read_lib  # noqa: E402


def key(path):
    o = read_lib(path)[0].obj
    secs = sorted((s.id, s.group, s.align, s.name, bytes(s.data), s.bss, repr(s.relocs))
                  for s in o.sections.values())
    return (secs, sorted(o.xdefs), sorted(o.xrefs.items()), sorted(o.locals),
            sorted(o.commons), o.cpu)


def main():
    a = sys.argv[1:]
    if not a or len(a) % 2:
        sys.exit(__doc__)
    bad = 0
    for x, y in zip(a[::2], a[1::2]):
        if key(x) != key(y):
            print("DIFF %s %s" % (x, y))
            bad = 1
    sys.exit(bad)


if __name__ == "__main__":
    main()
