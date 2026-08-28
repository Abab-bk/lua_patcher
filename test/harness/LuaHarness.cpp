// Linux smoke-test for the LuaPatcher Lua bindings.
//
// Compiles the real src/LuaApi.cpp, src/LeveledList.cpp, src/Equipment.cpp,
// src/Magic.cpp, src/FormList.cpp and src/ScriptLoader.cpp (as separate
// translation units, like the real build) against mocked RE types
// (test/harness/mocks), system Lua and the sol2 headers, then exercises the
// API from Lua and asserts the resulting game-state mutations.
//
// Build (from repo root):
//   cmake -S test/harness -B build/harness && cmake --build build/harness

#include <sol/sol.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include "LuaApi.h"
#include "ScriptLoader.h"
#include "Utils.h"

namespace
{
	void Check(bool a_condition, const char* a_message)
	{
		if (!a_condition) {
			std::fprintf(stderr, "FAIL: %s\n", a_message);
			std::exit(1);
		}
		std::printf("ok: %s\n", a_message);
	}

	// Runs a Lua chunk; on error prints the message (sol2 protected calls
	// already attach a stack traceback) and fails.
	void DoString(sol::state_view a_lua, const char* a_script)
	{
		sol::protected_function_result result = a_lua.safe_script(a_script, sol::script_pass_on_error);
		if (!result.valid()) {
			sol::error err = result;
			std::fprintf(stderr, "FAIL: %s\n", err.what());
			std::exit(1);
		}
	}

	// Pushes a mock form with the given properties.
	template <class T>
	T* AddForm(RE::FormID a_fullID, RE::FormType a_type, std::string a_editorId, std::string a_name,
		RE::TESFile* a_file)
	{
		auto* form = new T();
		form->formID = a_fullID;
		form->formType = a_type;
		form->editorId = a_editorId;
		form->name = std::move(a_name);
		if (a_file) {
			form->sourceFiles.push_back(a_file);
		}
		RE::MockFormsByID()[a_fullID] = form;
		if (!form->editorId.empty()) {
			RE::MockFormsByEditorID()[form->editorId] = form;
			RE::MockEditorIdMap().GetMap()[form->editorId] = form;
		}
		RE::TESDataHandler::MockForms<T>().push_back(form);
		return form;
	}

	RE::LEVELED_OBJECT MakeEntry(RE::TESForm* a_form, std::uint16_t a_count, std::uint16_t a_level)
	{
		RE::LEVELED_OBJECT entry{};
		entry.form = a_form;
		entry.count = a_count;
		entry.level = a_level;
		return entry;
	}
}

