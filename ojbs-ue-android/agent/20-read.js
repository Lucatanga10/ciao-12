'use strict';

// validated reads. every accessor returns null on failure — a partial or
// faulting read is a failure, never a zero. pointer plausibility is checked
// against a sorted index of readable ranges built once per run.

var R = (function () {
  var MIN = ptr(OJBS.limits.min_pointer);
  var MAX = ptr(OJBS.limits.max_pointer);
  var MAXF = OJBS.limits.max_abs_float;

  var idxBase = [];
  var idxEnd = [];

  function buildIndex() {
    idxBase = [];
    idxEnd = [];
    var rs = Process.enumerateRanges('r--');
    rs.sort(function (a, b) { return a.base.compare(b.base); });
    for (var i = 0; i < rs.length; i++) {
      idxBase.push(C.num(rs[i].base));
      idxEnd.push(C.num(rs[i].base) + rs[i].size);
    }
  }

  function readableNum(n) {
    var lo = 0, hi = idxBase.length - 1;
    while (lo <= hi) {
      var mid = (lo + hi) >> 1;
      if (n < idxBase[mid]) hi = mid - 1;
      else if (n >= idxEnd[mid]) lo = mid + 1;
      else return true;
    }
    return false;
  }

  function okPtr(p) {
    if (p === null || p === undefined || p.isNull()) return false;
    if (p.compare(MIN) < 0 || p.compare(MAX) > 0) return false;
    return readableNum(C.num(p));
  }

  function u8(p)  { try { return p.readU8();  } catch (e) { return null; } }
  function i8(p)  { try { return p.readS8();  } catch (e) { return null; } }
  function u16(p) { try { return p.readU16(); } catch (e) { return null; } }
  function i16(p) { try { return p.readS16(); } catch (e) { return null; } }
  function u32(p) { try { return p.readU32(); } catch (e) { return null; } }
  function i32(p) { try { return p.readS32(); } catch (e) { return null; } }
  function u64(p) { try { return p.readU64().toString(); } catch (e) { return null; } }
  function i64(p) { try { return p.readS64().toString(); } catch (e) { return null; } }
  function pointer(p) { try { return p.readPointer(); } catch (e) { return null; } }

  function f32(p) {
    var v;
    try { v = p.readFloat(); } catch (e) { return null; }
    if (!isFinite(v)) return null;
    if (v !== 0 && Math.abs(v) > MAXF) return null;
    return v;
  }

  function f64(p) {
    var v;
    try { v = p.readDouble(); } catch (e) { return null; }
    if (!isFinite(v)) return null;
    if (v !== 0 && Math.abs(v) > MAXF) return null;
    return v;
  }

  function vec(p, n) {
    var out = [];
    for (var i = 0; i < n; i++) {
      var v = f32(p.add(i * 4));
      if (v === null) return null;
      out.push(v);
    }
    return out;
  }

  function bytes(p, n) {
    var buf;
    try { buf = p.readByteArray(n); } catch (e) { return null; }
    if (buf === null) return null;
    var view = new Uint8Array(buf);
    if (view.length !== n) return null;
    var s = '';
    for (var i = 0; i < n; i++) {
      var h = view[i].toString(16);
      s += (h.length === 1 ? '0' : '') + h;
    }
    return s;
  }

  // ascii only, bounded, rejects anything with a non-printable byte
  function ascii(p, maxLen) {
    var out = '';
    for (var i = 0; i < maxLen; i++) {
      var c = u8(p.add(i));
      if (c === null) return null;
      if (c === 0) break;
      if (c < 0x20 || c > 0x7e) return null;
      out += String.fromCharCode(c);
    }
    return out;
  }

  function asciiFixed(p, len) {
    var out = '';
    for (var i = 0; i < len; i++) {
      var c = u8(p.add(i));
      if (c === null) return null;
      if (c < 0x20 || c > 0x7e) return null;
      out += String.fromCharCode(c);
    }
    return out;
  }

  function utf16Fixed(p, len) {
    var out = '';
    for (var i = 0; i < len; i++) {
      var c = u16(p.add(i * 2));
      if (c === null) return null;
      if (c === 0) break;
      if (c > 0xffff) return null;
      out += String.fromCharCode(c);
    }
    return out;
  }

  function typed(addr, type) {
    if (type.indexOf('bytes:') === 0) return bytes(addr, parseInt(type.substring(6), 10));
    switch (type) {
      case 'i8':   return i8(addr);
      case 'u8':   return u8(addr);
      case 'bool': { var b = u8(addr); return b === null ? null : (b !== 0); }
      case 'i16':  return i16(addr);
      case 'u16':  return u16(addr);
      case 'i32':  return i32(addr);
      case 'u32':  return u32(addr);
      case 'i64':  return i64(addr);
      case 'u64':  return u64(addr);
      case 'f32':  return f32(addr);
      case 'f64':  return f64(addr);
      case 'ptr':  { var p = pointer(addr); return p === null ? null : p.toString(); }
      case 'vec2': return vec(addr, 2);
      case 'vec3': return vec(addr, 3);
      case 'vec4': return vec(addr, 4);
      case 'tarray_num': {
        var t = tarray(addr);
        return t === null ? null : t.num;
      }
      default: return null;
    }
  }

  // TArray<T> { T* Data; int32 Num; int32 Max } — accepted only when the
  // triplet is internally consistent, so a wrong offset reports null instead
  // of handing back a plausible-looking count.
  function tarray(addr) {
    var data = pointer(addr);
    var num = i32(addr.add(C.PTR_SIZE));
    var max = i32(addr.add(C.PTR_SIZE + 4));
    if (num === null || max === null) return null;
    if (num < 0 || max < 0 || num > max) return null;
    if (max > 0x1000000) return null;
    if (num > 0 && !okPtr(data)) return null;
    return { data: data, num: num, max: max };
  }

  // sweep writable ranges for slots holding a plausible pointer.
  // chunked byte-array reads keep this fast enough to cover a few hundred MB.
  function scanPointers(ranges, onCandidate) {
    var CHUNK = 0x40000;
    var stride = OJBS.scan.stride;
    var minN = C.num(MIN);
    var maxN = C.num(MAX);
    var hits = 0;
    for (var r = 0; r < ranges.length; r++) {
      var base = ranges[r].base;
      var size = ranges[r].size;
      for (var off = 0; off < size; off += CHUNK) {
        var len = Math.min(CHUNK, size - off);
        if (len < 8) break;
        var buf;
        try { buf = base.add(off).readByteArray(len); } catch (e) { continue; }
        if (buf === null) continue;
        var dv = new DataView(buf);
        for (var i = 0; i + 8 <= len; i += stride) {
          var lo = dv.getUint32(i, true);
          var hi = dv.getUint32(i + 4, true);
          if (hi > 0x7fff) continue;
          var v = hi * 4294967296 + lo;
          if (v < minN || v > maxN) continue;
          if (!readableNum(v)) continue;
          if (onCandidate(base.add(off + i), ptr(v)) === true) {
            hits++;
            if (hits >= OJBS.scan.max_hits) return hits;
          }
        }
      }
    }
    return hits;
  }

  return {
    buildIndex: buildIndex,
    okPtr: okPtr,
    readableNum: readableNum,
    u8: u8, i8: i8, u16: u16, i16: i16, u32: u32, i32: i32,
    u64: u64, i64: i64, f32: f32, f64: f64,
    pointer: pointer, vec: vec, bytes: bytes,
    ascii: ascii, asciiFixed: asciiFixed, utf16Fixed: utf16Fixed,
    typed: typed, tarray: tarray, scanPointers: scanPointers
  };
})();
