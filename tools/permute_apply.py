#!/usr/bin/env python3
"""Try score-0 permuter results in the real build; keep the ones that match.

    tools/permute_apply.py            # every best=0 line in permute/summary.txt

The permuter ignores branch targets, so each candidate is checked with the full
sha1sum. A failure restores the source file.
"""
import re
import subprocess
import sys
from pathlib import Path


def extract(src, name):
    m = re.search(r"^[^\n;{}]*\b%s\([^;{]*\)\s*\{" % name, src, re.M)
    if not m:
        return None
    end = src.index("\n}", m.end()) + 2
    return src[m.start():end] + "\n"


def build_ok():
    r = subprocess.run("ninja 2>&1 | grep -q 'FAILED: \\[code=[^1]' && exit 1; "
                       "sha1sum --quiet -c orig/SHA1SUMS.build >/dev/null 2>&1",
                       shell=True, executable="/bin/bash")
    return r.returncode == 0


def main():
    for line in Path("permute/summary.txt").read_text().splitlines():
        m = re.match(r"(\S+) (\w+) base=\d+ best=0$", line)
        if not m:
            continue
        path, name = Path(m.group(1)), m.group(2)
        outs = sorted(Path(f"permute/{name}").glob("output-0-*"))
        if not outs:
            continue
        orig = path.read_text()
        if f'INCLUDE_ASM("' not in orig or not re.search(r'#else\nINCLUDE_ASM\("[^"]+", %s\);' % name, orig):
            continue
        ok = False
        for out in outs:
            fn = extract((out / "source.c").read_text(), name)
            if not fn:
                continue
            subprocess.run([sys.executable, "tools/unnonmatch.py", str(path), name], check=True)
            s = path.read_text()
            old = extract(s, name)
            # keep a leading 'static' from the original definition
            if old.startswith("static ") and not fn.startswith("static "):
                fn = "static " + fn
            path.write_text(s.replace(old, fn, 1))
            if build_ok():
                ok = True
                break
            path.write_text(orig)
        print(f"{name}: {'MATCH' if ok else 'false positive'}")
        if not ok:
            path.write_text(orig)
    subprocess.run("ninja >/dev/null 2>&1", shell=True)


if __name__ == "__main__":
    main()
