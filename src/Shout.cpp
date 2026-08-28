#include "LuaApi.h"

#include <RE/S/SpellItem.h>
#include <RE/T/TESShout.h>
#include <RE/T/TESWordOfPower.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
	RE::TESShout* ToShout(const LuaPatcher::LuaShout& a_form) { return a_form.form->As<RE::TESShout>(); }

	constexpr std::size_t kVariationCount = RE::TESShout::VariationIDs::kTotal;

	sol::table PushVariation(sol::state_view a_lua, const RE::TESShout::Variation& a_variation)
	{
		sol::table row = a_lua.create_table(0, 3);
		row["word"] = LuaPatcher::PushForm(a_lua, a_variation.word);
		row["spell"] = LuaPatcher::PushForm(a_lua, a_variation.spell);
		row["recoveryTime"] = a_variation.recoveryTime;
		return row;
	}

	std::size_t CheckVariationIndex(lua_Integer a_index)
	{
		if (a_index < 1 || a_index > static_cast<lua_Integer>(kVariationCount)) {
			throw sol::error{ "bad variation index (expected 1..3)" };
		}
		return static_cast<std::size_t>(a_index) - 1;
	}

	// Parses a variation entry table { word?, spell?, recoveryTime? }; missing
	// fields keep the current variation's values, nil clears them.
	RE::TESShout::Variation ParseVariation(const sol::object& a_value, const RE::TESShout::Variation& a_current)
	{
		if (!a_value.is<sol::table>()) {
			throw sol::error{ "expected a { word, spell, recoveryTime } variation table" };
		}
		const auto row = a_value.as<sol::table>();

		RE::TESShout::Variation result = a_current;

		const auto word = row.get<sol::optional<sol::object>>("word");
		if (word) {
			if (word->is<sol::nil_t>()) {
				result.word = nullptr;
			} else {
				auto* form = LuaPatcher::CheckForm(*word);
				result.word = form->As<RE::TESWordOfPower>();
				if (!result.word) {
					throw sol::error{ "'word' must be a word of power form" };
				}
			}
		}

		const auto spell = row.get<sol::optional<sol::object>>("spell");
		if (spell) {
			if (spell->is<sol::nil_t>()) {
				result.spell = nullptr;
			} else {
				auto* form = LuaPatcher::CheckForm(*spell);
				result.spell = form->As<RE::SpellItem>();
				if (!result.spell) {
					throw sol::error{ "'spell' must be a spell form" };
				}
			}
		}

		const auto recoveryTime = row.get<sol::optional<sol::object>>("recoveryTime");
		if (recoveryTime) {
			if (!recoveryTime->is<double>()) {
				throw sol::error{ "bad variation value 'recoveryTime' (expected a number)" };
			}
			result.recoveryTime = static_cast<float>(recoveryTime->as<double>());
		}

		return result;
	}

	sol::object AllShouts(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::TESShout>();

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
	void RegisterShout(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaShout>(
			"Shout", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaShout>), sol::meta_function::to_string,
			[](const LuaShout& a_form) { return fmt::format("Shout[{:08X}]", a_form.form->GetFormID()); }, "variations",
			[](const LuaShout& a_form, sol::this_state a_state) -> sol::object {
				sol::state_view lua(a_state);
				auto* shout = ToShout(a_form);

				sol::table result = lua.create_table(static_cast<int>(kVariationCount), 0);
				for (std::size_t i = 0; i < kVariationCount; ++i) {
					result[static_cast<lua_Integer>(i) + 1] = PushVariation(lua, shout->variations[i]);
				}
				return result;
			},
			"setVariation",
			[](LuaShout& a_form, lua_Integer a_index, const sol::object& a_entry) {
				auto* shout = ToShout(a_form);
				const auto index = CheckVariationIndex(a_index);
				shout->variations[index] = ParseVariation(a_entry, shout->variations[index]);
			},
			"setVariations",
			[](LuaShout& a_form, const sol::object& a_list) {
				auto* shout = ToShout(a_form);
				if (!a_list.is<sol::table>()) {
					throw sol::error{ "expected an array of { word, spell, recoveryTime } variation tables" };
				}
				const auto rows = a_list.as<sol::table>();
				if (rows.size() > kVariationCount) {
					throw sol::error{ fmt::format("expected at most {} variation entries", kVariationCount) };
				}

				// validate every variation first so a bad table cannot leave
				// the shout half-mutated
				std::vector<RE::TESShout::Variation> planned;
				planned.reserve(rows.size());
				for (std::size_t i = 1; i <= rows.size(); ++i) {
					planned.push_back(
						ParseVariation(rows.get<sol::object>(static_cast<lua_Integer>(i)), shout->variations[i - 1]));
				}

				for (std::size_t i = 0; i < planned.size(); ++i) {
					shout->variations[i] = planned[i];
				}
			},
			"word",
			[](const LuaShout& a_form, lua_Integer a_index, sol::this_state a_state) {
				sol::state_view lua(a_state);
				const auto index = CheckVariationIndex(a_index);
				return PushForm(lua, ToShout(a_form)->variations[index].word);
			},
			"spell",
			[](const LuaShout& a_form, lua_Integer a_index, sol::this_state a_state) {
				sol::state_view lua(a_state);
				const auto index = CheckVariationIndex(a_index);
				return PushForm(lua, ToShout(a_form)->variations[index].spell);
			});

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allShouts"] = &AllShouts;
	}
}