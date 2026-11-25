'use strict';

// UStruct / FField / FProperty layout derivation and reflection access.
//
// Once the name pool and the object array are up, the engine's own reflection
// data describes every field of every class. The offsets needed to read that
// data are themselves derived here, by scoring candidate offsets against
// invariants:
//   SuperStruct      points at another object whose class is "Class"
//   PropertiesSize   an int32 never smaller than the super's, bounded by 0x20000
//   ChildProperties  points at a node that is NOT in the object array
//   FField::Name     an int32 that resolves in the pool for every node walked
//   FField::Next     a pointer that terminates the list within bounds
//   FField::Class    points at an FFieldClass whose first FName ends "Property"
//   Offset_Internal  an int32 below the owning struct's PropertiesSize
//   ElementSize      an int32 in [1,0x4000] with Offset+Size <= PropertiesSize
//
// Anything that fails to score stays null, is reported as unknown, and gates
// only the paths that need it.

var Reflect_ = (function () {
  var off = {
    super_: null, childProps: null, children: null, size: null,
    fieldNext: null, fieldName: null, fieldClass: null,
    propOffset: null, propElemSize: null,
    structPropStruct: null, arrayPropInner: null
  };
  var tier = {};
  var mode = 'none';          // 'ffield' (UE>=4.25) or 'ufield' (UE<4.25)
  var classIndex = null;      // name -> [objPtr]
  var structIndex = null;     // name -> [objPtr]
  var propCache = {};

  function objEnd() {
    var o = GObjects.offsets;
    var m = 0;
    ['index', 'klass', 'name', 'outer'].forEach(function (k) {
      if (o[k] !== null && o[k] > m) m = o[k];
    });
    return m + 8;
  }

  function isUObject(p) {
    if (!GObjects.looksLikeObject(p)) return false;
    var i = GObjects.indexOf(p);
    if (i === null || i < 0) return false;
    var o = GObjects.objectAt(i);
    return o !== null && o.equals(p);
  }

  // ---- indices -----------------------------------------------------------

  function buildIndices() {
    classIndex = {};
    structIndex = {};
    var n = 0;
    GObjects.forEach(function (obj) {
      n++;
      if ((n % 200000) === 0) C.log('indexing objects: ' + n);
      var cn = GObjects.classNameOf(obj);
      if (cn === null) return;
      var target = null;
      if (cn === 'Class' || cn === 'BlueprintGeneratedClass' || cn === 'WidgetBlueprintGeneratedClass' ||
          cn === 'AnimBlueprintGeneratedClass' || cn === 'DynamicClass') target = classIndex;
      else if (cn === 'ScriptStruct' || cn === 'Struct' || cn === 'UserDefinedStruct') target = structIndex;
      if (target === null) return;
      var name = GObjects.nameOf(obj);
      if (name === null) return;
      if (!Object.prototype.hasOwnProperty.call(target, name)) target[name] = [];
      target[name].push(obj);
    }, 0);
    C.log('indexed ' + Object.keys(classIndex).length + ' classes, ' +
          Object.keys(structIndex).length + ' structs from ' + n + ' objects');
  }

  function findClass(name) {
    if (classIndex === null) buildIndices();
    var l = classIndex[name];
    return (l && l.length > 0) ? l[0] : null;
  }

  function findStruct(name) {
    if (structIndex === null) buildIndices();
    var l = structIndex[name];
    if (l && l.length > 0) return l[0];
    return findClass(name);
  }

  function sampleClasses(count) {
    if (classIndex === null) buildIndices();
    var names = Object.keys(classIndex);
    var out = [];
    var step = Math.max(1, Math.floor(names.length / (count * 2)));
    for (var i = 0; i < names.length && out.length < count; i += step) {
      var o = classIndex[names[i]][0];
      if (o !== null) out.push(o);
    }
    return out;
  }

  // ---- UStruct derivation ------------------------------------------------

  function deriveSuper(samples, lo, hi) {
    var best = null;
    for (var o = lo; o <= hi; o += 8) {
      var good = 0, chainOk = 0;
      for (var i = 0; i < samples.length; i++) {
        var p = R.pointer(samples[i].add(o));
        if (p === null) continue;
        if (p.isNull()) { good++; continue; }   // Object itself has no super
        if (!isUObject(p)) continue;
        var cn = GObjects.classNameOf(p);
        if (cn === null || cn.indexOf('Class') < 0) continue;
        good++;
        // the chain must terminate
        var cur = p, steps = 0, seen = {};
        while (cur !== null && !cur.isNull() && steps++ < 24) {
          var key = cur.toString();
          if (seen[key]) { steps = 999; break; }
          seen[key] = 1;
          cur = R.pointer(cur.add(o));
          if (cur !== null && !cur.isNull() && !isUObject(cur)) { steps = 999; break; }
        }
        if (steps < 24) chainOk++;
      }
      var score = (good / samples.length) * 0.5 + (chainOk / samples.length) * 0.5;
      if (score >= 0.7 && (best === null || score > best.score)) best = { offset: o, score: score };
    }
    return best;
  }

  function superOf(structObj) {
    if (off.super_ === null || structObj === null) return null;
    var p = R.pointer(structObj.add(off.super_));
    if (p === null || p.isNull() || !isUObject(p)) return null;
    return p;
  }

  function deriveSizeCands(samples, lo, hi) {
    var cands = [];
    for (var o = lo; o <= hi; o += 4) {
      var good = 0, tested = 0;
      for (var i = 0; i < samples.length; i++) {
        var sup = superOf(samples[i]);
        if (sup === null) continue;
        tested++;
        var v = R.i32(samples[i].add(o));
        var sv = R.i32(sup.add(o));
        if (v === null || sv === null) continue;
        if (sv < 0 || v < sv || v > 0x20000 || v < 8) continue;
        good++;
      }
      if (tested >= 4 && good / tested >= 0.85) cands.push({ offset: o, score: good / tested });
    }
    cands.sort(function (a, b) { return b.score - a.score || a.offset - b.offset; });
    return cands;
  }

  function sizeOf(structObj) {
    if (off.size === null || structObj === null) return null;
    return R.i32(structObj.add(off.size));
  }

  // ---- FField derivation -------------------------------------------------

  function childCandidates(samples, lo, hi, wantUObject) {
    var cands = [];
    for (var o = lo; o <= hi; o += 8) {
      var good = 0, nulls = 0;
      for (var i = 0; i < samples.length; i++) {
        var p = R.pointer(samples[i].add(o));
        if (p === null) continue;
        if (p.isNull()) { nulls++; continue; }
        if (!R.okPtr(p)) continue;
        var vt = R.pointer(p);
        if (!R.okPtr(vt) || !Mod.inModule(vt)) continue;
        var isObj = isUObject(p);
        if (wantUObject ? isObj : !isObj) good++;
      }
      if (good >= Math.max(3, samples.length * 0.3)) {
        cands.push({ offset: o, good: good, nulls: nulls });
      }
    }
    cands.sort(function (a, b) { return b.good - a.good || a.offset - b.offset; });
    return cands;
  }

  function walkChainScore(samples, cpOff, nameOff, nextOff, wantUObject) {
    var ok = 0, tried = 0;
    for (var i = 0; i < samples.length; i++) {
      var node = R.pointer(samples[i].add(cpOff));
      if (node === null || node.isNull() || !R.okPtr(node)) continue;
      if (wantUObject ? !isUObject(node) : isUObject(node)) continue;
      tried++;
      var steps = 0, named = 0, cur = node, clean = true;
      while (cur !== null && !cur.isNull() && steps < 64) {
        if (!R.okPtr(cur)) { clean = false; break; }
        var idx = R.i32(cur.add(nameOff));
        if (idx === null || !FName.sane(FName.get(idx))) { clean = false; break; }
        named++;
        var nxt = R.pointer(cur.add(nextOff));
        if (nxt === null) { clean = false; break; }
        if (nxt.isNull()) break;
        if (!R.okPtr(nxt)) { clean = false; break; }
        if (wantUObject ? !isUObject(nxt) : isUObject(nxt)) { clean = false; break; }
        cur = nxt;
        steps++;
      }
      if (clean && named >= 1 && steps < 64) ok++;
    }
    if (tried < 3) return 0;
    return ok / tried;
  }

  function deriveFieldLayout(samples, lo, hi) {
    var variants = [
      { wantUObject: false, mode: 'ffield' },
      { wantUObject: true, mode: 'ufield' }
    ];
    var best = null;
    for (var v = 0; v < variants.length; v++) {
      var cps = childCandidates(samples, lo, hi, variants[v].wantUObject);
      for (var c = 0; c < Math.min(cps.length, 6); c++) {
        var nameRange = variants[v].wantUObject
          ? [GObjects.offsets.name]
          : [0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c];
        for (var ni = 0; ni < nameRange.length; ni++) {
          var nameOff = nameRange[ni];
          if (nameOff === null || nameOff === undefined) continue;
          for (var nextOff = 8; nextOff <= 0x48; nextOff += 8) {
            if (nameOff >= nextOff && nameOff < nextOff + 8) continue;
            var score = walkChainScore(samples, cps[c].offset, nameOff, nextOff, variants[v].wantUObject);
            if (score >= 0.7 && (best === null || score > best.score)) {
              best = {
                score: score, mode: variants[v].mode,
                childOff: cps[c].offset, nameOff: nameOff, nextOff: nextOff
              };
            }
          }
        }
      }
      if (best !== null) break;
    }
    return best;
  }

  function fieldName(node) {
    if (off.fieldName === null) return null;
    return FName.at(node.add(off.fieldName));
  }

  function fieldNext(node) {
    if (off.fieldNext === null) return null;
    var p = R.pointer(node.add(off.fieldNext));
    if (p === null || p.isNull() || !R.okPtr(p)) return null;
    return p;
  }

  function fieldList(structObj, max) {
    var out = [];
    if (off.childProps === null && off.children === null) return out;
    var head = R.pointer(structObj.add(off.childProps !== null ? off.childProps : off.children));
    var cur = head;
    var guard = 0;
    while (cur !== null && !cur.isNull() && R.okPtr(cur) && guard++ < (max || 4096)) {
      out.push(cur);
      cur = fieldNext(cur);
    }
    return out;
  }

  // FFieldClass { FName Name; ... } — the property kind, by name
  function deriveFieldClass(nodes) {
    for (var o = 0; o <= 0x20; o += 8) {
      var hits = 0, tried = 0;
      for (var i = 0; i < nodes.length; i++) {
        var p = R.pointer(nodes[i].add(o));
        if (p === null || p.isNull() || !R.okPtr(p)) continue;
        tried++;
        var idx = R.i32(p);
        if (idx === null) continue;
        var n = FName.get(idx);
        if (n !== null && n.length > 8 && n.lastIndexOf('Property') === n.length - 8) hits++;
      }
      if (tried >= 4 && hits / tried >= 0.7) return { offset: o, score: hits / tried };
    }
    return null;
  }

  function kindOf(node) {
    if (mode === 'ufield') {
      var cls = GObjects.classOf(node);
      return cls === null ? null : GObjects.nameOf(cls);
    }
    if (off.fieldClass === null) return null;
    var fc = R.pointer(node.add(off.fieldClass));
    if (fc === null || !R.okPtr(fc)) return null;
    var idx = R.i32(fc);
    return idx === null ? null : FName.get(idx);
  }

  // ---- FProperty derivation ---------------------------------------------

  function collectPropertyNodes(samples, maxPerClass) {
    var out = [];
    for (var i = 0; i < samples.length; i++) {
      var size = sizeOf(samples[i]);
      // a loose bound keeps FProperty derivation alive when PropertiesSize
      // itself could not be derived; the tier records that it was loose
      if (size === null || size <= 8) size = 0x2000;
      var nodes = fieldList(samples[i], maxPerClass);
      for (var j = 0; j < nodes.length; j++) out.push({ node: nodes[j], size: size });
    }
    return out;
  }

  function derivePropOffsets(pnodes, lo, hi) {
    var offCands = [];
    for (var o = lo; o <= hi; o += 4) {
      var inRange = 0, ones = 0, values = {}, distinct = 0, tried = 0;
      for (var i = 0; i < pnodes.length; i++) {
        var v = R.i32(pnodes[i].node.add(o));
        if (v === null) continue;
        tried++;
        if (v === 1) ones++;
        if (v >= 0 && v < pnodes[i].size) inRange++;
        if (!Object.prototype.hasOwnProperty.call(values, '' + v)) { values['' + v] = 1; distinct++; }
      }
      if (tried < 8) continue;
      if (ones / tried > 0.9) continue;            // ArrayDim
      if (inRange / tried >= 0.9 && distinct >= 4) {
        offCands.push({ offset: o, score: inRange / tried, distinct: distinct });
      }
    }
    offCands.sort(function (a, b) { return b.distinct - a.distinct || b.score - a.score || a.offset - b.offset; });

    var elemCands = [];
    for (var e = lo; e <= hi; e += 4) {
      var good = 0, t = 0;
      for (var k = 0; k < pnodes.length; k++) {
        var ev = R.i32(pnodes[k].node.add(e));
        if (ev === null) continue;
        t++;
        if (ev >= 1 && ev <= 0x4000) good++;
      }
      if (t >= 8 && good / t >= 0.95) elemCands.push({ offset: e, score: good / t });
    }

    // pick the pair that satisfies Offset + ElementSize <= PropertiesSize
    var best = null;
    for (var a = 0; a < Math.min(offCands.length, 6); a++) {
      for (var b = 0; b < Math.min(elemCands.length, 8); b++) {
        if (offCands[a].offset === elemCands[b].offset) continue;
        var fit = 0, n = 0;
        for (var m = 0; m < pnodes.length; m++) {
          var ov = R.i32(pnodes[m].node.add(offCands[a].offset));
          var sv = R.i32(pnodes[m].node.add(elemCands[b].offset));
          if (ov === null || sv === null) continue;
          n++;
          if (ov + sv <= pnodes[m].size) fit++;
        }
        if (n < 8) continue;
        var score = fit / n;
        if (score >= 0.9 && (best === null || score > best.score)) {
          best = { offsetOff: offCands[a].offset, elemOff: elemCands[b].offset, score: score };
        }
      }
    }
    return { best: best, offsetCandidates: offCands.slice(0, 6), elemCandidates: elemCands.slice(0, 6) };
  }

  // FStructProperty::Struct and FArrayProperty::Inner sit past the FProperty
  // header; both are found by what they point at, not by where they sit.
  function derivePointerExtras(pnodes, lo, hi) {
    var structNodes = [], arrayNodes = [];
    for (var i = 0; i < pnodes.length; i++) {
      var k = kindOf(pnodes[i].node);
      if (k === 'StructProperty') structNodes.push(pnodes[i].node);
      else if (k === 'ArrayProperty') arrayNodes.push(pnodes[i].node);
    }
    var res = { struct: null, inner: null };
    var o;
    if (structNodes.length >= 3) {
      for (o = lo; o <= hi; o += 8) {
        var hits = 0;
        for (var s = 0; s < structNodes.length; s++) {
          var p = R.pointer(structNodes[s].add(o));
          if (p === null || p.isNull() || !isUObject(p)) continue;
          var cn = GObjects.classNameOf(p);
          if (cn === 'ScriptStruct' || cn === 'Struct' || cn === 'UserDefinedStruct') hits++;
        }
        if (hits / structNodes.length >= 0.8) { res.struct = o; break; }
      }
    }
    if (arrayNodes.length >= 3) {
      for (o = lo; o <= hi; o += 8) {
        var ih = 0;
        for (var a = 0; a < arrayNodes.length; a++) {
          var ip = R.pointer(arrayNodes[a].add(o));
          if (ip === null || ip.isNull() || !R.okPtr(ip)) continue;
          if (mode === 'ffield' && isUObject(ip)) continue;
          var kn = kindOf(ip);
          if (kn !== null && kn.length > 8 && kn.lastIndexOf('Property') === kn.length - 8) ih++;
        }
        if (ih / arrayNodes.length >= 0.8) { res.inner = o; break; }
      }
    }
    return res;
  }

  // ---- public derivation -------------------------------------------------

  function derive() {
    var lo = objEnd();
    var hi = 0x200;
    var samples = sampleClasses(48);
    if (samples.length < 8) { C.log('not enough classes indexed to derive UStruct layout'); return false; }

    if (OJBS.offsets.ustruct_super) { off.super_ = parseInt(OJBS.offsets.ustruct_super, 16); tier.super_ = 'operator'; }
    else {
      var s = deriveSuper(samples, lo, hi);
      if (s !== null) { off.super_ = s.offset; tier.super_ = 'derived'; }
    }

    var sizeCands = [];
    if (OJBS.offsets.ustruct_size) { off.size = parseInt(OJBS.offsets.ustruct_size, 16); tier.size = 'operator'; }
    else if (off.super_ !== null) {
      sizeCands = deriveSizeCands(samples, lo, hi);
      if (sizeCands.length > 0) { off.size = sizeCands[0].offset; tier.size = 'derived'; }
    }

    if (OJBS.offsets.ustruct_childprops || OJBS.offsets.ustruct_children) {
      if (OJBS.offsets.ustruct_childprops) {
        off.childProps = parseInt(OJBS.offsets.ustruct_childprops, 16); mode = 'ffield';
      } else {
        off.children = parseInt(OJBS.offsets.ustruct_children, 16); mode = 'ufield';
      }
      tier.child = 'operator';
      off.fieldName = OJBS.offsets.ffield_name ? parseInt(OJBS.offsets.ffield_name, 16) : GObjects.offsets.name;
      off.fieldNext = OJBS.offsets.ffield_next ? parseInt(OJBS.offsets.ffield_next, 16) : null;
    } else {
      var fl = deriveFieldLayout(samples, lo, hi);
      if (fl !== null) {
        mode = fl.mode;
        if (mode === 'ffield') off.childProps = fl.childOff; else off.children = fl.childOff;
        off.fieldName = fl.nameOff;
        off.fieldNext = fl.nextOff;
        tier.child = 'derived';
      }
    }

    if (off.fieldNext === null || off.fieldName === null) {
      C.log('field list layout not derived — property dumping is gated off');
      return false;
    }

    var pnodes = collectPropertyNodes(samples, 64);
    if (pnodes.length < 16) { C.log('too few property nodes reachable to derive FProperty layout'); return false; }

    if (mode === 'ffield') {
      var fc = deriveFieldClass(pnodes.map(function (p) { return p.node; }));
      if (fc !== null) { off.fieldClass = fc.offset; tier.fieldClass = 'derived'; }
    } else {
      tier.fieldClass = 'n/a';
    }

    var propLo = Math.max(off.fieldNext, off.fieldName) + 4;
    var pr = derivePropOffsets(pnodes, propLo, propLo + 0x60);
    if (OJBS.offsets.fproperty_offset) { off.propOffset = parseInt(OJBS.offsets.fproperty_offset, 16); tier.propOffset = 'operator'; }
    else if (pr.best !== null) { off.propOffset = pr.best.offsetOff; tier.propOffset = 'derived'; }
    if (OJBS.offsets.fproperty_elemsize) { off.propElemSize = parseInt(OJBS.offsets.fproperty_elemsize, 16); tier.propElemSize = 'operator'; }
    else if (pr.best !== null) { off.propElemSize = pr.best.elemOff; tier.propElemSize = 'derived'; }

    // refine PropertiesSize against the properties we can now read
    if (off.propOffset !== null && sizeCands.length > 1) {
      var bestSize = null;
      for (var c = 0; c < sizeCands.length; c++) {
        var cand = sizeCands[c].offset, fit = 0, tot = 0;
        for (var i = 0; i < samples.length; i++) {
          var sz = R.i32(samples[i].add(cand));
          if (sz === null || sz <= 0) continue;
          var nodes = fieldList(samples[i], 64);
          for (var j = 0; j < nodes.length; j++) {
            var ov = R.i32(nodes[j].add(off.propOffset));
            if (ov === null) continue;
            tot++;
            if (ov < sz) fit++;
          }
        }
        if (tot >= 16) {
          var sc = fit / tot;
          if (bestSize === null || sc > bestSize.score) bestSize = { offset: cand, score: sc };
        }
      }
      if (bestSize !== null && bestSize.score >= 0.9) { off.size = bestSize.offset; tier.size = 'confirmed'; }
    }

    var extras = derivePointerExtras(pnodes, propLo, propLo + 0x80);
    if (OJBS.offsets.fstructprop_struct) { off.structPropStruct = parseInt(OJBS.offsets.fstructprop_struct, 16); tier.structPropStruct = 'operator'; }
    else if (extras.struct !== null) { off.structPropStruct = extras.struct; tier.structPropStruct = 'derived'; }
    if (OJBS.offsets.farrayprop_inner) { off.arrayPropInner = parseInt(OJBS.offsets.farrayprop_inner, 16); tier.arrayPropInner = 'operator'; }
    else if (extras.inner !== null) { off.arrayPropInner = extras.inner; tier.arrayPropInner = 'derived'; }

    propCache = {};
    return off.propOffset !== null;
  }

  // ---- property access ---------------------------------------------------

  function defaultTypeFor(kind, elemSize) {
    switch (kind) {
      case 'FloatProperty': return 'f32';
      case 'DoubleProperty': return 'f64';
      case 'IntProperty': return 'i32';
      case 'UInt32Property': return 'u32';
      case 'Int64Property': return 'i64';
      case 'UInt64Property': return 'u64';
      case 'Int16Property': return 'i16';
      case 'UInt16Property': return 'u16';
      case 'Int8Property': return 'i8';
      case 'ByteProperty': return 'u8';
      case 'BoolProperty': return 'u8';
      case 'NameProperty': return 'i32';
      case 'ObjectProperty':
      case 'ClassProperty':
      case 'WeakObjectProperty':
      case 'SoftObjectProperty':
      case 'LazyObjectProperty':
      case 'InterfaceProperty': return 'ptr';
      default: return 'bytes:' + Math.min(elemSize || 8, 64);
    }
  }

  function propsOf(structObj) {
    if (structObj === null) return [];
    var key = structObj.toString();
    if (Object.prototype.hasOwnProperty.call(propCache, key)) return propCache[key];
    var out = [];
    var cur = structObj;
    var guard = 0;
    while (cur !== null && guard++ < 24) {
      var nodes = fieldList(cur, OJBS.dump.max_props);
      for (var i = 0; i < nodes.length; i++) {
        var n = nodes[i];
        var nm = fieldName(n);
        if (nm === null) continue;
        var o = off.propOffset === null ? null : R.i32(n.add(off.propOffset));
        if (o === null || o < 0) continue;
        var es = off.propElemSize === null ? null : R.i32(n.add(off.propElemSize));
        var k = kindOf(n);
        var rec = {
          name: nm, offset: o, elemSize: es, kind: k, node: n,
          structObj: null, inner: null
        };
        if (k === 'StructProperty' && off.structPropStruct !== null) {
          var sp = R.pointer(n.add(off.structPropStruct));
          if (sp !== null && isUObject(sp)) rec.structObj = sp;
        }
        if (k === 'ArrayProperty' && off.arrayPropInner !== null) {
          var ip = R.pointer(n.add(off.arrayPropInner));
          if (ip !== null && R.okPtr(ip)) {
            var ik = kindOf(ip);
            var ies = off.propElemSize === null ? null : R.i32(ip.add(off.propElemSize));
            var ist = null;
            if (ik === 'StructProperty' && off.structPropStruct !== null) {
              var isp = R.pointer(ip.add(off.structPropStruct));
              if (isp !== null && isUObject(isp)) ist = isp;
            }
            rec.inner = { kind: ik, elemSize: ies, structObj: ist };
          }
        }
        out.push(rec);
      }
      cur = superOf(cur);
    }
    propCache[key] = out;
    return out;
  }

  function findProp(structObj, name) {
    var list = propsOf(structObj);
    for (var i = 0; i < list.length; i++) if (list[i].name === name) return list[i];
    // tolerate the "_0" style numeric suffix the pool appends
    for (var j = 0; j < list.length; j++) {
      if (list[j].name.indexOf(name + '_') === 0) return list[j];
    }
    return null;
  }

  function structNameOf(structObj) {
    return structObj === null ? null : GObjects.nameOf(structObj);
  }

  // ---- path walking ------------------------------------------------------
  // hop forms: "Prop", "Prop[i]", "*"
  // The trace is a replayable program: the external /proc/mem reader walks the
  // same chain with no engine knowledge at all.
  //   {op:'add',   offset:N}            addr += N
  //   {op:'deref'}                      addr  = *addr
  //   {op:'array', index:i, stride:S}   addr  = (*addr) + i*S
  function walk(rootAddr, rootStruct, path, finalType) {
    var state = { addr: rootAddr, struct: rootStruct };
    var trace = [];
    for (var i = 0; i < path.length; i++) {
      var hop = path[i];
      var last = (i === path.length - 1);
      if (hop === '*') {
        var d = R.pointer(state.addr);
        if (!R.okPtr(d)) return { ok: false, error: 'deref failed at hop ' + i, trace: trace };
        trace.push({ op: 'deref' });
        state = { addr: d, struct: GObjects.classOf(d) };
        if (last) return { ok: true, addr: state.addr, value: state.addr.toString(), type: 'ptr', trace: trace };
        continue;
      }
      var idx = -1;
      var name = hop;
      var br = hop.indexOf('[');
      if (br > 0 && hop.charAt(hop.length - 1) === ']') {
        name = hop.substring(0, br);
        idx = parseInt(hop.substring(br + 1, hop.length - 1), 10);
        if (isNaN(idx) || idx < 0) return { ok: false, error: 'bad index in hop "' + hop + '"', trace: trace };
      }
      if (state.struct === null) return { ok: false, error: 'no struct for hop "' + hop + '"', trace: trace };
      var prop = findProp(state.struct, name);
      if (prop === null) {
        return { ok: false, error: 'property "' + name + '" not found on ' + (structNameOf(state.struct) || '?'), trace: trace };
      }
      var base = state.addr.add(prop.offset);
      trace.push({ op: 'add', offset: prop.offset, hop: name });

      if (idx >= 0) {
        if (prop.kind !== 'ArrayProperty') return { ok: false, error: '"' + name + '" is ' + prop.kind + ', not an array', trace: trace };
        if (prop.inner === null || prop.inner.elemSize === null) {
          return { ok: false, error: 'array inner layout unknown for "' + name + '" — FProperty ElementSize or ArrayProperty Inner not derived', trace: trace };
        }
        var ta = R.tarray(base);
        if (ta === null) return { ok: false, error: 'TArray at "' + name + '" failed validation', trace: trace };
        if (idx >= ta.num) return { ok: false, error: 'index ' + idx + ' past Num=' + ta.num + ' on "' + name + '"', trace: trace };
        var elem = ta.data.add(idx * prop.inner.elemSize);
        if (!R.okPtr(elem)) return { ok: false, error: 'element address unreadable on "' + name + '"', trace: trace };
        trace.push({ op: 'array', index: idx, stride: prop.inner.elemSize });
        var innerKind = prop.inner.kind || '';
        if (innerKind.indexOf('ObjectProperty') >= 0 || innerKind === 'ClassProperty' || innerKind === 'InterfaceProperty') {
          var eo = R.pointer(elem);
          if (!R.okPtr(eo)) return { ok: false, error: 'null element at ' + name + '[' + idx + ']', trace: trace };
          trace.push({ op: 'deref' });
          if (last) return readLeaf(eo, 'ptr', finalType, trace);
          state = { addr: eo, struct: GObjects.classOf(eo) };
        } else {
          if (last) return readLeaf(elem, defaultTypeFor(innerKind, prop.inner.elemSize), finalType, trace);
          state = { addr: elem, struct: prop.inner.structObj };
        }
        continue;
      }

      if (last) return readLeaf(base, defaultTypeFor(prop.kind, prop.elemSize), finalType, trace);

      if (prop.kind === 'StructProperty') {
        state = { addr: base, struct: prop.structObj };
        if (state.struct === null) {
          return { ok: false, error: 'struct type of "' + name + '" unknown — FStructProperty Struct pointer not derived', trace: trace };
        }
        continue;
      }

      var nxt = R.pointer(base);
      if (!R.okPtr(nxt)) return { ok: false, error: 'null or unreadable pointer at "' + name + '"', trace: trace };
      trace.push({ op: 'deref' });
      var cls = GObjects.classOf(nxt);
      if (cls === null) return { ok: false, error: '"' + name + '" does not point at a live object', trace: trace };
      state = { addr: nxt, struct: cls };
    }
    return { ok: false, error: 'empty path', trace: trace };
  }

  function readLeaf(addr, natural, requested, trace) {
    var t = requested || natural;
    var v = R.typed(addr, t);
    if (v === null) return { ok: false, error: 'read failed at ' + addr.toString() + ' as ' + t, trace: trace };
    return { ok: true, addr: addr, value: v, type: t, trace: trace };
  }

  function info() {
    var h = function (v) { return v === null ? null : '0x' + v.toString(16); };
    return {
      mode: mode,
      ustruct: { super: h(off.super_), child_properties: h(off.childProps), children: h(off.children), properties_size: h(off.size) },
      ffield: { next: h(off.fieldNext), name: h(off.fieldName), klass: h(off.fieldClass) },
      fproperty: { offset_internal: h(off.propOffset), element_size: h(off.propElemSize) },
      extras: { structprop_struct: h(off.structPropStruct), arrayprop_inner: h(off.arrayPropInner) },
      tier: tier
    };
  }

  return {
    derive: derive,
    buildIndices: buildIndices,
    findClass: findClass,
    findStruct: findStruct,
    isUObject: isUObject,
    superOf: superOf,
    sizeOf: sizeOf,
    propsOf: propsOf,
    findProp: findProp,
    kindOf: kindOf,
    fieldName: fieldName,
    structNameOf: structNameOf,
    defaultTypeFor: defaultTypeFor,
    walk: walk,
    info: info,
    offsets: off,
    classIndexRef: function () { return classIndex; }
  };
})();
