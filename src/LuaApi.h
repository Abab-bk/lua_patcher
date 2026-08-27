#pragma once

#include <lua.hpp>

#include <string_view>

namespace RE
{
	class TESForm;
}

namespace LuaPatcher
{
	inline constexpr std::string_view kFormMeta = "LuaPatcher.Form";
	inline constexpr std::string_view kLeveledListMeta = "LuaPatcher.LeveledList";
	inline constexpr std::string_view kWeaponMeta = "LuaPatcher.Weapon";
	inline constexpr std::string_view kArmorMeta = "LuaPatcher.Armor";

	// Resolves a "Plugin.esm|000123" / "Plugin.esm|123" / EditorID identifier, mirroring
	// SkyPatcher's GetFormFromIdentifier (light plugins mask to 0xFFF). Results are cached.
	RE::TESForm* GetFormFromIdentifier(std::string_view a_identifier);

	// Accepts an identifier string or any form userdata (Form/LeveledList/Weapon/Armor).
	// Raises a Lua argument error if the value does not resolve to a loaded form.
	RE::TESForm* CheckForm(lua_State* a_state, int a_index);

	// Returns the TESForm stored in any form userdata at a_index (no identifier strings).
	RE::TESForm* ToAnyForm(lua_State* a_state, int a_index);

	// Pushes a typed userdata for a_form (Weapon/Armor/LeveledList/Form by form type),
	// or nil for a null form. Returns 1.
	int PushForm(lua_State* a_state, RE::TESForm* a_form);

	// Shared property lookup for the Form and typed metatables.
	int FormIndexCommon(lua_State* a_state, RE::TESForm* a_form, std::string_view a_key);

	// Registers the `lua_patcher` global table, the `print` override and the Form metatable.
	void RegisterApi(lua_State* a_state);

	// Registers the LeveledList metatable and the leveled list API functions.
	void RegisterLeveledList(lua_State* a_state);

	// Registers the Weapon/Armor metatables and the equipment API functions.
	void RegisterEquipment(lua_State* a_state);
}