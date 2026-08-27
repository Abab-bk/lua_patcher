#include "LuaApi.h"

#include "PCH.h"

#include <RE/E/EffectSetting.h>
#include <RE/S/SpellItem.h>

#include <cstring>
#include <string_view>

namespace
{
	RE::TESForm* ToSpellForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(
			luaL_checkudata(a_state, a_index, LuaPatcher::kSpellMeta.data()));
	}

	RE::TESForm* ToMagicEffectForm(lua_State* a_state, int a_index)
	{
		return *static_cast<RE::TESForm**>(
			luaL_checkudata(a_state, a_index, LuaPatcher::kMagicEffectMeta.data()));
	}

	// ---- Spell helpers ----
	std::string_view SpellTypeName(RE::MagicSystem::SpellType a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::SpellType::kSpell: return "Spell";
		case RE::MagicSystem::SpellType::kDisease: return "Disease";
		case RE::MagicSystem::SpellType::kPower: return "Power";
		case RE::MagicSystem::SpellType::kLesserPower: return "LesserPower";
		case RE::MagicSystem::SpellType::kAbility: return "Ability";
		case RE::MagicSystem::SpellType::kPoison: return "Poison";
		case RE::MagicSystem::SpellType::kEnchantment: return "Enchantment";
		case RE::MagicSystem::SpellType::kPotion: return "Potion";
		case RE::MagicSystem::SpellType::kIngredient: return "Ingredient";
		case RE::MagicSystem::SpellType::kLeveledSpell: return "LeveledSpell";
		case RE::MagicSystem::SpellType::kAddiction: return "Addiction";
		case RE::MagicSystem::SpellType::kVoicePower: return "VoicePower";
		default: return "Other";
		}
	}

	bool TryParseSpellType(std::string_view a_sv, RE::MagicSystem::SpellType& a_out)
	{
		if (a_sv == "Spell") { a_out = RE::MagicSystem::SpellType::kSpell; return true; }
		if (a_sv == "Disease") { a_out = RE::MagicSystem::SpellType::kDisease; return true; }
		if (a_sv == "Power") { a_out = RE::MagicSystem::SpellType::kPower; return true; }
		if (a_sv == "LesserPower") { a_out = RE::MagicSystem::SpellType::kLesserPower; return true; }
		if (a_sv == "Ability") { a_out = RE::MagicSystem::SpellType::kAbility; return true; }
		if (a_sv == "Poison") { a_out = RE::MagicSystem::SpellType::kPoison; return true; }
		return false;
	}

	std::string_view CastingTypeName(RE::MagicSystem::CastingType a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::CastingType::kConstantEffect: return "ConstantEffect";
		case RE::MagicSystem::CastingType::kFireAndForget: return "FireAndForget";
		case RE::MagicSystem::CastingType::kConcentration: return "Concentration";
		case RE::MagicSystem::CastingType::kScroll: return "Scroll";
		default: return "Other";
		}
	}

	bool TryParseCastingType(std::string_view a_sv, RE::MagicSystem::CastingType& a_out)
	{
		if (a_sv == "ConstantEffect") { a_out = RE::MagicSystem::CastingType::kConstantEffect; return true; }
		if (a_sv == "FireAndForget") { a_out = RE::MagicSystem::CastingType::kFireAndForget; return true; }
		if (a_sv == "Concentration") { a_out = RE::MagicSystem::CastingType::kConcentration; return true; }
		if (a_sv == "Scroll") { a_out = RE::MagicSystem::CastingType::kScroll; return true; }
		return false;
	}

	std::string_view DeliveryName(RE::MagicSystem::Delivery a_type)
	{
		switch (a_type) {
		case RE::MagicSystem::Delivery::kSelf: return "Self";
		case RE::MagicSystem::Delivery::kTouch: return "Touch";
		case RE::MagicSystem::Delivery::kAimed: return "Aimed";
		case RE::MagicSystem::Delivery::kTargetActor: return "TargetActor";
		case RE::MagicSystem::Delivery::kTargetLocation: return "TargetLocation";
		default: return "Other";
		}
	}

	bool TryParseDelivery(std::string_view a_sv, RE::MagicSystem::Delivery& a_out)
	{
		if (a_sv == "Self") { a_out = RE::MagicSystem::Delivery::kSelf; return true; }
		if (a_sv == "Touch") { a_out = RE::MagicSystem::Delivery::kTouch; return true; }
		if (a_sv == "Aimed") { a_out = RE::MagicSystem::Delivery::kAimed; return true; }
		if (a_sv == "TargetActor") { a_out = RE::MagicSystem::Delivery::kTargetActor; return true; }
		if (a_sv == "TargetLocation") { a_out = RE::MagicSystem::Delivery::kTargetLocation; return true; }
		return false;
	}

	std::string_view ActorValueName(RE::ActorValue a_av)
	{
		switch (a_av) {
		case RE::ActorValue::kAlteration: return "Alteration";
		case RE::ActorValue::kConjuration: return "Conjuration";
		case RE::ActorValue::kDestruction: return "Destruction";
		case RE::ActorValue::kIllusion: return "Illusion";
		case RE::ActorValue::kRestoration: return "Restoration";
		case RE::ActorValue::kEnchanting: return "Enchanting";
		case RE::ActorValue::kNone: return "None";
		default: return "Other";
		}
	}

	bool TryParseActorValue(std::string_view a_sv, RE::ActorValue& a_out)
	{
		if (a_sv == "Alteration") { a_out = RE::ActorValue::kAlteration; return true; }
		if (a_sv == "Conjuration") { a_out = RE::ActorValue::kConjuration; return true; }
		if (a_sv == "Destruction") { a_out = RE::ActorValue::kDestruction; return true; }
		if (a_sv == "Illusion") { a_out = RE::ActorValue::kIllusion; return true; }
		if (a_sv == "Restoration") { a_out = RE::ActorValue::kRestoration; return true; }
		if (a_sv == "Enchanting") { a_out = RE::ActorValue::kEnchanting; return true; }
		if (a_sv == "None") { a_out = RE::ActorValue::kNone; return true; }
		return false;
	}

	std::string_view ArchetypeName(RE::EffectArchetypes::ArchetypeID a_arch)
	{
		switch (a_arch) {
		case RE::EffectArchetypes::ArchetypeID::kValueModifier: return "ValueModifier";
		case RE::EffectArchetypes::ArchetypeID::kScript: return "Script";
		case RE::EffectArchetypes::ArchetypeID::kDispel: return "Dispel";
		case RE::EffectArchetypes::ArchetypeID::kCureDisease: return "CureDisease";
		case RE::EffectArchetypes::ArchetypeID::kAbsorb: return "Absorb";
		case RE::EffectArchetypes::ArchetypeID::kDualValueModifier: return "DualValueModifier";
		case RE::EffectArchetypes::ArchetypeID::kCalm: return "Calm";
		case RE::EffectArchetypes::ArchetypeID::kDemoralize: return "Demoralize";
		case RE::EffectArchetypes::ArchetypeID::kFrenzy: return "Frenzy";
		case RE::EffectArchetypes::ArchetypeID::kDisarm: return "Disarm";
		case RE::EffectArchetypes::ArchetypeID::kCommandSummoned: return "CommandSummoned";
		case RE::EffectArchetypes::ArchetypeID::kInvisibility: return "Invisibility";
		case RE::EffectArchetypes::ArchetypeID::kLight: return "Light";
		default: return "Other";
		}
	}

	// ---- Spell methods ----
	int Spell_AddKeyword(lua_State* a_state)
	{
		auto* form = ToSpellForm(a_state, 1);
		auto* spell = form->As<RE::SpellItem>();
		auto* kw = LuaPatcher::CheckForm(a_state, 2)->As<RE::BGSKeyword>();
		if (!kw) return luaL_argerror(a_state, 2, "expected a keyword form");
		bool ok = spell->AddKeyword(kw);
		lua_pushboolean(a_state, ok);
		return 1;
	}

	int Spell_RemoveKeyword(lua_State* a_state)
	{
		auto* form = ToSpellForm(a_state, 1);
		auto* spell = form->As<RE::SpellItem>();
		auto* kw = LuaPatcher::CheckForm(a_state, 2)->As<RE::BGSKeyword>();
		if (!kw) return luaL_argerror(a_state, 2, "expected a keyword form");
		bool ok = spell->RemoveKeyword(kw);
		lua_pushboolean(a_state, ok);
		return 1;
	}

	int SpellIndex(lua_State* a_state)
	{
		auto* form = ToSpellForm(a_state, 1);
		auto* spell = form->As<RE::SpellItem>();
		const auto key = luaL_checkstring(a_state, 2);
		std::string_view sv(key);

		if (sv == "costOverride") { lua_pushinteger(a_state, spell->data.costOverride); return 1; }
		if (sv == "spellType") { auto n = SpellTypeName(spell->data.spellType); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "castingType") { auto n = CastingTypeName(spell->data.castingType); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "delivery") { auto n = DeliveryName(spell->data.delivery); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "chargeTime") { lua_pushnumber(a_state, spell->data.chargeTime); return 1; }
		if (sv == "castDuration") { lua_pushnumber(a_state, spell->data.castDuration); return 1; }
		if (sv == "range") { lua_pushnumber(a_state, spell->data.range); return 1; }
		if (sv == "addKeyword") { lua_pushcfunction(a_state, Spell_AddKeyword); return 1; }
		if (sv == "removeKeyword") { lua_pushcfunction(a_state, Spell_RemoveKeyword); return 1; }

		return LuaPatcher::FormIndexCommon(a_state, form, sv);
	}

	int SpellNewIndex(lua_State* a_state)
	{
		auto* form = ToSpellForm(a_state, 1);
		auto* spell = form->As<RE::SpellItem>();
		const char* key = luaL_checkstring(a_state, 2);

		if (std::strcmp(key, "costOverride") == 0) {
			spell->data.costOverride = static_cast<std::int32_t>(luaL_checkinteger(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "spellType") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::MagicSystem::SpellType t;
			if (!TryParseSpellType(sv, t)) return luaL_argerror(a_state, 3, "invalid spellType (Spell/Disease/Power/LesserPower/Ability/Poison)");
			spell->data.spellType = t;
			return 0;
		}
		if (std::strcmp(key, "castingType") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::MagicSystem::CastingType t;
			if (!TryParseCastingType(sv, t)) return luaL_argerror(a_state, 3, "invalid castingType");
			spell->data.castingType = t;
			return 0;
		}
		if (std::strcmp(key, "delivery") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::MagicSystem::Delivery d;
			if (!TryParseDelivery(sv, d)) return luaL_argerror(a_state, 3, "invalid delivery");
			spell->data.delivery = d;
			return 0;
		}
		if (std::strcmp(key, "chargeTime") == 0) {
			spell->data.chargeTime = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "castDuration") == 0) {
			spell->data.castDuration = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "range") == 0) {
			spell->data.range = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}

		return luaL_error(a_state, "property '%s' is read-only or not writable on Spell", key);
	}

	// ---- MagicEffect methods ----
	int MGEF_AddKeyword(lua_State* a_state)
	{
		auto* form = ToMagicEffectForm(a_state, 1);
		auto* mgef = form->As<RE::EffectSetting>();
		auto* kw = LuaPatcher::CheckForm(a_state, 2)->As<RE::BGSKeyword>();
		if (!kw) return luaL_argerror(a_state, 2, "expected a keyword form");
		bool ok = mgef->AddKeyword(kw);
		lua_pushboolean(a_state, ok);
		return 1;
	}

	int MGEF_RemoveKeyword(lua_State* a_state)
	{
		auto* form = ToMagicEffectForm(a_state, 1);
		auto* mgef = form->As<RE::EffectSetting>();
		auto* kw = LuaPatcher::CheckForm(a_state, 2)->As<RE::BGSKeyword>();
		if (!kw) return luaL_argerror(a_state, 2, "expected a keyword form");
		bool ok = mgef->RemoveKeyword(kw);
		lua_pushboolean(a_state, ok);
		return 1;
	}

	int MagicEffectIndex(lua_State* a_state)
	{
		auto* form = ToMagicEffectForm(a_state, 1);
		auto* mgef = form->As<RE::EffectSetting>();
		const auto key = luaL_checkstring(a_state, 2);
		std::string_view sv(key);

		if (sv == "baseCost") { lua_pushnumber(a_state, mgef->data.baseCost); return 1; }
		if (sv == "minimumSkill") { lua_pushinteger(a_state, mgef->data.minimumSkill); return 1; }
		if (sv == "spellmakingArea") { lua_pushinteger(a_state, mgef->data.spellmakingArea); return 1; }
		if (sv == "spellmakingChargeTime") { lua_pushnumber(a_state, mgef->data.spellmakingChargeTime); return 1; }
		if (sv == "taperWeight") { lua_pushnumber(a_state, mgef->data.taperWeight); return 1; }
		if (sv == "taperCurve") { lua_pushnumber(a_state, mgef->data.taperCurve); return 1; }
		if (sv == "skillUsageMult") { lua_pushnumber(a_state, mgef->data.skillUsageMult); return 1; }
		if (sv == "associatedSkill") { auto n = ActorValueName(mgef->data.associatedSkill); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "resistVariable") { auto n = ActorValueName(mgef->data.resistVariable); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "castingType") { auto n = CastingTypeName(mgef->data.castingType); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "delivery") { auto n = DeliveryName(mgef->data.delivery); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "archetype") { auto n = ArchetypeName(mgef->data.archetype); lua_pushlstring(a_state, n.data(), n.size()); return 1; }
		if (sv == "isHostile") { lua_pushboolean(a_state, mgef->IsHostile()); return 1; }
		if (sv == "isDetrimental") { lua_pushboolean(a_state, mgef->IsDetrimental()); return 1; }
		if (sv == "addKeyword") { lua_pushcfunction(a_state, MGEF_AddKeyword); return 1; }
		if (sv == "removeKeyword") { lua_pushcfunction(a_state, MGEF_RemoveKeyword); return 1; }

		return LuaPatcher::FormIndexCommon(a_state, form, sv);
	}

	int MagicEffectNewIndex(lua_State* a_state)
	{
		auto* form = ToMagicEffectForm(a_state, 1);
		auto* mgef = form->As<RE::EffectSetting>();
		const char* key = luaL_checkstring(a_state, 2);

		if (std::strcmp(key, "baseCost") == 0) {
			mgef->data.baseCost = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "minimumSkill") == 0) {
			mgef->data.minimumSkill = static_cast<std::int32_t>(luaL_checkinteger(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "spellmakingArea") == 0) {
			mgef->data.spellmakingArea = static_cast<std::int32_t>(luaL_checkinteger(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "spellmakingChargeTime") == 0) {
			mgef->data.spellmakingChargeTime = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "taperWeight") == 0) {
			mgef->data.taperWeight = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "taperCurve") == 0) {
			mgef->data.taperCurve = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "skillUsageMult") == 0) {
			mgef->data.skillUsageMult = static_cast<float>(luaL_checknumber(a_state, 3));
			return 0;
		}
		if (std::strcmp(key, "associatedSkill") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::ActorValue av;
			if (!TryParseActorValue(sv, av)) return luaL_argerror(a_state, 3, "invalid ActorValue (Alteration/Conjuration/Destruction/Illusion/Restoration/Enchanting/None)");
			mgef->data.associatedSkill = av;
			return 0;
		}
		if (std::strcmp(key, "castingType") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::MagicSystem::CastingType t;
			if (!TryParseCastingType(sv, t)) return luaL_argerror(a_state, 3, "invalid castingType");
			mgef->data.castingType = t;
			return 0;
		}
		if (std::strcmp(key, "delivery") == 0) {
			std::string_view sv = luaL_checkstring(a_state, 3);
			RE::MagicSystem::Delivery d;
			if (!TryParseDelivery(sv, d)) return luaL_argerror(a_state, 3, "invalid delivery");
			mgef->data.delivery = d;
			return 0;
		}

		return luaL_error(a_state, "property '%s' is read-only or not writable on MagicEffect", key);
	}

	int SpellToString(lua_State* a_state)
	{
		auto* form = ToSpellForm(a_state, 1);
		lua_pushstring(a_state, fmt::format("Spell[{:08X}]", form->GetFormID()).c_str());
		return 1;
	}

	int MagicEffectToString(lua_State* a_state)
	{
		auto* form = ToMagicEffectForm(a_state, 1);
		lua_pushstring(a_state, fmt::format("MagicEffect[{:08X}]", form->GetFormID()).c_str());
		return 1;
	}

	template <class T>
	int PushFormArray(lua_State* a_state)
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<T>();
		lua_createtable(a_state, static_cast<int>(forms.size()), 0);
		lua_Integer idx = 1;
		for (auto* form : forms) {
			LuaPatcher::PushForm(a_state, form);
			lua_rawseti(a_state, -2, idx++);
		}
		return 1;
	}

	int AllSpells(lua_State* a_state) { return PushFormArray<RE::SpellItem>(a_state); }
	int AllMagicEffects(lua_State* a_state) { return PushFormArray<RE::EffectSetting>(a_state); }
}

namespace LuaPatcher
{
	void RegisterMagic(lua_State* a_state)
	{
		luaL_newmetatable(a_state, kSpellMeta.data());
		lua_pushcfunction(a_state, SpellIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, SpellNewIndex);
		lua_setfield(a_state, -2, "__newindex");
		lua_pushcfunction(a_state, SpellToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		luaL_newmetatable(a_state, kMagicEffectMeta.data());
		lua_pushcfunction(a_state, MagicEffectIndex);
		lua_setfield(a_state, -2, "__index");
		lua_pushcfunction(a_state, MagicEffectNewIndex);
		lua_setfield(a_state, -2, "__newindex");
		lua_pushcfunction(a_state, MagicEffectToString);
		lua_setfield(a_state, -2, "__tostring");
		lua_pop(a_state, 1);

		lua_getglobal(a_state, "lua_patcher");
		lua_pushcfunction(a_state, AllSpells);
		lua_setfield(a_state, -2, "allSpells");
		lua_pushcfunction(a_state, AllMagicEffects);
		lua_setfield(a_state, -2, "allMagicEffects");
		lua_pop(a_state, 1);
	}
}
