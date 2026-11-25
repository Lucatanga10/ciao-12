'use strict';
var mod=Process.findModuleByName('libUnreal.so');
var gw=mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function rf(a){try{return a.readFloat();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}
function readable(p){if(!ok(p))return false;try{p.readU8();return true;}catch(e){return false;}}
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('n='+actN);

// PART A: direct doubles on actor memory (no pointer indirection)
var dirHits=0;
for(var ai=3;ai<Math.min(20,actN)&&dirHits<3;ai++){
  var actor=rp(actD.add(ai*8));
  if(!readable(actor))continue;
  for(var off=0x80;off<=0x500;off+=8){
    var x=rd(actor.add(off));
    if(x===null||!isFinite(x))continue;
    var y=rd(actor.add(off+8));
    var z=rd(actor.add(off+0x10));
    if(y===null||z===null||!isFinite(y)||!isFinite(z))continue;
    var mag=Math.sqrt(x*x+y*y+z*z);
    if(mag>50&&mag<2000000){
      console.log('DIR a['+ai+'] +0x'+off.toString(16)+' xyz='+x.toFixed(1)+','+y.toFixed(1)+','+z.toFixed(1)+' mag='+mag.toFixed(0));
      dirHits++;off=0x999;
    }
  }
}

// PART B: through-pointer position scan with counting
var testA=[5,10,15,20,25];
var best={c:0};
for(var ro=0x0;ro<=0x500;ro+=8){
  for(var lo=0x80;lo<=0x280;lo+=8){
    var c=0;
    for(var t=0;t<testA.length;t++){
      if(testA[t]>=actN)continue;
      var actor=rp(actD.add(testA[t]*8));
      if(!readable(actor))continue;
      var rc=rp(actor.add(ro));
      if(!readable(rc))continue;
      var x=rd(rc.add(lo)),y=rd(rc.add(lo+8)),z=rd(rc.add(lo+0x10));
      if(x===null||y===null||z===null)continue;
      if(!isFinite(x)||!isFinite(y)||!isFinite(z))continue;
      if(Math.sqrt(x*x+y*y+z*z)>50)c++;
    }
    if(c>best.c)best={c:c,ro:ro,lo:lo};
  }
}
if(best.c>=2){
  console.log('RC rc=+0x'+best.ro.toString(16)+' loc=+0x'+best.lo.toString(16)+' cnt='+best.c);
  for(var t=0;t<testA.length;t++){
    if(testA[t]>=actN)continue;
    var actor=rp(actD.add(testA[t]*8));
    if(!readable(actor))continue;
    var rc=rp(actor.add(best.ro));
    if(!readable(rc))continue;
    var x=rd(rc.add(best.lo)),y=rd(rc.add(best.lo+8)),z=rd(rc.add(best.lo+0x10));
    if(x!==null&&isFinite(x))console.log('  a['+testA[t]+'] '+x.toFixed(0)+','+y.toFixed(0)+','+z.toFixed(0));
  }
}else console.log('RC best='+best.c);

// PART C: camera - float FOV scan through chain
var camDone=false;
var giList=[[0x170,0x168],[0x280,0x168],[0x270,0x1c8]];
for(var g=0;g<giList.length&&!camDone;g++){
  var gi=rp(gw.add(giList[g][0]));
  if(!readable(gi))continue;
  var lpD=rp(gi.add(giList[g][1]));
  if(!ok(lpD))continue;
  if(ri(gi.add(giList[g][1]+8))!==1)continue;
  var lp0=rp(lpD);
  if(!readable(lp0))continue;
  for(var pco=0x20;pco<=0x100&&!camDone;pco+=8){
    var pc=rp(lp0.add(pco));
    if(!readable(pc))continue;
    for(var po=0x200;po<=0x500&&!camDone;po+=8){
      var pcm=rp(pc.add(po));
      if(!readable(pcm))continue;
      for(var fo=0x30;fo<=0xC00;fo+=4){
        var fov=rf(pcm.add(fo));
        if(fov===null||fov<60||fov>120)continue;
        var lx=rd(pcm.add(fo-0x30));
        if(lx===null||!isFinite(lx))continue;
        var ly=rd(pcm.add(fo-0x28));
        var lz=rd(pcm.add(fo-0x20));
        if(ly===null||lz===null)continue;
        var m=Math.sqrt(lx*lx+ly*ly+lz*lz);
        if(m<10)continue;
        console.log('CAM GI=W+0x'+giList[g][0].toString(16)+' LP=GI+0x'+giList[g][1].toString(16)+' PC=LP+0x'+pco.toString(16)+' PCM=PC+0x'+po.toString(16)+' fov@+0x'+fo.toString(16)+'='+fov.toFixed(1)+' loc='+lx.toFixed(0)+','+ly.toFixed(0)+','+lz.toFixed(0));
        camDone=true;break;
      }
    }
  }
}
if(!camDone)console.log('NO CAM');
console.log('DONE');
