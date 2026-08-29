#pragma once

#include <sol/sol.hpp>

namespace LuaPatcher
{
	// Registers the ConstructibleObject usertype (recipe output / required
	// items / bench keyword) and `lua_patcher.allConstructibleObjects()`.
	void RegisterCrafting(sol::state_view& a_lua);
}