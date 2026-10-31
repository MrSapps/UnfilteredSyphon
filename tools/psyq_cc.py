#!/usr/bin/env python3
"""Compile a C file with the original Psy-Q toolchain, expanding INCLUDE_ASM.

    psyq_cc.py [ccpsx flags...] [--srcname s/foo.c] [--depfile F.d] [--keep-s DIR]
               -c FILE.c -oFILE.obj

runs what `ccpsx -c` would, with tools/psyq_asm.py spliced in between the compiler
and the assembler:

    ccpsx -E (cpppsx)  ->  cc1psx  ->  psyq_asm.py  ->  aspsx -q -G<n>

`ccpsx -S` itself runs `cpppsx ... FILE.c C:/users/<user>/Temp/PQ2` and then
`cc1psx -quiet <-O/-G/-f/-m/-g flags> PQ2 -o OUT.s` (seen with -v), and ccpsx -c then
runs `aspsx -q -G<n> in -o out`.  The PQ2 temp file has a fixed name inside the wine
prefix, so two ccpsx running at once overwrite each other's preprocessed source (and
wine ignores TMP/TEMP from the environment).  So the two stages run separately here:
`ccpsx -E -o x.i` writes the preprocessor output straight to a private file, and
cc1psx is called the way ccpsx calls it.  For a file without INCLUDE_ASM the object
is identical to plain `ccpsx -c`.

--srcname NAME  compile the file under this relative name (__FILE__, the OBJ's file
                record): a private temp dir gets a symlink <dirname NAME> -> the
                source's directory.  The original's names are "s/<file>.c".
--depfile F     write a make-style depfile (taken from cpppsx's line markers).
--keep-s DIR    keep the intermediate files in DIR (default: OUT minus .obj plus
                .work, removed afterwards).

The tools run through tools/psyq_wine.sh (PSYQ_WINEPREFIX, PSYQ_SDK apply).  Each call
works in its own temp dir, so parallel builds are safe.
"""
import os
import re
import shutil
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import psyq_asm  # noqa: E402

# ccpsx flags that go to cc1psx (everything else is preprocessor or driver business)
CC1_FLAG = re.compile(r"-(O\d?|G\d+|f\S+|m\S+|g\S*|W\S*|w|ansi|pedantic\S*|traditional)$")


def wine(args, cwd, tmp=None):
    env = dict(os.environ)
    if tmp:
        # cc1psx makes temp files (cta*) under TMPDIR; ccpsx would set it, so a direct
        # call needs one, and a private one keeps parallel builds apart
        env["TMPDIR"] = "Z:" + tmp.replace("/", "\\")
    r = subprocess.run([os.path.join(TOOLS, "psyq_wine.sh")] + args, cwd=cwd, env=env,
                       capture_output=True, text=True, errors="replace")
    return r.returncode, r.stdout + r.stderr


def depfile(ifile, cwd, out, path):
    """Make-style depfile from the `# N "file"` line markers of cpppsx output."""
    deps = []
    for l in open(ifile, errors="replace"):
        m = re.match(r'# \d+ "([^"]+)"', l)
        if not m:
            continue
        f = m.group(1).replace("\\", "/")
        if re.match(r"[A-Za-z]:", f):
            if not f[:2].upper() == "Z:":
                continue
            f = f[2:]
        f = os.path.realpath(os.path.join(cwd, f))
        if os.path.exists(f):
            f = os.path.relpath(f)
            if f not in deps:
                deps.append(f)
    with open(path, "w") as fp:
        fp.write("%s: %s\n" % (os.path.relpath(out), " \\\n  ".join(d.replace(" ", "\\ ") for d in deps)))


def main():
    argv = sys.argv[1:]
    opts = {}
    for k in ("--keep-s", "--srcname", "--depfile"):
        if k in argv:
            i = argv.index(k)
            opts[k] = argv[i + 1]
            del argv[i:i + 2]
    out, src, flags = None, None, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o":
            out = argv[i + 1]
            i += 2
            continue
        if a.startswith("-o"):
            out = a[2:]
        elif a == "-c":
            pass
        elif a.endswith(".c") and not a.startswith("-"):
            src = a
        else:
            flags.append(a)
        i += 1
    if not out or not src:
        sys.exit(__doc__)
    out = os.path.abspath(out)
    # include paths become absolute: the tools run in a temp dir
    flags = ["-I" + os.path.abspath(f[2:]) if f.startswith("-I") and len(f) > 2 else f for f in flags]
    g = [f for f in flags if re.match(r"-G\d+$", f)]
    cc1_flags = [f for f in flags if CC1_FLAG.match(f)]
    keep = opts.get("--keep-s")
    # a fixed work dir next to the output (not a random one): aspsx records the
    # source's absolute path in the OBJ, so this keeps the OBJ reproducible
    tmp = os.path.abspath(keep) if keep else os.path.splitext(out)[0] + ".work"
    if not keep:
        shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    if "--srcname" in opts:
        name = opts["--srcname"]
        d = os.path.dirname(name)
        link = os.path.join(tmp, d)
        if os.path.lexists(link):
            os.unlink(link)
        os.makedirs(os.path.dirname(link), exist_ok=True)
        os.symlink(os.path.dirname(os.path.abspath(src)), link)
        cwd, srcarg = tmp, name
    else:
        cwd, srcarg = os.getcwd(), src
    ifile = os.path.join(tmp, base + ".i")
    cc1_s = os.path.join(tmp, base + ".cc1.s")
    as_s = os.path.join(tmp, base + ".s")
    for f in (ifile, cc1_s, out):
        if os.path.exists(f):
            os.unlink(f)
    try:
        rc, log = wine(["ccpsx"] + flags + ["-E", srcarg, "-o" + ifile], cwd)
        if rc or not os.path.exists(ifile):
            sys.stderr.write(log)
            sys.exit(1)
        rc, log1 = wine(["cc1psx", "-quiet"] + cc1_flags + [ifile, "-o", cc1_s], cwd, tmp)
        if rc or not os.path.exists(cc1_s):
            sys.stderr.write(log + log1)
            sys.exit(1)
        text = open(cc1_s, newline="").read().replace("\r\n", "\n")
        lines = psyq_asm.filter_cc1(text, ROOT)
        with open(as_s, "w", newline="") as f:
            f.write("\r\n".join(lines) + "\r\n")
        rc, log2 = wine(["aspsx", "-q"] + g + [as_s, "-o", out], cwd)
        if not os.path.exists(out) or rc:
            sys.stderr.write(log + log1 + log2)
            if os.path.exists(out):
                os.unlink(out)
            sys.exit(1)
        if "--depfile" in opts:
            depfile(ifile, cwd, out, opts["--depfile"])
    finally:
        if not keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
