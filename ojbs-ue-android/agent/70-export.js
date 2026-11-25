'use strict';

// file output from inside the target.
//
// The agent runs with the game's own uid, so it writes into the package's
// private directory and the shell copies the results out as root afterwards.
// Every artifact is finished with a marker file so the host side can tell a
// completed run from a truncated one instead of parsing half a dump.

var Out = (function () {
  var dir = OJBS.out_dir;
  var ready = false;

  function ensure() {
    if (ready) return true;
    C.mkdirp(dir);
    ready = true;
    return true;
  }

  function open(name) {
    ensure();
    var path = dir + '/' + name;
    var f;
    try { f = new File(path, 'w'); } catch (e) { C.log('cannot open ' + path + ': ' + e); return null; }
    return { file: f, path: path, lines: 0 };
  }

  function append(name) {
    ensure();
    var path = dir + '/' + name;
    var f;
    try { f = new File(path, 'a'); } catch (e) { C.log('cannot append ' + path + ': ' + e); return null; }
    return { file: f, path: path, lines: 0 };
  }

  function line(h, obj) {
    if (h === null) return;
    try {
      h.file.write(JSON.stringify(obj) + '\n');
      h.lines++;
      if ((h.lines % 2000) === 0) h.file.flush();
    } catch (e) { C.log('write failed on ' + h.path + ': ' + e); }
  }

  function raw(h, text) {
    if (h === null) return;
    try { h.file.write(text); } catch (e) { C.log('write failed on ' + h.path + ': ' + e); }
  }

  function close(h) {
    if (h === null) return;
    try { h.file.flush(); } catch (e) { /* closing anyway */ }
    try { h.file.close(); } catch (e) { /* already gone */ }
    C.chmod777(h.path);
  }

  function json(name, obj) {
    var h = open(name);
    if (h === null) return false;
    raw(h, JSON.stringify(obj, null, 2) + '\n');
    close(h);
    return true;
  }

  function marker(name, obj) {
    return json(name, obj);
  }

  return { open: open, append: append, line: line, raw: raw, close: close, json: json, marker: marker, dir: dir };
})();
