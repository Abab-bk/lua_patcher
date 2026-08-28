-- LuaPatcher example: EverythingRandomizer — shuffle the world
-- priority: 50 (after GearInjection, so freshly injected gear gets shuffled too)
--
-- A Dark-Souls-style "everything randomizer":
--   * every leveled list entry is swapped with another entry of the SAME form
--     type (weapons <-> weapons, armor <-> armor, ...), preserving counts and
--     levels — total items and type pools are conserved
--   * leveled spell lists shuffle the same way (spells <-> spells)
--   * FormList contents shuffle too, excluding "*Set" smithing material sets
--   * container contents swap between containers, same-form-type pools
--   * ingredient / potion / enchantment effect slots swap within their pools
--   * shout spell variations swap between shouts
--   * gear stats get a seeded jitter (damage / armorRating / weight / value)
--   * lights get radius/color/fade jitter; NPCs can get level/attribute
--     jitter + skill shuffles (both off by default)
--
-- SCOPE: only TESForm records are modified (leveled lists, FormLists,
-- containers, ingredients, potions, enchantments, shouts, lights, NPC_
-- records and gear). GameSettings (GMST) are not a form type and are
-- deliberately not touched.
--
-- DETERMINISM: the layout is fully determined by CONFIG.seed (any integer;
-- the same seed always produces the same world). The patcher runs once per
-- game launch (kDataLoaded, not per save), so the same seed produces the same
-- world on every save. Change the seed to get a new world on the next launch;
-- share seeds to compare runs.
--
-- LIMITATIONS (no "logic" checking, unlike DS randomizer logic mode):
--   * quest-critical forms may end up in odd lists — protect quest lists with
--     excludePrefixes (e.g. { "LItemQuest" })
--   * TESLevCharacter (NPC spawn) lists are NOT shuffled by default — enable
--     via shuffleLeveledCharacters
--   * randomizeActors touches EVERY NPC_ record (creatures included): quest
--     NPCs get tougher/weaker too — keep it off unless you want that chaos
--   * smithing material sets stay untouched unless you remove "Set" from
--     formListExcludeSuffixes
--
-- CONFIG (optional): Data/SKSE/Plugins/LuaPatcher/Scripts/EverythingRandomizer_config.lua
-- (flat sibling of the script, mirrors examples/ layout).

local CONFIG = {
    enabled = true,
    seed = 1337,

    shuffleLeveledItems = true,
    shuffleLeveledCharacters = false, -- NPC spawn lists (quest-hostile)
    shuffleLeveledSpells = true,      -- spell leveled lists
    -- empty = every leveled list (vanilla + mods); or restrict, e.g. { "LItem" }
    includePrefixes = {},
    excludePrefixes = {},         -- quest-list protection, e.g. { "LItemQuest" }
    excludeEditorIdSuffixes = {}, -- e.g. { "Unique" }

    shuffleFormLists = true,
    formListExcludeSuffixes = { "Set" }, -- keep smithing material sets sane

    shuffleContainers = true,            -- chest/lootable contents swap, same-type pools

    shuffleIngredients = true,           -- ingredient effect slots swap
    shufflePotions = true,               -- potion/poison/food effect slots swap
    shuffleEnchantments = true,          -- enchantment effect slots swap
    shuffleShouts = true,                -- shout spell variations swap

    randomizeStats = true,
    statJitter = 0.2, -- +/-20% per stat
    skipEnchanted = true,

    randomizeMagic = false,
    magicJitter = 0.2,

    randomizeActors = false, -- level/attribute jitter + skill shuffle (chaotic)
    actorJitter = 0.2,

randomizeLights = true,    -- radius / color / fade
    lightJitter = 0.3,
}

do
    local loaded = nil
    if lua_patcher.tryLoadConfig then
        local ok, result = pcall(lua_patcher.tryLoadConfig, "EverythingRandomizer")
        if ok and type(result) == "table" then
            loaded = result
        elseif not ok then
            print(string.format("EverythingRandomizer: tryLoadConfig failed: %s", tostring(result)))
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
    if type(CONFIG.excludeEditorIdSuffixes) ~= "table" then CONFIG.excludeEditorIdSuffixes = {} end
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

