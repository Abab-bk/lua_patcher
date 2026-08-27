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

local TARGET_PREFIXES = { "LItem" }

-- Balanced level config (mirrors Gear Spreader bottomlevel/toplevel/maxlevel)
local BOTTOM_LEVEL = 1
local TOP_LEVEL = 46      -- vanilla Daedric weapon/light 46, heavy 48 -> 46 unified, keyword can exceed
local MAX_LEVEL = 50
local TOP_GOLD_FALLBACK = 2500  -- Gear Spreader default for Clothing fallback when no vanilla stats

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
    print("EquipmentInjection: edit TARGET_PREFIXES in this script to match your load order")
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

    if not form.playable then
        return false
    end

    if form.enchantment then
        return false
    end

    return isUnassigned(form)
end

-- -------------------------------------------------------------------------
-- Balanced level calculation
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
    ["Hide"]         = 1,
    ["Fur"]          = 1,
    ["Studded"]      = 1,
    ["Leather"]      = 6,
    ["Elven"]        = 12,
    ["Chitin"]       = 11,
    ["Scaled"]       = 19,
    ["Glass"]        = 36,
    ["Stalhrim"]     = 35,
    ["Dragonscale"]  = 46,
    -- Heavy-ish
    ["Iron"]         = 1,
    ["Banded"]       = 1,
    ["Steel"]        = 6,
    ["SteelPlate"]   = 18,
    ["Dwarven"]      = 12,
    ["Orcish"]       = 25,
    ["Ebony"]        = 32,
    ["Dragonplate"]  = 40,
    ["Daedric"]      = 48,
    -- Shared / DLC
    ["Bonemold"]     = 1,
    ["Nordic"]       = 18,
    ["Imperial"]     = 1,
    ["Stormcloak"]   = 1,
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
        -- also try name fallback for robustness
        local hit = nil
        for mat, lvl in pairs(map) do
            if string.find(ed, mat, 1, true) then
                if not hit or lvl > hit then hit = lvl end
                -- For Armor, "SteelPlate" must beat generic "Steel"
                -- already handled by picking max lvl among matches on same keyword.
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
    for _, a in ipairs(lua_patcher.allArmors()) do
        if VANILLA[a.plugin] and a.playable and not a.enchantment then
            local t = a.armorType -- "Light"/"Heavy"/"Clothing"
            if t == "Light" or t == "Heavy" then
                local r = a.armorRating or 0
                -- ignore 0-rated non-playable glitches
                if r and r > 0 then
                    local st = armorStats[t]
                    if r < st.min then st.min = r end
                    if r > st.max then st.max = r end
                end
            elseif t == "Clothing" then
                local v = a.value or 0
                if v then
                    if v < armorStats.Clothing.minVal then armorStats.Clothing.minVal = v end
                    if v > armorStats.Clothing.maxVal then armorStats.Clothing.maxVal = v end
                end
            end
        end
    end
    for _, w in ipairs(lua_patcher.allWeapons()) do
        if VANILLA[w.plugin] and w.playable and not w.enchantment then
            -- only consider weapon types we actually inject (melee/bow)
            if w.skill == "OneHanded" or w.skill == "TwoHanded" or w.ranged then
                local dps = (w.damage or 0) * (w.speed or 1)
                -- For bows/crossbows damage already captures tier; speed ~0.5-1
                if dps and dps > 0 then
                    if dps < weaponStats.min then weaponStats.min = dps end
                    if dps > weaponStats.max then weaponStats.max = dps end
                end
            end
        end
    end
    -- Fallbacks if load order has no vanilla entries (shouldn't happen)
    for _, k in ipairs({ "Light", "Heavy" }) do
        if armorStats[k].min == math.huge then armorStats[k].min = 5 end
        if armorStats[k].max == -math.huge then armorStats[k].max = 40 end
        if armorStats[k].max <= armorStats[k].min then armorStats[k].max = armorStats[k].min + 10 end
    end
    if armorStats.Clothing.minVal == math.huge then armorStats.Clothing.minVal = 0 end
    if armorStats.Clothing.maxVal == -math.huge then armorStats.Clothing.maxVal = TOP_GOLD_FALLBACK end
    if armorStats.Clothing.maxVal <= armorStats.Clothing.minVal then armorStats.Clothing.maxVal = armorStats.Clothing.minVal + 100 end
    if weaponStats.min == math.huge then weaponStats.min = 4 end
    if weaponStats.max == -math.huge then weaponStats.max = 30 end
    if weaponStats.max <= weaponStats.min then weaponStats.max = weaponStats.min + 10 end

    print(string.format(
        "EquipmentInjection: vanilla stats Light[%.1f-%.1f] Heavy[%.1f-%.1f] ClothingVal[%d-%d] WeaponDPS[%.2f-%.2f]",
        armorStats.Light.min, armorStats.Light.max,
        armorStats.Heavy.min, armorStats.Heavy.max,
        armorStats.Clothing.minVal, armorStats.Clothing.maxVal,
        weaponStats.min, weaponStats.max))
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
    -- 1) keyword path (best accuracy)
    local kwLvl = levelFromKeywords(armor, false)
    if kwLvl then
        return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL)
    end
    -- 2) rating/value interpolation
    local t = armor.armorType
    if t == "Light" or t == "Heavy" then
        local r = armor.armorRating or 0
        local st = armorStats[t]
        return levelFromInterpolation(r, st.min, st.max)
    else -- Clothing or unknown: use gold value (Gear Spreader method=gold for clothing)
        local v = armor.value or 0
        local st = armorStats.Clothing
        -- if modded clothing has extreme value, fall back to 0-2500 scale when beyond vanilla max
        local vmin, vmax = st.minVal, st.maxVal
        if v > vmax then
            vmax = math.max(vmax, TOP_GOLD_FALLBACK)
            -- expand range to include modded max to avoid always clamping to TOP_LEVEL
            -- but keep vanilla distribution stable: interpolate against 0-2500 if vanilla max < 500
            if st.maxVal < 500 then
                vmin = 0
                vmax = TOP_GOLD_FALLBACK
            end
        end
        return levelFromInterpolation(v, vmin, vmax)
    end
