-- ============================================
-- FORTNITE MOBILE - FIND PLAYER XYZ POSITION
-- Method: Smart coordinate scanning with GG native search
-- ============================================
-- ISTRUZIONI:
-- 1. Entra in una partita Battle Royale
-- 2. Atterra e stai FERMO per 3 secondi
-- 3. Esegui questo script
-- 4. Quando dice "CAMMINA AVANTI", cammina dritto per 3-5 secondi
-- 5. Lo script trova la tua posizione
-- ============================================

gg.clearResults()
gg.setVisible(false)

function log(msg)
  print(tostring(msg))
end

function toast(msg)
  gg.toast(tostring(msg))
end

-- Step 1: Get libUnreal.so memory regions
function getLibRegions()
  local ranges = gg.getRangesList("libUnreal.so")
  if #ranges == 0 then
    toast("libUnreal.so non trovato!")
    return nil
  end

  local base = nil
  local rw = {}

  for _, r in ipairs(ranges) do
    if base == nil and string.find(r.type, "x") then
      base = r.start
    end
    -- collect readable/writable regions (heap objects allocated by the engine)
    if string.find(r.type, "rw") then
      table.insert(rw, r)
    end
  end

  toast("libUnreal.so base: " .. string.format("0x%X", base or 0))
  return base, rw
end

