#!/usr/bin/env python3
"""vt.py src func variants.txt : variants separated by lines '====', each 'old\n----\nnew'.
Applies each (independently) to a temp copy and prints its score; src is never modified. Variant 0 = baseline."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from score import compile, nm_funcs, score

src, name, vf = sys.argv[1:4]
d = dict((n, dd) for dd, n in nm_funcs(src))[name]
orig = open(src).read()
vs = [""] + open(vf).read().split("====\n")
for i, v in enumerate(vs):
    t = orig
    if v.strip():
        for part in v.split("~~~~\n"):
            old, new = part.split("----\n")
            if old not in t:
                print(i, "OLD NOT FOUND"); t = None; break
            t = t.replace(old, new, 1)
        if t is None:
            continue
    o = compile(src, text=t)
    print(i, score(o, name, f"{d}/{name}.s") if o else "FAIL", flush=True)
