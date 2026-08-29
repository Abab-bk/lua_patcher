#include "LuaApi.h"

#include "Effects.h"

#include <RE/A/AlchemyItem.h>
#include <RE/I/IngredientItem.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace
{
	RE::IngredientItem* ToIngredient(const LuaPatcher::LuaIngredient& a_form)
	{
		return a_form.form->As<RE::IngredientItem>();
	}

	RE::AlchemyItem* ToPotion(const LuaPatcher::LuaPotion& a_form) { return a_form.form->As<RE::AlchemyItem>(); }

	template <class T>
	sol::object PushFormArray(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<T>();

		sol::table result = lua.create_table(static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			result[index++] = LuaPatcher::PushForm(lua, form);
		}
		return result;
	}

	sol::object AllIngredients(sol::this_state a_state) { return PushFormArray<RE::IngredientItem>(a_state); }

	sol::object AllPotions(sol::this_state a_state) { return PushFormArray<RE::AlchemyItem>(a_state); }
}

namespace LuaPatcher
{
	// Registers the effect-slot methods shared by Ingredient/Potion (the
	// Enchantment usertype registers the same surface itself).
	void RegisterAlchemy(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaIngredient>(
			"Ingredient", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaIngredient>), sol::meta_function::to_string,
			[](const LuaIngredient& a_form) { return fmt::format("Ingredient[{:08X}]", a_form.form->GetFormID()); },
			"costOverride",
			sol::property(
				[](const LuaIngredient& a_form) {
					return static_cast<lua_Integer>(ToIngredient(a_form)->data.costOverride);
				},
				[](LuaIngredient& a_form, lua_Integer a_value) {
					auto* ingredient = ToIngredient(a_form);
					a_value = std::max<lua_Integer>(a_value, 0);
					ingredient->data.costOverride = static_cast<std::int32_t>(a_value);
				}),
			"effects",
			[](const LuaIngredient& a_form, sol::this_state a_state) {
				return Effects::PushEffectList(sol::state_view(a_state), ToIngredient(a_form));
			},
			"setEffects",
			[](LuaIngredient& a_form, const sol::object& a_list) {
				Effects::SetEffectList(ToIngredient(a_form), a_list);
			},
			"addEffect",
			[](LuaIngredient& a_form, sol::variadic_args a_args) {
				return Effects::AddEffect(ToIngredient(a_form), a_args);
			},
			"clearEffects", [](LuaIngredient& a_form) { Effects::ClearEffects(ToIngredient(a_form)); });

		a_lua.new_usertype<LuaPotion>(
			"Potion", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaPotion>), sol::meta_function::to_string,
			[](const LuaPotion& a_form) { return fmt::format("Potion[{:08X}]", a_form.form->GetFormID()); },
			"costOverride",
			sol::property(
				[](const LuaPotion& a_form) { return static_cast<lua_Integer>(ToPotion(a_form)->data.costOverride); },
				[](LuaPotion& a_form, lua_Integer a_value) {
					auto* potion = ToPotion(a_form);
					a_value = std::max<lua_Integer>(a_value, 0);
					potion->data.costOverride = static_cast<std::int32_t>(a_value);
				}),
			"isPoison", sol::property([](const LuaPotion& a_form) { return ToPotion(a_form)->IsPoison(); }), "isFood",
			sol::property([](const LuaPotion& a_form) { return ToPotion(a_form)->IsFood(); }), "effects",
			[](const LuaPotion& a_form, sol::this_state a_state) {
				return Effects::PushEffectList(sol::state_view(a_state), ToPotion(a_form));
			},
			"setEffects",
			[](LuaPotion& a_form, const sol::object& a_list) { Effects::SetEffectList(ToPotion(a_form), a_list); },
			"addEffect",
			[](LuaPotion& a_form, sol::variadic_args a_args) { return Effects::AddEffect(ToPotion(a_form), a_args); },
			"clearEffects", [](LuaPotion& a_form) { Effects::ClearEffects(ToPotion(a_form)); });

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allIngredients"] = &AllIngredients;
		patcher["allPotions"] = &AllPotions;
	}
}