#!/usr/bin/env python3
"""Expand INCLUDE_ASM for the original Psy-Q assembler (aspsx).

Under ccpsx (_PSYQ) include/common.h turns INCLUDE_ASM into marker statements:

    __asm__(".psyq_include 1 \"asm/.../NAME.s\"")                 file scope (rodata)
    void __psyq_include_asm_NAME() { __asm__(".psyq_include 2 ...") }   code, jump tables

so cc1psx puts each part exactly where the modern build's GAS includes land (see
tools/split_parts.py).  This filter sits between cc1psx and aspsx, like maspsx in the
modern build:

  * each marker is replaced by the matching INC_PART block of the splat .s, rewritten
    in aspsx syntax;
  * the dummy function around a part-2 marker (label, .ent/.frame/.end, `j $31`, its
    .globl) is removed, so only the included code remains;
  * after a part-1 block the section cc1 was in is restored.

Rewriting: any instruction without a relocation is emitted as `.word <original
encoding>` (taken from the splat comment), so aspsx cannot reinterpret it.  Only
instructions carrying %hi/%lo/%gp_rel operands and j/jal to labels stay symbolic,
with numeric registers.  `%gp_rel(sym)($gp)` becomes aspsx's macro form `op $r, sym`,
which aspsx assembles gp-relative only when sym is small data defined in this file;
otherwise it needs $at and fails under .set noat, so a wrong guess cannot slip through.

usage: psyq_asm.py [--root DIR] IN.s OUT.s      (OUT is written with CRLF for aspsx)
"""
import argparse
import os
import re
import sys

REGS = {"zero": 0, "at": 1, "v0": 2, "v1": 3, "a0": 4, "a1": 5, "a2": 6, "a3": 7,
        "t0": 8, "t1": 9, "t2": 10, "t3": 11, "t4": 12, "t5": 13, "t6": 14, "t7": 15,
        "s0": 16, "s1": 17, "s2": 18, "s3": 19, "s4": 20, "s5": 21, "s6": 22, "s7": 23,
        "t8": 24, "t9": 25, "k0": 26, "k1": 27, "gp": 28, "sp": 29, "fp": 30, "s8": 30,
        "ra": 31}
SECTIONS = {".rodata": ".rdata", ".rdata": ".rdata", ".text": ".text", ".data": ".data",
            ".sdata": ".sdata", ".bss": ".bss", ".sbss": ".sbss"}
HACK = "__psyq_include_asm_"
MARKER = re.compile(r'^\s*\.psyq_include\s+([12])\s+"([^"]+)"')
COMMENT = re.compile(r"^\s*/\*\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)(?:\s+([0-9A-Fa-f]+))?\s*\*/\s*(.*)$")
SECTION_DIR = re.compile(r"^\s*\.(text|data|rdata|sdata|sbss|bss)\s*$|^\s*\.section\s+(\S+)")


def reg(m):
    name = m.group(1)
    if name.isdigit():
        return "$" + name
    return "$%d" % REGS[name]


def symbolic(op, args):
    args = re.sub(r"\$(\w+)", reg, args)
    m = re.match(r"(\$\d+),\s*%gp_rel\((.+)\)\(\$28\)$", args)
    if m:
        sym = re.sub(r"\s+", "", m.group(2))
        return "\t%s\t%s, %s" % (op, m.group(1), sym)
    if "%gp_rel" in args:
        raise ValueError("unsupported gp_rel form: %s %s" % (op, args))
    args = re.sub(r"%(hi|lo)\(([^)]+)\)", lambda m: "%%%s(%s)" % (m.group(1), re.sub(r"\s+", "", m.group(2))), args)
    return "\t%s\t%s" % (op, args)


def le_words(hexbytes):
    b = bytes.fromhex(hexbytes)
    return ["0x%08X" % int.from_bytes(b[i:i + 4], "little") for i in range(0, len(b), 4)]


def gas_string(s):
    """Bytes of a GAS string literal body (between the quotes)."""
    out, i = bytearray(), 0
    esc = {"n": 10, "t": 9, "r": 13, "b": 8, "f": 12, "v": 11, "a": 7, "\\": 92, '"': 34, "'": 39}
    while i < len(s):
        c = s[i]
        if c != "\\":
            out += c.encode("latin-1")
            i += 1
            continue
        n = s[i + 1]
        if n in esc:
            out.append(esc[n])
            i += 2
        elif n in "01234567":
            j = i + 1
            while j < len(s) and j < i + 4 and s[j] in "01234567":
                j += 1
            out.append(int(s[i + 1:j], 8) & 0xFF)
            i = j
        elif n == "x":
            j = i + 2
            while j < len(s) and s[j] in "0123456789abcdefABCDEF":
                j += 1
            out.append(int(s[i + 2:j], 16) & 0xFF)
            i = j
        else:
            raise ValueError("unknown escape \\%s" % n)
    return bytes(out)


def bytes_lines(b):
    return ["\t.byte\t" + ",".join("0x%02X" % x for x in b[i:i + 16]) for i in range(0, len(b), 16)]