end

local function calcWeaponLevel(weapon)
    local kwLvl = levelFromKeywords(weapon, true)
    if kwLvl then
        return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL)
    end
    local dps = (weapon.damage or 0) * (weapon.speed or 1)
    -- For weapons with nil speed (rare), fallback to damage alone scaled to similar range
    if not weapon.speed or weapon.speed == 0 then
        dps = (weapon.damage or 0) * 1.0
        -- adjust min/max to damage-only scale roughly: DPS range ~5-25, damage range ~4-16
        -- keep same weaponStats (DPS) but this still maps reasonably.
    end
    return levelFromInterpolation(dps, weaponStats.min, weaponStats.max)
end

-- Returns the number of lists that actually received the form.
local function inject(form, level)
    local added = 0
    level = clamp(level or BOTTOM_LEVEL, BOTTOM_LEVEL, MAX_LEVEL)
    for _, ll in ipairs(targetLists) do
        if not ll:has(form) then
            ll:add(form, level, 1) -- count stays 1 (vanilla weapons/armors always 1; arrows 12 not injected)
            added = added + 1
        end
    end
    return added
end

local injectedArmor, injectedWeapons = 0, 0
local levelHistArmor, levelHistWeapon = {}, {}

for _, armor in ipairs(lua_patcher.allArmors()) do
    if isInjectionCandidate(armor) and
        (armor.armorType == "Light" or armor.armorType == "Heavy" or armor.armorType == "Clothing") then
        local lvl = calcArmorLevel(armor)
        injectedArmor = injectedArmor + inject(armor, lvl)
        levelHistArmor[lvl] = (levelHistArmor[lvl] or 0) + 1
        print(string.format("EquipmentInjection: armor %s [%s] rating=%.1f value=%s -> level %d",
            armor.identifier, armor.armorType, armor.armorRating or 0, tostring(armor.value), lvl))
    end
end

for _, weapon in ipairs(lua_patcher.allWeapons()) do
    if isInjectionCandidate(weapon) and
        (weapon.skill == "OneHanded" or weapon.skill == "TwoHanded" or weapon.ranged) then
        local lvl = calcWeaponLevel(weapon)
        injectedWeapons = injectedWeapons + inject(weapon, lvl)
        levelHistWeapon[lvl] = (levelHistWeapon[lvl] or 0) + 1
        local dps = (weapon.damage or 0) * (weapon.speed or 1)
        print(string.format("EquipmentInjection: weapon %s [%s] dmg=%s speed=%.2f dps=%.2f value=%s -> level %d",
            weapon.identifier, weapon.skill, tostring(weapon.damage), weapon.speed or 0, dps, tostring(weapon.value), lvl))
    end
end

-- summary histogram for balancing verification
local function histToString(h)
    local parts = {}
    for lvl = BOTTOM_LEVEL, MAX_LEVEL do
        if h[lvl] then table.insert(parts, string.format("%d:%d", lvl, h[lvl])) end
    end
    return #parts > 0 and table.concat(parts, " ") or "none"
end

print(string.format(
    "EquipmentInjection: injected %d armors and %d weapons into %d leveled lists",
    injectedArmor, injectedWeapons, #targetLists))
print("EquipmentInjection: armor level hist -> " .. histToString(levelHistArmor))
print("EquipmentInjection: weapon level hist -> " .. histToString(levelHistWeapon))
