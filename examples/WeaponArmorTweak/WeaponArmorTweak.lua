-- LuaPatcher example: Weapon & Armor global fixup (real-world)
-- priority: 10 (rebalance stats first so KeywordFixer tiers see final values)
--
-- The writable Equipment API added in LuaPatcher:
--   weapon.damage / speed / reach / stagger / critDamage / weight / value  (rw)
--   armor.armorRating / weight / value                                      (rw)
--   weapon:addKeyword(kw) / removeKeyword(kw)  — same for armor

local VANILLA = {
    ["Skyrim.esm"] = true,
    ["Update.esm"] = true,
    ["Dawnguard.esm"] = true,
    ["HearthFires.esm"] = true,
    ["Dragonborn.esm"] = true,
}

local CONFIG = {
    fixMissingMaterialKeywords = true,
    rebalanceWeaponDamage = false, -- set true to normalize outlier damage
    rebalanceArmorRating = false,  -- set true to normalize outlier armorRating
    capHeavyWeight = true,
    maxHeavyWeight = 35.0,
    weightValueRatioFix = false, -- if true, clamp insane value/weight ratios
}

do
    local loaded = nil
    if lua_patcher.tryLoadConfig then
        local ok, res = pcall(lua_patcher.tryLoadConfig, "WeaponArmorTweak")
        if ok and type(res) == "table" then
            loaded = res
        elseif not ok then
            print("WeaponArmorTweak: tryLoadConfig failed: " .. tostring(res))
        end
    end
    if loaded then
        for k, v in pairs(loaded) do CONFIG[k] = v end
        print("WeaponArmorTweak: loaded user config")
    else
        print(string.format("WeaponArmorTweak: no user config, defaults fixKeywords=%s rebalanceW=%s rebalanceA=%s",
            tostring(CONFIG.fixMissingMaterialKeywords), tostring(CONFIG.rebalanceWeaponDamage),
            tostring(CONFIG.rebalanceArmorRating)))
    end
end

-- Material tier tables (same thresholds as GearInjection, derived from UESP)
local WEAPON_TIERS = {
    { mat = "Iron",    lvl = 1,  maxDmg = 8 }, -- Iron Sword 7, Mace 9-ish
    { mat = "Steel",   lvl = 2,  maxDmg = 9 },
    { mat = "Orcish",  lvl = 6,  maxDmg = 11 },
    { mat = "Dwarven", lvl = 12, maxDmg = 12 },
    { mat = "Elven",   lvl = 19, maxDmg = 13 },
    { mat = "Glass",   lvl = 27, maxDmg = 14 },
    { mat = "Ebony",   lvl = 36, maxDmg = 16 },
    { mat = "Daedric", lvl = 46, maxDmg = 99 },
}

local ARMOR_TIERS = {
    { mat = "Iron",        isHeavy = true,  lvl = 1,  maxRating = 10 }, -- Hide/Iron
    { mat = "Steel",       isHeavy = true,  lvl = 6,  maxRating = 18 },
    { mat = "Dwarven",     isHeavy = true,  lvl = 12, maxRating = 22 },
    { mat = "Orcish",      isHeavy = true,  lvl = 25, maxRating = 28 },
    { mat = "Ebony",       isHeavy = true,  lvl = 32, maxRating = 32 },
    { mat = "Daedric",     isHeavy = true,  lvl = 48, maxRating = 99 },
    { mat = "Hide",        isHeavy = false, lvl = 1,  maxRating = 6 },
    { mat = "Leather",     isHeavy = false, lvl = 6,  maxRating = 8 },
    { mat = "Elven",       isHeavy = false, lvl = 12, maxRating = 11 },
    { mat = "Scaled",      isHeavy = false, lvl = 19, maxRating = 15 },
    { mat = "Glass",       isHeavy = false, lvl = 36, maxRating = 18 },
    { mat = "Dragonscale", isHeavy = false, lvl = 46, maxRating = 99 },
}

-- Helpers to resolve keywords
local function getKeywordByEditorId(ed)
    -- editorId like "WeapMaterialDaedric" / "ArmorMaterialGlass"
    local f = lua_patcher.getForm(ed)
    if f and f.type == "Keyword" then return f end
    -- try without prefix
    return nil
end

local MATERIAL_KEYWORDS = {
    -- populated lazily
}

local function findMaterialKeyword(mat, isWeapon)
    -- try WeapMaterialX / ArmorMaterialX / ArmorMaterialLightX etc.
    local candidates = {}

    if isWeapon then
        table.insert(candidates, "WeapMaterial" .. mat)
        table.insert(candidates, "WeaponMaterial" .. mat)
    else
        table.insert(candidates, "ArmorMaterial" .. mat)
        table.insert(candidates, "ArmorMaterialLight" .. mat)
        table.insert(candidates, "ArmorMaterialHeavy" .. mat)
    end

    for _, ed in ipairs(candidates) do
        local kw = getKeywordByEditorId(ed)
        if kw then return kw end
    end

    return nil
