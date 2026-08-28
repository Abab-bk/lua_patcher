-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    enabled = true,

    -- The seed. Change it to get a new world on the next launch.
    -- It must be an integer.
    seed = 7777,

    -- =====================================================================
    -- Filtering — what never gets shuffled
    -- =====================================================================
    --   1. useProtection     quest aliases / game default objects
    --   2. filterInternal    built-in QA/Test/Dummy/ghost-copy rules
    --   3. excludeForm       your own function
    -- The prefix/suffix lists below are convenience shorthands for simple
    -- EditorID matching; excludeForm can express the same and more.
    useProtection = true,
    filterInternal = true,

    -- excludeForm = function(editorId, form)
    --     return editorId:match("^XXX") ~= nil or form.plugin == "SomeMod.esp"
    -- end,
    excludeForm = nil,

    -- List-level filters (apply to leveled lists, FormLists and containers):
    -- includePrefixes empty = all lists; otherwise only matching EditorIDs.
    -- excludePrefixes e.g. { "LItemQuest" } (quest-list protection)
    -- excludeSuffixes e.g. { "Unique" }
    includePrefixes = {},
    excludePrefixes = {},
    excludeSuffixes = {},

    -- Smithing material sets (WeapMaterialSteelSet, ...) stay intact by
    -- default so tempering keeps working; remove "Set" to shuffle them too.
    formListExcludeSuffixes = { "Set" },

    -- =====================================================================
    -- Shuffles — which domains get randomized
    -- =====================================================================
    -- Leveled lists (drops / chests / vendors / NPC spawns / spell lists)
    shuffleLeveledItems = true,
    shuffleLeveledCharacters = false, -- NPC spawn lists; Note: Not recommended to enable, this will cause mobs to spawn *everywhere*, it's basically Skyrim GTA Edition
    shuffleLeveledSpells = true,      -- spell leveled lists
    -- FormList contents (vendor stock, groups, ...)
    shuffleFormLists = true,
    -- Container contents (chests / lootable objects); slots keep their count
    shuffleContainers = true,
    -- Alchemy / enchanting / shouts
    shuffleIngredients = true,   -- ingredient effect slots swap
    shufflePotions = true,       -- potion/poison/food effect slots swap
    shuffleEnchantments = false, -- enchantment effect slots swap
    shuffleShouts = true,        -- shout spell variations swap
    -- Gear enchantment swap (weapon-type <-> weapon-type, armor-type <-> armor-type)
    shuffleGearEnchantments = true,
    -- Encounter zone difficulty swap (order-preserving: each zone only trades
    -- levels with neighbors up to zoneSwapWindow tiers away)
    shuffleEncounterZones = true,
    zoneSwapWindow = 3,

    -- =====================================================================
    -- Jitters — numeric wobble on top of the shuffles
    -- =====================================================================
    -- Gear stats: each stat multiplied by 1 +/- statJitter
    randomizeStats = true,
    statJitter = 0.2,
    skipEnchanted = true, -- enchanted gear keeps its stats (enchantment scales off them)
    -- Magic costs (spell costOverride / effect baseCost)
    randomizeMagic = false,
    magicJitter = 0.2,
    -- Actor chaos (NPC_ records, creatures included; quest NPCs too)
    randomizeActors = false,
    actorJitter = 0.2,
    -- Lights (radius / color / fade); carried lights are never touched; Not recommended to enabled, it can cause some weird crashes, but it could also an issue with my modlist.
    randomizeLights = false,
    lightJitter = 0.3,
}
