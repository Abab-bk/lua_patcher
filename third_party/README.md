# third_party — protectgen, the offline protection-dataset generator

`protectgen` reads the Skyrim plugin files (.esm/.esp/.esl) **outside** the
game process and generates the protection dataset shipped with the
EverythingRandomizer example
(`examples/EverythingRandomizer/EverythingRandomizer_protection.lua`).
It is a development tool, not part of the runtime plugin.

## Release manifest

When EverythingRandomizer is published as a standalone mod, this directory is
packaged as a companion tools archive (see `invoke package-tools` in
tasks.py). The release contains:

- `protectgen.exe` — win-x64 **self-contained** single-file binary (no .NET
  runtime install needed), produced by `dotnet publish`
- `protectgen/` — the C# source
- `README.md` — this file
- `LICENSE` — GNU GPL-3.0 (copied from the repository root)

## License

Everything in this directory is GNU GPL-3.0. The tool links
[Mutagen.Bethesda](https://github.com/Mutagen-Modding/Mutagen) (NuGet,
GPL-3.0), so it inherits the same license. GPL-3.0 permits binary
distribution as long as the corresponding source ships alongside (it does, in
the same archive).

## Usage

**Interactive** (recommended for end users): run `protectgen.exe` with no
arguments — on Windows a native folder picker opens for the Skyrim folder, then
one for `plugins.txt` and the MO2 mods folder (cancelling falls back to typing
the path; pressing Enter through the prompts produces the vanilla + Creation
Club dataset).

**Headless**:

```
protectgen <SkyrimDataOrGameFolder> [--ccc <Skyrim.ccc>]
           [--plugins <plugins.txt>] [--mods-dir <MO2 mods folder>]
           [--out <file>] [--json <file>]
```

- `<SkyrimDataOrGameFolder>` — Data directory or the game root (Data/ and
  Skyrim.ccc are auto-detected)
- `--plugins` — MO2-format plugins.txt; includes the modded load order
  (full plugins get sequential indices after the CC files, ESLs map to 0xFE)
- `--mods-dir` — MO2 mods staging folder; plugins are resolved from there
  (usvfs-style: later mods override earlier, files at the mod root or under
  `Data/`), falling back to the Data directory
- `--out` — output Lua file (default: `EverythingRandomizer_protection.lua`
  in the working directory)
- `--json` — additionally dump the raw dataset for debugging

The Lua file is loaded at runtime by EverythingRandomizer.lua via
`lua_patcher.loadLua("EverythingRandomizer_protection.lua")` and returns
`{ protected = { [formId] = true, ... }, info = { [formId] = editorId, ... } }`.

### What the dataset contains

Per plugin file, the tool extracts:

- a record census (type -> count)
- quest alias references (typed `Quest.Aliases` fields) plus a raw
  `ALFR/ALFL/ALFA/ALLS` subrecord scan that recovers references Mutagen's
  typed model drops when the declared link type does not match the target
  (e.g. `ALLS -> TreasBag`, a container)
- default-object (DOBJ) references
- editor IDs for the shuffled record types

The formID mapping follows the fixed vanilla master indices
(Skyrim.esm=0 … Dragonborn.esm=4) with Creation Club files from
`Skyrim.ccc` (ESMs sequential, ESLs in the 0xFE range).

Notes:

- quest script references (the VMAD SCRO tail) are intentionally not included:
  a hand-rolled parser was validated against the quest VMAD layout
  (FragmentCount is u16 and the alias array is variable-length) and the
  extracted references turned out to be parse noise (low base-formIDs), so
  the signal was dropped.
- Skyrim has no "QuestItem" keyword (that is Fallout 4); quest items are
  protected through quest-alias references, which is what the dataset
  captures.

## Building / regenerating

The repository ships a pre-generated dataset from the **vanilla + Creation
Club masters only** (`EverythingRandomizer_protection.lua`, never generated
from a private mod list). Regenerate it with:

```bash
invoke gen-protection --data-dir /path/to/Skyrim
```

(`--data-dir` defaults to `$SKYRIM_FOLDER` from `.env`.) For a personal
modded load order:

```bash
invoke gen-protection --plugins /path/to/plugins.txt --mods-dir /path/to/mods
```

— but keep the result out of the repository. Or run the tool directly:

```bash
cd third_party/protectgen
dotnet run -- /path/to/Skyrim
```