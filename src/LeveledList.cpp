#include "LuaApi.h"

#include <RE/T/TESLeveledList.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
	RE::TESLeveledList* ToLeveledList(const LuaPatcher::LuaLeveledList& a_list)
	{
		return a_list.form->As<RE::TESLeveledList>();
	}

	// Copies the game's entry array into a std::vector. The engine's SimpleArray
	// cannot be resized safely while patching, so all mutations go through the
	// copy-modify-write-back cycle.
	std::vector<RE::LEVELED_OBJECT> SnapshotEntries(RE::TESLeveledList* a_list)
	{
		std::vector<RE::LEVELED_OBJECT> result;
		result.reserve(a_list->entries.size());
		for (const auto& entry : a_list->entries) {
			result.push_back(entry);
		}
		return result;
	}

	void WriteEntries(RE::TESLeveledList* a_list, const std::vector<RE::LEVELED_OBJECT>& a_entries)
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
		std::ranges::sort(a_entries,
			[](const RE::LEVELED_OBJECT& a, const RE::LEVELED_OBJECT& b) { return a.level < b.level; });
	}

	// Adds an entry, keeping the list sorted by level. Returns whether the entry
	// was actually appended (false when a_allowDuplicate is false and the form
	// is already present).
	bool InsertEntry(RE::TESLeveledList* a_list, RE::TESForm* a_form, lua_Integer a_level, lua_Integer a_count,
		bool a_allowDuplicate)
	{
		auto entries = SnapshotEntries(a_list);

		if (!a_allowDuplicate) {
			const bool already = std::ranges::any_of(entries,
				[a_form](const RE::LEVELED_OBJECT& x) { return x.form->formID == a_form->formID; });
			if (already) {
				return false;
			}
		}

		entries.push_back({ .form = a_form,
			.count = static_cast<std::uint16_t>(a_count),
			.level = static_cast<std::uint16_t>(a_level) });
		SortByLevel(entries);
		WriteEntries(a_list, entries);
		return true;
	}

	// Reads a numeric option strictly: present-but-not-a-number raises instead
	// of being silently ignored.
	lua_Integer GetNumberOption(const sol::table& a_options, std::string_view a_key, lua_Integer a_default)
	{
		const auto value = a_options.get<sol::optional<sol::object>>(a_key);
		if (!value) {
			return a_default;
		}
		if (!value->is<double>()) {
			throw sol::error{ fmt::format("bad options-table value '{}' (expected a number)", a_key) };
		}
		return static_cast<lua_Integer>(value->as<double>());
	}

	// level/count arguments: `add(form)`, `add(form, level, count)` or
	// `add(form, { level = n, count = n })`. Missing values default to 1.
	struct LevelCount
	{
		lua_Integer level = 1;
		lua_Integer count = 1;
	};

	// Reads a variadic argument as an optional object (nullopt when absent).
	sol::optional<sol::object> VariadicArg(const sol::variadic_args& a_args, std::size_t a_index)
	{
		return a_index < a_args.size() ? sol::optional<sol::object>{ a_args.get<sol::object>(a_index) } : sol::nullopt;
	}

	LevelCount ParseLevelCount(sol::optional<sol::object> a_level, sol::optional<sol::object> a_count,
		const std::string_view& a_method)
	{
		LevelCount result;
		if (!a_level) {
			return result;
		}

		const sol::object level = *a_level;
		if (level.is<double>()) {
			result.level = static_cast<lua_Integer>(level.as<double>());
			if (a_count) {
				const sol::object count = *a_count;
				if (!count.is<double>()) {
					throw sol::error{ fmt::format("bad argument #3 to '{}' (expected a count number)", a_method) };
				}
				result.count = static_cast<lua_Integer>(count.as<double>());
			}
			return result;
		}

		if (level.is<sol::table>()) {
			const auto options = level.as<sol::table>();
			result.level = GetNumberOption(options, "level", result.level);
			result.count = GetNumberOption(options, "count", result.count);
			return result;
		}

		throw sol::error{ fmt::format("bad argument #2 to '{}' (expected a level number, an options table, or nothing)",
			a_method) };
	}

	// Optional inclusive bounds for `remove(form, { minLevel, maxLevel, minCount, maxCount })`.
	struct Bounds
	{
		lua_Integer minLevel = 0;
		lua_Integer maxLevel = 0;
		lua_Integer minCount = 0;
		lua_Integer maxCount = 0;
		bool hasMinLevel = false;
		bool hasMaxLevel = false;
		bool hasMinCount = false;
		bool hasMaxCount = false;
	};

	Bounds ParseBounds(sol::optional<sol::object> a_options)
	{
		Bounds bounds;
		if (!a_options) {
			return bounds;
		}

		const sol::object options = *a_options;
		if (!options.is<sol::table>()) {
			throw sol::error{ "bad argument #2 to 'remove' (expected an options table or nothing)" };
		}

		const auto table = options.as<sol::table>();

		if (const auto value = table.get<sol::optional<sol::object>>("minLevel")) {
			if (!value->is<double>()) {
				throw sol::error{ "bad options-table value 'minLevel' (expected a number)" };
			}
			bounds.minLevel = static_cast<lua_Integer>(value->as<double>());
			bounds.hasMinLevel = true;
		}

		if (const auto value = table.get<sol::optional<sol::object>>("maxLevel")) {
			if (!value->is<double>()) {
				throw sol::error{ "bad options-table value 'maxLevel' (expected a number)" };
			}
			bounds.maxLevel = static_cast<lua_Integer>(value->as<double>());
			bounds.hasMaxLevel = true;
		}

		if (const auto value = table.get<sol::optional<sol::object>>("minCount")) {
			if (!value->is<double>()) {
				throw sol::error{ "bad options-table value 'minCount' (expected a number)" };
			}
			bounds.minCount = static_cast<lua_Integer>(value->as<double>());
			bounds.hasMinCount = true;
		}

		if (const auto value = table.get<sol::optional<sol::object>>("maxCount")) {
			if (!value->is<double>()) {
				throw sol::error{ "bad options-table value 'maxCount' (expected a number)" };
			}
			bounds.maxCount = static_cast<lua_Integer>(value->as<double>());
			bounds.hasMaxCount = true;
		}

		return bounds;
	}

	bool MatchesBounds(const Bounds& a_bounds, lua_Integer a_level, lua_Integer a_count)
	{
		if (a_bounds.hasMinLevel && a_level < a_bounds.minLevel) {
			return false;
		}
		if (a_bounds.hasMaxLevel && a_level > a_bounds.maxLevel) {
			return false;
		}
		if (a_bounds.hasMinCount && a_count < a_bounds.minCount) {
			return false;
		}
		if (a_bounds.hasMaxCount && a_count > a_bounds.maxCount) {
			return false;
		}
		return true;
	}

	// Removes matching entries; returns how many were removed.
	std::size_t EraseMatching(RE::TESLeveledList* a_list, const std::function<bool(const RE::LEVELED_OBJECT&)>& a_match)
	{
		auto entries = SnapshotEntries(a_list);
		const auto before = entries.size();
		const auto result = std::ranges::remove_if(entries, a_match);
		entries.erase(result.begin(), result.end());

		if (entries.size() != before) {
			SortByLevel(entries);
			WriteEntries(a_list, entries);
		}
		return before - entries.size();
	}

	// Pushes an entry snapshot table { form, count, level }.
	sol::table PushEntry(sol::state_view a_lua, const RE::LEVELED_OBJECT& a_entry)
	{
		sol::table row = a_lua.create_table(0, 3);
		row["form"] = LuaPatcher::PushForm(a_lua, a_entry.form);
		row["count"] = a_entry.count;
		row["level"] = a_entry.level;
		return row;
	}

	// ---- named level-list flags (read/write booleans) ----

	bool HasFlag(const LuaPatcher::LuaLeveledList& a_list, RE::TESLeveledList::Flag a_flag)
	{
		return (ToLeveledList(a_list)->llFlags & a_flag) != 0;
	}

	void SetFlag(LuaPatcher::LuaLeveledList& a_list, RE::TESLeveledList::Flag a_flag, bool a_value)
	{
		auto& flags = ToLeveledList(a_list)->llFlags;
		flags = a_value ? static_cast<RE::TESLeveledList::Flag>(flags | a_flag) :
		                  static_cast<RE::TESLeveledList::Flag>(flags & ~a_flag);
	}

	// ---- lua_patcher leveled list functions ----

	sol::object LeveledListGet(sol::this_state a_state, sol::variadic_args a_args)
	{
		sol::state_view lua(a_state);
		auto* form = LuaPatcher::ParseFormRef(a_args, "leveledList").form;
		if (!form->As<RE::TESLevItem>() && !form->As<RE::TESLevCharacter>() && !form->As<RE::TESLevSpell>()) {
			throw sol::error{ "form is not a leveled item, leveled character or leveled spell list" };
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

	sol::object AllLeveledItems(sol::this_state a_state) { return PushLeveledListArray<RE::TESLevItem>(a_state); }

	sol::object AllLeveledCharacters(sol::this_state a_state)
	{
		return PushLeveledListArray<RE::TESLevCharacter>(a_state);
	}

	sol::object AllLeveledSpells(sol::this_state a_state) { return PushLeveledListArray<RE::TESLevSpell>(a_state); }

	// Reverse index: formID -> leveled lists (TESLevItem + TESLevCharacter +
	// TESLevSpell) that contain an entry referencing it.
	//
	// Snapshot semantics: built once per data handler on first call and never
	// invalidated afterwards -- patches applied *during* this run are not
	// reflected, which is exactly what "is this form already in the game's
	// leveled lists" means. Built on the pristine form arrays (fixed for the
	// session), so it is safe to cache at kDataLoaded.
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
				for (auto* list : dataHandler->GetFormArray<RE::TESLevSpell>()) {
					for (const auto& entry : list->entries) {
						index[entry.form->formID].push_back(list);
					}
				}
			}
		}
		return index;
	}

	sol::object FindLeveledListsContaining(sol::this_state a_state, sol::variadic_args a_args)
	{
		sol::state_view lua(a_state);
		auto* form = LuaPatcher::ParseFormRef(a_args, "findLeveledListsContaining").form;
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
	// Registers the read-only properties and the flag/global accessors. Split
	// from RegisterLeveledListOperations so each registration function stays
	// below the cognitive-complexity threshold (registration is declarative).
	void RegisterLeveledListProperties(sol::usertype<LuaLeveledList>& a_type)
	{
		a_type["numEntries"] = sol::property(
			[](const LuaLeveledList& a_list) { return static_cast<lua_Integer>(ToLeveledList(a_list)->numEntries); });

		a_type["chanceNone"] = sol::property(
			[](const LuaLeveledList& a_list) { return static_cast<lua_Integer>(ToLeveledList(a_list)->chanceNone); },
			[](LuaLeveledList& a_list, lua_Integer a_value) {
				ToLeveledList(a_list)->chanceNone = static_cast<std::int8_t>(a_value);
			});

		a_type["chanceGlobal"] = sol::property(
			[](const LuaLeveledList& a_list) -> sol::optional<LuaForm> {
				if (auto* global = ToLeveledList(a_list)->chanceGlobal) {
					return LuaForm{ global };
				}
				return sol::nullopt;
			},

			[](LuaLeveledList& a_list, const sol::object& a_value) {
				if (a_value.is<sol::nil_t>()) {
					ToLeveledList(a_list)->chanceGlobal = nullptr;
					return;
				}
				auto* form = CheckFormValue(a_value);
				auto* global = form->As<RE::TESGlobal>();
				if (!global) {
					throw sol::error{ "chanceGlobal must be a global form or nil" };
				}
				ToLeveledList(a_list)->chanceGlobal = global;
			});

		a_type["calculateFromAllLevels"] = sol::property(
			[](const LuaLeveledList& a_list) {
				return HasFlag(a_list, RE::TESLeveledList::Flag::kCalculateFromAllLevelsLTOrEqPCLevel);
			},
			[](LuaLeveledList& a_list, bool a_value) {
				SetFlag(a_list, RE::TESLeveledList::Flag::kCalculateFromAllLevelsLTOrEqPCLevel, a_value);
			});

		a_type["calculateForEachItem"] = sol::property(
			[](const LuaLeveledList& a_list) {
				return HasFlag(a_list, RE::TESLeveledList::Flag::kCalculateForEachItemInCount);
			},
			[](LuaLeveledList& a_list, bool a_value) {
				SetFlag(a_list, RE::TESLeveledList::Flag::kCalculateForEachItemInCount, a_value);
			});

		a_type["useAll"] = sol::property(
			[](const LuaLeveledList& a_list) { return HasFlag(a_list, RE::TESLeveledList::Flag::kUseAll); },
			[](LuaLeveledList& a_list, bool a_value) { SetFlag(a_list, RE::TESLeveledList::Flag::kUseAll, a_value); });

		a_type["specialLoot"] = sol::property(
			[](const LuaLeveledList& a_list) { return HasFlag(a_list, RE::TESLeveledList::Flag::kSpecialLoot); },
			[](LuaLeveledList& a_list, bool a_value) {
				SetFlag(a_list, RE::TESLeveledList::Flag::kSpecialLoot, a_value);
			});

		a_type["entries"] = [](const LuaLeveledList& a_list, sol::this_state a_state) -> sol::object {
			sol::state_view lua(a_state);
			auto* list = ToLeveledList(a_list);

			sol::table result = lua.create_table(static_cast<int>(list->entries.size()), 0);
			lua_Integer index = 1;
			for (const auto& entry : list->entries) {
				result[index++] = PushEntry(lua, entry);
			}
			return result;
		};
	}

	void RegisterLeveledListOperations(sol::usertype<LuaLeveledList>& a_type)
	{
		a_type["add"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			const auto ref = ParseFormRef(a_args, "add");
			auto* form = ref.form;
			auto args =
				ParseLevelCount(VariadicArg(a_args, ref.consumed), VariadicArg(a_args, ref.consumed + 1), "add");

			if (InsertEntry(list, form, args.level, args.count, true)) {
				logger::debug("LuaPatcher: {:08X} added Form {:08X}", listForm->formID, form->formID);
			}
		};

		a_type["addIfAbsent"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			const auto ref = ParseFormRef(a_args, "addIfAbsent");
			auto* form = ref.form;
			auto args = ParseLevelCount(VariadicArg(a_args, ref.consumed), VariadicArg(a_args, ref.consumed + 1),
				"addIfAbsent");

			const bool added = InsertEntry(list, form, args.level, args.count, false);
			logger::debug("LuaPatcher: {:08X} addIfAbsent Form {:08X} added: {}", listForm->formID, form->formID,
				added);
		};

		a_type["remove"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			const auto ref = ParseFormRef(a_args, "remove");
			auto* delForm = ref.form;
			auto bounds = ParseBounds(VariadicArg(a_args, ref.consumed));

			const auto removed = EraseMatching(list, [&](const RE::LEVELED_OBJECT& x) {
				return x.form == delForm && MatchesBounds(bounds, x.level, x.count);
			});
			logger::debug("LuaPatcher: {:08X} removed {} entries of Form {:08X}", listForm->formID, removed,
				delForm->formID);
		};

		a_type["removeIf"] = [](LuaLeveledList& a_list, sol::this_state a_state, const sol::object& a_predicate) {
			// sol2 skips function-argument type checks in release builds;
			// validate explicitly so a non-function is a clean Lua error.
			if (!a_predicate.is<sol::function>() && !a_predicate.is<sol::protected_function>()) {
				throw sol::error{ "bad argument #2 to 'removeIf' (expected a function)" };
			}
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			sol::state_view lua(a_state);
			sol::protected_function predicate = a_predicate.as<sol::protected_function>();

			std::vector<RE::LEVELED_OBJECT> kept;
			kept.reserve(list->entries.size());
			for (const auto& entry : list->entries) {
				const sol::protected_function_result verdict = predicate(PushEntry(lua, entry));
				if (!verdict.valid()) {
					sol::error err = verdict;
					throw sol::error{ err.what() };
				}
				const sol::object value = verdict.get<sol::object>();
				// Lua truthiness: non-nil and non-false counts as "remove".
				const bool remove = value.valid() && (!value.is<bool>() || value.as<bool>());
				if (!remove) {
					kept.push_back(entry);
				}
			}

			if (kept.size() != list->entries.size()) {
				const auto removed = list->entries.size() - kept.size();
				SortByLevel(kept);
				WriteEntries(list, kept);
				logger::debug("LuaPatcher: {:08X} removeIf removed {} entries", listForm->formID, removed);
			}
		};
	}

	void RegisterLeveledListMutators(sol::usertype<LuaLeveledList>& a_type)
	{
		a_type["removeByKeyword"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			auto* form = ParseFormRef(a_args, "removeByKeyword").form;
			auto* keyword = form->As<RE::BGSKeyword>();
			if (!keyword) {
				throw sol::error{ "expected a keyword form" };
			}

			const auto removed = EraseMatching(list, [&](const RE::LEVELED_OBJECT& x) {
				const auto* keyForm = x.form->As<RE::BGSKeywordForm>();
				return keyForm && keyForm->HasKeyword(keyword);
			});
			logger::debug("LuaPatcher: {:08X} removed {} items with keyword {:08X}", listForm->formID, removed,
				keyword->formID);
		};

		a_type["replace"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			const auto fromRef = ParseFormRef(a_args, "replace");
			if (a_args.size() <= fromRef.consumed) {
				throw sol::error{ "bad argument to 'replace' (missing replacement form reference)" };
			}
			const auto toRef = ParseFormRef(
				sol::variadic_args(a_args.lua_state(), a_args.stack_index() + static_cast<int>(fromRef.consumed)),
				"replace");
			auto* from = fromRef.form;
			auto* to = toRef.form;

			auto entries = SnapshotEntries(list);
			std::size_t replaced = 0;
			for (auto& entry : entries) {
				if (entry.form == from) {
					entry.form = to;
					++replaced;
				}
			}

			if (replaced > 0) {
				SortByLevel(entries);
				WriteEntries(list, entries);
			}
			logger::debug("LuaPatcher: {:08X} replaced {:08X} with {:08X} ({} entries)", listForm->formID, from->formID,
				to->formID, replaced);
		};

		a_type["multiplyCount"] = [](LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			const auto ref = ParseFormRef(a_args, "multiplyCount");
			auto* form = ref.form;
			if (a_args.size() <= ref.consumed || !a_args.get<sol::object>(ref.consumed).is<double>()) {
				throw sol::error{ "bad argument #2 to 'multiplyCount' (expected a factor number)" };
			}
			const double a_factor = a_args.get<sol::object>(ref.consumed).as<double>();

			auto entries = SnapshotEntries(list);
			std::size_t changed = 0;
			for (auto& entry : entries) {
				if (entry.form == form) {
					entry.count = static_cast<std::uint16_t>(std::ceil(static_cast<double>(entry.count) * a_factor));
					++changed;
				}
			}

			if (changed > 0) {
				WriteEntries(list, entries);
			}
			logger::debug("LuaPatcher: {:08X} multiplied count of {:08X} by {} ({} entries)", listForm->formID,
				form->formID, a_factor, changed);
		};

		a_type["has"] = [](const LuaLeveledList& a_list, sol::variadic_args a_args) {
			auto* list = ToLeveledList(a_list);
			auto* form = ParseFormRef(a_args, "has").form;
			const bool found = std::ranges::any_of(list->entries,
				[form](const RE::LEVELED_OBJECT& x) { return x.form->formID == form->formID; });
			return found;
		};

		a_type["clear"] = [](LuaLeveledList& a_list) {
			auto* listForm = a_list.form;
			auto* list = ToLeveledList(a_list);
			Clear(list);
			logger::debug("LuaPatcher: {:08X} cleared", listForm->formID);
		};

		a_type["sort"] = [](LuaLeveledList& a_list) {
			auto* list = ToLeveledList(a_list);
			auto entries = SnapshotEntries(list);
			SortByLevel(entries);
			WriteEntries(list, entries);
		};
	}

	void RegisterLeveledList(sol::state_view& a_lua)
	{
		sol::usertype<LuaLeveledList> type = a_lua.new_usertype<LuaLeveledList>("LeveledList", sol::base_classes,
			sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaLeveledList>), sol::meta_function::to_string,
			[](const LuaLeveledList& a_list) { return fmt::format("LeveledList[{:08X}]", a_list.form->formID); });

		RegisterLeveledListProperties(type);
		RegisterLeveledListOperations(type);
		RegisterLeveledListMutators(type);

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["leveledList"] = &LeveledListGet;
		patcher["allLeveledItems"] = &AllLeveledItems;
		patcher["allLeveledCharacters"] = &AllLeveledCharacters;
		patcher["allLeveledSpells"] = &AllLeveledSpells;
		patcher["findLeveledListsContaining"] = &FindLeveledListsContaining;
	}
}