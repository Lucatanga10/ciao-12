#!/usr/bin/env python3
"""bss_xref_scan.py

Count ARM64 code references to every slot of an ELF shared object's .bss,
straight from the file on disk. No Ghidra, no project, no analysis pass.

Two reference classes are counted separately:

  direct    ADRP xN, page ; ADD/LDR/STR ... [xN, #imm]   -> address lands in .bss
  indirect  ADRP xN, page ; LDR xM, [xN, #imm]           -> address lands on a
            relocated pointer slot (.got / .data.rel.ro) whose relocation
            addend lands in .bss

Engine globals (GEngine, GWorld, GObjects, GNames) are read by thousands of
distinct functions; an ordinary file-static global is read by two or three.
Sorting by total reference count floats the engine globals to the top.

Usage:
  python bss_xref_scan.py <path-to-.so> [--top N] [--window N] [--min N]
                          [--section NAME] [--out FILE]
  python bss_xref_scan.py <path-to-.so> --pcs 0x19ea1234 [--max-pcs N]

The first form prints the ranked list. The second re-scans and prints the
code addresses (RVA) that reference one specific slot, for spot checking in a
disassembler.
"""

import mmap
import os
import struct
import sys
import time

# ---------------------------------------------------------------------------
# ELF
# ---------------------------------------------------------------------------

ELFCLASS64 = 2
ELFDATA2LSB = 1
EM_AARCH64 = 183

SHT_RELA = 4

R_AARCH64_ABS64 = 257
R_AARCH64_GLOB_DAT = 1025
R_AARCH64_RELATIVE = 1027


class Section(object):
    __slots__ = ("name", "type", "flags", "addr", "offset", "size",
                 "link", "info", "align", "entsize")

    def __init__(self, name, fields):
        (self.type, self.flags, self.addr, self.offset, self.size,
         self.link, self.info, self.align, self.entsize) = fields
        self.name = name

    def __repr__(self):
        return "<Section %s addr=0x%x size=0x%x>" % (
            self.name, self.addr, self.size)


