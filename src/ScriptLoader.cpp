#include "ScriptLoader.h"

#include "LuaApi.h"
#include "Utils.h"

#include <algorithm>
#include <filesystem>
#include <sol/state_view.hpp>
#include <string_view>

namespace
{
	constexpr std::string_view kScriptFolder = "Data/SKSE/Plugins/LuaPatcher/Scripts";

	// Header of a script file, scanned for a "-- priority: N" declaration.
	constexpr std::size_t kPriorityScanBytes = 512;

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

	// Scripts declare an execution order with a "-- priority: N" comment;
	// missing declarations default to 0 and run first.
	int ReadScriptPriority(const std::filesystem::path& a_path)
	{
		std::ifstream in(a_path, std::ios::binary);
		std::array<char, kPriorityScanBytes> head{};
		in.read(head.data(), static_cast<std::streamsize>(head.size() - 1));
		return ExampleMod::ParseScriptPriority(std::string_view(head.data()));
	}

	void RunScript(sol::state_view& a_lua, const std::filesystem::path& a_path)
	{
		std::ifstream file(a_path, std::ios::binary);

		auto contents = std::string(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});

		if (contents.empty()) {
			logger::warn("LuaPatcher: script '{}' is empty", a_path.generic_string());
			return;
		}

		const auto chunkName = "@" + a_path.generic_string();

		// sol2's protected call attaches a full stack traceback to the error message.
		auto result = a_lua.safe_script(contents, sol::script_pass_on_error, chunkName, sol::load_mode::any);

		if (!result.valid()) {
			sol::error err = result;
			logger::error("LuaPatcher: script '{}' failed: {}", a_path.generic_string(), err.what());
		}
	}

	// Scripts are trusted config code, but the interpreter should not be able to
	// touch the machine out of the box: strip file-system and process functions.
	void RestrictLibraries(sol::state_view& a_lua)
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
	int QuietExceptionHandler(lua_State* a_state, sol::optional<const std::exception&> a_exception,
		sol::string_view a_what)
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

		// Scripts declare an execution order via "-- priority: N" comments;
		// missing declarations default to 0 and run first. Path order breaks
		// ties, keeping the pre-priority alphabetical behavior for equal
		// priorities.
		std::vector<std::pair<int, fs::path>> scripts;
		for (const auto& entry : fs::recursive_directory_iterator(base)) {
			if (entry.is_regular_file() && HasLuaExtension(entry.path()) && !IsConfigFile(entry.path())) {
				scripts.emplace_back(ReadScriptPriority(entry.path()), entry.path());
			}
		}
		std::ranges::sort(scripts,
			[](const auto& a, const auto& b) { return a.first != b.first ? a.first < b.first : a.second < b.second; });

		if (scripts.empty()) {
			logger::info("LuaPatcher: no .lua scripts found under '{}'", kScriptFolder);
			return;
		}

		sol::state lua;
		lua.set_exception_handler(QuietExceptionHandler);
		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine, sol::lib::string, sol::lib::os,
			sol::lib::math, sol::lib::table, sol::lib::utf8, sol::lib::io, sol::lib::debug);

		RestrictLibraries(lua);
		RegisterApi(lua);
		RegisterLeveledList(lua);
		RegisterEquipment(lua);
		RegisterMagic(lua);
		RegisterFormList(lua);

		for (const auto& [priority, script] : scripts) {
			logger::info("LuaPatcher: running script '{}' (priority {})", script.generic_string(), priority);
			RunScript(lua, script);
		}
	}
}