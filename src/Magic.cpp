#include "LuaApi.h"

#include <RE/E/EffectSetting.h>
#include <RE/S/SpellItem.h>

#include <string>
#include <string_view>

namespace
{
	// ---- Spell helpers ----
	std::string_view SpellTypeName(RE::MagicSystem::SpellType a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::SpellType::kSpell:
			return "Spell";
		case RE::MagicSystem::SpellType::kDisease:
			return "Disease";
		case RE::MagicSystem::SpellType::kPower:
			return "Power";
		case RE::MagicSystem::SpellType::kLesserPower:
			return "LesserPower";
		case RE::MagicSystem::SpellType::kAbility:
			return "Ability";
		case RE::MagicSystem::SpellType::kPoison:
			return "Poison";
		case RE::MagicSystem::SpellType::kEnchantment:
			return "Enchantment";
		case RE::MagicSystem::SpellType::kPotion:
			return "Potion";
		case RE::MagicSystem::SpellType::kIngredient:
			return "Ingredient";
		case RE::MagicSystem::SpellType::kLeveledSpell:
			return "LeveledSpell";
		case RE::MagicSystem::SpellType::kAddiction:
			return "Addiction";
		case RE::MagicSystem::SpellType::kVoicePower:
			return "VoicePower";
		default:
			return "Other";
		}
	}

	bool TryParseSpellType(std::string_view a_sv, RE::MagicSystem::SpellType& a_out)
	{
		if (a_sv == "Spell") {
			a_out = RE::MagicSystem::SpellType::kSpell;
			return true;
		}
		if (a_sv == "Disease") {
			a_out = RE::MagicSystem::SpellType::kDisease;
			return true;
		}
		if (a_sv == "Power") {
			a_out = RE::MagicSystem::SpellType::kPower;
			return true;
		}
		if (a_sv == "LesserPower") {
			a_out = RE::MagicSystem::SpellType::kLesserPower;
			return true;
		}
		if (a_sv == "Ability") {
			a_out = RE::MagicSystem::SpellType::kAbility;
			return true;
		}
		if (a_sv == "Poison") {
			a_out = RE::MagicSystem::SpellType::kPoison;
			return true;
		}
		return false;
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

	std::string_view ActorValueName(RE::ActorValue a_av)
	{
		switch (a_av) {
		case RE::ActorValue::kAlteration:
			return "Alteration";
		case RE::ActorValue::kConjuration:
			return "Conjuration";
		case RE::ActorValue::kDestruction:
			return "Destruction";
		case RE::ActorValue::kIllusion:
			return "Illusion";
		case RE::ActorValue::kRestoration:
			return "Restoration";
		case RE::ActorValue::kEnchanting:
			return "Enchanting";
		case RE::ActorValue::kNone:
			return "None";
		default:
			return "Other";
		}
	}

	bool TryParseActorValue(std::string_view a_sv, RE::ActorValue& a_out)
	{
		if (a_sv == "Alteration") {
			a_out = RE::ActorValue::kAlteration;
			return true;
		}
		if (a_sv == "Conjuration") {
			a_out = RE::ActorValue::kConjuration;
			return true;
		}
		if (a_sv == "Destruction") {
			a_out = RE::ActorValue::kDestruction;
			return true;
		}
		if (a_sv == "Illusion") {
			a_out = RE::ActorValue::kIllusion;
			return true;
		}
		if (a_sv == "Restoration") {
			a_out = RE::ActorValue::kRestoration;
			return true;
		}
		if (a_sv == "Enchanting") {
			a_out = RE::ActorValue::kEnchanting;
			return true;
		}
		if (a_sv == "None") {
			a_out = RE::ActorValue::kNone;
			return true;
		}
		return false;
	}

	std::string_view ArchetypeName(RE::EffectArchetypes::ArchetypeID a_arch)
	{
		switch (a_arch) {
		case RE::EffectArchetypes::ArchetypeID::kValueModifier:
			return "ValueModifier";
		case RE::EffectArchetypes::ArchetypeID::kScript:
			return "Script";
		case RE::EffectArchetypes::ArchetypeID::kDispel:
			return "Dispel";
		case RE::EffectArchetypes::ArchetypeID::kCureDisease:
			return "CureDisease";
		case RE::EffectArchetypes::ArchetypeID::kAbsorb:
			return "Absorb";
		case RE::EffectArchetypes::ArchetypeID::kDualValueModifier:
			return "DualValueModifier";
		case RE::EffectArchetypes::ArchetypeID::kCalm:
			return "Calm";
		case RE::EffectArchetypes::ArchetypeID::kDemoralize:
			return "Demoralize";
		case RE::EffectArchetypes::ArchetypeID::kFrenzy:
			return "Frenzy";
		case RE::EffectArchetypes::ArchetypeID::kDisarm:
			return "Disarm";
		case RE::EffectArchetypes::ArchetypeID::kCommandSummoned:
			return "CommandSummoned";
		case RE::EffectArchetypes::ArchetypeID::kInvisibility:
			return "Invisibility";
		case RE::EffectArchetypes::ArchetypeID::kLight:
			return "Light";
		default:
			return "Other";
		}
	}

	RE::BGSKeyword* CheckKeyword(const sol::object& a_value)
	{
		auto* keyword = LuaPatcher::CheckForm(a_value)->As<RE::BGSKeyword>();
		if (!keyword) {
			throw sol::error{ "expected a keyword form" };
		}
		return keyword;
	}

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

	sol::object AllSpells(sol::this_state a_state) { return PushFormArray<RE::SpellItem>(a_state); }

	sol::object AllMagicEffects(sol::this_state a_state) { return PushFormArray<RE::EffectSetting>(a_state); }
}

