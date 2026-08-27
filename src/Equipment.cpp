#include "LuaApi.h"

#include "PCH.h"

#include <RE/B/BGSBipedObjectForm.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectWEAP.h>

#include <cstdint>
#include <string_view>

namespace
{
	RE::TESForm* ToWeaponForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(
			luaL_checkudata(a_state, a_index, LuaPatcher::kWeaponMeta.data()));
	}

	RE::TESForm* ToArmorForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(
			luaL_checkudata(a_state, a_index, LuaPatcher::kArmorMeta.data()));
	}

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

	int WeaponIndex(lua_State* a_state)
	{
		auto*      form = ToWeaponForm(a_state, 1);
		auto*      weapon = form->As<RE::TESObjectWEAP>();
		const auto key = luaL_checkstring(a_state, 2);

		if (std::string_view(key) == "damage") {
			lua_pushinteger(a_state, weapon->GetAttackDamage());
			return 1;
		}
		if (std::string_view(key) == "speed") {
			lua_pushnumber(a_state, weapon->GetSpeed());
			return 1;
		}
		if (std::string_view(key) == "reach") {
			lua_pushnumber(a_state, weapon->GetReach());
			return 1;
		}
		if (std::string_view(key) == "stagger") {
			lua_pushnumber(a_state, weapon->GetStagger());
			return 1;
		}
		if (std::string_view(key) == "critDamage") {
			lua_pushinteger(a_state, weapon->GetCritDamage());
			return 1;
		}
		if (std::string_view(key) == "weaponType") {
			const auto type = WeaponTypeName(weapon->GetWeaponType());
			lua_pushlstring(a_state, type.data(), type.size());
			return 1;
		}
		if (std::string_view(key) == "skill") {
			const auto skill = WeaponSkillName(weapon);
			lua_pushlstring(a_state, skill.data(), skill.size());
			return 1;
		}
		if (std::string_view(key) == "melee") {
			lua_pushboolean(a_state, weapon->IsMelee());
			return 1;
		}
		if (std::string_view(key) == "ranged") {
			lua_pushboolean(a_state, weapon->IsRanged());
			return 1;
		}
		if (std::string_view(key) == "bow") {
			lua_pushboolean(a_state, weapon->IsBow());
			return 1;
		}
		if (std::string_view(key) == "staff") {
			lua_pushboolean(a_state, weapon->IsStaff());
			return 1;
		}
		if (std::string_view(key) == "crossbow") {
			lua_pushboolean(a_state, weapon->IsCrossbow());
			return 1;
		}
		if (std::string_view(key) == "playable") {
			lua_pushboolean(a_state, weapon->GetPlayable());
			return 1;
		}

		return LuaPatcher::FormIndexCommon(a_state, form, key);
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

	int ArmorIndex(lua_State* a_state)
	{
		auto*      form = ToArmorForm(a_state, 1);
		auto*      armor = form->As<RE::TESObjectARMO>();
		const auto key = luaL_checkstring(a_state, 2);

		if (std::string_view(key) == "armorRating") {
			lua_pushnumber(a_state, armor->GetArmorRating());
			return 1;
		}
		if (std::string_view(key) == "armorType") {
			const auto* biped = armor->As<RE::BGSBipedObjectForm>();
			const auto  type = biped ? ArmorTypeName(biped->GetArmorType()) : std::string_view("Other");
			lua_pushlstring(a_state, type.data(), type.size());
			return 1;
		}
		if (std::string_view(key) == "slots") {
			const auto* biped = armor->As<RE::BGSBipedObjectForm>();
			lua_createtable(a_state, 0, 4);
			lua_Integer index = 1;
			if (biped) {
				const auto mask = biped->GetSlotMask();
				for (std::uint32_t bit = 1; bit != 0; bit <<= 1) {
					const auto slot = static_cast<RE::BIPED_MODEL::BipedObjectSlot>(bit);
					if (mask.any(slot)) {
						const auto name = SlotName(slot);
						lua_pushlstring(a_state, name.data(), name.size());
						lua_rawseti(a_state, -2, index++);
					}
				}
			}
			return 1;
		}
		if (std::string_view(key) == "playable") {
			const bool playable = (form->GetFormFlags() & RE::TESForm::RecordFlags::kNonPlayable) == 0;
			lua_pushboolean(a_state, playable);
			return 1;
		}

		return LuaPatcher::FormIndexCommon(a_state, form, key);
	}

	int WeaponToString(lua_State* a_state)
	{
		auto* form = ToWeaponForm(a_state, 1);
		lua_pushstring(a_state, fmt::format("Weapon[{:08X}]", form->GetFormID()).c_str());
		return 1;
	}

	int ArmorToString(lua_State* a_state)
	{
		auto* form = ToArmorForm(a_state, 1);
		lua_pushstring(a_state, fmt::format("Armor[{:08X}]", form->GetFormID()).c_str());
		return 1;
	}

	template <class T>
	int PushFormArray(lua_State* a_state)
	{
		auto*       dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<T>();

		lua_createtable(a_state, static_cast<int>(forms.size()), 0);
		lua_Integer index = 1;
		for (auto* form : forms) {
			LuaPatcher::PushForm(a_state, form);
			lua_rawseti(a_state, -2, index++);
		}
		return 1;
	}

	int AllWeapons(lua_State* a_state)
	{
		return PushFormArray<RE::TESObjectWEAP>(a_state);
	}

	int AllArmors(lua_State* a_state)
	{
		return PushFormArray<RE::TESObjectARMO>(a_state);
	}
}

namespace LuaPatcher
{
	void RegisterEquipment(lua_State* a_state)
	{
		luaL_newmetatable(a_state, kWeaponMeta.data());
		lua_pushcfunction(a_state, WeaponIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, WeaponToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		luaL_newmetatable(a_state, kArmorMeta.data());
		lua_pushcfunction(a_state, ArmorIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, ArmorToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		lua_getglobal(a_state, "lua_patcher");
		lua_pushcfunction(a_state, AllWeapons);
		lua_setfield(a_state, -2, "allWeapons");
		lua_pushcfunction(a_state, AllArmors);
		lua_setfield(a_state, -2, "allArmors");
		lua_pop(a_state, 1);
	}
}