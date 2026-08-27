# Examples

Monorepo layout: each distributable mod lives in its own folder and can be released independently. Snippets are reference-only.

```
examples/
  EquipmentInjection/               # distributable — can be zipped via `invoke package-example`
    EquipmentInjection.lua          # → Data/SKSE/Plugins/LuaPatcher/Scripts/EquipmentInjection.lua
    EquipmentInjection_Config.lua   # → Data/SKSE/Plugins/LuaPatcher/Config/EquipmentInjection.lua (copy & rename)
    README.md
  snippets/                         # NOT distributable — copy-paste reference
    LeveledLists.lua
    README.md
```

Naming convention: each distributable folder contains `<ModName>.lua` + `<ModName>_Config.lua` so flat `Config/` can hold many mods without collision.

## Conventions

- **Distributable:** `examples/<Name>/` where `<Name>` matches the main `<Name>.lua` script (e.g. `EquipmentInjection/EquipmentInjection.lua`). `invoke package-example --name <Name>` zips it as `SKSE/Plugins/LuaPatcher/Scripts/...`.
- **Snippets:** `examples/snippets/` (or any folder in `SNIPPETS_DIRS` like `mini_example`, `_snippets`) is excluded from packaging. Keep small demos and API cheat-sheets there.
- User config always lives **outside** the mod (`Data/SKSE/Plugins/LuaPatcher/Config/<Name>.lua`) so updates don't clobber it; the repo only ships `Config.example.lua`.

## Deploy

```
# DLL + your chosen example
invoke deploy --copy-example --example EquipmentInjection

# List what can be packaged
invoke list-examples

# Zip a distributable example
invoke package-example --name EquipmentInjection --out dist/
```
