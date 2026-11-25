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
function goodF(v){return v!==null&&isFinite(v)&&!isNaN(v)&&Math.abs(v)<1e9;}

console.log('GW='+gw);
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('actors='+actN);

// CAMERA: read PCM+0x1580 (loc), PCM+0x1598 (rot), PCM+0x15B0 (fov)
var gi=rp(gw.add(0x170));
var lpD=rp(gi.add(0x168));
var lp0=rp(lpD);
var pc=rp(lp0.add(0x20));
var pcm=rp(pc.add(0x338));
console.log('PCM='+pcm);
var cx=rd(pcm.add(0x1580)),cy=rd(pcm.add(0x1588)),cz=rd(pcm.add(0x1590));
var rp_=rd(pcm.add(0x1598)),ry=rd(pcm.add(0x15a0)),rr=rd(pcm.add(0x15a8));
var fov=rf(pcm.add(0x15b0));
console.log('CAM loc='+cx+','+cy+','+cz);
console.log('CAM rot='+rp_+','+ry+','+rr);
console.log('CAM fov='+fov);

// POSITION: scan actors for doubles at wide range
var idx=[];
for(var s=5;s<Math.min(actN,200);s+=10)idx.push(s);
var actors=[];
for(var i=0;i<idx.length;i++){
  var a=rp(actD.add(idx[i]*8));
  actors.push(readable(a)?a:null);
}

// A: direct doubles on actor 0x0-0x1000
var aF=false;
for(var off=0x0;off<=0x1000&&!aF;off+=8){
  var pts=[];
  for(var i=0;i<actors.length;i++){
    if(!actors[i])continue;
    var x=rd(actors[i].add(off)),y=rd(actors[i].add(off+8)),z=rd(actors[i].add(off+0x10));
    if(!goodD(x)||!goodD(y)||!goodD(z))continue;
    pts.push([x,y,z,idx[i]]);
  }
  if(pts.length<3)continue;
  for(var j=1;j<pts.length;j++){
    var d=Math.sqrt(Math.pow(pts[j][0]-pts[0][0],2)+Math.pow(pts[j][1]-pts[0][1],2)+Math.pow(pts[j][2]-pts[0][2],2));
    if(d>5){aF=true;break;}
  }
  if(aF){
    console.log('DIR +0x'+off.toString(16)+' n='+pts.length);
    for(var j=0;j<Math.min(5,pts.length);j++)
      console.log('  a['+pts[j][3]+'] '+pts[j][0].toFixed(1)+','+pts[j][1].toFixed(1)+','+pts[j][2].toFixed(1));
  }
}
if(!aF)console.log('no DIR');

// B: through-pointer doubles, actor 0x0-0x800, loc 0x0-0x300
var bF=false;
for(var ro=0x0;ro<=0x800&&!bF;ro+=8){
  var rcs=[];
  for(var i=0;i<actors.length;i++){
    if(!actors[i]){rcs.push(null);continue;}
    var rc=rp(actors[i].add(ro));
    rcs.push(readable(rc)?rc:null);
  }
  var any=false;for(var i=0;i<rcs.length;i++)if(rcs[i])any=true;
  if(!any)continue;
  for(var lo=0x0;lo<=0x300&&!bF;lo+=8){
    var pts=[];
    for(var i=0;i<rcs.length;i++){
      if(!rcs[i])continue;
      var x=rd(rcs[i].add(lo)),y=rd(rcs[i].add(lo+8)),z=rd(rcs[i].add(lo+0x10));
      if(!goodD(x)||!goodD(y)||!goodD(z))continue;
      pts.push([x,y,z,idx[i]]);
    }
    if(pts.length<3)continue;
    for(var j=1;j<pts.length;j++){
      var d=Math.sqrt(Math.pow(pts[j][0]-pts[0][0],2)+Math.pow(pts[j][1]-pts[0][1],2)+Math.pow(pts[j][2]-pts[0][2],2));
      if(d>5){bF=true;break;}
    }
    if(bF){
      console.log('RC rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' n='+pts.length);
      for(var j=0;j<Math.min(5,pts.length);j++)
        console.log('  a['+pts[j][3]+'] '+pts[j][0].toFixed(1)+','+pts[j][1].toFixed(1)+','+pts[j][2].toFixed(1));
    }
  }
}
if(!bF)console.log('no RC');

// C: float position (direct on actor)
var cF=false;
for(var off=0x0;off<=0x800&&!cF;off+=4){
  var pts=[];
  for(var i=0;i<actors.length;i++){
    if(!actors[i])continue;
    var x=rf(actors[i].add(off)),y=rf(actors[i].add(off+4)),z=rf(actors[i].add(off+8));
    if(!goodF(x)||!goodF(y)||!goodF(z))continue;
    pts.push([x,y,z,idx[i]]);
  }
  if(pts.length<3)continue;
  for(var j=1;j<pts.length;j++){
    var d=Math.sqrt(Math.pow(pts[j][0]-pts[0][0],2)+Math.pow(pts[j][1]-pts[0][1],2)+Math.pow(pts[j][2]-pts[0][2],2));
    if(d>5){cF=true;break;}
  }
  if(cF){
    console.log('FLT +0x'+off.toString(16)+' n='+pts.length);
    for(var j=0;j<Math.min(5,pts.length);j++)
      console.log('  a['+pts[j][3]+'] '+pts[j][0].toFixed(1)+','+pts[j][1].toFixed(1)+','+pts[j][2].toFixed(1));
  }
}
if(!cF)console.log('no FLT');

console.log('DONE');
