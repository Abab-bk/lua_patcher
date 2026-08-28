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
	{
	};

	struct LuaEncounterZone : LuaForm
	{
	};

	// Fallback __index handler: raises a Lua error for unknown properties
	// instead of silently returning nil (matches the pre-sol2 behavior).
	template <class T>
	inline sol::object UnknownPropertyGetter(const T& /*unused*/)
	{
		throw sol::error{ "unknown property" };
	}

	// Resolves a form reference given as two strings: plugin name + hex local
	// formID ("000123" for light plugins, "00012345" for full plugins; the
	// masked local part is read as-is). Results are cached per pair.
	RE::TESForm* LookupFormByPluginFormId(const std::string_view& a_plugin, const std::string_view& a_formId);

	// Resolves a bare EditorID string. Results are cached per editorID.
	RE::TESForm* LookupFormByEditorId(const std::string_view& a_editorId);

	// A form reference as parsed from the leading arguments of a variadic call.
	struct FormRef
	{
		RE::TESForm* form = nullptr;
		std::size_t consumed = 1;  // 2 when the caller used the ("Plugin", "000123") pair form
	};

	// Parses a form reference from the leading arguments of a call:
	//   (Form)                    single form object
	//   ("EditorID")              single editorID string
	//   ("Plugin.esp", "000123")  plugin + hex local formID pair
	// Raises a Lua error with method/argument context when the reference is
	// missing or does not resolve to a loaded form.
	FormRef ParseFormRef(const sol::variadic_args& a_args, std::string_view a_method);

	// Lenient leading-argument form reference lookup (used by getForm): mirrors
	// ParseFormRef but returns nullptr on a miss instead of raising.
	RE::TESForm* LookupFormRef(const sol::variadic_args& a_args, std::string_view a_method);

	// Resolves a single-value form reference (table entries, property values):
	// a Form object, an "EditorID" string, or a { "Plugin", "000123" } pair
	// table. Raises a Lua error when the value does not resolve to a loaded form.
	RE::TESForm* CheckFormValue(const sol::object& a_value);

	// Lenient single-value lookup (used by getForm): returns nullptr instead of
	// raising when the value does not resolve.
	RE::TESForm* LookupFormValue(const sol::object& a_value);

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

	// Registers the EncounterZone usertype and the encounter zone API functions.
	void RegisterEncounterZone(sol::state_view& a_lua);
}