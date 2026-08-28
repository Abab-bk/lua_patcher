-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    enabled = true,
    enableWeapons = true,
    enableArmors = true,

    -- Never touch non-playable (quest/unique) gear.
    skipNonPlayable = true,

    -- === Targeting ===
    -- "" = all non-vanilla plugins; or a single plugin, e.g. "MyGear.esp".
    plugin = "",

    -- Only fix gear whose EditorID starts with any of these (empty = all).
    editorIdPrefixes = {},

    excludedPlugins = { "CBBE.esp", "3BBB.esp", "XPMSE.esp" },
}