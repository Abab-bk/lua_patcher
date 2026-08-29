-- Swap-based shuffles for the EverythingRandomizer. Every function takes a
-- context table supplied by EverythingRandomizer.lua:
--   ctx.config     — the validated CONFIG table
--   ctx.isExcluded — form filter (quest protection + internal + custom)
--   ctx.skipped    — protected-slot counter, incremented as slots are skipped
--
-- Module files are never executed as top-level scripts by LuaPatcher.

local util = require("_randomizer_util")

-- Swaps the forms of the same type pool across the given lists. Each slot
-- keeps its own count/level; only the form changes. Conservation: every slot
-- receives a form because the pool is exactly the multiset of forms of its
-- type. Protected forms (quest refs) are excluded from pools and their slots
-- keep the original form, so quest lists never receive shuffled gear.
--
-- Difficulty: slots are paired with forms of a similar power rank within a
-- window (bandedPairing), so early-game lists keep early-game loot. Enchanted
-- variants beyond ctx.config.enchantedLootRatio never enter the pools; they
-- stay in their original lists (vanilla placement, tier-aligned by construction).
local function shuffleLeveledEntries(ctx, lists)
    local slotsByType = {}
    local poolByType = {}
    for _, list in ipairs(lists) do
        local entries = list:entries()
        for idx, entry in ipairs(entries) do
            local f = entry.form
            if f and not ctx.isExcluded(f) then
                local t = f.type
                slotsByType[t] = slotsByType[t] or {}
                table.insert(slotsByType[t],
                    { list = list, idx = idx, form = f, level = entry.level, count = entry.count })
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
            return (util.formPower(a.form) or -1) < (util.formPower(b.form) or -1)
        end)
        table.sort(pool, function(a, b)
            return (util.formPower(a.form) or -1) < (util.formPower(b.form) or -1)
        end)

        -- enchantedLootRatio: cap Ench* variants in weapon/armor loot pools.
        -- The capped-out variants keep their original slots (never pooled).
        if ctx.config.enchantedLootRatio < 1.0 and (t == "Weapon" or t == "Armor") then
            local ench = {}
            for i, e in ipairs(pool) do
                if util.isEnchantedVariant(e.form) then
                    table.insert(ench, i)
                end
            end
            local keep = math.min(#ench, math.ceil(#ench * ctx.config.enchantedLootRatio))
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

        local assign = util.bandedPairing(#slots, ctx.config.tierBands, ctx.config.tierDrift)
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
                ctx.skipped = ctx.skipped + 1
                list:add(entry.form, entry.level, entry.count)
            end
        end
    end
    return changed
end

-- Same swap for FormList contents (no counts/levels there), banded the same
-- way so power-ordered lists keep their rough progression.
local function shuffleFormLists(ctx, lists)
    local slotsByType = {}
    local poolByType = {}
    for _, fl in ipairs(lists) do
        for idx, form in ipairs(fl:forms()) do
            if form and not ctx.isExcluded(form) then
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
            return (util.formPower(a.form) or -1) < (util.formPower(b.form) or -1)
        end)
        table.sort(pool, function(a, b)
            return (util.formPower(a.form) or -1) < (util.formPower(b.form) or -1)
        end)

        local assign = util.bandedPairing(#slots, ctx.config.tierBands, ctx.config.tierDrift)
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
                ctx.skipped = ctx.skipped + 1
                fl:add(form)
            end
        end
    end
    return changed
end

-- Container contents swap: slots (form + count) keep their count, the form is
-- swapped within a same-type pool. Empty containers stay empty. Protected
-- slots keep their original form.
local function shuffleContainerContents(ctx, containers)
    local pools = {}
    for _, c in ipairs(containers) do
        for _, entry in ipairs(c:contents()) do
            if entry.form and not ctx.isExcluded(entry.form) then
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
        util.fisherYates(pools[t])
    end

    local cursor = {}
    local changed = 0
    for _, c in ipairs(containers) do
        local contents = c:contents()
        local assigned = {}
        for _, entry in ipairs(contents) do
            if ctx.isExcluded(entry.form) then
                -- quest slot: keep the original item, do not consume the pool
                ctx.skipped = ctx.skipped + 1
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

local function shuffleLeveledCharacters(ctx, lists)
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
                if entry.form and not ctx.isExcluded(entry.form) then
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
        util.fisherYates(pool)

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

local function effectProtected(ctx, e)
    return e.baseEffect ~= nil and ctx.isExcluded(e.baseEffect)
end

local function shuffleEffectSlots(ctx, items)
    local slots = {}
    for _, item in ipairs(items) do
        if not ctx.isExcluded(item) then
            for _, e in ipairs(item:effects()) do
                if not effectProtected(ctx, e) then
                    table.insert(slots, e)
                end
            end
        end
    end
    util.fisherYates(slots)

    local cursor = 0
    local changed = 0
    for _, item in ipairs(items) do
        if ctx.isExcluded(item) then
            -- quest item: keep its effects untouched
            ctx.skipped = ctx.skipped + 1
        else
            local effects = item:effects()
            local assigned = {}
            local same = true
            for j, e in ipairs(effects) do
                if effectProtected(ctx, e) then
                    -- quest effect slot: keep the original effect
                    table.insert(assigned, e)
                    ctx.skipped = ctx.skipped + 1
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
local function shuffleShoutSpells(ctx, shouts)
    local pool = {}
    for _, s in ipairs(shouts) do
        for _, v in ipairs(s:variations()) do
            if v.spell and not ctx.isExcluded(v.spell) then
                table.insert(pool, v.spell)
            end
        end
    end
    util.fisherYates(pool)

    local cursor = 0
    local changed = 0
    for _, s in ipairs(shouts) do
        local assigned = {}
        for _, v in ipairs(s:variations()) do
            if v.spell then
                if ctx.isExcluded(v.spell) then
                    -- quest slot: keep the original spell, do not consume the pool
                    ctx.skipped = ctx.skipped + 1
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
local function shuffleGearEnchantments(ctx)
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
        if e and not ctx.isExcluded(w) and not ctx.isExcluded(e) then
            local key = poolFor(e)
            table.insert(pools[key], { form = e, power = enchantPower(e) })
            table.insert(slots[key], { form = w, power = util.formPower(w) or 0 })
        end
    end
    for _, a in ipairs(lua_patcher.allArmors()) do
        local e = a.enchantment
        if e and not ctx.isExcluded(a) and not ctx.isExcluded(e) then
            local key = poolFor(e)
            table.insert(pools[key], { form = e, power = enchantPower(e) })
            table.insert(slots[key], { form = a, power = util.formPower(a) or 0 })
        end
    end

    local changed = 0
    for _, key in ipairs({ "cast", "constant" }) do
        local pool = pools[key]
        local gear = slots[key]
        table.sort(gear, function(a, b) return a.power < b.power end)
        table.sort(pool, function(a, b) return a.power < b.power end)

        local assign = util.bandedPairing(#gear, ctx.config.tierBands, ctx.config.tierDrift)
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
-- neighbor within a small window (ctx.config.zoneSwapWindow positions). Weak
-- areas stay weak, strong areas stay strong, but the concrete numbers still
-- shuffle. Only zones with fully explicit levels (min >= 0 and max >= 0)
-- participate; pairs are swapped whole, so no zone ever ends up with min > max.
local function shuffleEncounterZones(ctx)
    local zones = {}
    for _, z in ipairs(lua_patcher.allEncounterZones()) do
        if z.hasLevels and z.minLevel >= 0 and z.maxLevel >= 0 and not ctx.isExcluded(z) then
            table.insert(zones, z)
        end
    end

    table.sort(zones, function(a, b) return a.minLevel < b.minLevel end)

    local changed = 0
    for i = 1, #zones do
        local lo = math.max(1, i - ctx.config.zoneSwapWindow)
        local hi = math.min(#zones, i + ctx.config.zoneSwapWindow)
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

return {
    leveledEntries = shuffleLeveledEntries,
    formLists = shuffleFormLists,
    containerContents = shuffleContainerContents,
    leveledCharacters = shuffleLeveledCharacters,
    effectSlots = shuffleEffectSlots,
    shoutSpells = shuffleShoutSpells,
    gearEnchantments = shuffleGearEnchantments,
    encounterZones = shuffleEncounterZones,
}
