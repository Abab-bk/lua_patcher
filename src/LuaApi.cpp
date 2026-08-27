#include "LuaApi.h"

#include "PCH.h"

#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESEnchantableForm.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	RE::TESForm* GetFormFromIdentifierCached(std::string_view a_identifier)
	{
		static std::unordered_map<std::string, RE::TESForm*> cache;

		const std::string key(a_identifier);
		if (const auto it = cache.find(key); it != cache.end()) {
			return it->second;
		}

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return nullptr;
		}

		RE::TESForm* result = nullptr;
		try {
			if (const auto delimiter = a_identifier.find('|'); delimiter != std::string_view::npos) {
				const auto modName = a_identifier.substr(0, delimiter);
				const auto modForm = a_identifier.substr(delimiter + 1);
				auto       rawFormID = static_cast<std::uint32_t>(std::stoul(std::string(modForm), nullptr, 16));

				const auto* mod = dataHandler->LookupModByName(modName);
				if (mod && mod->IsLight()) {
					rawFormID &= 0xFFF;
				} else {
					rawFormID &= 0xFFFFFF;
				}

				result = dataHandler->LookupForm(rawFormID, modName);
			} else {
				result = RE::TESForm::LookupByEditorID(a_identifier);
			}
		} catch (const std::exception& e) {
			logger::warn("LuaPatcher: invalid form identifier '{}': {}", a_identifier, e.what());
		}

		cache.emplace(key, result);
		return result;
	}

	RE::TESForm* ToForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(luaL_checkudata(a_state, a_index, LuaPatcher::kFormMeta.data()));
	}

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
		static RE::TESDataHandler*                         owner = nullptr;
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

	int FormIndex(lua_State* a_state)
	{
		const auto key = luaL_checkstring(a_state, 2);
		auto*      form = ToForm(a_state, 1);
		return LuaPatcher::FormIndexCommon(a_state, form, key);
	}

	int FormHasKeyword(lua_State* a_state)
	{
		auto*       form = LuaPatcher::ToAnyForm(a_state, 1);
		auto*       keyword = LuaPatcher::CheckForm(a_state, 2);
		auto*       kw = keyword->As<RE::BGSKeyword>();
		const auto* keywordForm = form->As<RE::BGSKeywordForm>();
		lua_pushboolean(a_state, kw && keywordForm && keywordForm->HasKeyword(kw));
		return 1;
	}

	int FormToString(lua_State* a_state)
	{
		auto*      form = ToForm(a_state, 1);
		const auto identifier = FormToIdentifier(form);
		lua_pushstring(a_state, fmt::format("Form[{}|{:08X}]", identifier, form->GetFormID()).c_str());
		return 1;
	}

	// ---- lua_patcher core functions ----

	void ConcatArgs(lua_State* a_state, std::string& a_out)
	{
		const int n = lua_gettop(a_state);
		for (int i = 1; i <= n; ++i) {
			size_t      len = 0;
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

	int GetForm(lua_State* a_state)
	{
		const auto identifier = luaL_checkstring(a_state, 1);
		return LuaPatcher::PushForm(a_state, LuaPatcher::GetFormFromIdentifier(identifier));
	}

	int IsPluginInstalled(lua_State* a_state)
	{
		const auto name = luaL_checkstring(a_state, 1);
		auto*      dataHandler = RE::TESDataHandler::GetSingleton();
		const bool installed = dataHandler && (dataHandler->LookupLoadedModByName(name) != nullptr ||
												  dataHandler->LookupLoadedLightModByName(name) != nullptr);
		lua_pushboolean(a_state, installed);
		return 1;
	}
}

namespace LuaPatcher
{
	RE::TESForm* GetFormFromIdentifier(std::string_view a_identifier)
	{
		return GetFormFromIdentifierCached(a_identifier);
	}

	RE::TESForm* CheckForm(lua_State* a_state, int a_index)
	{
		if (lua_type(a_state, a_index) == LUA_TSTRING) {
			auto* form = GetFormFromIdentifier(lua_tostring(a_state, a_index));
			if (!form) {
				luaL_argerror(a_state, a_index, "form identifier does not resolve to a loaded form");
			}
			return form;
		}

		return ToAnyForm(a_state, a_index);
	}

	RE::TESForm* ToAnyForm(lua_State* a_state, int a_index)
	{
		constexpr std::string_view metas[] = {
			LuaPatcher::kFormMeta,
			LuaPatcher::kLeveledListMeta,
			LuaPatcher::kWeaponMeta,
			LuaPatcher::kArmorMeta,
		};
		for (const auto meta : metas) {
			if (auto* ud = luaL_testudata(a_state, a_index, meta.data())) {
				return *static_cast<RE::TESForm**>(ud);
			}
		}
		luaL_argerror(a_state, a_index, "expected a form identifier string or a Form");
		return nullptr;
	}

	int PushForm(lua_State* a_state, RE::TESForm* a_form)
	{
		if (!a_form) {
			lua_pushnil(a_state);
			return 1;
		}

		auto** ud = static_cast<RE::TESForm**>(lua_newuserdatauv(a_state, sizeof(RE::TESForm*), 0));
		*ud = a_form;

		std::string_view meta = kFormMeta;
		switch (a_form->GetFormType()) {
		case RE::FormType::LeveledItem:
		case RE::FormType::LeveledNPC:
			meta = kLeveledListMeta;
			break;
		case RE::FormType::Weapon:
			meta = kWeaponMeta;
			break;
		case RE::FormType::Armor:
			meta = kArmorMeta;
			break;
		default:
			break;
		}
		luaL_setmetatable(a_state, meta.data());
		return 1;
	}

	void RegisterApi(lua_State* a_state)
	{
		// Form metatable
		luaL_newmetatable(a_state, kFormMeta.data());
		lua_pushcfunction(a_state, FormIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, FormToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		// lua_patcher table
		lua_createtable(a_state, 0, 8);
		lua_pushliteral(a_state, "0.1.0");
		lua_setfield(a_state, -2, "version");

		static const luaL_Reg functions[] = {
			{ "log", Log },
			{ "warn", Warn },
			{ "error", Error },
			{ "getForm", GetForm },
			{ "isPluginInstalled", IsPluginInstalled },
			{ nullptr, nullptr },
		};
		luaL_setfuncs(a_state, functions, 0);

		lua_setglobal(a_state, "lua_patcher");

		// Route print() through the plugin log
		lua_pushcfunction(a_state, Log);
		lua_setglobal(a_state, "print");
	}

	int FormIndexCommon(lua_State* a_state, RE::TESForm* a_form, std::string_view a_key)
	{
		if (a_key == "formId") {
			lua_pushinteger(a_state, static_cast<lua_Integer>(a_form->GetFormID()));
			return 1;
		}
		if (a_key == "typeId") {
			lua_pushinteger(a_state, static_cast<lua_Integer>(a_form->GetFormType()));
			return 1;
		}
		if (a_key == "type") {
			const auto type = RE::FormTypeToString(a_form->GetFormType());
			lua_pushlstring(a_state, type.data(), type.size());
			return 1;
		}
		if (a_key == "editorId") {
			const auto& cache = EditorIdCache();
			if (const auto it = cache.find(a_form->GetFormID()); it != cache.end()) {
				lua_pushlstring(a_state, it->second.data(), it->second.size());
			} else if (const char* editorID = a_form->GetFormEditorID(); editorID && *editorID) {
				lua_pushstring(a_state, editorID);
			} else {
				lua_pushnil(a_state);
			}
			return 1;
		}
		if (a_key == "name") {
			if (const char* name = a_form->GetName(); name && *name) {
				lua_pushstring(a_state, name);
			} else {
				lua_pushnil(a_state);
			}
			return 1;
		}
		if (a_key == "identifier") {
			const auto identifier = FormToIdentifier(a_form);
			lua_pushlstring(a_state, identifier.data(), identifier.size());
			return 1;
		}
		if (a_key == "plugin") {
			if (auto* file = a_form->GetFile(0)) {
				const auto name = file->GetFilename();
				lua_pushlstring(a_state, name.data(), name.size());
			} else {
				lua_pushnil(a_state);
			}
			return 1;
		}
		if (a_key == "value") {
			const auto* valueForm = a_form->As<RE::TESValueForm>();
			if (valueForm) {
				lua_pushinteger(a_state, valueForm->value);
			} else {
				lua_pushnil(a_state);
			}
			return 1;
		}
		if (a_key == "weight") {
			const auto* weightForm = a_form->As<RE::TESWeightForm>();
			if (weightForm) {
				lua_pushnumber(a_state, weightForm->weight);
			} else {
				lua_pushnil(a_state);
			}
			return 1;
		}
		if (a_key == "enchantment") {
			const auto* enchantable = a_form->As<RE::TESEnchantableForm>();
			if (enchantable && enchantable->formEnchanting) {
				return PushForm(a_state, enchantable->formEnchanting);
			}
			lua_pushnil(a_state);
			return 1;
		}
		if (a_key == "keywords") {
			const auto* keywordForm = a_form->As<RE::BGSKeywordForm>();
			if (!keywordForm) {
				lua_createtable(a_state, 0, 0);
				return 1;
			}
			lua_createtable(a_state, keywordForm->numKeywords, 0);
			lua_Integer index = 1;
			for (std::uint32_t i = 0; i < keywordForm->numKeywords; ++i) {
				PushForm(a_state, keywordForm->keywords[i]);
				lua_rawseti(a_state, -2, index++);
			}
			return 1;
		}
		if (a_key == "hasKeyword") {
			lua_pushcfunction(a_state, FormHasKeyword);
			return 1;
		}
		return luaL_error(a_state, "unknown property '%s'", std::string(a_key).c_str());
	}
}
