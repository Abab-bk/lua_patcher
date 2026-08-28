#include "LuaApi.h"

#include <RE/B/BGSBipedObjectForm.h>
#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESObjectARMO.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
	std::string_view WeaponTypeName(RE::WEAPON_TYPE a_type)
	{
		switch (a_type) {
		case RE::WEAPON_TYPE::kHandToHandMelee:
			return "HandToHandMelee";
		case RE::WEAPON_TYPE::kOneHandSword:
			return "OneHandedSword";
		case RE::WEAPON_TYPE::kOneHandDagger:
			return "OneHandedDagger";
		case RE::WEAPON_TYPE::kOneHandAxe:
			return "OneHandedAxe";
		case RE::WEAPON_TYPE::kOneHandMace:
			return "OneHandedMace";
		case RE::WEAPON_TYPE::kTwoHandSword:
			return "TwoHandedSword";
		case RE::WEAPON_TYPE::kTwoHandAxe:
			return "TwoHandedAxe";
		case RE::WEAPON_TYPE::kBow:
			return "Bow";
		case RE::WEAPON_TYPE::kStaff:
			return "Staff";
		case RE::WEAPON_TYPE::kCrossbow:
			return "Crossbow";
		default:
			return "Other";
		}
	}

	// The weapon's governing skill. Note the game's enum is kArchery but the
	// in-game skill is named "Marksman".
	std::string_view WeaponSkillName(const RE::TESObjectWEAP* a_weapon)
	{
		if (a_weapon->weaponData.skill == RE::ActorValue::kOneHanded) {
			return "OneHanded";
		}
		if (a_weapon->weaponData.skill == RE::ActorValue::kTwoHanded) {
			return "TwoHanded";
		}
		if (a_weapon->weaponData.skill == RE::ActorValue::kArchery) {
			return "Marksman";
		}
		if (a_weapon->weaponData.skill == RE::ActorValue::kNone) {
			return "None";
		}
		return "Other";
	}

	// ---- keyword helpers (shared by Weapon/Armor) ----
	RE::BGSKeyword* CheckKeyword(const sol::object& a_value)
	{
		auto* keyword = LuaPatcher::CheckForm(a_value)->As<RE::BGSKeyword>();
		if (!keyword) {
			throw sol::error{ "expected a keyword form" };
		}
		return keyword;
	}

	bool AddKeyword(RE::TESForm* a_form, const sol::object& a_value)
	{
		auto* keywordForm = a_form->As<RE::BGSKeywordForm>();
		return keywordForm && keywordForm->AddKeyword(CheckKeyword(a_value));
	}

	bool RemoveKeyword(RE::TESForm* a_form, const sol::object& a_value)
	{
		auto* keywordForm = a_form->As<RE::BGSKeywordForm>();
		return keywordForm && keywordForm->RemoveKeyword(CheckKeyword(a_value));
	}

	std::string_view ArmorTypeName(RE::BIPED_MODEL::ArmorType a_type)
	{
		switch (a_type) {
		case RE::BIPED_MODEL::ArmorType::kLightArmor:
			return "Light";
		case RE::BIPED_MODEL::ArmorType::kHeavyArmor:
			return "Heavy";
		case RE::BIPED_MODEL::ArmorType::kClothing:
			return "Clothing";
		default:
			return "Other";
		}
	}

	std::string_view SlotName(RE::BIPED_MODEL::BipedObjectSlot a_slot)
	{
		switch (a_slot) {
		case RE::BIPED_MODEL::BipedObjectSlot::kHead:
			return "Head";
		case RE::BIPED_MODEL::BipedObjectSlot::kHair:
			return "Hair";
		case RE::BIPED_MODEL::BipedObjectSlot::kBody:
			return "Body";
		case RE::BIPED_MODEL::BipedObjectSlot::kHands:
			return "Hands";
		case RE::BIPED_MODEL::BipedObjectSlot::kForearms:
			return "Forearms";
		case RE::BIPED_MODEL::BipedObjectSlot::kAmulet:
			return "Amulet";
		case RE::BIPED_MODEL::BipedObjectSlot::kRing:
			return "Ring";
		case RE::BIPED_MODEL::BipedObjectSlot::kFeet:
			return "Feet";
		case RE::BIPED_MODEL::BipedObjectSlot::kCalves:
			return "Calves";
		case RE::BIPED_MODEL::BipedObjectSlot::kShield:
			return "Shield";
		case RE::BIPED_MODEL::BipedObjectSlot::kTail:
			return "Tail";
		case RE::BIPED_MODEL::BipedObjectSlot::kLongHair:
			return "LongHair";
		case RE::BIPED_MODEL::BipedObjectSlot::kCirclet:
			return "Circlet";
		case RE::BIPED_MODEL::BipedObjectSlot::kEars:
			return "Ears";
		default:
			return "Other";
		}
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

	sol::object AllWeapons(sol::this_state a_state) { return PushFormArray<RE::TESObjectWEAP>(a_state); }

	sol::object AllArmors(sol::this_state a_state) { return PushFormArray<RE::TESObjectARMO>(a_state); }
}

