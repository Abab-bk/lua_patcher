"""LuaPatcher development tasks (invoke).

Requires: pip install invoke

Usage:
    invoke build                             configure + build (release-linux)
    
    invoke deploy                            build + deploy the DLL only
    invoke deploy --copy-example             also copy default example (EquipmentInjection)
    invoke deploy --copy-all-examples        also copy every distributable example
    invoke deploy --example NAME             choose which single example to copy (default: EquipmentInjection)
    
    invoke format                            format C++ sources with clang-format
    
    invoke package                           build (if needed) + create distributable zip
    invoke package --build-dir DIR           use existing build dir instead of building
    invoke package --version X.Y.Z           override version

    invoke package-example                   package EquipmentInjection as standalone mod (alias: packageExample)
    invoke package-example --example NAME    package any distributable example
    invoke package-example --version X.Y.Z   override version
    invoke package-example --out DIR         override output dir/file (default: dist/)

    invoke list-examples                     list distributable examples
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from invoke import Context, Exit, task

ROOT = Path(__file__).resolve().parent

# Deployed mod layout: <SKYRIM_MODS_FOLDER>/LuaPatcher/SKSE/Plugins/LuaPatcher/Scripts/
# Flat sibling per examples/ : <Name>.lua + <Name>_config.lua (same directory, no Config/ split)
# Legacy Config/ is kept only as fallback in the plugin for old installs.
MOD_FOLDER_NAME = "LuaPatcher"
SCRIPTS_RELATIVE = Path("SKSE/Plugins/LuaPatcher/Scripts")
# Legacy: Data/SKSE/Plugins/LuaPatcher/Config/<Name>.lua  (renamed from <Name>_config.lua)
CONFIG_RELATIVE = Path("SKSE/Plugins/LuaPatcher/Config")

# Layout:
#   examples/
#     EquipmentInjection/
#       EquipmentInjection.lua
#       EquipmentInjection_config.lua
#     snippets/   -> NOT distributable, reference only
EXAMPLES_ROOT = ROOT / "examples"
DEFAULT_EXAMPLE = "EquipmentInjection"


def _load_env() -> None:
    env_file = ROOT / ".env"
    if not env_file.is_file():
        return
    for line in env_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if match := re.match(r"export\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)", line):
            key, value = match.groups()
            os.environ[key] = value.strip().strip("\"'")


def _ensure_root() -> None:
    if not (ROOT / "CMakeLists.txt").is_file():
        raise Exit("must be run from the project root (e.g. invoke build)")


def _mods_folder(c: Context) -> str:
    _ensure_root()
    _load_env()
    mods_folder = os.environ.get("SKYRIM_MODS_FOLDER", "")
    if not mods_folder:
        raise Exit(
            "SKYRIM_MODS_FOLDER is not set.\n"
            "  Set it in .env or export it before running this task."
        )
    return mods_folder


def _is_distributable(p: Path) -> bool:
    return p.is_dir() and p.name != "snippets" and not p.name.startswith(".")  # only snippets is excluded


def _list_distributable_examples() -> list[Path]:
    if not EXAMPLES_ROOT.is_dir():
        return []
    return sorted([p for p in EXAMPLES_ROOT.iterdir() if _is_distributable(p)])


def _resolve_version(version: str = "") -> str:
    """Resolve version from explicit string, git tag, or CMakeLists.txt."""
    ver = version.strip() if version else ""
    if ver:
        if not re.match(r"^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$", ver):
            raise Exit(f"invalid version '{ver}' — expected X.Y.Z or X.Y.Z-pre or X.Y.Z+build")
        return ver

    cmake_txt = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"^\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake_txt, re.MULTILINE)
    
    if m:
        ver = m.group(1)

    if not ver:
        raise Exit("could not determine version from CMakeLists.txt")

    if not re.match(r"^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$", ver):
        raise Exit(f"invalid version '{ver}' — expected X.Y.Z or X.Y.Z-pre or X.Y.Z+build")
    return ver


def _find_example_script(name: str) -> Path | None:
    cand = EXAMPLES_ROOT / name / f"{name}.lua"
    if cand.is_file():
        return cand
    dir_ = EXAMPLES_ROOT / name
    if dir_.is_dir():
        luas = sorted(dir_.glob("*.lua"))
        # prefer non-config lua
        for f in luas:
            if not f.name.endswith("_config.lua"):
                return f
        if luas:
            return luas[0]
    legacy = EXAMPLES_ROOT / f"{name}.lua"
    if legacy.is_file():
        return legacy
    for p in EXAMPLES_ROOT.rglob(f"{name}.lua"):
        if p.is_file():
            return p
    return None


def _find_example_config(name: str) -> Path | None:
    for cand in [
        EXAMPLES_ROOT / name / f"{name}_config.lua",
        EXAMPLES_ROOT / name / "Config.lua",
    ]:
        if cand.is_file():
            return cand
    return None


def _copy_single_example(example: str, mods_folder: str) -> None:
    if example == "snippets":
        raise Exit("snippets is not a distributable example")

    src = _find_example_script(example)
    if not src or not src.is_file():
        raise Exit(f"Example script for '{example}' not found. Looked in examples/{example}/")

    scripts_dir = Path(mods_folder) / MOD_FOLDER_NAME / SCRIPTS_RELATIVE
    scripts_dir.mkdir(parents=True, exist_ok=True)
    dst = scripts_dir / src.name
    shutil.copy2(src, dst)
    print(f"Copied example '{example}' -> {dst}")

    cfg_src = _find_example_config(example)
    if cfg_src:
        # Flat sibling: Scripts/<Name>.lua + Scripts/<Name>_config.lua (same directory, matches examples/)
        cfg_dst = scripts_dir / cfg_src.name
        shutil.copy2(cfg_src, cfg_dst)
        print(f"Copied config {cfg_src.name} -> {cfg_dst}")


@task
def build(c: Context):
    """Configure and build the mod for Linux (clang-cl cross-compilation)."""
    _ensure_root()
    _load_env()

    print("Configuring...")
    with c.cd(str(ROOT)):
        c.run("cmake --preset release-linux", echo=True)

    print("Building...")
    with c.cd(str(ROOT)):
        c.run("cmake --build --preset release-linux", echo=True)

    print("\nBuild complete")


@task
def format(c: Context):
    """Format C++ sources with clang-format."""
    fmt = "clang-format"

    # Find src files like: find src \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 "$FMT" -i
    src_dir = ROOT / "src"
    if not src_dir.is_dir():
        print("No src/ directory found, nothing to format.")
        return

    files: list[Path] = []
    for ext in ("*.cpp", "*.h"):
        files.extend(src_dir.rglob(ext))

    if not files:
        print("No C++ sources found in src/.")
        return

    # Run formatter in-place
    for f in sorted(files):
        # Use c.run to keep invoke semantics, but fall back to subprocess
        result = subprocess.run([fmt, "-i", str(f)], capture_output=True, text=True)
        if result.returncode != 0:
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            raise Exit(f"{fmt} failed on {f}", code=result.returncode)

    print(f"Formatted {len(files)} file(s) with {fmt}")


@task
def package(c: Context, build_dir: str = "", version: str = ""):
    """Build (optional) and create a distributable zip for NexusMods/mod managers.

    --build-dir DIR  skip cmake build; search DIR for the DLL instead of build/release-linux
    --version X.Y.Z  optional, e.g. 1.0.0 (default: exact git tag or CMakeLists.txt version)
    """
    _ensure_root()

    if not shutil.which("7z"):
        raise Exit("7z is not installed")

    ver = _resolve_version(version)

    # Determine build_dir
    build_dir_path: Path
    if not build_dir:
        print("Building...")
        with c.cd(str(ROOT)):
            c.run("cmake --preset release-linux", echo=True)
            c.run("cmake --build --preset release-linux", echo=True)
        build_dir_path = ROOT / "build" / "release-linux"
    else:
        build_dir_path = Path(build_dir)
        if not build_dir_path.is_absolute():
            build_dir_path = ROOT / build_dir_path

    if not build_dir_path.is_dir():
        raise Exit(f"build dir not found: {build_dir_path}")

    # Determine project DLL name from CMakeLists.txt OUTPUT_NAME
    cmake_txt = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"PROPERTIES\s+OUTPUT_NAME\s+([\w-]+)", cmake_txt)
    project_name = m.group(1) if m else ""
    if not project_name:
        raise Exit("could not determine DLL name from CMakeLists.txt (OUTPUT_NAME not found)")

    # Find DLL(s) maxdepth 2
    dlls: list[Path] = []
    # depth 1
    for p in build_dir_path.glob(f"{project_name}.dll"):
        if p.is_file():
            dlls.append(p)
    # depth 2
    for p in build_dir_path.glob(f"*/{project_name}.dll"):
        if p.is_file():
            dlls.append(p)

    if len(dlls) != 1:
        raise Exit(f"expected exactly 1 DLL in {build_dir_path}, found {len(dlls)}")

    dll = dlls[0]
    name = dll.stem
    zip_name = ROOT / "dist" / f"{name}-{ver}.zip"
    zip_name.parent.mkdir(parents=True, exist_ok=True)

    tmp = Path(tempfile.mkdtemp())
    try:
        plugins_dir = tmp / "SKSE" / "Plugins"
        plugins_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(dll, plugins_dir / dll.name)

        # Create zip: (cd "$tmp" && 7z a -tzip out.zip SKSE > /dev/null)
        out_zip = tmp / "out.zip"
        result = subprocess.run(
            ["7z", "a", "-tzip", str(out_zip), "SKSE"],
            cwd=str(tmp),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            raise Exit(f"7z failed: {result.stderr}")

        shutil.move(str(out_zip), str(zip_name))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print(f"\nPackage: {zip_name}")

    github_output = os.environ.get("GITHUB_OUTPUT", "")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as f:
            f.write(f"zip={zip_name}\n")


@task(aliases=["packageExample"])
def package_example(
    c: Context,
    example: str = DEFAULT_EXAMPLE,
    version: str = "",
    out: str = "",
):
    """Create a distributable zip for a single example mod (default: EquipmentInjection).

    Packages examples/<Name>/ as a standalone mod with flat sibling layout:
        SKSE/Plugins/LuaPatcher/Scripts/<Name>.lua
        SKSE/Plugins/LuaPatcher/Scripts/<Name>_config.lua  (same directory, mirrors examples/)

    --example NAME  example folder/name to package (default: EquipmentInjection)
    --version X.Y.Z override version (default: git tag or CMakeLists.txt)
    --out DIR       output dir or file (default: dist/<Name>-<ver>.zip)

    Examples:
        invoke package-example
        invoke package-example --example EquipmentInjection
        invoke package-example --example MagicTweak --version 0.0.1 --out dist/
        invoke packageExample --example WeaponArmorTweak
    """
    _ensure_root()

    if not shutil.which("7z"):
        raise Exit("7z is not installed")

    chosen = example.strip() if example and example.strip() else DEFAULT_EXAMPLE

    if chosen == "snippets" or chosen.startswith("."):
        raise Exit(f"'{chosen}' is not a distributable example")

    # Verify distributable
    example_dir = EXAMPLES_ROOT / chosen
    # Allow legacy flat file case but prefer folder check
    is_dir_example = example_dir.is_dir()
    # Also consider _is_distributable helper for directories
    if is_dir_example and not _is_distributable(example_dir):
        raise Exit(f"'{chosen}' is not a distributable example")
    if not is_dir_example:
        # Check if there's any matching script at all (legacy flat)
        if not _find_example_script(chosen):
            # Provide helpful list
            avail = ", ".join(p.name for p in _list_distributable_examples()) or "(none)"
            raise Exit(f"Example '{chosen}' not found in examples/. Available: {avail}")

    src = _find_example_script(chosen)
    if not src or not src.is_file():
        avail = ", ".join(p.name for p in _list_distributable_examples()) or "(none)"
        raise Exit(f"Example script for '{chosen}' not found. Looked in examples/{chosen}/. Available: {avail}")

    cfg_src = _find_example_config(chosen)

    ver = _resolve_version(version)

    # Determine output zip path
    if out and out.strip():
        out_path = Path(out.strip())
        if not out_path.is_absolute():
            out_path = ROOT / out_path
        # If out is a directory or ends with separator, treat as dir
        if out_path.suffix.lower() == ".zip":
            zip_name = out_path
            zip_name.parent.mkdir(parents=True, exist_ok=True)
        else:
            # Directory
            out_path.mkdir(parents=True, exist_ok=True)
            zip_name = out_path / f"{chosen}-{ver}.zip"
    else:
        zip_name = ROOT / "dist" / f"{chosen}-{ver}.zip"
        zip_name.parent.mkdir(parents=True, exist_ok=True)

    tmp = Path(tempfile.mkdtemp())
    try:
        # Flat sibling structure (mirrors examples/): Scripts/<Name>.lua + Scripts/<Name>_config.lua
        scripts_dir = tmp / SCRIPTS_RELATIVE
        scripts_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, scripts_dir / src.name)
        print(f"Added script: {SCRIPTS_RELATIVE / src.name}")

        if cfg_src:
            cfg_dst = scripts_dir / cfg_src.name
            shutil.copy2(cfg_src, cfg_dst)
            print(f"Added config: {SCRIPTS_RELATIVE / cfg_dst.name} (from {cfg_src.name})")
        else:
            print(f"No config found for '{chosen}' (looked for {chosen}_config.lua), packaging script only")

        # Create zip: (cd "$tmp" && 7z a -tzip out.zip SKSE > /dev/null)
        out_zip = tmp / "out.zip"
        result = subprocess.run(
            ["7z", "a", "-tzip", str(out_zip), "SKSE"],
            cwd=str(tmp),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            raise Exit(f"7z failed: {result.stderr}")

        # Move to final location (handle overwrite)
        if zip_name.exists():
            zip_name.unlink()
        shutil.move(str(out_zip), str(zip_name))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print(f"\nPackageExample: {zip_name}  (example={chosen} version={ver})")

    github_output = os.environ.get("GITHUB_OUTPUT", "")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as f:
            f.write(f"zip_example={zip_name}\n")
            f.write(f"example={chosen}\n")


@task(aliases=["listExamples"])
def list_examples(c: Context):
    """List distributable examples available for packaging/deploy."""
    _ensure_root()
    examples = _list_distributable_examples()
    if not examples:
        print("No distributable examples found in examples/")
        return
    print("Distributable examples:")
    for p in examples:
        script = _find_example_script(p.name)
        cfg = _find_example_config(p.name)
        script_info = script.name if script else "(no .lua found)"
        cfg_info = cfg.name if cfg else "(no config)"
        print(f"  - {p.name}: script={script_info} config={cfg_info}")


@task
def deploy(
    c: Context,
    copy_example: bool = False,
    copy_all_examples: bool = False,
    example: str = DEFAULT_EXAMPLE,
):
    """Build and deploy the mod.

    With --copy-example copy a single example (default: EquipmentInjection,
    use --example NAME to choose).
    With --copy-all-examples copy every distributable example in examples/.
    """
    mods_folder = _mods_folder(c)

    print(f"Building and deploying to: {mods_folder}")
    with c.cd(str(ROOT)):
        c.run("cmake --workflow --preset deploy", echo=True)

    if copy_all_examples:
        copied = 0
        for p in sorted(EXAMPLES_ROOT.iterdir()):
            if _is_distributable(p):
                _copy_single_example(p.name, mods_folder)
                copied += 1
        if copied == 0:
            print("No distributable examples found in examples/")
        print("\nDeploy complete")
        return

    if copy_example:
        _copy_single_example(example, mods_folder)

    print("\nDeploy complete")
