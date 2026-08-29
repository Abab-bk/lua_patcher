#include "LuaApi.h"

#include "ContainerOps.h"

#include <RE/T/TESContainer.h>
#include <RE/T/TESObjectCONT.h>

#include <cstdint>
#include <string>

namespace
{
	RE::TESContainer* ToContainer(const LuaPatcher::LuaContainer& a_form)
	{
		return a_form.form->As<RE::TESContainer>();
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
					result[index++] = ContainerOps::PushEntry(lua, *entry);
				}
			}
			return result;
		};

		type["setContents"] = [](LuaContainer& a_form, const sol::object& a_list) {
			auto* container = ToContainer(a_form);
			ContainerOps::ReplaceContents(container, ContainerOps::ParseEntries(a_list));
		};

		type["addItem"] = [](LuaContainer& a_form, sol::variadic_args a_args) {
			auto* container = ToContainer(a_form);
			const auto ref = LuaPatcher::ParseFormRef(a_args, "addItem");
			auto* bound = ContainerOps::CheckBoundObject(ref.form);

			lua_Integer count = 1;
			if (a_args.size() > ref.consumed && !a_args.get<sol::object>(ref.consumed).is<sol::nil_t>()) {
				if (!a_args.get<sol::object>(ref.consumed).is<double>()) {
					throw sol::error{ "bad argument to 'addItem' (expected a count number)" };
				}
				count = static_cast<lua_Integer>(a_args.get<sol::object>(ref.consumed).as<double>());
			}
			if (count < 1) {
				throw sol::error{ "bad argument to 'addItem' (count must be >= 1)" };
			}
			container->AddObjectToContainer(bound, static_cast<std::int32_t>(count), nullptr);
		};

		type["removeItem"] = [](LuaContainer& a_form, sol::variadic_args a_args) {
			auto* container = ToContainer(a_form);
			auto* bound = ContainerOps::CheckBoundObject(LuaPatcher::ParseFormRef(a_args, "removeItem").form);
			const auto before = container->numContainerObjects;
			ContainerOps::RemoveAllOf(container, bound);
			return container->numContainerObjects != before;
		};

		type["has"] = [](const LuaContainer& a_form, sol::variadic_args a_args) {
			auto* container = ToContainer(a_form);
			auto* form = LuaPatcher::ParseFormRef(a_args, "has").form;
			for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
				if (const auto entry = container->containerObjects[i]; entry && entry->obj == form) {
					return true;
				}
			}
			return false;
		};

		type["clearContents"] = [](LuaContainer& a_form) {
			auto* container = ToContainer(a_form);
			for (auto* form : ContainerOps::SnapshotForms(container)) {
				ContainerOps::RemoveAllOf(container, form);
			}
		};

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allContainers"] = &AllContainers;
	}
}