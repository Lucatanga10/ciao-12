-- ============================================
-- FORTNITE MOBILE - FIND PLAYER XYZ
-- Metodo: scansione diretta float + refine
-- ============================================
-- 1. Entra in Battle Royale, atterra
-- 2. Stai FERMO e guarda la mappa: nota la tua posizione approssimativa
-- 3. Esegui lo script e segui le istruzioni
-- ============================================

gg.clearResults()
gg.setVisible(false)
gg.setRanges(gg.REGION_C_ALLOC | gg.REGION_ANONYMOUS | gg.REGION_OTHER)

function toast(msg)
  gg.toast(tostring(msg))
  print(tostring(msg))
end

-- FASE 1: Cerca un valore float specifico
-- Chiedi al player di leggere una coordinata dalla mappa/bussola
toast("=== RICERCA POSIZIONE PLAYER ===")

local choice = gg.choice({
  "Metodo 1: Scansione automatica (consigliato)",
  "Metodo 2: Inserisci coordinata manuale",
  "Esci"
}, nil, "Scegli metodo")

if choice == 3 or choice == nil then os.exit() end

if choice == 2 then
  -- Metodo manuale: il player inserisce un valore approssimativo
  local val = gg.prompt({"Inserisci un valore float da cercare (es. la tua X o Y dalla bussola):"}, {""}, {"number"})
  if val == nil then os.exit() end

  toast("Cerco valore: " .. val[1])
  gg.searchNumber(val[1], gg.TYPE_FLOAT)
  local count = gg.getResultsCount()
  toast("Trovati: " .. count)

  if count > 0 and count < 100000 then
    local results = gg.getResults(math.min(count, 5000))
    print("\n=== RISULTATI ===")
    for i = 1, math.min(20, #results) do
      print(string.format("0x%X = %.2f", results[i].address, results[i].value))
    end
  end
  gg.clearResults()
  os.exit()
end

-- METODO 1: Scansione automatica
-- Cerca float in un range tipico di coordinate Fortnite
-- Poi filtra con movimento

toast("FASE 1: Stai FERMO!")
toast("Scansione valori tra 500 e 150000...")
gg.sleep(2000)

-- Prima scansione: cerca float in range coordinate mondo
gg.searchNumber("500~150000", gg.TYPE_FLOAT)
local count = gg.getResultsCount()
toast("Scan 1 (positivi): " .. count)

-- Salva risultati positivi
local pos_results = {}
if count > 0 and count < 500000 then
  pos_results = gg.getResults(math.min(count, 100000))
end

gg.clearResults()

-- Cerca anche negativi
gg.searchNumber("-150000~-500", gg.TYPE_FLOAT)
local count_neg = gg.getResultsCount()
toast("Scan 1 (negativi): " .. count_neg)

local neg_results = {}
if count_neg > 0 and count_neg < 500000 then
  neg_results = gg.getResults(math.min(count_neg, 100000))
end

gg.clearResults()

-- Combina
local all = {}
for _, r in ipairs(pos_results) do table.insert(all, r) end
for _, r in ipairs(neg_results) do table.insert(all, r) end

toast("Totale valori trovati: " .. #all)

if #all == 0 then
  toast("Nessun valore trovato. Sei in partita?")
  os.exit()
end

-- FASE 2: Per ogni risultato, leggi i 2 float adiacenti e cerca pattern XYZ
toast("FASE 2: Cerco pattern XYZ (3 float consecutivi)...")

local candidates = {}

for i = 1, #all do
  local addr = all[i].address
  local val = all[i].value

  -- Controlla se l'indirizzo e' allineato a 4
  if addr % 4 == 0 then
    -- Leggi il float prima e dopo
    local t = {
      {address = addr - 4, flags = gg.TYPE_FLOAT},
      {address = addr, flags = gg.TYPE_FLOAT},
      {address = addr + 4, flags = gg.TYPE_FLOAT},
      {address = addr + 8, flags = gg.TYPE_FLOAT}
    }
    gg.getValues(t)

    -- Pattern 1: questo e' X, poi Y e Z seguono
    local x = t[2].value
    local y = t[3].value
    local z = t[4].value

    if math.abs(x) > 100 and math.abs(x) < 200000
      and math.abs(y) > 100 and math.abs(y) < 200000
      and z > -10000 and z < 100000
      and math.abs(z) < 50000 then
      table.insert(candidates, {
        address = addr, x = x, y = y, z = z, pattern = "XYZ"
      })
    end

    -- Pattern 2: questo e' Y, X e' prima
    local x2 = t[1].value
    local y2 = t[2].value
    local z2 = t[3].value

    if math.abs(x2) > 100 and math.abs(x2) < 200000
      and math.abs(y2) > 100 and math.abs(y2) < 200000
      and z2 > -10000 and z2 < 100000
      and math.abs(z2) < 50000 then
      table.insert(candidates, {
        address = addr - 4, x = x2, y = y2, z = z2, pattern = "XYZ"
      })
    end
  end

  -- Non sovraccaricare GG — processa a blocchi
  if i % 5000 == 0 then
    toast("Analizzati " .. i .. "/" .. #all .. "...")
    gg.sleep(10)
  end
end

-- Rimuovi duplicati per indirizzo
local seen = {}
local unique = {}
for _, c in ipairs(candidates) do
  local key = tostring(c.address)
  if not seen[key] then
    seen[key] = true
    table.insert(unique, c)
  end
end
candidates = unique

toast("Candidati XYZ unici: " .. #candidates)

if #candidates == 0 then
  toast("Nessun XYZ trovato. Prova Metodo 2.")
  print("\n=== DEBUG: primi 30 valori trovati ===")
  for i = 1, math.min(30, #all) do
    print(string.format("0x%X = %.2f", all[i].address, all[i].value))
  end
  os.exit()
end

-- FASE 3: Chiedi di muoversi per verificare
toast("FASE 3: Verifica movimento")
gg.alert("Trovati " .. #candidates .. " candidati.\n\nADESSO CAMMINA per 5 secondi,\npoi premi OK per verificare quali cambiano.")

-- Rileggi tutti i candidati
local changed = {}
for _, c in ipairs(candidates) do
  local t = {
    {address = c.address, flags = gg.TYPE_FLOAT},
    {address = c.address + 4, flags = gg.TYPE_FLOAT},
    {address = c.address + 8, flags = gg.TYPE_FLOAT}
  }
  gg.getValues(t)

  local new_x = t[1].value
  local new_y = t[2].value
  local new_z = t[3].value

  -- Se almeno X o Y e' cambiato (abbiamo camminato)
  local dx = math.abs(new_x - c.x)
  local dy = math.abs(new_y - c.y)

  if (dx > 5 or dy > 5) and dx < 50000 and dy < 50000 then
    -- Verifica che i nuovi valori siano ancora nel range
    if math.abs(new_x) > 50 and math.abs(new_x) < 200000
      and math.abs(new_y) > 50 and math.abs(new_y) < 200000 then
      table.insert(changed, {
        address = c.address,
        old_x = c.x, old_y = c.y, old_z = c.z,
        new_x = new_x, new_y = new_y, new_z = new_z,
        delta = dx + dy
      })
    end
  end
end

toast("Valori cambiati con movimento: " .. #changed)

if #changed == 0 then
  toast("Nessun valore cambiato. Mostra tutti i candidati:")
  print("\n=== TUTTI I CANDIDATI ===")
  local show = math.min(30, #candidates)
  for i = 1, show do
    local c = candidates[i]
    print(string.format("0x%X: X=%.1f Y=%.1f Z=%.1f", c.address, c.x, c.y, c.z))
  end
  os.exit()
end

-- Raggruppa per coordinate simili
local groups = {}
for _, c in ipairs(changed) do
  local found = false
  for _, g in ipairs(groups) do
    if math.abs(c.new_x - g.x) < 5 and math.abs(c.new_y - g.y) < 5 then
      g.count = g.count + 1
      table.insert(g.items, c)
      found = true
      break
    end
  end
  if not found then
    table.insert(groups, {
      x = c.new_x, y = c.new_y, z = c.new_z,
      count = 1,
      items = {c}
    })
  end
end

table.sort(groups, function(a, b) return a.count > b.count end)

-- Output
print("\n========================================")
print("=== POSIZIONI PLAYER TROVATE ===")
print("========================================\n")

local show = math.min(#groups, 10)
for i = 1, show do
  local g = groups[i]
  print(string.format("--- Gruppo %d (%d copie) ---", i, g.count))
  print(string.format("  X = %.2f", g.x))
  print(string.format("  Y = %.2f", g.y))
  print(string.format("  Z = %.2f", g.z))
  for j = 1, math.min(5, #g.items) do
    local it = g.items[j]
    print(string.format("  Addr: 0x%X (delta: %.1f)", it.address, it.delta))
  end
  print("")
end

-- Salva
local f = io.open("/sdcard/fortnite_pos.txt", "w")
if f then
  for i = 1, math.min(#groups, 10) do
    local g = groups[i]
    f:write(string.format("Gruppo %d (%d copie): X=%.4f Y=%.4f Z=%.4f\n", i, g.count, g.x, g.y, g.z))
    for j = 1, math.min(10, #g.items) do
      f:write(string.format("  0x%X\n", g.items[j].address))
    end
    f:write("\n")
  end
  f:close()
  toast("Salvato: /sdcard/fortnite_pos.txt")
end

-- Mostra risultato
local best = groups[1]
gg.alert(string.format(
  "POSIZIONE TROVATA!\n\nX = %.1f\nY = %.1f\nZ = %.1f\n\nCopie: %d\nAddr: 0x%X\n\nSalvato in /sdcard/fortnite_pos.txt",
  best.x, best.y, best.z, best.count, best.items[1].address))

gg.clearResults()
