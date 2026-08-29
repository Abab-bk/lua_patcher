-- LuaPatcher example: EverythingRandomizer — shuffle the world
-- priority: 50
--
-- "everything randomizer":
--   * every leveled list entry is swapped with another entry of the SAME form
--     type (weapons <-> weapons, armor <-> armor, ...), preserving counts and
--     levels — total items and type pools are conserved
--   * leveled spell lists shuffle the same way (spells <-> spells)
--   * FormList contents shuffle too, excluding "*Set" smithing material sets
--   * container contents swap between containers, same-form-type pools
--   * ingredient / potion / enchantment effect slots swap within their pools
--   * shout spell variations swap between shouts
--   * gear stats get a seeded jitter (damage / armorRating / weight / value)
--   * enchantments swap between gear (weapon-type <-> weapon-type, armor-type
--     <-> armor-type; unenchanted gear stays unenchanted)
--   * encounter zone difficulty swaps order-preservingly (each zone only
--     trades levels with a neighbor within zoneSwapWindow difficulty tiers)
--   * crafting recipe outputs swap within their own form type (banded by
--     power); recipe ingredients can swap too (off by default — it makes
--     recipes hard to fulfill); tempering material sets never leave
--     their recipes
--   * lights get radius/color/fade jitter; NPCs can get level/attribute
--     jitter + skill shuffles (both off by default)
--
-- DIFFICULTY (tierBands / tierDrift): pools are paired with slots by power
-- rank within a window, so early-game loot stays early-game. Enchanted-variant
-- forms above enchantedLootRatio stay in their original lists instead of
-- entering the loot pools.
--
-- The layout is fully determined by CONFIG.seed (any integer).
--
-- Note:
--   * quest-critical forms may end up in odd lists, protect quest lists with config.useProtection
--   * smithing material sets stay untouched unless you remove "Set" from
--     formListExcludeSuffixes
--
-- QUEST PROTECTION (on by default, useProtection = false to disable):
--   * EverythingRandomizer_protection.lua (a sibling generated from the
--     vanilla + Creation Club masters, see third_party/) carries every form
--     referenced by a quest alias or the game's default objects (DOBJ).
--     Protected forms are never pooled and never reassigned: slots that hold
--     them keep their original form.
--   * lua_patcher.isQuestReferenced(form) adds the same protection for ANY
--     loaded quest's aliases (mod quests included), so quest gear stays put
--     even without the dataset.
--
-- INTERNAL-FORM FILTER (filterInternal = true by default): QA/dev-test and
-- placeholder records (editor IDs like QA*, Test*, *Dummy*, runtime ghost
-- copies) never appear in normal gameplay and are kept out of every shuffle.
-- A fully custom CONFIG.excludeForm(editorId, form) function can express any
-- arbitrary exclusion rule on top.
--
-- MODULES: this file is the entry point; the shuffles live in sibling
-- module files loaded with require() (resolved next to the script):
--   _randomizer_util.lua     — pure helpers (RNG, power scoring, banding)
--   _randomizer_shuffles.lua — the swap-based shuffles
--   _randomizer_jitter.lua   — the stat/attribute jitter passes
-- Module files are never executed as top-level scripts by LuaPatcher.
--
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/EverythingRandomizer_config.lua
-- (flat sibling of the script, mirrors examples/ layout).

local CONFIG = {
    enabled = true,

    seed = 1337,

    useProtection = true,
    filterInternal = true,

    excludeForm = nil,

    includePrefixes = {},
    excludePrefixes = {},
    excludeSuffixes = {},

    formListExcludeSuffixes = {},

    tierBands = 4,
    tierDrift = 1,
    enchantedLootRatio = 0.5,

    shuffleLeveledItems = true,
    shuffleLeveledCharacters = false,
    shuffleLeveledSpells = true,
    shuffleFormLists = true,
    shuffleContainers = true,
    shuffleIngredients = true,
    shufflePotions = true,
    shuffleEnchantments = false,
    shuffleShouts = true,
    shuffleGearEnchantments = false,
    shuffleEncounterZones = true,
    zoneSwapWindow = 3,

    shuffleRecipeOutputs = true,
    shuffleRecipeIngredients = false,

    randomizeStats = true,
    statJitter = 0.2,
    skipEnchanted = true,
    randomizeMagic = false,
    magicJitter = 0.2,
    randomizeActors = false,
    actorJitter = 0.2,
    randomizeLights = false,
    lightJitter = 0.3,
}

