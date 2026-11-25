'use strict';

// UWorld discovery and the GWorld static behind it.
//
// The live world is found through the object array, by class identity, not by
// a pointer chain anyone wrote down. The static that holds it is then found by
// sweeping the module's writable ranges for a slot that currently contains
// exactly that pointer — which is what GWorld is, by definition.

var World = (function () {
  var world = null;
  var gworldAddr = null;

  function candidates(limit) {
    var cls = Reflect_.findClass('World');
    if (cls === null) return [];
    var out = [];
    GObjects.forEach(function (o) {
      var c = GObjects.classOf(o);
      if (c === null || !c.equals(cls)) return;
      var n = GObjects.nameOf(o);
      if (n === null || n.indexOf('Default__') === 0) return;
      out.push(o);
      if (out.length >= limit) return false;
    }, 0);
    return out;
  }

  function hasLevel(w) {
    var cls = GObjects.classOf(w);
    if (cls === null) return false;
    var prop = Reflect_.findProp(cls, 'PersistentLevel');
    if (prop === null) return false;
    var p = R.pointer(w.add(prop.offset));
    return R.okPtr(p) && Reflect_.isUObject(p);
  }

  function resolveWorld() {
    if (world !== null) return world;
    // without class and name offsets there is no way to recognise a UWorld,
    // and no point walking the object array looking for one
    if (GObjects.offsets.klass === null || GObjects.offsets.name === null) return null;
    // a configured GWorld wins, when it still holds something that validates
    if (OJBS.offsets.gworld_rva) {
      var a = Mod.fromRva(OJBS.offsets.gworld_rva);
      if (a !== null) {
        var w = R.pointer(a);
        if (R.okPtr(w) && Reflect_.isUObject(w) && GObjects.classNameOf(w) === 'World') {
          gworldAddr = a;
          world = w;
          return world;
        }
        C.log('configured UE_GWORLD_RVA did not hold a UWorld — rescanning');
      }
    }
    var list = candidates(16);
    for (var i = 0; i < list.length; i++) {
      if (hasLevel(list[i])) { world = list[i]; break; }
    }
    if (world === null && list.length > 0) world = list[0];
    return world;
  }

  function resolveGWorld() {
    if (gworldAddr !== null) return gworldAddr;
    var w = resolveWorld();
    if (w === null) return null;
    var target = C.num(w);
    var ranges = Mod.scanRanges();
    var found = null;
    R.scanPointers(ranges, function (addr, value) {
      if (found !== null) return false;
      if (C.num(value) !== target) return false;
      found = addr;
      return true;
    });
    gworldAddr = found;
    return gworldAddr;
  }

  function info() {
    var w = resolveWorld();
    var g = resolveGWorld();
    return {
      world_va: w === null ? null : w.toString(),
      world_name: w === null ? null : GObjects.fullNameOf(w),
      gworld_va: g === null ? null : g.toString(),
      gworld_rva: g === null ? null : Mod.rva(g),
      gworld_in_module: g === null ? false : Mod.inModule(g)
    };
  }

  return {
    resolveWorld: resolveWorld,
    resolveGWorld: resolveGWorld,
    info: info
  };
})();
