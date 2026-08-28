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
| `log(...)` | — | Joins arguments with tabs (tostring on each) into the plugin log |
| `warn(...)` | — | Like log, at warn level |
| `error(...)` | — | Like log, at error level |
| `getForm(identifier)` | Form or nil | Resolves `"Mod.esm\|000123"` / EditorID; never raises |
| `isPluginInstalled(name)` | bool | Is the plugin in the load order |
| `tryLoadConfig(name)` | table or nil | Loads sibling `Scripts/<name>_config.lua`; nil if missing/failed; raises on invalid names |
| `leveledList(formOrId)` | LeveledList | Raises if the form is not a leveled item/character list |
| `allLeveledItems()` | array of LeveledList | Every `TESLevItem` |
| `allLeveledCharacters()` | array of LeveledList | Every `TESLevCharacter` |
| `findLeveledListsContaining(formOrId)` | array of LeveledList | Snapshot reverse index: lists referencing the form in the game's pristine data (patches made this run are not visible) |
| `allWeapons()` | array of Weapon | Every `TESObjectWEAP` |
| `allArmors()` | array of Armor | Every `TESObjectARMO` |
| `allSpells()` | array of Spell | Every `SpellItem` |
| `allMagicEffects()` | array of MagicEffect | Every `EffectSetting` |
| `formList(formOrId)` | FormList | Raises if the form is not a form list |
| `allFormLists()` | array of FormList | Every `BGSListForm` |
| `print(...)` | — | Redirected to the plugin log (same formatting as log) |


## Form (base type)

Weapon, Armor, LeveledList, Spell and MagicEffect objects inherit these.

`tostring(form)` → `"Form[{}\|{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `formId` | ro | integer | Full formID |
| `typeId` | ro | integer | Engine form type |
| `type` | ro | string | e.g. `"Weapon"`, `"Armor"`, `"Spell"`, `"LeveledItem"`, ... |
| `editorId` | ro | string or nil | From the game's editorID table |
| `name` | ro | string or nil | Full name |
| `identifier` | ro | string | `"Plugin.esm\|000123"` form |
| `plugin` | ro | string or nil | Source plugin |
| `value` | ro | integer or nil | If the form has a value |
| `weight` | ro | number or nil | If the form has a weight |
| `enchantment` | ro | Form or nil | The enchantment form |
| `keywords` | ro | array of Form | As form objects |

| Method | Returns | Notes |
|---|---|---|
| `form:hasKeyword(kwOrId)` | bool | |

## Weapon (extends Form)

`tostring(weapon)` → `"Weapon[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `damage` | rw | integer | Clamped to 0..65535 |
| `speed` | rw | number |  |
| `reach` | rw | number |  |
| `stagger` | rw | number |  |
| `critDamage` | rw | integer | Clamped to 0..65535 |
| `weight` | rw | number |  |
| `value` | rw | integer | Non-negative |
| `weaponType` | ro | string | "HandToHandMelee", "OneHandedSword", "OneHandedDagger", "OneHandedAxe", "OneHandedMace", "TwoHandedSword", "TwoHandedAxe", "Bow", "Staff", "Crossbow" |
| `skill` | ro | string | "OneHanded", "TwoHanded", "Marksman", "None" |
| `melee` | ro | bool |  |
| `ranged` | ro | bool |  |
| `bow` | ro | bool |  |
| `staff` | ro | bool |  |
| `crossbow` | ro | bool |  |
| `playable` | ro | bool |  |

| Method | Returns | Notes |
|---|---|---|
| `weapon:addKeyword(kwOrId)` | bool | |
| `weapon:removeKeyword(kwOrId)` | bool | |

## Armor (extends Form)

`tostring(armor)` → `"Armor[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `armorRating` | rw | number | Stored as rating x 100, rounded |
| `armorType` | ro | string | "Light", "Heavy", "Clothing" |
| `slots` | ro | array of string | "Head", "Hair", "Body", "Hands", "Forearms", "Amulet", "Ring", "Feet", "Calves", "Shield", "Tail", "LongHair", "Circlet", "Ears" |
| `playable` | ro | bool |  |
| `weight` | rw | number |  |
| `value` | rw | integer | Non-negative |

| Method | Returns | Notes |
|---|---|---|
| `armor:addKeyword(kwOrId)` | bool | |
| `armor:removeKeyword(kwOrId)` | bool | |

## LeveledList (extends Form)

`tostring(list)` → `"LeveledList[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `numEntries` | ro | integer |  |
| `chanceNone` | rw | integer | Stored as int8 |
| `chanceGlobal` | rw | Form or nil | Must be a TESGlobal |
| `calculateFromAllLevels` | rw | bool | kCalculateFromAllLevelsLTOrEqPCLevel flag |
| `calculateForEachItem` | rw | bool | kCalculateForEachItemInCount flag |
| `useAll` | rw | bool | kUseAll flag |
| `specialLoot` | rw | bool | kSpecialLoot flag |

