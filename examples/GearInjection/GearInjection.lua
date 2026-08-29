-- LuaPatcher example: GearInjection — config-driven equipment injection
-- priority: 40
--
-- The successor to EquipmentInjection (which it fully replaces): everything is
-- driven by a single config table, so one script covers every "drop this mod's
-- gear into leveled lists" use case:
--
--   * restrict to one plugin ("MyGear.esp") or scan every non-vanilla plugin
--   * include/exclude by EditorID prefix
--   * fixed level for simple cases, or vanilla-style balancing
--     (keyword -> level, fallback rating/DPS interpolation)
--   * skip enchanted / non-playable gear
--
-- Semantics:
--   * runs once at game load on pristine data
--   * a form is injected only if NO leveled list references it in the game's
--     original data (findLeveledListsContaining snapshot), so re-running is
--     always idempotent
--
-- Defaults reproduce the former EquipmentInjection behavior exactly
-- (scan every non-vanilla plugin, balanced levels, skip enchanted and
-- non-playable gear), so existing EquipmentInjection users can switch by
-- copying their config keys over.
--
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/GearInjection_config.lua
-- (flat sibling of the script, mirrors examples/ layout). Missing/invalid keys
-- fall back to the defaults below. Example:
--
--   return {
--     plugin = "MyGear.esp",
--     editorIdPrefixes = { "MG_" },
--     targetPrefixes = { "LItem" },
--     levelMode = "fixed",
--     fixedLevel = 5,
--   }

local VANILLA = {
    ["Skyrim.esm"] = true,
    ["Update.esm"] = true,
    ["Dawnguard.esm"] = true,
    ["HearthFires.esm"] = true,
    ["Dragonborn.esm"] = true,
}

local CONFIG = {
    -- === Targeting ===
    -- "" = every non-vanilla, non-blocked plugin; or a single plugin name.
    plugin = "",
    -- Only inject gear whose EditorID starts with any of these (empty = all).
    editorIdPrefixes = {},
    -- Never inject gear with these EditorID prefixes (physics skeletons etc.).
    excludeEditorIdPrefixes = { "aaSMP", "SMP3" },
    -- Plugins whose gear is never injected.
    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },
    -- Which leveled list EditorID prefixes receive injections.
    targetPrefixes = { "LItem" },

    -- === Categories ===
    enableArmor = true,
    enableWeapon = true,

    -- === Level ===
    -- "balanced": keyword -> level, else rating/DPS interpolation between
    --             bottomLevel and topLevel (mirrors vanilla tier research).
    -- "fixed":    every injected item gets fixedLevel.
    levelMode = "balanced",
    fixedLevel = 1,
    bottomLevel = 1,
    topLevel = 46,   -- vanilla Daedric weapon/light 46, heavy 48 -> 46 unified
    maxLevel = 50,

    -- === Filters ===
    skipEnchanted = true,
    skipNonPlayable = true,

    -- === Distribution ===
    count = 1,               -- count per entry (arrows: 12, gear: 1)
    maxListsPerItem = 15,    -- 0 = every matching list (very strong)
    injectionChance = 1.0,   -- per-list roll 0..1 after subset
}

do
    local loaded = nil
    if lua_patcher.loadLua then
        local ok, result = pcall(lua_patcher.loadLua, "GearInjection_config.lua")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("GearInjection: loadLua failed: %s", tostring(result)))
        end
    end
    if loaded then
        for k, v in pairs(loaded) do
            CONFIG[k] = v
        end
        print("GearInjection: loaded user config")
    else
        print("GearInjection: no user config, using defaults")
    end

    if CONFIG.levelMode ~= "balanced" then CONFIG.levelMode = "fixed" end
    if type(CONFIG.editorIdPrefixes) ~= "table" then CONFIG.editorIdPrefixes = {} end
    if type(CONFIG.excludeEditorIdPrefixes) ~= "table" then CONFIG.excludeEditorIdPrefixes = {} end
    if type(CONFIG.excludedPlugins) ~= "table" then CONFIG.excludedPlugins = {} end
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


