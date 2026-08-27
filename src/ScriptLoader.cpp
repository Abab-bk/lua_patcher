#include "ScriptLoader.h"

#include "LuaApi.h"
#include "PCH.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	constexpr std::string_view kScriptFolder = "Data/SKSE/Plugins/LuaPatcher/Scripts";

	bool HasLuaExtension(const std::filesystem::path& a_path)
	{
		const auto extension = a_path.extension().string();
		return extension.size() == 4 &&
		       (extension[1] == 'l' || extension[1] == 'L') &&
		       (extension[2] == 'u' || extension[2] == 'U') &&
		       (extension[3] == 'a' || extension[3] == 'A');
	}

	bool IsConfigFile(const std::filesystem::path& a_path)
	{
		// Config files are sibling "<Name>_Config.lua" next to the script;
		// they must not be executed as top-level scripts.
		// Mirrors examples/ layout: ModName.lua + ModName_Config.lua
		const auto filename = a_path.filename().string();
		if (filename.size() < 11) {
			return false;
		}
		std::string suffix = filename.substr(filename.size() - 11);
		for (char& c : suffix) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return suffix == "_config.lua";
	}

	// Message handler for lua_pcall: turns errors into a full stack traceback.
	int TracebackHandler(lua_State* a_state)
	{
		const char* msg = lua_tostring(a_state, 1);
		if (msg == nullptr) {
			if (luaL_callmeta(a_state, 1, "__tostring") && lua_type(a_state, -1) == LUA_TSTRING) {
				return 1;
			}
			msg = lua_pushfstring(a_state, "(error object is a %s value)", luaL_typename(a_state, 1));
		}
		luaL_traceback(a_state, a_state, msg, 1);
		return 1;
	}

	void RunScript(lua_State* a_state, const std::filesystem::path& a_path)
	{
		std::ifstream     file(a_path, std::ios::binary);
		const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (contents.empty()) {
			logger::warn("LuaPatcher: script '{}' is empty", a_path.generic_string());
			return;
		}

		const std::string chunkName = "@" + a_path.generic_string();
		if (luaL_loadbufferx(a_state, contents.data(), contents.size(), chunkName.c_str(), nullptr) != LUA_OK) {
			logger::error("LuaPatcher: failed to load script '{}': {}", a_path.generic_string(), lua_tostring(a_state, -1));
			lua_pop(a_state, 1);
			return;
		}

		lua_pushcfunction(a_state, TracebackHandler);
		lua_insert(a_state, -2);
		if (lua_pcall(a_state, 0, 0, -2) != LUA_OK) {
			logger::error("LuaPatcher: script '{}' failed: {}", a_path.generic_string(), lua_tostring(a_state, -1));
			lua_pop(a_state, 1);
		}
	}

	// Scripts are trusted config code, but the interpreter should not be able to
	// touch the machine out of the box: strip file-system and process functions.
	void RestrictLibraries(lua_State* a_state)
	{
		lua_getglobal(a_state, "os");
		if (lua_istable(a_state, -1)) {
			for (const char* name : { "execute", "exit", "remove", "rename", "tmpname" }) {
				lua_pushnil(a_state);
				lua_setfield(a_state, -2, name);
			}
		}
		lua_pop(a_state, 1);

		lua_pushnil(a_state);
		lua_setglobal(a_state, "io");

		lua_pushnil(a_state);
		lua_setglobal(a_state, "debug");
	}
}

namespace LuaPatcher
{
	void RunScripts()
	{
		namespace fs = std::filesystem;

		const fs::path base(kScriptFolder);
		if (!fs::exists(base) || !fs::is_directory(base)) {
			logger::info("LuaPatcher: script folder '{}' not found, nothing to run", kScriptFolder);
			return;
		}

		std::vector<fs::path> scripts;
		for (const auto& entry : fs::recursive_directory_iterator(base)) {
			if (entry.is_regular_file() && HasLuaExtension(entry.path()) && !IsConfigFile(entry.path())) {
				scripts.push_back(entry.path());
			}
		}
		std::sort(scripts.begin(), scripts.end());

		if (scripts.empty()) {
			logger::info("LuaPatcher: no .lua scripts found under '{}'", kScriptFolder);
			return;
		}

		lua_State* state = luaL_newstate();
		if (!state) {
			logger::error("LuaPatcher: failed to create Lua state");
			return;
		}
		luaL_openlibs(state);
		RestrictLibraries(state);
		RegisterApi(state);
		RegisterLeveledList(state);
		RegisterEquipment(state);
		RegisterMagic(state);

		for (const auto& script : scripts) {
			logger::info("LuaPatcher: running script '{}'", script.generic_string());
			RunScript(state, script);
		}

		lua_close(state);
	}
}