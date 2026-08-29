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
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/EverythingRandomizer_config.lua
-- (flat sibling of the script, mirrors examples/ layout).

local CONFIG = {
    enabled = true,
    seed = 1337,

    -- Filtering layers (see header): quest protection, built-in internal
    -- filter, custom function, plus convenience prefix/suffix lists.
    useProtection = true,
    filterInternal = true,
    excludeForm = nil,
    includePrefixes = {},                -- empty = every list; or restrict, e.g. { "LItem" }
    excludePrefixes = {},                -- quest-list protection, e.g. { "LItemQuest" }
    excludeSuffixes = {},                -- e.g. { "Unique" }
    formListExcludeSuffixes = { "Set" }, -- keep smithing material sets sane

    -- Difficulty: shuffled slots are paired with forms of a similar power rank
    -- (weapon damage / armor rating). tierBands splits the power-ordered pool;
    -- tierDrift widens the pairing window (0 = strict bands, 1 = full chaos).
    -- enchantedLootRatio caps the Ench* variants that join the loot pools
    -- (1.0 = vanilla pool, 0 = enchanted loot stays where vanilla put it).
    tierBands = 4,
    tierDrift = 0.3,
    enchantedLootRatio = 1.0,

    -- Shuffles
    shuffleLeveledItems = true,
    shuffleLeveledCharacters = false, -- NPC spawn lists (quest-hostile)
    shuffleLeveledSpells = true,      -- spell leveled lists
    shuffleFormLists = true,
    shuffleContainers = true,         -- chest/lootable contents swap, same-type pools
    shuffleIngredients = true,        -- ingredient effect slots swap
    shufflePotions = true,            -- potion/poison/food effect slots swap
    shuffleEnchantments = true,       -- enchantment effect slots swap
    shuffleShouts = true,             -- shout spell variations swap
    shuffleGearEnchantments = true,   -- enchantments swap between gear (per type)
    shuffleEncounterZones = true,     -- zone difficulty swaps within a neighbor window
    zoneSwapWindow = 3,               -- how many difficulty tiers apart zones may swap

    -- Jitters
    randomizeStats = true,
    statJitter = 0.2, -- +/-20% per stat
    skipEnchanted = true,
    randomizeMagic = false,
    magicJitter = 0.2,
    randomizeActors = false, -- level/attribute jitter + skill shuffle (chaotic)
    actorJitter = 0.2,
    randomizeLights = true,  -- radius / color / fade
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

local skippedProtected = 0

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

local function hasPrefix(editorId, prefixes)
    for _, p in ipairs(prefixes) do
        if string.sub(editorId, 1, #p) == p then return true end
    end
    return false
end

local function hasSuffix(editorId, suffixes)
    for _, s in ipairs(suffixes) do
        if #editorId >= #s and string.sub(editorId, #editorId - #s + 1) == s then return true end
    end
    return false
end

-- Fisher-Yates on the seeded RNG (math.randomseed set once before all shuffles).
local function fisherYates(t)
    for i = #t, 2, -1 do
        local j = math.random(i)
        t[i], t[j] = t[j], t[i]
    end
    return t
end

-- Power score of a form for difficulty banding: weapon damage / armor rating /
-- spell cost override. Forms without a natural power (keys, ingredients, ...)
-- score nil and sort to the bottom.
local function formPower(form)
    local t = form.type
    if t == "Weapon" then return form.damage or 0 end
    if t == "Armor" then return form.armorRating or 0 end
    if t == "Spell" then return form.costOverride or 0 end
    return nil
end

-- Enchanted-variant forms (editor IDs like EnchIronDaggerFrost01) carry their
-- enchantment in the form itself; vanilla gates them behind level, the
-- randomizer's flat pools would spread them everywhere.
local function isEnchantedVariant(form)
    return string.sub(form.editorId or "", 1, 4) == "Ench"
end

-- Difficulty banding: pairs n slots with n forms of equal power rank, then
-- perturbs the pairing within a window. window = one band at tierDrift 0
-- (strict bands, difficulty curve kept) and the whole pool at tierDrift 1
-- (full chaos). tierBands slices the power-ordered pool.
local function bandedPairing(n)
    local bandSize = math.max(1, math.ceil(n / CONFIG.tierBands))
    local window = math.max(bandSize, math.floor(n * CONFIG.tierDrift))
    local assign = {}
    for i = 1, n do
        assign[i] = i
    end
    for i = 1, n do
        local lo = math.max(1, i - window)
        local hi = math.min(n, i + window)
        local j = math.random(lo, hi)
        assign[i], assign[j] = assign[j], assign[i]
    end
    return assign
end

-- Swaps the forms of the same type pool across the given lists. Each slot
-- keeps its own count/level; only the form changes. Conservation: every slot
-- receives a form because the pool is exactly the multiset of forms of its
-- type. Protected forms (quest refs) are excluded from pools and their slots
-- keep the original form, so quest lists never receive shuffled gear.
--
-- Difficulty: slots are paired with forms of a similar power rank within a
-- window (bandedPairing), so early-game lists keep early-game loot. Enchanted
-- variants beyond CONFIG.enchantedLootRatio never enter the pools; they stay
-- in their original lists (vanilla placement, tier-aligned by construction).
local function shuffleLeveledEntries(lists)
    local slotsByType = {}
    local poolByType = {}
    for _, list in ipairs(lists) do
        local entries = list:entries()
        for idx, entry in ipairs(entries) do
            local f = entry.form
            if f and not isExcluded(f) then
                local t = f.type
                slotsByType[t] = slotsByType[t] or {}
                table.insert(slotsByType[t], { list = list, idx = idx, form = f, level = entry.level, count = entry.count })
                poolByType[t] = poolByType[t] or {}
                table.insert(poolByType[t], { form = f })
            end
        end
    end
    -- pairs() order over a hash is non-deterministic between processes (Lua's
    -- random hash seed); sort the pool keys so the same seed always consumes
    -- the RNG in the same order -> the same world on every launch.
    local poolKeys = {}
    for t in pairs(poolByType) do
        table.insert(poolKeys, t)
    end
    table.sort(poolKeys)

    local changed = 0
    local plan = {} -- list -> idx -> { form, level, count }
    for _, t in ipairs(poolKeys) do
        local slots = slotsByType[t]
        local pool = poolByType[t]

        -- power-sort both sides (pool[i] corresponds to slots[i] by construction)
        table.sort(slots, function(a, b)
            return (formPower(a.form) or -1) < (formPower(b.form) or -1)
        end)
        table.sort(pool, function(a, b)
            return (formPower(a.form) or -1) < (formPower(b.form) or -1)
        end)

        -- enchantedLootRatio: cap Ench* variants in weapon/armor loot pools.
        -- The capped-out variants keep their original slots (never pooled).
        if CONFIG.enchantedLootRatio < 1.0 and (t == "Weapon" or t == "Armor") then
            local ench = {}
            for i, e in ipairs(pool) do
                if isEnchantedVariant(e.form) then
                    table.insert(ench, i)
                end
            end
            local keep = math.min(#ench, math.ceil(#ench * CONFIG.enchantedLootRatio))
            local removed = {}
            for k = 1, #ench - keep do
                removed[ench[k]] = true
            end
            if #removed > 0 then
                local keepSlots, keepPool = {}, {}
                for i = 1, #pool do
                    if not removed[i] then
                        table.insert(keepSlots, slots[i])
                        table.insert(keepPool, pool[i])
                    end
                end
                slots, pool = keepSlots, keepPool
            end
        end

        local assign = bandedPairing(#slots)
        for i, slot in ipairs(slots) do
            local form = pool[assign[i]].form
            if form.formId ~= slot.form.formId then
                changed = changed + 1
            end
            plan[slot.list] = plan[slot.list] or {}
            plan[slot.list][slot.idx] = { form = form, level = slot.level, count = slot.count }
        end
    end

    for _, list in ipairs(lists) do
        local entries = list:entries()
        list:clear()
        for idx, entry in ipairs(entries) do
            local p = plan[list] and plan[list][idx]
            if p then
                list:add(p.form, p.level, p.count)
            else
                -- quest-protected slot: keep the original form, do NOT consume
                -- the pool (the pool holds exactly one form per open slot)
                skippedProtected = skippedProtected + 1
                list:add(entry.form, entry.level, entry.count)
            end
        end
    end
    return changed
end

-- Same swap for FormList contents (no counts/levels there), banded the same
-- way so power-ordered lists keep their rough progression.
local function shuffleFormLists(lists)
    local slotsByType = {}
    local poolByType = {}
    for _, fl in ipairs(lists) do
        for idx, form in ipairs(fl:forms()) do
            if form and not isExcluded(form) then
                local t = form.type
                slotsByType[t] = slotsByType[t] or {}
                table.insert(slotsByType[t], { list = fl, idx = idx, form = form })
                poolByType[t] = poolByType[t] or {}
                table.insert(poolByType[t], { form = form })
            end
        end
    end
    -- pairs() order over a hash is non-deterministic between processes (Lua's
    -- random hash seed); sort the pool keys so the same seed always consumes
    -- the RNG in the same order -> the same world on every launch.
    local poolKeys = {}
    for t in pairs(poolByType) do
        table.insert(poolKeys, t)
    end
    table.sort(poolKeys)

    local changed = 0
    local plan = {} -- list -> idx -> form
    for _, t in ipairs(poolKeys) do
        local slots = slotsByType[t]
        local pool = poolByType[t]
        table.sort(slots, function(a, b)
            return (formPower(a.form) or -1) < (formPower(b.form) or -1)
        end)
        table.sort(pool, function(a, b)
            return (formPower(a.form) or -1) < (formPower(b.form) or -1)
        end)

        local assign = bandedPairing(#slots)
        for i, slot in ipairs(slots) do
            local form = pool[assign[i]].form
            if form.formId ~= slot.form.formId then
                changed = changed + 1
            end
            plan[slot.list] = plan[slot.list] or {}
            plan[slot.list][slot.idx] = form
        end
    end

    for _, fl in ipairs(lists) do
        local forms = fl:forms()
        fl:clear()
        for idx, form in ipairs(forms) do
            local slot = plan[fl] and plan[fl][idx]
            if slot then
                fl:add(slot)
            else
                skippedProtected = skippedProtected + 1
                fl:add(form)
            end
        end
    end
    return changed
end

-- Container contents swap: slots (form + count) keep their count, the form is
-- swapped within a same-type pool. Empty containers stay empty. Protected
-- slots keep their original form.
local function shuffleContainerContents(containers)
    local pools = {}
    for _, c in ipairs(containers) do
        for _, entry in ipairs(c:contents()) do
            if entry.form and not isExcluded(entry.form) then
                local t = entry.form.type
                pools[t] = pools[t] or {}
                table.insert(pools[t], entry.form)
            end
        end
    end
    local poolKeys = {}
    for t in pairs(pools) do
        table.insert(poolKeys, t)
    end
    table.sort(poolKeys)
    for _, t in ipairs(poolKeys) do
        fisherYates(pools[t])
    end

    local cursor = {}
    local changed = 0
    for _, c in ipairs(containers) do
        local contents = c:contents()
        local assigned = {}
        for _, entry in ipairs(contents) do
            if isExcluded(entry.form) then
                -- quest slot: keep the original item, do not consume the pool
                skippedProtected = skippedProtected + 1
                table.insert(assigned, { form = entry.form, count = entry.count })
            else
                local t = entry.form.type
                local pool = pools[t]
                local idx = (cursor[t] or 0) + 1
                cursor[t] = idx
                local form = pool and pool[idx] or entry.form
                if form.formId ~= entry.form.formId then changed = changed + 1 end
                table.insert(assigned, { form = form, count = entry.count })
            end
        end
        c:setContents(assigned)
    end
    return changed
end

-- Leveled-character (LVLN) shuffle, structure-preserving. Skyrim's NPC
-- spawn lists form a pyramid: LChar encounter lists reference SubChar
-- difficulty-tier lists, which reference the concrete NPCs. The naive
-- all-in-one-pool swap (shuffleLeveledEntries) breaks that pyramid and mixes
-- every difficulty into every region ("Violent Skyrim"). Here:
--   * nested entries (a list referencing another list) stay put
--   * SubChar tier lists only trade NPCs with the SAME tier (the number
--     embedded in the EditorID: SubCharGhost06 <-> SubCharThalmor06)
--   * LChar encounter lists trade their direct NPCs within the LChar pool
--   * quest/dungeon-specific lists (dun*, MQ*, DA*, CR0, WEA*, ...) are
--     left alone entirely
local function characterListKey(ll)
    local ed = ll.editorId or ""
    local tier = ed:match("^Sub%a+(%d+)")
    if tier then
        return "Sub" .. tier
    end
    if ed:find("^LChar") then
        return "LChar"
    end
    return nil
end

local function shuffleLeveledCharacterEntries(lists)
    local groups = {}
    for _, ll in ipairs(lists) do
        local key = characterListKey(ll)
        if key then
            groups[key] = groups[key] or {}
            table.insert(groups[key], ll)
        end
    end

    local changed = 0
    local keys = {}
    for k in pairs(groups) do
        table.insert(keys, k)
    end
    table.sort(keys)
    for _, key in ipairs(keys) do
        local pool = {}
        local slots = {} -- { list, form, level, count, keep }
        for _, ll in ipairs(groups[key]) do
            for _, entry in ipairs(ll:entries()) do
                if entry.form and not isExcluded(entry.form) then
                    if entry.form.type == "LeveledNPC" then
                        -- nested list reference: keep the pyramid
                        table.insert(slots, {
                            list = ll, form = entry.form, level = entry.level, count = entry.count, keep = true,
                        })
                    else
                        table.insert(pool, entry.form)
                        table.insert(slots, {
                            list = ll, form = entry.form, level = entry.level, count = entry.count,
                        })
                    end
                end
            end
        end
        fisherYates(pool)

        local cleared = {}
        for _, ll in ipairs(groups[key]) do
            if not cleared[ll] then
                cleared[ll] = true
                ll:clear()
            end
        end

        local cursor = 0
        for _, slot in ipairs(slots) do
            local form
            if slot.keep then
                form = slot.form
            else
                cursor = cursor + 1
                form = pool[cursor]
            end
            if form.formId ~= slot.form.formId then changed = changed + 1 end
            slot.list:add(form, slot.level, slot.count)
        end
    end
    return changed
end

-- Effect-slot swap for magic items (ingredients/potions/enchantments): every
-- item keeps its number of slots; the slots themselves (baseEffect + stats)
-- are pooled and redistributed. Same conservation argument as leveled lists.
-- Protected items are left untouched and protected base effects stay out of
-- the pools.
local function sameEffect(a, b)
    if (a.baseEffect == nil) ~= (b.baseEffect == nil) then return false end
    -- userdata identity comparison is unreliable; forms are unique per formId
    if a.baseEffect and a.baseEffect.formId ~= b.baseEffect.formId then return false end
    return a.magnitude == b.magnitude and a.area == b.area and a.duration == b.duration
end

local function effectProtected(e)
    return e.baseEffect ~= nil and isExcluded(e.baseEffect)
end

local function shuffleEffectSlots(items)
    local slots = {}
    for _, item in ipairs(items) do
        if not isExcluded(item) then
            for _, e in ipairs(item:effects()) do
                if not effectProtected(e) then
                    table.insert(slots, e)
                end
            end
        end
    end
    fisherYates(slots)

    local cursor = 0
    local changed = 0
    for _, item in ipairs(items) do
        if isExcluded(item) then
            -- quest item: keep its effects untouched
            skippedProtected = skippedProtected + 1
        else
            local effects = item:effects()
            local assigned = {}
            local same = true
            for j, e in ipairs(effects) do
                if effectProtected(e) then
                    -- quest effect slot: keep the original effect
                    table.insert(assigned, e)
                    skippedProtected = skippedProtected + 1
                else
                    cursor = cursor + 1
                    table.insert(assigned, slots[cursor])
                end
                if same and not sameEffect(effects[j], assigned[j]) then
                    same = false
                end
            end
            if not same then
                changed = changed + 1
                item:setEffects(assigned)
            end
        end
    end
    return changed
end

-- Shout spell swap: every spell variation slot keeps its word/recoveryTime,
-- the spell is pooled across all shouts and redistributed. Protected spells
-- stay out of the pool and protected slots keep their original spell.
local function shuffleShoutSpells(shouts)
    local pool = {}
    for _, s in ipairs(shouts) do
        for _, v in ipairs(s:variations()) do
            if v.spell and not isExcluded(v.spell) then
                table.insert(pool, v.spell)
            end
        end
    end
    fisherYates(pool)

    local cursor = 0
    local changed = 0
    for _, s in ipairs(shouts) do
        local assigned = {}
        for _, v in ipairs(s:variations()) do
            if v.spell then
                if isExcluded(v.spell) then
                    -- quest slot: keep the original spell, do not consume the pool
                    skippedProtected = skippedProtected + 1
                    table.insert(assigned, { word = v.word, spell = v.spell, recoveryTime = v.recoveryTime })
                else
                    cursor = cursor + 1
                    local spell = pool[cursor]
                    if spell.formId ~= v.spell.formId then changed = changed + 1 end
                    table.insert(assigned, { word = v.word, spell = spell, recoveryTime = v.recoveryTime })
                end
            end
        end
        s:setVariations(assigned)
    end
    return changed
end

local function jitterStats()
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * CONFIG.statJitter)
    end

    for _, w in ipairs(lua_patcher.allWeapons()) do
        if isExcluded(w) then
            skippedProtected = skippedProtected + 1
            goto continue
        end
        if CONFIG.skipEnchanted and w.enchantment then goto continue end
        if not w.playable then goto continue end
        w.damage = math.max(1, math.floor(jitter(w.damage or 1) + 0.5))
        if w.weight then w.weight = math.max(0.1, jitter(w.weight)) end
        if w.value then w.value = math.max(1, math.floor(jitter(w.value) + 0.5)) end
        changed = changed + 1
        ::continue::
    end
    for _, a in ipairs(lua_patcher.allArmors()) do
        if isExcluded(a) then
            skippedProtected = skippedProtected + 1
            goto continue
        end
        if CONFIG.skipEnchanted and a.enchantment then goto continue end
        if not a.playable then goto continue end
        a.armorRating = math.max(1, jitter(a.armorRating or 1))
        if a.weight then a.weight = math.max(0.1, jitter(a.weight)) end
        if a.value then a.value = math.max(1, math.floor(jitter(a.value) + 0.5)) end
        changed = changed + 1
        ::continue::
    end
    return changed
end

local function jitterMagic()
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * CONFIG.magicJitter)
    end

    for _, s in ipairs(lua_patcher.allSpells()) do
        if isExcluded(s) then goto continue end
        if s.costOverride and s.costOverride > 0 then
            s.costOverride = math.max(1, math.floor(jitter(s.costOverride) + 0.5))
            changed = changed + 1
        end
        ::continue::
    end
    for _, m in ipairs(lua_patcher.allMagicEffects()) do
        if isExcluded(m) then goto continue end
        if m.baseCost and m.baseCost > 0 then
            m.baseCost = math.max(0.1, jitter(m.baseCost))
            changed = changed + 1
        end
        ::continue::
    end
    return changed
end

-- Actor chaos: level/attribute jitter plus a per-skill pool shuffle across
-- every NPC_ record. Off by default: touches quest NPCs and creatures too.
local function randomizeActors()
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * CONFIG.actorJitter)
    end

    local actors = lua_patcher.allActors()
    for _, a in ipairs(actors) do
        if isExcluded(a) then
            -- quest NPC (or the player): keep level/attributes
            skippedProtected = skippedProtected + 1
            goto continue
        end
        if a.level > 0 then -- 0 = scales with player level, keep it
            a.level = math.max(1, math.floor(jitter(a.level) + 0.5))
            changed = changed + 1
        end
        a.health = math.max(1, math.floor(jitter(a.health or 1) + 0.5))
        a.magicka = math.max(0, math.floor(jitter(a.magicka or 0) + 0.5))
        a.stamina = math.max(0, math.floor(jitter(a.stamina or 0) + 0.5))
        ::continue::
    end

    -- cache the skill arrays once per actor instead of per skill column
    local actorSkills = {}
    for i, a in ipairs(actors) do
        actorSkills[i] = a:skills()
    end

    -- shuffle each skill column across all actors (conservation per skill)
    local skills = actorSkills[1] or {}
    for i = 1, #skills do
        local pool = {}
        for j, a in ipairs(actors) do
            if not isExcluded(a) then
                table.insert(pool, actorSkills[j][i].value)
            end
        end
        fisherYates(pool)
        local cursor = 0
        for j, a in ipairs(actors) do
            if not isExcluded(a) then
                cursor = cursor + 1
                a:setSkill(i, pool[cursor])
            end
        end
    end
    return changed
end

local function randomizeLights()
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * CONFIG.lightJitter)
    end

    for _, l in ipairs(lua_patcher.allLights()) do
        if isExcluded(l) then
            skippedProtected = skippedProtected + 1
            goto continue
        end
        if l.canCarry then goto continue end -- torches/equipped lights: keep sane
        l.radius = math.max(32, math.min(2048, math.floor(jitter(l.radius or 256) + 0.5)))
        l.fade = math.max(0.1, jitter(l.fade or 1))
        local c = l.color
        l.color = {
            r = math.max(0, math.min(255, math.floor(jitter(c and c.r or 255) + 0.5))),
            g = math.max(0, math.min(255, math.floor(jitter(c and c.g or 255) + 0.5))),
            b = math.max(0, math.min(255, math.floor(jitter(c and c.b or 255) + 0.5))),
        }
        changed = changed + 1
        ::continue::
    end
    return changed
end

-- Enchantment swap: every enchanted piece of gear keeps its slot, the
-- enchantment itself is pooled within its casting type and redistributed.
-- Constant-effect enchantments (armor-type) only land on armor, cast-type
-- enchantments (weapon-type) only on weapons; unenchanted gear stays
-- unenchanted. Gear that is quest-protected or carries a protected
-- enchantment is left untouched.
--
-- Difficulty: gear slots and enchantments are paired by power rank within a
-- window (bandedPairing), so a steel sword trades enchantments with its
-- power peers — an iron dagger no longer ends up with Absorb Health 25.
local function shuffleGearEnchantments()
    local pools = { cast = {}, constant = {} }
    local slots = { cast = {}, constant = {} }

    local function poolFor(e)
        return e.castingType == "ConstantEffect" and "constant" or "cast"
    end

    -- Total effect magnitude as the enchantment's power score.
    local function enchantPower(e)
        local total = 0
        for _, ef in ipairs(e:effects()) do
            total = total + (ef.magnitude or 0)
        end
        return total
    end

    for _, w in ipairs(lua_patcher.allWeapons()) do
        local e = w.enchantment
        if e and not isExcluded(w) and not isExcluded(e) then
            local key = poolFor(e)
            table.insert(pools[key], { form = e, power = enchantPower(e) })
            table.insert(slots[key], { form = w, power = formPower(w) or 0 })
        end
    end
    for _, a in ipairs(lua_patcher.allArmors()) do
        local e = a.enchantment
        if e and not isExcluded(a) and not isExcluded(e) then
            local key = poolFor(e)
            table.insert(pools[key], { form = e, power = enchantPower(e) })
            table.insert(slots[key], { form = a, power = formPower(a) or 0 })
        end
    end

    local changed = 0
    for _, key in ipairs({ "cast", "constant" }) do
        local pool = pools[key]
        local gear = slots[key]
        table.sort(gear, function(a, b) return a.power < b.power end)
        table.sort(pool, function(a, b) return a.power < b.power end)

        local assign = bandedPairing(#gear)
        for i, slot in ipairs(gear) do
            local ench = pool[assign[i]].form
            if ench.formId ~= slot.form.enchantment.formId then
                slot.form.enchantment = ench
                changed = changed + 1
            end
        end
    end
    return changed
end

-- Encounter zone difficulty swap, order-preserving: zones are sorted by their
-- min level and each zone only trades its (minLevel, maxLevel) pair with a
-- neighbor within a small window (zoneSwapWindow positions). Weak areas stay
-- weak, strong areas stay strong, but the concrete numbers still shuffle.
-- Only zones with fully explicit levels (min >= 0 and max >= 0) participate;
-- pairs are swapped whole, so no zone ever ends up with min > max.
local function shuffleEncounterZones()
    local zones = {}
    for _, z in ipairs(lua_patcher.allEncounterZones()) do
        if z.hasLevels and z.minLevel >= 0 and z.maxLevel >= 0 and not isExcluded(z) then
            table.insert(zones, z)
        end
    end

    table.sort(zones, function(a, b) return a.minLevel < b.minLevel end)

    local changed = 0
    for i = 1, #zones do
        local lo = math.max(1, i - CONFIG.zoneSwapWindow)
        local hi = math.min(#zones, i + CONFIG.zoneSwapWindow)
        local j = math.random(lo, hi)
        if i ~= j then
            local mi, xi = zones[i].minLevel, zones[i].maxLevel
            zones[i].minLevel, zones[i].maxLevel = zones[j].minLevel, zones[j].maxLevel
            zones[j].minLevel, zones[j].maxLevel = mi, xi
            changed = changed + 2
        end
    end
    return changed
end

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
        if not isExcluded(ll) and not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeSuffixes) then
            if #CONFIG.includePrefixes == 0 or hasPrefix(ed, CONFIG.includePrefixes) then
                table.insert(lists, ll)
            end
        end
    end
    if not CONFIG.shuffleLeveledItems then
        lists = {}
    end
    candidateLists = candidateLists + #lists
    slotChanges = slotChanges + shuffleLeveledEntries(lists)

    if CONFIG.shuffleLeveledCharacters then
        local charLists = {}
        for _, ll in ipairs(lua_patcher.allLeveledCharacters()) do
            local ed = ll.editorId or ""
            if not isExcluded(ll) and not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeSuffixes) then
                if #CONFIG.includePrefixes == 0 or hasPrefix(ed, CONFIG.includePrefixes) then
                    table.insert(charLists, ll)
                end
            end
        end
        candidateLists = candidateLists + #charLists
        slotChanges = slotChanges + shuffleLeveledCharacterEntries(charLists)
    end

    if CONFIG.shuffleLeveledSpells then
        local spellLists = {}
        for _, ll in ipairs(lua_patcher.allLeveledSpells()) do
            local ed = ll.editorId or ""
            if not isExcluded(ll) and not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeSuffixes) then
                if #CONFIG.includePrefixes == 0 or hasPrefix(ed, CONFIG.includePrefixes) then
                    table.insert(spellLists, ll)
                end
            end
        end
        candidateLists = candidateLists + #spellLists
        slotChanges = slotChanges + shuffleLeveledEntries(spellLists)
    end
