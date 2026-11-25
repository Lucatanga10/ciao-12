/*
 * probe_match.js — Frida probe: trova il vero UWorld di partita.
 *
 * Legge GWorld dal RVA confermato, poi cerca il match world
 * scansionando UWorld::Levels e percorsi alternativi.
 *
 * Uso: frida-inject -p $(pidof com.epicgames.fortnite) -s probe_match.js --no-pause
 */

var mod = Process.findModuleByName("libUnreal.so");
if (!mod) { console.log("FATAL: libUnreal.so non trovato"); } else {
    var base = mod.base;
    console.log("base=" + base + " size=0x" + mod.size.toString(16));

    var GWORLD_RVA = 0x19cec360;

    // Leggi GWorld dal RVA noto (lobby world)
    var gw_addr = base.add(GWORLD_RVA);
    var lobby_world = gw_addr.readPointer();
    console.log("\n=== GWorld RVA 0x" + GWORLD_RVA.toString(16) + " ===");
    console.log("addr=" + gw_addr + " -> world=" + lobby_world);

    // Helper: leggi TArray
    function readTArray(addr) {
        try {
            var data = addr.readPointer();
            var num = addr.add(8).readS32();
            var max = addr.add(12).readS32();
            if (num < 0 || num > 1000000 || max < num) return null;
            return { data: data, num: num, max: max };
        } catch(e) { return null; }
    }

    // Helper: leggi puntatore sicuro
    function safePtr(addr) {
        try { return addr.readPointer(); } catch(e) { return ptr(0); }
    }

    // Helper: check se sembra un UObject (vtable punta nel modulo)
    function looksLikeUObject(addr) {
        if (addr.isNull()) return false;
        try {
            var vt = addr.readPointer();
            var diff = vt.sub(base).toInt32();
            return diff >= 0 && diff < mod.size;
        } catch(e) { return false; }
    }

    // 1. Conta attori del lobby world
    var lobby_level = safePtr(lobby_world.add(0x20));
    var lobby_actors = readTArray(lobby_level.add(0xe0));
    console.log("lobby_level=" + lobby_level +
        " actors=" + (lobby_actors ? lobby_actors.num : "FAIL"));

    // 2. Cerca UWorld::Levels — scan offsets 0x28..0x250 dal world
    console.log("\n=== Scan UWorld per Levels TArray (cerca >280 attori) ===");
    for (var off = 0x28; off <= 0x250; off += 8) {
        var ta = readTArray(lobby_world.add(off));
        if (!ta || ta.num < 1 || ta.num > 200) continue;

        // Ogni elemento e' un ULevel*
        for (var li = 0; li < ta.num && li < 10; li++) {
            var lvl = safePtr(ta.data.add(li * 8));
            if (lvl.isNull() || !looksLikeUObject(lvl)) continue;

            var lvl_actors = readTArray(lvl.add(0xe0));
            if (!lvl_actors) continue;

            if (lvl_actors.num > 280) {
                console.log("  +0x" + off.toString(16) + " levels[" + li + "]=" +
                    lvl + " actors=" + lvl_actors.num + " <-- MATCH LEVEL?");
            }
        }
    }

    // 3. Scan per un world diverso: cerca puntatori nel range 0x28..0x400
    //    del GameInstance che puntano a un UWorld diverso
    console.log("\n=== Scan GameInstance per World alternativo ===");
    var gi = safePtr(lobby_world.add(0x170));
    console.log("GameInstance=" + gi);
    if (!gi.isNull()) {
        for (var off = 0x0; off <= 0x400; off += 8) {
            try {
                var candidate = gi.add(off).readPointer();
                if (candidate.isNull()) continue;
                if (candidate.equals(lobby_world)) continue;
                if (!looksLikeUObject(candidate)) continue;

                // Prova a leggere PersistentLevel da questo candidato
                var cand_level = safePtr(candidate.add(0x20));
                if (cand_level.isNull() || !looksLikeUObject(cand_level)) continue;

                var cand_actors = readTArray(cand_level.add(0xe0));
                if (!cand_actors || cand_actors.num <= 0) continue;

                console.log("  GI+0x" + off.toString(16) + " -> world=" + candidate +
                    " level=" + cand_level + " actors=" + cand_actors.num);
            } catch(e) {}
        }
    }

    // 4. Scan adiacente al GWorld RVA — forse il vero GWorld e' a un offset vicino
    console.log("\n=== Scan attorno a GWorld RVA (+-0x200) ===");
    for (var delta = -0x200; delta <= 0x200; delta += 8) {
        if (delta === 0) continue;
        try {
            var candidate = base.add(GWORLD_RVA + delta).readPointer();
            if (candidate.isNull()) continue;
            if (candidate.equals(lobby_world)) continue;
            if (!looksLikeUObject(candidate)) continue;

            var cand_level = safePtr(candidate.add(0x20));
            if (cand_level.isNull()) continue;

            var cand_actors = readTArray(cand_level.add(0xe0));
            if (!cand_actors || cand_actors.num <= 0) continue;

            console.log("  RVA+0x" + (GWORLD_RVA + delta).toString(16) +
                " -> world=" + candidate +
                " actors=" + cand_actors.num);
        } catch(e) {}
    }

    // 5. Dump camera dal lobby world per conferma
    console.log("\n=== Camera dal lobby world ===");
    var pc_chain = (function() {
        var gi2 = safePtr(lobby_world.add(0x170));
        var lp_ta = readTArray(gi2.add(0x168));
        if (!lp_ta || lp_ta.num < 1) return null;
        var lp0 = safePtr(lp_ta.data);
        var pc = safePtr(lp0.add(0x20));
        return pc;
    })();
    if (pc_chain && !pc_chain.isNull()) {
        var pcm = safePtr(pc_chain.add(0x338));
        console.log("PC=" + pc_chain + " PCM=" + pcm);

        if (!pcm.isNull()) {
            var pov = pcm.add(0x1580);
            try {
                var cx = pov.readDouble();
                var cy = pov.add(8).readDouble();
                var cz = pov.add(16).readDouble();
                var fov = pov.add(0x30).readFloat();
                console.log("cam=(" + cx.toFixed(1) + "," + cy.toFixed(1) + "," +
                    cz.toFixed(1) + ") fov=" + fov.toFixed(1));
            } catch(e) { console.log("cam read error: " + e); }

            // Prova offset camera alternativi nel PCM
            console.log("\n=== Scan PCM per camera non-zero ===");
            for (var co = 0x100; co <= 0x2000; co += 8) {
                try {
                    var tx = pcm.add(co).readDouble();
                    var ty = pcm.add(co + 8).readDouble();
                    var tz = pcm.add(co + 16).readDouble();
                    if (isNaN(tx) || isNaN(ty) || isNaN(tz)) continue;
                    if (tx === 0 && ty === 0 && tz === 0) continue;
                    if (Math.abs(tx) > 2e7 || Math.abs(ty) > 2e7 || Math.abs(tz) > 2e7) continue;

                    // Check se sembra una posizione plausibile (10..1e6 range)
                    var mag = Math.sqrt(tx*tx + ty*ty + tz*tz);
                    if (mag < 100 || mag > 1e7) continue;

                    // Check FOV vicino
                    var f1 = pcm.add(co + 0x30).readFloat();
                    if (f1 > 50 && f1 < 140) {
                        console.log("  PCM+0x" + co.toString(16) +
                            " pos=(" + tx.toFixed(0) + "," + ty.toFixed(0) + "," +
                            tz.toFixed(0) + ") +0x30 fov=" + f1.toFixed(1));
                    }
                } catch(e) {}
            }
        }
    }

    console.log("\n=== DONE ===");
}
