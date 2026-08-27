-- EquipmentInjection user config - EXAMPLE
--
-- Copy this file to:
--   Data/SKSE/Plugins/LuaPatcher/Config/EquipmentInjection.lua
-- (create the Config folder if it doesn't exist)
--
-- This file is NEVER overwritten by mod updates - the mod ships only
-- examples/EquipmentInjection.lua (the script) and this example.
-- If this file is missing or fails to load, the script falls back to
-- balanced=true, targetPrefixes={"LItem"} defaults.
--
-- Must `return` a table. Only keys you set override defaults; remove
-- a line to keep its default.

return {
    -- false => legacy behavior: ll:add(form, 1, 1) for every item (no balancing)
    -- true  => keyword -> level + rating/DPS interpolation (recommended)
    balanced = true,

    -- Level range for interpolation (like Gear Spreader bottomlevel/toplevel)
    -- Vanilla Daedric is 46 (weapon/light) / 48 (heavy); maxLevel caps overflow.
    bottomLevel = 1,
    topLevel = 46,
    maxLevel = 50,

    -- Which leveled list EditorID prefixes to inject into.
    -- "LItem" covers most loot; add "LChar" prefixes only if you also want NPC outfits.
    -- Must match EditorIDs that actually exist in your load order (see log).
    targetPrefixes = { "LItem" },

    -- Master toggles - set false to skip that category entirely
    enableArmor = true,
    enableWeapon = true,
}