end

if CONFIG.shuffleFormLists then
    local lists = {}
    for _, fl in ipairs(lua_patcher.allFormLists()) do
        local ed = fl.editorId or ""
        if not isExcluded(fl) and not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.formListExcludeSuffixes) then
            table.insert(lists, fl)
        end
    end
    candidateLists = candidateLists + #lists
    slotChanges = slotChanges + shuffleFormLists(lists)
end

local containerChanges = 0
if CONFIG.shuffleContainers then
    local containers = {}
    for _, c in ipairs(lua_patcher.allContainers()) do
        local ed = c.editorId or ""
        if not isExcluded(c) and not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeSuffixes) then
            table.insert(containers, c)
        end
    end
    containerChanges = shuffleContainerContents(containers)
end

local effectChanges = 0
if CONFIG.shuffleIngredients then
    effectChanges = effectChanges + shuffleEffectSlots(lua_patcher.allIngredients())
end
if CONFIG.shufflePotions then
    effectChanges = effectChanges + shuffleEffectSlots(lua_patcher.allPotions())
end
if CONFIG.shuffleEnchantments then
    effectChanges = effectChanges + shuffleEffectSlots(lua_patcher.allEnchantments())
end
if CONFIG.shuffleShouts then
    effectChanges = effectChanges + shuffleShoutSpells(lua_patcher.allShouts())
end

local statChanges = 0
if CONFIG.randomizeStats then
    statChanges = jitterStats()
end

local magicChanges = 0
if CONFIG.randomizeMagic then
    magicChanges = jitterMagic()
end

local actorChanges = 0
if CONFIG.randomizeActors then
    actorChanges = randomizeActors()
end

local lightChanges = 0
if CONFIG.randomizeLights then
    lightChanges = randomizeLights()
end

local enchantmentChanges = 0
if CONFIG.shuffleGearEnchantments then
    enchantmentChanges = shuffleGearEnchantments()
end

local zoneChanges = 0
if CONFIG.shuffleEncounterZones then
    zoneChanges = shuffleEncounterZones()
end

print(string.format(
    "EverythingRandomizer: seed=%d lists=%d swappedSlots=%d containers=%d effects=%d statJittered=%d magicJittered=%d actors=%d lights=%d enchantments=%d zones=%d protectedSlots=%d",
    CONFIG.seed, candidateLists, slotChanges, containerChanges, effectChanges, statChanges, magicChanges, actorChanges,
    lightChanges, enchantmentChanges, zoneChanges, skippedProtected))
