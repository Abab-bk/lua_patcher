#include "LuaApi.h"

#include <RE/T/TESContainer.h>
#include <RE/T/TESObjectCONT.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
	RE::TESContainer* ToContainer(const LuaPatcher::LuaContainer& a_form)
	{
		return a_form.form->As<RE::TESContainer>();
	}

	RE::TESBoundObject* CheckBoundObject(const sol::object& a_value)
	{
		auto* form = LuaPatcher::CheckForm(a_value);
		auto* bound = form->As<RE::TESBoundObject>();
		if (!bound) {
			throw sol::error{ "expected a bound object (item) form" };
		}
		return bound;
	}

	// Pushes an entry snapshot table { form, count }.
	sol::table PushEntry(sol::state_view a_lua, const RE::ContainerObject& a_entry)
	{
		sol::table row = a_lua.create_table(0, 2);
		row["form"] = LuaPatcher::PushForm(a_lua, a_entry.obj);
		row["count"] = a_entry.count;
		return row;
	}

	// Removes every entry of a_form (the engine API only deletes entries whose
	// count matches exactly, so remove one-by-one with each entry's own count).
	void RemoveAllOf(RE::TESContainer* a_container, RE::TESBoundObject* a_form)
	{
		for (std::uint32_t i = 0; i < a_container->numContainerObjects; ++i) {
			if (const auto entry = a_container->containerObjects[i]; entry && entry->obj == a_form) {
				a_container->RemoveObjectFromContainer(a_form, entry->count);
				// entry is gone; the next entry shifts into this index
				--i;
			}
		}
	}

	sol::object AllContainers(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::TESObjectCONT>();

		sol::table result = lua.create_table(static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			result[index++] = LuaPatcher::PushForm(lua, form);
		}
		return result;
	}
}

namespace LuaPatcher
{
	void RegisterContainer(sol::state_view& a_lua)
	{
		sol::usertype<LuaContainer> type = a_lua.new_usertype<LuaContainer>("Container", sol::base_classes,
			sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaContainer>), sol::meta_function::to_string,
			[](const LuaContainer& a_form) { return fmt::format("Container[{:08X}]", a_form.form->GetFormID()); });

		type["numObjects"] = sol::property([](const LuaContainer& a_form) {
			return static_cast<lua_Integer>(ToContainer(a_form)->numContainerObjects);
		});

		type["allowStolenItems"] =
			sol::property([](const LuaContainer& a_form) { return ToContainer(a_form)->allowStolenItems; },
				[](LuaContainer& a_form, bool a_value) { ToContainer(a_form)->allowStolenItems = a_value; });

		type["contents"] = [](const LuaContainer& a_form, sol::this_state a_state) -> sol::object {
			sol::state_view lua(a_state);
			auto* container = ToContainer(a_form);

			sol::table result = lua.create_table(static_cast<int>(container->numContainerObjects), 0);
			lua_Integer index = 1;
			for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
				if (const auto entry = container->containerObjects[i]) {
					result[index++] = PushEntry(lua, *entry);
				}
			}
			return result;
		};

		type["setContents"] = [](LuaContainer& a_form, const sol::object& a_list) {
			auto* container = ToContainer(a_form);
			if (!a_list.is<sol::table>()) {
				throw sol::error{ "expected an array of { form, count } entries" };
			}
			const auto rows = a_list.as<sol::table>();

			// Validate every entry first so a bad table cannot leave the
			// container half-mutated.
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
				planned.emplace_back(CheckBoundObject(*form), static_cast<std::int32_t>(n));
			}

			std::vector<RE::TESBoundObject*> removeOrder;
			for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
				if (const auto entry = container->containerObjects[i]) {
					removeOrder.push_back(entry->obj);
				}
			}
			for (auto* form : removeOrder) {
				RemoveAllOf(container, form);
			}
			for (const auto& [form, count] : planned) {
				container->AddObjectToContainer(form, count, nullptr);
			}
		};

		type["addItem"] = [](LuaContainer& a_form, const sol::object& a_formOrId, const sol::object& a_count) {
			auto* container = ToContainer(a_form);
			auto* bound = CheckBoundObject(a_formOrId);

			lua_Integer count = 1;
			if (!a_count.is<sol::nil_t>()) {
				if (!a_count.is<double>()) {
					throw sol::error{ "bad argument #2 to 'addItem' (expected a count number)" };
				}
				count = static_cast<lua_Integer>(a_count.as<double>());
			}
			if (count < 1) {
				throw sol::error{ "bad argument #2 to 'addItem' (count must be >= 1)" };
			}
			container->AddObjectToContainer(bound, static_cast<std::int32_t>(count), nullptr);
		};

		type["removeItem"] = [](LuaContainer& a_form, const sol::object& a_formOrId) {
			auto* container = ToContainer(a_form);
			auto* bound = CheckBoundObject(a_formOrId);
			const auto before = container->numContainerObjects;
			RemoveAllOf(container, bound);
			return container->numContainerObjects != before;
		};

		type["has"] = [](const LuaContainer& a_form, const sol::object& a_formOrId) {
			auto* container = ToContainer(a_form);
			auto* form = CheckForm(a_formOrId);
			for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
				if (const auto entry = container->containerObjects[i]; entry && entry->obj == form) {
					return true;
				}
			}
			return false;
		};

		type["clearContents"] = [](LuaContainer& a_form) {
			auto* container = ToContainer(a_form);
			std::vector<RE::TESBoundObject*> removeOrder;
			for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
				if (const auto entry = container->containerObjects[i]) {
					removeOrder.push_back(entry->obj);
				}
			}
			for (auto* form : removeOrder) {
				RemoveAllOf(container, form);
			}
		};

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allContainers"] = &AllContainers;
	}
}