namespace LuaPatcher
{
	void RegisterEquipment(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaWeapon>(
			"Weapon", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaWeapon>), sol::meta_function::to_string,
			[](const LuaWeapon& a_form) { return fmt::format("Weapon[{:08X}]", a_form.form->GetFormID()); }, "damage",
			sol::property(
				[](const LuaWeapon& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::TESObjectWEAP>()->GetAttackDamage());
				},
				[](LuaWeapon& a_form, lua_Integer a_value) {
					auto* weapon = a_form.form->As<RE::TESObjectWEAP>();
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					weapon->attackDamage = static_cast<std::uint16_t>(a_value);
				}),

			"speed",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->GetSpeed(); },
				[](LuaWeapon& a_form, double a_value) {
					a_form.form->As<RE::TESObjectWEAP>()->weaponData.speed = static_cast<float>(a_value);
				}),
			"reach",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->GetReach(); },
				[](LuaWeapon& a_form, double a_value) {
					a_form.form->As<RE::TESObjectWEAP>()->weaponData.reach = static_cast<float>(a_value);
				}),
			"stagger",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->GetStagger(); },
				[](LuaWeapon& a_form, double a_value) {
					a_form.form->As<RE::TESObjectWEAP>()->weaponData.staggerValue = static_cast<float>(a_value);
				}),
			"critDamage",
			sol::property(
				[](const LuaWeapon& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::TESObjectWEAP>()->GetCritDamage());
				},
				[](LuaWeapon& a_form, lua_Integer a_value) {
					auto* weapon = a_form.form->As<RE::TESObjectWEAP>();
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					weapon->criticalData.damage = static_cast<std::uint16_t>(a_value);
				}),

			"enchantment",
			sol::property(
				[](const LuaWeapon& a_form) -> sol::optional<LuaEnchantment> {
					auto* enchantable = a_form.form->As<RE::TESEnchantableForm>();
					if (enchantable && enchantable->formEnchanting) {
						return LuaEnchantment{ enchantable->formEnchanting };
					}
					return sol::nullopt;
				},
				[](LuaWeapon& a_form, const sol::object& a_value) {
					auto* enchantable = a_form.form->As<RE::TESEnchantableForm>();
					if (a_value.is<sol::nil_t>()) {
						enchantable->formEnchanting = nullptr;
						return;
					}
					auto* enchantment = CheckForm(a_value)->As<RE::EnchantmentItem>();
					if (!enchantment) {
						throw sol::error{ "enchantment must be an enchantment form or nil" };
					}
					enchantable->formEnchanting = enchantment;
				}),

			"weight",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->weight; },
				[](LuaWeapon& a_form, double a_value) {
					a_form.form->As<RE::TESObjectWEAP>()->weight = static_cast<float>(a_value);
				}),
			"value",
			sol::property(
				[](const LuaWeapon& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::TESObjectWEAP>()->value);
				},
				[](LuaWeapon& a_form, lua_Integer a_value) {
					auto* weapon = a_form.form->As<RE::TESObjectWEAP>();
					a_value = std::max<lua_Integer>(a_value, 0);
					weapon->value = static_cast<std::int32_t>(a_value);
				}),
			"weaponType", sol::property([](const LuaWeapon& a_form) {
				return std::string(WeaponTypeName(a_form.form->As<RE::TESObjectWEAP>()->GetWeaponType()));
			}),
			"skill", sol::property([](const LuaWeapon& a_form) {
				return std::string(WeaponSkillName(a_form.form->As<RE::TESObjectWEAP>()));
			}),
			"melee",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->IsMelee(); }),
			"ranged",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->IsRanged(); }),
			"bow", sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->IsBow(); }),
			"staff",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->IsStaff(); }),
			"crossbow",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->IsCrossbow(); }),
			"playable",
			sol::property([](const LuaWeapon& a_form) { return a_form.form->As<RE::TESObjectWEAP>()->GetPlayable(); }),
			"addKeyword",
			[](LuaWeapon& a_form, const sol::object& a_keyword) { return AddKeyword(a_form.form, a_keyword); },
			"removeKeyword",
			[](LuaWeapon& a_form, const sol::object& a_keyword) { return RemoveKeyword(a_form.form, a_keyword); });

		a_lua.new_usertype<LuaArmor>(
			"Armor", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaArmor>), sol::meta_function::to_string,
			[](const LuaArmor& a_form) { return fmt::format("Armor[{:08X}]", a_form.form->GetFormID()); },

			"enchantment",
			sol::property(
				[](const LuaArmor& a_form) -> sol::optional<LuaEnchantment> {
					auto* enchantable = a_form.form->As<RE::TESEnchantableForm>();
					if (enchantable && enchantable->formEnchanting) {
						return LuaEnchantment{ enchantable->formEnchanting };
					}
					return sol::nullopt;
				},
				[](LuaArmor& a_form, const sol::object& a_value) {
					auto* enchantable = a_form.form->As<RE::TESEnchantableForm>();
					if (a_value.is<sol::nil_t>()) {
						enchantable->formEnchanting = nullptr;
						return;
					}
					auto* enchantment = CheckForm(a_value)->As<RE::EnchantmentItem>();
					if (!enchantment) {
						throw sol::error{ "enchantment must be an enchantment form or nil" };
					}
					enchantable->formEnchanting = enchantment;
				}),

			"armorRating",
			sol::property([](const LuaArmor& a_form) { return a_form.form->As<RE::TESObjectARMO>()->GetArmorRating(); },
				[](LuaArmor& a_form, double a_value) {
					auto* armor = a_form.form->As<RE::TESObjectARMO>();
					a_value = std::max<double>(a_value, 0);
					armor->armorRating = static_cast<std::uint32_t>(std::lround(a_value * 100.0));
				}),
			"armorType", sol::property([](const LuaArmor& a_form) {
				const auto* biped = a_form.form->As<RE::BGSBipedObjectForm>();
				return biped ? std::string(ArmorTypeName(biped->GetArmorType())) : std::string("Other");
			}),
			"slots", sol::property([](const LuaArmor& a_form) {
				const auto* biped = a_form.form->As<RE::BGSBipedObjectForm>();
				std::vector<std::string> result;
				if (biped) {
					const auto mask = biped->GetSlotMask();
					for (std::uint32_t bit = 1; bit != 0; bit <<= 1) {
						const auto slot = static_cast<RE::BIPED_MODEL::BipedObjectSlot>(bit);
						if (mask.any(slot)) {
							result.emplace_back(SlotName(slot));
						}
					}
				}
				return result;
			}),
			"playable", sol::property([](const LuaArmor& a_form) {
				return (a_form.form->GetFormFlags() & RE::TESForm::RecordFlags::kNonPlayable) == 0;
			}),
			"weight",
			sol::property([](const LuaArmor& a_form) { return a_form.form->As<RE::TESObjectARMO>()->weight; },
				[](LuaArmor& a_form, double a_value) {
					a_form.form->As<RE::TESObjectARMO>()->weight = static_cast<float>(a_value);
				}),
			"value",
			sol::property(
				[](const LuaArmor& a_form) {
					return static_cast<lua_Integer>(a_form.form->As<RE::TESObjectARMO>()->value);
				},
				[](LuaArmor& a_form, lua_Integer a_value) {
					auto* armor = a_form.form->As<RE::TESObjectARMO>();
					a_value = std::max<lua_Integer>(a_value, 0);
					armor->value = static_cast<std::int32_t>(a_value);
				}),

			"addKeyword",
			[](LuaArmor& a_form, const sol::object& a_keyword) { return AddKeyword(a_form.form, a_keyword); },

			"removeKeyword",
			[](LuaArmor& a_form, const sol::object& a_keyword) { return RemoveKeyword(a_form.form, a_keyword); });

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allWeapons"] = &AllWeapons;
		patcher["allArmors"] = &AllArmors;
	}
}