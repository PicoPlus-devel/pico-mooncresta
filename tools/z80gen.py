#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Andre I. Holub
"""
Generates mooncresta/z80_ops.h, the instruction loop of the Z80 core, from
MAME's opcode list mooncresta/z80.lst.

Derived from MAME's src/devices/cpu/z80/z80make.py. The parser and the macro
expansion are MAME's; the output stage is new and emits plain C:

  - whole instructions run without interruption. MAME suspends an instruction
    in the middle when its cycle budget runs out and resumes it later (the
    m_ref step states); here the budget is only checked between instructions,
    so `N !! op` becomes `op; m_icount -= N;` and `+ N` becomes
    `m_icount -= N;`.
  - only the plain Z80 is emitted; the z80n, r800, nsc800 and t6a84 variants
    are skipped.
  - the C++ forms of the list are rewritten for mooncresta/z80.c: member
    access goes through `cpu->`, the service-attention templates become
    macros, and the daisy chain, debugger and logging hooks are dropped.

Usage: tools/z80gen.py mooncresta/z80.lst mooncresta/z80_ops.h
"""
import re
import sys


class IndStr:
    def __init__(self, src, indent=None):
        self.str = src.strip()
        self.indent = indent
        if not indent:
            self.indent = src[:len(src) - len(src.lstrip())]

    def line(self):
        return self.str

    def is_comment(self):
        return self.str.startswith("#") and not self.str.startswith("#if") and not self.str.startswith("#endif")

    def is_blank(self):
        return not self.str

    def has_indent(self):
        return self.indent != '' or self.str[0] == "{" or self.str[0] == "}"

    def replace(self, old, new):
        return IndStr(self.str.replace(old, new), self.indent)

    def with_str(self, new_str):
        return IndStr(new_str, self.indent)

    def split(self):
        return self.str.split()


class Opcode:
    def __init__(self, prefix, code, comment):
        self.prefix = prefix
        self.code = code
        self.comment = comment
        self.source = []

    def add_source_lines(self, lines):
        self.source.extend(lines)


class Macro:
    def __init__(self, name, arg_names=None):
        self.name = name
        self.source = []
        self.arg_names = arg_names

    def apply(self, args):
        if self.arg_names is not None:
            src = self.source
            for i, arg in enumerate(args.split(",")):
                src = [r.replace(self.arg_names[i], arg) for r in src]
            return src
        return self.source

    def add_source_lines(self, lines):
        self.source.extend(lines)


# Macros whose MAME bodies call into the device framework. The replacements
# keep the cycle counts and register effects.
OVERRIDES = {
    # no daisy chain: the vector is whatever the board puts on the bus
    "irqfetch": ["m_tmp_irq_vector = m_irq_vector;"],
}


class OpcodeList:
    def __init__(self, fname):
        self.opcode_info = {}  # prefix -> [Opcode]
        self.macros = {}

        with open(fname, "r") as f:
            lines = f.readlines()

        inf = None
        for ln in lines:
            line = IndStr(ln)
            if line.is_comment() or line.is_blank():
                continue
            if line.has_indent():
                if inf is not None:
                    if isinstance(inf, Macro):
                        inf.add_source_lines([line])
                    else:
                        inf.add_source_lines(self.pre_process(line))
                continue

            tokens = line.split()
            if tokens[0] == "macro":
                arg_names = tokens[2:] if len(tokens) > 2 else None
                nnames = tokens[1].split(":")
                inf = Macro(nnames[-1], arg_names)
                if len(nnames) == 2:
                    pass  # a variant's macro: parsed, never registered
                elif nnames[0] in OVERRIDES:
                    # register the replacement; MAME's body goes to the unregistered inf
                    m = Macro(nnames[0], arg_names)
                    m.add_source_lines([IndStr("\t" + s) for s in OVERRIDES[nnames[0]]])
                    self.macros[nnames[0]] = m
                else:
                    self.macros[nnames[0]] = inf
            else:
                ntokens = tokens[0].split(":")
                comment = line.str[len(tokens[0]):].strip().lstrip("#").strip()
                if len(ntokens) != 1:
                    inf = Opcode("", "/dev/null", "")  # a variant's opcode: dropped
                    continue
                prefix = ntokens[0][:2]
                opcode = ntokens[0][2:]
                inf = Opcode(prefix, opcode, comment)
                self.opcode_info.setdefault(prefix, []).append(inf)

    def pre_process(self, iline):
        out = []
        line = iline.str
        line_toc = line.split()
        times = 1
        if len(line_toc) > 2 and line_toc[1] == "*":
            times = int(line_toc[0])
            line_toc = line_toc[2:]
            line = " ".join(line_toc)
        for _ in range(times):
            if line_toc[0].startswith('@'):
                name = line_toc[0][1:]
                args = " ".join(line_toc[1:]) if len(line_toc) > 1 else None
                if name not in self.macros:
                    sys.exit("macro not found: %s" % name)
                for il in self.macros[name].apply(args):
                    out.extend(self.pre_process(il))
            else:
                out.append(iline.with_str(line))
        return out


