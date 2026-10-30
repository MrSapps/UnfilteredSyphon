#!/usr/bin/env python3
"""Show what the permuter changed: base.c vs output-0-* for one function.

    tools/permute_diff.py iKeyRestore
"""
import difflib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from permute_apply import extract  # noqa: E402

name = sys.argv[1]
d = Path(f"permute/{name}")
out = sorted(d.glob("output-0-*"))[0]
a = extract((d / "base.c").read_text(), name).splitlines()
b = extract((out / "source.c").read_text(), name).splitlines()
sys.stdout.writelines(l + "\n" for l in difflib.unified_diff(a, b, "base", "permuted", lineterm="", n=1))
