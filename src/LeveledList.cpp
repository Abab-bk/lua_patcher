#include "LuaApi.h"

#include "PCH.h"

#include <RE/T/TESLeveledList.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	RE::TESForm* ToForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(
			luaL_checkudata(a_state, a_index, LuaPatcher::kLeveledListMeta.data()));
	}

	RE::TESLeveledList* ToLeveledList(lua_State* a_state, int a_index)
	{
		return ToForm(a_state, a_index)->As<RE::TESLeveledList>();
	}

	// Copies the game array into a std::vector, the same pattern SkyPatcher's
	// convertLLtoVec uses.
	std::vector<RE::LEVELED_OBJECT> ToVector(RE::TESLeveledList* a_list)
	{
		std::vector<RE::LEVELED_OBJECT> result;
		result.reserve(a_list->entries.size());
		for (const auto& entry : a_list->entries) {
			result.push_back(entry);
		}
		return result;
	}

	// Writes a vector back into the game array, mirroring SkyPatcher's createLL.
	void Apply(RE::TESLeveledList* a_list, const std::vector<RE::LEVELED_OBJECT>& a_entries)
	{
		if (a_entries.empty()) {
			a_list->entries.clear();
			a_list->numEntries = 0;
			return;
		}

		if (a_entries.size() > UINT8_MAX) {
			logger::warn(
				"LuaPatcher: leveled list has {} entries, exceeding the engine's 255-entry limit; "
				"the list may behave incorrectly in-game",
				a_entries.size());
		}

		a_list->entries.resize(a_entries.size());
		for (std::size_t i = 0; i < a_entries.size(); ++i) {
			a_list->entries[i] = a_entries[i];
		}
		a_list->numEntries = static_cast<std::uint8_t>(a_entries.size());
	}

	void Clear(RE::TESLeveledList* a_list)
	{
		a_list->entries.clear();
		a_list->numEntries = 0;
	}

	void SortByLevel(std::vector<RE::LEVELED_OBJECT>& a_entries)
	{
		std::sort(a_entries.begin(), a_entries.end(), [](const RE::LEVELED_OBJECT& a, const RE::LEVELED_OBJECT& b) {
			return a.level < b.level;
		});
	}

	// A level/count filter, e.g. nil (no filter), 5 (== 5), ">5", ">=5", "<5", "<=5".
	struct Condition
	{
		bool        active = false;
		bool        eq = false;
		char        op = 0;
		lua_Integer num = 0;
	};

	Condition CheckCondition(lua_State* a_state, int a_index)
	{
		Condition condition;
		if (lua_isnoneornil(a_state, a_index)) {
			return condition;
		}

		if (lua_isnumber(a_state, a_index)) {
			condition.active = true;
			condition.num = lua_tointeger(a_state, a_index);
			return condition;
		}

		const auto text = luaL_checkstring(a_state, a_index);
		if (!text || !*text || std::string_view(text) == "none") {
			return condition;
		}

		std::string_view sv(text);
		if (sv.size() >= 2 && sv[0] == '>' && sv[1] == '=') {
			condition.op = '>';
			condition.eq = true;
			sv.remove_prefix(2);
		} else if (sv.size() >= 2 && sv[0] == '<' && sv[1] == '=') {
			condition.op = '<';
			condition.eq = true;
			sv.remove_prefix(2);
		} else if (sv[0] == '>' || sv[0] == '<') {
			condition.op = sv[0];
			sv.remove_prefix(1);
		} else if (sv[0] == '=' && sv.size() >= 2 && sv[1] == '=') {
			sv.remove_prefix(2);
		}

		if (sv.empty()) {
			luaL_argerror(a_state, a_index, "invalid condition, expected e.g. '>5', '<=10' or a number");
		}
		try {
			condition.num = std::stoll(std::string(sv));
		} catch (const std::exception&) {
			luaL_error(a_state, "invalid condition '%s', expected e.g. '>5', '<=10' or a number", text);
		}
		condition.active = true;
		return condition;
	}

	bool Matches(const Condition& a_condition, lua_Integer a_value)
	{
		if (!a_condition.active) {
			return true;
		}
		switch (a_condition.op) {
		case '>':
			return a_condition.eq ? a_value >= a_condition.num : a_value > a_condition.num;
		case '<':
			return a_condition.eq ? a_value <= a_condition.num : a_value < a_condition.num;
		default:
			return a_value == a_condition.num;
		}
	}

	// ---- LeveledList methods ----

	int LL_Entries(lua_State* a_state)
	{
		auto* list = ToLeveledList(a_state, 1);

		lua_createtable(a_state, static_cast<int>(list->entries.size()), 0);
		lua_Integer index = 1;
		for (const auto& entry : list->entries) {
			lua_createtable(a_state, 0, 3);
			LuaPatcher::PushForm(a_state, entry.form);
			lua_setfield(a_state, -2, "form");
			lua_pushinteger(a_state, entry.count);
			lua_setfield(a_state, -2, "count");
			lua_pushinteger(a_state, entry.level);
			lua_setfield(a_state, -2, "level");
			lua_rawseti(a_state, -2, index++);
		}
		return 1;
	}

	int LL_Add(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* form = LuaPatcher::CheckForm(a_state, 2);
		auto  level = luaL_optinteger(a_state, 3, 1);
		auto  count = luaL_optinteger(a_state, 4, 1);

		auto entries = ToVector(list);
		entries.push_back({ form, static_cast<std::uint16_t>(count), static_cast<std::uint16_t>(level) });
		SortByLevel(entries);
		Apply(list, entries);
		logger::debug("LuaPatcher: {:08X} added Form {:08X}", listForm->formID, form->formID);
		return 0;
	}

	int LL_AddOnce(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* form = LuaPatcher::CheckForm(a_state, 2);
		auto  level = luaL_optinteger(a_state, 3, 1);
		auto  count = luaL_optinteger(a_state, 4, 1);

		auto       entries = ToVector(list);
		const bool already = std::any_of(entries.begin(), entries.end(), [form](const RE::LEVELED_OBJECT& x) {
			return x.form->formID == form->formID;
		});
		if (!already) {
			entries.push_back({ form, static_cast<std::uint16_t>(count), static_cast<std::uint16_t>(level) });
			SortByLevel(entries);
			Apply(list, entries);
		}
		logger::debug("LuaPatcher: {:08X} addOnce Form {:08X} already present: {}", listForm->formID, form->formID, already);
		return 0;
	}

	int LL_Remove(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* delForm = LuaPatcher::CheckForm(a_state, 2);
		auto  levelCond = CheckCondition(a_state, 3);
		auto  countCond = CheckCondition(a_state, 4);

		auto entries = ToVector(list);
		entries.erase(std::remove_if(entries.begin(), entries.end(),
						  [&](const RE::LEVELED_OBJECT& x) {
							  return x.form == delForm &&
			                         Matches(levelCond, x.level) &&
			                         Matches(countCond, x.count);
						  }),
			entries.end());

		SortByLevel(entries);
		Apply(list, entries);
		logger::debug("LuaPatcher: {:08X} removed Form {:08X}", listForm->formID, delForm->formID);
		return 0;
	}

	int LL_RemoveByKeyword(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* form = LuaPatcher::CheckForm(a_state, 2);
		auto* keyword = form->As<RE::BGSKeyword>();
		if (!keyword) {
			return luaL_argerror(a_state, 2, "expected a keyword form");
		}

		auto       entries = ToVector(list);
		const auto before = entries.size();
		entries.erase(std::remove_if(entries.begin(), entries.end(),
						  [&](const RE::LEVELED_OBJECT& x) {
							  const auto* keyForm = x.form->As<RE::BGSKeywordForm>();
							  return keyForm && keyForm->HasKeyword(keyword);
						  }),
			entries.end());

		if (entries.size() != before) {
			SortByLevel(entries);
			Apply(list, entries);
		}
		logger::debug("LuaPatcher: {:08X} removed {} items with keyword {:08X}", listForm->formID, before - entries.size(), keyword->formID);
		return 0;
	}

	int LL_Replace(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* from = LuaPatcher::CheckForm(a_state, 2);
		auto* to = LuaPatcher::CheckForm(a_state, 3);

		auto        entries = ToVector(list);
		std::size_t replaced = 0;
		for (auto& entry : entries) {
			if (entry.form == from) {
				entry.form = to;
				++replaced;
			}
		}

		if (replaced > 0) {
			SortByLevel(entries);
			Apply(list, entries);
		}
		logger::debug("LuaPatcher: {:08X} replaced {:08X} with {:08X} ({} entries)", listForm->formID, from->formID, to->formID, replaced);
		return 0;
	}

	int LL_MultiplyCount(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		auto* form = LuaPatcher::CheckForm(a_state, 2);
		auto  multiplier = luaL_checknumber(a_state, 3);

		auto        entries = ToVector(list);
		std::size_t changed = 0;
		for (auto& entry : entries) {
			if (entry.form == form) {
				entry.count = static_cast<std::uint16_t>(std::ceil(static_cast<double>(entry.count) * multiplier));
				++changed;
			}
		}

		if (changed > 0) {
			Apply(list, entries);
		}
		logger::debug("LuaPatcher: {:08X} multiplied count of {:08X} by {} ({} entries)", listForm->formID, form->formID, multiplier, changed);
		return 0;
	}

	int LL_Has(lua_State* a_state)
	{
		auto*      list = ToLeveledList(a_state, 1);
		auto*      form = LuaPatcher::CheckForm(a_state, 2);
		const bool found = std::any_of(list->entries.begin(), list->entries.end(),
			[form](const RE::LEVELED_OBJECT& x) {
				return x.form->formID == form->formID;
			});
		lua_pushboolean(a_state, found);
		return 1;
	}

	int LL_Clear(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		Clear(list);
		logger::debug("LuaPatcher: {:08X} cleared", listForm->formID);
		return 0;
	}

	int LL_ClearFlags(lua_State* a_state)
	{
		auto* listForm = ToForm(a_state, 1);
		auto* list = ToLeveledList(a_state, 1);
		list->llFlags = static_cast<RE::TESLeveledList::Flag>(0);
		logger::debug("LuaPatcher: {:08X} cleared all flags", listForm->formID);
		return 0;
	}

	int LL_Sort(lua_State* a_state)
	{
		auto* list = ToLeveledList(a_state, 1);
		auto  entries = ToVector(list);
		SortByLevel(entries);
		Apply(list, entries);
		return 0;
	}

	// ---- LeveledList metatable ----

	int LeveledListIndex(lua_State* a_state)
	{
		auto*       form = ToForm(a_state, 1);
		auto*       list = form->As<RE::TESLeveledList>();
		const auto* key = luaL_checkstring(a_state, 2);

		if (std::string_view(key) == "numEntries") {
			lua_pushinteger(a_state, list->numEntries);
			return 1;
		}
		if (std::string_view(key) == "flags") {
			lua_pushinteger(a_state, list->llFlags);
			return 1;
		}
		if (std::string_view(key) == "chanceNone") {
			lua_pushinteger(a_state, list->chanceNone);
			return 1;
		}
		if (std::string_view(key) == "chanceGlobal") {
			return LuaPatcher::PushForm(a_state, list->chanceGlobal);
		}

		static const luaL_Reg methods[] = {
			{ "entries", LL_Entries },
			{ "add", LL_Add },
			{ "addOnce", LL_AddOnce },
			{ "remove", LL_Remove },
			{ "removeByKeyword", LL_RemoveByKeyword },
			{ "replace", LL_Replace },
			{ "multiplyCount", LL_MultiplyCount },
			{ "has", LL_Has },
			{ "clear", LL_Clear },
			{ "clearFlags", LL_ClearFlags },
			{ "sort", LL_Sort },
			{ nullptr, nullptr },
		};
		for (const auto* method = methods; method->name; ++method) {
			if (key == std::string_view(method->name)) {
				lua_pushcfunction(a_state, method->func);
				return 1;
			}
		}

		// Fall back to Form properties (a leveled list is a TESForm).
		return LuaPatcher::FormIndexCommon(a_state, form, key);
	}

	int LeveledListNewIndex(lua_State* a_state)
	{
		auto*       list = ToLeveledList(a_state, 1);
		const auto* key = luaL_checkstring(a_state, 2);

		if (std::string_view(key) == "flags") {
			list->llFlags = static_cast<RE::TESLeveledList::Flag>(luaL_checkinteger(a_state, 3));
			return 0;
		}
		if (std::string_view(key) == "chanceNone") {
			list->chanceNone = static_cast<std::int8_t>(luaL_checkinteger(a_state, 3));
			return 0;
		}
		if (std::string_view(key) == "chanceGlobal") {
			if (lua_isnil(a_state, 3)) {
				list->chanceGlobal = nullptr;
				return 0;
			}
			auto* form = LuaPatcher::CheckForm(a_state, 3);
			auto* global = form->As<RE::TESGlobal>();
			if (!global) {
				return luaL_argerror(a_state, 3, "chanceGlobal must be a global form or nil");
			}
			list->chanceGlobal = global;
			return 0;
		}

		return luaL_error(a_state, "property '%s' is read-only", key);
	}

	int LeveledListToString(lua_State* a_state)
	{
		auto* form = ToForm(a_state, 1);
		lua_pushstring(a_state, fmt::format("LeveledList[{:08X}]", form->formID).c_str());
		return 1;
	}

	// ---- lua_patcher leveled list functions ----

	int LeveledListGet(lua_State* a_state)
	{
		auto* form = LuaPatcher::CheckForm(a_state, 1);
		if (!form->As<RE::TESLevItem>() && !form->As<RE::TESLevCharacter>()) {
			return luaL_argerror(a_state, 1, "form is not a leveled item or leveled character list");
		}
		return LuaPatcher::PushForm(a_state, form);
	}

	template <class T>
	int PushLeveledListArray(lua_State* a_state)
	{
		auto*       dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<T>();

		lua_createtable(a_state, static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			LuaPatcher::PushForm(a_state, form);
			lua_rawseti(a_state, -2, index++);
		}
		return 1;
	}

	int AllLeveledItems(lua_State* a_state)
	{
		return PushLeveledListArray<RE::TESLevItem>(a_state);
	}

	int AllLeveledCharacters(lua_State* a_state)
	{
		return PushLeveledListArray<RE::TESLevCharacter>(a_state);
	}

	// Reverse index: formID -> leveled lists (TESLevItem + TESLevCharacter) that
	// contain an entry referencing it.
	//
	// Snapshot semantics: built once per data handler on first call and never
	// invalidated afterwards -- patches applied *during* this run are not
	// reflected, which is exactly what "is this form already in the game's
	// leveled lists" means. Safe because config parsing and patching run
	// single-threaded at kDataLoaded and the form arrays are fixed for the
	// session (same lifetime assumptions as SkyPatcher's cached_mod_lookup).
	const std::unordered_map<RE::FormID, std::vector<RE::TESForm*>>& LeveledListIndexCache()
	{
		static RE::TESDataHandler*                                       owner = nullptr;
		static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> index;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (owner != dataHandler) {
			index.clear();
			owner = dataHandler;
			if (dataHandler) {
				for (auto* list : dataHandler->GetFormArray<RE::TESLevItem>()) {
					for (const auto& entry : list->entries) {
						index[entry.form->formID].push_back(list);
					}
				}
				for (auto* list : dataHandler->GetFormArray<RE::TESLevCharacter>()) {
					for (const auto& entry : list->entries) {
						index[entry.form->formID].push_back(list);
					}
				}
			}
		}
		return index;
	}

	int FindLeveledListsContaining(lua_State* a_state)
	{
		auto*       form = LuaPatcher::CheckForm(a_state, 1);
		const auto& index = LeveledListIndexCache();

		lua_createtable(a_state, 0, 0);
		lua_Integer                      count = 1;
		std::unordered_set<RE::TESForm*> seen;
		if (const auto it = index.find(form->formID); it != index.end()) {
			for (auto* list : it->second) {
				if (seen.insert(list).second) {
					LuaPatcher::PushForm(a_state, list);
					lua_rawseti(a_state, -2, count++);
				}
			}
		}
		return 1;
	}
}

