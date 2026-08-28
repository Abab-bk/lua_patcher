#pragma once
// Internal helpers shared by the Alchemy/Enchantment registrations: reading and
// rewriting a MagicItem's effect list (IngredientItem / AlchemyItem /
// EnchantmentItem all store their effects in RE::MagicItem::effects).
// Not part of the public API surface; docs/API.md is generated from the *.cpp
// registration calls only.

#include "LuaApi.h"

#include <RE/E/Effect.h>
#include <RE/M/MagicItem.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace LuaPatcher::Effects
{
	// Strict numeric option read (raises on a present-but-non-number value),
	// mirroring LeveledList's options-table handling.
	inline double GetNumberOption(const sol::table& a_options, std::string_view a_key, double a_default)
	{
		const auto value = a_options.get<sol::optional<sol::object>>(a_key);
		if (!value) {
			return a_default;
		}
		if (!value->is<double>()) {
			throw sol::error{ fmt::format("bad options-table value '{}' (expected a number)", a_key) };
		}
		return value->as<double>();
	}

	// One effect slot snapshot: { baseEffect = <MagicEffect or nil>, magnitude,
	// area, duration, cost }.
	inline sol::table PushEffect(sol::state_view a_lua, const RE::Effect& a_effect)
	{
		sol::table row = a_lua.create_table(0, 5);
		row["baseEffect"] = LuaPatcher::PushForm(a_lua, a_effect.baseEffect);
		row["magnitude"] = a_effect.effectItem.magnitude;
		row["area"] = static_cast<lua_Integer>(a_effect.effectItem.area);
		row["duration"] = static_cast<lua_Integer>(a_effect.effectItem.duration);
		row["cost"] = a_effect.cost;
		return row;
	}

	// Array of effect slot snapshots for a magic item.
	inline sol::table PushEffectList(sol::state_view a_lua, RE::MagicItem* a_item)
	{
		sol::table result = a_lua.create_table(static_cast<int>(a_item->effects.size()), 0);
		lua_Integer index = 1;
		for (auto* effect : a_item->effects) {
			if (!effect) {
				continue;
			}
			result[index++] = PushEffect(a_lua, *effect);
		}
		return result;
	}

	// Resolves the effect's base effect from a snapshot row: the baseEffect
	// field may be nil (empty slot) or a form identifier/form.
	inline RE::EffectSetting* ParseBaseEffect(const sol::table& a_row)
	{
		const auto base = a_row.get<sol::optional<sol::object>>("baseEffect");
		if (!base || base->is<sol::nil_t>()) {
			return nullptr;
		}
		auto* form = LuaPatcher::CheckForm(*base);
		auto* effect = form->As<RE::EffectSetting>();
		if (!effect) {
			throw sol::error{ "baseEffect must be a magic effect form" };
		}
		return effect;
	}

	// Builds a RE::Effect from a snapshot row { baseEffect, magnitude?, area?,
	// duration?, cost? }; a bare form value (no table) means a zeroed effect.
	inline RE::Effect* ParseEffect(const sol::object& a_value)
	{
		auto* effect = new RE::Effect();
		if (a_value.is<sol::table>()) {
			const auto row = a_value.as<sol::table>();
			effect->baseEffect = ParseBaseEffect(row);
			effect->effectItem.magnitude =
				static_cast<float>(GetNumberOption(row, "magnitude", effect->effectItem.magnitude));
			effect->effectItem.area = static_cast<std::uint32_t>(
				static_cast<lua_Integer>(GetNumberOption(row, "area", static_cast<double>(effect->effectItem.area))));
			effect->effectItem.duration = static_cast<std::uint32_t>(static_cast<lua_Integer>(
				GetNumberOption(row, "duration", static_cast<double>(effect->effectItem.duration))));
			effect->cost = static_cast<float>(GetNumberOption(row, "cost", effect->cost));
		} else if (!a_value.is<sol::nil_t>()) {
			effect->baseEffect = LuaPatcher::CheckForm(a_value)->As<RE::EffectSetting>();
			if (!effect->baseEffect) {
				throw sol::error{ "expected a magic effect form or an effects-table entry" };
			}
		}
		return effect;
	}

	// Replaces the item's effect list with the given array of snapshots.
	inline void SetEffectList(RE::MagicItem* a_item, const sol::object& a_list)
	{
		if (!a_list.is<sol::table>()) {
			throw sol::error{ "expected an array of effect entries" };
		}
		const auto rows = a_list.as<sol::table>();

		std::vector<RE::Effect*> effects;
		effects.reserve(rows.size());
		for (std::size_t i = 1; i <= rows.size(); ++i) {
			effects.push_back(ParseEffect(rows.get<sol::object>(static_cast<lua_Integer>(i))));
		}

		a_item->effects.clear();
		for (auto* effect : effects) {
			a_item->effects.push_back(effect);
		}
	}

	// Appends one effect slot; returns the index of the new slot (1-based).
	// `addEffect(baseEffect, { magnitude?, area?, duration?, cost? })` with
	// baseEffect as a form/id (or nil for an empty slot), or
	// `addEffect({ baseEffect, magnitude, area, duration, cost })` as a full
	// snapshot table.
	inline lua_Integer AddEffect(RE::MagicItem* a_item, const sol::object& a_base, const sol::object& a_options)
	{
		auto* effect = ParseEffect(a_base);

		if (a_base.is<sol::table>()) {
			if (!a_options.is<sol::nil_t>()) {
				throw sol::error{
					"bad argument #2 to 'addEffect' (expected nothing when the first argument is a snapshot table)"
				};
			}
		} else if (a_options.is<sol::table>()) {
			const auto opts = a_options.as<sol::table>();
			effect->effectItem.magnitude =
				static_cast<float>(GetNumberOption(opts, "magnitude", effect->effectItem.magnitude));
			effect->effectItem.area = static_cast<std::uint32_t>(
				static_cast<lua_Integer>(GetNumberOption(opts, "area", static_cast<double>(effect->effectItem.area))));
			effect->effectItem.duration = static_cast<std::uint32_t>(static_cast<lua_Integer>(
				GetNumberOption(opts, "duration", static_cast<double>(effect->effectItem.duration))));
			effect->cost = static_cast<float>(GetNumberOption(opts, "cost", effect->cost));
		} else if (!a_options.is<sol::nil_t>()) {
			throw sol::error{ "bad argument #2 to 'addEffect' (expected an options table or nothing)" };
		}

		a_item->effects.push_back(effect);
		return static_cast<lua_Integer>(a_item->effects.size());
	}

	inline void ClearEffects(RE::MagicItem* a_item) { a_item->effects.clear(); }
}  // namespace LuaPatcher::Effects