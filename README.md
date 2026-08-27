# LuaPatcher

A SKSE plugin.

LuaPatcher is similar to SkyPatcher, allowing you to patch Forms at runtime, but
using Lua.

AI Disclosure: AI was used to code this mod.

Credits:

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)
- [SkyPatcher](https://github.com/Zzyxz/SkyPatcher)
- [CommonLibSSE-NG-template](https://github.com/codepuncher/CommonLibSSE-NG-template)

## Docs

Check examples/

## Development

### Prerequisites

#### All platforms

- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/download/) 3.21+
- [vcpkg](https://vcpkg.io/en/getting-started) — set `VCPKG_ROOT` in your
  environment

#### Linux

- LLVM/Clang (provides `clang-cl`, `lld-link`, `llvm-lib`, `llvm-rc`, `llvm-mt`)
- [xwin](https://github.com/Jake-Shadle/xwin) — downloads the real Windows SDK
  and MSVC CRT headers/libs
- [Ninja](https://ninja-build.org/)

```bash
# Arch / CachyOS
sudo pacman -S clang lld llvm ninja

# Install xwin (requires Rust/cargo)
cargo install xwin

# Fetch Windows SDK + MSVC CRT headers to ~/.xwin  (one-time, ~700 MB)
xwin splat --output ~/.xwin
```

> **Note:** On first configure, `cmake/toolchains/clang-cl-cross.cmake` creates
> TitleCase symlinks inside your xwin installation, e.g.:
>
> ```
> ~/.xwin/sdk/lib/um/x86_64/Advapi32.lib  ->  advapi32.lib
> ```
>
> lld-link is case-sensitive but CommonLibSSE-NG references libs with mixed-case
> names. The originals are untouched.

#### Configure deploy path

Copy `.env.example` to `.env` if you don't have one yet (the init script does
this for you), then set `SKYRIM_MODS_FOLDER` to your mod manager's staging
folder:

```bash
# MO2:
SKYRIM_MODS_FOLDER=$HOME/MO2/mods
```

#### Build

```bash
./scripts/build.sh
# or directly:
cmake --preset release-linux
cmake --build --preset release-linux
```

The DLL lands in `build/release-linux/LuaPatcher.dll`.

##### Deploy to mod manager

```bash
invoke deploy (optional: --copy-example)

# or directly
source .env && cmake --workflow --preset deploy
```

This configures, builds, and copies `LuaPatcher.dll` + `LuaPatcher.pdb` directly
into:

```
$SKYRIM_MODS_FOLDER/LuaPatcher/SKSE/Plugins/
```

```bash
invoke deploy (optional: --copy-example)
```

#### Updating CommonLibSSE-NG

```bash
git submodule update --remote lib/commonlibsse-ng
git add lib/commonlibsse-ng
git commit -m "chore: update CommonLibSSE-NG submodule"
```