namespace LuaPatcher
{
	void RegisterLeveledList(lua_State* a_state)
	{
		luaL_newmetatable(a_state, kLeveledListMeta.data());
		lua_pushcfunction(a_state, LeveledListIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, LeveledListNewIndex);
		lua_setfield(a_state, -2, "__newindex");
		lua_pushcfunction(a_state, LeveledListToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		lua_getglobal(a_state, "lua_patcher");
		lua_pushcfunction(a_state, LeveledListGet);
		lua_setfield(a_state, -2, "leveledList");
		lua_pushcfunction(a_state, AllLeveledItems);
		lua_setfield(a_state, -2, "allLeveledItems");
		lua_pushcfunction(a_state, AllLeveledCharacters);
		lua_setfield(a_state, -2, "allLeveledCharacters");
		lua_pushcfunction(a_state, FindLeveledListsContaining);
		lua_setfield(a_state, -2, "findLeveledListsContaining");

		lua_createtable(a_state, 0, 4);
		lua_pushinteger(a_state, RE::TESLeveledList::Flag::kCalculateFromAllLevelsLTOrEqPCLevel);
		lua_setfield(a_state, -2, "calculateFromAllLevels");
		lua_pushinteger(a_state, RE::TESLeveledList::Flag::kCalculateForEachItemInCount);
		lua_setfield(a_state, -2, "calculateForEachItem");
		lua_pushinteger(a_state, RE::TESLeveledList::Flag::kUseAll);
		lua_setfield(a_state, -2, "useAll");
		lua_pushinteger(a_state, RE::TESLeveledList::Flag::kSpecialLoot);
		lua_setfield(a_state, -2, "specialLoot");
		lua_setfield(a_state, -2, "LeveledListFlags");
		lua_pop(a_state, 1);
	}
}