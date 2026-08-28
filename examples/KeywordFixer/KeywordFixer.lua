-- LuaPatcher example: KeywordFixer — missing/invalid keyword fixes for mod gear
-- priority: 20 (after stat rebalances, before TemperingLists/GearInjection)
--
-- Problems this solves:
--   1) Mod armors/weapons often lack material keywords (WeapMaterialSteel,
--      ArmorMaterialDaedric, ...), so smithing perks, "same material" rules and
--      material-based mods silently don't apply to them. This script derives
--      the tier from the item's stats (damage / armorRating + type) and adds
--      the matching keyword.
--   2) Optionally adds slot keywords (ArmorBody, ArmorHands, ...) from the
--      item's biped slots, so slot-based rules/perks apply.
--   3) Declarative custom rules: add/remove any keywords on gear matched by
--      plugin and/or EditorID prefix (e.g. drop a conflicting keyword that a
--      mod adds to everything).
--
-- Only non-vanilla gear is touched by default; targeting is config-driven.
-- Adding a keyword is idempotent (the engine's AddKeyword is a no-op when the
-- keyword is already present), so re-running is always safe.
--
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/KeywordFixer_config.lua
-- (flat sibling of the script, mirrors examples/ layout).

local VANILLA = {
    ["Skyrim.esm"] = true,
    ["Update.esm"] = true,
    ["Dawnguard.esm"] = true,
    ["HearthFires.esm"] = true,
    ["Dragonborn.esm"] = true,
}

local CONFIG = {
    enabled = true,
    fixMaterialKeywords = true,
    addSlotKeywords = false,
    skipNonPlayable = true,

    -- Targeting (same shape as GearInjection):
    plugin = "",                 -- "" = all non-vanilla plugins, or one plugin
    editorIdPrefixes = {},       -- only gear whose EditorID starts with these
    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },

    -- Declarative rules: { plugin = "X.esp", editorIdPrefix = "X_",
    --                      add = { "kwEditorId", ... }, remove = { "kwEditorId", ... } }
    -- A missing field matches anything; both add and remove are optional.
    rules = {},
}

do
    local loaded = nil
    if lua_patcher.tryLoadConfig then
        local ok, result = pcall(lua_patcher.tryLoadConfig, "KeywordFixer")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("KeywordFixer: tryLoadConfig failed: %s", tostring(result)))
        end
    end
    if loaded then
        for k, v in pairs(loaded) do
            CONFIG[k] = v
        end
        print("KeywordFixer: loaded user config")
    else
        print("KeywordFixer: no user config, using defaults")
    end

    if type(CONFIG.editorIdPrefixes) ~= "table" then CONFIG.editorIdPrefixes = {} end
    if type(CONFIG.excludedPlugins) ~= "table" then CONFIG.excludedPlugins = {} end
    if type(CONFIG.rules) ~= "table" then CONFIG.rules = {} end
end

-- ---------------------------------------------------------------------------
-- Stat tier tables (same research as GearInjection/WeaponArmorTweak, UESP)
-- ---------------------------------------------------------------------------

-- Weapon tier by damage (a tier matches when damage <= maxDmg).
local WEAPON_TIERS = {
    { mat = "Iron",    maxDmg = 8 },
    { mat = "Steel",   maxDmg = 9 },
    { mat = "Orcish",  maxDmg = 11 },
    { mat = "Dwarven", maxDmg = 12 },
    { mat = "Elven",   maxDmg = 13 },
    { mat = "Glass",   maxDmg = 14 },
    { mat = "Ebony",   maxDmg = 16 },
    { mat = "Daedric", maxDmg = math.huge },
}

-- Armor tier by armorRating, split into Heavy/Light.
local ARMOR_TIERS = {
    Heavy = {
        { mat = "Iron",        maxRating = 10 },
        { mat = "Steel",       maxRating = 18 },
        { mat = "Dwarven",     maxRating = 22 },
        { mat = "Orcish",      maxRating = 28 },
        { mat = "Ebony",       maxRating = 32 },
        { mat = "Daedric",     maxRating = math.huge },
    },
    Light = {
        { mat = "Hide",        maxRating = 6 },
        { mat = "Leather",     maxRating = 8 },
        { mat = "Elven",       maxRating = 11 },
        { mat = "Scaled",      maxRating = 15 },
        { mat = "Glass",       maxRating = 18 },
        { mat = "Dragonscale", maxRating = math.huge },
    },
}

-- Biped slot -> keyword EditorID (only when addSlotKeywords is enabled).
local SLOT_KEYWORDS = {
    ["Head"]    = "ArmorHead",
    ["Body"]    = "ArmorBody",
    ["Hands"]   = "ArmorHands",
    ["Feet"]    = "ArmorFeet",
    ["Shield"]  = "ArmorShield",
    ["Tail"]    = "ArmorTail",
}

-- ---------------------------------------------------------------------------
-- Helpers
-- ---------------------------------------------------------------------------

local function keywordForm(editorId)
    local f = lua_patcher.getForm(editorId)
    if f and f.type == "Keyword" then return f end
    return nil
end


local function hasMaterialKeyword(form)
    for _, kw in ipairs(form.keywords) do
        local ed = kw.editorId or ""
        if string.find(ed, "Material") then return true end
    end
    return false
