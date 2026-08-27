# EquipmentInjection – Distributable Example

Fully automatic equipment injection for LuaPatcher (monorepo example).

This folder is a **standalone mod** in the monorepo. It can be zipped and published independently:

```
examples/EquipmentInjection/  ->  EquipmentInjection.zip
```

## Contents

- `EquipmentInjection.lua` — main script (goes to `Data/SKSE/Plugins/LuaPatcher/Scripts/`)
  Scans every weapon/armor, injects unassigned non-vanilla gear into `LItem*` leveled lists with balanced level (`keyword → level` + `rating/DPS` fallback).
- `EquipmentInjection_Config.lua` — user config template (follows `<ModName>_Config.lua` convention so multiple mods can have configs side-by-side)
  Copy to `Data/SKSE/Plugins/LuaPatcher/Config/EquipmentInjection.lua` and edit.
  Controls `balanced`, `bottomLevel/topLevel`, `targetPrefixes`, `enableArmor/Weapon`. Missing file → defaults (`balanced=true`).

## Install (as standalone mod)

**Mod manager:**
1. Zip this folder via `invoke package-example --name EquipmentInjection`.
2. Install the zip via MO2/Vortex.

**Manual:**
- Copy `EquipmentInjection.lua` → `Data/SKSE/Plugins/LuaPatcher/Scripts/`
- (Optional) Copy `EquipmentInjection_Config.lua` → `Data/SKSE/Plugins/LuaPatcher/Config/EquipmentInjection.lua` and edit.

Config is stored **outside** the mod (in `Config/`), so mod updates never overwrite it.

## Development

Deploy via:

```
invoke deploy --copy-example --example EquipmentInjection
invoke package-example --name EquipmentInjection --out dist/
```

See top-level `README.md` and `tasks.py`.
