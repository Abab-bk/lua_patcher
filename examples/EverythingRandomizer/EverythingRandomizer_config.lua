-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    enabled = true,

    -- The world layout is fully determined by this seed. Same seed = same
    -- world on every save (the patch runs once per launch). Change it to
    -- get a new world on the next launch.
    seed = 1234,

    -- === Leveled list shuffle (drops / chests / vendors) ===
    shuffleLeveledItems = true,
    shuffleLeveledCharacters = false, -- NPC spawn lists (quest-hostile)
    shuffleLeveledSpells = false,     -- spell leveled lists
    -- empty = every leveled list (vanilla + mods); or restrict, e.g. { "LItem" }
    includePrefixes = {},
    -- quest-list protection: lists whose EditorID starts with any of these
    -- are never touched, e.g. { "LItemQuest" }
    excludePrefixes = {},
    excludeEditorIdSuffixes = {}, -- e.g. { "Unique" }

    -- === FormList shuffle (vendor stock, groups, ...) ===
    shuffleFormLists = true,
    -- smithing material sets (WeapMaterialSteelSet, ...) stay intact by
    -- default so tempering keeps working; remove "Set" to shuffle them too
    formListExcludeSuffixes = { "Set" },

    -- === Container contents shuffle (chests / lootable objects) ===
    -- Slots keep their count; the item swaps within a same-type pool.
    shuffleContainers = true,

    -- === Alchemy / enchanting / shouts ===
    shuffleIngredients = false,  -- ingredient effect slots swap
    shufflePotions = true,       -- potion/poison/food effect slots swap
    shuffleEnchantments = false, -- enchantment effect slots swap
    shuffleShouts = true,        -- shout spell variations swap

    -- === Gear stat jitter ===
    randomizeStats = true,
    statJitter = 0.2,     -- each stat is multiplied by 1 +/- statJitter
    skipEnchanted = true, -- enchanted gear keeps its stats (enchantment scales off them)

    -- === Magic cost jitter (spell costOverride / effect baseCost) ===
    randomizeMagic = false,
    magicJitter = 0.2,

    -- === Actor chaos (NPC_ records, creatures included) ===
    -- Touches quest NPCs too: level/attribute jitter + skill shuffles.
    randomizeActors = false,
    actorJitter = 0.2,

    -- === Light jitter (radius / color / fade) ===
    -- Carried lights (torches, ...) are never touched.
    randomizeLights = false,
    lightJitter = 0.3,
}
