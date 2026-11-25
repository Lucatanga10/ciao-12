'use strict';

// GUObjectArray discovery and UObject layout derivation.
//
// Nothing here is a remembered constant. The array is recognised by its own
// internal consistency, the item stride is measured, and the four UObject
// field offsets are derived from invariants the engine cannot violate:
//   InternalIndex   an int32 that equals the object's own array index
//   ClassPrivate    a pointer whose own ClassPrivate resolves to "Class"
//   NamePrivate     an int32 that resolves in the name pool for every sample
//   OuterPrivate    a pointer to another live object, null for top-level ones

var GObjects = (function () {
  var arr = null;     // { addr, objects, maxElements, numElements, maxChunks, numChunks, perChunk, itemSize }
  var off = {         // derived UObject offsets
    index: null, klass: null, name: null, outer: null
  };
  var tier = { index: 'unknown', klass: 'unknown', name: 'unknown', outer: 'unknown' };
  var alts = { index: [], klass: [], name: [], outer: [] };

  function readHeader(addr, objects) {
    var maxElements = R.i32(addr.add(0x10));
    var numElements = R.i32(addr.add(0x14));
    var maxChunks = R.i32(addr.add(0x18));
    var numChunks = R.i32(addr.add(0x1c));
    if (maxElements === null || numElements === null || maxChunks === null || numChunks === null) return null;
    if (numElements < 1000 || numElements > 50000000) return null;
    if (maxElements < numElements) return null;
    if (numChunks < 1 || maxChunks < numChunks || maxChunks > 65536) return null;
    if (maxElements % maxChunks !== 0) return null;
    var perChunk = maxElements / maxChunks;
    if (perChunk < 64 || perChunk > 1048576) return null;
    if (Math.ceil(numElements / perChunk) > numChunks) return null;
    return {
      addr: addr, objects: objects,
      maxElements: maxElements, numElements: numElements,
      maxChunks: maxChunks, numChunks: numChunks,
      perChunk: perChunk, itemSize: 0
    };
  }

  function deriveItemSize(cand) {
    var chunk0 = R.pointer(cand.objects);
    if (!R.okPtr(chunk0)) return 0;
    var sizes = [24, 16, 32];
    var configured = OJBS.offsets.fuobjectitem_size;
    if (configured) sizes = [configured];
    var best = 0, bestScore = 0;
    for (var s = 0; s < sizes.length; s++) {
      var size = sizes[s];
      var good = 0, tried = 0;
      for (var i = 0; i < 24; i++) {
        tried++;
        var obj = R.pointer(chunk0.add(i * size));
        if (!R.okPtr(obj)) continue;
        var vt = R.pointer(obj);
        if (!R.okPtr(vt) || !Mod.inModule(vt)) continue;
        good++;
      }
      var score = good / tried;
      if (score > bestScore) { bestScore = score; best = size; }
    }
    return bestScore >= 0.75 ? best : 0;
  }

  function validateAt(addr, value) {
    if (!R.okPtr(value)) return null;
    var cand = readHeader(addr, value);
    if (cand === null) return null;
    // the chunk table must hold live chunk pointers
    for (var c = 0; c < Math.min(cand.numChunks, 3); c++) {
      var chunk = R.pointer(cand.objects.add(c * C.PTR_SIZE));
      if (!R.okPtr(chunk)) return null;
    }
    var size = deriveItemSize(cand);
    if (size === 0) return null;
    cand.itemSize = size;
    return cand;
  }

  function fromConfig() {
    var rvaStr = OJBS.offsets.gobjects_rva;
    if (!rvaStr) return null;
    var addr = Mod.fromRva(rvaStr);
    if (addr === null) return null;
    var v = R.pointer(addr);
    if (v === null) return null;
    var cand = validateAt(addr, v);
    if (cand === null) {
      C.log('configured UE_GOBJECTS_RVA did not validate against this build — rescanning');
      return null;
    }
    return cand;
  }

  function scan() {
    var ranges = Mod.scanRanges();
    var found = null;
    R.scanPointers(ranges, function (addr, value) {
      if (found !== null) return false;
      var cand = validateAt(addr, value);
      if (cand === null) return false;
      found = cand;
      return true;
    });
    return found;
  }

  function resolve() {
    if (arr !== null) return arr;
    arr = fromConfig();
    if (arr === null) arr = scan();
    return arr;
  }

  function objectAt(i) {
    if (arr === null) return null;
    if (i < 0 || i >= arr.numElements) return null;
    var chunkIdx = Math.floor(i / arr.perChunk);
    var chunk = R.pointer(arr.objects.add(chunkIdx * C.PTR_SIZE));
    if (!R.okPtr(chunk)) return null;
    var obj = R.pointer(chunk.add((i % arr.perChunk) * arr.itemSize));
    if (!R.okPtr(obj)) return null;
    return obj;
  }

  function looksLikeObject(p) {
    if (!R.okPtr(p)) return false;
    var vt = R.pointer(p);
    return R.okPtr(vt) && Mod.inModule(vt);
  }

  function sample(count) {
    var out = [];
    if (arr === null) return out;
    var step = Math.max(1, Math.floor(arr.numElements / (count * 4)));
    for (var i = 1; i < arr.numElements && out.length < count; i += step) {
      var o = objectAt(i);
      if (o === null || !looksLikeObject(o)) continue;
      out.push({ index: i, obj: o });
    }
    return out;
  }

  // ---- derivation --------------------------------------------------------

  function deriveIndex(samples, maxOff) {
    var cands = [];
    for (var o = 4; o <= maxOff; o += 4) {
      var all = true;
      for (var i = 0; i < samples.length; i++) {
        if (R.i32(samples[i].obj.add(o)) !== samples[i].index) { all = false; break; }
      }
      if (all) cands.push(o);
    }
    return cands;
  }

  function deriveNameCands(samples, maxOff, skip) {
    var cands = [];
    for (var o = 4; o <= maxOff; o += 4) {
      if (o === skip) continue;
      var seen = {}, distinct = 0, ok = 0;
      for (var i = 0; i < samples.length; i++) {
        var idx = R.i32(samples[i].obj.add(o));
        if (idx === null || idx < 0) break;
        var s = FName.get(idx);
        if (!FName.sane(s)) break;
        ok++;
        if (!Object.prototype.hasOwnProperty.call(seen, s)) { seen[s] = 1; distinct++; }
      }
      if (ok === samples.length && distinct >= Math.max(3, samples.length * 0.3)) {
        cands.push({ offset: o, distinct: distinct });
      }
    }
    cands.sort(function (a, b) { return b.distinct - a.distinct || a.offset - b.offset; });
    return cands;
  }

  function derivePointerCands(samples, maxOff) {
    var cands = [];
    for (var o = 8; o <= maxOff; o += 8) {
      var good = 0, nulls = 0;
      for (var i = 0; i < samples.length; i++) {
        var p = R.pointer(samples[i].obj.add(o));
        if (p === null) continue;
        if (p.isNull()) { nulls++; continue; }
        if (looksLikeObject(p)) good++;
      }
      var ratio = good / samples.length;
      if (ratio >= 0.5) cands.push({ offset: o, ratio: ratio, nulls: nulls });
    }
    cands.sort(function (a, b) { return b.ratio - a.ratio || a.offset - b.offset; });
    return cands;
  }

  // a UClass' own ClassPrivate resolves to the name "Class": decisive test
  function classMetaCheck(samples, classOff, nameOff) {
    var hits = 0;
    for (var i = 0; i < samples.length; i++) {
      var cls = R.pointer(samples[i].obj.add(classOff));
      if (!looksLikeObject(cls)) continue;
      var meta = R.pointer(cls.add(classOff));
      if (!looksLikeObject(meta)) continue;
      var idx = R.i32(meta.add(nameOff));
      if (idx === null) continue;
      if (FName.get(idx) === 'Class') hits++;
    }
    return hits / samples.length;
  }

  function derive() {
    if (arr === null) return false;
    var maxOff = parseInt(OJBS.derive.max_field_offset, 16);
    if (isNaN(maxOff)) maxOff = 0x60;
    var samples = sample(OJBS.derive.samples);
    if (samples.length < 8) {
      C.log('too few live objects sampled to derive UObject offsets');
      return false;
    }

    if (OJBS.offsets.uobject_index) {
      off.index = parseInt(OJBS.offsets.uobject_index, 16); tier.index = 'operator';
    } else {
      var idxCands = deriveIndex(samples, maxOff);
      alts.index = idxCands.slice(0);
      if (idxCands.length > 0) { off.index = idxCands[0]; tier.index = 'derived'; }
    }

    var nameCands = [];
    if (OJBS.offsets.uobject_name) {
      off.name = parseInt(OJBS.offsets.uobject_name, 16); tier.name = 'operator';
    } else {
      // the name offset and the pool's block width are settled together: a
      // wrong width turns high indices into garbage, so no offset scores.
      var bitsList = FName.bitsCandidates();
      for (var bi = 0; bi < bitsList.length; bi++) {
        if (bi > 0 && !FName.setBits(bitsList[bi])) break;
        nameCands = deriveNameCands(samples, maxOff, off.index);
        if (nameCands.length > 0) {
          if (bi > 0) C.log('name pool block width settled at ' + bitsList[bi] + ' bits');
          break;
        }
      }
      alts.name = nameCands.slice(0, 6);
      if (nameCands.length > 0) { off.name = nameCands[0].offset; tier.name = 'derived'; }
    }

    if (OJBS.offsets.uobject_class) {
      off.klass = parseInt(OJBS.offsets.uobject_class, 16); tier.klass = 'operator';
    } else {
      var ptrCands = derivePointerCands(samples, maxOff);
      alts.klass = ptrCands.slice(0, 6);
      var chosen = null;
      for (var i = 0; i < ptrCands.length && chosen === null; i++) {
        for (var j = 0; j < Math.max(1, nameCands.length); j++) {
          var nOff = nameCands.length > 0 ? nameCands[j].offset : off.name;
          if (nOff === null || nOff === undefined) continue;
          if (classMetaCheck(samples, ptrCands[i].offset, nOff) >= 0.8) {
            chosen = ptrCands[i].offset;
            off.name = nOff;
            tier.name = (tier.name === 'operator') ? 'operator' : 'confirmed';
            break;
          }
        }
      }
      if (chosen !== null) { off.klass = chosen; tier.klass = 'confirmed'; }
      else if (ptrCands.length > 0) { off.klass = ptrCands[0].offset; tier.klass = 'derived'; }
    }

    if (OJBS.offsets.uobject_outer) {
      off.outer = parseInt(OJBS.offsets.uobject_outer, 16); tier.outer = 'operator';
    } else {
      var outerCands = derivePointerCands(samples, maxOff).filter(function (c) {
        return c.offset !== off.klass && c.offset > (off.name === null ? 0 : off.name);
      });
      alts.outer = outerCands.slice(0, 6);
      if (outerCands.length > 0) { off.outer = outerCands[0].offset; tier.outer = 'derived'; }
    }

    return off.klass !== null && off.name !== null;
  }

  // ---- accessors ---------------------------------------------------------

  function nameOf(obj) {
    if (off.name === null || !R.okPtr(obj)) return null;
    return FName.at(obj.add(off.name));
  }

  function classOf(obj) {
    if (off.klass === null || !R.okPtr(obj)) return null;
    var c = R.pointer(obj.add(off.klass));
    return looksLikeObject(c) ? c : null;
  }

  function outerOf(obj) {
    if (off.outer === null || !R.okPtr(obj)) return null;
    var o = R.pointer(obj.add(off.outer));
    return looksLikeObject(o) ? o : null;
  }

  function indexOf(obj) {
    if (off.index === null || !R.okPtr(obj)) return null;
    return R.i32(obj.add(off.index));
  }

  function fullNameOf(obj) {
    var parts = [];
    var cur = obj;
    var guard = 0;
    while (cur !== null && guard++ < 16) {
      var n = nameOf(cur);
      if (n === null) break;
      parts.unshift(n);
      cur = outerOf(cur);
    }
    if (parts.length === 0) return null;
    return parts.join('.');
  }

  function classNameOf(obj) {
    var c = classOf(obj);
    return c === null ? null : nameOf(c);
  }

  function forEach(cb, limit) {
    if (arr === null) return 0;
    var n = arr.numElements;
    if (limit && limit > 0 && limit < n) n = limit;
    var visited = 0;
    for (var i = 0; i < n; i++) {
      var o = objectAt(i);
      if (o === null) continue;
      visited++;
      if (cb(o, i) === false) break;
    }
    return visited;
  }

  function info() {
    if (arr === null) return null;
    return {
      gobjects_va: arr.addr.toString(),
      gobjects_rva: Mod.rva(arr.addr),
      objects_table: arr.objects.toString(),
      num_elements: arr.numElements,
      max_elements: arr.maxElements,
      num_chunks: arr.numChunks,
      max_chunks: arr.maxChunks,
      per_chunk: arr.perChunk,
      item_size: arr.itemSize,
      uobject: {
        index: off.index === null ? null : '0x' + off.index.toString(16),
        klass: off.klass === null ? null : '0x' + off.klass.toString(16),
        name: off.name === null ? null : '0x' + off.name.toString(16),
        outer: off.outer === null ? null : '0x' + off.outer.toString(16)
      },
      tier: tier,
      alternatives: alts
    };
  }

  return {
    resolve: resolve,
    derive: derive,
    objectAt: objectAt,
    looksLikeObject: looksLikeObject,
    nameOf: nameOf,
    classOf: classOf,
    outerOf: outerOf,
    indexOf: indexOf,
    fullNameOf: fullNameOf,
    classNameOf: classNameOf,
    forEach: forEach,
    info: info,
    offsets: off,
    array: function () { return arr; }
  };
})();