do
    local loaded = nil

    if lua_patcher.loadLua then
        local ok, result = pcall(lua_patcher.loadLua, "EverythingRandomizer_config.lua")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("EverythingRandomizer: loadLua failed: %s", tostring(result)))
        end
    end

    if loaded then
        for k, v in pairs(loaded) do
            CONFIG[k] = v
        end
        print(string.format("EverythingRandomizer: loaded user config (seed=%s)", tostring(CONFIG.seed)))
    else
        print(string.format("EverythingRandomizer: no user config, using defaults (seed=%s)", tostring(CONFIG.seed)))
    end

    if type(CONFIG.includePrefixes) ~= "table" then CONFIG.includePrefixes = {} end
    if type(CONFIG.excludePrefixes) ~= "table" then CONFIG.excludePrefixes = {} end
    if type(CONFIG.excludeSuffixes) ~= "table" then CONFIG.excludeSuffixes = {} end
    if type(CONFIG.formListExcludeSuffixes) ~= "table" then CONFIG.formListExcludeSuffixes = {} end

    -- seed must be an integer: Lua silently turns non-integral values into 0,
    -- which would make every invalid seed collapse to the same world
    if type(CONFIG.seed) ~= "number" or CONFIG.seed % 1 ~= 0 then
        lua_patcher.warn(string.format(
            "EverythingRandomizer: invalid seed (%s), falling back to 1337", tostring(CONFIG.seed)))
        CONFIG.seed = 1337
    end

    if type(CONFIG.statJitter) ~= "number" or CONFIG.statJitter < 0 then CONFIG.statJitter = 0.2 end
    if CONFIG.statJitter > 0.9 then CONFIG.statJitter = 0.9 end

    if type(CONFIG.magicJitter) ~= "number" or CONFIG.magicJitter < 0 then CONFIG.magicJitter = 0.2 end
    if CONFIG.magicJitter > 0.9 then CONFIG.magicJitter = 0.9 end

    if type(CONFIG.actorJitter) ~= "number" or CONFIG.actorJitter < 0 then CONFIG.actorJitter = 0.2 end
    if CONFIG.actorJitter > 0.9 then CONFIG.actorJitter = 0.9 end

    if type(CONFIG.lightJitter) ~= "number" or CONFIG.lightJitter < 0 then CONFIG.lightJitter = 0.3 end
    if CONFIG.lightJitter > 0.9 then CONFIG.lightJitter = 0.9 end

    if type(CONFIG.useProtection) ~= "boolean" then CONFIG.useProtection = true end
    if type(CONFIG.filterInternal) ~= "boolean" then CONFIG.filterInternal = true end

    if CONFIG.excludeForm ~= nil and type(CONFIG.excludeForm) ~= "function" then
        lua_patcher.warn("EverythingRandomizer: invalid excludeForm (expected a function), ignoring")
        CONFIG.excludeForm = nil
    end

    if type(CONFIG.zoneSwapWindow) ~= "number" or CONFIG.zoneSwapWindow < 1 then CONFIG.zoneSwapWindow = 3 end
    if CONFIG.zoneSwapWindow > 10 then CONFIG.zoneSwapWindow = 10 end
    if type(CONFIG.tierBands) ~= "number" or CONFIG.tierBands < 1 then CONFIG.tierBands = 4 end
    if CONFIG.tierBands > 10 then CONFIG.tierBands = 10 end
    if type(CONFIG.tierDrift) ~= "number" or CONFIG.tierDrift < 0 then CONFIG.tierDrift = 0.3 end
    if CONFIG.tierDrift > 1 then CONFIG.tierDrift = 1 end
    if type(CONFIG.enchantedLootRatio) ~= "number" or CONFIG.enchantedLootRatio < 0 then
        CONFIG.enchantedLootRatio = 1.0
    end

    if CONFIG.enchantedLootRatio > 1 then CONFIG.enchantedLootRatio = 1 end
end

-- ---- quest protection ----
--
-- Two layers:
--   1. EverythingRandomizer_protection.lua (a sibling generated from the
--      vanilla + CC masters; loaded via loadLua).
--   2. lua_patcher.isQuestReferenced(): every loaded quest's alias refs
--      (mod quests included), computed by the plugin at load time.
local PROTECTED = {}

