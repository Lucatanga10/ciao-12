'use strict';
var mod = Process.findModuleByName('libUnreal.so');
var gw = mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function rf(a){try{return a.readFloat();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}

// PART 1: validate camera chains by reading FOV
var chains = [
  [0x170,0x168,0x20],[0x170,0x168,0x30],
  [0x270,0x1c8,0x78],[0x270,0x1c8,0xe8],
  [0x280,0x168,0x20],[0x280,0x168,0x30],[0x280,0x168,0xa8],
  [0x2f0,0x98,0x40],[0x2f0,0x98,0x60]
];
for(var ci=0;ci<chains.length;ci++){
  var c=chains[ci];
  var gi=rp(gw.add(c[0])); if(!ok(gi)) continue;
  var lpD=rp(gi.add(c[1])); if(!ok(lpD)) continue;
  var lp0=rp(lpD); if(!ok(lp0)) continue;
  var pc=rp(lp0.add(c[2])); if(!ok(pc)) continue;
  for(var po=0x300;po<=0x400;po+=8){
    var pcm=rp(pc.add(po)); if(!ok(pcm)) continue;
    for(var co=0x1500;co<=0x1700;co+=0x10){
      var fov=rd(pcm.add(co+0x30));
      if(fov===null) continue;
      if(fov>50&&fov<170){
        var lx=rd(pcm.add(co));
        var ly=rd(pcm.add(co+8));
        var lz=rd(pcm.add(co+0x10));
        var rp_=rd(pcm.add(co+0x18));
        var ry=rd(pcm.add(co+0x20));
        console.log('CAM W+0x'+c[0].toString(16)+' GI+0x'+c[1].toString(16)+' LP+0x'+c[2].toString(16)+' PC+0x'+po.toString(16)+' cache+0x'+co.toString(16)+' fov='+fov.toFixed(1)+' loc='+lx.toFixed(0)+','+ly.toFixed(0)+','+lz.toFixed(0)+' rot='+rp_.toFixed(1)+','+ry.toFixed(1));
      }
    }
  }
}

// PART 2: find RootComponent offset on actors
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('actors n='+actN);
for(var ai=1;ai<Math.min(5,actN);ai++){
  var actor=rp(actD.add(ai*8));
  if(!ok(actor)) continue;
  for(var ro=0x100;ro<=0x300;ro+=8){
    var rc=rp(actor.add(ro));
    if(!ok(rc)) continue;
    for(var lo=0x100;lo<=0x200;lo+=8){
      var x=rd(rc.add(lo));
      var y=rd(rc.add(lo+8));
      var z=rd(rc.add(lo+0x10));
      if(x===null||y===null||z===null) continue;
      if(Math.abs(x)<500000&&Math.abs(y)<500000&&Math.abs(z)<500000){
        if(Math.abs(x)>1||Math.abs(y)>1||Math.abs(z)>1){
          console.log('POS a['+ai+'] rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' xyz='+x.toFixed(0)+','+y.toFixed(0)+','+z.toFixed(0));
          lo=0x999;ro=0x999;
        }
      }
    }
  }
}
console.log('DONE');
