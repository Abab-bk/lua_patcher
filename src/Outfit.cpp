#include "LuaApi.h"
#include "RE/B/BGSOutfit.h"
#include "RE/T/TESForm.h"

namespace
{
	RE::BGSOutfit* ToOutfit(const LuaPatcher::LuaOutfit& a_form) { return a_form.form->As<RE::BGSOutfit>(); }

	sol::object AllOutfits(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::BGSOutfit>();

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
	void RegisterOutfits(sol::state_view& a_lua)
	{
		auto type =
			a_lua.new_usertype<LuaOutfit>("Outfit", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
				sol::readonly_property(UnknownPropertyGetter<LuaOutfit>), sol::meta_function::to_string,
				[](const LuaOutfit& a_form) { return fmt::format("Outfit[{:08X}]", a_form.form->GetFormID()); });

		type["items"] = [](const LuaOutfit& a_form, sol::this_state a_state) -> sol::object {
			sol::state_view lua(a_state);
			auto* outfit = ToOutfit(a_form);

			sol::table result = lua.create_table(static_cast<int>(outfit->outfitItems.size()), 0);
			lua_Integer index = 1;

			for (auto* entry : outfit->outfitItems) {
				if (entry) {
					result[index++] = *entry;
				}
			}

			return result;
		};

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allOutfits"] = &AllOutfits;
	}
}
