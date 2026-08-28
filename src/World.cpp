#include "LuaApi.h"

#include <RE/T/TESGlobal.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace
{
	RE::TESGlobal* ToGlobal(const LuaPatcher::LuaGlobal& a_form) { return a_form.form->As<RE::TESGlobal>(); }

	std::string_view GlobalTypeName(RE::TESGlobal::Type a_type)
	{
		switch (a_type) {
		case RE::TESGlobal::Type::kFloat:
			return "Float";
		case RE::TESGlobal::Type::kLong:
			return "Long";
		case RE::TESGlobal::Type::kShort:
			return "Short";
		default:
			return "Other";
		}
	}

	sol::object AllGlobals(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::TESGlobal>();

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
	void RegisterWorld(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaGlobal>(
			"Global", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaGlobal>), sol::meta_function::to_string,
			[](const LuaGlobal& a_form) { return fmt::format("Global[{:08X}]", a_form.form->GetFormID()); }, "value",
			sol::property([](const LuaGlobal& a_form) { return static_cast<double>(ToGlobal(a_form)->value); },
				[](LuaGlobal& a_form, double a_value) { ToGlobal(a_form)->value = static_cast<float>(a_value); }),
			"globalType", sol::property([](const LuaGlobal& a_form) {
				return std::string(
					GlobalTypeName(static_cast<RE::TESGlobal::Type>(ToGlobal(a_form)->type.underlying())));
			}));

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allGlobals"] = &AllGlobals;
	}
}