-- Step 2: Scan anonymous/heap memory for coordinate patterns
-- Fortnite world coordinates are 3 consecutive floats (X, Y, Z)
-- X and Y are typically large (1000-150000 range), Z depends on height
function findPlayerCoords()
  toast("=== RICERCA POSIZIONE PLAYER ===")
  toast("Stai FERMO! Scansione in corso...")
  gg.sleep(2000)

  -- Get ALL memory regions (not just libUnreal.so — player data is on the heap)
  -- Use Ca (C_ALLOC) and A (Anonymous) regions which contain UObject allocations
  local regions = gg.getRangesList("")

  -- We need to search in heap memory
  -- Set search range to Other (Ca, A, etc)
  gg.setRanges(gg.REGION_C_ALLOC | gg.REGION_ANONYMOUS | gg.REGION_OTHER)

  -- PHASE 1: Ask player to note approximate position from the map
  -- Or use group search for 3 floats pattern

  toast("Fase 1: Scansione iniziale (stai FERMO)...")
  gg.sleep(1000)

  -- Search for any float in a reasonable coordinate range
  -- We'll use a "changed/unchanged" approach
  -- First scan: search for float values that are "unknown" — we snapshot everything

  -- Actually use Fuzzy search which is designed for this
  -- Fuzzy search step 1: snapshot all floats
  gg.searchFuzzy("0", gg.TYPE_FLOAT, 0.1, gg.SIGN_FUZZY_EQUAL, 0, -1)
  -- This won't work well. Let's use a different approach.
  gg.clearResults()

  -- BETTER METHOD: Group search for 3 consecutive floats
  -- We know player coords are 3 floats in a row at aligned offset
  -- Search for a float, then check neighbors

  -- Phase 1: Search unchanged values
  toast("FASE 1: Stai COMPLETAMENTE FERMO per 5 secondi...")
  gg.sleep(5000)

  -- Do a fuzzy "unknown" search to snapshot all float values
  gg.searchUnknown(gg.TYPE_FLOAT, nil, nil, gg.SIGN_EQUAL, 0, -1)
  local count1 = gg.getResultsCount()
  toast("Snapshot: " .. count1 .. " valori")
  gg.sleep(500)

  -- Phase 1b: Search for UNCHANGED values (still standing still)
  toast("Resta fermo ancora 2 secondi...")
  gg.sleep(2000)
  gg.searchFuzzy("0", gg.TYPE_FLOAT, 0, gg.SIGN_FUZZY_EQUAL, 0, -1)
  local count2 = gg.getResultsCount()
  toast("Valori stabili: " .. count2)

  -- Phase 2: Walk forward
  toast(">>> ADESSO CAMMINA DRITTO PER 5 SECONDI <<<")
  gg.alert("CAMMINA DRITTO per 5 secondi, poi premi OK")

  -- Phase 2b: Search for CHANGED values (position changed because we moved)
  toast("Cerco valori cambiati...")
  gg.searchFuzzy("0", gg.TYPE_FLOAT, 0, gg.SIGN_FUZZY_NOT_EQUAL, 0, -1)
  local count3 = gg.getResultsCount()
  toast("Valori cambiati: " .. count3)

  if count3 > 5000000 then
    toast("Troppi risultati. Riproviamo con filtro piu' stretto...")
    -- Filter: changed, and current value is in a reasonable coordinate range
    gg.searchNumber("-200000~200000", gg.TYPE_FLOAT, false, gg.SIGN_EQUAL, 0, -1)
    count3 = gg.getResultsCount()
    toast("Dopo filtro range: " .. count3)
  end

  if count3 > 1000000 then
    -- Additional filter: ask to stop
    toast(">>> ADESSO FERMATI <<<")
    gg.alert("FERMATI completamente, poi premi OK")
    gg.sleep(2000)

    -- Search unchanged again (we stopped)
    gg.searchFuzzy("0", gg.TYPE_FLOAT, 0, gg.SIGN_FUZZY_EQUAL, 0, -1)
    count3 = gg.getResultsCount()
    toast("Valori ora stabili: " .. count3)

    -- Walk again in a DIFFERENT direction
    toast(">>> CAMMINA in un'ALTRA DIREZIONE per 5 sec <<<")
    gg.alert("Cammina in ALTRA direzione, poi premi OK")

    gg.searchFuzzy("0", gg.TYPE_FLOAT, 0, gg.SIGN_FUZZY_NOT_EQUAL, 0, -1)
    count3 = gg.getResultsCount()
    toast("Valori cambiati di nuovo: " .. count3)
  end

  if count3 > 500000 then
    toast("Ancora troppi. Filtro aggiuntivo range coordinate...")
    gg.searchNumber("-150000~150000", gg.TYPE_FLOAT, false, gg.SIGN_EQUAL, 0, -1)
    count3 = gg.getResultsCount()
    toast("Risultati finali: " .. count3)
  end

  -- Phase 3: Get results and find groups of 3 consecutive floats
  toast("Analisi risultati per trovare XYZ...")

  local max_check = math.min(count3, 50000)
  local results = gg.getResults(max_check)

  -- Sort by address
  table.sort(results, function(a, b) return a.address < b.address end)

  -- Find groups of 3 consecutive floats (X, Y, Z at +0, +4, +8)
  local candidates = {}

  for i = 1, #results - 2 do
    local a1 = results[i].address
    local a2 = results[i+1].address
    local a3 = results[i+2].address

    -- Check if they are 3 consecutive floats (4 bytes apart)
    if a2 == a1 + 4 and a3 == a1 + 8 then
      local x = results[i].value
      local y = results[i+1].value
      local z = results[i+2].value

      -- Validate: at least X or Y should be "big" (world coords)
      -- Z can be smaller (height) but should be reasonable
      if math.abs(x) > 50 and math.abs(x) < 200000
        and math.abs(y) > 50 and math.abs(y) < 200000
        and z > -10000 and z < 200000 then
        table.insert(candidates, {
          address = a1,
          x = x, y = y, z = z
        })
      end
    end
  end

  toast("Candidati XYZ trovati: " .. #candidates)

  if #candidates == 0 then
    toast("Nessun candidato trovato. Riprova in partita.")
    gg.clearResults()
    return nil
  end

  -- Phase 4: Verify — read current values and show most likely ones
  -- Group similar coordinates (multiple pointers to same location)
  log("\n=== CANDIDATI POSIZIONE PLAYER ===\n")

  -- Re-read current values to verify they're still valid
  local verified = {}
  for i, c in ipairs(candidates) do
    local t = {
      {address = c.address, flags = gg.TYPE_FLOAT},
      {address = c.address + 4, flags = gg.TYPE_FLOAT},
      {address = c.address + 8, flags = gg.TYPE_FLOAT}
    }
    gg.getValues(t)
    local x = t[1].value
    local y = t[2].value
    local z = t[3].value

    if math.abs(x) > 50 and math.abs(x) < 200000
      and math.abs(y) > 50 and math.abs(y) < 200000
      and z > -10000 and z < 200000 then
      table.insert(verified, {
        address = c.address,
        x = x, y = y, z = z
      })
    end
  end

  log("Candidati verificati: " .. #verified)

  -- Show top results
  local show = math.min(#verified, 20)
  for i = 1, show do
    local c = verified[i]
    log(string.format("#%d  Addr: 0x%X  X=%.1f  Y=%.1f  Z=%.1f",
      i, c.address, c.x, c.y, c.z))
  end

  -- Group by similar coordinates (within 1.0 of each other)
  local groups = {}
  for _, c in ipairs(verified) do
    local found = false
    for _, g in ipairs(groups) do
      if math.abs(c.x - g.x) < 1 and math.abs(c.y - g.y) < 1 and math.abs(c.z - g.z) < 1 then
        g.count = g.count + 1
        table.insert(g.addresses, c.address)
        found = true
        break
      end
    end
    if not found then
      table.insert(groups, {
        x = c.x, y = c.y, z = c.z,
        count = 1,
        addresses = {c.address}
      })
    end
  end

  -- Sort groups by count (most common = most likely to be the real position)
  table.sort(groups, function(a, b) return a.count > b.count end)

  log("\n=== GRUPPI DI COORDINATE (ordinate per frequenza) ===\n")
  local show_groups = math.min(#groups, 10)
  for i = 1, show_groups do
    local g = groups[i]
    log(string.format("Gruppo %d (%d copie): X=%.1f  Y=%.1f  Z=%.1f",
      i, g.count, g.x, g.y, g.z))
    for j, addr in ipairs(g.addresses) do
      if j <= 5 then
        log(string.format("   Addr: 0x%X", addr))
      end
    end
  end

  -- Save results
  local f = io.open("/sdcard/fortnite_player_pos.txt", "w")
  if f then
    f:write("=== FORTNITE PLAYER POSITION SCAN ===\n\n")
    for i = 1, math.min(#groups, 10) do
      local g = groups[i]
      f:write(string.format("Gruppo %d (%d copie): X=%.4f  Y=%.4f  Z=%.4f\n",
        i, g.count, g.x, g.y, g.z))
      for j, addr in ipairs(g.addresses) do
        if j <= 10 then
          f:write(string.format("  Address: 0x%X\n", addr))
        end
      end
      f:write("\n")
    end
    f:close()
    toast("Salvato in /sdcard/fortnite_player_pos.txt")
  end

  gg.clearResults()

  if #groups > 0 then
    local best = groups[1]
    toast(string.format("POSIZIONE PROBABILE: X=%.0f Y=%.0f Z=%.0f (%d copie)",
      best.x, best.y, best.z, best.count))
    return best
  end

  return nil
end

-- RUN
local result = findPlayerCoords()

if result then
  log("\n=== PROSSIMO PASSO ===")
  log("Se le coordinate corrispondono alla tua posizione sulla mappa,")
  log("esegui lo script di teleport con questi indirizzi.")
  log("L'indirizzo con piu' copie e' quello giusto da modificare.")
end
