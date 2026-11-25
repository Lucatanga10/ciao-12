'use strict';
var mod = Process.findModuleByName('libUnreal.so');
var gw = mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}
function readable(p){if(!ok(p))return false;try{p.readU8();return true;}catch(e){return false;}}

// PART 1: find RootComponent + RelativeLocation on actors
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('actors='+actN);
var posFound=false;
for(var ai=5;ai<Math.min(20,actN)&&!posFound;ai++){
  var actor=rp(actD.add(ai*8));
  if(!ok(actor)||!readable(actor))continue;
  for(var ro=0x0;ro<=0x500&&!posFound;ro+=8){
    var rc=rp(actor.add(ro));
    if(!readable(rc))continue;
    for(var lo=0x0;lo<=0x300;lo+=8){
      var x=rd(rc.add(lo));
      if(x===null||!isFinite(x))continue;
      var y=rd(rc.add(lo+8));
      var z=rd(rc.add(lo+0x10));
      if(y===null||z===null||!isFinite(y)||!isFinite(z))continue;
      if(x===0&&y===0&&z===0)continue;
      if(Math.abs(x)>1&&Math.abs(x)<500000&&Math.abs(y)<500000&&Math.abs(z)<500000){
        console.log('POS a['+ai+'] rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' xyz='+x.toFixed(0)+','+y.toFixed(0)+','+z.toFixed(0));
        posFound=true;
      }
    }
  }
}

// PART 2: find camera FOV anywhere in the chain
// Use the top GI candidate and walk to PCM, then scan for FOV
var giOffs=[0x170,0x280,0x270];
var lpOffs=[0x168,0x1c8];
var pcOffs=[0x20,0x30,0x78];
for(var g=0;g<giOffs.length;g++){
  var gi=rp(gw.add(giOffs[g]));
  if(!ok(gi))continue;
  for(var l=0;l<lpOffs.length;l++){
    var lpD=rp(gi.add(lpOffs[l]));
    if(!ok(lpD))continue;
    var lpN=ri(gi.add(lpOffs[l]+8));
    if(lpN!==1)continue;
    var lp0=rp(lpD);
    if(!readable(lp0))continue;
    for(var p=0;p<pcOffs.length;p++){
      var pc=rp(lp0.add(pcOffs[p]));
      if(!readable(pc))continue;
      for(var po=0x280;po<=0x400;po+=8){
        var pcm=rp(pc.add(po));
        if(!readable(pcm))continue;
        for(var co=0x0;co<=0x2000;co+=0x10){
          var fov=rd(pcm.add(co+0x30));
          if(fov!==null&&fov>50&&fov<170){
            var lx=rd(pcm.add(co));
            var ly=rd(pcm.add(co+8));
            var lz=rd(pcm.add(co+0x10));
            if(lx!==null&&Math.abs(lx)<500000){
              console.log('CAM GI=W+0x'+giOffs[g].toString(16)+' LP=+0x'+lpOffs[l].toString(16)+' PC=+0x'+pcOffs[p].toString(16)+' PCM=+0x'+po.toString(16)+' pov=+0x'+co.toString(16)+' fov='+fov.toFixed(1)+' loc='+lx.toFixed(0)+','+ly.toFixed(0)+','+lz.toFixed(0));
            }
          }
        }
      }
    }
  }
}
console.log('DONE');
