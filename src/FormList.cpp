#include "LuaApi.h"

#include <RE/B/BGSListForm.h>

#include <algorithm>
#include <cstdint>
#include <string>

// Windows.h (pulled in through the PCH) includes winspool.h, which defines an
// object-like AddForm -> AddFormA macro for the print-spooler API. Undef it so
// RE::BGSListForm::AddForm keeps its name (the harness on Linux never sees
// winspool.h, so this only matters for the Windows build).
#undef AddForm

namespace
{
	RE::BGSListForm* ToFormList(const LuaPatcher::LuaFormList& a_list) { return a_list.form->As<RE::BGSListForm>(); }

	// The engine exposes no BGSListForm::RemoveForm; removal goes through the
	// same copy-modify-write-back cycle as leveled list entries.
	std::vector<RE::TESForm*> SnapshotForms(RE::BGSListForm* a_list)
	{
		std::vector<RE::TESForm*> result;
		result.reserve(a_list->forms.size());
		for (auto* form : a_list->forms) {
			result.push_back(form);
		}
		return result;
	}

	void WriteForms(RE::BGSListForm* a_list, const std::vector<RE::TESForm*>& a_forms)
	{
		a_list->forms.clear();
		for (auto* form : a_forms) {
			a_list->forms.push_back(form);
		}
	}

	// ---- lua_patcher form list functions ----

	sol::object FormListGet(sol::this_state a_state, const sol::object& a_form)
	{
		sol::state_view lua(a_state);
		auto* form = LuaPatcher::CheckForm(a_form);
		if (!form->As<RE::BGSListForm>()) {
			throw sol::error{ "form is not a form list" };
		}
		return LuaPatcher::PushForm(lua, form);
	}

	sol::object AllFormLists(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::BGSListForm>();

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
	void RegisterFormList(sol::state_view& a_lua)
	{
		sol::usertype<LuaFormList> type = a_lua.new_usertype<LuaFormList>("FormList", sol::base_classes,
			sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaFormList>), sol::meta_function::to_string,
			[](const LuaFormList& a_list) { return fmt::format("FormList[{:08X}]", a_list.form->GetFormID()); });

		type["numForms"] = sol::property(
			[](const LuaFormList& a_list) { return static_cast<lua_Integer>(ToFormList(a_list)->forms.size()); });

		type["forms"] = [](const LuaFormList& a_list, sol::this_state a_state) -> sol::object {
			sol::state_view lua(a_state);
			auto* list = ToFormList(a_list);

			sol::table result = lua.create_table(static_cast<int>(list->forms.size()), 0);
			lua_Integer index = 1;
			for (auto* form : list->forms) {
				result[index++] = LuaPatcher::PushForm(lua, form);
			}
			return result;
		};

		// Set semantics: adding an already-present form is a no-op (false).
		type["add"] = [](LuaFormList& a_list, const sol::object& a_form) {
			auto* list = ToFormList(a_list);
			auto* form = CheckForm(a_form);
			if (list->HasForm(form)) {
				return false;
			}
			list->AddForm(form);
			return true;
		};

		type["remove"] = [](LuaFormList& a_list, const sol::object& a_form) {
			auto* list = ToFormList(a_list);
			auto* form = CheckForm(a_form);

			auto forms = SnapshotForms(list);
			const auto before = forms.size();
			const auto result = std::ranges::remove(forms, form);
			forms.erase(result.begin(), result.end());

			if (forms.size() != before) {
				WriteForms(list, forms);
				return true;
			}
			return false;
		};

		type["has"] = [](const LuaFormList& a_list, const sol::object& a_form) {
			auto* list = ToFormList(a_list);
			auto* form = CheckForm(a_form);
			return list->HasForm(form);
		};

		type["clear"] = [](LuaFormList& a_list) { ToFormList(a_list)->forms.clear(); };

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["formList"] = &FormListGet;
		patcher["allFormLists"] = &AllFormLists;
	}
}