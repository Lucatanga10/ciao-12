'use strict';

// FName resolution.
//
// The pool is not found with a byte pattern. It is found by decoding the
// candidate and demanding that entry 0 of block 0 is the string "None" —
// an invariant the engine guarantees for every build it has ever shipped.
// Two storage layouts are tried: the block pool (UE4.23+, UE5) and the older
// chunked TNameEntryArray. Whichever validates is reported by name.

var FName = (function () {
  var pool = null;      // { kind, blocks, bits, perChunk }
  var cache = {};

  // ---- UE4.23+ / UE5 block pool -----------------------------------------
  // FNameEntryHeader { uint16 bIsWide:1; uint16 LowercaseProbeHash:5; uint16 Len:10 }
  function decodePoolEntry(entry) {
    var hdr = R.u16(entry);
    if (hdr === null) return null;
    var wide = hdr & 1;
    var len = hdr >> 6;
    if (len <= 0 || len > 1024) return null;
    var s = wide ? R.utf16Fixed(entry.add(2), len) : R.asciiFixed(entry.add(2), len);
    if (s === null || s.length === 0) return null;
    return s;
  }

  function validatePool(blocksAddr, bits) {
    var block0 = R.pointer(blocksAddr);
    if (!R.okPtr(block0)) return null;
    var first = decodePoolEntry(block0);
    if (first !== 'None') return null;
    return { kind: 'pool', blocks: blocksAddr, bits: bits, perChunk: 0 };
  }

  // ---- pre-4.23 TNameEntryArray -----------------------------------------
  // FNameEntry { int32 Index; int32 pad; FNameEntry* HashNext; char Name[] }
  function validateLegacy(chunksAddr) {
    var chunk0 = R.pointer(chunksAddr);
    if (!R.okPtr(chunk0)) return null;
    var entry0 = R.pointer(chunk0);
    if (!R.okPtr(entry0)) return null;
    var idx = R.i32(entry0);
    if (idx === null || (idx >> 1) !== 0) return null;
    var name = R.ascii(entry0.add(0x10), 16);
    if (name !== 'None') return null;
    return { kind: 'legacy', blocks: chunksAddr, bits: 0, perChunk: 16384 };
  }

  function set(found) { pool = found; cache = {}; return pool; }

  function fromConfig() {
    var rvaStr = OJBS.offsets.fnamepool_blocks_rva;
    if (!rvaStr) return null;
    var addr = Mod.fromRva(rvaStr);
    if (addr === null) return null;
    var bits = OJBS.offsets.fname_block_bits;
    var p = validatePool(addr, bits);
    if (p !== null) return set(p);
    p = validateLegacy(addr);
    if (p !== null) return set(p);
    C.log('configured UE_FNAMEPOOL_BLOCKS_RVA did not validate against this build — rescanning');
    return null;
  }

  // Block 0 decodes identically whatever the block-index width is, so the
  // scan proves the pool and not the width. The width is settled later by
  // tuneBits(), against indices that actually cross a block boundary.
  function scan() {
    var ranges = Mod.scanRanges();
    var found = null;
    R.scanPointers(ranges, function (addr) {
      var p = validatePool(addr, OJBS.offsets.fname_block_bits);
      if (p !== null) { found = p; return true; }
      var l = validateLegacy(addr);
      if (l !== null) { found = l; return true; }
      return false;
    });
    if (found === null) return null;
    return set(found);
  }

  function setBits(b) {
    if (pool === null || pool.kind !== 'pool') return false;
    pool.bits = b;
    cache = {};
    return true;
  }

  function bitsCandidates() {
    var cfg = OJBS.offsets.fname_block_bits;
    var list = [cfg];
    var all = [16, 17, 15, 14, 13, 18];
    for (var i = 0; i < all.length; i++) if (all[i] !== cfg) list.push(all[i]);
    return list;
  }

  function resolve() {
    if (pool !== null) return pool;
    var p = fromConfig();
    if (p !== null) return p;
    return scan();
  }

  function entryAddr(index) {
    if (pool === null) return null;
    if (pool.kind === 'pool') {
      var block = index >>> pool.bits;
      var off = index & ((1 << pool.bits) - 1);
      var blockPtr = R.pointer(pool.blocks.add(block * C.PTR_SIZE));
      if (!R.okPtr(blockPtr)) return null;
      return blockPtr.add(off * 2);
    }
    var chunk = R.pointer(pool.blocks.add(Math.floor(index / pool.perChunk) * C.PTR_SIZE));
    if (!R.okPtr(chunk)) return null;
    var e = R.pointer(chunk.add((index % pool.perChunk) * C.PTR_SIZE));
    if (!R.okPtr(e)) return null;
    return e;
  }

  // index -> string, or null. bounded, cached, never throws.
  function get(index) {
    if (pool === null) return null;
    if (index === null || index < 0 || index > 0x7fffffff) return null;
    var key = '' + index;
    if (Object.prototype.hasOwnProperty.call(cache, key)) return cache[key];
    var e = entryAddr(index);
    var s = null;
    if (e !== null) {
      s = (pool.kind === 'pool') ? decodePoolEntry(e) : R.ascii(e.add(0x10), 256);
      if (s !== null && s.length === 0) s = null;
    }
    cache[key] = s;
    return s;
  }

  // FName at an address: { ComparisonIndex:int32, Number:int32 }
  function at(addr) {
    var idx = R.i32(addr);
    if (idx === null) return null;
    var s = get(idx);
    if (s === null) return null;
    var num = R.i32(addr.add(4));
    if (num !== null && num > 0) s = s + '_' + (num - 1);
    return s;
  }

  // a name that could plausibly be an engine identifier
  function sane(s) {
    if (s === null) return false;
    if (s.length < 1 || s.length > 128) return false;
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      var okChar = (c >= 48 && c <= 57) || (c >= 65 && c <= 90) ||
                   (c >= 97 && c <= 122) || c === 95 || c === 45 || c === 46 ||
                   c === 32 || c === 47;
      if (!okChar) return false;
    }
    return true;
  }

  function info() {
    if (pool === null) return null;
    return {
      kind: pool.kind,
      blocks_va: pool.blocks.toString(),
      blocks_rva: Mod.rva(pool.blocks),
      block_bits: pool.bits,
      per_chunk: pool.perChunk
    };
  }

  return {
    resolve: resolve,
    get: get,
    at: at,
    sane: sane,
    info: info,
    setBits: setBits,
    bitsCandidates: bitsCandidates,
    current: function () { return pool; }
  };
})();