-- Swaps the forms of the same type pool across the given lists. Each slot
-- keeps its own count/level; only the form changes. Conservation: every slot
-- receives a form because the pool is exactly the multiset of forms of its
-- type.
local function shuffleLeveledEntries(lists)
    local pools = {}
    for _, list in ipairs(lists) do
        for _, entry in ipairs(list:entries()) do
            if entry.form then
                local t = entry.form.type
                pools[t] = pools[t] or {}
                table.insert(pools[t], entry.form)
            end
        end
    end
    -- pairs() order over a hash is non-deterministic between processes (Lua's
    -- random hash seed); sort the pool keys so the same seed always consumes
    -- the RNG in the same order -> the same world on every launch.
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
    for _, list in ipairs(lists) do
        local entries = list:entries()
        list:clear()
        for _, entry in ipairs(entries) do
            local t = entry.form.type
            local pool = pools[t]
            local idx = (cursor[t] or 0) + 1
            cursor[t] = idx
            local form = pool and pool[idx]
            if not form then
                form = entry.form
            end
            -- userdata identity comparison is unreliable; forms are unique per formId
            if form.formId ~= entry.form.formId then changed = changed + 1 end
            list:add(form, entry.level, entry.count)
        end
    end
    return changed
end

-- Same swap for FormList contents (no counts/levels there).
local function shuffleFormLists(lists)
    local pools = {}
    for _, fl in ipairs(lists) do
        for _, form in ipairs(fl:forms()) do
            if form then
                local t = form.type
                pools[t] = pools[t] or {}
                table.insert(pools[t], form)
            end
        end
    end
    -- pairs() order over a hash is non-deterministic between processes (Lua's
    -- random hash seed); sort the pool keys so the same seed always consumes
    -- the RNG in the same order -> the same world on every launch.
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
    for _, fl in ipairs(lists) do
        local forms = fl:forms()
        fl:clear()
        for _, form in ipairs(forms) do
            local t = form.type
            local pool = pools[t]
            local idx = (cursor[t] or 0) + 1
            cursor[t] = idx
            local slot = pool and pool[idx] or form
            if slot.formId ~= form.formId then changed = changed + 1 end
            fl:add(slot)
        end
    end
    return changed
end

-- Container contents swap: slots (form + count) keep their count, the form is
-- swapped within a same-type pool. Empty containers stay empty.
local function shuffleContainerContents(containers)
    local pools = {}
    for _, c in ipairs(containers) do
        for _, entry in ipairs(c:contents()) do
            if entry.form then
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
            local t = entry.form.type
            local pool = pools[t]
            local idx = (cursor[t] or 0) + 1
            cursor[t] = idx
            local form = pool and pool[idx] or entry.form
            if form.formId ~= entry.form.formId then changed = changed + 1 end
            table.insert(assigned, { form = form, count = entry.count })
        end
        c:setContents(assigned)
    end
    return changed
end

-- Effect-slot swap for magic items (ingredients/potions/enchantments): every
-- item keeps its number of slots; the slots themselves (baseEffect + stats)
-- are pooled and redistributed. Same conservation argument as leveled lists.
local function sameEffect(a, b)
    if (a.baseEffect == nil) ~= (b.baseEffect == nil) then return false end
    -- userdata identity comparison is unreliable; forms are unique per formId
    if a.baseEffect and a.baseEffect.formId ~= b.baseEffect.formId then return false end
    return a.magnitude == b.magnitude and a.area == b.area and a.duration == b.duration
end

