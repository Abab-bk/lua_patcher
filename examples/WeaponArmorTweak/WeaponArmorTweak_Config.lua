-- WeaponArmorTweak user config — EXAMPLE
--
-- Copy to:
--   Data/SKSE/Plugins/LuaPatcher/Config/WeaponArmorTweak.lua
--
-- Must `return` a table. Missing file → defaults below.
return {
    fixMissingMaterialKeywords = true,  -- add WeapMaterial*/ArmorMaterial* if absent
    rebalanceWeaponDamage = false,      -- clamp outlier damage to tier
    rebalanceArmorRating = false,       -- clamp outlier armorRating to tier
    capHeavyWeight = true,              -- cap Heavy armor weight for encumbrance
    maxHeavyWeight = 35.0,
    weightValueRatioFix = false,        -- clamp insane value/weight
}