class Elf(object):
    def __init__(self, path):
        self.path = path
        self.fh = open(path, "rb")
        self.mm = mmap.mmap(self.fh.fileno(), 0, access=mmap.ACCESS_READ)
        self.sections = []
        self._parse()

    def close(self):
        self.mm.close()
        self.fh.close()

    def _parse(self):
        mm = self.mm
        if mm[:4] != b"\x7fELF":
            raise ValueError("not an ELF file: %s" % self.path)
        if mm[4] != ELFCLASS64:
            raise ValueError("not ELF64")
        if mm[5] != ELFDATA2LSB:
            raise ValueError("not little endian")
        (e_machine,) = struct.unpack_from("<H", mm, 18)
        if e_machine != EM_AARCH64:
            raise ValueError("not AArch64 (e_machine=%d)" % e_machine)
        self.machine = e_machine

        (e_shoff,) = struct.unpack_from("<Q", mm, 40)
        (e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from("<HHH", mm, 58)
        if e_shoff == 0 or e_shnum == 0:
            raise ValueError("no section headers; this scanner needs them")

        raw = []
        for i in range(e_shnum):
            base = e_shoff + i * e_shentsize
            sh_name, = struct.unpack_from("<I", mm, base)
            fields = struct.unpack_from("<IQQQQIIQQ", mm, base + 4)
            raw.append((sh_name, fields))

        strtab_off = raw[e_shstrndx][1][3]
        strtab_size = raw[e_shstrndx][1][4]
        strtab = bytes(mm[strtab_off:strtab_off + strtab_size])

        for sh_name, fields in raw:
            end = strtab.find(b"\x00", sh_name)
            name = strtab[sh_name:end].decode("utf-8", "replace")
            self.sections.append(Section(name, fields))

    def section(self, name):
        for s in self.sections:
            if s.name == name:
                return s
        return None

    def build_id(self):
        s = self.section(".note.gnu.build-id")
        if s is None or s.size == 0:
            return None
        blob = bytes(self.mm[s.offset:s.offset + s.size])
        if len(blob) < 12:
            return None
        namesz, descsz, ntype = struct.unpack_from("<III", blob, 0)
        name_start = 12
        name_pad = (namesz + 3) & ~3
        desc_start = name_start + name_pad
        desc = blob[desc_start:desc_start + descsz]
        return desc.hex()


# ---------------------------------------------------------------------------
# relocations: pointer slot VA -> pointee VA
# ---------------------------------------------------------------------------

def build_pointer_map(elf, lo, hi):
    """Map every relocated pointer slot whose value lands in [lo, hi) to that
    value. Covers R_AARCH64_RELATIVE (addend holds the VA) and the symbolic
    forms GLOB_DAT / ABS64 (value from .dynsym st_value, addend added)."""
    mm = elf.mm
    out = {}

    dynsym = elf.section(".dynsym")
    symvals = []
    if dynsym is not None and dynsym.size:
        n = dynsym.size // 24
        for i in range(n):
            st_value, = struct.unpack_from("<Q", mm, dynsym.offset + i * 24 + 8)
            symvals.append(st_value)

    for s in elf.sections:
        if s.type != SHT_RELA or s.size == 0:
            continue
        n = s.size // 24
        for i in range(n):
            r_offset, r_info, r_addend = struct.unpack_from(
                "<QQq", mm, s.offset + i * 24)
            rtype = r_info & 0xFFFFFFFF
            if rtype == R_AARCH64_RELATIVE:
                target = r_addend
            elif rtype in (R_AARCH64_GLOB_DAT, R_AARCH64_ABS64):
                symidx = r_info >> 32
                if symidx >= len(symvals):
                    continue
                base = symvals[symidx]
                if base == 0:
                    continue
                target = base + r_addend
            else:
                continue
            if lo <= target < hi:
                out[r_offset] = target
    return out


# ---------------------------------------------------------------------------
# AArch64 instruction scan
# ---------------------------------------------------------------------------

# mask 0xFFC00000 -> scale, clobbers_int_rt
LDST_UIMM = {
    0x39000000: (1, False),   # STRB  Wt, [Xn, #imm]
    0x39400000: (1, True),    # LDRB  Wt, [Xn, #imm]
    0x79000000: (2, False),   # STRH  Wt, [Xn, #imm]
    0x79400000: (2, True),    # LDRH  Wt, [Xn, #imm]
    0xB9000000: (4, False),   # STR   Wt, [Xn, #imm]
    0xB9400000: (4, True),    # LDR   Wt, [Xn, #imm]
    0xF9000000: (8, False),   # STR   Xt, [Xn, #imm]
    0xF9400000: (8, True),    # LDR   Xt, [Xn, #imm]
    0xBD000000: (4, False),   # STR   St, [Xn, #imm]
    0xBD400000: (4, False),   # LDR   St, [Xn, #imm]
    0xFD000000: (8, False),   # STR   Dt, [Xn, #imm]
    0xFD400000: (8, False),   # LDR   Dt, [Xn, #imm]
    0x3D800000: (16, False),  # STR   Qt, [Xn, #imm]
    0x3DC00000: (16, False),  # LDR   Qt, [Xn, #imm]
}

RET_INSN = 0xD65F03C0


def scan(elf, text, bss_lo, bss_hi, ptrmap, window, want_pcs=None,
         max_pcs=64, progress=None):
    """Single linear pass over .text.

    Returns (direct, indirect, stats) where direct/indirect are dicts keyed by
    target VA. When want_pcs is set, returns the referencing PCs for that one
    target instead of the full tables.
    """
    mm = elf.mm
    text_va = text.addr
    nbytes = text.size & ~3
    view = memoryview(mm)[text.offset:text.offset + nbytes]
    words = view.cast("I")
    total = len(words)

    direct = {}
    indirect = {}
    pcs = []

    # reg -> adrp page, reg -> instruction index where the page was formed
    page = [0] * 32
    page_i = [-1 << 30] * 32

    adrp_count = 0
    pair_count = 0
    report_every = 4 * 1024 * 1024

    for i in range(total):
        w = words[i]

        if w & 0x9F000000 == 0x90000000:        # ADRP Xd, #imm
            rd = w & 31
            imm = (((w >> 5) & 0x7FFFF) << 2) | ((w >> 29) & 3)
            if imm & 0x100000:
                imm -= 0x200000
            page[rd] = (((text_va + (i << 2)) & ~0xFFF) + (imm << 12)) & 0xFFFFFFFFFFFFFFFF
            page_i[rd] = i
            adrp_count += 1
            if progress is not None and (i & (report_every - 1)) == 0:
                progress(i, total)
            continue

        if w & 0xFF800000 == 0x91000000:        # ADD Xd, Xn, #imm (LSL 0)
            rn = (w >> 5) & 31
            if i - page_i[rn] <= window:
                tgt = page[rn] + ((w >> 10) & 0xFFF)
                pair_count += 1
                if bss_lo <= tgt < bss_hi:
                    if want_pcs is None:
                        direct[tgt] = direct.get(tgt, 0) + 1
                    elif tgt == want_pcs and len(pcs) < max_pcs:
                        pcs.append((text_va + (i << 2), "add"))
                else:
                    ind = ptrmap.get(tgt)
                    if ind is not None:
                        if want_pcs is None:
                            indirect[ind] = indirect.get(ind, 0) + 1
                        elif ind == want_pcs and len(pcs) < max_pcs:
                            pcs.append((text_va + (i << 2), "add->ptr"))
            rd = w & 31
            if rd != rn:
                page_i[rd] = -1 << 30
            continue

        ldst = LDST_UIMM.get(w & 0xFFC00000)
        if ldst is not None:
            scale, clobbers = ldst
            rn = (w >> 5) & 31
            if i - page_i[rn] <= window:
                tgt = page[rn] + ((w >> 10) & 0xFFF) * scale
                pair_count += 1
                if bss_lo <= tgt < bss_hi:
                    if want_pcs is None:
                        direct[tgt] = direct.get(tgt, 0) + 1
                    elif tgt == want_pcs and len(pcs) < max_pcs:
                        pcs.append((text_va + (i << 2), "ldst"))
                else:
                    ind = ptrmap.get(tgt)
                    if ind is not None:
                        if want_pcs is None:
                            indirect[ind] = indirect.get(ind, 0) + 1
                        elif ind == want_pcs and len(pcs) < max_pcs:
                            pcs.append((text_va + (i << 2), "ldst->ptr"))
            if clobbers:
                rt = w & 31
                if rt != rn:
                    page_i[rt] = -1 << 30
            continue

        if w == RET_INSN or w & 0xFC000000 == 0x14000000:
            # RET, or unconditional B: next instruction is a different block
            for r in range(32):
                page_i[r] = -1 << 30
            continue

    if progress is not None:
        progress(total, total)

    stats = {"instructions": total, "adrp": adrp_count, "pairs": pair_count}
    if want_pcs is not None:
        return pcs, stats
    return direct, indirect, stats


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------

def parse_args(argv):
    opts = {
        "path": None,
        "top": 48,
        "window": 8,
        "min": 1,
        "section": ".bss",
        "out": None,
        "pcs": None,
        "max_pcs": 64,
    }
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--top":
            opts["top"] = int(argv[i + 1]); i += 2
        elif a == "--window":
            opts["window"] = int(argv[i + 1]); i += 2
        elif a == "--min":
            opts["min"] = int(argv[i + 1]); i += 2
        elif a == "--section":
            opts["section"] = argv[i + 1]; i += 2
        elif a == "--out":
            opts["out"] = argv[i + 1]; i += 2
        elif a == "--pcs":
            opts["pcs"] = int(argv[i + 1], 0); i += 2
        elif a == "--max-pcs":
            opts["max_pcs"] = int(argv[i + 1]); i += 2
        elif a in ("-h", "--help"):
            sys.stdout.write(__doc__)
            raise SystemExit(0)
        elif opts["path"] is None:
            opts["path"] = a; i += 1
        else:
            raise SystemExit("unexpected argument: %s" % a)
    if opts["path"] is None:
        sys.stdout.write(__doc__)
        raise SystemExit(2)
    return opts


def progress_printer(start):
    def report(i, total):
        pct = 100.0 * i / total if total else 100.0
        el = time.time() - start
        sys.stderr.write("\r  scan %6.2f%%  %7.1fs" % (pct, el))
        sys.stderr.flush()
    return report


def main(argv):
    opts = parse_args(argv)
    elf = Elf(opts["path"])
    try:
        text = elf.section(".text")
        bss = elf.section(opts["section"])
        if text is None:
            raise SystemExit("no .text section")
        if bss is None:
            raise SystemExit("no %s section" % opts["section"])

        bss_lo = bss.addr
        bss_hi = bss.addr + bss.size

        print("module      %s" % os.path.abspath(opts["path"]))
        print("size        %d byte" % os.path.getsize(opts["path"]))
        bid = elf.build_id()
        print("build-id    %s" % (bid if bid else "(none)"))
        print(".text       va 0x%08x  size 0x%x  file 0x%x"
              % (text.addr, text.size, text.offset))
        print("%-11s va 0x%08x  size 0x%x"
              % (opts["section"], bss.addr, bss.size))

        t0 = time.time()
        ptrmap = build_pointer_map(elf, bss_lo, bss_hi)
        print("ptr slots   %d relocated pointers into %s  (%.1fs)"
              % (len(ptrmap), opts["section"], time.time() - t0))

        t1 = time.time()
        if opts["pcs"] is not None:
            pcs, stats = scan(elf, text, bss_lo, bss_hi, ptrmap,
                              opts["window"], want_pcs=opts["pcs"],
                              max_pcs=opts["max_pcs"],
                              progress=progress_printer(t1))
            sys.stderr.write("\n")
            print("instructions %d  adrp %d  pairs %d  (%.1fs)"
                  % (stats["instructions"], stats["adrp"], stats["pairs"],
                     time.time() - t1))
            print("")
            print("referencing code addresses for 0x%08x" % opts["pcs"])
            if not pcs:
                print("  none")
            for pc, kind in pcs:
                print("  0x%08x  %s" % (pc, kind))
            return 0

        direct, indirect, stats = scan(elf, text, bss_lo, bss_hi, ptrmap,
                                       opts["window"],
                                       progress=progress_printer(t1))
        sys.stderr.write("\n")
        print("instructions %d  adrp %d  pairs %d  (%.1fs)"
              % (stats["instructions"], stats["adrp"], stats["pairs"],
                 time.time() - t1))

        keys = set(direct)
        keys.update(indirect)
        rows = []
        for k in keys:
            d = direct.get(k, 0)
            n = indirect.get(k, 0)
            t = d + n
            if t >= opts["min"]:
                rows.append((t, d, n, k))
        rows.sort(key=lambda r: (-r[0], r[3]))

        print("slots       %d distinct addresses referenced in %s"
              % (len(rows), opts["section"]))
        print("")
        header = "%-12s %8s %8s %8s" % ("rva", "xref", "direct", "viaptr")
        print(header)
        print("-" * len(header))
        shown = rows[:opts["top"]] if opts["top"] > 0 else rows
        for t, d, n, k in shown:
            print("0x%08x   %8d %8d %8d" % (k, t, d, n))

        if opts["out"]:
            with open(opts["out"], "w", encoding="utf-8") as fh:
                fh.write("# %s\n" % os.path.abspath(opts["path"]))
                fh.write("# build-id %s\n" % (bid if bid else "(none)"))
                fh.write("# section %s va 0x%08x size 0x%x\n"
                         % (opts["section"], bss.addr, bss.size))
                fh.write("# rva  xref  direct  viaptr\n")
                for t, d, n, k in rows:
                    fh.write("0x%08x   xref %d   direct %d   viaptr %d\n"
                             % (k, t, d, n))
            print("")
            print("wrote %s  (%d rows)" % (os.path.abspath(opts["out"]),
                                           len(rows)))
        return 0
    finally:
        elf.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
