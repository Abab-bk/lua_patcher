#!/usr/bin/env python3
"""Generate docs/API.md from the sol2 usertype registrations in src/*.cpp.

The API surface (type names, properties, read/write kind, method signatures,
value sets) is parsed out of the registration calls:
  - a_lua.new_usertype<T>("Name", ..., "key", sol::property(...), ...)
  - a_type["key"] = sol::property(...) / lambda            (LeveledList split registrations)
  - patcher["key"] = &Function / "string"

Prose sections and a few explanatory notes are maintained in this file; the
tables always match the code.

Usage:
    python3 docs/gen_api.py     # or: invoke docs
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "src"
OUT_PATH = ROOT / "docs" / "API.md"

# Section order in the generated document.
TYPE_ORDER = [
    "Form",
    "Weapon",
    "Armor",
    "LeveledList",
    "Spell",
    "MagicEffect",
    "Enchantment",
    "Ingredient",
    "Potion",
    "Container",
    "Actor",
    "Shout",
    "Light",
    "Global",
    "FormList",
]

# Method receiver shown in signatures, per type.
RECEIVERS = {
    "Form": "form",
    "Weapon": "weapon",
    "Armor": "armor",
    "LeveledList": "list",
    "Spell": "spell",
    "MagicEffect": "mgef",
    "Enchantment": "enchantment",
    "Ingredient": "ingredient",
    "Potion": "potion",
    "Container": "container",
    "Actor": "actor",
    "Shout": "shout",
    "Light": "light",
    "Global": "global",
    "FormList": "fl",
}

# Hand-maintained one-liners for the lua_patcher module functions.
MODULE_NOTES = {
    "version": "Plugin API version",
    "log": "Joins arguments with tabs (tostring on each) into the plugin log",
    "warn": "Like log, at warn level",
    "error": "Like log, at error level",
    "getForm": "Resolves a form reference; never raises",
    "isPluginInstalled": "Is the plugin in the load order",
    "tryLoadConfig": "Loads sibling `Scripts/<name>_config.lua`; nil if missing/failed; raises on invalid names",
    "isQuestReferenced": "True when any loaded quest alias references the form (forced refs, created objects, unique actors); protects quest gear/NPCs from randomizers",
    "leveledList": "Raises if the form is not a leveled item/character/spell list",
    "allLeveledItems": "Every `TESLevItem`",
    "allLeveledCharacters": "Every `TESLevCharacter`",
    "allLeveledSpells": "Every `TESLevSpell`",
    "allWeapons": "Every `TESObjectWEAP`",
    "allArmors": "Every `TESObjectARMO`",
    "allSpells": "Every `SpellItem`",
    "allMagicEffects": "Every `EffectSetting`",
    "allIngredients": "Every `IngredientItem`",
    "allPotions": "Every `AlchemyItem` (potions, poisons and food)",
    "allEnchantments": "Every `EnchantmentItem`",
    "allContainers": "Every `TESObjectCONT`",
    "allActors": "Every `TESNPC` (NPC_ records; covers creatures)",
    "allGlobals": "Every `TESGlobal`",
    "allShouts": "Every `TESShout`",
    "allLights": "Every `TESObjectLIGH`",
    "allEncounterZones": "Every `BGSEncounterZone` (ENCOUNTER_ZONE records)",
    "findLeveledListsContaining": "Snapshot reverse index: lists referencing the form in the game's pristine data (patches made this run are not visible)",
    "formList": "Raises if the form is not a form list",
    "allFormLists": "Every `BGSListForm`",
}

# Hand-maintained argument lists for functions registered as variadic lambdas
# (form references accept several shapes; see the Conventions section).
MODULE_ARGS = {
    "getForm": "formRef",
    "leveledList": "formRef",
    "formList": "formRef",
    "findLeveledListsContaining": "formRef",
    "isQuestReferenced": "formRef",
}

# Hand-maintained argument lists for usertype methods (variadic form refs).
METHOD_ARGS = {
    "Form.hasKeyword": "kwRef",
    "Weapon.addKeyword": "kwRef",
    "Weapon.removeKeyword": "kwRef",
    "Armor.addKeyword": "kwRef",
    "Armor.removeKeyword": "kwRef",
    "Spell.addKeyword": "kwRef",
    "Spell.removeKeyword": "kwRef",
    "MagicEffect.addKeyword": "kwRef",
    "MagicEffect.removeKeyword": "kwRef",
    "LeveledList.add": "formRef, level?, count?",
    "LeveledList.addIfAbsent": "formRef, level?, count?",
    "LeveledList.remove": "formRef, options?",
    "LeveledList.removeByKeyword": "kwRef",
    "LeveledList.replace": "fromRef, toRef",
    "LeveledList.multiplyCount": "formRef, factor",
    "LeveledList.has": "formRef",
    "FormList.add": "formRef",
    "FormList.remove": "formRef",
    "FormList.has": "formRef",
    "Container.addItem": "formRef, count?",
    "Container.removeItem": "formRef",
    "Container.has": "formRef",
    "Ingredient.addEffect": "baseRef, options?",
    "Potion.addEffect": "baseRef, options?",
    "Enchantment.addEffect": "baseRef, options?",
}

# Hand-maintained return values for the lua_patcher module functions.
MODULE_RETURNS = {
    "getForm": "Form or nil",
    "isPluginInstalled": "bool",
    "tryLoadConfig": "table or nil",
    "isQuestReferenced": "bool",
    "leveledList": "LeveledList",
    "allLeveledItems": "array of LeveledList",
    "allLeveledCharacters": "array of LeveledList",
    "allLeveledSpells": "array of LeveledList",
    "allWeapons": "array of Weapon",
    "allArmors": "array of Armor",
    "allSpells": "array of Spell",
    "allMagicEffects": "array of MagicEffect",
    "allIngredients": "array of Ingredient",
    "allPotions": "array of Potion",
    "allEnchantments": "array of Enchantment",
    "allContainers": "array of Container",
    "allActors": "array of Actor",
    "allGlobals": "array of Global",
    "allShouts": "array of Shout",
    "allLights": "array of Light",
    "allEncounterZones": "array of EncounterZone",
    "findLeveledListsContaining": "array of LeveledList",
    "formList": "FormList",
    "allFormLists": "array of FormList",
}

# Hand-maintained return values for usertype methods (keyed by "Type.member").
METHOD_RETURNS = {
    "Form.hasKeyword": "bool",
    "Weapon.addKeyword": "bool",
    "Weapon.removeKeyword": "bool",
    "Armor.addKeyword": "bool",
    "Armor.removeKeyword": "bool",
    "LeveledList.has": "bool",
    "LeveledList.entries": "array of entry snapshots",
    "Spell.addKeyword": "bool",
    "Spell.removeKeyword": "bool",
    "MagicEffect.addKeyword": "bool",
    "MagicEffect.removeKeyword": "bool",
    "Ingredient.setEffects": "—",
    "Ingredient.addEffect": "index",
    "Ingredient.effects": "array of effect snapshots",
    "Potion.setEffects": "—",
    "Potion.addEffect": "index",
    "Potion.effects": "array of effect snapshots",
    "Enchantment.setEffects": "—",
    "Enchantment.addEffect": "index",
    "Enchantment.effects": "array of effect snapshots",
    "Container.setContents": "—",
    "Container.contents": "array of entry snapshots",
    "Container.addItem": "—",
    "Container.removeItem": "bool",
    "Container.has": "bool",
    "Actor.skills": "array of skill snapshots",
    "Actor.setSkill": "—",
    "Shout.setVariation": "—",
    "Shout.setVariations": "—",
    "Shout.variations": "array of variation snapshots",
    "Shout.word": "Form or nil",
    "Shout.spell": "Form or nil",
    "FormList.add": "bool",
    "FormList.remove": "bool",
    "FormList.has": "bool",
    "FormList.forms": "array of Form",
}

# Hand-maintained notes for individual properties/methods (per type).
MEMBER_NOTES = {
    "Form.formId": "Full formID",
    "Form.typeId": "Engine form type",
    "Form.type": 'e.g. `"Weapon"`, `"Armor"`, `"Spell"`, `"LeveledItem"`, ...',
    "Form.editorId": "From the game's editorID table",
    "Form.name": "Full name",
    "Form.identifier": '`"Plugin.esm|000123"` display string (not parseable)',
    "Form.plugin": "Source plugin",
    "Form.value": "If the form has a value",
    "Form.weight": "If the form has a weight",
    "Form.enchantment": "The enchantment form",
    "Form.keywords": "As form objects",
    "Form.hasKeyword": "Checks the keyword form",
    "Global.value": "The global's float value",
    "Global.globalType": "Float/Long/Short",
    "Container.numObjects": "Entry count",
    "Container.allowStolenItems": "Whether looted items are flagged stolen",
    "Ingredient.costOverride": "Explicit value (0 = auto)",
    "Potion.costOverride": "Explicit value (0 = auto)",
    "Potion.isPoison": "Poison flag",
    "Potion.isFood": "Food flag",
    "Enchantment.costOverride": "Explicit value (0 = auto)",
    "Enchantment.chargeOverride": "Explicit charge amount (0 = auto)",
    "Enchantment.chargeTime": "Charge time in seconds",
    "Enchantment.castingType": 'ConstantEffect/FireAndForget/Concentration/Scroll',
    "Enchantment.delivery": 'Self/Touch/Aimed/TargetActor/TargetLocation',
    "Enchantment.baseEnchantment": "The base enchantment form",
    "Actor.level": "0 = scales with player level",
    "Actor.health": "Base health",
    "Actor.magicka": "Base magicka",
    "Actor.stamina": "Base stamina",
    "Actor.race": "The actor's race",
    "Actor.npcClass": "The actor's class",
    "Actor.setSkill": "By name or 1..18 index",
    "Shout.word": "Word of power at variation index (1..3)",
    "Shout.spell": "Spell at variation index (1..3)",
    "Light.radius": "In game units",
    "Light.color": "Table `{ r, g, b }` with 0..255 channels",
    "Light.fov": "Spotlight field of view",
    "Light.falloff": "Light falloff exponent",
    "Light.fade": "FNAM fade value",
    "Light.canCarry": "Can be picked up",
    "Light.dynamic": "Dynamic light flag",
    "Ingredient.effects": "Up to 4 effect slots",
    "Potion.effects": "Effect slots",
    "Enchantment.effects": "Effect slots",
    "Shout.variations": "3 entries `{ word, spell, recoveryTime }`",
    "Shout.setVariation": "Missing fields keep the current variation's values",
    "Shout.setVariations": "At most 3 entries; missing fields keep current values",
    "Container.contents": "Entry snapshots `{ form, count }`",
    "Container.setContents": "Replaces all contents",
    "Container.addItem": "Appends or accumulates the count",
    "Container.removeItem": "Removes every entry of the form",
}

# Extra prose injected after the LeveledList section.
LEVELED_LIST_PROSE = """
Entry snapshots have the shape `{ form = <Form>, count = integer, level = integer }`.

