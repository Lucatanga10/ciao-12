'use strict';

// full reflection dump: one JSON object per class or struct, written as it is
// produced so a long dump never has to be held in memory.

var Dump = (function () {
  function propRecord(p) {
    return {
      name: p.name,
      offset: p.offset,
      offset_hex: '0x' + p.offset.toString(16),
      element_size: p.elemSize,
      kind: p.kind,
      struct: p.structObj === null ? null : GObjects.nameOf(p.structObj),
      inner_kind: p.inner === null ? null : p.inner.kind,
      inner_size: p.inner === null ? null : p.inner.elemSize,
      inner_struct: (p.inner === null || p.inner.structObj === null) ? null : GObjects.nameOf(p.inner.structObj)
    };
  }

  function wanted(fullName) {
    var f = OJBS.dump.class_filter;
    if (!f) return true;
    return fullName !== null && fullName.indexOf(f) >= 0;
  }

  function run() {
    var h = Out.open('classes.ndjson');
    if (h === null) return { ok: false, error: 'cannot open classes.ndjson' };
    var written = 0, skipped = 0, scanned = 0;
    var limit = OJBS.dump.max_classes;

    GObjects.forEach(function (obj) {
      scanned++;
      if ((scanned % 200000) === 0) C.log('dump scan ' + scanned + ', written ' + written);
      var cn = GObjects.classNameOf(obj);
      if (cn === null) return;
      var kind;
      if (cn === 'Class' || cn === 'BlueprintGeneratedClass' || cn === 'WidgetBlueprintGeneratedClass' ||
          cn === 'AnimBlueprintGeneratedClass' || cn === 'DynamicClass') kind = 'class';
      else if (cn === 'ScriptStruct' || cn === 'Struct' || cn === 'UserDefinedStruct') kind = 'struct';
      else return;

      var full = GObjects.fullNameOf(obj);
      if (!wanted(full)) { skipped++; return; }

      var sup = Reflect_.superOf(obj);
      var rec = {
        kind: kind,
        name: GObjects.nameOf(obj),
        full_name: full,
        address: obj.toString(),
        index: GObjects.indexOf(obj),
        meta_class: cn,
        super: sup === null ? null : GObjects.nameOf(sup),
        super_full: sup === null ? null : GObjects.fullNameOf(sup),
        size: Reflect_.sizeOf(obj),
        props: []
      };

      var own = [];
      var nodes = [];
      try { nodes = Reflect_.propsOf(obj); } catch (e) { nodes = []; }
      for (var i = 0; i < nodes.length; i++) own.push(propRecord(nodes[i]));
      rec.props = own;
      rec.prop_count = own.length;

      Out.line(h, rec);
      written++;
      if (limit > 0 && written >= limit) return false;
    }, 0);

    Out.close(h);
    return { ok: true, written: written, skipped: skipped, scanned: scanned, path: h.path };
  }

  return { run: run };
})();