def convert_line(line, force=False):
    """aspsx lines for one line of a splat .s body.  force: emit an instruction
    symbolically even without a relocation."""
    s = line.strip()
    if not s or s.startswith("#"):
        return []
    m = COMMENT.match(s)
    if m:
        raw, body = m.group(3), m.group(4).strip()
        if not body:
            return []
        op, _, args = body.partition(" ")
        args = args.strip()
        if op == ".word":
            if raw and not re.search(r"[A-Za-z_.]\w*", re.sub(r"0x[0-9A-Fa-f]+", "", args)):
                return ["\t.word\t" + ",".join(le_words(raw))]
            return ["\t.word\t" + re.sub(r"\s+", "", args)]
        if op in (".asciz", ".ascii"):
            sm = re.match(r'^"(.*)"$', args)
            if not sm:
                raise ValueError("bad string: " + s)
            data = gas_string(sm.group(1)) + (b"\0" if op == ".asciz" else b"")
            return bytes_lines(data)
        if op in (".short", ".half", ".byte"):
            if raw:
                return bytes_lines(bytes.fromhex(raw))
            vals = [int(v, 0) for v in args.split(",")]
            w = 2 if op != ".byte" else 1
            return bytes_lines(b"".join(v.to_bytes(w, "little", signed=v < 0) for v in vals))
        if op.startswith("."):
            raise ValueError("unsupported data directive: " + s)
        # an instruction
        if not raw:
            raise ValueError("instruction without encoding: " + s)
        if force or "%" in args or (op in ("j", "jal") and not args.startswith("$")):
            return [symbolic(op, args)]
        return ["\t.word\t" + le_words(raw)[0]]
    if s.startswith("/*") and s.endswith("*/"):
        return []
    op, _, args = s.partition(" ")
    args = args.strip()
    if op in ("glabel", "dlabel", "alabel", "ehlabel"):
        name = args.split(",")[0].strip()
        return ["\t.globl\t" + name, name + ":"]
    if op == "jlabel":
        return [args.split(",")[0].strip() + ":"]
    if op in ("endlabel", "enddlabel", "nonmatching"):
        return []
    if re.match(r"^[\w.$]+:$", s):
        return [s]
    if op == ".section":
        name = args.split(",")[0].strip().strip('"')
        if name not in SECTIONS:
            raise ValueError("unsupported section: " + s)
        return ["\t" + SECTIONS[name]]
    if op in SECTIONS and not args:
        return ["\t" + SECTIONS[op]]
    if op == ".align":
        return ["\t.align\t" + args]
    if op in (".size", ".type", ".ent", ".end", ".include"):
        return []
    if op == ".set":
        return ["\t.set\t" + args]
    raise ValueError("unsupported line: " + s)


def inc_part(path, part):
    """aspsx lines for INC_PART == part of a split_parts-tagged splat file."""
    lines = open(path).read().splitlines()
    tagged = any(l.strip().startswith(".if INC_PART") for l in lines)
    out, active = [], not tagged or part == 2
    delay = False
    for l in lines:
        s = l.strip()
        m = re.match(r"\.if\s+INC_PART\s*==\s*(\d)", s)
        if m:
            active = int(m.group(1)) == part
            continue
        if s == ".endif":
            active = not tagged
            continue
        if active:
            try:
                conv = convert_line(l, force=delay)
                if conv and not conv[-1].endswith(":"):
                    # aspsx ignores .word when tracking delay slots: the instruction
                    # after a symbolic j/jal must be a real one, or the next symbolic
                    # jump is rejected as "not allowed in a delay slot"
                    delay = bool(re.match(r"\t(j|jal)\t", conv[-1]))
                out += conv
            except (ValueError, KeyError) as e:
                raise SystemExit("%s: %s" % (path, e))
    return out


def filter_cc1(text, root):
    out = []
    section = ".text"
    in_hack = None
    for line in text.splitlines():
        s = line.strip()
        m = MARKER.match(line)
        if m:
            part, path = int(m.group(1)), os.path.join(root, m.group(2))
            body = inc_part(path, part)
            if part == 1:
                if body:
                    out += ["\t.rdata"] + body + ["\t" + section]
            else:
                out += ["\t.text", "\t.set\tnoreorder", "\t.set\tnoat"] + body + [
                    "\t.text", "\t.set\tat", "\t.set\treorder"]
            continue
        if in_hack:
            if re.match(r"\.end\s+" + re.escape(in_hack) + "$", s):
                in_hack = None
            continue
        m = re.match(r"\.ent\s+(" + HACK + r"\w+)$", s)
        if m:
            in_hack = m.group(1)
            # the marker inside is handled when reached; skip the frame boilerplate
            continue
        if re.match(r"\.globl\s+" + HACK, s):
            continue
        m = SECTION_DIR.match(line)
        if m:
            section = "." + m.group(1) if m.group(1) else m.group(2)
        out.append(line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("inp")
    ap.add_argument("out")
    a = ap.parse_args()
    text = open(a.inp, newline="").read().replace("\r\n", "\n")
    lines = filter_cc1(text, a.root)
    with open(a.out, "w", newline="") as f:
        f.write("\r\n".join(lines) + "\r\n")


if __name__ == "__main__":
    main()
