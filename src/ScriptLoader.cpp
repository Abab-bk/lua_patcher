#include "ScriptLoader.h"

#include "LuaApi.h"
#include "PCH.h"

#include <algorithm>
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
		const std::string_view sv(extension);
		return sv == ".lua" || sv == ".LUA";
	}

	bool IsConfigFile(const std::filesystem::path& a_path)
	{
		// Config files are sibling "<Name>_config.lua" next to the script;
		// they must not be executed as top-level scripts.
		// Mirrors examples/ layout: ModName.lua + ModName_config.lua
		const auto filename = a_path.filename().string();
		return filename.ends_with("_config.lua");
	}

	void RunScript(sol::state_view a_lua, const std::filesystem::path& a_path)
	{
		std::ifstream file(a_path, std::ios::binary);

		auto contents = std::string(
			std::istreambuf_iterator<char>{ file },
			std::istreambuf_iterator<char>{});

		if (contents.empty()) {
			logger::warn("LuaPatcher: script '{}' is empty", a_path.generic_string());
			return;
		}

		const auto chunkName = "@" + a_path.generic_string();

		// sol2's protected call attaches a full stack traceback to the error message.
		sol::protected_function_result result = a_lua.safe_script(contents, sol::script_pass_on_error, chunkName, sol::load_mode::any);
		if (!result.valid()) {
			sol::error err = result;
			logger::error("LuaPatcher: script '{}' failed: {}", a_path.generic_string(), err.what());
		}
	}

	// Scripts are trusted config code, but the interpreter should not be able to
	// touch the machine out of the box: strip file-system and process functions.
	void RestrictLibraries(sol::state_view a_lua)
	{
		a_lua["os"]["execute"] = sol::nil;
		a_lua["os"]["exit"] = sol::nil;
		a_lua["os"]["remove"] = sol::nil;
		a_lua["os"]["rename"] = sol::nil;
		a_lua["os"]["tmpname"] = sol::nil;

		a_lua["io"] = sol::nil;
		a_lua["debug"] = sol::nil;
	}

	// sol2 prints C++ exceptions to stderr before converting them to Lua errors;
	// route that noise away (pcall'd script errors should only surface in the log).
	int QuietExceptionHandler(lua_State* a_state, sol::optional<const std::exception&> a_exception, sol::string_view a_what)
	{
		(void)a_exception;
		lua_pushlstring(a_state, a_what.data(), a_what.size());
		return 1;
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
		std::ranges::sort(scripts);

		if (scripts.empty()) {
			logger::info("LuaPatcher: no .lua scripts found under '{}'", kScriptFolder);
			return;
		}

		sol::state lua;
		lua.set_exception_handler(QuietExceptionHandler);
		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine, sol::lib::string,
			sol::lib::os, sol::lib::math, sol::lib::table, sol::lib::utf8, sol::lib::io, sol::lib::debug);

		RestrictLibraries(lua);
		RegisterApi(lua);
		RegisterLeveledList(lua);
		RegisterEquipment(lua);
		RegisterMagic(lua);

		for (const auto& script : scripts) {
			logger::info("LuaPatcher: running script '{}'", script.generic_string());
			RunScript(lua, script);
		}
	}
}