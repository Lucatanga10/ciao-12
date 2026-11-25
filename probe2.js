'use strict';
var mod = Process.findModuleByName('libUnreal.so');
var gw = mod.base.add(0x19cec360).readPointer();
function rp(a){try{return a.readPointer();}catch(e){return null;}}
function rd(a){try{return a.readDouble();}catch(e){return null;}}
function ri(a){try{return a.readS32();}catch(e){return null;}}
function ok(p){return p!==null&&!p.isNull()&&p.compare(ptr(0x10000))>0;}

// confirm actors at UWorld+0x20 -> +0xe0
var level = rp(gw.add(0x20));
console.log('Level='+level);
var actD = rp(level.add(0xe0));
var actN = ri(level.add(0xe8));
console.log('Actors data='+actD+' n='+actN);
if(ok(actD) && actN > 0){
  var a0 = rp(actD);
  console.log('actor[0]='+a0);
  if(ok(a0)){
    var rc = rp(a0.add(0x1B0));
    console.log('RootComp(+0x1B0)='+rc);
    if(ok(rc)){
      var x=rd(rc.add(0x140));
      var y=rd(rc.add(0x148));
      var z=rd(rc.add(0x150));
      console.log('pos='+x+','+y+','+z);
    }
  }
}

// find GameInstance: scan UWorld for ptr->obj with TArray(1) that leads to PC->PCM
var giCandidates = [0x110,0x130,0x150,0x170,0x1a8,0x1c8,0x218,0x238,0x270,0x278,0x280,0x2d0,0x2f0,0x2f8,0x318,0x358,0x378,0x3e0,0x3f0];
for(var ci=0;ci<giCandidates.length;ci++){
  var wo = giCandidates[ci];
  var gi = rp(gw.add(wo));
  if(!ok(gi)) continue;
  for(var lpo=0x20;lpo<=0x200;lpo+=8){
    var lpD = rp(gi.add(lpo));
    var lpN = ri(gi.add(lpo+8));
    if(lpN !== 1 || !ok(lpD)) continue;
    var lp0 = rp(lpD);
    if(!ok(lp0)) continue;
    for(var pco=0x20;pco<=0x100;pco+=8){
      var pc = rp(lp0.add(pco));
      if(!ok(pc)) continue;
      for(var pcmo=0x300;pcmo<=0x400;pcmo+=8){
        var pcm = rp(pc.add(pcmo));
        if(!ok(pcm)) continue;
        var cam = rp(pcm.add(0x1590));
        if(cam!==null){
          console.log('CHAIN: GI=W+0x'+wo.toString(16)+' LP=GI+0x'+lpo.toString(16)+' PC=LP+0x'+pco.toString(16)+' PCM=PC+0x'+pcmo.toString(16));
        }
      }
    }
  }
}
console.log('DONE');
