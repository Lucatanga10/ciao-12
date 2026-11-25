# Turn off the analyzers that make a 414 MB ARM64 image take hours.
# Run as -preScript, before auto-analysis starts.
# @category OJBS
# @runtime Jython

from ghidra.program.model.listing import Program

SLOW = (
    "Demangler",
    "Decompiler Parameter ID",
    "Decompiler Switch Analysis",
    "Call Convention ID",
    "Shared Return Calls",
    "Create Address Tables",
    "Non-Returning Functions - Discovered",
    "Stack",
    "Variadic Function Signature Override",
    "Embedded Media",
    "DWARF",
)


def should_disable(name):
    for s in SLOW:
        if name.startswith(s) or s in name:
            return True
    return False


def run():
    opts = currentProgram.getOptions(Program.ANALYSIS_PROPERTIES)
    disabled = []
    kept = []
    for name in opts.getOptionNames():
        if should_disable(name):
            try:
                opts.setBoolean(name, False)
                disabled.append(name)
            except Exception, e:
                print("[prescript] could not disable %s: %s" % (name, e))
        else:
            kept.append(name)

    print("[prescript] disabled %d analyzers:" % len(disabled))
    for name in disabled:
        print("[prescript]   - %s" % name)
    print("[prescript] left %d analyzer options untouched" % len(kept))


run()
