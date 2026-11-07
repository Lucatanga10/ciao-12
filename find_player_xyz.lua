-- ============================================
-- FORTNITE MOBILE - FIND PLAYER XYZ POSITION
-- ============================================
-- 1. Entra in Battle Royale
-- 2. Atterra e stai FERMO
-- 3. Esegui lo script e segui le istruzioni
-- ============================================

gg.clearResults()
gg.setVisible(false)
gg.setRanges(gg.REGION_C_ALLOC | gg.REGION_ANONYMOUS | gg.REGION_OTHER)

function toast(msg)
  gg.toast(tostring(msg))
  print(tostring(msg))
end

toast("=== RICERCA POSIZIONE PLAYER ===")
toast("Stai FERMO! Scansione tra 3 secondi...")
gg.sleep(3000)

-- FASE 1: Snapshot — cerca tutti i valori float (unknown initial search)
toast("Fase 1: Snapshot valori...")
gg.searchUnknown(gg.TYPE_FLOAT)
local count = gg.getResultsCount()
toast("Snapshot: " .. count .. " valori")
gg.sleep(1000)

-- FASE 2: Filtra valori che NON cambiano (stai fermo)
toast("Resta FERMO ancora 3 secondi...")
gg.sleep(3000)
gg.searchFuzzy("0", gg.SIGN_FUZZY_EQUAL)
count = gg.getResultsCount()
toast("Valori stabili: " .. count)

-- FASE 3: Cammina
gg.alert("ADESSO CAMMINA DRITTO per 5 secondi, poi premi OK")

-- FASE 4: Trova valori che SONO cambiati
toast("Cerco valori cambiati...")
gg.searchFuzzy("0", gg.SIGN_FUZZY_NOT_EQUAL)
count = gg.getResultsCount()
toast("Cambiati: " .. count)

-- Se troppi, raffina
if count > 2000000 then
  toast("Troppi. Fermati di nuovo...")
  gg.alert("FERMATI completamente, poi premi OK")
  gg.sleep(2000)
  gg.searchFuzzy("0", gg.SIGN_FUZZY_EQUAL)
  count = gg.getResultsCount()
  toast("Stabili di nuovo: " .. count)

  gg.alert("Cammina in ALTRA direzione, poi premi OK")
  gg.searchFuzzy("0", gg.SIGN_FUZZY_NOT_EQUAL)
  count = gg.getResultsCount()
  toast("Cambiati: " .. count)
end

-- Filtra per range coordinate Fortnite
if count > 500000 then
  toast("Filtro range coordinate...")
  gg.refineNumber("-200000~200000", gg.TYPE_FLOAT)
  count = gg.getResultsCount()
  toast("Nel range: " .. count)
end

-- Ripeti fermo/cammina se ancora troppi
if count > 500000 then
  gg.alert("FERMATI, poi premi OK")
  gg.sleep(2000)
  gg.searchFuzzy("0", gg.SIGN_FUZZY_EQUAL)
  gg.alert("CAMMINA di nuovo, poi premi OK")
  gg.searchFuzzy("0", gg.SIGN_FUZZY_NOT_EQUAL)
  count = gg.getResultsCount()
  toast("Filtrati: " .. count)
end

if count == 0 then
  toast("Nessun risultato. Sei in partita?")
  os.exit()
end

-- FASE 5: Prendi risultati e cerca gruppi XYZ
toast("Analisi risultati...")
local max_get = math.min(count, 80000)
local results = gg.getResults(max_get)

-- Ordina per indirizzo
table.sort(results, function(a, b) return a.address < b.address end)

-- Cerca 3 float consecutivi (X, Y, Z a +0, +4, +8)
local candidates = {}

