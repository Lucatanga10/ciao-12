-- ============================================
-- FORTNITE MOBILE - FIND PLAYER XYZ v4
-- ============================================
-- 1. Entra in Battle Royale, atterra
-- 2. Esegui lo script e segui le istruzioni
-- ============================================

gg.clearResults()
gg.setVisible(false)
gg.setRanges(gg.REGION_C_ALLOC | gg.REGION_ANONYMOUS | gg.REGION_OTHER)

function toast(msg)
  gg.toast(tostring(msg))
  print(tostring(msg))
end

toast("=== RICERCA POSIZIONE PLAYER ===")
toast("Stai FERMO! Scansione tra 3 sec...")
gg.sleep(3000)

-- FASE 1: Cerca float in range piu' stretto
-- Fortnite mappa e' circa -130000 a 130000 su ogni asse
-- Usiamo range stretto per ridurre risultati
toast("Scan range 1000~80000...")
gg.searchNumber("1000~80000", gg.TYPE_FLOAT)
local count = gg.getResultsCount()
toast("Trovati: " .. count)

-- Prendiamo i primi 50000 risultati anche se ce ne sono milioni
local results = gg.getResults(50000)
toast("Presi " .. #results .. " risultati da analizzare")

-- FASE 2: Per ogni risultato controlla se ha 2 float adiacenti validi
toast("Cerco pattern XYZ...")
local candidates = {}
local batch_size = 200
local batch = {}

for i = 1, #results do
  local addr = results[i].address
  if addr % 4 == 0 then
    -- Aggiungi alla batch di lettura: leggi addr+4 e addr+8
    table.insert(batch, {address = addr + 4, flags = gg.TYPE_FLOAT})
    table.insert(batch, {address = addr + 8, flags = gg.TYPE_FLOAT})

    -- Processa batch ogni N elementi
    if #batch >= batch_size or i == #results then
      gg.getValues(batch)

      -- Analizza i risultati in coppia
      for j = 1, #batch, 2 do
        local y_val = batch[j].value
        local z_val = batch[j+1].value
        local x_addr = batch[j].address - 4
        local x_val = nil

        -- Trova il valore X corrispondente
        for _, r in ipairs(results) do
          if r.address == x_addr then
            x_val = r.value
            break
          end
        end

        if x_val == nil then
          -- Leggiamo X direttamente
          local tmp = {{address = x_addr, flags = gg.TYPE_FLOAT}}
          gg.getValues(tmp)
          x_val = tmp[1].value
        end

        -- Verifica pattern XYZ valido
        if x_val and type(y_val) == "number" and type(z_val) == "number" then
          if math.abs(x_val) > 100 and math.abs(x_val) < 200000
            and math.abs(y_val) > 100 and math.abs(y_val) < 200000
            and z_val > -20000 and z_val < 100000 then
            table.insert(candidates, {
              address = x_addr,
              x = x_val, y = y_val, z = z_val
            })
          end
        end
      end

      batch = {}
    end
  end

  if i % 10000 == 0 then
    toast("Analizzati " .. i .. "/" .. #results)
    gg.sleep(5)
  end
end

gg.clearResults()

-- Rimuovi duplicati
local seen = {}
local unique = {}
for _, c in ipairs(candidates) do
  if not seen[c.address] then
    seen[c.address] = true
    table.insert(unique, c)
  end
end
candidates = unique

toast("Candidati XYZ: " .. #candidates)

if #candidates == 0 then
  toast("Nessun XYZ. Provo range negativo...")

  gg.searchNumber("-80000~-1000", gg.TYPE_FLOAT)
  count = gg.getResultsCount()
  toast("Negativi: " .. count)
  results = gg.getResults(50000)

  for i = 1, #results do
    local addr = results[i].address
    if addr % 4 == 0 then
      local t = {
        {address = addr, flags = gg.TYPE_FLOAT},
        {address = addr + 4, flags = gg.TYPE_FLOAT},
        {address = addr + 8, flags = gg.TYPE_FLOAT}
      }
      gg.getValues(t)
      local x = t[1].value
      local y = t[2].value
      local z = t[3].value

      if math.abs(x) > 100 and math.abs(x) < 200000
        and math.abs(y) > 100 and math.abs(y) < 200000
        and z > -20000 and z < 100000 then
        if not seen[addr] then
          seen[addr] = true
          table.insert(candidates, {address = addr, x = x, y = y, z = z})
        end
      end
    end
  end

  gg.clearResults()
  toast("Candidati totali: " .. #candidates)
end

if #candidates == 0 then
  toast("Nessun candidato trovato.")
  os.exit()
end

-- FASE 3: Verifica con movimento
toast("Trovati " .. #candidates .. " candidati!")
gg.alert("Trovati " .. #candidates .. " candidati XYZ.\n\nADESSO CAMMINA DRITTO per 5 secondi,\npoi premi OK.")

local verified = {}
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

  local dx = math.abs(new_x - c.x)
  local dy = math.abs(new_y - c.y)

  -- Cambiato = ci siamo mossi
  if (dx > 10 or dy > 10) and dx < 50000 and dy < 50000 then
    if math.abs(new_x) > 50 and math.abs(new_x) < 200000
      and math.abs(new_y) > 50 and math.abs(new_y) < 200000 then
      table.insert(verified, {
        address = c.address,
        old_x = c.x, old_y = c.y, old_z = c.z,
        x = new_x, y = new_y, z = new_z,
        delta = dx + dy
      })
    end
  end
end

toast("Verificati con movimento: " .. #verified)

if #verified == 0 then
  -- Mostra tutti i candidati anche senza verifica movimento
  print("\n=== CANDIDATI (non verificati) ===\n")
  local show = math.min(30, #candidates)
  for i = 1, show do
    local c = candidates[i]
    print(string.format("#%d 0x%X: X=%.1f Y=%.1f Z=%.1f", i, c.address, c.x, c.y, c.z))
  end

  gg.alert("Nessun valore cambiato col movimento.\nControlla il log per i candidati.")
  os.exit()
end

-- Raggruppa
local groups = {}
for _, c in ipairs(verified) do
  local found = false
  for _, g in ipairs(groups) do
    if math.abs(c.x - g.x) < 5 and math.abs(c.y - g.y) < 5 then
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

for i = 1, math.min(#groups, 10) do
  local g = groups[i]
  print(string.format("--- Gruppo %d (%d copie) ---", i, g.count))
  print(string.format("  X = %.2f  Y = %.2f  Z = %.2f", g.x, g.y, g.z))
  for j = 1, math.min(5, #g.items) do
    print(string.format("  Addr: 0x%X (delta: %.1f)", g.items[j].address, g.items[j].delta))
  end
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

local best = groups[1]
gg.alert(string.format(
  "POSIZIONE TROVATA!\n\nX = %.1f\nY = %.1f\nZ = %.1f\n\nCopie: %d\nAddr: 0x%X",
  best.x, best.y, best.z, best.count, best.items[1].address))
