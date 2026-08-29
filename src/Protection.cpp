#include "Protection.h"

#include "LuaApi.h"

#include <RE/B/BGSBaseAlias.h>
#include <RE/B/BGSRefAlias.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESQuest.h>

#include <cstdint>
#include <unordered_set>

namespace
{
	const std::unordered_set<RE::FormID>& QuestProtectionSet()
	{
		static RE::TESDataHandler* owner = nullptr;
		static std::unordered_set<RE::FormID> protectedForms;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (owner != dataHandler) {
			protectedForms.clear();
			owner = dataHandler;
			if (dataHandler) {
				for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
					for (auto* alias : quest->aliases) {
						// BGSBaseAlias is not a TESForm; the concrete class is
						// identified through the virtual QType() ("Ref").
						if (!alias || alias->GetTypeString() != "Ref"sv) {
							continue;
						}
						auto* refAlias = static_cast<RE::BGSRefAlias*>(alias);

						// The fill data is a union; only the field matching the
						// alias's fill type is valid.
						switch (alias->fillType.get()) {
						case RE::BGSBaseAlias::FILL_TYPE::kForced:
							{
								const auto handle = refAlias->fillData.forced.forcedRef.native_handle();
								if (handle != 0) {
									protectedForms.insert(static_cast<RE::FormID>(handle >> 16));
								}
								break;
							}
						case RE::BGSBaseAlias::FILL_TYPE::kCreated:
							{
								if (auto* object = refAlias->fillData.created.object) {
									protectedForms.insert(object->GetFormID());
								}
								break;
							}
						case RE::BGSBaseAlias::FILL_TYPE::kUniqueActor:
							{
								if (auto* actor = refAlias->fillData.uniqueActor.uniqueActor) {
									protectedForms.insert(actor->GetFormID());
								}
								break;
							}
						default:
							break;
						}
					}
				}
				logger::info("LuaPatcher: quest protection set built ({} forms)", protectedForms.size());
			}
		}
		return protectedForms;
	}
}

namespace LuaPatcher
{
	void BuildQuestProtection() { (void)QuestProtectionSet(); }

	bool IsQuestReferenced(RE::TESForm* a_form) { return a_form && QuestProtectionSet().contains(a_form->GetFormID()); }

	// Lua: lua_patcher.isQuestReferenced(formOrId) -> bool
	bool IsQuestReferencedLua(sol::variadic_args a_args)
	{
		auto* form = ParseFormRef(a_args, "isQuestReferenced").form;
		return IsQuestReferenced(form);
	}

	void RegisterProtection(sol::state_view& a_lua)
	{
		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["isQuestReferenced"] = [](sol::variadic_args a_args) {
			auto* form = LuaPatcher::ParseFormRef(a_args, "isQuestReferenced").form;
			return LuaPatcher::IsQuestReferenced(form);
		};
	}
}