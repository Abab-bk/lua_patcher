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
	RE::TESLeveledList* ToLeveledList(const LuaPatcher::LuaLeveledList& a_list)
	{
		return a_list.form->As<RE::TESLeveledList>();
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
		bool active = false;
		bool eq = false;
		char op = 0;
		lua_Integer num = 0;
	};

	Condition CheckCondition(sol::object a_value)
	{
		Condition condition;
		if (a_value.is<sol::nil_t>()) {
			return condition;
		}

		if (a_value.is<double>()) {
			condition.active = true;
			condition.num = static_cast<lua_Integer>(a_value.as<double>());
			return condition;
		}

		const std::string text = a_value.as<std::string>();
		if (text.empty() || text == "none") {
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
			throw sol::error{ "invalid condition, expected e.g. '>5', '<=10' or a number" };
		}
		try {
			condition.num = std::stoll(std::string(sv));
		} catch (const std::exception&) {
			throw sol::error{ fmt::format("invalid condition '{}', expected e.g. '>5', '<=10' or a number", text) };
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

	// ---- lua_patcher leveled list functions ----

	sol::object LeveledListGet(sol::this_state a_state, sol::object a_form)
	{
		sol::state_view lua(a_state);
		auto* form = LuaPatcher::CheckForm(a_form);
		if (!form->As<RE::TESLevItem>() && !form->As<RE::TESLevCharacter>()) {
			throw sol::error{ "form is not a leveled item or leveled character list" };
		}
		return LuaPatcher::PushForm(lua, form);
	}

	template <class T>
	sol::object PushLeveledListArray(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<T>();

		sol::table result = lua.create_table(static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			result[index++] = LuaPatcher::PushForm(lua, form);
		}
		return result;
	}

	sol::object AllLeveledItems(sol::this_state a_state)
	{
		return PushLeveledListArray<RE::TESLevItem>(a_state);
	}

	sol::object AllLeveledCharacters(sol::this_state a_state)
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
		static RE::TESDataHandler* owner = nullptr;
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

	sol::object FindLeveledListsContaining(sol::this_state a_state, sol::object a_form)
	{
		sol::state_view lua(a_state);
		auto* form = LuaPatcher::CheckForm(a_form);
		const auto& index = LeveledListIndexCache();

		sol::table result = lua.create_table(0, 0);
		lua_Integer count = 1;
		std::unordered_set<RE::TESForm*> seen;
		if (const auto it = index.find(form->formID); it != index.end()) {
			for (auto* list : it->second) {
				if (seen.insert(list).second) {
					result[count++] = LuaPatcher::PushForm(lua, list);
				}
			}
		}
		return result;
	}
}

namespace LuaPatcher
{
	void RegisterLeveledList(sol::state_view a_lua)
	{
		a_lua.new_usertype<LuaLeveledList>("LeveledList", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index, sol::readonly_property(UnknownPropertyGetter<LuaLeveledList>), sol::meta_function::to_string, [](const LuaLeveledList& a_list) { return fmt::format("LeveledList[{:08X}]", a_list.form->formID); }, "numEntries", sol::property([](const LuaLeveledList& a_list) { return static_cast<lua_Integer>(ToLeveledList(a_list)->numEntries); }), "flags", sol::property([](const LuaLeveledList& a_list) { return static_cast<lua_Integer>(ToLeveledList(a_list)->llFlags); }, [](LuaLeveledList& a_list, lua_Integer a_value) { ToLeveledList(a_list)->llFlags = static_cast<RE::TESLeveledList::Flag>(a_value); }), "chanceNone", sol::property([](const LuaLeveledList& a_list) { return static_cast<lua_Integer>(ToLeveledList(a_list)->chanceNone); }, [](LuaLeveledList& a_list, lua_Integer a_value) { ToLeveledList(a_list)->chanceNone = static_cast<std::int8_t>(a_value); }), "chanceGlobal", sol::property([](const LuaLeveledList& a_list) -> sol::optional<LuaForm> {
					if (auto* global = ToLeveledList(a_list)->chanceGlobal) {
						return LuaForm{ global };
					}
					return sol::nullopt; }, [](LuaLeveledList& a_list, sol::object a_value) {
					if (a_value.is<sol::nil_t>()) {
						ToLeveledList(a_list)->chanceGlobal = nullptr;
						return;
					}
					auto* form = CheckForm(a_value);
					auto* global = form->As<RE::TESGlobal>();
					if (!global) {
						throw sol::error{ "chanceGlobal must be a global form or nil" };
					}
					ToLeveledList(a_list)->chanceGlobal = global; }), "entries", [](const LuaLeveledList& a_list, sol::this_state a_state) -> sol::object {
				sol::state_view lua(a_state);
				auto*           list = ToLeveledList(a_list);

				sol::table result = lua.create_table(static_cast<int>(list->entries.size()), 0);
				lua_Integer index = 1;
				for (const auto& entry : list->entries) {
					sol::table row = lua.create_table(0, 3);
					row["form"] = PushForm(lua, entry.form);
					row["count"] = entry.count;
					row["level"] = entry.level;
					result[index++] = row;
				}
				return result; }, "add", [](LuaLeveledList& a_list, sol::object a_form, sol::optional<lua_Integer> a_level, sol::optional<lua_Integer> a_count) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* form = CheckForm(a_form);
				auto  level = a_level.value_or(1);
				auto  count = a_count.value_or(1);

				auto entries = ToVector(list);
				entries.push_back({ form, static_cast<std::uint16_t>(count), static_cast<std::uint16_t>(level) });
				SortByLevel(entries);
				Apply(list, entries);
				logger::debug("LuaPatcher: {:08X} added Form {:08X}", listForm->formID, form->formID); }, "addOnce", [](LuaLeveledList& a_list, sol::object a_form, sol::optional<lua_Integer> a_level, sol::optional<lua_Integer> a_count) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* form = CheckForm(a_form);
				auto  level = a_level.value_or(1);
				auto  count = a_count.value_or(1);

				auto       entries = ToVector(list);
				const bool already = std::any_of(entries.begin(), entries.end(), [form](const RE::LEVELED_OBJECT& x) {
					return x.form->formID == form->formID;
				});
				if (!already) {
					entries.push_back({ form, static_cast<std::uint16_t>(count), static_cast<std::uint16_t>(level) });
					SortByLevel(entries);
					Apply(list, entries);
				}
				logger::debug("LuaPatcher: {:08X} addOnce Form {:08X} already present: {}", listForm->formID, form->formID, already); }, "remove", [](LuaLeveledList& a_list, sol::object a_form, sol::object a_level, sol::object a_count) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* delForm = CheckForm(a_form);
				auto  levelCond = CheckCondition(a_level);
				auto  countCond = CheckCondition(a_count);

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
				logger::debug("LuaPatcher: {:08X} removed Form {:08X}", listForm->formID, delForm->formID); }, "removeByKeyword", [](LuaLeveledList& a_list, sol::object a_keyword) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* form = CheckForm(a_keyword);
				auto* keyword = form->As<RE::BGSKeyword>();
				if (!keyword) {
					throw sol::error{ "expected a keyword form" };
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
				logger::debug("LuaPatcher: {:08X} removed {} items with keyword {:08X}", listForm->formID, before - entries.size(), keyword->formID); }, "replace", [](LuaLeveledList& a_list, sol::object a_from, sol::object a_to) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* from = CheckForm(a_from);
				auto* to = CheckForm(a_to);

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
				logger::debug("LuaPatcher: {:08X} replaced {:08X} with {:08X} ({} entries)", listForm->formID, from->formID, to->formID, replaced); }, "multiplyCount", [](LuaLeveledList& a_list, sol::object a_form, double a_multiplier) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				auto* form = CheckForm(a_form);

				auto        entries = ToVector(list);
				std::size_t changed = 0;
				for (auto& entry : entries) {
					if (entry.form == form) {
						entry.count = static_cast<std::uint16_t>(std::ceil(static_cast<double>(entry.count) * a_multiplier));
						++changed;
					}
				}

				if (changed > 0) {
					Apply(list, entries);
				}
				logger::debug("LuaPatcher: {:08X} multiplied count of {:08X} by {} ({} entries)", listForm->formID, form->formID, a_multiplier, changed); }, "has", [](const LuaLeveledList& a_list, sol::object a_form) {
				auto*      list = ToLeveledList(a_list);
				auto*      form = CheckForm(a_form);
				const bool found = std::any_of(list->entries.begin(), list->entries.end(),
					[form](const RE::LEVELED_OBJECT& x) {
						return x.form->formID == form->formID;
					});
				return found; }, "clear", [](LuaLeveledList& a_list) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				Clear(list);
				logger::debug("LuaPatcher: {:08X} cleared", listForm->formID); }, "clearFlags", [](LuaLeveledList& a_list) {
				auto* listForm = a_list.form;
				auto* list = ToLeveledList(a_list);
				list->llFlags = static_cast<RE::TESLeveledList::Flag>(0);
				logger::debug("LuaPatcher: {:08X} cleared all flags", listForm->formID); }, "sort", [](LuaLeveledList& a_list) {
				auto* list = ToLeveledList(a_list);
				auto  entries = ToVector(list);
				SortByLevel(entries);
				Apply(list, entries); });

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["leveledList"] = &LeveledListGet;
		patcher["allLeveledItems"] = &AllLeveledItems;
		patcher["allLeveledCharacters"] = &AllLeveledCharacters;
		patcher["findLeveledListsContaining"] = &FindLeveledListsContaining;

		sol::table flags = a_lua.create_table(0, 4);
		flags["calculateFromAllLevels"] = static_cast<lua_Integer>(RE::TESLeveledList::Flag::kCalculateFromAllLevelsLTOrEqPCLevel);
		flags["calculateForEachItem"] = static_cast<lua_Integer>(RE::TESLeveledList::Flag::kCalculateForEachItemInCount);
		flags["useAll"] = static_cast<lua_Integer>(RE::TESLeveledList::Flag::kUseAll);
		flags["specialLoot"] = static_cast<lua_Integer>(RE::TESLeveledList::Flag::kSpecialLoot);
		patcher["LeveledListFlags"] = flags;
	}
}