local function hasEditorIdPrefix(form, prefixes)
    if not prefixes or #prefixes == 0 then return false end
    local ed = form.editorId
    if not ed then return false end
    for _, prefix in ipairs(prefixes) do
        if string.sub(ed, 1, #prefix) == prefix then return true end
    end
    return false
end


local function findListsByEditorIdPrefix(prefix)
    local out = {}
    for _, ll in ipairs(lua_patcher.allLeveledItems()) do
        local editorId = ll.editorId
        if editorId and string.sub(editorId, 1, #prefix) == prefix then
            table.insert(out, ll)
        end
    end
    print(string.format("GearInjection: prefix '%s' matched %d lists", prefix, #out))
    return out
end


local targetLists = {}
for _, prefix in ipairs(CONFIG.targetPrefixes) do
    for _, ll in ipairs(findListsByEditorIdPrefix(prefix)) do
        table.insert(targetLists, ll)
    end
end

if #targetLists == 0 then
    print("GearInjection: WARNING -- no target lists matched, nothing will be injected")
    print("GearInjection: edit targetPrefixes in GearInjection_config.lua")
end


-- A form counts as "assigned" if ANY leveled list references it in the game's
-- pristine data (plugin-side snapshot cache, cheap to query per item).
local function isUnassigned(form)
    return #lua_patcher.findLeveledListsContaining(form) == 0
end


local function isInjectionCandidate(form)
    if VANILLA[form.plugin] then return false end

    if CONFIG.plugin ~= "" and form.plugin ~= CONFIG.plugin then return false end

    if CONFIG.excludedPlugins[form.plugin] then return false end

    if #CONFIG.editorIdPrefixes > 0 and not hasEditorIdPrefix(form, CONFIG.editorIdPrefixes) then return false end

    if hasEditorIdPrefix(form, CONFIG.excludeEditorIdPrefixes) then return false end

    if CONFIG.skipNonPlayable and not form.playable then return false end

    if CONFIG.skipEnchanted and form.enchantment then return false end

    return isUnassigned(form)
end


-- ---------------------------------------------------------------------------
-- Level calculation
-- ---------------------------------------------------------------------------

-- Material -> level tables derived from UESP research: weapons per
-- Weapons:Leveled_Lists, armor per slot split into Light/Heavy.
local WEAPON_MATERIAL_LEVEL = {
    ["Iron"] = 1,       ["Steel"] = 2,    ["Orcish"] = 6,    ["Dwarven"] = 12,
    ["Nordic"] = 18,    ["Elven"] = 19,   ["Glass"] = 27,    ["Stalhrim"] = 35,
    ["Ebony"] = 36,     ["Daedric"] = 46, ["Dragonbone"] = 46, ["Dragon"] = 46,
}

local ARMOR_MATERIAL_LEVEL = {
    ["Hide"] = 1,       ["Fur"] = 1,      ["Studded"] = 1,   ["Leather"] = 6,
    ["Elven"] = 12,     ["Chitin"] = 11,  ["Scaled"] = 19,   ["Glass"] = 36,
    ["Stalhrim"] = 35,  ["Dragonscale"] = 46,
    ["Iron"] = 1,       ["Banded"] = 1,   ["Steel"] = 6,     ["SteelPlate"] = 18,
    ["Dwarven"] = 12,   ["Orcish"] = 25,  ["Ebony"] = 32,    ["Dragonplate"] = 40,
    ["Daedric"] = 48,   ["Bonemold"] = 1, ["Nordic"] = 18,   ["Imperial"] = 1,
    ["Stormcloak"] = 1,
}

local function levelFromKeywords(form, isWeapon)
    local keywords = form.keywords
    if not keywords or #keywords == 0 then return nil end
    local map = isWeapon and WEAPON_MATERIAL_LEVEL or ARMOR_MATERIAL_LEVEL
    local best = nil
    for _, kw in ipairs(keywords) do
        local ed = kw.editorId or kw.identifier or ""
        for mat, lvl in pairs(map) do
            if string.find(ed, mat, 1, true) then
                if not best or lvl > best then best = lvl end
            end
        end
    end
    return best
end


-- Vanilla rating/DPS ranges per type, collected at runtime for interpolation.
local armorStats = {
    Light = { min = 0, max = 0 },
    Heavy = { min = 0, max = 0 },
    Clothing = { minVal = 0, maxVal = 2500 },
}
local weaponStats = { min = 0, max = 0 }

local function percentile(sorted, p)
    if #sorted == 0 then return nil end
    table.sort(sorted)
    local idx = math.floor(p * #sorted) + 1
    if idx < 1 then idx = 1 end
    if idx > #sorted then idx = #sorted end
    return sorted[idx]
end

local function collectStats()
    if CONFIG.levelMode ~= "balanced" then return end

    local lightVals, heavyVals, clothingVals, weaponVals = {}, {}, {}, {}

    for _, a in ipairs(lua_patcher.allArmors()) do
        if VANILLA[a.plugin] and a.playable and not a.enchantment then
            local t = a.armorType
            if t == "Light" or t == "Heavy" then
                local r = a.armorRating or 0
                if r > 0 then
                    if t == "Light" then table.insert(lightVals, r) else table.insert(heavyVals, r) end
                end
            elseif t == "Clothing" then
                local v = a.value or 0
                table.insert(clothingVals, v)
            end
        end
    end

    for _, w in ipairs(lua_patcher.allWeapons()) do
        if VANILLA[w.plugin] and w.playable and not w.enchantment then
            local skill, ranged = w.skill, w.ranged
            local isWeapon = skill == "OneHanded" or skill == "TwoHanded" or ranged
            if isWeapon then
                local dps = (w.damage or 0) * (w.speed or 1)
                if dps > 0 and dps < 1000 then
                    table.insert(weaponVals, dps)
                end
            end
        end
    end

    local function set(vals, fallbackMin, fallbackMax)
        if #vals == 0 then return fallbackMin, fallbackMax end
        if #vals < 10 then
            local mn, mx = math.huge, -math.huge
            for _, v in ipairs(vals) do
                if v < mn then mn = v end
                if v > mx then mx = v end
            end
            return mn, mx
        end
        local mn, mx = percentile(vals, 0.05), percentile(vals, 0.95)
        if mx <= mn then mx = mn + 10 end
        return mn, mx
    end

    armorStats.Light.min, armorStats.Light.max = set(lightVals, 5, 40)
    armorStats.Heavy.min, armorStats.Heavy.max = set(heavyVals, 5, 40)
    armorStats.Clothing.minVal, armorStats.Clothing.maxVal = set(clothingVals, 0, 2500)
    weaponStats.min, weaponStats.max = set(weaponVals, 4, 30)
    if weaponStats.max <= weaponStats.min then weaponStats.max = weaponStats.min + 10 end
    if armorStats.Light.max <= armorStats.Light.min then armorStats.Light.max = armorStats.Light.min + 10 end
    if armorStats.Heavy.max <= armorStats.Heavy.min then armorStats.Heavy.max = armorStats.Heavy.min + 10 end

    print(string.format(
        "GearInjection: vanilla stats Light[%.1f-%.1f] Heavy[%.1f-%.1f] ClothingVal[%d-%d] WeaponDPS[%.2f-%.2f]",
        armorStats.Light.min, armorStats.Light.max, armorStats.Heavy.min, armorStats.Heavy.max,
        armorStats.Clothing.minVal, armorStats.Clothing.maxVal, weaponStats.min, weaponStats.max))
end

collectStats()


local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end


local function levelFromInterpolation(value, vmin, vmax)
    if vmax <= vmin then return BOTTOM_LEVEL end
    local t = clamp((value - vmin) / (vmax - vmin), 0, 1)
    return clamp(math.floor(BOTTOM_LEVEL + t * (TOP_LEVEL - BOTTOM_LEVEL) + 0.5), BOTTOM_LEVEL, MAX_LEVEL)
end


local function calcArmorLevel(armor)
    if CONFIG.levelMode ~= "balanced" then return CONFIG.fixedLevel end
    local kwLvl = levelFromKeywords(armor, false)
    if kwLvl then return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL) end

    local t = armor.armorType
    if t == "Light" or t == "Heavy" then
        return levelFromInterpolation(armor.armorRating or 0, armorStats[t].min, armorStats[t].max)
    end
    -- Clothing: interpolate by value; outliers (unique/crafted pieces worth
    -- more than any vanilla clothing) scale up instead of clamping.
    local st = armorStats.Clothing
    local vmin, vmax = st.minVal, st.maxVal
    local v = armor.value or 0
    if v > vmax then
        vmax = math.max(vmax, 2500)
        if st.maxVal < 500 then
            vmin = 0
            vmax = 2500
        end
    end
    return levelFromInterpolation(v, vmin, vmax)
end


local function calcWeaponLevel(weapon)
    if CONFIG.levelMode ~= "balanced" then return CONFIG.fixedLevel end
    local kwLvl = levelFromKeywords(weapon, true)
    if kwLvl then return clamp(kwLvl, BOTTOM_LEVEL, MAX_LEVEL) end

    local dps = (weapon.damage or 0) * (weapon.speed or 1)
    if not weapon.speed or weapon.speed == 0 then dps = weapon.damage or 0 end
    return levelFromInterpolation(dps, weaponStats.min, weaponStats.max)
end


-- ---------------------------------------------------------------------------
-- Injection
-- ---------------------------------------------------------------------------

local function selectRandomSubset(all)
    if #all == 0 then return all end
    if CONFIG.injectionChance < 1.0 then
        local tmp = {}
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


local function inject(form, level)
    local added = 0
    level = clamp(level or BOTTOM_LEVEL, BOTTOM_LEVEL, MAX_LEVEL)
    local candidates = selectRandomSubset(targetLists)
    for _, ll in ipairs(candidates) do
        if not ll:has(form) then
            ll:add(form, level, CONFIG.count)
            added = added + 1
        end
    end
    return added
end


local injectedArmor, injectedWeapons = 0, 0
local levelHistArmor, levelHistWeapon = {}, {}

if CONFIG.enableArmor then
    for _, armor in ipairs(lua_patcher.allArmors()) do
        local t = armor.armorType
        if (t == "Light" or t == "Heavy" or t == "Clothing") and isInjectionCandidate(armor) then
            local lvl = calcArmorLevel(armor)
            local added = inject(armor, lvl)
            if added > 0 then
                injectedArmor = injectedArmor + added
                levelHistArmor[lvl] = (levelHistArmor[lvl] or 0) + 1
                print(string.format("GearInjection: armor %s [%s] rating=%.1f -> level %d added=%d",
                    armor.identifier, t, armor.armorRating or 0, lvl, added))
            end
        end
    end
else
    print("GearInjection: armor injection disabled by config")
end

if CONFIG.enableWeapon then
    for _, weapon in ipairs(lua_patcher.allWeapons()) do
        local skill, ranged = weapon.skill, weapon.ranged
        local isWeaponType = skill == "OneHanded" or skill == "TwoHanded" or ranged
        if isWeaponType and isInjectionCandidate(weapon) then
            local lvl = calcWeaponLevel(weapon)
            local added = inject(weapon, lvl)
            if added > 0 then
                injectedWeapons = injectedWeapons + added
                levelHistWeapon[lvl] = (levelHistWeapon[lvl] or 0) + 1
                print(string.format("GearInjection: weapon %s [%s] dmg=%s -> level %d added=%d",
                    weapon.identifier, skill or "unknown", tostring(weapon.damage), lvl, added))
            end
        end
    end
else
    print("GearInjection: weapon injection disabled by config")
end

local function histToString(h)
    local parts = {}
    for lvl = BOTTOM_LEVEL, MAX_LEVEL do
        if h[lvl] then table.insert(parts, string.format("%d:%d", lvl, h[lvl])) end
    end
    return #parts > 0 and table.concat(parts, " ") or "none"
end

print(string.format(
    "GearInjection: injected %d armor-entries and %d weapon-entries into %d leveled lists (plugin='%s' mode=%s maxPerItem=%d chance=%.2f)",
    injectedArmor, injectedWeapons, #targetLists, CONFIG.plugin, CONFIG.levelMode, CONFIG.maxListsPerItem,
    CONFIG.injectionChance))
print("GearInjection: armor level hist (distinct items) -> " .. histToString(levelHistArmor))
print("GearInjection: weapon level hist (distinct items) -> " .. histToString(levelHistWeapon))