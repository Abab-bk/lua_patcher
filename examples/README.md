# Examples

Monorepo layout: each distributable mod lives in its own folder and can be released independently. Snippets are reference-only.

```
examples/
  GearInjection/                    # distributable — config-driven generic gear injection
    GearInjection.lua               # plugin/EditorID targeting, fixed or balanced levels, filters
    GearInjection_config.lua        # replaces the former EquipmentInjection (superset, same defaults)
  EverythingRandomizer/              # distributable — seeded world shuffle (Dark-Souls style)
    EverythingRandomizer.lua         # type-pooled swaps: leveled lists, FormLists, containers,
                                     # effect slots, shouts, enchantments, encounter zones + jitters
    EverythingRandomizer_config.lua  # seed = layout; change it for a new world
    EverythingRandomizer_protection.lua  # generated: quest-referenced/DOBJ form IDs (see below)
  snippets/                         # NOT distributable — copy-paste reference
    LeveledLists.lua
    README.md
```

Flat sibling layout: script and config are siblings as `ModName.lua` + `ModName_config.lua` in the same `Scripts/` directory (mirrors `examples/`).

Naming convention: each distributable folder contains `<ModName>.lua` + `<ModName>_config.lua` as flat siblings.

## Conventions

- **Distributable:** `examples/<Name>/` where `<Name>` matches the main `<Name>.lua` script (e.g. `GearInjection/GearInjection.lua`). `invoke package-example --example <Name>` zips it as `SKSE/Plugins/LuaPatcher/Scripts/<Name>.lua` + `Scripts/<Name>_config.lua`.
- **Generated data:** `examples/<Name>/<Name>_protection.lua` files (if present) ship as flat siblings too — they are loaded via `lua_patcher.tryLoadConfig("<Name>_protection")`, which requires them to sit next to the script.
- **Snippets:** `examples/snippets/` (or any folder in `SNIPPETS_DIRS` like `mini_example`, `_snippets`) is excluded from packaging. Keep small demos and API cheat-sheets there.
- Config is a flat sibling (`Scripts/<Name>_config.lua`) never executed as a script; `lua_patcher.tryLoadConfig("<Name>")` loads `Scripts/<Name>_config.lua` (legacy `Config/<Name>.lua` still supported as fallback).
- Ordering is by a `-- priority: N` comment in the script header (lowest first, missing = 0); see `docs/API.md` → Script layout. EverythingRandomizer runs at priority 50 (after any user scripts in the 10–40 range).

## Deploy

```
# DLL + your chosen example
invoke deploy --copy-example --example GearInjection

# List what can be packaged
invoke list-examples

# Zip a distributable example
invoke package-example --example GearInjection --out dist/
```
