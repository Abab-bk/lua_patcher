# AGENTS.md

SKSE plugin for Skyrim SE/AE: patches Forms at runtime via Lua scripts. C++23, sol2 + Lua 5.5, Windows-only target cross-compiled from Linux with clang-cl + xwin + lld-link (preset `release-linux`).

## Build & verify

All dev commands are `invoke` tasks (see `tasks.py`, the source of truth). Plain cmake works too:

```sh
invoke build            # cmake --preset release-linux && cmake --build --preset release-linux
invoke deploy           # build + copy DLL/PDB to $SKYRIM_MODS_FOLDER/LuaPatcher/SKSE/Plugins
invoke harness          # configure/build/run test/harness
invoke docs             # regenerate docs/API.md from src/*.cpp sol2 registrations
invoke format           # clang-format on src/
```

- The DLL lands at `build/release-linux/LuaPatcher.dll`.
- `SKYRIM_MODS_FOLDER` / `SKYRIM_FOLDER` live in `.env` (gitignored, format `export KEY=value`). `invoke` tasks load it themselves; plain cmake calls need `source .env`.
- First configure creates TitleCase symlinks inside `~/.xwin` (lld-link is case-sensitive; CommonLibSSE-NG references mixed-case libs). Originals untouched.
- `invoke deploy` **wipes the entire `$SKYRIM_MODS_FOLDER/LuaPatcher` folder first**, then builds via the `deploy` workflow preset.
- Packaging needs `7z`: `invoke package`, `invoke package-example`, `invoke package-tools`. Version resolves from CMakeLists.txt or git tag.
- `invoke gen-protection` regenerates `examples/EverythingRandomizer/EverythingRandomizer_protection.lua` from Skyrim plugin files via `dotnet run --project third_party/protectgen`. Only commit the vanilla+CC default (no `--plugins`); modded load orders must not be committed.

## Testing

- **`test/harness` (Linux only)** — compiles the *real* `src/*.cpp` binding sources (all of them, incl. `Magic.cpp` and `ScriptLoader.cpp`) against mocked RE types in `test/harness/mocks/RE`, system `lua5.5` + `fmt` (pkg-config), and a FetchContent sol2 patched for Lua 5.5 via `lib/vcpkg/ports/sol2/lua-5.5.diff`. Runs the shipped example scripts and asserts behavior. Run from repo root (`ctest` uses `WORKING_DIRECTORY` = repo root; the harness opens `examples/...` via relative paths).
- When a binding starts calling a new RE method, mirror it in `test/harness/mocks/RE/Skyrim.h` or the harness build breaks.
- **Windows unit tests** (`PLUGIN_TESTS_ONLY=ON`, preset `test-windows`, vcpkg feature `tests`): pure-logic helpers from `src/Utils.h` only — no RE/SKSE. Cannot run on Linux.
- Both harness and plugin force `SOL_ALL_SAFETIES_ON=1` (sol2 arg type checks default off in release) — keep them in sync.

## Layout & conventions

- `src/` — plugin: `Plugin.cpp` (entry, only needs `SKSEPlugin_Load`), `LuaApi.cpp` (registers the whole Lua API), one `*.cpp` per Form domain (LeveledList, Equipment, Magic, Actors, …), `ScriptLoader.cpp` (loads scripts from `SKSE/Plugins/LuaPatcher/Scripts/`, flat sibling `<Name>.lua` + `<Name>_config.lua`; legacy `Config/` is fallback only).
- `examples/` — Lua scripts: `<Name>/<Name>.lua` + `<Name>_config.lua` per folder; `snippets/` is reference-only and never packaged/deployed. Flat-sibling layout mirrors the deployed mod.
- `docs/API.md` is **generated** by `docs/gen_api.py` from sol2 registration calls; prose lives in the generator. Never edit `docs/API.md` by hand — run `invoke docs`.
- `lib/` — submodules: `commonlibsse-ng` (branch `ng`) and vendored `vcpkg` (manifest mode, `builtin-baseline` pinned). Dependencies come from `vcpkg.json` + overlay ports in `cmake/ports`.
- PCH rule (CONTRIBUTING.md): plugin-only `.cpp` files must include `src/PCH.h` first; headers/sources shared with the harness or the PCH-less test target (e.g. `src/Utils.h`) must **not** include it — the harness force-includes it with `-include`.
- Style is C++ Core Guidelines (not Google/LLVM): `if (!ptr)` never `== nullptr`, no naked `new`/`delete`, always braces, always check return values.
- clangd: `.clangd` adds `--target=x86_64-pc-windows-msvc`; `compile_commands.json` is a symlink to `build/release-linux/compile_commands.json`.