#pragma once
// Internal helpers shared by the Container and ConstructibleObject
// registrations: reading and rewriting a TESContainer's entry list (container
// contents / recipe required items). Not part of the public API surface;
// docs/API.md is generated from the *.cpp registration calls only.

#include "LuaApi.h"

#include <RE/T/TESContainer.h>
#include <RE/T/TESForm.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace LuaPatcher::ContainerOps
{
	// Validates a form and returns it as a bound object (entries must be
	// placeable items); raises otherwise.
	inline RE::TESBoundObject* CheckBoundObject(RE::TESForm* a_form)
	{
		auto* bound = a_form->As<RE::TESBoundObject>();
		if (!bound) {
			throw sol::error{ "expected a bound object (item) form" };
		}
		return bound;
	}

	// Pushes an entry snapshot table { form, count }.
	inline sol::table PushEntry(sol::state_view a_lua, const RE::ContainerObject& a_entry)
	{
		sol::table row = a_lua.create_table(0, 2);
		row["form"] = LuaPatcher::PushForm(a_lua, a_entry.obj);
		row["count"] = a_entry.count;
		return row;
	}

	// Removes every entry of a_form (the engine API only deletes entries whose
	// count matches exactly, so remove one-by-one with each entry's own count).
	inline void RemoveAllOf(RE::TESContainer* a_container, RE::TESBoundObject* a_form)
	{
		for (std::uint32_t i = 0; i < a_container->numContainerObjects; ++i) {
			if (const auto entry = a_container->containerObjects[i]; entry && entry->obj == a_form) {
				a_container->RemoveObjectFromContainer(a_form, entry->count);
				// entry is gone; the next entry shifts into this index
				--i;
			}
		}
	}

	// Validates a Lua array of { form, count } entry tables and returns the
	// planned (form, count) pairs. Every entry is validated first so a bad
	// table cannot leave the container half-mutated.
	inline std::vector<std::pair<RE::TESBoundObject*, std::int32_t>> ParseEntries(const sol::object& a_list)
	{
		if (!a_list.is<sol::table>()) {
			throw sol::error{ "expected an array of { form, count } entries" };
		}
		const auto rows = a_list.as<sol::table>();

		std::vector<std::pair<RE::TESBoundObject*, std::int32_t>> planned;
		planned.reserve(rows.size());
		for (std::size_t i = 1; i <= rows.size(); ++i) {
			const sol::object row = rows.get<sol::object>(static_cast<lua_Integer>(i));
			if (!row.is<sol::table>()) {
				throw sol::error{ "expected { form = formOrId, count = n } entry tables" };
			}
			const auto entry = row.as<sol::table>();
			const auto form = entry.get<sol::optional<sol::object>>("form");
			const auto count = entry.get<sol::optional<sol::object>>("count");
			if (!form) {
				throw sol::error{ "missing 'form' in container entry" };
			}
			lua_Integer n = 1;
			if (count) {
				if (!count->is<double>()) {
					throw sol::error{ "bad container entry value 'count' (expected a number)" };
				}
				n = static_cast<lua_Integer>(count->as<double>());
			}
			planned.emplace_back(CheckBoundObject(LuaPatcher::CheckFormValue(*form)), static_cast<std::int32_t>(n));
		}
		return planned;
	}

	// The forms currently in the entry list, in order (skipping null entries).
	inline std::vector<RE::TESBoundObject*> SnapshotForms(RE::TESContainer* a_container)
	{
		std::vector<RE::TESBoundObject*> forms;
		for (std::uint32_t i = 0; i < a_container->numContainerObjects; ++i) {
			if (const auto entry = a_container->containerObjects[i]) {
				// The mock stores entries as TESForm* (recipe required items
				// may reference FormLists); the real container API types them
				// as bound objects and the engine stores non-bound refs in
				// those slots as-is, so mirror that cast.
				forms.push_back(static_cast<RE::TESBoundObject*>(entry->obj));
			}
		}
		return forms;
	}

	// Replaces the whole entry list with the planned entries: removes the
	// current entries, then adds the planned ones in order.
	inline void ReplaceContents(RE::TESContainer* a_container,
		const std::vector<std::pair<RE::TESBoundObject*, std::int32_t>>& a_planned)
	{
		for (auto* form : SnapshotForms(a_container)) {
			RemoveAllOf(a_container, form);
		}
		for (const auto& [form, count] : a_planned) {
			a_container->AddObjectToContainer(form, count, nullptr);
		}
	}
}  // namespace LuaPatcher::ContainerOps