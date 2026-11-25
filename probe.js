'use strict';
var mod = Process.findModuleByName('libUnreal.so');
var gw = mod.base.add(0x19cec360).readPointer();
console.log('GW=' + gw);

function rp(a) {
  try { return a.readPointer(); } catch(e) { return null; }
}
function ri(a) {
  try { return a.readS32(); } catch(e) { return null; }
}
function ok(p) {
  return p !== null && !p.isNull() && p.compare(ptr(0x10000)) > 0;
}

// scan UWorld for TArrays
for (var o = 0; o <= 0x400; o += 8) {
  var p = rp(gw.add(o));
  if (!ok(p)) continue;
  for (var f = 0x20; f <= 0x200; f += 8) {
    var d = rp(p.add(f));
    var n = ri(p.add(f + 8));
    var mx = ri(p.add(f + 12));
    if (n === null || mx === null) continue;
    if (n > 30 && n < 300000 && mx >= n && mx < 1000000 && ok(d)) {
      console.log('ACTORS? W+0x' + o.toString(16) +
        ' obj+0x' + f.toString(16) +
        ' n=' + n + ' max=' + mx);
    }
    if (n >= 1 && n <= 4 && mx >= n && mx < 100 && ok(d)) {
      var lp0 = rp(d);
      if (ok(lp0)) {
        console.log('SMALL W+0x' + o.toString(16) +
          ' obj+0x' + f.toString(16) +
          ' n=' + n + ' first=' + lp0);
      }
    }
  }
}
console.log('DONE');
