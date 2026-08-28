#include "EncounterZone.h"

#include "LuaApi.h"

#include <RE/B/BGSEncounterZone.h>
#include <RE/T/TESDataHandler.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace
{
	RE::BGSEncounterZone* ToZone(const LuaPatcher::LuaEncounterZone& a_zone)
	{
		return a_zone.form->As<RE::BGSEncounterZone>();
	}

	sol::object AllEncounterZones(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::BGSEncounterZone>();

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
	void RegisterEncounterZone(sol::state_view& a_lua)
	{
		sol::usertype<LuaEncounterZone> type = a_lua.new_usertype<LuaEncounterZone>("EncounterZone",
			sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaEncounterZone>), sol::meta_function::to_string,
			[](const LuaEncounterZone& a_zone) {
				return fmt::format("EncounterZone[{:08X}]", a_zone.form->GetFormID());
			});

		// -1 means "unset" (the engine falls back to the location/zone default).
		type["minLevel"] = sol::property(
			[](const LuaEncounterZone& a_zone) {
				return static_cast<lua_Integer>(ToZone(a_zone)->data.minLevel);
			},
			[](LuaEncounterZone& a_zone, lua_Integer a_value) {
				ToZone(a_zone)->data.minLevel = static_cast<std::int8_t>(std::clamp(a_value, lua_Integer{-1}, lua_Integer{100}));
			});

		type["maxLevel"] = sol::property(
			[](const LuaEncounterZone& a_zone) {
				return static_cast<lua_Integer>(ToZone(a_zone)->data.maxLevel);
			},
			[](LuaEncounterZone& a_zone, lua_Integer a_value) {
				ToZone(a_zone)->data.maxLevel = static_cast<std::int8_t>(std::clamp(a_value, lua_Integer{-1}, lua_Integer{100}));
			});

		// True when the zone carries explicit level bounds (otherwise the
		// engine derives them from the location).
		type["hasLevels"] = sol::property([](const LuaEncounterZone& a_zone) {
			const auto& data = ToZone(a_zone)->data;
			return data.minLevel >= 0 || data.maxLevel >= 0;
		});

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allEncounterZones"] = &AllEncounterZones;
	}
}