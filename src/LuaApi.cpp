#include "LuaApi.h"

#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESEnchantableForm.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
	// Reverse of the game's editorID -> form lookup table.
	//
	// The virtual GetFormEditorID() returns "" for most form classes (only a few,
	// e.g. BGSKeyword, synthesize a member-based ID), so the actual editor IDs
	// must come from the game's own table that LookupByEditorID uses. Built once
	// per data handler: the table is fully populated by plugin loading and
	// read-only afterwards, so this cache has the same lifetime assumptions as
	// the leveled list index (single-threaded at kDataLoaded).
	const std::unordered_map<RE::FormID, std::string>& EditorIdCache()
	{
		static RE::TESDataHandler* owner = nullptr;
		static std::unordered_map<RE::FormID, std::string> cache;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (owner != dataHandler) {
			cache.clear();
			owner = dataHandler;
			const auto& [map, lock] = RE::TESForm::GetAllFormsByEditorID();
			if (map) {
				const RE::BSReadLockGuard l{ lock };
				for (const auto& [editorID, form] : *map) {
					cache[form->formID] = editorID;
				}
			}
		}
		return cache;
	}

	std::string FormToIdentifier(RE::TESForm* a_form)
	{
		if (auto* file = a_form->GetFile(0)) {
			return fmt::format("{}|{:06X}", file->GetFilename(), a_form->GetLocalFormID());
		}

		const auto& cache = EditorIdCache();
		if (const auto it = cache.find(a_form->GetFormID()); it != cache.end()) {
			return it->second;
		}

		if (const char* editorID = a_form->GetFormEditorID(); editorID && *editorID) {
			return editorID;
		}

		return fmt::format("{:08X}", a_form->GetFormID());
	}

	// ---- print / logging (plain lua_CFunctions so print() can tostring any value) ----

	void ConcatArgs(lua_State* a_state, std::string& a_out)
	{
		const int n = lua_gettop(a_state);
		for (int i = 1; i <= n; ++i) {
			size_t len = 0;
			const char* s = luaL_tolstring(a_state, i, &len);
			if (i > 1) {
				a_out += '\t';
			}
			a_out.append(s, len);
			lua_pop(a_state, 1);
		}
	}

	int Log(lua_State* a_state)
	{
		std::string msg;
		ConcatArgs(a_state, msg);
		logger::info("LuaPatcher: {}", msg);
		return 0;
	}

	int Warn(lua_State* a_state)
	{
		std::string msg;
		ConcatArgs(a_state, msg);
		logger::warn("LuaPatcher: {}", msg);
		return 0;
	}

	int Error(lua_State* a_state)
	{
		std::string msg;
		ConcatArgs(a_state, msg);
		logger::error("LuaPatcher: {}", msg);
		return 0;
	}

	// ---- lua_patcher core functions ----

	sol::object GetFormById(sol::this_state a_state, sol::variadic_args a_args)
	{
		sol::state_view lua(a_state);
		return LuaPatcher::PushForm(lua, LuaPatcher::LookupFormRef(a_args, "getForm"));
	}

	bool IsPluginInstalled(const sol::object& a_name)
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto name = a_name.as<std::string>();
		return dataHandler && (dataHandler->LookupLoadedModByName(name) != nullptr ||
								  dataHandler->LookupLoadedLightModByName(name) != nullptr);
	}

	// Whitelisted file loader: loads Data/SKSE/Plugins/LuaPatcher/Scripts/<name>
