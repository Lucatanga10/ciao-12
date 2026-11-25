# Count references to every 8-aligned address in .bss and write them sorted.
# Cross-check for tools/bss_xref_scan.py: this one uses Ghidra's own
# reference database, so it also catches pairs the linear scanner misses
# (ADRP split far from its use, PC-relative forms, data references).
#
# Run as -postScript, after auto-analysis. Takes one optional argument:
# the output file path.
#
# @category OJBS
# @runtime Jython

import os

DEFAULT_OUT = "bss_xrefs.txt"
STEP = 8
BLOCK_NAMES = (".bss", "bss", ".noptrbss")


def pick_block(mem):
    for name in BLOCK_NAMES:
        blk = mem.getBlock(name)
        if blk is not None:
            return blk
    return None


def out_path():
    args = getScriptArgs()
    if args and len(args) > 0 and args[0]:
        return args[0]
    return os.path.join(os.path.expanduser("~"), DEFAULT_OUT)


def run():
    mem = currentProgram.getMemory()
    blk = pick_block(mem)
    if blk is None:
        print("[bss_xrefs] no .bss block in %s" % currentProgram.getName())
        return

    start = blk.getStart()
    size = blk.getSize()
    count = int(size // STEP)

    print("[bss_xrefs] program  %s" % currentProgram.getName())
    print("[bss_xrefs] block    %s  start %s  size 0x%x  slots %d"
          % (blk.getName(), start, size, count))

    refmgr = currentProgram.getReferenceManager()
    monitor.initialize(count)
    monitor.setMessage("counting .bss references")

    rows = []
    for i in range(count):
        monitor.checkCanceled()
        monitor.incrementProgress(1)
        addr = start.add(i * STEP)
        n = refmgr.getReferenceCountTo(addr)
        if n > 0:
            rows.append((n, addr))

    rows.sort(key=lambda r: (-r[0], r[1].getOffset()))

    path = out_path()
    fh = open(path, "w")
    try:
        fh.write("# %s\n" % currentProgram.getExecutablePath())
        fh.write("# block %s start %s size 0x%x step %d\n"
                 % (blk.getName(), start, size, STEP))
        fh.write("# rva  xref\n")
        for n, addr in rows:
            fh.write("0x%08x   xref %d\n" % (addr.getOffset(), n))
    finally:
        fh.close()

    print("[bss_xrefs] %d referenced slots, wrote %s" % (len(rows), path))
    print("[bss_xrefs] top 40:")
    for n, addr in rows[:40]:
        print("[bss_xrefs]   0x%08x   xref %d" % (addr.getOffset(), n))


run()
