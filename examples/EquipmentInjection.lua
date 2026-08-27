-- LuaPatcher example: fully automatic equipment injection
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

-- Returns the number of lists that actually received the form.
local function inject(form)
    local added = 0
    for _, ll in ipairs(targetLists) do
        if not ll:has(form) then
            ll:add(form, 1, 1)
            added = added + 1
        end
    end
    return added
end

local injectedArmor, injectedWeapons = 0, 0

for _, armor in ipairs(lua_patcher.allArmors()) do
    if isInjectionCandidate(armor) and
        (armor.armorType == "Light" or armor.armorType == "Heavy" or armor.armorType == "Clothing") then
        injectedArmor = injectedArmor + inject(armor)
    end
end

for _, weapon in ipairs(lua_patcher.allWeapons()) do
    if isInjectionCandidate(weapon) and
        (weapon.skill == "OneHanded" or weapon.skill == "TwoHanded" or weapon.ranged) then
        injectedWeapons = injectedWeapons + inject(weapon)
    end
end

print(string.format(
    "EquipmentInjection: injected %d armors and %d weapons into leveled lists",
    injectedArmor, injectedWeapons))
