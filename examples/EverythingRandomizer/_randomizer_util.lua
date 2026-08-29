-- Shared helpers for the EverythingRandomizer shuffles. Pure functions: no
-- CONFIG, no form-API access. Required by _randomizer_shuffles.lua and
-- _randomizer_jitter.lua via require("_randomizer_util"); modules load
-- relative to the script folder (package.path is prefixed by the loader).
--
-- Module files are never executed as top-level scripts by LuaPatcher.

-- Fisher-Yates on the seeded RNG (math.randomseed set once before all shuffles).
local function fisherYates(t)
    for i = #t, 2, -1 do
        local j = math.random(i)
        t[i], t[j] = t[j], t[i]
    end
    return t
end

local function hasPrefix(editorId, prefixes)
    for _, p in ipairs(prefixes) do
        if string.sub(editorId, 1, #p) == p then return true end
    end
    return false
end

local function hasSuffix(editorId, suffixes)
    for _, s in ipairs(suffixes) do
        if #editorId >= #s and string.sub(editorId, #editorId - #s + 1) == s then return true end
    end
    return false
end

-- Power score of a form for difficulty banding: weapon damage / armor rating /
-- spell cost override. Forms without a natural power (keys, ingredients, ...)
-- score nil and sort to the bottom.
local function formPower(form)
    local t = form.type
    if t == "Weapon" then return form.damage or 0 end
    if t == "Armor" then return form.armorRating or 0 end
    if t == "Spell" then return form.costOverride or 0 end
    return nil
end

-- Enchanted-variant forms (editor IDs like EnchIronDaggerFrost01) carry their
-- enchantment in the form itself; vanilla gates them behind level, the
-- randomizer's flat pools would spread them everywhere.
local function isEnchantedVariant(form)
    return string.sub(form.editorId or "", 1, 4) == "Ench"
end

-- Difficulty banding: pairs n slots with n forms of equal power rank, then
-- perturbs the pairing within a window. window = one band at tierDrift 0
-- (strict bands, difficulty curve kept) and the whole pool at tierDrift 1
-- (full chaos). tierBands slices the power-ordered pool.
local function bandedPairing(n, tierBands, tierDrift)
    local bandSize = math.max(1, math.ceil(n / tierBands))
    local window = math.max(bandSize, math.floor(n * tierDrift))
    local assign = {}
    for i = 1, n do
        assign[i] = i
    end
    for i = 1, n do
        local lo = math.max(1, i - window)
        local hi = math.min(n, i + window)
        local j = math.random(lo, hi)
        assign[i], assign[j] = assign[j], assign[i]
    end
    return assign
end

return {
    fisherYates = fisherYates,
    hasPrefix = hasPrefix,
    hasSuffix = hasSuffix,
    formPower = formPower,
    isEnchantedVariant = isEnchantedVariant,
    bandedPairing = bandedPairing,
}