After every mutation the list is kept sorted by level. `findLeveledListsContaining`
and the entries of `entries()` are snapshots: patches applied during this run are
not reflected.

```lua
local ll = lua_patcher.leveledList("LuaPatcherExample.esp", "00001000")

ll:add("LuaPatcherExample.esp", "00002000", 5, 2)          -- plugin + local formID
ll:add("LuaPatcherExample.esp", "00002001", { level = 10 })-- options table
ll:add("LItemPotion", 1, 1)                                 -- editorID string
ll:add(ll:entries()[1].form, 3)                             -- form object

-- remove every entry of the form with level >= 5 and count <= 2
ll:remove("LuaPatcherExample.esp", "00002002", { minLevel = 5, maxCount = 2 })

-- predicate: full Lua power
ll:removeIf(function(e)
    return e.form.plugin == "SomeMod.esp" and e.level > 20
end)

ll.calculateForEachItem = true
ll.chanceNone = 50
```
"""

PROSE_INTRO = """# LuaPatcher API Reference

LuaPatcher patches Skyrim SE/AE game forms at runtime. Scripts live in
`Data/SKSE/Plugins/LuaPatcher/Scripts/` (recursively scanned, sorted), run once
at game load. Sibling `*_config.lua` files are **not** executed as scripts; they
are user configuration loaded on demand via `lua_patcher.tryLoadConfig`.

