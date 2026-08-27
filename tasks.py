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

# Deployed mod layout: <SKYRIM_MODS_FOLDER>/LuaPatcher/SKSE/Plugins/
MOD_FOLDER_NAME = "LuaPatcher"
SCRIPTS_RELATIVE = Path("SKSE/Plugins/LuaPatcher/Scripts")
CONFIG_RELATIVE = Path("SKSE/Plugins/LuaPatcher/Config")

# Layout:
#   examples/
#     EquipmentInjection/
#       EquipmentInjection.lua
#       EquipmentInjection_Config.lua
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


def _find_example_script(name: str) -> Path | None:
    cand = EXAMPLES_ROOT / name / f"{name}.lua"
    if cand.is_file():
        return cand
    dir_ = EXAMPLES_ROOT / name
    if dir_.is_dir():
        luas = sorted(dir_.glob("*.lua"))
        # prefer non-config lua
        for f in luas:
            if not f.name.endswith("_Config.lua"):
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
        EXAMPLES_ROOT / name / f"{name}_Config.lua",
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
        cfg_dir = Path(mods_folder) / MOD_FOLDER_NAME / CONFIG_RELATIVE
        cfg_dir.mkdir(parents=True, exist_ok=True)
        # repo: <Name>_Config.lua -> deployed: <Name>.lua (what tryLoadConfig expects)
        cfg_dst = cfg_dir / f"{example}.lua"
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

    ver = version.strip() if version else ""
    if not ver:
        # try git describe --tags --exact-match
        try:
            proc = subprocess.run(
                ["git", "describe", "--tags", "--exact-match"],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                check=False,
            )
            if proc.returncode == 0:
                git_tag = proc.stdout.strip()
                ver = git_tag[1:] if git_tag.startswith("v") else git_tag
            else:
                # fallback to CMakeLists.txt
                cmake_txt = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
                m = re.search(r"^\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake_txt, re.MULTILINE)
                if m:
                    ver = m.group(1)
        except FileNotFoundError:
            # git not installed, try CMakeLists fallback
            cmake_txt = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
            m = re.search(r"^\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake_txt, re.MULTILINE)
            if m:
                ver = m.group(1)

        if not ver:
            raise Exit("could not determine version from CMakeLists.txt")

    # Validate version
    if not re.match(r"^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$", ver):
        raise Exit(f"invalid version '{ver}' — expected X.Y.Z or X.Y.Z-pre or X.Y.Z+build")

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
