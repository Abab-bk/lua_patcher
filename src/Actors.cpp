#include "LuaApi.h"

#include <RE/T/TESNPC.h>
#include <RE/T/TESRaceForm.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
	RE::TESNPC* ToActor(const LuaPatcher::LuaActor& a_form) { return a_form.form->As<RE::TESNPC>(); }

	// The 18 NPC skill slots, in engine order (TESNPC::Skills indices).
	constexpr std::array<std::string_view, RE::TESNPC::Skills::kTotal> kSkillNames = {
		"OneHanded",
		"TwoHanded",
		"Marksman",
		"Block",
		"Smithing",
		"HeavyArmor",
		"LightArmor",
		"Pickpocket",
		"Lockpicking",
		"Sneak",
		"Alchemy",
		"Speechcraft",
		"Alteration",
		"Conjuration",
		"Destruction",
		"Illusion",
		"Restoration",
		"Enchanting",
	};

	// Resolves a skill slot by 1-based index (1..18) or name.
	std::size_t ParseSkillIndex(const sol::object& a_skill)
	{
		if (a_skill.is<double>()) {
			const auto index = static_cast<std::size_t>(a_skill.as<double>());
			if (index < 1 || index > kSkillNames.size()) {
				throw sol::error{ "bad skill index (expected 1..18)" };
			}
			return index - 1;
		}
		if (a_skill.is<std::string>()) {
			const std::string_view name = a_skill.as<std::string_view>();
			for (std::size_t i = 0; i < kSkillNames.size(); ++i) {
				if (kSkillNames[i] == name) {
					return i;
				}
			}
			throw sol::error{ "unknown skill name" };
		}
		throw sol::error{ "bad skill argument (expected a skill name or a 1..18 index)" };
	}

	sol::object AllActors(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::TESNPC>();

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
	void RegisterActors(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaActor>(
			"Actor", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaActor>), sol::meta_function::to_string,
			[](const LuaActor& a_form) { return fmt::format("Actor[{:08X}]", a_form.form->GetFormID()); }, "level",
			sol::property(
				[](const LuaActor& a_form) { return static_cast<lua_Integer>(ToActor(a_form)->actorData.level); },
				[](LuaActor& a_form, lua_Integer a_value) {
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					ToActor(a_form)->actorData.level = static_cast<std::uint16_t>(a_value);
				}),
			"health",
			sol::property(
				[](const LuaActor& a_form) { return static_cast<lua_Integer>(ToActor(a_form)->playerSkills.health); },
				[](LuaActor& a_form, lua_Integer a_value) {
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					ToActor(a_form)->playerSkills.health = static_cast<std::uint16_t>(a_value);
				}),
			"magicka",
			sol::property(
				[](const LuaActor& a_form) { return static_cast<lua_Integer>(ToActor(a_form)->playerSkills.magicka); },
				[](LuaActor& a_form, lua_Integer a_value) {
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					ToActor(a_form)->playerSkills.magicka = static_cast<std::uint16_t>(a_value);
				}),
			"stamina",
			sol::property(
				[](const LuaActor& a_form) { return static_cast<lua_Integer>(ToActor(a_form)->playerSkills.stamina); },
				[](LuaActor& a_form, lua_Integer a_value) {
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					ToActor(a_form)->playerSkills.stamina = static_cast<std::uint16_t>(a_value);
				}),
			"race", sol::property([](const LuaActor& a_form) -> sol::optional<LuaForm> {
				if (auto* race = ToActor(a_form)->race) {
					return LuaForm{ race };
				}
				return sol::nullopt;
			}),
			"npcClass", sol::property([](const LuaActor& a_form) -> sol::optional<LuaForm> {
				if (auto* npcClass = ToActor(a_form)->npcClass) {
					return LuaForm{ npcClass };
				}
				return sol::nullopt;
			}),
			"skills",
			[](const LuaActor& a_form, sol::this_state a_state) -> sol::object {
				sol::state_view lua(a_state);
				auto* actor = ToActor(a_form);

				sol::table result = lua.create_table(static_cast<int>(kSkillNames.size()), 0);
				for (std::size_t i = 0; i < kSkillNames.size(); ++i) {
					sol::table row = lua.create_table(0, 3);
					row["name"] = std::string(kSkillNames[i]);
					row["value"] = static_cast<lua_Integer>(actor->playerSkills.values[i]);
					row["offset"] = static_cast<lua_Integer>(actor->playerSkills.offsets[i]);
					result[static_cast<lua_Integer>(i) + 1] = row;
				}
				return result;
			},
			"setSkill",
			[](LuaActor& a_form, const sol::object& a_skill, lua_Integer a_value) {
				auto* actor = ToActor(a_form);
				const auto index = ParseSkillIndex(a_skill);
				a_value = std::max<lua_Integer>(a_value, 0);
				a_value = std::min<lua_Integer>(a_value, 0xFF);
				actor->playerSkills.values[index] = static_cast<std::uint8_t>(a_value);
			});

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allActors"] = &AllActors;
	}
}