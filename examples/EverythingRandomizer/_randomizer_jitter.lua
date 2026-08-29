-- Jitter passes for the EverythingRandomizer: seeded stat/attribute noise on
-- top of the swaps. Every function takes the context table from
-- EverythingRandomizer.lua:
--   ctx.config     — the validated CONFIG table
--   ctx.isExcluded — form filter (quest protection + internal + custom)
--   ctx.skipped    — protected-slot counter, incremented as forms are skipped
--
-- Module files are never executed as top-level scripts by LuaPatcher.

local util = require("_randomizer_util")

local function jitterStats(ctx)
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * ctx.config.statJitter)
    end

    for _, w in ipairs(lua_patcher.allWeapons()) do
        if ctx.isExcluded(w) then
            ctx.skipped = ctx.skipped + 1
            goto continue
        end
        if ctx.config.skipEnchanted and w.enchantment then goto continue end
        if not w.playable then goto continue end
        w.damage = math.max(1, math.floor(jitter(w.damage or 1) + 0.5))
        if w.weight then w.weight = math.max(0.1, jitter(w.weight)) end
        if w.value then w.value = math.max(1, math.floor(jitter(w.value) + 0.5)) end
        changed = changed + 1
        ::continue::
    end
    for _, a in ipairs(lua_patcher.allArmors()) do
        if ctx.isExcluded(a) then
            ctx.skipped = ctx.skipped + 1
            goto continue
        end
        if ctx.config.skipEnchanted and a.enchantment then goto continue end
        if not a.playable then goto continue end
        a.armorRating = math.max(1, jitter(a.armorRating or 1))
        if a.weight then a.weight = math.max(0.1, jitter(a.weight)) end
        if a.value then a.value = math.max(1, math.floor(jitter(a.value) + 0.5)) end
        changed = changed + 1
        ::continue::
    end
    return changed
end

local function jitterMagic(ctx)
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * ctx.config.magicJitter)
    end

    for _, s in ipairs(lua_patcher.allSpells()) do
        if ctx.isExcluded(s) then goto continue end
        if s.costOverride and s.costOverride > 0 then
            s.costOverride = math.max(1, math.floor(jitter(s.costOverride) + 0.5))
            changed = changed + 1
        end
        ::continue::
    end
    for _, m in ipairs(lua_patcher.allMagicEffects()) do
        if ctx.isExcluded(m) then goto continue end
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
local function randomizeActors(ctx)
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * ctx.config.actorJitter)
    end

    local actors = lua_patcher.allActors()
    for _, a in ipairs(actors) do
        if ctx.isExcluded(a) then
            -- quest NPC (or the player): keep level/attributes
            ctx.skipped = ctx.skipped + 1
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
            if not ctx.isExcluded(a) then
                table.insert(pool, actorSkills[j][i].value)
            end
        end
        util.fisherYates(pool)
        local cursor = 0
        for j, a in ipairs(actors) do
            if not ctx.isExcluded(a) then
                cursor = cursor + 1
                a:setSkill(i, pool[cursor])
            end
        end
    end
    return changed
end

local function randomizeLights(ctx)
    local changed = 0
    local function jitter(v)
        return v * (1 + (math.random() * 2 - 1) * ctx.config.lightJitter)
    end

    for _, l in ipairs(lua_patcher.allLights()) do
        if ctx.isExcluded(l) then
            ctx.skipped = ctx.skipped + 1
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

return {
    stats = jitterStats,
    magic = jitterMagic,
    actors = randomizeActors,
    lights = randomizeLights,
}