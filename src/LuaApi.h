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
	// class (Form base + typed wrappers registered with sol::bases)
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

	struct LuaFormList : LuaForm
	{};

	struct LuaIngredient : LuaForm
	{};

	struct LuaPotion : LuaForm
	{};

	struct LuaEnchantment : LuaForm
	{};

	struct LuaContainer : LuaForm
	{};

	struct LuaActor : LuaForm
	{};

	struct LuaGlobal : LuaForm
	{};

	struct LuaShout : LuaForm
	{};

	struct LuaLight : LuaForm
	{};

	// Fallback __index handler: raises a Lua error for unknown properties
	// instead of silently returning nil (matches the pre-sol2 behavior).
	template <class T>
	inline sol::object UnknownPropertyGetter(const T& /*unused*/)
	{
		throw sol::error{ "unknown property" };
	}

	// Resolves a "Plugin.esm|000123" / "Plugin.esm|123" / EditorID identifier
	// (light plugins mask to 0xFFF). Results are cached per identifier.
	RE::TESForm* LookupFormByIdentifier(const std::string_view& a_identifier);

	// Accepts an identifier string or any form userdata (Form/LeveledList/Weapon/Armor).
	// Raises a Lua argument error if the value does not resolve to a loaded form.
	RE::TESForm* CheckForm(const sol::object& a_value);

	// Returns the TESForm stored in any form userdata at a_index (no identifier strings).
	RE::TESForm* ToAnyForm(const sol::object& a_value);

	// Pushes a typed userdata for a_form (Weapon/Armor/LeveledList/Spell/MagicEffect/
	// FormList/Form by form type), or nil for a null form.
	sol::object PushForm(sol::state_view& a_lua, RE::TESForm* a_form);

	// Registers the `lua_patcher` global table, the `print` override and the Form usertype.
	void RegisterApi(sol::state_view& a_lua);

	// Registers the LeveledList usertype and the leveled list API functions.
	void RegisterLeveledList(sol::state_view& a_lua);

	// Registers the Weapon/Armor usertypes and the equipment API functions.
	void RegisterEquipment(sol::state_view& a_lua);

	// Registers Spell/MagicEffect usertypes and magic API functions.
	void RegisterMagic(sol::state_view& a_lua);

	// Registers the FormList usertype and the form list API functions.
	void RegisterFormList(sol::state_view& a_lua);

	// Registers the Ingredient/Potion usertypes and the alchemy API functions.
	void RegisterAlchemy(sol::state_view& a_lua);

	// Registers the Enchantment usertype and the enchantment API functions.
	void RegisterEnchantment(sol::state_view& a_lua);

	// Registers the Container usertype and the container API functions.
	void RegisterContainer(sol::state_view& a_lua);

	// Registers the Actor (TESNPC) usertype and the actor API functions.
	void RegisterActors(sol::state_view& a_lua);

	// Registers the Global usertype and the global API functions.
	void RegisterWorld(sol::state_view& a_lua);

	// Registers the Shout usertype and the shout API functions.
	void RegisterShout(sol::state_view& a_lua);

	// Registers the Light usertype and the light API functions.
	void RegisterLight(sol::state_view& a_lua);
}