// exactly as given ("EverythingRandomizer_protection.lua",
// "MyMod_config.lua", ...). Returns the chunk's return value (usually a
// table) or nil on miss/error.
// Security: name must be a plain file name, no path separators or "..".
	sol::object LoadLua(sol::this_state a_state, const sol::object& a_name)
	{
		sol::state_view lua(a_state);

		if (!a_name.is<std::string>()) {
			throw sol::error{ "bad argument #1 to 'loadLua' (file name must be a string)" };
		}

		const std::string nameStr = a_name.as<std::string>();
		if (nameStr.empty()) {
			throw sol::error{ "bad argument #1 to 'loadLua' (file name must be non-empty)" };
		}
		if (nameStr.contains("..") || nameStr.contains('/') || nameStr.contains('\\') || nameStr.contains(':')) {
			throw sol::error{
				"bad argument #1 to 'loadLua' (file name must be a plain file name without path separators or '..')"
			};
		}

		namespace fs = std::filesystem;
		const fs::path scriptDir = "Data/SKSE/Plugins/LuaPatcher/Scripts";
		const fs::path file = scriptDir / nameStr;
		if (!fs::exists(file)) {
			logger::info("LuaPatcher: '{}' not found in '{}', returning nil", nameStr, scriptDir.generic_string());
			return sol::nil;
		}

		std::ifstream in(file, std::ios::binary);
		if (!in) {
			logger::warn("LuaPatcher: '{}' found but could not be opened: {}", nameStr, file.generic_string());
			return sol::nil;
		}

		std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (contents.empty()) {
			logger::warn("LuaPatcher: '{}' is empty: {}", nameStr, file.generic_string());
			return sol::nil;
		}

		const std::string chunkName = "@" + file.generic_string();
		sol::protected_function_result result =
			lua.safe_script(contents, sol::script_pass_on_error, chunkName, sol::load_mode::any);
		if (!result.valid()) {
			sol::error err = result;
			logger::error("LuaPatcher: failed to load '{}': {}", file.generic_string(), err.what());
			return sol::nil;
		}

		// Chunks returning nothing are treated as nil (lua_pcall(0, 1, 0) semantics).
		logger::info("LuaPatcher: loaded '{}'", file.generic_string());
		if (result.return_count() == 0) {
			return sol::nil;
		}
		return result.get<sol::object>();
	}
}

namespace LuaPatcher
{
	RE::TESForm* LookupFormByPluginFormId(const std::string_view& a_plugin, const std::string_view& a_formId)
	{
		static std::unordered_map<std::string, RE::TESForm*> cache;

		const std::string key = fmt::format("{}|{}", a_plugin, a_formId);
		if (const auto it = cache.find(key); it != cache.end()) {
			return it->second;
		}

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		RE::TESForm* result = nullptr;
		if (dataHandler) {
			try {
				auto rawFormID = static_cast<std::uint32_t>(std::stoul(std::string(a_formId), nullptr, 16));

				const auto* mod = dataHandler->LookupModByName(a_plugin);
				if (mod && mod->IsLight()) {
					rawFormID &= 0xFFF;
				} else {
					rawFormID &= 0xFFFFFF;
				}

				result = dataHandler->LookupForm(rawFormID, a_plugin);
			} catch (const std::exception& e) {
				logger::warn("LuaPatcher: invalid formID '{}|{}': {}", a_plugin, a_formId, e.what());
			}
		}

		cache.emplace(key, result);
		return result;
	}

	RE::TESForm* LookupFormByEditorId(const std::string_view& a_editorId)
	{
		static std::unordered_map<std::string, RE::TESForm*> cache;

		const std::string key(a_editorId);
		if (const auto it = cache.find(key); it != cache.end()) {
			return it->second;
		}

		RE::TESForm* result = nullptr;
		if (RE::TESDataHandler::GetSingleton()) {
			result = RE::TESForm::LookupByEditorID(a_editorId);
		}

		cache.emplace(key, result);
		return result;
	}

	// Reads a string argument, tolerating a nil for missing values.
	std::string_view ArgString(const sol::object& a_value)
	{
		if (!a_value.is<std::string>()) {
			throw sol::error{ "expected a string" };
		}
		return a_value.as<std::string_view>();
	}

	RE::TESForm* ToAnyForm(const sol::object& a_value)
	{
		if (a_value.is<LuaForm>()) {
			return a_value.as<LuaForm>().form;
		}
		throw sol::error{ "expected a form identifier string or a Form" };
	}

	RE::TESForm* LookupFormValue(const sol::object& a_value)
	{
		if (a_value.is<LuaForm>()) {
			return ToAnyForm(a_value);
		}

		if (a_value.is<std::string>()) {
			return LookupFormByEditorId(a_value.as<std::string_view>());
		}

		if (a_value.is<sol::table>()) {
			const auto pair = a_value.as<sol::table>();
			const auto plugin = pair.get<sol::optional<sol::object>>(1);
			const auto formId = pair.get<sol::optional<sol::object>>(2);
			if (plugin && formId && plugin->is<std::string>() && formId->is<std::string>()) {
				return LookupFormByPluginFormId(plugin->as<std::string_view>(), formId->as<std::string_view>());
			}
			return nullptr;
		}

		return nullptr;
	}