# --- C output -----------------------------------------------------------------

# Lines dropped outright: logging, debugger and callbacks the boards here do
# not wire up. Each is a complete statement on its own line in z80.lst.
DROP = re.compile(r"^(LOGMASKED|logerror|debugger_\w+|m_irqack_cb|m_busack_cb|daisy_call_reti_device|using std::swap)\b"
                  r"|^m_(nomreq|refresh)_cb\(|^if \((nomreq|refresh)_en\)$")

SUBST = [
    (re.compile(r"set_service_attention<(\w+), (\d)>\(\)"), r"SET_SA(\1, \2)"),
    (re.compile(r"get_service_attention<(\w+)>\(\)"), r"GET_SA(\1)"),
    (re.compile(r"m_f\.(s|z|yx|h|pv)\(\)"), lambda m: "F_" + m.group(1).upper() + "()"),
    (re.compile(r"m_io\.read_interruptible\("), "io_read("),
    (re.compile(r"m_io\.write_interruptible\("), "io_write("),
    (re.compile(r"\bu(8|16|32)\("), r"(u\1)("),
    (re.compile(r"\bm_m1_cycles\b"), "Z80_M1_CYCLES"),
    (re.compile(r"\bm_mreq_cycles\b"), "Z80_MREQ_CYCLES"),
    (re.compile(r"\bm_iorq_cycles\b"), "Z80_IORQ_CYCLES"),
    (re.compile(r"\bm_ref\b"), "ref"),
    (re.compile(r"\bm_(\w+)"), r"cpu->\1"),
    (re.compile(r"\bCLEAR_LINE\b"), "0"),
    (re.compile(r"\bnullptr\b"), "NULL"),
    (re.compile(r"\bfalse\b"), "0"),
    (re.compile(r"\btrue\b"), "1"),
]


def c_line(s):
    for rx, rep in SUBST:
        s = rx.sub(rep, s)
    return s


def emit_body(op, indent, f):
    for il in op.source:
        tokens = il.split()
        if DROP.match(il.str):
            continue
        pad = indent + il.indent
        if tokens[0] == '+':
            print("%scpu->icount -= (%s);" % (pad, c_line(" ".join(tokens[1:]))), file=f)
        elif len(tokens) > 2 and tokens[1] == "!!":
            print("%s%s" % (pad, c_line(" ".join(tokens[2:]))), file=f)
            print("%scpu->icount -= (%s);" % (pad, c_line(tokens[0])), file=f)
        else:
            print("%s%s" % (pad, c_line(il.str)), file=f)


def save(ops, f):
    print("/* Generated by tools/z80gen.py from mooncresta/z80.lst - do not edit. */", file=f)
    print("/* license:BSD-3-Clause, see mooncresta/LICENSE */", file=f)
    print("", file=f)
    print("for (;;)", file=f)
    print("{", file=f)
    print("\t/* ffff: instruction boundary - budget, interrupts, halt, opcode fetch */", file=f)
    emit_body(ops.opcode_info['ff'][0], "", f)
    print("", file=f)
    print("process:", file=f)
    print("\tswitch ((ref >> 16) & 0xff) /* prefix */", file=f)
    print("\t{", file=f)
    for prefix in sorted(ops.opcode_info.keys()):
        if prefix == 'ff':
            continue
        print("\tcase 0x%s:" % prefix, file=f)
        print("\t\tswitch ((ref >> 8) & 0xff) /* opcode */", file=f)
        print("\t\t{", file=f)
        for op in ops.opcode_info[prefix]:
            print("\t\tcase 0x%s: /* %s */" % (op.code, op.comment), file=f)
            emit_body(op, "\t\t", f)
            print("\t\t\tcontinue;", file=f)
        print("\t\t}", file=f)
        print("\t\tbreak;", file=f)
    print("\t}", file=f)
    print("}", file=f)


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    ops = OpcodeList(argv[1])
    for prefix in ('00', 'cb', 'dd', 'ed', 'fd', 'fe'):
        n = len(ops.opcode_info.get(prefix, []))
        if n != 256:
            sys.exit("prefix %s: %d opcodes, expected 256" % (prefix, n))
    with open(argv[2], "w") as f:
        save(ops, f)


if __name__ == "__main__":
    main(sys.argv)
