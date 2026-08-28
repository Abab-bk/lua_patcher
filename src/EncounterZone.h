#pragma once

#include <sol/sol.hpp>

namespace LuaPatcher
{
	// Registers the EncounterZone usertype (min/max level) and
	// `lua_patcher.allEncounterZones()`.
	void RegisterEncounterZone(sol::state_view& a_lua);
}