end


local function hasAnyMaterialKeyword(form)
    for _, kw in ipairs(form.keywords) do
        local ed = kw.editorId or ""
        if ed:find("Material") then return true end
    end
    return false
end


local function tierForWeapon(weap)
    local dmg = weap.damage or 0
    for _, t in ipairs(WEAPON_TIERS) do
        if dmg <= t.maxDmg then return t end
    end
    return WEAPON_TIERS[#WEAPON_TIERS]
end


local function tierForArmor(armor)
    local r = armor.armorRating or 0
    local wantHeavy = (armor.armorType == "Heavy")
    -- filter tiers by heavy/light
    local best = nil
    for _, t in ipairs(ARMOR_TIERS) do
        if t.isHeavy == wantHeavy then
            if r <= t.maxRating then return t end
            best = t
        end
    end
    return best or ARMOR_TIERS[#ARMOR_TIERS]
end


local fixedKw, rebalancedW, rebalancedA, capped = 0, 0, 0, 0


-- -------------------------------------------------------------------------
-- Weapons
-- -------------------------------------------------------------------------
for _, weap in ipairs(lua_patcher.allWeapons()) do
    if not VANILLA[weap.plugin] and weap.playable and not weap.enchantment then
        -- 1) fix missing material keyword
        if CONFIG.fixMissingMaterialKeywords and not hasAnyMaterialKeyword(weap) then
            local tier = tierForWeapon(weap)
            local kw = findMaterialKeyword(tier.mat, true)
            if kw then
                local ok = weap:addKeyword(kw)
                if ok then
                    fixedKw = fixedKw + 1
                    print(string.format("WeaponArmorTweak: weapon %s dmg=%d -> added %s", weap.identifier, weap.damage,
                        kw.editorId))
                end
            end
        end
        -- 2) optional damage rebalance: clamp to tier maxDmg if outlier by >30%
        if CONFIG.rebalanceWeaponDamage then
            local tier = tierForWeapon(weap)
            -- allow up to tier.maxDmg+2, otherwise pull down to tier's max
            if weap.damage > tier.maxDmg + 2 then
                local old = weap.damage
                weap.damage = tier.maxDmg
                rebalancedW = rebalancedW + 1
                print(string.format("WeaponArmorTweak: rebalanced weapon %s %d -> %d (tier %s)", weap.identifier, old,
                    weap.damage, tier.mat))
            end
            -- also fix absurd weight/value if requested
            if CONFIG.weightValueRatioFix and weap.weight and weap.weight > 30 then
                weap.weight = 25
            end
        end
        -- 3) also fix value/weight outliers for non-vanilla if desired
        if CONFIG.weightValueRatioFix and weap.value and weap.value > 3000 then
            weap.value = 1500
        end
    end
end


-- -------------------------------------------------------------------------
-- Armors
-- -------------------------------------------------------------------------
for _, armor in ipairs(lua_patcher.allArmors()) do
    if not VANILLA[armor.plugin] and armor.playable and not armor.enchantment then
        -- fix missing material keywords
        if CONFIG.fixMissingMaterialKeywords and not hasAnyMaterialKeyword(armor) then
            local tier = tierForArmor(armor)
            local kw = findMaterialKeyword(tier.mat, false)
            if kw then
                local ok = armor:addKeyword(kw)
                if ok then
                    fixedKw = fixedKw + 1
                    print(string.format("WeaponArmorTweak: armor %s [%.1f %s] -> added %s", armor.identifier,
                        armor.armorRating, armor.armorType, kw.editorId))
                end
            end
        end

        -- cap heavy weight
        if CONFIG.capHeavyWeight and armor.armorType == "Heavy" and armor.weight and armor.weight > CONFIG.maxHeavyWeight then
            local old = armor.weight
            armor.weight = CONFIG.maxHeavyWeight
            capped = capped + 1
            print(string.format("WeaponArmorTweak: capped Heavy armor %s weight %.1f -> %.1f", armor.identifier, old,
                armor.weight))
        end

        -- optional rating rebalance
        if CONFIG.rebalanceArmorRating then
            local tier = tierForArmor(armor)
            if armor.armorRating > tier.maxRating + 5 then
                local old = armor.armorRating
                armor.armorRating = tier.maxRating
                rebalancedA = rebalancedA + 1
                print(string.format("WeaponArmorTweak: rebalanced armor %s %.1f -> %.1f (tier %s)", armor.identifier, old,
                    armor.armorRating, tier.mat))
            end
        end
    end
end


print(string.format("WeaponArmorTweak: done — fixedKeywords=%d rebalancedWeapons=%d rebalancedArmors=%d cappedWeight=%d",
    fixedKw, rebalancedW, rebalancedA, capped))