end


local function hasKeywordEditorId(form, editorId)
    for _, kw in ipairs(form.keywords) do
        if kw.editorId == editorId then return true end
    end
    return false
end


local function findMaterialKeyword(mat, isWeapon)
    local candidates
    if isWeapon then
        candidates = { "WeapMaterial" .. mat, "WeaponMaterial" .. mat }
    else
        candidates = { "ArmorMaterial" .. mat, "ArmorMaterialLight" .. mat, "ArmorMaterialHeavy" .. mat }
    end
    for _, ed in ipairs(candidates) do
        local kw = keywordForm(ed)
        if kw then return kw end
    end
    return nil
end


local function tierForWeapon(weapon)
    local dmg = weapon.damage or 0
    for _, t in ipairs(WEAPON_TIERS) do
        if dmg <= t.maxDmg then return t.mat end
    end
    return WEAPON_TIERS[#WEAPON_TIERS].mat
end


local function tierForArmor(armor)
    local r = armor.armorRating or 0
    local tiers = ARMOR_TIERS[armor.armorType] or ARMOR_TIERS.Light
    for _, t in ipairs(tiers) do
        if r <= t.maxRating then return t.mat end
    end
    return tiers[#tiers].mat
end


local function matchesTargeting(form)
    if VANILLA[form.plugin] then return false end
    if CONFIG.plugin ~= "" and form.plugin ~= CONFIG.plugin then return false end
    if CONFIG.excludedPlugins[form.plugin] then return false end
    if #CONFIG.editorIdPrefixes > 0 then
        local ed = form.editorId
        local hit = false
        if ed then
            for _, prefix in ipairs(CONFIG.editorIdPrefixes) do
                if string.sub(ed, 1, #prefix) == prefix then hit = true break end
            end
        end
        if not hit then return false end
    end
    if CONFIG.skipNonPlayable and not form.playable then return false end
    return true
end


local function addKeywordSafely(form, editorId, counter)
    local kw = keywordForm(editorId)
    if not kw then return end
    if hasKeywordEditorId(form, kw.editorId) then return end
    if form:addKeyword(kw) then
        counter[1] = counter[1] + 1
        print(string.format("KeywordFixer: %s -> +%s", form.identifier, kw.editorId))
    end
end

-- ---------------------------------------------------------------------------
-- Main
-- ---------------------------------------------------------------------------

if not CONFIG.enabled then
    print("KeywordFixer: disabled by config")
    return
end

local fixedMaterial, fixedSlot, ruleChanges = 0, 0, 0
local materialCounter = { 0 }
local slotCounter = { 0 }
local ruleCounter = { 0 }

local weapons = lua_patcher.allWeapons()
local armors = lua_patcher.allArmors()

if CONFIG.fixMaterialKeywords then
    for _, weapon in ipairs(weapons) do
        if matchesTargeting(weapon) and not hasMaterialKeyword(weapon) then
            local kw = findMaterialKeyword(tierForWeapon(weapon), true)
            if kw then
                addKeywordSafely(weapon, kw.editorId, materialCounter)
            end
        end
    end

    for _, armor in ipairs(armors) do
        if matchesTargeting(armor) and not hasMaterialKeyword(armor) then
            local kw = findMaterialKeyword(tierForArmor(armor), false)
            if kw then
                addKeywordSafely(armor, kw.editorId, materialCounter)
            end
        end
    end
    fixedMaterial = materialCounter[1]
end

if CONFIG.addSlotKeywords then
    for _, armor in ipairs(armors) do
        if matchesTargeting(armor) then
            for _, slot in ipairs(armor.slots) do
                local ed = SLOT_KEYWORDS[slot]
                if ed then
                    addKeywordSafely(armor, ed, slotCounter)
                end
            end
        end
    end
    fixedSlot = slotCounter[1]
end

for _, rule in ipairs(CONFIG.rules) do
    if type(rule) ~= "table" then
        print("KeywordFixer: skipping malformed rule (expected a table)")
    end
    local adds = rule.add or {}
    local removes = rule.remove or {}
    for _, formList in ipairs({ weapons, armors }) do
        for _, form in ipairs(formList) do
            if VANILLA[form.plugin] then goto continue end
            if rule.plugin and form.plugin ~= rule.plugin then goto continue end
            if rule.editorIdPrefix then
                local ed = form.editorId
                if not ed or string.sub(ed, 1, #rule.editorIdPrefix) ~= rule.editorIdPrefix then goto continue end
            end
            for _, ed in ipairs(adds) do
                addKeywordSafely(form, ed, ruleCounter)
            end
            for _, ed in ipairs(removes) do
                local kw = keywordForm(ed)
                if kw and hasKeywordEditorId(form, kw.editorId) and form:removeKeyword(kw) then
                    ruleCounter[1] = ruleCounter[1] + 1
                    print(string.format("KeywordFixer: %s -> -%s", form.identifier, ed))
                end
            end
            ::continue::
        end
    end
    ruleChanges = ruleCounter[1]
end

print(string.format(
    "KeywordFixer: done — materialKeywords=%d slotKeywords=%d ruleChanges=%d",
    fixedMaterial, fixedSlot, ruleChanges))