do
    if CONFIG.useProtection and lua_patcher.loadLua then
        local ok, result = pcall(lua_patcher.loadLua, "EverythingRandomizer_protection.lua")
        if ok and type(result) == "table" and type(result.protected) == "table" then
            PROTECTED = result.protected
            local n = 0
            for _ in pairs(PROTECTED) do n = n + 1 end
            print(string.format("EverythingRandomizer: loaded %d protected forms from dataset", n))
        elseif ok then
            print("EverythingRandomizer: protection dataset missing or malformed, using runtime quest refs only")
        else
            print(string.format("EverythingRandomizer: loadLua failed: %s", tostring(result)))
        end
    end

    local hasRuntime = CONFIG.useProtection and lua_patcher.isQuestReferenced ~= nil
    if not hasRuntime then
        print("EverythingRandomizer: quest protection disabled")
    end
end

-- ---- form filtering ----
--
-- A form is protected when the dataset marks it or a loaded quest alias
-- references it. Protected forms are never moved: they stay out of the pools
-- and their slots keep the original form.
local function isProtected(form)
    if not form then return false end
    if PROTECTED[form.formId] then return true end
    if lua_patcher.isQuestReferenced and lua_patcher.isQuestReferenced(form) then return true end
    return false
end

-- Built-in "internal form" filter (CONFIG.filterInternal, on by default):
-- QA / dev-test records and runtime placeholder copies that never reach
-- normal gameplay, identified from a naming-pattern analysis of the vanilla +
-- CC + modded load order. Deliberately narrow: encounter variants, ghost
-- enemies and dungeon content all stay in.
local function internalForm(editorId)
    if editorId == "" then return false end
    if editorId:find("^QA") or editorId:find("^Test") or editorId:find("^test") then return true end
    if editorId:find("Dummy") then return true end
    if editorId:find("^AADeleteWhenDoneTest") then return true end
    -- runtime ghost copies: player-death equipment ghosts, flicker lists.
    -- Deliberately "defaultGhost"-prefixed only: dungeon ghost spawn lists
    -- (DLC2dunHaknirGhostLeveledList, LCharGhostWizard) are real content.
    if editorId:find("defaultGhost") or editorId:find("DeathItemGhost") then return true end
    if editorId:find("FlickerList") then return true end
    return false
end

-- The single exclusion entry point used by every shuffle:
--   quest protection  OR  built-in internal filter  OR  CONFIG.excludeForm
-- CONFIG.excludeForm is an optional user function (editorId, form) -> bool;
-- return true to keep the form out of every pool and every slot.
local function isExcluded(form)
    if not form then return false end
    if isProtected(form) then return true end
    local ed = form.editorId or ""
    if CONFIG.filterInternal and internalForm(ed) then return true end
    local f = CONFIG.excludeForm
    if f and f(ed, form) then return true end
    return false
end

-- ---- shuffle modules ----
--
-- The swap and jitter passes live in sibling modules (see the header). They
-- are stateless: every call receives the context below, so multiple runs of
-- this script in one Lua state never leak state between each other.
local shuffles = require("_randomizer_shuffles")
local jitter = require("_randomizer_jitter")
local util = require("_randomizer_util")

local ctx = {
    config = CONFIG,
    isExcluded = isExcluded,
    skipped = 0, -- protected-slot counter, shared by every pass
}

if not CONFIG.enabled then
    print("EverythingRandomizer: disabled by config")
    return
end

math.randomseed(CONFIG.seed)

local slotChanges = 0
local candidateLists = 0

if CONFIG.shuffleLeveledItems or CONFIG.shuffleLeveledCharacters or CONFIG.shuffleLeveledSpells then
    local lists = {}
    for _, ll in ipairs(lua_patcher.allLeveledItems()) do
        local ed = ll.editorId or ""
        if not isExcluded(ll) and not util.hasPrefix(ed, CONFIG.excludePrefixes) and not util.hasSuffix(ed, CONFIG.excludeSuffixes) then
            if #CONFIG.includePrefixes == 0 or util.hasPrefix(ed, CONFIG.includePrefixes) then
                table.insert(lists, ll)
            end
        end
    end

    if not CONFIG.shuffleLeveledItems then
        lists = {}
    end
    candidateLists = candidateLists + #lists
    slotChanges = slotChanges + shuffles.leveledEntries(ctx, lists)

    if CONFIG.shuffleLeveledCharacters then
        local charLists = {}
        for _, ll in ipairs(lua_patcher.allLeveledCharacters()) do
            local ed = ll.editorId or ""
            if not isExcluded(ll) and not util.hasPrefix(ed, CONFIG.excludePrefixes) and not util.hasSuffix(ed, CONFIG.excludeSuffixes) then
                if #CONFIG.includePrefixes == 0 or util.hasPrefix(ed, CONFIG.includePrefixes) then
                    table.insert(charLists, ll)
                end
            end
        end
        candidateLists = candidateLists + #charLists
        slotChanges = slotChanges + shuffles.leveledCharacters(ctx, charLists)
    end

    if CONFIG.shuffleLeveledSpells then
        local spellLists = {}
        for _, ll in ipairs(lua_patcher.allLeveledSpells()) do
            local ed = ll.editorId or ""
            if not isExcluded(ll) and not util.hasPrefix(ed, CONFIG.excludePrefixes) and not util.hasSuffix(ed, CONFIG.excludeSuffixes) then
                if #CONFIG.includePrefixes == 0 or util.hasPrefix(ed, CONFIG.includePrefixes) then
                    table.insert(spellLists, ll)
                end
            end
        end
        candidateLists = candidateLists + #spellLists
        slotChanges = slotChanges + shuffles.leveledEntries(ctx, spellLists)
    end
