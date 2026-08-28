# Examples

Monorepo layout: each distributable mod lives in its own folder and can be released independently. Snippets are reference-only.

```
examples/
  GearInjection/                    # distributable — config-driven generic gear injection
    GearInjection.lua               # plugin/EditorID targeting, fixed or balanced levels, filters
    GearInjection_config.lua        # replaces the former EquipmentInjection (superset, same defaults)
  KeywordFixer/                     # distributable — adds missing material/slot keywords to mod gear
    KeywordFixer.lua                # stat-derived material tiers + declarative add/remove rules
    KeywordFixer_config.lua
  TemperingLists/                   # distributable — joins mod gear to its smithing material set
    TemperingLists.lua              # FormList API: material keyword -> WeapMaterialXSet/ArmorMaterialXSet
    TemperingLists_config.lua
  snippets/                         # NOT distributable — copy-paste reference
    LeveledLists.lua
    README.md
```

Flat sibling layout: script and config are siblings as `ModName.lua` + `ModName_config.lua` in the same `Scripts/` directory (mirrors `examples/`).

Naming convention: each distributable folder contains `<ModName>.lua` + `<ModName>_config.lua` as flat siblings.

## Conventions

- **Distributable:** `examples/<Name>/` where `<Name>` matches the main `<Name>.lua` script (e.g. `GearInjection/GearInjection.lua`). `invoke package-example --example <Name>` zips it as `SKSE/Plugins/LuaPatcher/Scripts/<Name>.lua` + `Scripts/<Name>_config.lua`.
- **Snippets:** `examples/snippets/` (or any folder in `SNIPPETS_DIRS` like `mini_example`, `_snippets`) is excluded from packaging. Keep small demos and API cheat-sheets there.
- Config is a flat sibling (`Scripts/<Name>_config.lua`) never executed as a script; `lua_patcher.tryLoadConfig("<Name>")` loads `Scripts/<Name>_config.lua` (legacy `Config/<Name>.lua` still supported as fallback).
- Ordering is by a `-- priority: N` comment in the script header (lowest first, missing = 0); see `docs/API.md` → Script layout. The gear pipeline is: WeaponArmorTweak 10 → KeywordFixer 20 → TemperingLists 30 → GearInjection 40.

## Deploy

```
# DLL + your chosen example
invoke deploy --copy-example --example GearInjection

# List what can be packaged
invoke list-examples

# Zip a distributable example
invoke package-example --example GearInjection --out dist/
```