| Method | Returns | Notes |
|---|---|---|
| `list:entries()` | array of entry snapshots | |
| `list:add(formOrId, level?, count?)` | — | |
| `list:addIfAbsent(formOrId, level?, count?)` | — | |
| `list:remove(formOrId, options?)` | — | |
| `list:removeIf(predicate)` | — | |
| `list:removeByKeyword(kwOrId)` | — | |
| `list:replace(fromOrId, toOrId)` | — | |
| `list:multiplyCount(formOrId, factor)` | — | |
| `list:has(formOrId)` | bool | |
| `list:clear()` | — | |
| `list:sort()` | — | |

Entry snapshots have the shape `{ form = <Form>, count = integer, level = integer }`.

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

`tostring(spell)` → `"Spell[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `costOverride` | rw | integer |  |
| `spellType` | rw | string | "Spell", "Disease", "Power", "LesserPower", "Ability", "Poison" |
| `castingType` | rw | string | "ConstantEffect", "FireAndForget", "Concentration", "Scroll" |
| `delivery` | rw | string | "Self", "Touch", "Aimed", "TargetActor", "TargetLocation" |
| `chargeTime` | rw | number |  |
| `castDuration` | rw | number |  |
| `range` | rw | number |  |

| Method | Returns | Notes |
|---|---|---|
| `spell:addKeyword(kwOrId)` | bool | |
| `spell:removeKeyword(kwOrId)` | bool | |

## MagicEffect (extends Form)

`tostring(mgef)` → `"MagicEffect[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `baseCost` | rw | number |  |
| `minimumSkill` | rw | integer |  |
| `spellmakingArea` | rw | integer |  |
| `spellmakingChargeTime` | rw | number |  |
| `taperWeight` | rw | number |  |
| `taperCurve` | rw | number |  |
| `skillUsageMult` | rw | number |  |
| `associatedSkill` | rw | string | "Alteration", "Conjuration", "Destruction", "Illusion", "Restoration", "Enchanting", "None" |
| `resistVariable` | ro | string | "Alteration", "Conjuration", "Destruction", "Illusion", "Restoration", "Enchanting", "None" |
| `castingType` | rw | string | "ConstantEffect", "FireAndForget", "Concentration", "Scroll" |
| `delivery` | rw | string | "Self", "Touch", "Aimed", "TargetActor", "TargetLocation" |
| `archetype` | ro | string | "ValueModifier", "Script", "Dispel", "CureDisease", "Absorb", "DualValueModifier", "Calm", "Demoralize", "Frenzy", "Disarm", "CommandSummoned", "Invisibility", "Light" |
| `isHostile` | ro | bool |  |
| `isDetrimental` | ro | bool |  |

| Method | Returns | Notes |
|---|---|---|
| `mgef:addKeyword(kwOrId)` | bool | |
| `mgef:removeKeyword(kwOrId)` | bool | |

## FormList (extends Form)

`tostring(fl)` → `"FormList[{:08X}]"`

| Property | R/W | Type | Notes |
|---|---|---|---|
| `numForms` | ro | integer |  |

| Method | Returns | Notes |
|---|---|---|
| `fl:forms()` | array of Form | |
| `fl:add(formOrId)` | bool | |
| `fl:remove(formOrId)` | bool | |
| `fl:has(formOrId)` | bool | |
| `fl:clear()` | — | |

## Script layout

```
Data/SKSE/Plugins/LuaPatcher/Scripts/
  MyMod.lua              # executed at load
  MyMod_config.lua       # NOT executed; loaded via lua_patcher.tryLoadConfig("MyMod")
```

Scripts run in priority order: a `-- priority: N` comment (first 512 bytes of the
file) sets the execution order, lowest first; scripts without a declaration get
priority 0 and run first. Equal priorities fall back to path order. Example
pipeline: rebalance stats (10) -> fix keywords (20) -> join tempering sets (30)
-> inject into leveled lists (40).

Config chunks return a table; `tryLoadConfig` returns it (or nil when the file
is missing or fails to load). Scripts typically merge it over defaults:

```lua
local CONFIG = { enabled = true, maxLevel = 50 }

local loaded = lua_patcher.tryLoadConfig("MyMod")
if loaded then
    for k, v in pairs(loaded) do CONFIG[k] = v end
end
```

<!--
This file is generated by docs/gen_api.py from the sol2 registrations in src/*.cpp.
Do not edit by hand - change the source or the generator, then run `invoke docs`.
-->