end

if CONFIG.shuffleFormLists then
    local lists = {}
    for _, fl in ipairs(lua_patcher.allFormLists()) do
        local ed = fl.editorId or ""
        if not isExcluded(fl) and not util.hasPrefix(ed, CONFIG.excludePrefixes) and not util.hasSuffix(ed, CONFIG.formListExcludeSuffixes) then
            table.insert(lists, fl)
        end
    end
    candidateLists = candidateLists + #lists
    slotChanges = slotChanges + shuffles.formLists(ctx, lists)
end

local containerChanges = 0
if CONFIG.shuffleContainers then
    local containers = {}
    for _, c in ipairs(lua_patcher.allContainers()) do
        local ed = c.editorId or ""
        if not isExcluded(c) and not util.hasPrefix(ed, CONFIG.excludePrefixes) and not util.hasSuffix(ed, CONFIG.excludeSuffixes) then
            table.insert(containers, c)
        end
    end
    containerChanges = shuffles.containerContents(ctx, containers)
end

local ingredientChanges = 0
local potionChanges = 0
local enchantmentChanges = 0
local shoutChanges = 0

if CONFIG.shuffleIngredients then
    ingredientChanges = ingredientChanges + shuffles.effectSlots(ctx, lua_patcher.allIngredients())
end
if CONFIG.shufflePotions then
    potionChanges = potionChanges + shuffles.effectSlots(ctx, lua_patcher.allPotions())
end
if CONFIG.shuffleEnchantments then
    enchantmentChanges = enchantmentChanges + shuffles.effectSlots(ctx, lua_patcher.allEnchantments())
end
if CONFIG.shuffleShouts then
    shoutChanges = shoutChanges + shuffles.shoutSpells(ctx, lua_patcher.allShouts())
end

local statChanges = 0
if CONFIG.randomizeStats then
    statChanges = jitter.stats(ctx)
end

local magicChanges = 0
if CONFIG.randomizeMagic then
    magicChanges = jitter.magic(ctx)
end

local actorChanges = 0
if CONFIG.randomizeActors then
    actorChanges = jitter.actors(ctx)
end

local lightChanges = 0
if CONFIG.randomizeLights then
    lightChanges = jitter.lights(ctx)
end

local enchantmentChanges = 0
if CONFIG.shuffleGearEnchantments then
    enchantmentChanges = shuffles.gearEnchantments(ctx)
end

local zoneChanges = 0
if CONFIG.shuffleEncounterZones then
    zoneChanges = shuffles.encounterZones(ctx)
end

local recipeChanges = 0
if CONFIG.shuffleRecipeOutputs or CONFIG.shuffleRecipeIngredients then
    recipeChanges = shuffles.recipes(ctx)
end

print(string.format([[
    EverythingRandomizer:
    seed=%d
    lists=%d
    swappedSlots=%d
    containers=%d

    ingredients=%d
    potions=%d
    enchantments=%d
    shouts=%d

    statJittered=%d
    magicJittered=%d
    actors=%d
    lights=%d
    enchantments=%d
    zones=%d
    recipes=%d
    protectedSlots=%d
]],
    CONFIG.seed,
    candidateLists,
    slotChanges,
    containerChanges,
    ingredientChanges,
    potionChanges,
    enchantmentChanges,
    shoutChanges,
    statChanges,
    magicChanges,
    actorChanges,
    lightChanges,
    enchantmentChanges,
    zoneChanges,
    recipeChanges,
    ctx.skipped
)
)
