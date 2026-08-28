#pragma once

#include <sol/sol.hpp>

#include <cstdint>
#include <unordered_set>

namespace RE
{
	class TESForm;
}

namespace LuaPatcher
{
	// Runtime quest-alias protection set: every form referenced by a loaded
	// quest's aliases (forced references, created objects, unique actors).
	// Mod quests are covered this way; the vanilla/CC dataset shipped with
	// EverythingRandomizer covers the alias fields CommonLibSSE does not
	// decode (locations, factions, spells, items, script refs).
	//
	// Snapshot semantics: built once on first use (kDataLoaded context) and
	// cached for the session.
	void BuildQuestProtection();

	// True when a_form is referenced by any loaded quest alias.
	bool IsQuestReferenced(RE::TESForm* a_form);

	// Lua entry point: resolves formOrId and checks the quest protection set.
	bool IsQuestReferencedLua(const sol::object& a_form);

	// Registers `lua_patcher.isQuestReferenced(formOrId)`.
	void RegisterProtection(sol::state_view& a_lua);
}