#include "Crafting.h"

#include "ContainerOps.h"
#include "LuaApi.h"

#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSKeyword.h>
#include <RE/T/TESDataHandler.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace
{
	RE::BGSConstructibleObject* ToRecipe(const LuaPatcher::LuaConstructibleObject& a_form)
	{
		return a_form.form->As<RE::BGSConstructibleObject>();
	}

	sol::object AllConstructibleObjects(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::BGSConstructibleObject>();

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
	void RegisterCrafting(sol::state_view& a_lua)
	{
		sol::usertype<LuaConstructibleObject> type = a_lua.new_usertype<LuaConstructibleObject>(
			"ConstructibleObject", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaConstructibleObject>), sol::meta_function::to_string,
			[](const LuaConstructibleObject& a_form) {
				return fmt::format("ConstructibleObject[{:08X}]", a_form.form->GetFormID());
			},

			// The output item (CNAM); nil on a malformed recipe. Pushed
			// through PushForm so typed properties (damage, armorRating,
			// ...) work on it.
			"createdItem",
			sol::property(
				[](const LuaConstructibleObject& a_form, sol::this_state a_state) -> sol::object {
					sol::state_view lua(a_state);
					return LuaPatcher::PushForm(lua, ToRecipe(a_form)->createdItem);
				},
				[](LuaConstructibleObject& a_form, const sol::object& a_value) {
					ToRecipe(a_form)->createdItem = LuaPatcher::CheckFormValue(a_value);
				}),

			// The crafting-station keyword (BNAM), e.g. CraftingSmithingForge.
			"benchKeyword",
			sol::property(
				[](const LuaConstructibleObject& a_form) -> sol::optional<LuaForm> {
					auto* recipe = ToRecipe(a_form);
					if (recipe->benchKeyword) {
						return LuaForm{ recipe->benchKeyword };
					}
					return sol::nullopt;
				},
				[](LuaConstructibleObject& a_form, const sol::object& a_value) {
					auto* keyword = LuaPatcher::CheckFormValue(a_value)->As<RE::BGSKeyword>();
					if (!keyword) {
						throw sol::error{ "benchKeyword must be a keyword form" };
					}
					ToRecipe(a_form)->benchKeyword = keyword;
				}),

			// How many copies of the output one craft produces (NAM1).
			"numConstructed",
			sol::property(
				[](const LuaConstructibleObject& a_form) {
					return static_cast<lua_Integer>(ToRecipe(a_form)->data.numConstructed);
				},
				[](LuaConstructibleObject& a_form, lua_Integer a_value) {
					ToRecipe(a_form)->data.numConstructed =
						static_cast<std::uint16_t>(std::clamp(a_value, lua_Integer{ 1 }, lua_Integer{ 1000 }));
				}),

			"numRequiredItems", sol::property([](const LuaConstructibleObject& a_form) {
				return static_cast<lua_Integer>(ToRecipe(a_form)->requiredItems.numContainerObjects);
			}),

			"requiredItems",
			[](const LuaConstructibleObject& a_form, sol::this_state a_state) -> sol::object {
				sol::state_view lua(a_state);
				auto& required = ToRecipe(a_form)->requiredItems;

				sol::table result = lua.create_table(static_cast<int>(required.numContainerObjects), 0);
				lua_Integer index = 1;
				for (std::uint32_t i = 0; i < required.numContainerObjects; ++i) {
					if (const auto entry = required.containerObjects[i]) {
						result[index++] = ContainerOps::PushEntry(lua, *entry);
					}
				}
				return result;
			},

			"setRequiredItems",
			[](LuaConstructibleObject& a_form, const sol::object& a_list) {
				auto& required = ToRecipe(a_form)->requiredItems;
				ContainerOps::ReplaceContents(&required, ContainerOps::ParseEntries(a_list));
			},

			"addRequiredItem",
			[](LuaConstructibleObject& a_form, sol::variadic_args a_args) {
				auto& required = ToRecipe(a_form)->requiredItems;
				const auto ref = LuaPatcher::ParseFormRef(a_args, "addRequiredItem");
				auto* bound = ContainerOps::CheckBoundObject(ref.form);

				lua_Integer count = 1;
				if (a_args.size() > ref.consumed && !a_args.get<sol::object>(ref.consumed).is<sol::nil_t>()) {
					if (!a_args.get<sol::object>(ref.consumed).is<double>()) {
						throw sol::error{ "bad argument to 'addRequiredItem' (expected a count number)" };
					}
					count = static_cast<lua_Integer>(a_args.get<sol::object>(ref.consumed).as<double>());
				}
				if (count < 1) {
					throw sol::error{ "bad argument to 'addRequiredItem' (count must be >= 1)" };
				}
				required.AddObjectToContainer(bound, static_cast<std::int32_t>(count), nullptr);
			},

			"removeRequiredItem",
			[](LuaConstructibleObject& a_form, sol::variadic_args a_args) {
				auto& required = ToRecipe(a_form)->requiredItems;
				auto* bound =
					ContainerOps::CheckBoundObject(LuaPatcher::ParseFormRef(a_args, "removeRequiredItem").form);
				const auto before = required.numContainerObjects;
				ContainerOps::RemoveAllOf(&required, bound);
				return required.numContainerObjects != before;
			},

			"hasRequiredItem",
			[](const LuaConstructibleObject& a_form, sol::variadic_args a_args) {
				auto& required = ToRecipe(a_form)->requiredItems;
				auto* form = LuaPatcher::ParseFormRef(a_args, "hasRequiredItem").form;
				for (std::uint32_t i = 0; i < required.numContainerObjects; ++i) {
					if (const auto entry = required.containerObjects[i]; entry && entry->obj == form) {
						return true;
					}
				}
				return false;
			},

			"clearRequiredItems",
			[](LuaConstructibleObject& a_form) {
				auto& required = ToRecipe(a_form)->requiredItems;
				for (auto* form : ContainerOps::SnapshotForms(&required)) {
					ContainerOps::RemoveAllOf(&required, form);
				}
			});

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allConstructibleObjects"] = &AllConstructibleObjects;
	}
}