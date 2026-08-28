# Contributing

## Code style

This project follows the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). Google and LLVM style guides are not used — both prohibit exceptions, which CommonLibSSE-NG relies on.

### Key rules

**Null checks** — use `if (!ptr)` / `if (ptr)`, never `== nullptr` or `!= nullptr` (ES.87).

**Memory** — no naked `new`/`delete` (R.11). No raw owning pointers (R.3). Use `std::unique_ptr` / `std::shared_ptr`.

**Control flow** — always use braces, even for single-statement bodies.

**Types and algorithms** — prefer `std::` types and algorithms over hand-rolled equivalents.

**Return values** — always check them. If a function returns `bool` or an error code, check it.

**Precompiled header** — `PCH.h` must be the first include in plugin-only `.cpp` files (`src/`). Sources compiled in both the plugin and native test targets (e.g. `SharedUtils.cpp`), and test-only sources (`tests/`), must not include `PCH.h`.

**Comments** — only where a non-obvious decision needs a reason. No filler, no redundant restatements of the code.

## Testing

The plugin targets Windows/Skyrim, so the Lua bindings cannot run in the game during
development. Two automated test targets exist:

- **`test/harness` (Linux)** — compiles the real binding sources
  (`src/LuaApi.cpp`, `src/LeveledList.cpp`, `src/Equipment.cpp`) against mocked RE
  types (`test/harness/mocks`), system Lua and sol2, then asserts the API behavior
  and runs the shipped example scripts. `Magic.cpp` is not included: `SpellItem`/
  `EffectSetting` are too complex to mock.
- **`test/ExampleTests.cpp` (Windows, `PLUGIN_TESTS_ONLY=ON`, Catch2)** — pure-logic
  helpers from `Utils.h` that don't touch RE/SKSE.

Build and run the harness from the repository root:

```sh
cmake -S test/harness -B build/harness
cmake --build build/harness
ctest --test-dir build/harness --output-on-failure
```

The harness force-includes `src/PCH.h` (`-include`, mirroring the plugin's
`target_precompile_headers`) and requires `fmt` + `lua5.5` dev packages
(`pkg-config`). The mocks must stay in sync with the CommonLibSSE API surface the
bindings use — when a binding starts calling a new RE method, mirror it in
`test/harness/mocks/RE/Skyrim.h` or the harness build breaks.