	RE::TESForm* CheckFormValue(const sol::object& a_value)
	{
		auto* form = LookupFormValue(a_value);
		if (!form) {
			throw sol::error{ "form reference does not resolve to a loaded form" };
		}
		return form;
	}

	FormRef ParseFormRef(const sol::variadic_args& a_args, const std::string_view a_method)
	{
		if (a_args.size() < 1) {
			throw sol::error{ fmt::format("bad argument #1 to '{}' (missing form reference)", a_method) };
		}

		const sol::object first = a_args.get<sol::object>(0);
		if (first.is<std::string>()) {
			if (a_args.size() >= 2 && a_args.get<sol::object>(1).is<std::string>()) {
				const auto plugin = first.as<std::string_view>();
				const auto formId = a_args.get<sol::object>(1).as<std::string_view>();
				auto* form = LookupFormByPluginFormId(plugin, formId);
				if (!form) {
					throw sol::error{
						fmt::format("bad argument #1-2 to '{}' (form '{}|{}' not found)", a_method, plugin, formId)
					};
				}
				return { form, 2 };
			}

			auto* form = LookupFormByEditorId(first.as<std::string_view>());
			if (!form) {
				throw sol::error{
					fmt::format("bad argument #1 to '{}' (no form with editorID '{}' is loaded)", a_method,
						first.as<std::string_view>())
				};
			}
			return { form, 1 };
		}

		return { ToAnyForm(first), 1 };
	}

	// Lenient form reference lookup for getForm: mirrors ParseFormRef but
	// returns nullptr on a miss instead of raising.
	RE::TESForm* LookupFormRef(const sol::variadic_args& a_args, const std::string_view a_method)
	{
		if (a_args.size() < 1) {
			throw sol::error{ fmt::format("bad argument #1 to '{}' (missing form reference)", a_method) };
		}

		const sol::object first = a_args.get<sol::object>(0);
		if (first.is<std::string>()) {
			if (a_args.size() >= 2 && a_args.get<sol::object>(1).is<std::string>()) {
				return LookupFormByPluginFormId(first.as<std::string_view>(), a_args.get<sol::object>(1).as<std::string_view>());
			}
			return LookupFormByEditorId(first.as<std::string_view>());
		}

		if (!first.is<LuaForm>()) {
			throw sol::error{ fmt::format("bad argument #1 to '{}' (expected a form reference)", a_method) };
		}

		return ToAnyForm(first);
	}

	sol::object PushForm(sol::state_view& a_lua, RE::TESForm* a_form)
	{
		if (!a_form) {
			return sol::nil;
		}

		switch (a_form->GetFormType()) {
		case RE::FormType::LeveledItem:
		case RE::FormType::LeveledNPC:
		case RE::FormType::LeveledSpell:
			return sol::make_object(a_lua, LuaLeveledList{ a_form });
		case RE::FormType::Weapon:
			return sol::make_object(a_lua, LuaWeapon{ a_form });
		case RE::FormType::Armor:
			return sol::make_object(a_lua, LuaArmor{ a_form });
		case RE::FormType::Spell:
			return sol::make_object(a_lua, LuaSpell{ a_form });
		case RE::FormType::MagicEffect:
			return sol::make_object(a_lua, LuaMagicEffect{ a_form });
		case RE::FormType::FormList:
			return sol::make_object(a_lua, LuaFormList{ a_form });
		case RE::FormType::Ingredient:
			return sol::make_object(a_lua, LuaIngredient{ a_form });
		case RE::FormType::AlchemyItem:
			return sol::make_object(a_lua, LuaPotion{ a_form });
		case RE::FormType::Enchantment:
			return sol::make_object(a_lua, LuaEnchantment{ a_form });
		case RE::FormType::Container:
			return sol::make_object(a_lua, LuaContainer{ a_form });
		case RE::FormType::NPC:
			return sol::make_object(a_lua, LuaActor{ a_form });
		case RE::FormType::Global:
			return sol::make_object(a_lua, LuaGlobal{ a_form });
		case RE::FormType::Shout:
			return sol::make_object(a_lua, LuaShout{ a_form });
		case RE::FormType::Light:
			return sol::make_object(a_lua, LuaLight{ a_form });
		default:
			return sol::make_object(a_lua, LuaForm{ a_form });
		}
	}

