'use strict';
var mod=Process.findModuleByName('libUnreal.so');
var gw=mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function rf(a){try{return a.readFloat();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0&&p.compare(ptr(0x7fffffffffff))<0;}
function readable(p){if(!ok(p))return false;try{p.readU8();return true;}catch(e){return false;}}
function goodF(v){return v!==null&&isFinite(v)&&!isNaN(v)&&Math.abs(v)<1e9;}
function goodD(v){return v!==null&&isFinite(v)&&Math.abs(v)<1e9;}

var gi=rp(gw.add(0x170));
var lpD=rp(gi.add(0x168));
var lp0=rp(lpD);
var pc=rp(lp0.add(0x20));
var pawn=rp(pc.add(0x80));
console.log('PC='+pc+' pawn='+pawn);

// A: pawn readable pointers map
var pp=[];
for(var o=0x0;o<=0x600;o+=8){var p=rp(pawn.add(o));if(readable(p))pp.push(o);}
console.log('pawn ptrs('+pp.length+'): '+pp.slice(0,20).map(function(o){return '0x'+o.toString(16);}).join(' '));

// B: float position direct on pawn
var hit=false;
for(var o=0x0;o<=0x800&&!hit;o+=4){
  var x=rf(pawn.add(o)),y=rf(pawn.add(o+4)),z=rf(pawn.add(o+8));
  if(!goodF(x)||!goodF(y)||!goodF(z))continue;
  if(x===0&&y===0&&z===0)continue;
  var m=Math.sqrt(x*x+y*y+z*z);
  if(m>20){console.log('PF +0x'+o.toString(16)+' '+x.toFixed(1)+','+y.toFixed(1)+','+z.toFixed(1));hit=true;}
}
if(!hit)console.log('no PF');

// C: float position through pawn pointers
hit=false;
for(var i=0;i<pp.length&&!hit;i++){
  var rc=rp(pawn.add(pp[i]));
  for(var lo=0x0;lo<=0x400&&!hit;lo+=4){
    var x=rf(rc.add(lo)),y=rf(rc.add(lo+4)),z=rf(rc.add(lo+8));
    if(!goodF(x)||!goodF(y)||!goodF(z))continue;
    if(x===0&&y===0&&z===0)continue;
    var m=Math.sqrt(x*x+y*y+z*z);
    if(m>50){console.log('PRF +0x'+pp[i].toString(16)+' +0x'+lo.toString(16)+' '+x.toFixed(1)+','+y.toFixed(1)+','+z.toFixed(1));hit=true;}
  }
}
if(!hit)console.log('no PRF');

// D: camera - scan PCM candidates at HIGH offsets for float FOV
hit=false;
for(var po=0x200;po<=0x600&&!hit;po+=8){
  var pcm=rp(pc.add(po));
  if(!readable(pcm))continue;
  for(var fo=0xE00;fo<=0x2000&&!hit;fo+=4){
    var fov=rf(pcm.add(fo));
    if(!goodF(fov)||fov<70||fov>100)continue;
    console.log('CAM PCM=PC+0x'+po.toString(16)+' fov@+0x'+fo.toString(16)+'='+fov.toFixed(1));
    for(var d=-0x38;d<=0x8;d+=4){
      var fv=rf(pcm.add(fo+d));
      if(goodF(fv)&&fv!==0)console.log('  '+(d>=0?'+':'')+d+': f='+fv.toFixed(2));
    }
    var lx=rd(pcm.add(fo-0x30)),ly=rd(pcm.add(fo-0x28)),lz=rd(pcm.add(fo-0x20));
    if(goodD(lx))console.log('  d-loc='+lx.toFixed(1)+','+ly.toFixed(1)+','+lz.toFixed(1));
    hit=true;
  }
}
if(!hit)console.log('no high-off cam');

// E: also check if sub at PC+0x58 has float position anywhere
var sub=rp(pc.add(0x58));
hit=false;
for(var o=0x0;o<=0xA00&&!hit;o+=4){
  var x=rf(sub.add(o)),y=rf(sub.add(o+4)),z=rf(sub.add(o+8));
  if(!goodF(x)||!goodF(y)||!goodF(z))continue;
  if(x===0&&y===0&&z===0)continue;
  var m=Math.sqrt(x*x+y*y+z*z);
  if(m>10){console.log('SUBF sub+0x'+o.toString(16)+' '+x.toFixed(1)+','+y.toFixed(1)+','+z.toFixed(1));hit=true;}
}
if(!hit)console.log('no sub float loc');

console.log('DONE');
