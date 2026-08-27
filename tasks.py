"""LuaPatcher development tasks (invoke).

Requires: pip install invoke

Usage:
    invoke deploy                          build + deploy the DLL only
    invoke deploy --copy-example           also copy default example (EquipmentInjection)
    invoke deploy --copy-all-examples      also copy every distributable example
    invoke deploy --example NAME           choose which single example to copy (default: EquipmentInjection)
"""

from __future__ import annotations

import os
import re
import shutil
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


def _mods_folder(c: Context) -> str:
    if not (ROOT / "CMakeLists.txt").is_file():
        raise Exit("must be run from the project root (e.g. invoke deploy)")
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
