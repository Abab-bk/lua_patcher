-- MagicTweak user config — EXAMPLE
-- Must `return` a table. Missing file → defaults in MagicTweak.lua.

return {
    costMultiplier = 0.8,     -- Spell costOverride * 0.8 (20% cheaper); 1.0 = disable
    onlyDestruction = true,   -- only touch Destruction spells
    fixHostileFlag = true,
    mgefCostMultiplier = 0.9, -- MagicEffect baseCost * 0.9
    addKeywords = true,
}
