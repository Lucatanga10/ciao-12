'use strict';
var mod=Process.findModuleByName('libUnreal.so');
var gw=mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function rf(a){try{return a.readFloat();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}
function readable(p){if(!ok(p))return false;try{p.readU8();return true;}catch(e){return false;}}
function goodD(v){return v!==null&&isFinite(v)&&Math.abs(v)<1e9;}
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('n='+actN);
var idx=[3,7,11,15,19,23,27];
var actors=[];
for(var t=0;t<idx.length;t++){
  if(idx[t]>=actN){actors.push(null);continue;}
  var a=rp(actD.add(idx[t]*8));
  actors.push(readable(a)?a:null);
}

// A: direct doubles on actor (no pointer hop)
var aF=false;
for(var off=0x0;off<=0x500&&!aF;off+=8){
  var pts=[];
  for(var t=0;t<actors.length;t++){
    if(!actors[t])continue;
    var x=rd(actors[t].add(off));
    if(!goodD(x))continue;
    var y=rd(actors[t].add(off+8)),z=rd(actors[t].add(off+0x10));
    if(!goodD(y)||!goodD(z))continue;
    pts.push([x,y,z,idx[t]]);
  }
  if(pts.length<3)continue;
  for(var i=1;i<pts.length;i++){
    var d=Math.sqrt(Math.pow(pts[i][0]-pts[0][0],2)+Math.pow(pts[i][1]-pts[0][1],2)+Math.pow(pts[i][2]-pts[0][2],2));
    if(d>1){aF=true;break;}
  }
  if(aF){
    console.log('DIRECT +0x'+off.toString(16)+' n='+pts.length);
    for(var i=0;i<pts.length;i++)
      console.log('  a['+pts[i][3]+'] '+pts[i][0].toFixed(1)+','+pts[i][1].toFixed(1)+','+pts[i][2].toFixed(1));
  }
}
if(!aF)console.log('no direct');

// B: through-pointer position with diversity check
var bF=false;
for(var ro=0x0;ro<=0x500&&!bF;ro+=8){
  var rcs=[];
  for(var t=0;t<actors.length;t++){
    if(!actors[t]){rcs.push(null);continue;}
    var rc=rp(actors[t].add(ro));
    rcs.push(readable(rc)?rc:null);
  }
  var anyRC=false;
  for(var t=0;t<rcs.length;t++)if(rcs[t])anyRC=true;
  if(!anyRC)continue;
  for(var lo=0x0;lo<=0x280&&!bF;lo+=8){
    var pts=[];
    for(var t=0;t<rcs.length;t++){
      if(!rcs[t])continue;
      var x=rd(rcs[t].add(lo)),y=rd(rcs[t].add(lo+8)),z=rd(rcs[t].add(lo+0x10));
      if(!goodD(x)||!goodD(y)||!goodD(z))continue;
      pts.push([x,y,z,idx[t]]);
    }
    if(pts.length<3)continue;
    for(var i=1;i<pts.length;i++){
      var d=Math.sqrt(Math.pow(pts[i][0]-pts[0][0],2)+Math.pow(pts[i][1]-pts[0][1],2)+Math.pow(pts[i][2]-pts[0][2],2));
      if(d>1){bF=true;break;}
    }
    if(bF){
      console.log('RC rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' n='+pts.length);
      for(var i=0;i<pts.length;i++)
        console.log('  a['+pts[i][3]+'] '+pts[i][0].toFixed(1)+','+pts[i][1].toFixed(1)+','+pts[i][2].toFixed(1));
    }
  }
}
if(!bF)console.log('no RC pos');

// C: camera via PC->sub->FOV(float) with rotation validation
var gi=rp(gw.add(0x170));
if(!readable(gi)){console.log('no GI');console.log('DONE');}
else{
var lpD=rp(gi.add(0x168));
var lpN=ri(gi.add(0x170));
if(!ok(lpD)||lpN!==1){console.log('no LP');console.log('DONE');}
else{
var lp0=rp(lpD);
if(!readable(lp0)){console.log('no LP0');console.log('DONE');}
else{
var pc=rp(lp0.add(0x20));
if(!readable(pc)){
  pc=rp(lp0.add(0x30));
  if(!readable(pc)){console.log('no PC');console.log('DONE');}
}
if(readable(pc)){
console.log('PC='+pc);
var camDone=false;
for(var po=0x0;po<=0x600&&!camDone;po+=8){
  var sub=rp(pc.add(po));
  if(!readable(sub))continue;
  for(var fo=0x30;fo<=0xA00&&!camDone;fo+=4){
    var fov=rf(sub.add(fo));
    if(fov===null||isNaN(fov)||!isFinite(fov)||fov<60||fov>120)continue;
    var lx=rd(sub.add(fo-0x30));
    if(!goodD(lx))continue;
    var ly=rd(sub.add(fo-0x28)),lz=rd(sub.add(fo-0x20));
    if(!goodD(ly)||!goodD(lz))continue;
    if(lx===0&&ly===0&&lz===0)continue;
    var rp_=rd(sub.add(fo-0x18)),ry=rd(sub.add(fo-0x10)),rr=rd(sub.add(fo-0x8));
    if(!goodD(rp_)||!goodD(ry)||!goodD(rr))continue;
    if(Math.abs(rp_)>360||Math.abs(ry)>360||Math.abs(rr)>360)continue;
    console.log('CAM PC+0x'+po.toString(16)+' fov@sub+0x'+fo.toString(16)+'='+fov.toFixed(1));
    console.log('  loc='+lx.toFixed(1)+','+ly.toFixed(1)+','+lz.toFixed(1));
    console.log('  rot='+rp_.toFixed(1)+','+ry.toFixed(1)+','+rr.toFixed(1));
    camDone=true;
  }
}
if(!camDone)console.log('NO CAM');
console.log('DONE');
}}}}
