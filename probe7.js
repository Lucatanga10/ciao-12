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

var gi=rp(gw.add(0x170));
var lpD=rp(gi.add(0x168));
var lp0=rp(lpD);
var pc=rp(lp0.add(0x20));
var sub=rp(pc.add(0x58));
console.log('PC='+pc+' sub='+sub);

// A: dump doubles+floats around FOV(sub+0x538)
for(var off=0x4E0;off<0x570;off+=8){
  var d=rd(sub.add(off));
  var f0=rf(sub.add(off));
  var f1=rf(sub.add(off+4));
  var s='+0x'+off.toString(16)+':';
  if(d!==null&&d!==0&&isFinite(d))s+=' d='+d.toFixed(2);
  if(f0!==null&&f0!==0&&isFinite(f0))s+=' f0='+f0.toFixed(2);
  if(f1!==null&&f1!==0&&isFinite(f1))s+=' f1='+f1.toFixed(2);
  if(s.length>10)console.log(s);
}

// B: scan sub 0x0-0xA00 for 3 non-zero doubles (camera location?)
var lHits=0;
for(var off=0x0;off<=0xA00&&lHits<5;off+=8){
  var x=rd(sub.add(off)),y=rd(sub.add(off+8)),z=rd(sub.add(off+0x10));
  if(!goodD(x)||!goodD(y)||!goodD(z))continue;
  if(x===0&&y===0&&z===0)continue;
  var m=Math.sqrt(x*x+y*y+z*z);
  if(m>10){
    console.log('LOC? sub+0x'+off.toString(16)+' '+x.toFixed(1)+','+y.toFixed(1)+','+z.toFixed(1));
    lHits++;
  }
}
if(!lHits)console.log('no loc on sub');

// C: find pawn = PC pointer that matches an actor
var level=rp(gw.add(0x20));
var actD=rp(level.add(0xe0));
var actN=ri(level.add(0xe8));
var aSet={};
for(var i=0;i<actN;i++){var a=rp(actD.add(i*8));if(a)aSet[a.toString()]=i;}
var pawn=null,pawnIdx=-1;
for(var off=0x0;off<=0x400;off+=8){
  var p=rp(pc.add(off));
  if(!p)continue;
  var k=p.toString();
  if(k in aSet){
    console.log('PAWN PC+0x'+off.toString(16)+' actor['+aSet[k]+']');
    pawn=p;pawnIdx=aSet[k];
    break;
  }
}
if(!pawn){console.log('no pawn');console.log('DONE');throw '';}

// D: scan pawn for position (through pointer and direct)
// direct on pawn
for(var off=0x0;off<=0x500;off+=8){
  var x=rd(pawn.add(off)),y=rd(pawn.add(off+8)),z=rd(pawn.add(off+0x10));
  if(!goodD(x)||!goodD(y)||!goodD(z))continue;
  if(x===0&&y===0&&z===0)continue;
  if(Math.abs(x)>1||Math.abs(y)>1||Math.abs(z)>1){
    console.log('PAWN-DIR +0x'+off.toString(16)+' '+x.toFixed(2)+','+y.toFixed(2)+','+z.toFixed(2));
    break;
  }
}
// through pointer on pawn
for(var ro=0x0;ro<=0x500;ro+=8){
  var rc=rp(pawn.add(ro));
  if(!readable(rc))continue;
  for(var lo=0x0;lo<=0x280;lo+=8){
    var x=rd(rc.add(lo)),y=rd(rc.add(lo+8)),z=rd(rc.add(lo+0x10));
    if(!goodD(x)||!goodD(y)||!goodD(z))continue;
    if(x===0&&y===0&&z===0)continue;
    if(Math.abs(x)>1||Math.abs(y)>1||Math.abs(z)>1){
      console.log('PAWN-RC +0x'+ro.toString(16)+' loc=+0x'+lo.toString(16)+' '+x.toFixed(2)+','+y.toFixed(2)+','+z.toFixed(2));
      lo=0x999;ro=0x999;
    }
  }
}
console.log('DONE');
