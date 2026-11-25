'use strict';

// entry point. runs on a timer tick so that a user script appended after this
// file has already defined ojbs_user_main() by the time mode "run" calls it.

function ojbsMissingReport() {
  var g = GObjects.info();
  var r = Reflect_.info();
  var miss = [];
  var add = function (key, what, recipe) { miss.push({ key: key, what: what, recipe: recipe }); };

  if (FName.current() === null) {
    add('UE_FNAMEPOOL_BLOCKS_RVA', 'FNamePool block table',
        'x64dbg/IDA on the engine .so: xref the string "None"; the pool is the writable global whose first block decodes entry 0 as a 2-byte header followed by "None". Set the RVA of the Blocks array.');
  }
  if (g === null) {
    add('UE_GOBJECTS_RVA', 'FUObjectArray',
        'Find a writable global laid out as {ptr Objects, ptr PreAllocated, i32 MaxElements, i32 NumElements, i32 MaxChunks, i32 NumChunks} where NumElements is in the hundreds of thousands.');
  } else {
    if (g.uobject.index === null) add('UE_OFF_UOBJECT_INDEX', 'UObject::InternalIndex', 'int32 inside the first 0x40 bytes of any object that equals that object index in GObjects.');
    if (g.uobject.klass === null) add('UE_OFF_UOBJECT_CLASS', 'UObject::ClassPrivate', 'pointer field present on every object; the object it points at has the same field pointing at a UClass named "Class".');
    if (g.uobject.name === null) add('UE_OFF_UOBJECT_NAME', 'UObject::NamePrivate', 'int32 field that resolves in the name pool for every object sampled.');
    if (g.uobject.outer === null) add('UE_OFF_UOBJECT_OUTER', 'UObject::OuterPrivate', 'pointer to another live object, null on top-level packages.');
  }
  if (r.ustruct.super === null) add('UE_OFF_USTRUCT_SUPER', 'UStruct::SuperStruct', 'pointer on a UClass to another UClass; the chain must terminate at Object.');
  if (r.ustruct.properties_size === null) add('UE_OFF_USTRUCT_SIZE', 'UStruct::PropertiesSize', 'int32 never smaller than the super class value, under 0x20000.');
  if (r.ustruct.child_properties === null && r.ustruct.children === null) {
    add('UE_OFF_USTRUCT_CHILDPROPS', 'UStruct::ChildProperties', 'pointer to a node that is NOT in GObjects (UE>=4.25). On older builds set UE_OFF_USTRUCT_CHILDREN instead, whose nodes are UObjects.');
  }
  if (r.ffield.next === null) add('UE_OFF_FFIELD_NEXT', 'FField::Next', 'pointer that walks the property list to null.');
  if (r.ffield.name === null) add('UE_OFF_FFIELD_NAME', 'FField::NamePrivate', 'int32 on each list node that resolves in the name pool.');
  if (r.fproperty.offset_internal === null) add('UE_OFF_FPROPERTY_OFFSET', 'FProperty::Offset_Internal', 'int32 on each property node below the owning class PropertiesSize, distinct across the list.');
  if (r.fproperty.element_size === null) add('UE_OFF_FPROPERTY_ELEMSIZE', 'FProperty::ElementSize', 'int32 in [1,0x4000] where Offset_Internal + ElementSize stays inside PropertiesSize.');
  if (r.extras.structprop_struct === null) add('UE_OFF_FSTRUCTPROP_STRUCT', 'FStructProperty::Struct', 'pointer past the FProperty header on StructProperty nodes, aimed at a UScriptStruct. Without it, inline struct hops are gated.');
  if (r.extras.arrayprop_inner === null) add('UE_OFF_FARRAYPROP_INNER', 'FArrayProperty::Inner', 'pointer past the FProperty header on ArrayProperty nodes, aimed at another property node. Without it, Prop[i] hops are gated.');
  if (World.info().gworld_rva === null) add('UE_GWORLD_RVA', 'GWorld', 'writable global currently holding the UWorld pointer that `ojbs derive` reports.');
  return miss;
}

