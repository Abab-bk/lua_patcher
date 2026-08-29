#pragma once

namespace LuaPatcher
{
	// Creates a Lua state, registers the API, and runs every .lua script found
	// under Data/SKSE/Plugins/LuaPatcher/Scripts (recursively, sorted, excluding
	// sibling configs "*_config.lua" which are loaded via lua_patcher.loadLua).
	// Safe to call from kDataLoaded; the state is destroyed afterwards.
	void RunScripts();
}