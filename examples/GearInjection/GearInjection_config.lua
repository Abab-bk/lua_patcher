-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    -- === Targeting ===
    -- "" = every non-vanilla, non-blocked plugin; or a single plugin name,
    -- e.g. "MyGear.esp" to only inject that mod's gear.
    plugin = "",

    -- Only inject gear whose EditorID starts with any of these (empty = all).
    -- Handy with `plugin`: e.g. plugin = "MyGear.esp", editorIdPrefixes = { "MG_" }
    editorIdPrefixes = {},

    -- Never inject gear with these EditorID prefixes (SMP physics internals etc.).
    excludeEditorIdPrefixes = { "aaSMP", "SMP3" },

    -- Plugins whose gear is never injected.
    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },

    -- Which leveled list EditorID prefixes receive injections.
    -- "LItem" covers most loot; must match EditorIDs that exist in your load order.
    targetPrefixes = { "LItem" },

    -- === Categories ===
    enableArmor = true,
    enableWeapon = true,

    -- === Level ===
    -- "balanced": keyword -> level, else rating/DPS interpolation (recommended)
    -- "fixed":    every injected item gets fixedLevel
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