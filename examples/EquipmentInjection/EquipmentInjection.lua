-- LuaPatcher example: fully automatic equipment injection - BALANCED
--
-- Scans EVERY weapon and armor in the load order (not just one mod's) and
-- injects unassigned, non-vanilla gear into leveled lists. Two filters decide:
--
--   1. The form's source plugin is not in the VANILLA table
--   2. No leveled list (item or character) references the form
--
-- Both must hold, then the gear is distributed by type. This runs once at game
-- load on pristine data, so running it again next launch is always idempotent.
--
-- BALANCED LEVEL CALCULATION (replaces hard-coded 1,1):
--   Vanilla tier research (UESP:Leveled_Lists / Weapons / Armor):
--     Weapons: Iron 1, Steel 2, Orcish 6, Dwarven 12, Nordic 18, Elven 19,
--              Glass 27, Stalhrim 35, Ebony 36, Daedric 46 (95% fallback to Ebony)
--     Light Armor: Hide/Fur 1, Leather 6, Elven/Chitin 12, Scaled 19, Glass 36,
--                  Stalhrim 35, Dragonscale 46
--     Heavy Armor: Iron 1, Steel 6, Dwarven 12, SteelPlate 18, Orcish 25,
--                  Ebony 32, Stalhrim 35, Dragonplate 40, Daedric 48
--     Material table agrees: Iron 1, Steel 4-6, Orcish 7, Dwarven 13, Elven 20,
--                            Glass 28, Ebony 37, Daedric 47 (5%)
--   This script mimics vanilla via two stages:
--     1) Keyword -> level (most accurate, e.g. ArmorMaterialDaedric/WeapMaterialEbony)
--     2) Fallback: rating/DPS interpolation between vanilla min/max per type
--        (like Nexus Gear Spreader: rating/dps ranked, bottomlevel=1 toplevel=46)
--   `count` stays 1 for weapons/armors (arrows use 12 in vanilla, not injected here).
--
-- CONFIG:
--   User config is a flat sibling next to the script:
--     Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection.lua
--     Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection_config.lua
--   The _config.lua file lives BESIDE the script (mirrors examples/ layout).
--   If missing / load fails, defaults below are used (balanced=true). Example:
--     -- EquipmentInjection_config.lua
--     return {
--       balanced = true,
--       bottomLevel = 1,
--       topLevel = 46,
--       maxLevel = 50,
--       targetPrefixes = { "LItem" },
--       enableArmor = true,
--       enableWeapon = true,
--       maxListsPerItem = 15,  -- 0 = all 1290, 15 = random subset (less is sparser)
--       injectionChance = 1.0,
--     }
--   See examples/EquipmentInjection/EquipmentInjection_config.lua
--   (copy to Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection_config.lua)
--
-- NOTE: if nothing gets injected, check the "prefix '...' matched N lists"
-- lines in your log -- the target lists below must match lists that actually
-- exist in your load order (editorIDs are looked up in the game's own table,
-- so the names must be exactly as shown in xEdit).

local VANILLA = {
    ["Skyrim.esm"] = true,
    ["Update.esm"] = true,
    ["Dawnguard.esm"] = true,
    ["HearthFires.esm"] = true,
    ["Dragonborn.esm"] = true,
}

local BLOCK_LIST = {
    ["CBBE.esp"] = true,
    ["3BBB.esp"] = true,
    ["XPMSE.esp"] = true,
}

local BLOCK_PREFIX_LIST = {
    "aaSMP", "SMP3"
}

-- -------------------------------------------------------------------------
-- User config with safe fallback (never stored inside mod body)
-- -------------------------------------------------------------------------
local CONFIG = {
    balanced = true, -- false => legacy ll:add(form,1,1) for all
    bottomLevel = 1,
    topLevel = 46,   -- vanilla Daedric weapon/light 46, heavy 48 -> 46 unified
    maxLevel = 50,
    targetPrefixes = { "LItem" },
    enableArmor = true,
    enableWeapon = true,
    maxListsPerItem = 15,  -- 0 = all matching (too strong), 15 = random subset per item
    injectionChance = 1.0, -- per-list roll 0..1 after subset
}

