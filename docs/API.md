# LuaPatcher API Reference

LuaPatcher patches Skyrim SE/AE game forms at runtime. Scripts live in
`Data/SKSE/Plugins/LuaPatcher/Scripts/` (recursively scanned, sorted), run once
at game load. Sibling `*_config.lua` files are **not** executed as scripts; they
are user configuration loaded on demand via `lua_patcher.tryLoadConfig`.

All API functions are written for Lua: options are passed as tables, filters as
predicate functions, and flags as named boolean properties — no string-encoded
conditions.

## Conventions

- **Form identifiers**: a string `"Plugin.esm|000123"` (mod name + hex local
  formID; light plugins use the 0xFFF-masked ID) or a bare `EditorID`.
- **Errors**: invalid arguments and failed lookups raise a Lua error (catchable
  with `pcall`). `lua_patcher.getForm` is the exception: it returns `nil` for a
  miss instead of raising.
- **Unknown properties** on form objects raise an error (typo protection);
  writing to a read-only property raises an error.

## `lua_patcher` module

| Function | Returns | Notes |
|---|---|---|
| `version` | string | Plugin API version |
| `log(...)` / `warn(...)` / `error(...)` | — | Joins arguments with tabs (`tostring` on each) into the plugin log |
| `getForm(identifier)` | Form or nil | Resolves `"Mod.esm\|000123"` / EditorID; never raises |
| `isPluginInstalled(name)` | bool | Is the plugin in the load order |
| `tryLoadConfig(name)` | table or nil | Loads sibling `Scripts/<name>_config.lua` (legacy `Config/<name>.lua` fallbacks), returns the chunk's return value; nil if missing/failed; raises on invalid names |
| `leveledList(formOrId)` | LeveledList | Raises if the form is not a leveled item/character list |
| `allLeveledItems()` | array of LeveledList | Every `TESLevItem` |
| `allLeveledCharacters()` | array of LeveledList | Every `TESLevCharacter` |
| `allWeapons()` / `allArmors()` / `allSpells()` / `allMagicEffects()` | arrays | Every form of that type |
| `findLeveledListsContaining(formOrId)` | array of LeveledList | Snapshot reverse index: lists that referenced the form in the game's pristine data (patches made this run are not visible) |

`print(...)` is redirected to the plugin log (same formatting as `log`).

## Form (base type)

Weapon, Armor, LeveledList, Spell and MagicEffect objects inherit these.

| Property | Type | Notes |
|---|---|---|
| `formId` | integer | Full formID |
| `typeId` | integer | Engine form type |
| `type` | string | `"Weapon"`, `"Armor"`, `"Spell"`, `"LeveledItem"`, ... |
| `editorId` | string or nil | From the game's editorID table |
| `name` | string or nil | Full name |
| `identifier` | string | `"Plugin.esm\|000123"` form |
| `plugin` | string or nil | Source plugin |
| `value` | integer or nil | If the form has a value |
| `weight` | number or nil | If the form has a weight |
| `enchantment` | Form or nil | The enchantment form |
| `keywords` | array of Form | All keywords (as form objects) |

| Method | Returns | Notes |
|---|---|---|
| `form:hasKeyword(kwOrId)` | bool | Checks the keyword form |

## Weapon (extends Form)

| Property | R/W | Type | Notes |
|---|---|---|---|
| `damage` | rw | integer | Clamped to 0..65535 |
| `speed` / `reach` / `stagger` | rw | number | |
| `critDamage` | rw | integer | Clamped to 0..65535 |
| `weight` | rw | number | |
| `value` | rw | integer | Non-negative |
| `weaponType` | ro | string | `"OneHandedSword"`, `"Bow"`, ... |
| `skill` | ro | string | `"OneHanded"`, `"TwoHanded"`, `"Marksman"`, `"None"` |
| `melee` / `ranged` / `bow` / `staff` / `crossbow` | ro | bool | |
| `playable` | ro | bool | |

Methods: `weapon:addKeyword(kwOrId)`, `weapon:removeKeyword(kwOrId)` → bool.

## Armor (extends Form)

| Property | R/W | Type | Notes |
|---|---|---|---|
| `armorRating` | rw | number | Stored as `rating * 100` (rounded) |
| `weight` | rw | number | |
| `value` | rw | integer | Non-negative |
| `armorType` | ro | string | `"Light"`, `"Heavy"`, `"Clothing"`, `"Other"` |
| `slots` | ro | array of string | Biped slot names (`"Head"`, `"Body"`, ...) |
| `playable` | ro | bool | |

Methods: `armor:addKeyword(kwOrId)`, `armor:removeKeyword(kwOrId)` → bool.

## LeveledList (extends Form)

