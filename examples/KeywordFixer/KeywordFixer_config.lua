-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    enabled = true,

    -- Add the material keyword derived from the item's stats when the item
    -- has none (e.g. a 25-rating heavy armor gets ArmorMaterialOrcish).
    -- This makes smithing perks and material-based rules work on mod gear.
    fixMaterialKeywords = true,

    -- Add slot keywords (ArmorBody, ArmorHands, ArmorFeet, ...) to armors
    -- from their biped slots. Off by default: slot keywords are noisier.
    addSlotKeywords = false,

    -- Never touch non-playable (quest/unique) gear.
    skipNonPlayable = true,

    -- === Targeting ===
    -- "" = all non-vanilla plugins; or a single plugin, e.g. "MyGear.esp".
    plugin = "",

    -- Only fix gear whose EditorID starts with any of these (empty = all).
    editorIdPrefixes = {},

    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },

    -- Declarative add/remove rules, e.g.:
    -- {
    --   { plugin = "OldMod.esp", remove = { "ClothingBody" } },
    --   { editorIdPrefix = "SW_", add = { "WeapTypeSword", "WeapMaterialSteel" } },
    -- }
    -- A missing field matches anything; add and remove are both optional.
    rules = {},
}