function ojbsReport(extra) {
  var doc = {
    schema: 1,
    mode: OJBS.mode,
    captured_at: C.nowIso(),
    build_id: OJBS.target.build_id,
    package: OJBS.target.package,
    module: Mod.identity(),
    fname: FName.info(),
    gobjects: GObjects.info(),
    reflection: Reflect_.info(),
    world: World.info(),
    missing: ojbsMissingReport()
  };
  if (extra) {
    for (var k in extra) {
      if (Object.prototype.hasOwnProperty.call(extra, k)) doc[k] = extra[k];
    }
  }
  return doc;
}

function ojbsMain() {
  var t0 = Date.now();
  R.buildIndex();

  var mod = Mod.identity();
  if (mod === null) {
    Out.json('derived.json', { error: 'no engine module found' });
    Out.marker('done.json', { ok: false, error: 'no engine module found', mode: OJBS.mode });
    return;
  }
  C.log('module ' + mod.name + ' base ' + mod.base + ' size ' + mod.size);

  var pool = FName.resolve();
  C.log('name pool: ' + (pool === null ? 'NOT FOUND' : pool.kind + ' at ' + pool.blocks));

  var arr = GObjects.resolve();
  C.log('object array: ' + (arr === null ? 'NOT FOUND' : arr.numElements + ' objects, item size ' + arr.itemSize));

  if (arr !== null && pool !== null) {
    GObjects.derive();
    var o = GObjects.offsets;
    C.log('UObject: index=' + o.index + ' class=' + o.klass + ' name=' + o.name + ' outer=' + o.outer);
  }

  var reflectOk = false;
  if (arr !== null && pool !== null && GObjects.offsets.klass !== null) {
    reflectOk = Reflect_.derive();
    C.log('reflection derived: ' + reflectOk + ' ' + JSON.stringify(Reflect_.info().fproperty));
  }

  if (reflectOk) {
    var w = World.info();
    C.log('world: ' + w.world_name + ' @ ' + w.world_va + ' gworld rva ' + w.gworld_rva);
  }

  var extra = {};
  var mode = OJBS.mode;

  if (mode === 'dump') {
    if (!reflectOk) {
      C.log('reflection not derived — dump is gated, writing the report only');
      extra.dump = { ok: false, error: 'reflection layout not derived' };
    } else {
      extra.dump = Dump.run();
      C.log('dump: ' + JSON.stringify(extra.dump));
    }
  }

  if (mode === 'resolve' || mode === 'watch') {
    if (!reflectOk) {
      extra.resolve = { ok: false, error: 'reflection layout not derived' };
    } else {
      var entries = Watch.resolveAll();
      Out.json('offsets.json', Watch.offsetsDocument(entries));
      var ch = Out.open('chains.txt');
      if (ch !== null) { Out.raw(ch, Watch.chainsText(entries)); Out.close(ch); }
      extra.resolve = { ok: true, entries: entries.length };
      if (mode === 'watch') {
        extra.watch = Watch.run(entries);
        Out.json('derived.json', ojbsReport(extra));
        return;   // the watch timer writes done.json when it finishes
      }
    }
  }

  if (mode === 'run') {
    if (typeof ojbs_user_main === 'function') {
      try { extra.user = ojbs_user_main({ C: C, R: R, Mod: Mod, FName: FName, GObjects: GObjects, Reflect: Reflect_, World: World, Out: Out }) || true; }
      catch (e) { extra.user = { error: '' + e }; C.log('user script threw: ' + e); }
    } else {
      extra.user = { error: 'no ojbs_user_main() defined in the injected script' };
    }
  }

  Out.json('derived.json', ojbsReport(extra));
  Out.marker('done.json', {
    ok: true, mode: mode, elapsed_ms: Date.now() - t0,
    missing: ojbsMissingReport().length, finished: C.nowIso()
  });
  C.log('done in ' + (Date.now() - t0) + ' ms');
}

setTimeout(ojbsMain, 0);