All API functions are written for Lua: options are passed as tables, filters as
predicate functions, and flags as named boolean properties — no string-encoded
conditions.

## Conventions

- **Form references**: a form object, an `"EditorID"` string, a
  `("Plugin.esm", "000123")` argument pair (plugin name + hex local formID;
  light plugins use the 0xFFF-masked ID), or a `{ "Plugin.esm", "000123" }`
  pair table for table fields. No `"Plugin|000123"` string splitting.
- **Errors**: invalid arguments and failed lookups raise a Lua error (catchable
  with `pcall`). `lua_patcher.getForm` is the exception: it returns `nil` for a
  miss instead of raising.
- **Unknown properties** on form objects raise an error (typo protection);
  writing to a read-only property raises an error.

## `lua_patcher` module
"""

PROSE_SCRIPT_LAYOUT = """## Script layout

```
Data/SKSE/Plugins/LuaPatcher/Scripts/
  MyMod.lua              # executed at load
  MyMod_config.lua       # NOT executed; loaded via lua_patcher.tryLoadConfig("MyMod")
```

Scripts run in priority order: a `-- priority: N` comment (first 512 bytes of the
file) sets the execution order, lowest first; scripts without a declaration get
priority 0 and run first. Equal priorities fall back to path order. EverythingRandomizer
declares priority 50 so user scripts in the 10-40 range run before it.

