#include "LuaApi.h"

#include "Effects.h"

#include <RE/E/EnchantmentItem.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace
{
	RE::EnchantmentItem* ToEnchantment(const LuaPatcher::LuaEnchantment& a_form)
	{
		return a_form.form->As<RE::EnchantmentItem>();
	}

	std::string_view CastingTypeName(RE::MagicSystem::CastingType a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::CastingType::kConstantEffect:
			return "ConstantEffect";
		case RE::MagicSystem::CastingType::kFireAndForget:
			return "FireAndForget";
		case RE::MagicSystem::CastingType::kConcentration:
			return "Concentration";
		case RE::MagicSystem::CastingType::kScroll:
			return "Scroll";
		default:
			return "Other";
		}
	}

	bool TryParseCastingType(std::string_view a_sv, RE::MagicSystem::CastingType& a_out)
	{
		if (a_sv == "ConstantEffect") {
			a_out = RE::MagicSystem::CastingType::kConstantEffect;
			return true;
		}
		if (a_sv == "FireAndForget") {
			a_out = RE::MagicSystem::CastingType::kFireAndForget;
			return true;
		}
		if (a_sv == "Concentration") {
			a_out = RE::MagicSystem::CastingType::kConcentration;
			return true;
		}
		if (a_sv == "Scroll") {
			a_out = RE::MagicSystem::CastingType::kScroll;
			return true;
		}
		return false;
	}

	std::string_view DeliveryName(RE::MagicSystem::Delivery a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::Delivery::kSelf:
			return "Self";
		case RE::MagicSystem::Delivery::kTouch:
			return "Touch";
		case RE::MagicSystem::Delivery::kAimed:
			return "Aimed";
		case RE::MagicSystem::Delivery::kTargetActor:
			return "TargetActor";
		case RE::MagicSystem::Delivery::kTargetLocation:
			return "TargetLocation";
		default:
			return "Other";
		}
	}

	bool TryParseDelivery(std::string_view a_sv, RE::MagicSystem::Delivery& a_out)
	{
		if (a_sv == "Self") {
			a_out = RE::MagicSystem::Delivery::kSelf;
			return true;
		}
		if (a_sv == "Touch") {
			a_out = RE::MagicSystem::Delivery::kTouch;
			return true;
		}
		if (a_sv == "Aimed") {
			a_out = RE::MagicSystem::Delivery::kAimed;
			return true;
		}
		if (a_sv == "TargetActor") {
			a_out = RE::MagicSystem::Delivery::kTargetActor;
			return true;
		}
		if (a_sv == "TargetLocation") {
			a_out = RE::MagicSystem::Delivery::kTargetLocation;
			return true;
		}
		return false;
	}

	sol::object AllEnchantments(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::EnchantmentItem>();

		sol::table result = lua.create_table(static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			result[index++] = LuaPatcher::PushForm(lua, form);
		}
		return result;
	}
}

namespace LuaPatcher
{
	void RegisterEnchantment(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaEnchantment>(
			"Enchantment", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaEnchantment>), sol::meta_function::to_string,
			[](const LuaEnchantment& a_form) { return fmt::format("Enchantment[{:08X}]", a_form.form->GetFormID()); },
			"costOverride",
			sol::property(
				[](const LuaEnchantment& a_form) {
					return static_cast<lua_Integer>(ToEnchantment(a_form)->data.costOverride);
				},
				[](LuaEnchantment& a_form, lua_Integer a_value) {
					auto* enchantment = ToEnchantment(a_form);
					a_value = std::max<lua_Integer>(a_value, 0);
					enchantment->data.costOverride = static_cast<std::int32_t>(a_value);
				}),
			"chargeOverride",
			sol::property(
				[](const LuaEnchantment& a_form) {
					return static_cast<lua_Integer>(ToEnchantment(a_form)->data.chargeOverride);
				},
				[](LuaEnchantment& a_form, lua_Integer a_value) {
					auto* enchantment = ToEnchantment(a_form);
					a_value = std::max<lua_Integer>(a_value, 0);
					enchantment->data.chargeOverride = static_cast<std::int32_t>(a_value);
				}),
			"chargeTime",
			sol::property([](const LuaEnchantment& a_form) { return ToEnchantment(a_form)->data.chargeTime; },
				[](LuaEnchantment& a_form, double a_value) {
					ToEnchantment(a_form)->data.chargeTime = static_cast<float>(a_value);
				}),
			"castingType",
			sol::property(
				[](const LuaEnchantment& a_form) {
					return std::string(CastingTypeName(ToEnchantment(a_form)->data.castingType));
				},
				[](LuaEnchantment& a_form, const std::string& a_value) {
					RE::MagicSystem::CastingType type = RE::MagicSystem::CastingType::kConstantEffect;
					if (!TryParseCastingType(a_value, type)) {
						throw sol::error{ "invalid castingType" };
					}
					ToEnchantment(a_form)->data.castingType = type;
				}),
			"delivery",
			sol::property(
				[](const LuaEnchantment& a_form) {
					return std::string(DeliveryName(ToEnchantment(a_form)->data.delivery));
				},
				[](LuaEnchantment& a_form, const std::string& a_value) {
					RE::MagicSystem::Delivery delivery = RE::MagicSystem::Delivery::kSelf;
					if (!TryParseDelivery(a_value, delivery)) {
						throw sol::error{ "invalid delivery" };
					}
					ToEnchantment(a_form)->data.delivery = delivery;
				}),
			"baseEnchantment", sol::property([](const LuaEnchantment& a_form) -> sol::optional<LuaForm> {
				if (auto* base = ToEnchantment(a_form)->data.baseEnchantment) {
					return LuaForm{ base };
				}
				return sol::nullopt;
			}),
			"effects",
			[](const LuaEnchantment& a_form, sol::this_state a_state) {
				return Effects::PushEffectList(sol::state_view(a_state), ToEnchantment(a_form));
			},
			"setEffects",
			[](LuaEnchantment& a_form, const sol::object& a_list) {
				Effects::SetEffectList(ToEnchantment(a_form), a_list);
			},
			"addEffect",
			[](LuaEnchantment& a_form, sol::variadic_args a_args) {
				return Effects::AddEffect(ToEnchantment(a_form), a_args);
			},
			"clearEffects", [](LuaEnchantment& a_form) { Effects::ClearEffects(ToEnchantment(a_form)); });

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allEnchantments"] = &AllEnchantments;
	}
}