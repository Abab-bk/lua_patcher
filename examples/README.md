# Examples

Monorepo layout: each distributable mod lives in its own folder and can be released independently. Snippets are reference-only.

```
examples/
  EquipmentInjection/               # distributable — can be zipped via `invoke package-example`
    EquipmentInjection.lua          # → Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection.lua
    EquipmentInjection_Config.lua   # → Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection_Config.lua (sibling, not executed)
    README.md
  snippets/                         # NOT distributable — copy-paste reference
    LeveledLists.lua
    README.md
```

Flat sibling layout: script and config are siblings as `ModName.lua` + `ModName_Config.lua` in the same `Scripts/` directory (mirrors `examples/`).

Naming convention: each distributable folder contains `<ModName>.lua` + `<ModName>_Config.lua` as flat siblings.

## Conventions

- **Distributable:** `examples/<Name>/` where `<Name>` matches the main `<Name>.lua` script (e.g. `EquipmentInjection/EquipmentInjection.lua`). `invoke package-example --example <Name>` zips it as `SKSE/Plugins/LuaPatcher/Scripts/<Name>.lua` + `Scripts/<Name>_Config.lua`.
- **Snippets:** `examples/snippets/` (or any folder in `SNIPPETS_DIRS` like `mini_example`, `_snippets`) is excluded from packaging. Keep small demos and API cheat-sheets there.
- Config is a flat sibling (`Scripts/<Name>_Config.lua`) never executed as a script; `lua_patcher.tryLoadConfig("<Name>")` loads `Scripts/<Name>_Config.lua` (legacy `Config/<Name>.lua` still supported as fallback).

## Deploy

```
# DLL + your chosen example
invoke deploy --copy-example --example EquipmentInjection

# List what can be packaged
invoke list-examples

# Zip a distributable example
invoke package-example --example EquipmentInjection --out dist/
```
