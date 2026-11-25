'use strict';

// Example for: bin/ojbs.sh run examples/user-example.js
//
// The toolkit is already up when this runs: name pool, object array, UObject
// offsets and the reflection layout have all been derived against the live
// build. Everything below addresses the game by property name — no numeric
// offset appears here, because none of them survive a patch.
//
// Define ojbs_user_main(api). Whatever it returns lands in out/derived.json
// under "user".

function ojbs_user_main(api) {
  var GObjects = api.GObjects;
  var Reflect_ = api.Reflect;
  var World = api.World;
  var Out = api.Out;
  var R = api.R;

  var world = World.resolveWorld();
  if (world === null) return { error: 'no UWorld' };

  function prop(obj, name) {
    var cls = GObjects.classOf(obj);
    if (cls === null) return null;
    return Reflect_.findProp(cls, name);
  }

  function follow(obj, name) {
    var p = prop(obj, name);
    if (p === null) return null;
    var next = R.pointer(obj.add(p.offset));
    if (!R.okPtr(next) || !Reflect_.isUObject(next)) return null;
    return next;
  }

  function readVec(obj, name) {
    var p = prop(obj, name);
    if (p === null) return null;
    return R.vec(obj.add(p.offset), 3);
  }

  // local chain, resolved by name on this build
  var gi = follow(world, 'OwningGameInstance');
  var localPlayer = null;
  if (gi !== null) {
    var lpProp = prop(gi, 'LocalPlayers');
    if (lpProp !== null) {
      var ta = R.tarray(gi.add(lpProp.offset));
      if (ta !== null && ta.num > 0) {
        var first = R.pointer(ta.data);
        if (R.okPtr(first) && Reflect_.isUObject(first)) localPlayer = first;
      }
    }
  }
  var pc = localPlayer === null ? null : follow(localPlayer, 'PlayerController');
  var pawn = pc === null ? null : follow(pc, 'AcknowledgedPawn');
  var root = pawn === null ? null : follow(pawn, 'RootComponent');

  var report = {
    world: GObjects.fullNameOf(world),
    game_instance: gi === null ? null : GObjects.fullNameOf(gi),
    local_player: localPlayer === null ? null : GObjects.fullNameOf(localPlayer),
    controller: pc === null ? null : GObjects.fullNameOf(pc),
    pawn: pawn === null ? null : GObjects.fullNameOf(pawn),
    pawn_class: pawn === null ? null : GObjects.classNameOf(pawn),
    location: root === null ? null : readVec(root, 'RelativeLocation')
  };

  // every actor of the pawn's own class currently in the object array,
  // with whatever location each one exposes
  var actors = [];
  if (pawn !== null) {
    var pawnCls = GObjects.classOf(pawn);
    GObjects.forEach(function (o) {
      var c = GObjects.classOf(o);
      if (c === null || !c.equals(pawnCls)) return;
      var n = GObjects.nameOf(o);
      if (n === null || n.indexOf('Default__') === 0) return;
      var rc = follow(o, 'RootComponent');
      actors.push({
        address: o.toString(),
        name: n,
        is_local: o.equals(pawn),
        location: rc === null ? null : readVec(rc, 'RelativeLocation')
      });
      if (actors.length >= 256) return false;
    }, 0);
  }
  report.actor_count = actors.length;

  var h = Out.open('user-actors.ndjson');
  if (h !== null) {
    for (var i = 0; i < actors.length; i++) Out.line(h, actors[i]);
    Out.close(h);
  }

  api.C.log('user script: ' + JSON.stringify(report));
  return report;
}