	void RegisterApi(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaForm>(
			"Form", sol::meta_function::index, sol::readonly_property(UnknownPropertyGetter<LuaForm>),
			sol::meta_function::to_string,

			[](const LuaForm& a_form) {
				const auto identifier = FormToIdentifier(a_form.form);

				return fmt::format("Form[{}|{:08X}]", identifier, a_form.form->GetFormID());
			},

			"formId",

			sol::property([](const LuaForm& a_form) { return static_cast<lua_Integer>(a_form.form->GetFormID()); }),

			"typeId",

			sol::property([](const LuaForm& a_form) { return static_cast<lua_Integer>(a_form.form->GetFormType()); }),

			"type",

			sol::property(
				[](const LuaForm& a_form) { return std::string(RE::FormTypeToString(a_form.form->GetFormType())); }),

			"editorId",

			sol::property([](const LuaForm& a_form) -> sol::optional<std::string> {
				const auto& cache = EditorIdCache();

				if (const auto it = cache.find(a_form.form->GetFormID()); it != cache.end()) {
					return it->second;
				}

				if (const char* editorID = a_form.form->GetFormEditorID(); editorID && *editorID) {
					return std::string(editorID);
				}

				return sol::nullopt;
			}),

			"name",

			sol::property([](const LuaForm& a_form) -> sol::optional<std::string> {
				if (const char* name = a_form.form->GetName(); name && *name) {
					return std::string(name);
				}

				return sol::nullopt;
			}),

			"identifier", sol::property([](const LuaForm& a_form) { return FormToIdentifier(a_form.form); }),

			"plugin", sol::property([](const LuaForm& a_form) -> sol::optional<std::string> {
				if (auto* file = a_form.form->GetFile(0)) {
					const auto name = file->GetFilename();
					return std::string(name);
				}

				return sol::nullopt;
			}),

			"value", sol::property([](const LuaForm& a_form) -> sol::optional<lua_Integer> {
				const auto* valueForm = a_form.form->As<RE::TESValueForm>();
				if (valueForm) {
					return static_cast<lua_Integer>(valueForm->value);
				}
				return sol::nullopt;
			}),

			"weight", sol::property([](const LuaForm& a_form) -> sol::optional<double> {
				const auto* weightForm = a_form.form->As<RE::TESWeightForm>();
				if (weightForm) {
					return weightForm->weight;
				}
				return sol::nullopt;
			}),

			"enchantment", sol::property([](const LuaForm& a_form) -> sol::optional<LuaForm> {
				const auto* enchantable = a_form.form->As<RE::TESEnchantableForm>();
				if (enchantable && enchantable->formEnchanting) {
					return LuaForm{ enchantable->formEnchanting };
				}
				return sol::nullopt;
			}),

			"keywords", sol::property([](const LuaForm& a_form) {
				const auto* keywordForm = a_form.form->As<RE::BGSKeywordForm>();
				std::vector<LuaForm> result;
				if (keywordForm) {
					const auto keywords = keywordForm->GetKeywords();
					result.reserve(keywords.size());
					for (auto* keyword : keywords) {
						result.emplace_back(LuaForm{ keyword });
					}
				}
				return result;
			}),

			"hasKeyword",
			[](const LuaForm& a_form, sol::variadic_args a_args) {
				auto* keyword = ParseFormRef(a_args, "hasKeyword").form->As<RE::BGSKeyword>();
				const auto* keywordForm = a_form.form->As<RE::BGSKeywordForm>();
				return keyword && keywordForm && keywordForm->HasKeyword(keyword);
			});

		sol::table patcher = a_lua.create_table(0, 8);
		patcher["version"] = "0.1.0";
		patcher["log"] = &Log;
		patcher["warn"] = &Warn;
		patcher["error"] = &Error;
		patcher["getForm"] = &GetFormById;
		patcher["isPluginInstalled"] = &IsPluginInstalled;
		patcher["loadLua"] = &LoadLua;
		a_lua["lua_patcher"] = patcher;

		// Route print() through the plugin log
		a_lua["print"] = &Log;
	}
}