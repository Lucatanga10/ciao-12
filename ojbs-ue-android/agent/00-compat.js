'use strict';

// frida API compatibility shim. the 16.x and 17.x export-lookup APIs differ,
// and frida-inject on a device may be either. everything below probes first
// and never assumes which runtime it landed in.

var C = (function () {
  var PTR_SIZE = Process.pointerSize;

  function findModule(name) {
    if (typeof Process.findModuleByName === 'function') {
      try {
        var m = Process.findModuleByName(name);
        if (m !== null) return m;
      } catch (e) { /* fall through */ }
    }
    var mods = Process.enumerateModules();
    for (var i = 0; i < mods.length; i++) {
      if (mods[i].name === name) return mods[i];
    }
    return null;
  }

  function globalExport(name) {
    if (typeof Module.findGlobalExportByName === 'function') {
      try {
        var a = Module.findGlobalExportByName(name);
        if (a !== null) return a;
      } catch (e) { /* fall through */ }
    }
    if (typeof Module.getGlobalExportByName === 'function') {
      try { return Module.getGlobalExportByName(name); } catch (e) { /* fall through */ }
    }
    if (typeof Module.findExportByName === 'function') {
      try { return Module.findExportByName(null, name); } catch (e) { /* fall through */ }
    }
    return null;
  }

  function libcFn(name, ret, args) {
    var addr = globalExport(name);
    if (addr === null || addr.isNull()) return null;
    try { return new NativeFunction(addr, ret, args); } catch (e) { return null; }
  }

  var _mkdir = libcFn('mkdir', 'int', ['pointer', 'int']);
  var _rename = libcFn('rename', 'int', ['pointer', 'pointer']);
  var _chmod = libcFn('chmod', 'int', ['pointer', 'int']);

  function mkdirp(path) {
    if (_mkdir === null) return false;
    var parts = path.split('/');
    var cur = '';
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] === '') { cur = cur === '' ? '/' : cur; continue; }
      cur = (cur === '/' || cur === '') ? (cur + parts[i]) : (cur + '/' + parts[i]);
      try { _mkdir(Memory.allocUtf8String(cur), 511); } catch (e) { /* exists or denied */ }
    }
    return true;
  }

  function chmod777(path) {
    if (_chmod === null) return false;
    try { _chmod(Memory.allocUtf8String(path), 511); return true; } catch (e) { return false; }
  }

  function renameFile(from, to) {
    if (_rename === null) return false;
    try { return _rename(Memory.allocUtf8String(from), Memory.allocUtf8String(to)) === 0; }
    catch (e) { return false; }
  }

  function num(p) {
    // NativePointer -> Number. exact below 2^53, which covers every
    // user-space address on arm64 android.
    var s = p.toString();
    if (s.indexOf('0x') === 0) s = s.substring(2);
    return parseInt(s, 16);
  }

  function hex(n) {
    if (typeof n === 'number') return '0x' + n.toString(16);
    return n.toString();
  }

  function nowIso() { return new Date().toISOString(); }

  function log(msg) {
    console.log('[ojbs] ' + msg);
  }

  return {
    PTR_SIZE: PTR_SIZE,
    findModule: findModule,
    globalExport: globalExport,
    libcFn: libcFn,
    mkdirp: mkdirp,
    chmod777: chmod777,
    renameFile: renameFile,
    num: num,
    hex: hex,
    nowIso: nowIso,
    log: log
  };
})();
