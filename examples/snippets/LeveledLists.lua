-- LuaPatcher example: LeveledLists
--
-- Form identifiers work like this:
--   "Plugin.esm|000123"  -> form in a specific plugin (light plugins use 0xFFF-masked IDs)
--   "SomeEditorID"       -> looked up by EditorID
--
-- Reference cheat sheet (all items are also available on LeveledList):
--   lua_patcher.getForm(id)                    -> Form (formId, editorId, name, type, identifier)
--   lua_patcher.isPluginInstalled("Skyrim.esm") -> true/false
--   lua_patcher.allLeveledItems()              -> every TESLevItem as LeveledList
--   lua_patcher.allLeveledCharacters()         -> every TESLevCharacter as LeveledList
--   lua_patcher.findLeveledListsContaining(f)  -> lists referencing a form (pristine snapshot)

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

-- 2) Add: positional or options table (level/count default to 1)
chest:add("LuaPatcherExample.esp|00002000", 1, 1)                          -- positional
chest:add("LuaPatcherExample.esp|00002001", { level = 10, count = 2 })     -- options table
chest:addIfAbsent("LuaPatcherExample.esp|00002002")                        -- add, but never twice

-- 3) Remove: plain (all entries of the form) or with inclusive bounds
chest:remove("LuaPatcherExample.esp|00002003")                             -- remove all entries of a form
chest:remove("LuaPatcherExample.esp|00002004", { minLevel = 6, minCount = 2 })  -- level >= 6 AND count >= 2

-- 4) Replace and rescale
chest:replace("LuaPatcherExample.esp|00003000", "LuaPatcherExample.esp|00003001")
chest:multiplyCount("LuaPatcherExample.esp|00004000", 1.5)                 -- count = ceil(count * 1.5)

-- 5) Flags / chance: named booleans, no bit arithmetic
chest.calculateFromAllLevels = true
chest.calculateForEachItem = true
chest.chanceNone = 50

-- 6) Inspect (equivalent of reading the list back)
for i, entry in ipairs(chest:entries()) do
	print(string.format("  entry %d: %s count=%d level=%d", i, entry.form.identifier, entry.count, entry.level))
end
print("numEntries = " .. chest.numEntries)

-- 7) Lua-native filtering: predicate over entry snapshots { form, count, level }
chest:removeIf(function(entry)
	return entry.form.plugin == "SomeMod.esp" and entry.level > 20
end)

-- 8) Bulk patch with Lua logic (the whole point of a scripting patcher):
--    every non-empty Skyrim.esm leveled item gets another entry appended at level 1.
for _, ll in ipairs(lua_patcher.allLeveledItems()) do
	if ll:entries()[1] ~= nil then
		ll:add("LuaPatcherExample.esp|00005000", 1, 1)
	end
end