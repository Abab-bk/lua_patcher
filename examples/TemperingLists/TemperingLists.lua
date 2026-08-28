-- LuaPatcher example: TemperingLists — make mod gear temperable
-- priority: 30 (needs material keywords from KeywordFixer, runs before injection)
--
-- The canonical FormList use case: vanilla smithing recipes (ConstructibleObject)
-- check membership in material sets — WeapMaterialIronSet, ArmorMaterialSteelSet,
-- ... — to decide which items can be tempered with which material. Mod gear
-- usually carries the material keyword (e.g. WeapMaterialDaedric) but never
-- joined its material set, so it cannot be tempered.
--
-- This script scans non-vanilla gear and, for every piece with a
-- WeapMaterialX / ArmorMaterialX keyword, joins the matching XSet FormList.
-- Joining is set semantics (adding a present form is a no-op), so re-running
-- is always idempotent.
--
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/TemperingLists_config.lua
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
    enableWeapons = true,
    enableArmors = true,
    skipNonPlayable = true,

    -- Targeting (same shape as GearInjection/KeywordFixer):
    plugin = "",                 -- "" = all non-vanilla plugins, or one plugin
    editorIdPrefixes = {},       -- only gear whose EditorID starts with these
    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },
}

do
    local loaded = nil
    if lua_patcher.tryLoadConfig then
        local ok, result = pcall(lua_patcher.tryLoadConfig, "TemperingLists")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("TemperingLists: tryLoadConfig failed: %s", tostring(result)))
        end
    end
    if loaded then
        for k, v in pairs(loaded) do
            CONFIG[k] = v
        end
        print("TemperingLists: loaded user config")
    else
        print("TemperingLists: no user config, using defaults")
    end

    if type(CONFIG.editorIdPrefixes) ~= "table" then CONFIG.editorIdPrefixes = {} end
    if type(CONFIG.excludedPlugins) ~= "table" then CONFIG.excludedPlugins = {} end
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

-- Returns the material set FormList for a form's first material keyword,
-- or nil. EditorID convention: keyword "WeapMaterialSteel" -> list
-- "WeapMaterialSteelSet" (same for ArmorMaterial*).
local function materialSetFor(form)
    for _, kw in ipairs(form.keywords) do
        local ed = kw.editorId or ""
        if string.sub(ed, 1, 12) == "WeapMaterial" or string.sub(ed, 1, 13) == "ArmorMaterial" then
            local list = lua_patcher.getForm(ed .. "Set")
            if list and list.type == "FormList" then
                return list
            end
        end
    end
    return nil
end

if not CONFIG.enabled then
    print("TemperingLists: disabled by config")
    return
end

local joined = 0

local function joinSets(forms)
    for _, form in ipairs(forms) do
        if matchesTargeting(form) then
            local list = materialSetFor(form)
            if list and not list:has(form) then
                if list:add(form) then
                    joined = joined + 1
                    print(string.format("TemperingLists: %s -> %s", form.identifier, list.editorId))
                end
            end
        end
    end
end

if CONFIG.enableWeapons then
    joinSets(lua_patcher.allWeapons())
end
if CONFIG.enableArmors then
    joinSets(lua_patcher.allArmors())
end

print(string.format("TemperingLists: done — %d items joined their material set", joined))