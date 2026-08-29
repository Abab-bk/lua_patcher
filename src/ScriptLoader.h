#pragma once

namespace LuaPatcher
{
	// Creates a Lua state, registers the API, and runs every .lua script found
	// under Data/SKSE/Plugins/LuaPatcher/Scripts (recursively, sorted, excluding
	// sibling configs "*_config.lua" loaded via lua_patcher.loadLua and "_"-prefixed
	// module files loaded via require(); package.path is prefixed with the script
	// folder so modules resolve next to the scripts that require them).
	// Safe to call from kDataLoaded; the state is destroyed afterwards.
	void RunScripts();
}