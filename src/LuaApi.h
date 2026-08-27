#pragma once

#include <sol/sol.hpp>

#include <string_view>

namespace RE
{
	class TESForm;
}

namespace LuaPatcher
{
	// sol2 usertype wrappers for RE::TESForm pointers. One usertype per form
	// class (Form base + typed wrappers registered with sol::bases), mirroring
	// the old per-metatable userdata dispatch.
	struct LuaForm
	{
		RE::TESForm* form;
	};
	struct LuaWeapon : LuaForm
	{};
	struct LuaArmor : LuaForm
	{};
	struct LuaLeveledList : LuaForm
	{};
	struct LuaSpell : LuaForm
	{};
	struct LuaMagicEffect : LuaForm
	{};

	// Fallback __index handler: raises a Lua error for unknown properties
	// instead of silently returning nil (matches the pre-sol2 behavior).
	template <class T>
	inline sol::object UnknownPropertyGetter(const T&)
	{
		throw sol::error{ "unknown property" };
	}

	// Resolves a "Plugin.esm|000123" / "Plugin.esm|123" / EditorID identifier, mirroring
	// SkyPatcher's GetFormFromIdentifier (light plugins mask to 0xFFF). Results are cached.
	RE::TESForm* GetFormFromIdentifier(std::string_view a_identifier);

	// Accepts an identifier string or any form userdata (Form/LeveledList/Weapon/Armor).
	// Raises a Lua argument error if the value does not resolve to a loaded form.
	RE::TESForm* CheckForm(sol::object a_value);

	// Returns the TESForm stored in any form userdata at a_index (no identifier strings).
	RE::TESForm* ToAnyForm(sol::object a_value);

	// Pushes a typed userdata for a_form (Weapon/Armor/LeveledList/Spell/MagicEffect/Form
	// by form type), or nil for a null form.
	sol::object PushForm(sol::state_view a_lua, RE::TESForm* a_form);

	// Registers the `lua_patcher` global table, the `print` override and the Form usertype.
	void RegisterApi(sol::state_view a_lua);

	// Registers the LeveledList usertype and the leveled list API functions.
	void RegisterLeveledList(sol::state_view a_lua);

	// Registers the Weapon/Armor usertypes and the equipment API functions.
	void RegisterEquipment(sol::state_view a_lua);

	// Registers Spell/MagicEffect usertypes and magic API functions.
	void RegisterMagic(sol::state_view a_lua);
}