int main()
{
	// --- mock world -----------------------------------------------------
	auto* mod = new RE::TESFile();
	mod->fileName = "MockPlugin.esp";
	mod->compileIndex = 1;

	auto* mod2 = new RE::TESFile();
	mod2->fileName = "MockLight.esl";
	mod2->light = true;
	mod2->compileIndex = 0xFE;
	mod2->smallFileCompileIndex = 3;

	RE::TESDataHandler::mockMods["MockPlugin.esp"] = mod;
	RE::TESDataHandler::mockMods["MockLight.esl"] = mod2;

	auto* llMain = AddForm<RE::TESLevItem>(0x01000100, RE::FormType::LeveledItem, "MockLeveledList", "Mock Chest", mod);
	auto* llChar =
		AddForm<RE::TESLevCharacter>(0x01000400, RE::FormType::LeveledNPC, "MockCharList", "Mock Bandit", mod);
	auto* kwForm = AddForm<RE::BGSKeyword>(0x01000500, RE::FormType::Keyword, "MockKeyword", "MockKeyword", mod);
	auto* global = AddForm<RE::TESGlobal>(0x01000300, RE::FormType::Global, "MockGlobal", "MockGlobal", mod);
	auto* globalLight =
		AddForm<RE::TESGlobal>(0xFE003001, RE::FormType::Global, "MockLightGlobal", "MockLightGlobal", mod2);

	// seed the main list: two entries, then a third form
	auto* formA = AddForm<RE::TESForm>(0x01000A00, RE::FormType::Keyword, "MockFormA", "Form A", mod);
	auto* formB = AddForm<RE::TESForm>(0x01000B00, RE::FormType::Keyword, "MockFormB", "Form B", mod);
	auto* formC = AddForm<RE::TESForm>(0x01000C00, RE::FormType::Keyword, "MockFormC", "Form C", mod);
	auto* formD = AddForm<RE::TESForm>(0x01000D00, RE::FormType::Keyword, "MockFormD", "Form D", mod);
	auto* formE = AddForm<RE::TESForm>(0x01000E00, RE::FormType::Keyword, "MockFormE", "Form E", mod);
	(void)globalLight;
	(void)formC;
	(void)formD;
	(void)formE;

	// form list: seeded with formA/formB, exercised by the FormList API tests
	auto* formList =
		AddForm<RE::BGSListForm>(0x01000600, RE::FormType::FormList, "MockFormList", "Mock Form List", mod);
	formList->forms = { formA, formB };

	llMain->entries = {
		MakeEntry(formA, 1, 1),
		MakeEntry(formA, 1, 10),
		MakeEntry(formB, 3, 5),
	};
	llMain->numEntries = 3;
	llMain->llFlags = RE::TESLeveledList::Flag::kCalculateForEachItemInCount;

	// --- equipment mock world -------------------------------------------
	auto* gearMod = new RE::TESFile();
	gearMod->fileName = "GearMod.esp";
	gearMod->compileIndex = 3;
	RE::TESDataHandler::mockMods["GearMod.esp"] = gearMod;

	auto* vanillaMod = new RE::TESFile();
	vanillaMod->fileName = "Skyrim.esm";
	vanillaMod->compileIndex = 4;
	RE::TESDataHandler::mockMods["Skyrim.esm"] = vanillaMod;

	auto* sword = AddForm<RE::TESObjectWEAP>(0x03001000, RE::FormType::Weapon, "GearSword", "Gear Sword", gearMod);
	sword->attackDamage = 25;
	sword->weaponData.speed = 1.2f;
	sword->weaponData.reach = 1.3f;
	sword->weaponData.staggerValue = 0.5f;
	sword->weaponData.skill = RE::ActorValue::kOneHanded;
	sword->weaponData.animationType = RE::kOneHandSword;
	sword->criticalData.damage = 5;
	sword->value = 150;
	sword->weight = 12.0f;

	auto* sword2 = AddForm<RE::TESObjectWEAP>(0x03001001, RE::FormType::Weapon, "GearSword2", "Gear Sword 2", gearMod);
	sword2->weaponData.skill = RE::ActorValue::kOneHanded;
	sword2->weaponData.animationType = RE::kOneHandSword;
	sword2->weaponData.flags = 0x80;  // non-playable

	auto* bow = AddForm<RE::TESObjectWEAP>(0x03001002, RE::FormType::Weapon, "GearBow", "Gear Bow", gearMod);
	bow->weaponData.skill = RE::ActorValue::kArchery;
	bow->weaponData.animationType = RE::kBow;
	bow->attackDamage = 12;

	auto* chest = AddForm<RE::TESObjectARMO>(0x03002000, RE::FormType::Armor, "GearChest", "Gear Cuirass", gearMod);
	chest->armorRating = 2500;  // 25.0
	chest->value = 300;
	chest->weight = 20.0f;
	chest->bipedObjectSlots = RE::BIPED_MODEL::BipedObjectSlot::kBody;
	chest->armorType = RE::BIPED_MODEL::ArmorType::kHeavyArmor;

	auto* helmet = AddForm<RE::TESObjectARMO>(0x03002001, RE::FormType::Armor, "GearHelmet", "Gear Helmet", gearMod);
	helmet->armorRating = 1000;  // 10.0
	helmet->armorType = RE::BIPED_MODEL::ArmorType::kLightArmor;
	helmet->bipedObjectSlots = RE::BIPED_MODEL::BipedObjectSlot::kHead;

	auto* vanillaChest =
		AddForm<RE::TESObjectARMO>(0x04002000, RE::FormType::Armor, "VanillaChest", "Iron Cuirass", vanillaMod);
	vanillaChest->armorType = RE::BIPED_MODEL::ArmorType::kHeavyArmor;

	auto* enchant =
		AddForm<RE::EnchantmentItem>(0x03003000, RE::FormType::Enchantment, "GearEnch", "Gear Ench", gearMod);
	sword->formEnchanting = enchant;

	auto* gearKw = AddForm<RE::BGSKeyword>(0x03004000, RE::FormType::Keyword, "GearKeyword", "GearKeyword", gearMod);
	auto* gearKw2 = AddForm<RE::BGSKeyword>(0x03004001, RE::FormType::Keyword, "GearKeyword2", "GearKeyword2", gearMod);
	// heap-allocated: the mock AddKeyword/RemoveKeyword delete[] this array,
	// mirroring the real engine's ownership semantics
	auto* gearKwArray = new RE::BGSKeyword*[2]{ gearKw, gearKw2 };
	chest->keywords = gearKwArray;
	chest->numKeywords = 2;

	// material keywords the KeywordFixer example derives from item stats
	// (sword dmg 25 -> Daedric, bow dmg 12 -> Dwarven, chest 25 heavy -> Orcish,
	//  helmet 10 light -> Elven)
	auto* kwWeapDaedric = AddForm<RE::BGSKeyword>(0x03004002, RE::FormType::Keyword, "WeapMaterialDaedric",
		"WeapMaterialDaedric", gearMod);
	auto* kwWeapDwarven = AddForm<RE::BGSKeyword>(0x03004003, RE::FormType::Keyword, "WeapMaterialDwarven",
		"WeapMaterialDwarven", gearMod);
	auto* kwArmorOrcish = AddForm<RE::BGSKeyword>(0x03004004, RE::FormType::Keyword, "ArmorMaterialOrcish",
		"ArmorMaterialOrcish", gearMod);
	auto* kwArmorElven =
		AddForm<RE::BGSKeyword>(0x03004005, RE::FormType::Keyword, "ArmorMaterialElven", "ArmorMaterialElven", gearMod);

	// smithing material sets (BGSListForm) joined by the TemperingLists example
	auto* tempDaedric = AddForm<RE::BGSListForm>(0x03004006, RE::FormType::FormList, "WeapMaterialDaedricSet",
		"WeapMaterialDaedricSet", gearMod);
	auto* tempDwarven = AddForm<RE::BGSListForm>(0x03004007, RE::FormType::FormList, "WeapMaterialDwarvenSet",
		"WeapMaterialDwarvenSet", gearMod);
	auto* tempOrcish = AddForm<RE::BGSListForm>(0x03004008, RE::FormType::FormList, "ArmorMaterialOrcishSet",
		"ArmorMaterialOrcishSet", gearMod);
	auto* tempElven = AddForm<RE::BGSListForm>(0x03004009, RE::FormType::FormList, "ArmorMaterialElvenSet",
		"ArmorMaterialElvenSet", gearMod);

	// magic types exercised by the Spell/MagicEffect tests
	auto* spell = AddForm<RE::SpellItem>(0x03005000, RE::FormType::Spell, "GearSpell", "Gear Spell", gearMod);
	spell->data.spellType = RE::MagicSystem::SpellType::kAbility;
	spell->data.costOverride = 100;

	auto* mgef =
		AddForm<RE::EffectSetting>(0x03005001, RE::FormType::MagicEffect, "GearMgef", "Gear Magic Effect", gearMod);
	mgef->data.baseCost = 50.0F;
	mgef->data.associatedSkill = RE::ActorValue::kDestruction;

	// loot lists targeted by the GearInjection example (editorId prefix match
	// against the example's default targetPrefixes = { "LItem" })
	std::vector<RE::TESLevItem*> lootLists;
	for (const char* editorId : { "LItemPlayerLootLight", "LItemPlayerLootHeavy", "LItemPlayerLootOneHand",
			 "LItemPlayerLootTwoHand", "LItemPlayerLootRanged" }) {
		auto* list = AddForm<RE::TESLevItem>(0x05000000u + static_cast<RE::FormID>(lootLists.size()),
			RE::FormType::LeveledItem, editorId, editorId, gearMod);
		lootLists.push_back(list);
	}

	// a mod armor that is ALREADY assigned to a leveled list -> excluded
	auto* assignedArmor =
		AddForm<RE::TESObjectARMO>(0x03002002, RE::FormType::Armor, "GearAssigned", "Gear Assigned", gearMod);
	assignedArmor->armorType = RE::BIPED_MODEL::ArmorType::kLightArmor;
	lootLists[0]->entries.push_back(MakeEntry(assignedArmor, 1, 1));
	lootLists[0]->numEntries = 1;

	// --- Lua state ------------------------------------------------------
	sol::state lua;
	// sol2 prints C++ exceptions to stderr before converting them to Lua
	// errors; silence that (expected errors are caught with pcall below).
	lua.set_exception_handler([](lua_State* L, sol::optional<const std::exception&>, sol::string_view what) -> int {
		lua_pushlstring(L, what.data(), what.size());
		return 1;
	});
	lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine, sol::lib::string, sol::lib::os,
		sol::lib::math, sol::lib::table, sol::lib::utf8, sol::lib::io, sol::lib::debug);
	LuaPatcher::RegisterApi(lua);
	LuaPatcher::RegisterLeveledList(lua);
	LuaPatcher::RegisterEquipment(lua);
	LuaPatcher::RegisterMagic(lua);
	LuaPatcher::RegisterFormList(lua);

	// --- core API tests -------------------------------------------------
	DoString(lua, R"LUA(
assert(lua_patcher.version == "0.1.0")
local f = lua_patcher.getForm("MockPlugin.esp|00000100")
assert(f ~= nil, "getForm by mod|id")
assert(f.formId == 0x01000100)
assert(f.type == "LeveledItem")
assert(f.editorId == "MockLeveledList")
assert(f.name == "Mock Chest")
assert(f.identifier == "MockPlugin.esp|000100")
assert(lua_patcher.getForm("MockFormA").formId == 0x01000A00, "getForm by editorId")
assert(lua_patcher.getForm("MockPlugin.esp|00000A00").editorId == "MockFormA", "getForm id masking")
assert(lua_patcher.getForm("MockLight.esl|000001") ~= nil, "light plugin id")
assert(lua_patcher.getForm("NoSuchPlugin.esp|000001") == nil, "unknown plugin")
assert(lua_patcher.getForm("NoSuchEditorID") == nil, "unknown editorId")
assert(lua_patcher.isPluginInstalled("MockPlugin.esp") == true)
assert(lua_patcher.isPluginInstalled("MockLight.esl") == true)
assert(lua_patcher.isPluginInstalled("Missing.esp") == false)
assert(#lua_patcher.allLeveledItems() == 6)  -- llMain + 5 loot lists
assert(#lua_patcher.allLeveledCharacters() == 1)
)LUA");

	// --- form list API ----------------------------------------------------
	DoString(lua, R"LUA(
local fl = lua_patcher.formList("MockFormList")
assert(fl ~= nil)
assert(fl.type == "FormList")
assert(fl.formId == 0x01000600)
assert(tostring(fl) == "FormList[01000600]", "form list tostring")
assert(fl.numForms == 2)

local forms = fl:forms()
assert(forms[1].editorId == "MockFormA" and forms[2].editorId == "MockFormB")
assert(fl:has("MockFormA") == true)
assert(fl:has("MockFormC") == false)
assert(fl:has("MockPlugin.esp|00000A00") == true, "has by formId identifier")

-- set semantics: add of a present form is a no-op
assert(fl:add("MockFormA") == false)
assert(fl.numForms == 2)
assert(fl:add("MockFormC") == true)
assert(fl.numForms == 3)
assert(fl:has("MockFormC") == true)

-- remove
assert(fl:remove("MockFormC") == true)
assert(fl:has("MockFormC") == false)
assert(fl:remove("MockFormC") == false)
assert(fl.numForms == 2)

-- clear + restore
fl:clear()
assert(fl.numForms == 0)
fl:add("MockFormA")
assert(fl.numForms == 1)

-- identifier form and enumeration (MockFormList + 4 material sets)
assert(lua_patcher.formList("MockPlugin.esp|00000600").identifier == "MockPlugin.esp|000600")
assert(#lua_patcher.allFormLists() == 5)

-- typed dispatch: getForm returns the FormList usertype
assert(lua_patcher.getForm("MockFormList").type == "FormList")
assert(lua_patcher.getForm("MockFormList").editorId == "MockFormList")

-- errors
local ok, err = pcall(function() return lua_patcher.formList("MockFormA") end)
assert(not ok, "formList on a non-list form must fail")
ok, err = pcall(function() return fl.nope end)
assert(not ok, "unknown property must fail")
ok, err = pcall(function() fl:add("NoSuchEditorId") end)
assert(not ok, "missing form must fail")
)LUA");

	// --- leveled list operations ----------------------------------------
	DoString(lua, R"LUA(
local ll = lua_patcher.leveledList("MockPlugin.esp|00000100")
assert(ll ~= nil)
assert(ll.numEntries == 3)
assert(ll.calculateForEachItem == true)
assert(ll.calculateFromAllLevels == false and ll.useAll == false and ll.specialLoot == false)
assert(ll.chanceNone == 0)

-- read entries back
local entries = ll:entries()
assert(#entries == 3)
assert(entries[1].form.editorId == "MockFormA" and entries[1].count == 1 and entries[1].level == 1)
assert(entries[3].form.editorId == "MockFormB" and entries[3].count == 3 and entries[3].level == 5)

-- add positional (sorted by level)
ll:add("MockFormC", 7, 2)
assert(ll.numEntries == 4)
assert(ll:entries()[3].form.editorId == "MockFormC" and ll:entries()[3].level == 7)

-- addIfAbsent: existing form -> no duplicate
ll:addIfAbsent("MockFormA")
assert(ll.numEntries == 4)
-- addIfAbsent: new form -> added at level 2
ll:addIfAbsent("MockFormD", 2, 1)
assert(ll.numEntries == 5)
assert(ll:entries()[2].form.editorId == "MockFormD")

-- add with options table (duplicates are allowed for plain add)
ll:add("MockFormB", { level = 4, count = 1 })
assert(ll.numEntries == 6)
assert(ll:entries()[3].form.editorId == "MockFormB" and ll:entries()[3].level == 4)

-- remove with inclusive level bounds (level >= 6 of MockFormA)
ll:remove("MockFormA", { minLevel = 6 })
assert(ll.numEntries == 5)
for _, e in ipairs(ll:entries()) do
  assert(not (e.form.editorId == "MockFormA" and e.level >= 6))
end

-- remove with count bound (MockFormB count 3 removed, count 1 kept)
ll:remove("MockFormB", { minCount = 3 })
assert(ll.numEntries == 4)
assert(ll:has("MockFormB") == true)

-- remove plain (MockFormD)
ll:remove("MockFormD")
assert(ll.numEntries == 3)

-- replace MockFormC -> MockFormA
ll:replace("MockFormC", "MockFormA")
assert(ll.numEntries == 3)

-- multiplyCount (counts 1 and 2 -> ceil(1*2.5)=3, ceil(2*2.5)=5)
ll:multiplyCount("MockFormA", 2.5)
assert(ll:entries()[1].count == 3)
assert(ll:entries()[3].form.editorId == "MockFormA" and ll:entries()[3].count == 5)

-- chance/global
ll.chanceNone = 25
assert(ll.chanceNone == 25)
ll.chanceGlobal = "MockGlobal"
assert(ll.chanceGlobal.editorId == "MockGlobal")
ll.chanceGlobal = nil
assert(ll.chanceGlobal == nil)

-- named flags (no bit arithmetic): set/unset each bit independently
ll.calculateFromAllLevels = true
assert(ll.calculateFromAllLevels == true and ll.calculateForEachItem == true)
ll.calculateFromAllLevels = false
assert(ll.calculateFromAllLevels == false and ll.calculateForEachItem == true)
ll.useAll = true
ll.specialLoot = true
assert(ll.useAll == true and ll.specialLoot == true)
ll.useAll = false
ll.specialLoot = false
assert(ll.useAll == false and ll.specialLoot == false and ll.calculateForEachItem == true)

-- removeIf predicate (entry snapshots carry a typed form)
ll:add("MockFormB", 1, 1)
ll:add("MockFormC", { level = 3, count = 4 })
assert(ll.numEntries == 5)
ll:removeIf(function(e) return e.form.editorId == "MockFormA" and e.count >= 4 end)
assert(#ll:entries() == 4)
assert(ll:has("MockFormA") == true)
assert(ll:has("MockFormC") == true)
ll:removeIf(function() return false end)
assert(#ll:entries() == 4)
local ok, err = pcall(function() ll:removeIf(function() error("boom") end) end)
assert(not ok, "removeIf predicate error propagates")
assert(#ll:entries() == 4, "removeIf is atomic on predicate error")

-- clear
ll:clear()
assert(ll.numEntries == 0)
)LUA");

	// --- error paths -----------------------------------------------------
	DoString(lua, R"LUA(
local ok, err
ok, err = pcall(function() lua_patcher.getForm("MockPlugin.esp|ZZZZ") end)
assert(ok, "invalid hex id -> nil, not an error")
assert(lua_patcher.getForm("MockPlugin.esp|ZZZZ") == nil, "invalid hex id returns nil")

ok, err = pcall(function() lua_patcher.leveledList("MockKeyword") end)
assert(not ok, "non-leveled form must fail")

local ll = lua_patcher.leveledList("MockPlugin.esp|00000100")
ok, err = pcall(function() ll:add("MissingEditorId") end)
assert(not ok, "missing form must fail")

ok, err = pcall(function() ll:remove("MockFormA", "banana") end)
assert(not ok, "non-table remove options must fail")

ok, err = pcall(function() ll:add("MockFormA", "x") end)
assert(not ok, "bad level argument must fail")

ok, err = pcall(function() ll:add("MockFormA", { level = "x" }) end)
assert(not ok, "bad options-table value must fail")

ok, err = pcall(function() ll:removeIf(42) end)
assert(not ok, "removeIf requires a function")

ok, err = pcall(function() ll.useAll = "yes" end)
assert(not ok, "flag write with wrong type must fail")

ok, err = pcall(function() ll.calculateForEachItem = true end)
assert(ok, "flag write ok")
ll.calculateForEachItem = true

ok, err = pcall(function() ll.chanceNone = 5 end)
assert(ok, "chanceNone write ok")
ll.chanceNone = 0

ok, err = pcall(function() ll.nope = 1 end)
assert(not ok, "read-only property must fail")

ok, err = pcall(function() return ll.nope end)
assert(not ok, "unknown property must fail")

ok, err = pcall(function() ll:add("MockGlobal") end)
assert(ok, "adding a global form to a list is fine")
ll:remove("MockGlobal")

ok, err = pcall(function() ll.chanceGlobal = "MockKeyword" end)
assert(not ok, "non-global chanceGlobal must fail")

local form = lua_patcher.getForm("MockFormA")
assert(type(form.type) == "string" and type(form.typeId) == "number")
assert(form.type == "Keyword" and form.typeId > 0)

-- __tostring must produce real formIDs, not literal format specifiers
local ll0 = lua_patcher.leveledList("MockPlugin.esp|00000100")
assert(tostring(ll0) == "LeveledList[01000100]", "leveled list tostring")
assert(tostring(lua_patcher.getForm("MockPlugin.esp|00000100")) == "LeveledList[01000100]", "typed getForm tostring")
assert(tostring(form) == "Form[MockPlugin.esp|000A00|01000A00]", "plain form tostring")
)LUA");

	// --- C++ side assertions on mutated state ----------------------------
	Check(llMain->numEntries == 0, "main list was cleared by script");

	// restore main list state for the example-script run
	llMain->entries = { MakeEntry(formA, 1, 1) };
	llMain->numEntries = 1;
	llMain->llFlags = static_cast<RE::TESLeveledList::Flag>(0);

	// --- typed dispatch + equipment properties ---------------------------
	DoString(lua, R"LUA(
local sword = lua_patcher.getForm("GearMod.esp|00001000")
assert(sword ~= nil)
assert(sword.type == "Weapon")
assert(sword.damage == 25)
assert(math.abs(sword.speed - 1.2) < 0.001)
assert(math.abs(sword.reach - 1.3) < 0.001)
assert(math.abs(sword.stagger - 0.5) < 0.001)
assert(sword.critDamage == 5)
assert(sword.weaponType == "OneHandedSword")
assert(sword.skill == "OneHanded")
assert(sword.melee == true and sword.ranged == false and sword.bow == false)
assert(sword.playable == true)
assert(sword.value == 150)
assert(sword.weight == 12)
assert(sword.plugin == "GearMod.esp")
assert(sword.enchantment ~= nil and sword.enchantment.editorId == "GearEnch")
assert(tostring(sword):match("^Weapon%["), "weapon tostring")
assert(tostring(sword) == "Weapon[03001000]", "weapon tostring formID")

-- editorId comes from the game's editorID table, not the (empty) virtual
assert(sword.editorId == "GearSword", "editorId via table")

local bow = lua_patcher.getForm("GearBow")
assert(bow.bow == true and bow.ranged == true and bow.crossbow == false)
assert(bow.skill == "Marksman")

local sword2 = lua_patcher.getForm("GearSword2")
assert(sword2.playable == false)

local chest = lua_patcher.getForm("GearChest")
assert(chest.type == "Armor")
assert(chest.armorRating == 25)
assert(chest.armorType == "Heavy")
assert(chest.slots[1] == "Body")
assert(chest.playable == true)
assert(chest.value == 300 and chest.weight == 20)
assert(chest.plugin == "GearMod.esp")
assert(chest.enchantment == nil)
assert(#chest.keywords == 2)
assert(chest:hasKeyword("GearKeyword") == true)
assert(chest:hasKeyword("GearKeyword2") == true)
assert(chest:hasKeyword("MockKeyword") == false)
assert(tostring(chest):match("^Armor%["), "armor tostring")
assert(tostring(chest) == "Armor[03002000]", "armor tostring formID")

local helmet = lua_patcher.getForm("GearHelmet")
assert(helmet.armorType == "Light")
assert(helmet.slots[1] == "Head")
assert(helmet.armorRating == 10)

-- generic properties on a plain form
local plain = lua_patcher.getForm("MockFormA")
assert(plain.plugin == "MockPlugin.esp")
assert(plain.value == nil and plain.weight == nil and plain.enchantment == nil)
assert(#plain.keywords == 0)
)LUA");

	// --- magic types ------------------------------------------------------
	DoString(lua, R"LUA(
local spell = lua_patcher.getForm("GearSpell")
assert(spell ~= nil and spell.type == "Spell")
assert(spell.spellType == "Ability")
assert(spell.costOverride == 100)
assert(tostring(spell) == "Spell[03005000]", "spell tostring")
spell.costOverride = 250
assert(spell.costOverride == 250)
spell.spellType = "Power"
assert(spell.spellType == "Power")
local ok, err = pcall(function() spell.spellType = "Nope" end)
assert(not ok, "invalid spellType must fail")

local mgef = lua_patcher.getForm("GearMgef")
assert(mgef ~= nil and mgef.type == "MagicEffect")
assert(mgef.baseCost == 50)
assert(mgef.associatedSkill == "Destruction")
assert(mgef.isHostile == false and mgef.isDetrimental == false)
mgef.baseCost = 75
assert(mgef.baseCost == 75)
assert(tostring(mgef) == "MagicEffect[03005001]", "magic effect tostring")

assert(#lua_patcher.allSpells() == 1)
assert(#lua_patcher.allMagicEffects() == 1)
)LUA");

	// --- pure-logic priority parser ---------------------------------------
	Check(ExampleMod::ParseScriptPriority("-- priority: 20\nlocal x = 1") == 20, "priority: basic");
	Check(ExampleMod::ParseScriptPriority("-- priority = 30\n") == 30, "priority: '=' form");
	Check(ExampleMod::ParseScriptPriority("-- priority:10\n") == 10, "priority: no space");
	Check(ExampleMod::ParseScriptPriority("-- priority: 1.5\n") == 1, "priority: stops at non-digit");
	Check(ExampleMod::ParseScriptPriority("-- priority: nope\n") == 0, "priority: non-numeric");
	Check(ExampleMod::ParseScriptPriority("-- no marker here\n") == 0, "priority: missing");
	Check(ExampleMod::ParseScriptPriority("") == 0, "priority: empty");

	// --- assignment queries ----------------------------------------------
	DoString(lua, R"LUA(
local ll = lua_patcher.leveledList("MockPlugin.esp|00000100")
local llChar = lua_patcher.leveledList("MockPlugin.esp|00000400")
assert(ll:has("MockFormA") == true)
assert(ll:has("MockFormB") == false)
assert(llChar:has("MockFormA") == false)

-- formA appears twice in llMain: deduped to one result
local containing = lua_patcher.findLeveledListsContaining("MockFormA")
assert(#containing == 1)
assert(containing[1].editorId == "MockLeveledList")
assert(#lua_patcher.findLeveledListsContaining("MockFormC") == 0)
assert(#lua_patcher.findLeveledListsContaining("MockGlobal") == 0)

-- snapshot: adding an entry now does NOT appear in the cached index
llChar:add("MockFormA")
assert(#lua_patcher.findLeveledListsContaining("MockFormA") == 1)
llChar:clear()
)LUA");

	// --- run the shipped example script against the mock world ----------
	{
		// mark the guard plugin as "installed"
		auto* exampleMod = new RE::TESFile();
		exampleMod->fileName = "LuaPatcherExample.esp";
		exampleMod->compileIndex = 2;
		RE::TESDataHandler::mockMods["LuaPatcherExample.esp"] = exampleMod;

		auto* exChest =
			AddForm<RE::TESLevItem>(0x02001000, RE::FormType::LeveledItem, "ExampleChest", "Example Chest", exampleMod);
		auto* ex01 = AddForm<RE::TESForm>(0x02002000, RE::FormType::Keyword, "ExForm01", "Ex 01", exampleMod);
		auto* ex02 = AddForm<RE::TESForm>(0x02002001, RE::FormType::Keyword, "ExForm02", "Ex 02", exampleMod);
		auto* ex03 = AddForm<RE::TESForm>(0x02002002, RE::FormType::Keyword, "ExForm03", "Ex 03", exampleMod);
		auto* ex04 = AddForm<RE::TESForm>(0x02002003, RE::FormType::Keyword, "ExForm04", "Ex 04", exampleMod);
		auto* ex05 = AddForm<RE::TESForm>(0x02002004, RE::FormType::Keyword, "ExForm05", "Ex 05", exampleMod);
		auto* ex06 = AddForm<RE::TESForm>(0x02003000, RE::FormType::Keyword, "ExForm06", "Ex 06", exampleMod);
		auto* ex07 = AddForm<RE::TESForm>(0x02003001, RE::FormType::Keyword, "ExForm07", "Ex 07", exampleMod);
		auto* ex08 = AddForm<RE::TESForm>(0x02004000, RE::FormType::Keyword, "ExForm08", "Ex 08", exampleMod);
		auto* ex09 = AddForm<RE::TESForm>(0x02005000, RE::FormType::Keyword, "ExForm09", "Ex 09", exampleMod);
		(void)ex05;

		exChest->entries = {
			MakeEntry(ex04, 1, 1),   // removed by :remove (no condition)
			MakeEntry(ex01, 1, 1),   // replaced by ex06 via :replace
			MakeEntry(ex03, 1, 2),   // kept
			MakeEntry(ex02, 2, 20),  // removed by level ">5"
		};
		exChest->numEntries = 4;

		// keep the example script's read-only io/paths working (it does not use io)
		std::string example;
		{
			FILE* f = std::fopen("examples/snippets/LeveledLists.lua", "rb");
			Check(f != nullptr, "example script readable");
			char buf[4096];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				example.append(buf, n);
			}
			std::fclose(f);
		}
		DoString(lua, example.c_str());

		// Example semantics (0x2000..0x2004 = Form01..05, 0x3000/0x3001 = Form06/07,
		// 0x4000 = Form08, 0x5000 = Form09):
		//   seed: ex04(1,1), ex01(1,1), ex03(1,2), ex02(2,20)
		//   add ex01(1,1), add ex02(10,2)         -> duplicates allowed
		//   addIfAbsent ex03                     -> already present, skipped
		//   remove ex04 (plain)                   -> gone
		//   remove ex05 with {minLevel=6,minCount=2} -> not present, no-op
		//   replace ex06 -> ex07                  -> ex06 absent, no-op
		//   multiplyCount ex08 * 1.5              -> ex08 absent, no-op
		//   final: ex01 x2, ex02 x2, ex03, ex09 (sorted by level)
		Check(exChest->numEntries == 6, "example: final entry count");
		std::size_t n01 = 0, n02 = 0, n03 = 0, n09 = 0;
		bool has04 = false, has06 = false, has07 = false, has08 = false;
		for (const auto& e : exChest->entries) {
			if (e.form == ex01)
				++n01;
			if (e.form == ex02)
				++n02;
			if (e.form == ex03)
				++n03;
			if (e.form == ex09)
				++n09;
			if (e.form == ex04)
				has04 = true;
			if (e.form == ex06)
				has06 = true;
			if (e.form == ex07)
				has07 = true;
			if (e.form == ex08)
				has08 = true;
		}
		Check(n01 == 2 && n02 == 2 && n03 == 1 && n09 == 1, "example: expected forms and counts");
		Check(!has04 && !has06 && !has07 && !has08, "example: removed/absent forms stay gone");
		Check(exChest->chanceNone == 50, "example: chanceNone set to 50");

		// bulk step: every non-empty leveled item got ex09 appended at level 1
		for (auto* item : { static_cast<RE::TESLevItem*>(llMain), exChest }) {
			bool found = false;
			for (const auto& e : item->entries) {
				if (e.form == ex09 && e.level == 1 && e.count == 1) {
					found = true;
				}
			}
			Check(found, "example: bulk add applied to every non-empty list");
		}
		Check(llMain->numEntries == 2, "example: restored main list also got the bulk add");
	}

	// --- run the shipped GearInjection.lua example (generic, config-driven) ---
	{
		std::string example;
		{
			FILE* f = std::fopen("examples/GearInjection/GearInjection.lua", "rb");
			Check(f != nullptr, "gearinjection example readable");
			char buf[4096];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				example.append(buf, n);
			}
			std::fclose(f);
		}
		DoString(lua, example.c_str());

		// Same candidate set as the former EquipmentInjection by default (plugin = ""):
		//   injected: helmet (light), chest (heavy), bow (ranged)
		//   excluded: sword (enchanted), sword2 (non-playable),
		//             vanillaChest (vanilla plugin), assignedArmor (already in a list)
		for (auto* list : lootLists) {
			bool hasHelmet = false, hasChest = false, hasBow = false;
			bool hasSword = false, hasSword2 = false, hasVanilla = false, hasAssigned = false;
			for (const auto& e : list->entries) {
				if (e.form == helmet)
					hasHelmet = true;
				if (e.form == chest)
					hasChest = true;
				if (e.form == bow)
					hasBow = true;
				if (e.form == sword)
					hasSword = true;
				if (e.form == sword2)
					hasSword2 = true;
				if (e.form == vanillaChest)
					hasVanilla = true;
				if (e.form == assignedArmor)
					hasAssigned = true;
			}
			Check(hasHelmet && hasChest && hasBow, "gearinjection: gear injected into each loot list");
			Check(!hasSword && !hasSword2 && !hasVanilla, "gearinjection: excluded gear stays out");
		}
		// assignedArmor must remain in exactly one list
		std::size_t assignedCount = 0;
		for (auto* list : lootLists) {
			for (const auto& e : list->entries) {
				if (e.form == assignedArmor) {
					++assignedCount;
				}
			}
		}
		Check(assignedCount == 1, "gearinjection: already-assigned gear untouched");
		// target lists are "LItem"-prefixed only: llMain is untouched
		Check(llMain->numEntries == 2, "gearinjection: non-LItem lists untouched");
	}

	// --- run the shipped KeywordFixer.lua example -------------------------
	{
		std::string example;
		{
			FILE* f = std::fopen("examples/KeywordFixer/KeywordFixer.lua", "rb");
			Check(f != nullptr, "keywordfixer example readable");
			char buf[4096];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				example.append(buf, n);
			}
			std::fclose(f);
		}
		DoString(lua, example.c_str());

		auto hasKeyword = [](RE::TESForm* a_form, RE::BGSKeyword* a_kw) {
			auto* kwForm = a_form->As<RE::BGSKeywordForm>();
			return kwForm && kwForm->HasKeyword(a_kw);
		};

		// material keywords derived from stats:
		//   sword dmg 25 -> Daedric, bow dmg 12 -> Dwarven
		//   chest rating 25 (heavy) -> Orcish, helmet rating 10 (light) -> Elven
		Check(hasKeyword(sword, kwWeapDaedric), "keywordfixer: sword got Daedric material keyword");
		Check(hasKeyword(bow, kwWeapDwarven), "keywordfixer: bow got Dwarven material keyword");
		Check(hasKeyword(chest, kwArmorOrcish), "keywordfixer: chest got Orcish material keyword");
		Check(hasKeyword(helmet, kwArmorElven), "keywordfixer: helmet got Elven material keyword");

		// vanilla / non-playable gear untouched, existing keywords preserved
		Check(!hasKeyword(sword2, kwWeapDaedric), "keywordfixer: non-playable gear untouched");
		Check(!hasKeyword(vanillaChest, kwArmorOrcish), "keywordfixer: vanilla gear untouched");
		Check(hasKeyword(chest, gearKw) && hasKeyword(chest, gearKw2), "keywordfixer: existing keywords preserved");
	}

	// --- run the shipped TemperingLists.lua example -----------------------
	// Runs after KeywordFixer: it needs the material keywords to exist.
	{
		std::string example;
		{
			FILE* f = std::fopen("examples/TemperingLists/TemperingLists.lua", "rb");
			Check(f != nullptr, "temperinglists example readable");
			char buf[4096];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				example.append(buf, n);
			}
			std::fclose(f);
		}
		DoString(lua, example.c_str());

		auto hasInList = [](RE::BGSListForm* a_list, RE::TESForm* a_form) { return a_list->HasForm(a_form); };

		// sword/bow/chest/helmet got their material keywords from KeywordFixer
		// and must now have joined the matching tempering sets
		Check(hasInList(tempDaedric, sword), "temperinglists: sword joined Daedric set");
		Check(hasInList(tempDwarven, bow), "temperinglists: bow joined Dwarven set");
		Check(hasInList(tempOrcish, chest), "temperinglists: chest joined Orcish set");
		Check(hasInList(tempElven, helmet), "temperinglists: helmet joined Elven set");

		// vanilla / non-playable / keyword-less gear untouched
		Check(!hasInList(tempOrcish, vanillaChest), "temperinglists: vanilla gear untouched");
		Check(!hasInList(tempDaedric, sword2), "temperinglists: non-playable gear untouched");
		Check(!hasInList(tempOrcish, assignedArmor), "temperinglists: keyword-less gear untouched");

		// idempotent: re-running the script must not duplicate entries
		DoString(lua, example.c_str());
		Check(tempDaedric->forms.size() == 1, "temperinglists: re-run is idempotent");
	}

	// --- script runner: priority ordering + config exclusion ---------------
	// Runs last: the chain mutates llMain, so nothing may depend on it after.
	{
		namespace fs = std::filesystem;
		const fs::path scriptsDir = "Data/SKSE/Plugins/LuaPatcher/Scripts";
		fs::remove_all("Data");
		fs::create_directories(scriptsDir);

		auto writeScript = [&](const char* a_name, const char* a_content) {
			std::ofstream out(scriptsDir / a_name, std::ios::binary);
			out << a_content;
		};

		// chain: each script removes its predecessor's marker and adds its
		// own; correct priority order leaves only the last marker behind.
		// File names are deliberately anti-alphabetical to prove the order
		// comes from priorities, not path sorting.
		writeScript("zz_30.lua",
			"-- priority: 30\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp|00000100\")\n"
			"ll:remove(\"MockFormC\")\n"
			"ll:add(\"MockFormD\")\n");
		writeScript("yy_20.lua",
			"-- priority: 20\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp|00000100\")\n"
			"ll:remove(\"MockFormB\")\n"
			"ll:add(\"MockFormC\")\n");
		writeScript("xx_10.lua",
			"-- priority: 10\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp|00000100\")\n"
			"ll:remove(\"MockFormA\")\n"
			"ll:add(\"MockFormB\")\n");
		// no priority declaration -> 0, runs first
		writeScript("aa_00.lua",
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp|00000100\")\n"
			"ll:clear()\n"
			"ll:add(\"MockFormA\")\n");
		// config files must never be executed as scripts
		writeScript("zz_30_config.lua", "lua_patcher.leveledList(\"MockPlugin.esp|00000100\"):add(\"MockFormE\")\n");

		LuaPatcher::RunScripts();

		Check(llMain->numEntries == 1, "script runner: chain left exactly one entry");
		Check(llMain->entries[0].form == formD, "script runner: last-priority marker survives");
		Check(llMain->entries[0].form != formE, "script runner: config file not executed");

		fs::remove_all("Data");
	}

	std::printf("All harness checks passed.\n");
	return 0;
}