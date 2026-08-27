"""LuaPatcher development tasks (invoke).

Requires: pip install invoke

Usage:
    invoke deploy                     build + deploy the DLL only
    invoke deploy --copy-example      also (re)copy the default EquipmentInjection.lua
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
DEFAULT_EQUIPMENT_SCRIPT = ROOT / "examples" / "EquipmentInjection.lua"


def _load_env() -> None:
    """Source .env into the process environment (mirrors deploy.sh)."""
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


@task
def deploy(c: Context, copy_example: bool = False):
    """Build and deploy the mod to your mod manager's staging folder.

    Requires SKYRIM_MODS_FOLDER to be set in .env or the environment.

    With --copy-example, the default examples/EquipmentInjection.lua is
    (re)copied into the deployed mod's Scripts folder, overwriting any
    existing copy there. Without the flag the Scripts folder is left alone,
    so scripts you have customized in the mod folder are never clobbered.
    """
    mods_folder = _mods_folder(c)

    print(f"Building and deploying to: {mods_folder}")
    with c.cd(str(ROOT)):
        c.run("cmake --workflow --preset deploy", echo=True)

    if copy_example:
        scripts_dir = Path(mods_folder) / MOD_FOLDER_NAME / SCRIPTS_RELATIVE
        scripts_dir.mkdir(parents=True, exist_ok=True)
        destination = scripts_dir / DEFAULT_EQUIPMENT_SCRIPT.name
        shutil.copy2(DEFAULT_EQUIPMENT_SCRIPT, destination)
        print(f"Copied default example to: {destination}")

    print("\nDeploy complete")