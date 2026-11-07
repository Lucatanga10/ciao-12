-- ============================================
-- FORTNITE MOBILE - FIND PLAYER XYZ v5
-- Metodo veloce: niente letture extra
-- ============================================

gg.clearResults()
gg.setVisible(false)
gg.setRanges(gg.REGION_C_ALLOC | gg.REGION_ANONYMOUS | gg.REGION_OTHER)

function toast(msg)
  gg.toast(tostring(msg))
  print(tostring(msg))
end

toast("=== FIND PLAYER XYZ v5 ===")
toast("Stai FERMO per 3 secondi...")
gg.sleep(3000)

-- FASE 1: Cerca float nel range coordinate Fortnite
-- Cerca sia positivi che negativi in un colpo solo
toast("Scansione...")
gg.searchNumber("-80000~-500;500~80000", gg.TYPE_FLOAT)
local count = gg.getResultsCount()
toast("Trovati: " .. count)

if count == 0 then
  toast("Nessun risultato. Sei in partita?")
  os.exit()
end

-- Prendi risultati (max 100k)
local results = gg.getResults(100000)
gg.clearResults()
toast("Analisi " .. #results .. " valori...")

-- FASE 2: Ordina per indirizzo
table.sort(results, function(a, b) return a.address < b.address end)

-- FASE 3: Cerca 3 float consecutivi (X,Y,Z = addr, addr+4, addr+8)
-- Tutti e 3 devono essere nei risultati (= tutti nel range 500-80000)
local candidates = {}
local addr_map = {}

-- Mappa indirizzo -> valore per lookup veloce
for _, r in ipairs(results) do
  addr_map[r.address] = r.value
end

toast("Cerco triple XYZ...")

for _, r in ipairs(results) do
  local a = r.address
  -- Controlla se a+4 e a+8 sono anche nei risultati
  if addr_map[a + 4] ~= nil and addr_map[a + 8] ~= nil then
    local x = addr_map[a]
    local y = addr_map[a + 4]
    local z = addr_map[a + 8]

    -- X e Y devono essere "grandi" (coordinate mondo)
    -- Z puo' essere piu' piccolo (altezza)
    if math.abs(x) > 200 and math.abs(y) > 200 then
      table.insert(candidates, {
        address = a, x = x, y = y, z = z
      })
    end
  end
end

toast("Triple XYZ trovate: " .. #candidates)

if #candidates == 0 then
  -- Prova con Z fuori range (Z potrebbe non essere 500-80000)
  toast("Nessuna tripla. Provo con Z qualsiasi...")

  for _, r in ipairs(results) do
    local a = r.address
    -- Solo X e Y nei risultati, Z lo leggiamo
    if addr_map[a + 4] ~= nil then
      local x = addr_map[a]
      local y = addr_map[a + 4]

      if math.abs(x) > 500 and math.abs(y) > 500 then
        table.insert(candidates, {
          address = a, x = x, y = y, z = 0, need_z = true
        })
      end
    end
  end

  toast("Coppie XY trovate: " .. #candidates)

  -- Leggi Z per le prime 500 coppie
  if #candidates > 0 then
    local check = math.min(#candidates, 500)
    local batch = {}
    for i = 1, check do
      table.insert(batch, {address = candidates[i].address + 8, flags = gg.TYPE_FLOAT})
    end
    gg.getValues(batch)
    for i = 1, check do
      candidates[i].z = batch[i].value
    end

    -- Filtra Z ragionevole
    local filtered = {}
    for i = 1, check do
      local c = candidates[i]
      if c.z > -20000 and c.z < 100000 then
        table.insert(filtered, c)
      end
    end
    candidates = filtered
    toast("Con Z valido: " .. #candidates)
  end
end

if #candidates == 0 then
  toast("Nessun candidato. Controlla il log.")
  print("\nPrimi 20 risultati raw:")
  for i = 1, math.min(20, #results) do
    print(string.format("0x%X = %.1f", results[i].address, results[i].value))
  end
  os.exit()
end

-- FASE 4: Verifica con movimento
gg.alert("Trovati " .. #candidates .. " candidati.\n\nCAMMINA DRITTO per 5 secondi,\npoi premi OK.")

-- Leggi nuovi valori in batch
local check_count = math.min(#candidates, 1000)
local batch = {}
for i = 1, check_count do
  table.insert(batch, {address = candidates[i].address, flags = gg.TYPE_FLOAT})
  table.insert(batch, {address = candidates[i].address + 4, flags = gg.TYPE_FLOAT})
  table.insert(batch, {address = candidates[i].address + 8, flags = gg.TYPE_FLOAT})
end

toast("Verifico " .. check_count .. " candidati...")
gg.getValues(batch)

local verified = {}
for i = 1, check_count do
  local bi = (i - 1) * 3
  local new_x = batch[bi + 1].value
  local new_y = batch[bi + 2].value
  local new_z = batch[bi + 3].value
  local c = candidates[i]

  local dx = math.abs(new_x - c.x)
  local dy = math.abs(new_y - c.y)

  if (dx > 10 or dy > 10) and dx < 50000 and dy < 50000 then
    if math.abs(new_x) > 50 and math.abs(new_x) < 200000
      and math.abs(new_y) > 50 and math.abs(new_y) < 200000 then
      table.insert(verified, {
        address = c.address,
        x = new_x, y = new_y, z = new_z,
        delta = dx + dy
      })
    end
  end
end

toast("Verificati: " .. #verified)

-- Se nessuno verificato, mostra candidati
local final = verified
if #final == 0 then
  toast("Nessun cambio con movimento. Mostro candidati.")
  final = {}
  for i = 1, math.min(check_count, 30) do
    local bi = (i - 1) * 3
    table.insert(final, {
      address = candidates[i].address,
      x = batch[bi + 1].value,
      y = batch[bi + 2].value,
      z = batch[bi + 3].value,
      delta = 0
    })
  end
end

-- Raggruppa
local groups = {}
for _, c in ipairs(final) do
  local found = false
  for _, g in ipairs(groups) do
    if math.abs(c.x - g.x) < 10 and math.abs(c.y - g.y) < 10 then
      g.count = g.count + 1
      table.insert(g.items, c)
      found = true
      break
    end
  end
  if not found then
    table.insert(groups, {
      x = c.x, y = c.y, z = c.z,
      count = 1, items = {c}
    })
  end
end

table.sort(groups, function(a, b) return a.count > b.count end)

-- Output
print("\n========================================")
print("=== POSIZIONI TROVATE ===")
print("========================================\n")

for i = 1, math.min(#groups, 15) do
  local g = groups[i]
  print(string.format("Gruppo %d (%d copie): X=%.1f  Y=%.1f  Z=%.1f", i, g.count, g.x, g.y, g.z))
  for j = 1, math.min(3, #g.items) do
    print(string.format("  0x%X (delta=%.0f)", g.items[j].address, g.items[j].delta))
  end
end

-- Salva
local f = io.open("/sdcard/fortnite_pos.txt", "w")
if f then
  for i = 1, math.min(#groups, 15) do
    local g = groups[i]
    f:write(string.format("Gruppo %d (%d copie): X=%.4f Y=%.4f Z=%.4f\n", i, g.count, g.x, g.y, g.z))
    for j = 1, math.min(10, #g.items) do
      f:write(string.format("  0x%X\n", g.items[j].address))
    end
    f:write("\n")
  end
  f:close()
end

local best = groups[1]
gg.alert(string.format(
  "RISULTATO\n\nX = %.1f\nY = %.1f\nZ = %.1f\n\nCopie: %d\nAddr: 0x%X\n\nSalvato: /sdcard/fortnite_pos.txt",
  best.x, best.y, best.z, best.count, best.items[1].address))

toast("FATTO!")