local function shuffleEffectSlots(items)
    local slots = {}
    local counts = {}
    for _, item in ipairs(items) do
        local effs = item:effects()
        table.insert(counts, #effs)
        for _, e in ipairs(effs) do
            table.insert(slots, e)
        end
    end
    fisherYates(slots)

    local cursor = 0
    local changed = 0
    for i, item in ipairs(items) do
        local n = counts[i]
        if n > 0 then
            local assigned = {}
            for _ = 1, n do
                cursor = cursor + 1
                table.insert(assigned, slots[cursor])
            end
            local before = item:effects()
            local same = true
            for j = 1, n do
                if not sameEffect(before[j], assigned[j]) then
                    same = false
                    break
                end
            end
            if not same then changed = changed + 1 end
            item:setEffects(assigned)
        end
    end
    return changed
end

-- Shout spell swap: every spell variation slot keeps its word/recoveryTime,
-- the spell is pooled across all shouts and redistributed.
local function shuffleShoutSpells(shouts)
    local pool = {}
    for _, s in ipairs(shouts) do
        for _, v in ipairs(s:variations()) do
            if v.spell then
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
                cursor = cursor + 1
                local spell = pool[cursor]
                if spell.formId ~= v.spell.formId then changed = changed + 1 end
                table.insert(assigned, { word = v.word, spell = spell, recoveryTime = v.recoveryTime })
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
        if CONFIG.skipEnchanted and w.enchantment then goto continue end
        if not w.playable then goto continue end
        w.damage = math.max(1, math.floor(jitter(w.damage or 1) + 0.5))
        if w.weight then w.weight = math.max(0.1, jitter(w.weight)) end
        if w.value then w.value = math.max(1, math.floor(jitter(w.value) + 0.5)) end
        changed = changed + 1
        ::continue::
    end
    for _, a in ipairs(lua_patcher.allArmors()) do
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
        if s.costOverride and s.costOverride > 0 then
            s.costOverride = math.max(1, math.floor(jitter(s.costOverride) + 0.5))
            changed = changed + 1
        end
    end
    for _, m in ipairs(lua_patcher.allMagicEffects()) do
        if m.baseCost and m.baseCost > 0 then
            m.baseCost = math.max(0.1, jitter(m.baseCost))
            changed = changed + 1
        end
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
        if a.level > 0 then -- 0 = scales with player level, keep it
            a.level = math.max(1, math.floor(jitter(a.level) + 0.5))
            changed = changed + 1
        end
        a.health = math.max(1, math.floor(jitter(a.health or 1) + 0.5))
        a.magicka = math.max(0, math.floor(jitter(a.magicka or 0) + 0.5))
        a.stamina = math.max(0, math.floor(jitter(a.stamina or 0) + 0.5))
    end

    -- shuffle each skill column across all actors (conservation per skill)
    local skills = actors[1] and actors[1]:skills() or {}
    for i = 1, #skills do
        local pool = {}
        for _, a in ipairs(actors) do
            table.insert(pool, a:skills()[i].value)
        end
        fisherYates(pool)
        local cursor = 0
        for _, a in ipairs(actors) do
            cursor = cursor + 1
            a:setSkill(i, pool[cursor])
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
        if not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeEditorIdSuffixes) then
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
            if not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeEditorIdSuffixes) then
                if #CONFIG.includePrefixes == 0 or hasPrefix(ed, CONFIG.includePrefixes) then
                    table.insert(charLists, ll)
                end
            end
        end
        candidateLists = candidateLists + #charLists
        slotChanges = slotChanges + shuffleLeveledEntries(charLists)
    end

    if CONFIG.shuffleLeveledSpells then
        local spellLists = {}
        for _, ll in ipairs(lua_patcher.allLeveledSpells()) do
            local ed = ll.editorId or ""
            if not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeEditorIdSuffixes) then
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
        if not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.formListExcludeSuffixes) then
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
        if not hasPrefix(ed, CONFIG.excludePrefixes) and not hasSuffix(ed, CONFIG.excludeEditorIdSuffixes) then
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

print(string.format(
    "EverythingRandomizer: seed=%d lists=%d swappedSlots=%d containers=%d effects=%d statJittered=%d magicJittered=%d actors=%d lights=%d",
    CONFIG.seed, candidateLists, slotChanges, containerChanges, effectChanges, statChanges, magicChanges, actorChanges,
    lightChanges))
