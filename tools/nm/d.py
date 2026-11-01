#!/usr/bin/env python3
"""d.py src func : compile with NON_MATCHING, show only differing regions (ours | orig)."""
import sys, difflib
sys.path.insert(0, __import__("os").path.dirname(__file__))
from score import compile, nm_funcs, built_words, orig_insns, maskw, disasm_words

src, name = sys.argv[1], sys.argv[2]
ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 2
d = dict((n, dd) for dd, n in nm_funcs(src))[name]
o = compile(src)
a = built_words(o, name); b = orig_insns(f"{d}/{name}.s")
da, db = disasm_words(a), disasm_words(b)
sm = difflib.SequenceMatcher(None, [maskw(w) for w in a], [maskw(w) for w in b], autojunk=False)
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == "equal":
        # still show immediate diffs
        for k in range(i2 - i1):
            if a[i1 + k] != b[j1 + k] and (int.from_bytes(bytes.fromhex(a[i1+k]),"little")>>26) not in (2,3):
                print(f"~ {i1+k:4d} {da[i1+k]:40s} {db[j1+k]}")
        continue
    print(f"--- {tag} ours[{i1}:{i2}] orig[{j1}:{j2}]")
    for k in range(max(i1 - ctx, 0), i1):
        print(f"  {k:4d} {da[k]}")
    n = max(i2 - i1, j2 - j1)
    for k in range(n):
        l = da[i1 + k] if i1 + k < i2 else ""
        r = db[j1 + k] if j1 + k < j2 else ""
        print(f"! {i1+k:4d} {l:40s} {r}")