for i = 1, #results - 2 do
  local a1 = results[i].address
  local a2 = results[i+1].address
  local a3 = results[i+2].address

  if a2 == a1 + 4 and a3 == a1 + 8 then
    local x = results[i].value
    local y = results[i+1].value
    local z = results[i+2].value

    -- Coordinate Fortnite: X e Y grandi, Z ragionevole
    if math.abs(x) > 100 and math.abs(x) < 200000
      and math.abs(y) > 100 and math.abs(y) < 200000
      and z > -10000 and z < 200000 then
      table.insert(candidates, {
        address = a1, x = x, y = y, z = z
      })
    end
  end
end

toast("Candidati XYZ: " .. #candidates)

if #candidates == 0 then
  -- Prova anche con soglia piu' bassa
  for i = 1, #results - 2 do
    local a1 = results[i].address
    local a2 = results[i+1].address
    local a3 = results[i+2].address

    if a2 == a1 + 4 and a3 == a1 + 8 then
      local x = results[i].value
      local y = results[i+1].value
      local z = results[i+2].value

      if math.abs(x) > 10 and math.abs(x) < 500000
        and math.abs(y) > 10 and math.abs(y) < 500000
        and z > -50000 and z < 500000 then
        table.insert(candidates, {
          address = a1, x = x, y = y, z = z
        })
      end
    end
  end
  toast("Candidati (soglia bassa): " .. #candidates)
end

if #candidates == 0 then
  toast("Nessun XYZ trovato. Prova a ripetere.")
  -- Dump primi risultati per debug
  print("\n=== PRIMI 50 RISULTATI RAW ===")
  for i = 1, math.min(50, #results) do
    print(string.format("0x%X = %.2f", results[i].address, results[i].value))
  end
  gg.clearResults()
  os.exit()
end

-- FASE 6: Raggruppa coordinate simili
local groups = {}
for _, c in ipairs(candidates) do
  local found = false
  for _, g in ipairs(groups) do
    if math.abs(c.x - g.x) < 2 and math.abs(c.y - g.y) < 2 and math.abs(c.z - g.z) < 2 then
      g.count = g.count + 1
      table.insert(g.addrs, c.address)
      found = true
      break
    end
  end
  if not found then
    table.insert(groups, {
      x = c.x, y = c.y, z = c.z,
      count = 1,
      addrs = {c.address}
    })
  end
end

table.sort(groups, function(a, b) return a.count > b.count end)

-- FASE 7: Output
print("\n=== POSIZIONI TROVATE ===\n")
local show = math.min(#groups, 15)
for i = 1, show do
  local g = groups[i]
  print(string.format("Gruppo %d (%d copie): X=%.1f  Y=%.1f  Z=%.1f",
    i, g.count, g.x, g.y, g.z))
  for j = 1, math.min(5, #g.addrs) do
    print(string.format("  -> 0x%X", g.addrs[j]))
  end
end

-- Salva su file
local f = io.open("/sdcard/fortnite_pos.txt", "w")
if f then
  f:write("=== FORTNITE PLAYER POSITION ===\n\n")
  for i = 1, math.min(#groups, 15) do
    local g = groups[i]
    f:write(string.format("Gruppo %d (%d copie): X=%.4f  Y=%.4f  Z=%.4f\n",
      i, g.count, g.x, g.y, g.z))
    for j = 1, math.min(10, #g.addrs) do
      f:write(string.format("  Address: 0x%X\n", g.addrs[j]))
    end
    f:write("\n")
  end
  f:close()
  toast("Salvato: /sdcard/fortnite_pos.txt")
end

gg.clearResults()

local best = groups[1]
toast(string.format("POSIZIONE: X=%.0f Y=%.0f Z=%.0f (%d copie)",
  best.x, best.y, best.z, best.count))
gg.alert(string.format("Posizione trovata!\n\nX = %.1f\nY = %.1f\nZ = %.1f\n\nCopie: %d\nIndirizzo: 0x%X\n\nRisultati salvati in /sdcard/fortnite_pos.txt",
  best.x, best.y, best.z, best.count, best.addrs[1]))
