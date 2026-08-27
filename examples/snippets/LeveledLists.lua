-- LuaPatcher example: LeveledLists
--
-- Form identifiers work exactly like SkyPatcher's:
--   "Plugin.esm|000123"  -> form in a specific plugin (light plugins use 0xFFF-masked IDs)
--   "SomeEditorID"       -> looked up by EditorID
--
-- Reference cheat sheet (all items are also available on LeveledList):
--   lua_patcher.getForm(id)                     -> Form (formId, editorId, name, type, identifier)
--   lua_patcher.isPluginInstalled("Skyrim.esm") -> true/false
--   lua_patcher.allLeveledItems()               -> every TESLevItem as LeveledList
--   lua_patcher.allLeveledCharacters()          -> every TESLevCharacter as LeveledList
--   lua_patcher.LeveledListFlags                -> { calculateFromAllLevels=1, calculateForEachItem=2, useAll=4, specialLoot=8 }

if not lua_patcher.isPluginInstalled("LuaPatcherExample.esp") then
	return
end

-- 1) Grab a leveled list by form ID
local chest = lua_patcher.leveledList("LuaPatcherExample.esp|00001000")
if chest == nil then
	lua_patcher.warn("Example: LuaPatcherExample.esp|00001000 not found, skipping")
	return
end

print("Patching leveled list " .. chest.identifier)

-- 2) Basic ops (SkyPatcher addToLLs / removeFromLLs equivalents)
chest:add("LuaPatcherExample.esp|00002000", 1, 1)           -- add form at level 1, count 1
chest:add("LuaPatcherExample.esp|00002001", 10, 2)          -- add at level 10, count 2
chest:addOnce("LuaPatcherExample.esp|00002002")             -- add, but never twice
chest:remove("LuaPatcherExample.esp|00002003")              -- remove all entries of a form
chest:remove("LuaPatcherExample.esp|00002004", ">5", ">=2") -- only level > 5 AND count >= 2

-- 3) Replace and rescale (SkyPatcher formsToReplace / objectMultCount)
chest:replace("LuaPatcherExample.esp|00003000", "LuaPatcherExample.esp|00003001")
chest:multiplyCount("LuaPatcherExample.esp|00004000", 1.5) -- count = ceil(count * 1.5)

-- 4) Flags / chance (SkyPatcher calcForLevel, calcEachItem, calcUseAll, chanceNone, chanceGlobal)
chest.flags = lua_patcher.LeveledListFlags.calculateFromAllLevels
	| lua_patcher.LeveledListFlags.calculateForEachItem
chest.chanceNone = 50
chest:clearFlags() -- ...or wipe them all

-- 5) Inspect (equivalent of reading the list back)
for i, entry in ipairs(chest:entries()) do
	print(string.format("  entry %d: %s count=%d level=%d", i, entry.form.identifier, entry.count, entry.level))
end
print("numEntries = " .. chest.numEntries)

-- 6) Bulk patch with Lua logic (the whole point of a scripting patcher):
--    every non-empty Skyrim.esm leveled item gets another entry appended at level 1.
for _, ll in ipairs(lua_patcher.allLeveledItems()) do
	if ll:entries()[1] ~= nil then
		ll:add("LuaPatcherExample.esp|00005000", 1, 1)
	end
end
