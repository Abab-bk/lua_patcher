# Snippets — Reference Only (Not Distributable)

Small, self-contained Lua snippets for the LuaPatcher API. These are **not** standalone mods and are not packaged for Nexus – they are documentation / copy-paste references.

- `LeveledLists.lua` — full cheat-sheet: `getForm`, `isPluginInstalled`, `allLeveledItems`, `leveledList`, `add/addOnce/remove/replace/multiplyCount`, flags, `entries()` inspection and bulk patching. Guarded by `isPluginInstalled("LuaPatcherExample.esp")` so it's a no-op without that plugin.

Add your own `*.lua` here for API demos; keep distributable, release-ready mods under `examples/<ModName>/` instead.

Run manually by copying to `Data/SKSE/Plugins/LuaPatcher/Scripts/` for testing.