| Property | R/W | Type | Notes |
|---|---|---|---|
| `numEntries` | ro | integer | |
| `chanceNone` | rw | integer | Stored as int8 |
| `chanceGlobal` | rw | Form or nil | Must be a `TESGlobal` |
| `calculateFromAllLevels` | rw | bool | `kCalculateFromAllLevelsLTOrEqPCLevel` flag |
| `calculateForEachItem` | rw | bool | `kCalculateForEachItemInCount` flag |
| `useAll` | rw | bool | `kUseAll` flag |
| `specialLoot` | rw | bool | `kSpecialLoot` flag |

Entry snapshots have the shape `{ form = <Form>, count = integer, level = integer }`.

| Method | Notes |
|---|---|
| `list:add(formOrId)` | Appends at level 1, count 1 |
| `list:add(formOrId, level)` | Positional form |
| `list:add(formOrId, level, count)` | Positional form |
| `list:add(formOrId, { level = n, count = n })` | Options-table form (both accepted) |
| `list:addIfAbsent(...)` | Like `add`, but skips if the form is already present |
| `list:remove(formOrId)` | Removes **all** entries of the form |
| `list:remove(formOrId, { minLevel =, maxLevel =, minCount =, maxCount = })` | Removes matching entries; bounds are **inclusive** |
| `list:removeIf(predicate)` | Removes every entry for which `predicate(entry)` is truthy; atomic (a predicate error leaves the list unchanged) |
| `list:removeByKeyword(kwOrId)` | Removes entries whose form has the keyword |
| `list:replace(fromOrId, toOrId)` | Swaps the form of every matching entry |
| `list:multiplyCount(formOrId, factor)` | `count = ceil(count * factor)` |
| `list:has(formOrId)` | bool |
| `list:clear()` | Removes all entries |
| `list:sort()` | Sorts entries by level |
| `list:entries()` | array of entry snapshots |

After every mutation the list is kept sorted by level. `findLeveledListsContaining`
and the entries of `entries()` are snapshots: patches applied during this run are
not reflected.

```lua
local ll = lua_patcher.leveledList("LuaPatcherExample.esp|00001000")

ll:add("LuaPatcherExample.esp|00002000", 5, 2)          -- positional
ll:add("LuaPatcherExample.esp|00002001", { level = 10 })-- options table

-- remove every entry of the form with level >= 5 and count <= 2
ll:remove("LuaPatcherExample.esp|00002002", { minLevel = 5, maxCount = 2 })

-- predicate: full Lua power
ll:removeIf(function(e)
    return e.form.plugin == "SomeMod.esp" and e.level > 20
end)

ll.calculateForEachItem = true
ll.chanceNone = 50
```

## Spell (extends Form)

| Property | R/W | Type | Notes |
|---|---|---|---|
| `costOverride` | rw | integer | |
| `spellType` | rw | string | `"Spell"`, `"Disease"`, `"Power"`, `"LesserPower"`, `"Ability"`, `"Poison"` |
| `castingType` | rw | string | `"ConstantEffect"`, `"FireAndForget"`, `"Concentration"`, `"Scroll"` |
| `delivery` | rw | string | `"Self"`, `"Touch"`, `"Aimed"`, `"TargetActor"`, `"TargetLocation"` |
| `chargeTime` / `castDuration` / `range` | rw | number | |

Methods: `spell:addKeyword(kwOrId)`, `spell:removeKeyword(kwOrId)` → bool.

## MagicEffect (extends Form)

| Property | R/W | Type | Notes |
|---|---|---|---|
| `baseCost` | rw | number | |
| `minimumSkill` / `spellmakingArea` | rw | integer | |
| `spellmakingChargeTime` / `taperWeight` / `taperCurve` / `skillUsageMult` | rw | number | |
| `associatedSkill` | rw | string | `"Alteration"`, `"Conjuration"`, `"Destruction"`, `"Illusion"`, `"Restoration"`, `"Enchanting"`, `"None"` |
| `resistVariable` | ro | string | Same value set as `associatedSkill` |
| `castingType` / `delivery` | rw | string | Same value sets as Spell |
| `archetype` | ro | string | `"ValueModifier"`, `"Script"`, `"Light"`, ... |
| `isHostile` / `isDetrimental` | ro | bool | |

Methods: `mgef:addKeyword(kwOrId)`, `mgef:removeKeyword(kwOrId)` → bool.

## Script layout

```
Data/SKSE/Plugins/LuaPatcher/Scripts/
  MyMod.lua              # executed at load
  MyMod_config.lua       # NOT executed; loaded via lua_patcher.tryLoadConfig("MyMod")
```

Config chunks return a table; `tryLoadConfig` returns it (or nil when the file
is missing or fails to load). Scripts typically merge it over defaults:

```lua
local CONFIG = { enabled = true, maxLevel = 50 }

local loaded = lua_patcher.tryLoadConfig("MyMod")
if loaded then
    for k, v in pairs(loaded) do CONFIG[k] = v end
end
```