do
    local loaded = nil
    if lua_patcher.tryLoadConfig then
        local ok, result = pcall(lua_patcher.tryLoadConfig, "EquipmentInjection")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("EquipmentInjection: tryLoadConfig failed: %s", tostring(result)))
        end
    else
        print("EquipmentInjection: lua_patcher.tryLoadConfig not available (old plugin?), using defaults")
    end

    if loaded then
        for k, v in pairs(loaded) do
            CONFIG[k] = v
        end
        print(
            "EquipmentInjection: loaded user config from Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection_config.lua")
    else
        print(string.format("EquipmentInjection: no user config, using defaults (balanced=%s)", tostring(CONFIG.balanced)))
    end

    -- normalize targetPrefixes
    if type(CONFIG.targetPrefixes) ~= "table" or #CONFIG.targetPrefixes == 0 then
        CONFIG.targetPrefixes = { "LItem" }
    end
    if type(CONFIG.maxListsPerItem) ~= "number" or CONFIG.maxListsPerItem < 0 then CONFIG.maxListsPerItem = 15 end
    if type(CONFIG.injectionChance) ~= "number" or CONFIG.injectionChance < 0 then CONFIG.injectionChance = 1.0 end
    if CONFIG.injectionChance > 1 then CONFIG.injectionChance = 1 end
end

local BOTTOM_LEVEL = CONFIG.bottomLevel
local TOP_LEVEL = CONFIG.topLevel
local MAX_LEVEL = CONFIG.maxLevel
local TOP_GOLD_FALLBACK = 2500 -- Gear Spreader default for Clothing fallback when no vanilla stats
local TARGET_PREFIXES = CONFIG.targetPrefixes


