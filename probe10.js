'use strict';
var mod=Process.findModuleByName('libUnreal.so');
var gw=mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rf(a){try{return a.readFloat();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}
function readable(p){if(!ok(p))return false;try{p.readU8();return true;}catch(e){return false;}}
function goodF(v){return v!==null&&isFinite(v)&&!isNaN(v)&&Math.abs(v)<1e9;}

var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
console.log('n='+actN);

var idx=[];
for(var s=3;s<Math.min(actN,60);s+=3)idx.push(s);
var actors=[];
for(var i=0;i<idx.length;i++){
  var a=rp(actD.add(idx[i]*8));
  actors.push(readable(a)?a:null);
}
var nReadable=0;for(var i=0;i<actors.length;i++)if(actors[i])nReadable++;
console.log('test actors='+idx.length+' readable='+nReadable);

// THROUGH-POINTER FLOAT SCAN
// This is the test we never ran: actor->pointer->3 floats
var found=false;
for(var ro=0x0;ro<=0x400&&!found;ro+=8){
  var rcs=[];
  for(var i=0;i<actors.length;i++){
    if(!actors[i]){rcs.push(null);continue;}
    var rc=rp(actors[i].add(ro));
    rcs.push(readable(rc)?rc:null);
  }
  var cnt=0;for(var i=0;i<rcs.length;i++)if(rcs[i])cnt++;
  if(cnt<3)continue;

  for(var lo=0x0;lo<=0x200&&!found;lo+=4){
    var pts=[];
    for(var i=0;i<rcs.length;i++){
      if(!rcs[i])continue;
      var x=rf(rcs[i].add(lo)),y=rf(rcs[i].add(lo+4)),z=rf(rcs[i].add(lo+8));
      if(!goodF(x)||!goodF(y)||!goodF(z))continue;
      if(x===0&&y===0&&z===0)continue;
      pts.push([x,y,z,idx[i]]);
    }
    if(pts.length<3)continue;
    for(var j=1;j<pts.length;j++){
      var d=Math.sqrt(Math.pow(pts[j][0]-pts[0][0],2)+Math.pow(pts[j][1]-pts[0][1],2)+Math.pow(pts[j][2]-pts[0][2],2));
      if(d>1){found=true;break;}
    }
    if(found){
      console.log('FLOAT-PTR rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' n='+pts.length);
      for(var j=0;j<Math.min(8,pts.length);j++)
        console.log('  a['+pts[j][3]+'] '+pts[j][0].toFixed(1)+','+pts[j][1].toFixed(1)+','+pts[j][2].toFixed(1));
    }
  }
}
if(!found)console.log('no float-ptr');

// Also try ComponentToWorld.Translation (doubles through pointer)
// at higher offsets on the component: 0x1A0-0x280
var found2=false;
for(var ro=0x100;ro<=0x200&&!found2;ro+=8){
  var rcs=[];
  for(var i=0;i<actors.length;i++){
    if(!actors[i]){rcs.push(null);continue;}
    var rc=rp(actors[i].add(ro));
    rcs.push(readable(rc)?rc:null);
  }
  var cnt=0;for(var i=0;i<rcs.length;i++)if(rcs[i])cnt++;
  if(cnt<3)continue;
  for(var lo=0x1A0;lo<=0x280&&!found2;lo+=8){
    var pts=[];
    for(var i=0;i<rcs.length;i++){
      if(!rcs[i])continue;
      var x=rd(rcs[i].add(lo)),y=rd(rcs[i].add(lo+8)),z=rd(rcs[i].add(lo+0x10));
      if(x===null||y===null||z===null)continue;
      if(!isFinite(x)||!isFinite(y)||!isFinite(z))continue;
      if(Math.abs(x)>1e9||Math.abs(y)>1e9||Math.abs(z)>1e9)continue;
      if(x===0&&y===0&&z===0)continue;
      pts.push([x,y,z,idx[i]]);
    }
    if(pts.length<3)continue;
    for(var j=1;j<pts.length;j++){
      var d=Math.sqrt(Math.pow(pts[j][0]-pts[0][0],2)+Math.pow(pts[j][1]-pts[0][1],2)+Math.pow(pts[j][2]-pts[0][2],2));
      if(d>1){found2=true;break;}
    }
    if(found2){
      console.log('DBL-PTR rc=+0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' n='+pts.length);
      for(var j=0;j<Math.min(8,pts.length);j++)
        console.log('  a['+pts[j][3]+'] '+pts[j][0].toFixed(1)+','+pts[j][1].toFixed(1)+','+pts[j][2].toFixed(1));
    }
  }
}
if(!found2)console.log('no dbl-ptr high');

console.log('DONE');
