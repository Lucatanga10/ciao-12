'use strict';

// watchlist resolution and in-process sampling.
//
// Resolution turns names into a replayable op list, written to offsets.json
// so later runs — including the external /proc/mem reader, which knows nothing
// about the engine — can follow the same chain without touching reflection.
//
// Sampling assumption, stated once because it is not modelled: values are read
// asynchronously from a running process. There is no tick alignment, no
// interpolation and no snapshot consistency between two entries in one line.

var Watch = (function () {
  function rootFor(entry) {
    var kind = entry.root || 'gworld';
    if (kind === 'gworld') {
      var w = World.resolveWorld();
      if (w === null) return { ok: false, error: 'no UWorld found' };
      var g = World.resolveGWorld();
      return {
        ok: true,
        addr: w,
        struct: GObjects.classOf(w),
        desc: { kind: 'gworld', rva: g === null ? null : Mod.rva(g), va: w.toString() },
        prefix: (g === null) ? [] : [{ op: 'deref' }],
        replayable: g !== null
      };
    }
    if (kind.indexOf('class:') === 0) {
      var cname = kind.substring(6);
      var cls = Reflect_.findClass(cname);
      if (cls === null) return { ok: false, error: 'class "' + cname + '" not in the object array' };
      var inst = null;
      GObjects.forEach(function (o) {
        var c = GObjects.classOf(o);
        if (c === null || !c.equals(cls)) return;
        var n = GObjects.nameOf(o);
        if (n === null || n.indexOf('Default__') === 0) return;
        inst = o;
        return false;
      }, 0);
      if (inst === null) return { ok: false, error: 'no live instance of "' + cname + '"' };
      return {
        ok: true,
        addr: inst,
        struct: cls,
        desc: { kind: 'class', name: cname, va: inst.toString() },
        prefix: [],
        replayable: false
      };
    }
    return { ok: false, error: 'unknown root "' + kind + '"' };
  }

  function resolveEntry(entry) {
    var root = rootFor(entry);
    if (!root.ok) {
      return { name: entry.name, status: 'gated', error: root.error, ops: [], type: entry.type || null };
    }
    var res = Reflect_.walk(root.addr, root.struct, entry.path, entry.type || null);
    var ops = root.prefix.concat(res.trace || []);
    if (!res.ok) {
      return {
        name: entry.name, status: 'gated', error: res.error, root: root.desc,
        ops: ops, type: entry.type || null, replayable: false
      };
    }
    return {
      name: entry.name, status: 'ok', root: root.desc, ops: ops,
      type: res.type, sample: res.value, address: res.addr.toString(),
      replayable: root.replayable
    };
  }

  function resolveAll() {
    var list = OJBS.watchlist && OJBS.watchlist.entries ? OJBS.watchlist.entries : [];
    var out = [];
    for (var i = 0; i < list.length; i++) {
      var e = list[i];
      if (!e || !e.name || !e.path) continue;
      var r;
      try { r = resolveEntry(e); }
      catch (ex) { r = { name: e.name, status: 'gated', error: 'exception: ' + ex, ops: [] }; }
      C.log('resolve ' + r.name + ': ' + r.status + (r.status === 'ok' ? ' = ' + r.sample : ' (' + r.error + ')'));
      out.push(r);
    }
    return out;
  }

  function offsetsDocument(entries) {
    var m = Mod.identity();
    var g = GObjects.info();
    return {
      schema: 1,
      captured_at: C.nowIso(),
      build_id: OJBS.target.build_id,
      assumption: 'values read asynchronously from a running process; no tick alignment, no interpolation modelled',
      module: m,
      globals: {
        fnamepool: FName.info(),
        gobjects_rva: g === null ? null : g.gobjects_rva,
        gobjects_item_size: g === null ? null : g.item_size,
        gobjects_per_chunk: g === null ? null : g.per_chunk,
        gworld_rva: World.info().gworld_rva
      },
      uobject: g === null ? null : g.uobject,
      reflection: Reflect_.info(),
      entries: entries
    };
  }

  function sample(entries) {
    var line = { t: Date.now(), values: {} };
    for (var i = 0; i < entries.length; i++) {
      var e = entries[i];
      if (e.status !== 'ok') { line.values[e.name] = null; continue; }
      var src = null;
      for (var j = 0; j < OJBS.watchlist.entries.length; j++) {
        if (OJBS.watchlist.entries[j].name === e.name) { src = OJBS.watchlist.entries[j]; break; }
      }
      if (src === null) { line.values[e.name] = null; continue; }
      var root = rootFor(src);
      if (!root.ok) { line.values[e.name] = null; continue; }
      var r = Reflect_.walk(root.addr, root.struct, src.path, src.type || null);
      line.values[e.name] = r.ok ? r.value : null;
    }
    return line;
  }

  function run(entries) {
    var h = Out.open('samples.ndjson');
    if (h === null) return { ok: false, error: 'cannot open samples.ndjson' };
    Out.line(h, {
      meta: true,
      started: C.nowIso(),
      period_ms: OJBS.watch.period_ms,
      duration_s: OJBS.watch.duration_s,
      assumption: 'asynchronous reads, no tick alignment'
    });
    var ticks = 0;
    var maxTicks = Math.max(1, Math.floor((OJBS.watch.duration_s * 1000) / OJBS.watch.period_ms));
    var timer = setInterval(function () {
      ticks++;
      var l;
      try { l = sample(entries); } catch (e) { l = { t: Date.now(), error: '' + e }; }
      Out.line(h, l);
      if (ticks >= maxTicks) {
        clearInterval(timer);
        Out.close(h);
        Out.marker('done.json', { mode: 'watch', ticks: ticks, finished: C.nowIso() });
        C.log('watch finished after ' + ticks + ' ticks');
      }
    }, OJBS.watch.period_ms);
    return { ok: true, ticks_planned: maxTicks };
  }

  // flat, shell-parsable form of the same chains:
  //   name|root_kind|root_rva|type|op,op,op
  // ops are add:0xN, deref, array:index:stride
  function chainsText(entries) {
    var lines = [];
    for (var i = 0; i < entries.length; i++) {
      var e = entries[i];
      if (e.status !== 'ok' || e.replayable !== true) continue;
      var ops = [];
      for (var j = 0; j < e.ops.length; j++) {
        var o = e.ops[j];
        if (o.op === 'add') ops.push('add:0x' + o.offset.toString(16));
        else if (o.op === 'deref') ops.push('deref');
        else if (o.op === 'array') ops.push('array:' + o.index + ':' + o.stride);
      }
      lines.push([e.name, e.root.kind, e.root.rva, e.type, ops.join(',')].join('|'));
    }
    return lines.join('\n') + (lines.length > 0 ? '\n' : '');
  }

  return { resolveAll: resolveAll, offsetsDocument: offsetsDocument, chainsText: chainsText, run: run };
})();