namespace LuaPatcher
{
	void RegisterMagic(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaSpell>(
			"Spell", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaSpell>), sol::meta_function::to_string,
			[](const LuaSpell& a_form) { return fmt::format("Spell[{:08X}]", a_form.form->GetFormID()); },
			"costOverride",
			sol::property(
				[](const LuaSpell& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::SpellItem>()->data.costOverride);
				},
				[](LuaSpell& a_form, lua_Integer a_value) {
					a_form.form->As<RE::SpellItem>()->data.costOverride = static_cast<std::int32_t>(a_value);
				}),
			"spellType",
			sol::property(
				[](const LuaSpell& a_form) {
					return std::string(SpellTypeName(a_form.form->As<RE::SpellItem>()->data.spellType));
				},
				[](LuaSpell& a_form, const std::string& a_value) {
					RE::MagicSystem::SpellType type = RE::MagicSystem::SpellType::kSpell;
					if (!TryParseSpellType(a_value, type)) {
						throw sol::error{ "invalid spellType (Spell/Disease/Power/LesserPower/Ability/Poison)" };
					}
					a_form.form->As<RE::SpellItem>()->data.spellType = type;
				}),
			"castingType",
			sol::property(
				[](const LuaSpell& a_form) {
					return std::string(CastingTypeName(a_form.form->As<RE::SpellItem>()->data.castingType));
				},
				[](LuaSpell& a_form, const std::string& a_value) {
					RE::MagicSystem::CastingType type = RE::MagicSystem::CastingType::kConstantEffect;
					if (!TryParseCastingType(a_value, type)) {
						throw sol::error{ "invalid castingType" };
					}
					a_form.form->As<RE::SpellItem>()->data.castingType = type;
				}),
			"delivery",
			sol::property(
				[](const LuaSpell& a_form) {
					return std::string(DeliveryName(a_form.form->As<RE::SpellItem>()->data.delivery));
				},
				[](LuaSpell& a_form, const std::string& a_value) {
					RE::MagicSystem::Delivery delivery = RE::MagicSystem::Delivery::kSelf;
					if (!TryParseDelivery(a_value, delivery)) {
						throw sol::error{ "invalid delivery" };
					}
					a_form.form->As<RE::SpellItem>()->data.delivery = delivery;
				}),
			"chargeTime",
			sol::property([](const LuaSpell& a_form) { return a_form.form->As<RE::SpellItem>()->data.chargeTime; },
				[](LuaSpell& a_form, double a_value) {
					a_form.form->As<RE::SpellItem>()->data.chargeTime = static_cast<float>(a_value);
				}),
			"castDuration",
			sol::property([](const LuaSpell& a_form) { return a_form.form->As<RE::SpellItem>()->data.castDuration; },
				[](LuaSpell& a_form, double a_value) {
					a_form.form->As<RE::SpellItem>()->data.castDuration = static_cast<float>(a_value);
				}),
			"range",
			sol::property([](const LuaSpell& a_form) { return a_form.form->As<RE::SpellItem>()->data.range; },
				[](LuaSpell& a_form, double a_value) {
					a_form.form->As<RE::SpellItem>()->data.range = static_cast<float>(a_value);
				}),
			"addKeyword",
			[](LuaSpell& a_form, const sol::object& a_keyword) {
				return a_form.form->As<RE::SpellItem>()->AddKeyword(CheckKeyword(a_keyword));
			},
			"removeKeyword",
			[](LuaSpell& a_form, const sol::object& a_keyword) {
				return a_form.form->As<RE::SpellItem>()->RemoveKeyword(CheckKeyword(a_keyword));
			});

		a_lua.new_usertype<LuaMagicEffect>(
			"MagicEffect", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaMagicEffect>), sol::meta_function::to_string,
			[](const LuaMagicEffect& a_form) { return fmt::format("MagicEffect[{:08X}]", a_form.form->GetFormID()); },
			"baseCost",
			sol::property(
				[](const LuaMagicEffect& a_form) { return a_form.form->As<RE::EffectSetting>()->data.baseCost; },
				[](LuaMagicEffect& a_form, double a_value) {
					a_form.form->As<RE::EffectSetting>()->data.baseCost = static_cast<float>(a_value);
				}),
			"minimumSkill",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::EffectSetting>()->data.minimumSkill);
				},
				[](LuaMagicEffect& a_form, lua_Integer a_value) {
					a_form.form->As<RE::EffectSetting>()->data.minimumSkill = static_cast<std::int32_t>(a_value);
				}),
			"spellmakingArea",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::EffectSetting>()->data.spellmakingArea);
				},
				[](LuaMagicEffect& a_form, lua_Integer a_value) {
					a_form.form->As<RE::EffectSetting>()->data.spellmakingArea = static_cast<std::int32_t>(a_value);
				}),
			"spellmakingChargeTime",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return a_form.form->As<RE::EffectSetting>()->data.spellmakingChargeTime;
				},
				[](LuaMagicEffect& a_form, double a_value) {
					a_form.form->As<RE::EffectSetting>()->data.spellmakingChargeTime = static_cast<float>(a_value);
				}),
			"taperWeight",
			sol::property(
				[](const LuaMagicEffect& a_form) { return a_form.form->As<RE::EffectSetting>()->data.taperWeight; },
				[](LuaMagicEffect& a_form, double a_value) {
					a_form.form->As<RE::EffectSetting>()->data.taperWeight = static_cast<float>(a_value);
				}),
			"taperCurve",
			sol::property(
				[](const LuaMagicEffect& a_form) { return a_form.form->As<RE::EffectSetting>()->data.taperCurve; },
				[](LuaMagicEffect& a_form, double a_value) {
					a_form.form->As<RE::EffectSetting>()->data.taperCurve = static_cast<float>(a_value);
				}),
			"skillUsageMult",
			sol::property(
				[](const LuaMagicEffect& a_form) { return a_form.form->As<RE::EffectSetting>()->data.skillUsageMult; },
				[](LuaMagicEffect& a_form, double a_value) {
					a_form.form->As<RE::EffectSetting>()->data.skillUsageMult = static_cast<float>(a_value);
				}),
			"associatedSkill",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return std::string(ActorValueName(a_form.form->As<RE::EffectSetting>()->data.associatedSkill));
				},
				[](LuaMagicEffect& a_form, const std::string& a_value) {
					RE::ActorValue actorValue = RE::ActorValue::kNone;
					if (!TryParseActorValue(a_value, actorValue)) {
						throw sol::error{
							"invalid ActorValue "
							"(Alteration/Conjuration/Destruction/Illusion/Restoration/Enchanting/None)"
						};
					}
					a_form.form->As<RE::EffectSetting>()->data.associatedSkill = actorValue;
				}),
			"resistVariable", sol::property([](const LuaMagicEffect& a_form) {
				return std::string(ActorValueName(a_form.form->As<RE::EffectSetting>()->data.resistVariable));
			}),
			"castingType",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return std::string(CastingTypeName(a_form.form->As<RE::EffectSetting>()->data.castingType));
				},
				[](LuaMagicEffect& a_form, const std::string& a_value) {
					RE::MagicSystem::CastingType type = RE::MagicSystem::CastingType::kConstantEffect;
					if (!TryParseCastingType(a_value, type)) {
						throw sol::error{ "invalid castingType" };
					}
					a_form.form->As<RE::EffectSetting>()->data.castingType = type;
				}),
			"delivery",
			sol::property(
				[](const LuaMagicEffect& a_form) {
					return std::string(DeliveryName(a_form.form->As<RE::EffectSetting>()->data.delivery));
				},
				[](LuaMagicEffect& a_form, const std::string& a_value) {
					RE::MagicSystem::Delivery delivery = RE::MagicSystem::Delivery::kSelf;
					if (!TryParseDelivery(a_value, delivery)) {
						throw sol::error{ "invalid delivery" };
					}
					a_form.form->As<RE::EffectSetting>()->data.delivery = delivery;
				}),
			"archetype", sol::property([](const LuaMagicEffect& a_form) {
				return std::string(ArchetypeName(a_form.form->As<RE::EffectSetting>()->data.archetype));
			}),
			"isHostile", sol::property([](const LuaMagicEffect& a_form) {
				return a_form.form->As<RE::EffectSetting>()->IsHostile();
			}),
			"isDetrimental", sol::property([](const LuaMagicEffect& a_form) {
				return a_form.form->As<RE::EffectSetting>()->IsDetrimental();
			}),
			"addKeyword",
			[](LuaMagicEffect& a_form, const sol::object& a_keyword) {
				return a_form.form->As<RE::EffectSetting>()->AddKeyword(CheckKeyword(a_keyword));
			},
			"removeKeyword",
			[](LuaMagicEffect& a_form, const sol::object& a_keyword) {
				return a_form.form->As<RE::EffectSetting>()->RemoveKeyword(CheckKeyword(a_keyword));
			});

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allSpells"] = &AllSpells;
		patcher["allMagicEffects"] = &AllMagicEffects;
	}
}