Config chunks return a table; `tryLoadConfig` returns it (or nil when the file
is missing or fails to load). Scripts typically merge it over defaults:

```lua
local CONFIG = { enabled = true, maxLevel = 50 }

local loaded = lua_patcher.tryLoadConfig("MyMod")
if loaded then
    for k, v in pairs(loaded) do CONFIG[k] = v end
end
```
"""

FOOTER = """<!--
This file is generated by docs/gen_api.py from the sol2 registrations in src/*.cpp.
Do not edit by hand - change the source or the generator, then run `invoke docs`.
-->
"""


# ------------------------------------------------------------- tokenizer


def _match(s: str, i: int, open_ch: str, close_ch: str) -> int:
    """Index just past the balanced group starting at s[i] (strings skipped)."""
    depth = 0
    j = i
    n = len(s)
    while j < n:
        ch = s[j]
        if ch == '"':
            j += 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == '"':
                    j += 1
                    break
                j += 1
            continue
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1
    return n


def _split_top(s: str, sep: str = ",") -> list[str]:
    parts: list[str] = []
    depth = 0
    cur: list[str] = []
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == sep and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
            continue
        cur.append(ch)
    if cur:
        parts.append("".join(cur).strip())
    return parts


_IDENT_RE = re.compile(r"(?:&)?[A-Za-z_]\w*(?:::\w+)*(?:<[^<>]*>)?")


def _tokenize(s: str):
    """Split C++ into top-level tokens: ('str', x) / ('call', name, inner) / ('lambda', sig, body) / ('ident', x)."""
    tokens = []
    i, n = 0, len(s)
    while i < n:
        ch = s[i]
        if ch.isspace():
            i += 1
            continue
        if ch == '"':
            j = i + 1
            buf = []
            while j < n:
                if s[j] == "\\" and j + 1 < n:
                    buf.append(s[j + 1])
                    j += 2
                elif s[j] == '"':
                    j += 1
                    break
                else:
                    buf.append(s[j])
                    j += 1
            tokens.append(("str", "".join(buf)))
            i = j
            continue
        if ch == "[":
            j = _match(s, i, "[", "]")
            sig = s[i:j]
            k = j
            while k < n and s[k].isspace():
                k += 1
            if k < n and s[k] == "(":
                k2 = _match(s, k, "(", ")")
                sig = s[i:k2]
                k = k2
            m = re.compile(r"\{").search(s, k)
            if m:
                k = m.start()
            while k < n and s[k].isspace():
                k += 1
            if k < n and s[k] == "{":
                k2 = _match(s, k, "{", "}")
                body = s[k + 1 : k2 - 1]
                i = k2
            else:
                body = ""
                i = j
            tokens.append(("lambda", sig, body))
            continue
        if ch in "(){}":
            i += 1
            continue
        m = _IDENT_RE.match(s, i)
        if m:
            tok = m.group(0)
            j = i + len(tok)
            k = j
            while k < n and s[k].isspace():
                k += 1
            if k < n and s[k] == "(":
                k2 = _match(s, k, "(", ")")
                tokens.append(("call", tok, s[k:k2]))
                i = k2
            else:
                tokens.append(("ident", tok))
                i = j
            continue
        i += 1
    return tokens


# ------------------------------------------------------------- value sets


def extract_value_sets(text: str) -> dict[str, list[str]]:
    """String literals from the *Name / TryParse* helper functions (value sets)."""
    sets: dict[str, list[str]] = {}
    for m in re.finditer(r"\b(?:std::string_view|std::string|bool)\s+(TryParse\w+|\w+Name)\s*\(", text):
        name = m.group(1)
        k = text.find("{", m.end())
        if k == -1:
            continue
        j = _match(text, k, "{", "}")
        body = text[k:j]
        vals = re.findall(r'==\s*"([A-Za-z_]+)"', body)
        vals += re.findall(r'return\s+"([A-Za-z_]+)"', body)
        vals = [v for v in vals if v != "Other"]
        if vals:
            sets[name] = list(dict.fromkeys(vals))
    return sets


# ------------------------------------------------------------- parsers


def parse_property(key: str, call_text: str, value_sets: dict[str, list[str]]) -> dict:
    inner = call_text[call_text.find("(") + 1 : call_text.rfind(")")]
    parts = _split_top(inner)
    getter = parts[0] if parts else ""
    setter = parts[1] if len(parts) > 1 else ""
    return {
        "name": key,
        "kind": "rw" if setter else "ro",
        "getter": getter,
        "setter": setter,
        "type": infer_type(getter, setter),
        "notes": infer_notes(key, getter, setter, value_sets),
    }


def parse_method(key: str, sig: str, body: str) -> dict:
    args: list[str] = []
    m = re.search(r"\((.*)\)\s*(?:->[^{]*)?$", sig, re.DOTALL)
    if m:
        for p in _split_top(m.group(1))[1:]:  # drop the self parameter
            p = p.strip()
            if not p or p.startswith("sol::this_state"):
                continue
            vm = re.search(r"\b(a_\w+)$", p)
            var = vm.group(1) if vm else p.split()[-1]
            mapped = {
                "a_form": "formOrId",
                "a_keyword": "kwOrId",
                "a_from": "fromOrId",
                "a_to": "toOrId",
                "a_level": "level?",
                "a_count": "count?",
                "a_options": "options?",
                "a_predicate": "predicate",
                "a_factor": "factor",
                "a_value": "value",
            }
            args.append(mapped.get(var, var[2:] if var.startswith("a_") else var))
    return {"name": key, "args": args, "returns": ""}


def infer_type(getter: str, setter: str) -> str:
    g = getter
    if "sol::optional<LuaForm>" in g:
        return "Form or nil"
    if "sol::optional<lua_Integer>" in g:
        return "integer or nil"
    if "sol::optional<double>" in g:
        return "number or nil"
    if "sol::optional<std::string>" in g:
        return "string or nil"
    if "std::vector<LuaForm>" in g:
        return "array of Form"
    if "std::vector<std::string>" in g:
        return "array of string"
    if "PushColor" in g:
        return "table { r, g, b }"
    if "static_cast<lua_Integer>" in g:
        return "integer"
    if "std::string(" in g or "FormToIdentifier" in g:
        return "string"
    if "HasFlag" in g:
        return "bool"
    if re.search(r"\b(IsMelee|IsRanged|IsBow|IsStaff|IsCrossbow|GetPlayable|IsHostile|IsDetrimental|IsPoison|IsFood|CanBeCarried)\(", g):
        return "bool"
    if "flags.all(" in g or "flags.any(" in g or "flags.none(" in g:
        return "bool"
    if "== 0" in g or "!= 0" in g:
        return "bool"
    if setter:
        m = re.search(r",\s*((?:const\s+)?[\w:]+)\s+a_value\b", setter)
        t = m.group(1) if m else ""
        if "bool" in t:
            return "bool"
        if "double" in t or "float" in t:
            return "number"
        if "lua_Integer" in t:
            return "integer"
        if "std::string" in t:
            return "string"
        if "sol::object" in t:
            return "Form or other"
    return "?"


def infer_notes(key: str, getter: str, setter: str, value_sets: dict[str, list[str]]) -> str:
    s = (getter or "") + (setter or "")
    if "0xFFFF" in s and "std::max" in s:
        return "Clamped to 0..65535"
    if "std::lround" in s and "* 100.0" in s:
        return "Stored as rating x 100, rounded"
    if "std::max" in s and "a_value" in s:
        return "Non-negative"
    if "As<RE::TESGlobal>" in s:
        return "Must be a TESGlobal"
    m = re.search(r"Flag::(\w+)", s)
    if m:
        return f"{m.group(1)} flag"
    if "static_cast<std::int8_t>" in s:
        return "Stored as int8"
    for fn in re.findall(r"TryParse(\w+)\(", setter):
        if "TryParse" + fn in value_sets:
            return ", ".join(f'"{v}"' for v in value_sets["TryParse" + fn])
    for fn in re.findall(r"(\w+Name)\(", getter):
        if fn in value_sets:
            return ", ".join(f'"{v}"' for v in value_sets[fn])
    return ""


def parse_usertype(body: str, value_sets: dict[str, list[str]]) -> dict:
    tokens = _tokenize(body)
    if not tokens or tokens[0][0] != "str":
        return {}
    entry = {"name": tokens[0][1], "extends": None, "tostring": None, "properties": [], "methods": []}
    pending_key = None
    for tok in tokens[1:]:
        kind = tok[0]
        if kind == "str":
            pending_key = tok[1]
        elif kind == "call":
            head = tok[1]
            if head.startswith("sol::bases"):
                m = re.search(r"<\s*(\w+)\s*>", head)
                if m:
                    entry["extends"] = m.group(1)
            elif head == "sol::property" and pending_key:
                entry["properties"].append(parse_property(pending_key, tok[2], value_sets))
                pending_key = None
        elif kind == "lambda":
            if pending_key:
                entry["methods"].append(parse_method(pending_key, tok[1], tok[2]))
                pending_key = None
            else:
                m = re.search(r'fmt::format\("([^"]+)"', tok[2])
                if m:
                    entry["tostring"] = m.group(1)
    return entry


# ------------------------------------------------------------- rendering


def esc(s: str) -> str:
    return s.replace("|", "\\|")


def render_method(name: str, method: dict, receiver: str, type_name: str) -> str:
    args = METHOD_ARGS.get(f"{type_name}.{name}")
    if args is None:
        args = ", ".join(method["args"]) if method["args"] else ""
    returns = METHOD_RETURNS.get(f"{type_name}.{name}", "—")
    return f"| `{receiver}:{name}({args})` | {returns} | |"


def render_property(prop: dict) -> str:
    return f"| `{prop['name']}` | {prop['kind']} | {esc(prop['type'])} | {esc(prop['notes'])} |"


def main() -> int:
    files = [
        SRC_DIR / "LuaApi.cpp",
        SRC_DIR / "LeveledList.cpp",
        SRC_DIR / "Equipment.cpp",
        SRC_DIR / "Magic.cpp",
        SRC_DIR / "FormList.cpp",
        SRC_DIR / "Alchemy.cpp",
        SRC_DIR / "Enchantment.cpp",
        SRC_DIR / "Container.cpp",
        SRC_DIR / "Actors.cpp",
        SRC_DIR / "World.cpp",
        SRC_DIR / "Shout.cpp",
        SRC_DIR / "Light.cpp",
        SRC_DIR / "EncounterZone.cpp",
        SRC_DIR / "Protection.cpp",
    ]
    types: dict[str, dict] = {}
    module: list[tuple[str, str, str]] = []  # (cpp function name, lua key, source text)
    value_sets: dict[str, list[str]] = {}

    for path in files:
        text = path.read_text(encoding="utf-8")
        value_sets.update(extract_value_sets(text))

        for m in re.finditer(r"new_usertype<\s*(\w+)\s*>\(", text):
            k = m.end()
            j = _match(text, k - 1, "(", ")")
            t = parse_usertype(text[k : j - 1], value_sets)
            if t:
                types[t["name"]] = t

        target = types.get("LeveledList")
        for m in re.finditer(r'(?:a_type|type)\[\s*"(\w+)"\s*\]\s*=\s*', text):
            val = _tokenize(text[m.end() :])
            if not val:
                continue
            # the usertype a property belongs to comes from the enclosing
            # Register* function name, e.g. RegisterFormList -> FormList,
            # RegisterLeveledListProperties -> LeveledList
            type_name = ""
            for fm in re.finditer(r"\bvoid\s+(Register\w+)\s*\(", text):
                if fm.end() <= m.start():
                    type_name = fm.group(1)[len("Register") :]
            for suffix in ("Properties", "Operations", "Mutators"):
                if type_name.endswith(suffix):
                    type_name = type_name[: -len(suffix)]
                    break
            target = types.get(type_name)
            if not target:
                continue
            kind = val[0][0]
            if kind == "call" and val[0][1] == "sol::property":
                target["properties"].append(parse_property(m.group(1), val[0][2], value_sets))
            elif kind == "lambda":
                target["methods"].append(parse_method(m.group(1), val[0][1], val[0][2]))

        for m in re.finditer(r'(?:a_lua|patcher)\[\s*"(\w+)"\s*\]\s*=\s*((?:&?[A-Za-z_]\w*)|"[^"]*"|\[)', text):
            if m.group(1) == "lua_patcher":
                continue
            value = m.group(2)
            cpp_name = value[1:] if value.startswith("&") else ""
            module.append((cpp_name, m.group(1), text))

    lines: list[str] = []
    lines.append(PROSE_INTRO.rstrip())
    lines.append("")
    lines.append("| Function | Returns | Notes |")
    lines.append("|---|---|---|")
    module_rows = [row for row in module if row[1] != "print"] + [row for row in module if row[1] == "print"]
    for cpp_name, name, text in module_rows:
        display = name
        if name in ("log", "warn", "error", "print"):
            display = name + "(...)"
        elif cpp_name:
            m = re.search(rf"\b(?:sol::object|bool|int|void)\s+{cpp_name}\s*\(([^)]*)\)", text)
            if m:
                args = []
                for p in _split_top(m.group(1)):
                    p = p.strip()
                    if not p or p.startswith("sol::this_state") or p.startswith("lua_State"):
                        continue
                    vm = re.search(r"\b(a_\w+)$", p)
                    var = vm.group(1) if vm else p.split()[-1]
                    args.append({"a_identifier": "identifier", "a_name": "name", "a_form": "formOrId"}.get(var, var))
                display = name + "(" + ", ".join(args) + ")"
        if name in MODULE_ARGS:
            display = name + "(" + MODULE_ARGS[name] + ")"
        returns = MODULE_RETURNS.get(name, "?")
        if name == "version":
            returns = "string"
        if name in ("log", "warn", "error", "print"):
            returns = "—"
        note = MODULE_NOTES.get(name, "")
        if name == "print":
            note = "Redirected to the plugin log (same formatting as log)"
        lines.append(f"| `{display}` | {returns} | {esc(note)} |")
    lines.append("")
    lines.append("")

    for tname in TYPE_ORDER:
        t = types.get(tname)
        if not t:
            continue
        if tname == "Form":
            lines.append("## Form (base type)")
            lines.append("")
            lines.append(
                "Weapon, Armor, LeveledList, Spell, MagicEffect, Enchantment, Ingredient, Potion, Container, "
                "Actor, Shout, Light and Global objects inherit these."
            )
            if t["tostring"]:
                lines.append("")
                lines.append(f"`tostring(form)` → `\"{esc(t['tostring'])}\"`")
        else:
            if t["extends"]:
                lines.append(f"## {tname} (extends {t['extends'].removeprefix('Lua')})")
            else:
                lines.append(f"## {tname}")
            if t["tostring"]:
                lines.append("")
                lines.append(f"`tostring({RECEIVERS.get(tname, 'x')})` → `\"{esc(t['tostring'])}\"`")
        if t["properties"]:
            lines.append("")
            lines.append("| Property | R/W | Type | Notes |")
            lines.append("|---|---|---|---|")
            for p in t["properties"]:
                notes = p["notes"] or MEMBER_NOTES.get(f"{tname}.{p['name']}", "")
                lines.append(render_property({**p, "notes": notes}))
        if t["methods"]:
            lines.append("")
            lines.append("| Method | Returns | Notes |")
            lines.append("|---|---|---|")
            for m in t["methods"]:
                lines.append(render_method(m["name"], m, RECEIVERS.get(tname, "x"), tname))
        if tname == "LeveledList":
            lines.append(LEVELED_LIST_PROSE.rstrip())
        lines.append("")

    lines.append(PROSE_SCRIPT_LAYOUT.rstrip())
    lines.append("")
    lines.append(FOOTER)

    OUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())