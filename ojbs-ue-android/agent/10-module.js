'use strict';

// engine module identity and the address ranges worth scanning.
// the module is never named by assumption: if the config leaves it empty the
// largest .so mapped out of the package's own storage wins, and whatever is
// picked is reported with base, size and path so the choice is auditable.

var Mod = (function () {
  function pickModule() {
    var want = OJBS.target.module;
    if (want) {
      var named = C.findModule(want);
      if (named !== null) return named;
      C.log('configured module "' + want + '" is not mapped — falling back to autodetect');
    }
    var best = null;
    var mods = Process.enumerateModules();
    for (var i = 0; i < mods.length; i++) {
      var m = mods[i];
      if (m.path === null || m.path === undefined) continue;
      if (m.path.lastIndexOf('.so') !== m.path.length - 3) continue;
      if (m.path.indexOf('/data/') !== 0 &&
          m.path.indexOf('/mnt/') !== 0 &&
          m.path.indexOf('/storage/') !== 0) continue;
      if (best === null || m.size > best.size) best = m;
    }
    return best;
  }

  var mod = pickModule();

  function identity() {
    if (mod === null) return null;
    return {
      name: mod.name,
      path: mod.path,
      base: mod.base.toString(),
      size: mod.size
    };
  }

  function inModule(p) {
    if (mod === null) return false;
    if (p === null || p.isNull()) return false;
    return p.compare(mod.base) >= 0 && p.compare(mod.base.add(mod.size)) < 0;
  }

  function rva(p) {
    if (mod === null) return null;
    return '0x' + p.sub(mod.base).toString(16);
  }

  function fromRva(rvaStr) {
    if (mod === null) return null;
    return mod.base.add(ptr(rvaStr));
  }

  // writable ranges to sweep for engine globals
  function scanRanges() {
    var out = [];
    var push = function (r) {
      if (r.size <= 0) return;
      out.push({ base: r.base, size: r.size });
    };
    if (OJBS.scan.all_ranges) {
      Process.enumerateRanges('rw-').forEach(push);
    } else if (mod !== null) {
      mod.enumerateRanges('rw-').forEach(push);
      // .bss of the engine module sometimes lands in an anonymous range that
      // directly follows the image; include anything adjacent to it.
      var end = mod.base.add(mod.size);
      Process.enumerateRanges('rw-').forEach(function (r) {
        if (r.file !== null && r.file !== undefined) return;
        if (r.base.compare(end) >= 0 && r.base.sub(end).compare(ptr(0x400000)) < 0) push(r);
      });
    }
    return out;
  }

  return {
    module: mod,
    identity: identity,
    inModule: inModule,
    rva: rva,
    fromRva: fromRva,
    scanRanges: scanRanges
  };
})();
