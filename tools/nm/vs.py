#!/usr/bin/env python3
"""vs.py src func variants.txt : like vt.py but strict score = masked diff + immediate mismatches
(immediates compared unless orig line has a reloc (%) or is j/jal)."""
import sys, os, re, difflib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from score import compile, nm_funcs, built_words, maskw

def orig(sfile):
    ins = []
    for l in open(sfile):
        m = re.match(r"\s*/\* \w+ \w+ ([0-9A-F]{8}) \*/\s+(.*)", l)
        if m:
            ins.append((m.group(1), '%' in m.group(2) or re.search(r'\b(j|jal)\b', m.group(2)) is not None))
    return ins

def strict(o, name, sfile):
    a = built_words(o, name)
    b = orig(sfile)
    if not a:
        return None
    am = [maskw(w) for w in a]
    bm = [maskw(w) for w, _ in b]
    sm = difflib.SequenceMatcher(None, am, bm, autojunk=False)
    same = 0
    imm = 0
    for bl in sm.get_matching_blocks():
        for k in range(bl.size):
            wa = a[bl.a + k]; wb, rel = b[bl.b + k]
            if wa != wb and not rel:
                imm += 1
            else:
                same += 1
        pass
    msk = max(len(a), len(b)) - sum(bl.size for bl in sm.get_matching_blocks())
    return msk, imm, len(b)

if __name__ == '__main__':
    src, name = sys.argv[1:3]
    vf = sys.argv[3] if len(sys.argv) > 3 else None
    d = dict((n, dd) for dd, n in nm_funcs(src))[name]
    base = open(src).read()
    vs = [""] + (open(vf).read().split("====\n") if vf else [])
    for i, v in enumerate(vs):
        t = base
        if v.strip():
            for part in v.split("~~~~\n"):
                old, new = part.split("----\n")
                if old not in t:
                    print(i, "OLD NOT FOUND"); t = None; break
                t = t.replace(old, new, 1)
            if t is None:
                continue
        o = compile(src, text=t)
        print(i, strict(o, name, f"{d}/{name}.s") if o else "FAIL", flush=True)