local function findListsByEditorIdPrefix(prefix)
    local out = {}
    for _, ll in ipairs(lua_patcher.allLeveledItems()) do
        local editorId = ll.editorId
        if editorId and string.sub(editorId, 1, #prefix) == prefix then
            table.insert(out, ll)
        end
    end
    print(string.format("EquipmentInjection: prefix '%s' matched %d lists", prefix, #out))
    return out
end


local targetLists = {}
for _, prefix in ipairs(TARGET_PREFIXES) do
    for _, ll in ipairs(findListsByEditorIdPrefix(prefix)) do
        table.insert(targetLists, ll)
    end
end


if #targetLists == 0 then
    print("EquipmentInjection: WARNING -- no target lists matched, nothing will be injected")
    print(
        "EquipmentInjection: edit targetPrefixes in Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection_config.lua")
end


-- A form counts as "assigned" if ANY leveled list references it.
-- The reverse index is cached by the plugin (snapshot of the game's original
-- data, taken on first call), so this is cheap to call per item.
local function isUnassigned(form)
    return #lua_patcher.findLeveledListsContaining(form) == 0
end


local function isInjectionCandidate(form)
    if VANILLA[form.plugin] then
        return false
    end

    if BLOCK_LIST[form.plugin] then
        return false
    end

    -- Block by EditorID prefix (e.g. SMP physics internals like aaSMP / SMP3)
    local ed = form.editorId
    if ed then
        for _, prefix in ipairs(BLOCK_PREFIX_LIST) do
            if string.sub(ed, 1, #prefix) == prefix then
                return false
            end
        end
    end

    if not form.playable then
        return false
    end

    if form.enchantment then
        return false
    end

    return isUnassigned(form)
end


-- -------------------------------------------------------------------------
-- Balanced level calculation (only used when CONFIG.balanced == true)
-- -------------------------------------------------------------------------

-- Material -> level tables derived from UESP. Weapon levels use
-- Weapons:Leveled_Lists mapping (Iron1,Steel2,Orcish6,Dwarven12,Elven19,Glass27,Ebony36,Daedric46).
-- Armor levels use Armor:Leveled_Lists per slot (Light/Heavy split).
local WEAPON_MATERIAL_LEVEL = {
    ["Iron"]       = 1,
    ["Steel"]      = 2,
    ["Orcish"]     = 6,
    ["Dwarven"]    = 12,
    ["Nordic"]     = 18,
    ["Elven"]      = 19,
    ["Glass"]      = 27,
    ["Stalhrim"]   = 35,
    ["Ebony"]      = 36,
    ["Daedric"]    = 46,
    ["Dragonbone"] = 46,
    ["Dragon"]     = 46, -- Dragonplate/bone fallback
}

local ARMOR_MATERIAL_LEVEL = {
    -- Light-ish
    ["Hide"]        = 1,
    ["Fur"]         = 1,
    ["Studded"]     = 1,
    ["Leather"]     = 6,
    ["Elven"]       = 12,
    ["Chitin"]      = 11,
    ["Scaled"]      = 19,
    ["Glass"]       = 36,
    ["Stalhrim"]    = 35,
    ["Dragonscale"] = 46,
    -- Heavy-ish
    ["Iron"]        = 1,
    ["Banded"]      = 1,
    ["Steel"]       = 6,
    ["SteelPlate"]  = 18,
    ["Dwarven"]     = 12,
    ["Orcish"]      = 25,
    ["Ebony"]       = 32,
    ["Dragonplate"] = 40,
    ["Daedric"]     = 48,
    -- Shared / DLC
    ["Bonemold"]    = 1,
    ["Nordic"]      = 18,
    ["Imperial"]    = 1,
    ["Stormcloak"]  = 1,
}

-- Try to resolve level from keywords by substring match (covers both
-- WeapMaterialX / ArmorMaterialX / MaterialX naming variants from CK).
-- Returns nil if no material keyword found.
local function levelFromKeywords(form, isWeapon)
    local keywords = form.keywords
    if not keywords or #keywords == 0 then
        return nil
    end
    local map = isWeapon and WEAPON_MATERIAL_LEVEL or ARMOR_MATERIAL_LEVEL
    local best = nil
    for _, kw in ipairs(keywords) do
        -- kw is a Form; editorId is the CK EditorID like "WeapMaterialDaedric"
        local ed = kw.editorId or kw.identifier or ""
        local hit = nil
        for mat, lvl in pairs(map) do
            if string.find(ed, mat, 1, true) then
                if not hit or lvl > hit then hit = lvl end
            end
        end
        if hit then
            if not best or hit > best then best = hit end
        end
    end
    return best
end

-- Collect vanilla rating/DPS stats for interpolation fallback.
-- Per-type so Light top (Glass 36) doesn't get undervalued vs Heavy top.
local armorStats = {
    Light    = { min = math.huge, max = -math.huge },
    Heavy    = { min = math.huge, max = -math.huge },
    Clothing = { minVal = math.huge, maxVal = -math.huge },
}
local weaponStats = { min = math.huge, max = -math.huge }

local function collectStats()
    if not CONFIG.balanced then
        return
    end

    local lightVals, heavyVals, clothingVals, weaponVals = {}, {}, {}, {}

    for _, a in ipairs(lua_patcher.allArmors()) do
        if VANILLA[a.plugin] and a.playable and not a.enchantment then
            local ok, t = pcall(function() return a.armorType end)
            if ok then
                if t == "Light" or t == "Heavy" then
                    local r = a.armorRating or 0
                    if r and r > 0 then
                        if t == "Light" then table.insert(lightVals, r) else table.insert(heavyVals, r) end
                    end
                elseif t == "Clothing" then
                    local v = a.value or 0
                    if v then table.insert(clothingVals, v) end
                end
            end
        end
    end

    for _, w in ipairs(lua_patcher.allWeapons()) do
        if VANILLA[w.plugin] and w.playable and not w.enchantment then
            local okSkill, skill = pcall(function() return w.skill end)
            local okRanged, ranged = pcall(function() return w.ranged end)
            local isWeapon = false
            if okSkill and (skill == "OneHanded" or skill == "TwoHanded") then isWeapon = true end
            if okRanged and ranged then isWeapon = true end
            if isWeapon then
                local okD, dmg = pcall(function() return w.damage end)
                local okS, spd = pcall(function() return w.speed end)
                local dps = (okD and dmg or 0) * ((okS and spd or 1) or 1)
                if dps and dps > 0 and dps < 1000 then
                    table.insert(weaponVals, dps)
                elseif dps and dps >= 1000 then
                    print(string.format("EquipmentInjection: outlier weapon %s DPS=%.2f ignored for stats",
                        w.identifier or "unknown", dps))
                end
            end
        end
    end

    local function percentile(sorted, p)
        if #sorted == 0 then return nil end
        table.sort(sorted)
        local idx = math.floor(p * #sorted) + 1
        if idx < 1 then idx = 1 end
        if idx > #sorted then idx = #sorted end
        return sorted[idx]
    end

    local function robustMinMax(vals, fallbackMin, fallbackMax)
        if #vals == 0 then return fallbackMin, fallbackMax end
        if #vals < 10 then
            local mn, mx = math.huge, -math.huge
            for _, v in ipairs(vals) do
                if v < mn then mn = v end; if v > mx then mx = v end
            end
            return mn, mx
        end
        table.sort(vals)
        local mn = percentile(vals, 0.05)
        local mx = percentile(vals, 0.95)
        if mx <= mn then mx = mn + 10 end
        return mn, mx
    end

    local mn, mx = robustMinMax(lightVals, 5, 40)
    armorStats.Light.min, armorStats.Light.max = mn, mx
    mn, mx = robustMinMax(heavyVals, 5, 40)
    armorStats.Heavy.min, armorStats.Heavy.max = mn, mx
    if #clothingVals == 0 then
        armorStats.Clothing.minVal, armorStats.Clothing.maxVal = 0, TOP_GOLD_FALLBACK
    else
        local cMin, cMax = robustMinMax(clothingVals, 0, TOP_GOLD_FALLBACK)
        armorStats.Clothing.minVal, armorStats.Clothing.maxVal = cMin, cMax
        if armorStats.Clothing.maxVal <= armorStats.Clothing.minVal then
            armorStats.Clothing.maxVal = armorStats.Clothing.minVal + 100
        end
    end

    if #weaponVals == 0 then
        weaponStats.min, weaponStats.max = 4, 30
    else
        local wMin, wMax = robustMinMax(weaponVals, 4, 30)
        weaponStats.min, weaponStats.max = wMin, wMax
    end
    if weaponStats.max <= weaponStats.min then weaponStats.max = weaponStats.min + 10 end
    if armorStats.Light.max <= armorStats.Light.min then armorStats.Light.max = armorStats.Light.min + 10 end
    if armorStats.Heavy.max <= armorStats.Heavy.min then armorStats.Heavy.max = armorStats.Heavy.min + 10 end

    if weaponStats.max > 100 then
        print(string.format("EquipmentInjection: weaponStats max %.2f clamped to 50 (outlier)", weaponStats.max))
        weaponStats.max = 50
    end

    print(string.format(
        "EquipmentInjection: vanilla stats Light[%.1f-%.1f] (%d samples) Heavy[%.1f-%.1f] (%d) ClothingVal[%d-%d] (%d) WeaponDPS[%.2f-%.2f] (%d)",
        armorStats.Light.min, armorStats.Light.max, #lightVals,
        armorStats.Heavy.min, armorStats.Heavy.max, #heavyVals,
        armorStats.Clothing.minVal, armorStats.Clothing.maxVal, #clothingVals,
        weaponStats.min, weaponStats.max, #weaponVals)
    )
end

collectStats()

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end


local function levelFromInterpolation(value, vmin, vmax)
    if vmax <= vmin then return BOTTOM_LEVEL end
    local t = (value - vmin) / (vmax - vmin)

    t = clamp(t, 0, 1)
    local lvl = math.floor(BOTTOM_LEVEL + t * (TOP_LEVEL - BOTTOM_LEVEL) + 0.5)

    return clamp(lvl, BOTTOM_LEVEL, MAX_LEVEL)
end


local function calcArmorLevel(armor)
    if not CONFIG.balanced then return BOTTOM_LEVEL end
    local kwLvl = levelFromKeywords(armor, false)

    if kwLvl then
        return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL)
    end

    local t = armor.armorType

    if t == "Light" or t == "Heavy" then
        local r = armor.armorRating or 0
        local st = armorStats[t]
        return levelFromInterpolation(r, st.min, st.max)
    else
        local v = armor.value or 0
        local st = armorStats.Clothing
        local vmin, vmax = st.minVal, st.maxVal
        if v > vmax then
            vmax = math.max(vmax, TOP_GOLD_FALLBACK)
            if st.maxVal < 500 then
                vmin = 0
                vmax = TOP_GOLD_FALLBACK
            end
        end
        return levelFromInterpolation(v, vmin, vmax)
    end
end


local function calcWeaponLevel(weapon)
    if not CONFIG.balanced then return BOTTOM_LEVEL end
    local kwLvl = levelFromKeywords(weapon, true)

    if kwLvl then
        return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL)
    end

    local dps = (weapon.damage or 0) * (weapon.speed or 1)
    if not weapon.speed or weapon.speed == 0 then
        dps = (weapon.damage or 0) * 1.0
    end

    return levelFromInterpolation(dps, weaponStats.min, weaponStats.max)
end


-- Random subset helper (maxListsPerItem / injectionChance) on top of all targetLists
local function selectRandomSubset(all)
    if #all == 0 then return all end
    local tmp = {}
    if CONFIG.injectionChance < 1.0 then
        for _, ll in ipairs(all) do
            if math.random() <= CONFIG.injectionChance then table.insert(tmp, ll) end
        end
        if #tmp == 0 then return tmp end
        all = tmp
    end
    if CONFIG.maxListsPerItem > 0 and #all > CONFIG.maxListsPerItem then
        for i = #all, 2, -1 do
            local j = math.random(i)
            all[i], all[j] = all[j], all[i]
        end
        local truncated = {}
        for i = 1, CONFIG.maxListsPerItem do truncated[i] = all[i] end
        all = truncated
    end
    return all
end

-- Returns the number of lists that actually received the form (after random subset).
local function inject(form, level)
    local added = 0
    level = clamp(level or BOTTOM_LEVEL, BOTTOM_LEVEL, MAX_LEVEL)
    local candidates = selectRandomSubset(targetLists)
    for _, ll in ipairs(candidates) do
        if not ll:has(form) then
            ll:add(form, level, 1)
            added = added + 1
        end
    end
    return added, #candidates
end

local injectedArmor, injectedWeapons = 0, 0
local levelHistArmor, levelHistWeapon = {}, {}

if CONFIG.enableArmor then
    for _, armor in ipairs(lua_patcher.allArmors()) do
        local okType, t = pcall(function() return armor.armorType end)
        local isArmorType = okType and (t == "Light" or t == "Heavy" or t == "Clothing")
        if isInjectionCandidate(armor) and isArmorType then
            local lvl = calcArmorLevel(armor)
            local added, cand = inject(armor, lvl)
            if added > 0 then levelHistArmor[lvl] = (levelHistArmor[lvl] or 0) + 1 end
            injectedArmor = injectedArmor + added
            print(string.format(
                "EquipmentInjection: armor %s [%s] rating=%.1f value=%s -> level %d candidates=%d added=%d",
                armor.identifier, t or "unknown", armor.armorRating or 0, tostring(armor.value), lvl,
                cand or #targetLists, added))
        end
    end
else
    print("EquipmentInjection: armor injection disabled by config")
end


if CONFIG.enableWeapon then
    for _, weapon in ipairs(lua_patcher.allWeapons()) do
        local okSkill, skill = pcall(function() return weapon.skill end)
        local okRanged, ranged = pcall(function() return weapon.ranged end)
        local isWeaponType = false
        if okSkill and (skill == "OneHanded" or skill == "TwoHanded") then isWeaponType = true end
        if okRanged and ranged then isWeaponType = true end
        if isInjectionCandidate(weapon) and isWeaponType then
            local lvl = calcWeaponLevel(weapon)
            local added, cand = inject(weapon, lvl)
            if added > 0 then levelHistWeapon[lvl] = (levelHistWeapon[lvl] or 0) + 1 end
            injectedWeapons = injectedWeapons + added
            local dps = (weapon.damage or 0) * (weapon.speed or 1)
            if not weapon.speed or weapon.speed == 0 then dps = (weapon.damage or 0) * 1.0 end
            local sk = okSkill and skill or "unknown"
            print(string.format(
                "EquipmentInjection: weapon %s [%s] dmg=%s speed=%.2f dps=%.2f value=%s -> level %d candidates=%d added=%d",
                weapon.identifier, sk, tostring(weapon.damage), weapon.speed or 0, dps, tostring(weapon.value), lvl,
                cand or #targetLists, added))
        end
    end
else
    print("EquipmentInjection: weapon injection disabled by config")
end


local function histToString(h)
    local parts = {}

    for lvl = BOTTOM_LEVEL, MAX_LEVEL do
        if h[lvl] then table.insert(parts, string.format("%d:%d", lvl, h[lvl])) end
    end

    return #parts > 0 and table.concat(parts, " ") or "none"
end


print(string.format(
    "EquipmentInjection: injected %d armor-entries and %d weapon-entries into %d leveled lists (balanced=%s maxPerItem=%d chance=%.2f)",
    injectedArmor, injectedWeapons, #targetLists, tostring(CONFIG.balanced), CONFIG.maxListsPerItem,
    CONFIG.injectionChance))
print("EquipmentInjection: armor level hist (distinct items) -> " .. histToString(levelHistArmor))
print("EquipmentInjection: weapon level hist (distinct items) -> " .. histToString(levelHistWeapon))
