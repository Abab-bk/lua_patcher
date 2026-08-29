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
#include "EncounterZone.h"
#include "Crafting.h"
#include "Protection.h"

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

	// --- Tier-1 randomizer world: alchemy, enchantment, containers, actors,
	//     shouts, lights, globals, game settings and leveled spells ---------
	auto* race = AddForm<RE::TESRace>(0x03006000, RE::FormType::Keyword, "GearRace", "Gear Race", gearMod);
	auto* npcClass = AddForm<RE::TESClass>(0x03006001, RE::FormType::Keyword, "GearClass", "Gear Class", gearMod);

	auto* ingredient =
		AddForm<RE::IngredientItem>(0x03007000, RE::FormType::Ingredient, "GearIngredient", "Gear Ingredient", gearMod);
	ingredient->data.costOverride = 15;
	ingredient->effects = {
		new RE::Effect(),
		new RE::Effect(),
	};
	ingredient->effects[0]->baseEffect = mgef;
	ingredient->effects[0]->effectItem.magnitude = 10.0F;
	ingredient->effects[0]->effectItem.duration = 30;
	ingredient->effects[0]->cost = 5.0F;
	ingredient->effects[1]->baseEffect = mgef;
	ingredient->effects[1]->effectItem.magnitude = 5.0F;

	auto* potion =
		AddForm<RE::AlchemyItem>(0x03007001, RE::FormType::AlchemyItem, "GearPotion", "Gear Potion", gearMod);
	potion->data.costOverride = 40;
	potion->data.flags = 1u << 17;  // kPoison
	potion->effects = { new RE::Effect() };
	potion->effects[0]->baseEffect = mgef;
	potion->effects[0]->effectItem.magnitude = 25.0F;

	auto* enchant2 =
		AddForm<RE::EnchantmentItem>(0x03007002, RE::FormType::Enchantment, "GearEnch2", "Gear Ench 2", gearMod);
	enchant2->data.costOverride = 500;
	enchant2->data.chargeOverride = 1200;
	enchant2->data.chargeTime = 3.0F;
	enchant2->data.castingType = RE::MagicSystem::CastingType::kConstantEffect;
	enchant2->effects = { new RE::Effect() };
	enchant2->effects[0]->baseEffect = mgef;
	enchant2->effects[0]->effectItem.magnitude = 40.0F;
	enchant2->effects[0]->cost = 100.0F;

	auto* container =
		AddForm<RE::TESObjectCONT>(0x03007003, RE::FormType::Container, "GearChestBox", "Gear Chest Box", gearMod);
	container->AddObjectToContainer(sword, 1, nullptr);
	container->AddObjectToContainer(chest, 2, nullptr);

	auto* npc = AddForm<RE::TESNPC>(0x03007004, RE::FormType::NPC, "GearBandit", "Gear Bandit", gearMod);
	npc->actorData.level = 10;
	npc->playerSkills.health = 100;
	npc->playerSkills.magicka = 50;
	npc->playerSkills.stamina = 75;
	npc->playerSkills.values[RE::TESNPC::Skills::kOneHanded] = 40;
	npc->playerSkills.values[RE::TESNPC::Skills::kDestruction] = 25;
	npc->race = race;
	npc->npcClass = npcClass;

	auto* levSpell =
		AddForm<RE::TESLevSpell>(0x03007005, RE::FormType::LeveledSpell, "GearLevSpell", "Gear Lev Spell", gearMod);
	levSpell->entries = { MakeEntry(spell, 1, 1) };
	levSpell->numEntries = 1;

	auto* word1 = AddForm<RE::TESWordOfPower>(0x03007006, RE::FormType::Keyword, "GearWord1", "Gear Word 1", gearMod);
	auto* word2 = AddForm<RE::TESWordOfPower>(0x03007007, RE::FormType::Keyword, "GearWord2", "Gear Word 2", gearMod);
	auto* shoutSpell =
		AddForm<RE::SpellItem>(0x03007008, RE::FormType::Spell, "GearShoutSpell", "Gear Shout Spell", gearMod);
	auto* shout = AddForm<RE::TESShout>(0x03007009, RE::FormType::Shout, "GearShout", "Gear Shout", gearMod);
	shout->variations[0] = { word1, shoutSpell, 30.0F };
	shout->variations[1] = { word2, nullptr, 40.0F };

	auto* light = AddForm<RE::TESObjectLIGH>(0x0300700A, RE::FormType::Light, "GearLight", "Gear Light", gearMod);
	light->data.radius = 256;
	light->data.color = { 255, 128, 0 };
	light->data.fov = 60.0F;
	light->data.fallofExponent = 1.5F;
	light->fade = 2.0F;
	light->data.flags = RE::TES_LIGHT_FLAGS::kCanCarry;

	global->value = 3.5F;
	global->type = RE::TESGlobal::Type::kLong;

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
	LuaPatcher::RegisterAlchemy(lua);
	LuaPatcher::RegisterEnchantment(lua);
	LuaPatcher::RegisterContainer(lua);
	LuaPatcher::RegisterActors(lua);
	LuaPatcher::RegisterWorld(lua);
	LuaPatcher::RegisterShout(lua);
	LuaPatcher::RegisterLight(lua);
	LuaPatcher::RegisterEncounterZone(lua);
	LuaPatcher::RegisterCrafting(lua);
	LuaPatcher::BuildQuestProtection();
	LuaPatcher::RegisterProtection(lua);

	// --- core API tests -------------------------------------------------
	DoString(lua, R"LUA(
assert(lua_patcher.version == "0.1.0")
local f = lua_patcher.getForm("MockPlugin.esp", "00000100")
assert(f ~= nil, "getForm by mod|id")
assert(f.formId == 0x01000100)
assert(f.type == "LeveledItem")
assert(f.editorId == "MockLeveledList")
assert(f.name == "Mock Chest")
assert(f.identifier == "MockPlugin.esp|000100")
assert(lua_patcher.getForm("MockFormA").formId == 0x01000A00, "getForm by editorId")
assert(lua_patcher.getForm("MockPlugin.esp", "00000A00").editorId == "MockFormA", "getForm id masking")
assert(lua_patcher.getForm("MockLight.esl", "000001") ~= nil, "light plugin id")
assert(lua_patcher.getForm("NoSuchPlugin.esp", "000001") == nil, "unknown plugin")
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
assert(fl:has("MockPlugin.esp", "00000A00") == true, "has by formId identifier")

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
assert(lua_patcher.formList("MockPlugin.esp", "00000600").identifier == "MockPlugin.esp|000600")
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
local ll = lua_patcher.leveledList("MockPlugin.esp", "00000100")
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

-- replace MockFormC -> MockFormA (pair syntax: two strings are a (plugin, formId) pair)
ll:replace("MockPlugin.esp", "00000C00", "MockPlugin.esp", "00000A00")
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
ok, err = pcall(function() lua_patcher.getForm("MockPlugin.esp", "ZZZZ") end)
assert(ok, "invalid hex id -> nil, not an error")
assert(lua_patcher.getForm("MockPlugin.esp", "ZZZZ") == nil, "invalid hex id returns nil")

ok, err = pcall(function() lua_patcher.leveledList("MockKeyword") end)
assert(not ok, "non-leveled form must fail")

local ll = lua_patcher.leveledList("MockPlugin.esp", "00000100")
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
local ll0 = lua_patcher.leveledList("MockPlugin.esp", "00000100")
assert(tostring(ll0) == "LeveledList[01000100]", "leveled list tostring")
assert(tostring(lua_patcher.getForm("MockPlugin.esp", "00000100")) == "LeveledList[01000100]", "typed getForm tostring")
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
local sword = lua_patcher.getForm("GearMod.esp", "00001000")
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

assert(#lua_patcher.allSpells() == 2)  -- GearSpell + GearShoutSpell
	assert(#lua_patcher.allMagicEffects() == 1)
)LUA");

	// --- alchemy / enchantment / containers / actors / shouts / lights / worlds ----
	DoString(lua, R"LUA(
-- ingredient: effect slots read/write/append/clear
local ing = lua_patcher.getForm("GearIngredient")
assert(ing ~= nil and ing.type == "Ingredient")
assert(ing.costOverride == 15)
assert(tostring(ing) == "Ingredient[03007000]", "ingredient tostring")
local effs = ing:effects()
assert(#effs == 2)
assert(effs[1].baseEffect.editorId == "GearMgef")
assert(effs[1].magnitude == 10 and effs[1].duration == 30 and effs[1].cost == 5)
assert(effs[2].magnitude == 5 and effs[2].area == 0)
ing:addEffect("GearMgef", { magnitude = 99, duration = 7 })
assert(#ing:effects() == 3)
assert(ing:effects()[3].magnitude == 99 and ing:effects()[3].duration == 7)
ing:clearEffects()
assert(#ing:effects() == 0)
ing:setEffects({
  { baseEffect = "GearMgef", magnitude = 1, area = 2, duration = 3, cost = 4 },
  { baseEffect = "GearMgef", magnitude = 8 },
})
local restored = ing:effects()
assert(#restored == 2 and restored[1].area == 2 and restored[1].duration == 3 and restored[1].cost == 4)
assert(restored[2].magnitude == 8)
ing.costOverride = 30
assert(ing.costOverride == 30)
local ok, err = pcall(function() ing:setEffects({ { baseEffect = "GearSword" } }) end)
assert(not ok, "non-magic-effect baseEffect must fail")
assert(#ing:effects() == 2, "failed setEffects leaves effects untouched")
assert(#lua_patcher.allIngredients() == 1)

-- potion: poison/food flags + effects
local potion = lua_patcher.getForm("GearPotion")
assert(potion ~= nil and potion.type == "Potion")
assert(potion.isPoison == true and potion.isFood == false)
assert(tostring(potion) == "Potion[03007001]", "potion tostring")
assert(#potion:effects() == 1 and potion:effects()[1].magnitude == 25)
potion:setEffects({ { baseEffect = "GearMgef", duration = 60 } })
assert(potion:effects()[1].duration == 60)
assert(#lua_patcher.allPotions() == 1)

-- enchantment: charge + effects + casting type
local ench = lua_patcher.getForm("GearEnch2")
assert(ench ~= nil and ench.type == "Enchantment")
assert(ench.costOverride == 500 and ench.chargeOverride == 1200)
assert(math.abs(ench.chargeTime - 3) < 0.001)
assert(ench.castingType == "ConstantEffect")
assert(tostring(ench) == "Enchantment[03007002]", "enchantment tostring")
assert(#ench:effects() == 1 and ench:effects()[1].magnitude == 40 and ench:effects()[1].cost == 100)
ench.castingType = "FireAndForget"
assert(ench.castingType == "FireAndForget")
ench.delivery = "Aimed"
assert(ench.delivery == "Aimed")
local ok, err = pcall(function() ench.castingType = "Nope" end)
assert(not ok, "invalid enchantment castingType must fail")
assert(#lua_patcher.allEnchantments() == 2)

-- container: contents read/write/add/remove
local cont = lua_patcher.getForm("GearChestBox")
assert(cont ~= nil and cont.type == "Container")
assert(cont.numObjects == 2)
assert(tostring(cont) == "Container[03007003]", "container tostring")
local contents = cont:contents()
assert(contents[1].form.editorId == "GearSword" and contents[1].count == 1)
assert(contents[2].form.editorId == "GearChest" and contents[2].count == 2)
assert(cont:has("GearSword") == true and cont:has("GearBow") == false)
cont:addItem("GearBow", 3)
assert(cont.numObjects == 3 and cont:has("GearBow") == true)
assert(cont:removeItem("GearBow") == true and cont:has("GearBow") == false)
assert(cont:removeItem("GearBow") == false)
cont:setContents({ { form = "GearHelmet", count = 4 } })
assert(cont.numObjects == 1)
local only = cont:contents()
assert(only[1].form.editorId == "GearHelmet" and only[1].count == 4)
cont:addItem("GearSword")
assert(cont:contents()[2].form.editorId == "GearSword" and cont:contents()[2].count == 1)
cont:clearContents()
assert(cont.numObjects == 0)
local ok, err = pcall(function() cont:addItem("MockKeyword") end)
assert(not ok, "non-bound object in container must fail")
assert(#lua_patcher.allContainers() == 1)

-- actor: level, attributes, skills, race/class
local actor = lua_patcher.getForm("GearBandit")
assert(actor ~= nil and actor.type == "Actor")
assert(actor.level == 10)
assert(actor.health == 100 and actor.magicka == 50 and actor.stamina == 75)
assert(tostring(actor) == "Actor[03007004]", "actor tostring")
assert(actor.race.editorId == "GearRace")
assert(actor.npcClass.editorId == "GearClass")
local skills = actor:skills()
assert(#skills == 18)
assert(skills[1].name == "OneHanded" and skills[1].value == 40)
assert(skills[15].name == "Destruction" and skills[15].value == 25)
actor:setSkill("Destruction", 60)
assert(actor:skills()[15].value == 60)
actor:setSkill(1, 55)
assert(actor:skills()[1].value == 55)
actor.level = 25
actor.health = 200
assert(actor.level == 25 and actor.health == 200)
local ok, err = pcall(function() actor:setSkill("Nope", 1) end)
assert(not ok, "unknown skill name must fail")
assert(#lua_patcher.allActors() == 1)

-- leveled spells: same API as leveled items
local lvspell = lua_patcher.leveledList("GearLevSpell")
assert(lvspell.numEntries == 1)
assert(lvspell:entries()[1].form.editorId == "GearSpell" and lvspell:entries()[1].level == 1)
lvspell:add("GearShoutSpell", 5, 1)
assert(lvspell.numEntries == 2)
lvspell:clear()
assert(lvspell.numEntries == 0)
assert(#lua_patcher.allLeveledSpells() == 1)

-- shout: variations read/partial-write/full-write
local shout = lua_patcher.getForm("GearShout")
assert(shout ~= nil and shout.type == "Shout")
assert(tostring(shout) == "Shout[03007009]", "shout tostring")
local variations = shout:variations()
assert(#variations == 3)
assert(variations[1].word.editorId == "GearWord1" and variations[1].spell.editorId == "GearShoutSpell")
assert(math.abs(variations[1].recoveryTime - 30) < 0.001)
assert(variations[2].word.editorId == "GearWord2" and variations[2].spell == nil)
assert(variations[3].spell == nil)
assert(shout:word(1).editorId == "GearWord1")
assert(shout:spell(1).editorId == "GearShoutSpell")
assert(shout:spell(2) == nil)
shout:setVariation(2, { spell = "GearShoutSpell", recoveryTime = 45 })
assert(shout:spell(2).editorId == "GearShoutSpell")
assert(shout:variations()[2].recoveryTime == 45)
shout:setVariations({
  { spell = "GearShoutSpell" },
  { spell = "GearShoutSpell", recoveryTime = 50 },
  { spell = "GearShoutSpell" },
})
-- form-object refs (like EverythingRandomizer's shuffleShoutSpells)
shout:setVariations({ { word = shout:word(1), spell = shout:spell(1) } })
local all = shout:variations()
assert(all[1].spell.editorId == "GearShoutSpell" and all[2].recoveryTime == 50)
assert(all[3].spell.editorId == "GearShoutSpell")
local ok, err = pcall(function() shout:setVariation(4, {}) end)
assert(not ok, "variation index out of range must fail")
local ok, err = pcall(function() shout:setVariation(1, { spell = "GearSword" }) end)
assert(not ok, "non-spell variation spell must fail")
assert(#lua_patcher.allShouts() == 1)

-- light: radius/color/fov/falloff/fade/flags
local light = lua_patcher.getForm("GearLight")
assert(light ~= nil and light.type == "Light")
assert(light.radius == 256)
assert(light.color.r == 255 and light.color.g == 128 and light.color.b == 0)
assert(math.abs(light.fov - 60) < 0.001)
assert(math.abs(light.falloff - 1.5) < 0.001)
assert(math.abs(light.fade - 2) < 0.001)
assert(light.canCarry == true and light.dynamic == false)
light.radius = 512
light.color = { r = 0, g = 255, b = 255 }
light.fov = 90
assert(light.radius == 512)
assert(light.color.g == 255 and light.color.b == 255)
local ok, err = pcall(function() light.color = { r = 999 } end)
assert(not ok, "color channel out of range must fail")
assert(#lua_patcher.allLights() == 1)

-- globals
local g = lua_patcher.getForm("MockGlobal")
assert(g ~= nil and g.type == "Global")
assert(math.abs(g.value - 3.5) < 0.001)
assert(g.globalType == "Long")
g.value = 7.25
assert(math.abs(g.value - 7.25) < 0.001)
assert(#lua_patcher.allGlobals() == 2)  -- MockGlobal + MockLightGlobal
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
local ll = lua_patcher.leveledList("MockPlugin.esp", "00000100")
local llChar = lua_patcher.leveledList("MockPlugin.esp", "00000400")
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
		//   injected: helmet (light), chest (heavy), bow (ranged),
		//             GearIngredient, GearPotion (alchemy has no playable flag)
		//   excluded: sword (enchanted), sword2 (non-playable),
		//             vanillaChest (vanilla plugin), assignedArmor (already in a list)
		for (auto* list : lootLists) {
			bool hasHelmet = false, hasChest = false, hasBow = false;
			bool hasIngredient = false, hasPotion = false;
			bool hasSword = false, hasSword2 = false, hasVanilla = false, hasAssigned = false;
			for (const auto& e : list->entries) {
				if (e.form == helmet)
					hasHelmet = true;
				if (e.form == chest)
					hasChest = true;
				if (e.form == bow)
					hasBow = true;
				if (e.form == ingredient)
					hasIngredient = true;
				if (e.form == potion)
					hasPotion = true;
				if (e.form == sword)
					hasSword = true;
				if (e.form == sword2)
					hasSword2 = true;
				if (e.form == vanillaChest)
					hasVanilla = true;
				if (e.form == assignedArmor)
					hasAssigned = true;
			}
			Check(hasHelmet && hasChest && hasBow && hasIngredient && hasPotion,
				"gearinjection: gear and alchemy injected into each loot list");
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
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\")\n"
			"ll:remove(\"MockFormC\")\n"
			"ll:add(\"MockFormD\")\n");
		writeScript("yy_20.lua",
			"-- priority: 20\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\")\n"
			"ll:remove(\"MockFormB\")\n"
			"ll:add(\"MockFormC\")\n");
		writeScript("xx_10.lua",
			"-- priority: 10\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\")\n"
			"ll:remove(\"MockFormA\")\n"
			"ll:add(\"MockFormB\")\n");
		// no priority declaration -> 0, runs first
		writeScript("aa_00.lua",
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\")\n"
			"ll:clear()\n"
			"ll:add(\"MockFormA\")\n");
		// config files must never be executed as scripts
		writeScript("zz_30_config.lua", "lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\"):add(\"MockFormE\")\n");
		// exact-file-name load: loadLua("Foo.lua") loads Foo.lua as-is
		// (generated datasets like EverythingRandomizer_protection.lua)
		writeScript("ExactConfig.lua", "return { protected = { [123] = true } }\n");
		// module files ("_" prefix) are never picked up as top-level scripts,
		// but ARE loadable via require() from sibling scripts (package.path
		// points at the scripts folder). _lib's top-level line adds a form to
		// llChar; with a correct loader it runs exactly once (via the require
		// in zz_40_module) — if the loader also executed it standalone, llChar
		// would end up with the form twice.
		llChar->entries.clear();
		llChar->numEntries = 0;
		writeScript("_lib.lua",
			"lua_patcher.leveledList(\"MockPlugin.esp\", \"00000400\"):add(\"MockFormE\")\n"
			"return { formName = \"MockFormA\" }\n");
		writeScript("zz_40_module.lua",
			"-- priority: 40\n"
			"local lib = require(\"_lib\")\n"
			"assert(lib.formName == \"MockFormA\", \"require returned the module table\")\n"
			"local ll = lua_patcher.leveledList(\"MockPlugin.esp\", \"00000100\")\n"
			"ll:remove(\"MockFormD\")\n"
			"ll:add(lib.formName)\n");

		LuaPatcher::RunScripts();

		Check(llMain->numEntries == 1, "script runner: chain left exactly one entry");
		Check(llMain->entries[0].form == formA, "script runner: module-required form applied");
		Check(llMain->entries[0].form != formE, "script runner: config file not executed");
		Check(llChar->numEntries == 1, "script runner: module executed once, via require only");
		{
			sol::protected_function_result res = lua.safe_script(
				"local t = lua_patcher.loadLua(\"ExactConfig.lua\")\n"
				"assert(t and t.protected and t.protected[123] == true, \"exact-name load\")\n"
				"assert(lua_patcher.loadLua(\"ExactConfig\") == nil, \"missing file -> nil\")\n"
				"assert(lua_patcher.loadLua(\"ExactConfig_config.lua\") == nil, \"wrong name -> nil\")\n",
				sol::script_pass_on_error);
			if (!res.valid()) {
				sol::error err = res;
				std::fprintf(stderr, "FAIL: %s\n", err.what());
				std::exit(1);
			}
			std::printf("ok: loadLua exact-name\n");
		}

		fs::remove_all("Data");
	}

	// --- EverythingRandomizer example -------------------------------------
	// Deterministic layout: same seed -> same world. Runs through DoString so
	// config files in the temporary Data/ dir drive the different scenarios.
	{
		namespace fs = std::filesystem;
		const fs::path scriptsDir = "Data/SKSE/Plugins/LuaPatcher/Scripts";
		fs::remove_all("Data");
		fs::create_directories(scriptsDir);

		// Ench* loot variant used by the enchantedLootRatio scenario (kept out
		// of the pools entirely at ratio 0). Added late so no earlier section
		// sees it.
		auto* enchSword = AddForm<RE::TESObjectWEAP>(
			0x03001003, RE::FormType::Weapon, "EnchGearSword", "Ench Gear Sword", gearMod);
		enchSword->attackDamage = 25;
		enchSword->formEnchanting = enchant;

		// empty leftover lists from earlier examples so the pools are exact
		for (auto* list : RE::TESDataHandler::MockForms<RE::TESLevItem>()) {
			if (list->editorId == "ExampleChest") {
				list->entries.clear();
				list->numEntries = 0;
			}
		}
		llChar->entries.clear();
		llChar->numEntries = 0;
		for (auto* list : lootLists) {
			list->entries.clear();
			list->numEntries = 0;
		}

		// controlled world: Keyword x4 in llMain, Armor x3 in list 0, Weapon x3 in list 1
		auto resetWorld = [&]() {
			llMain->entries = {
				MakeEntry(formA, 1, 1),
				MakeEntry(formB, 1, 2),
				MakeEntry(formC, 1, 3),
				MakeEntry(formD, 1, 4),
			};
			llMain->numEntries = 4;
			lootLists[0]->entries = { MakeEntry(helmet, 2, 1), MakeEntry(chest, 1, 2), MakeEntry(assignedArmor, 1, 3) };
			lootLists[0]->numEntries = 3;
			lootLists[1]->entries = { MakeEntry(sword, 1, 1), MakeEntry(bow, 1, 2), MakeEntry(sword2, 1, 3) };
			lootLists[1]->numEntries = 3;
		};

		std::string example;
		{
			FILE* f = std::fopen("examples/EverythingRandomizer/EverythingRandomizer.lua", "rb");
			Check(f != nullptr, "everythingrandomizer example readable");
			char buf[4096];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				example.append(buf, n);
			}
			std::fclose(f);
		}

		auto writeConfig = [&](const char* a_content) {
			std::ofstream out(scriptsDir / "EverythingRandomizer_config.lua", std::ios::binary);
			out << a_content;
		};

		// the real shipped dataset must load through the exact-name path
		{
			FILE* f = std::fopen("examples/EverythingRandomizer/EverythingRandomizer_protection.lua", "rb");
			Check(f != nullptr, "protection dataset readable");
			std::ofstream out(scriptsDir / "EverythingRandomizer_protection.lua", std::ios::binary);
			char buf[8192];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				out.write(buf, static_cast<std::streamsize>(n));
			}
			std::fclose(f);
		}

		using Slot = std::tuple<RE::TESForm*, std::uint16_t, std::uint16_t>;
		auto snapshot = [&](std::vector<Slot>& a_out) {
			a_out.clear();
			for (auto* list : { llMain, lootLists[0], lootLists[1] }) {
				for (const auto& e : list->entries) {
					a_out.emplace_back(e.form, e.count, e.level);
				}
			}
		};
		auto makeSlots = [](std::initializer_list<Slot> a_slots) { return std::vector<Slot>(a_slots); };


		// Expected layouts (deterministic given the mock world + seed). Slots keep
		// their own count/level; only the form moves. Defaults are banded
		// (tierBands 4, tierDrift 1) with formListExcludeSuffixes = {} — the
		// shipped example defaults, not the historical 0.3/{ "Set" } values.
		const auto kSeed1337 = makeSlots({
			{ formD, 1, 1 },
			{ formA, 1, 2 },
			{ formC, 1, 3 },
			{ formB, 1, 4 },
			{ assignedArmor, 2, 1 },
			{ helmet, 1, 2 },
			{ chest, 1, 3 },
			{ sword, 1, 1 },
			{ bow, 1, 2 },
			{ sword2, 1, 3 },
		});
		//   seed 4242: all three pools shuffle (slots keep their count/level)
		const auto kSeed4242 = makeSlots({
			{ formA, 1, 1 },
			{ formB, 1, 2 },
			{ formC, 1, 3 },
			{ formD, 1, 4 },
			{ assignedArmor, 2, 1 },
			{ chest, 1, 2 },
			{ helmet, 1, 3 },
			{ sword, 1, 1 },
			{ bow, 1, 2 },
			{ sword2, 1, 3 },
		});
		//   seed 1337 + excludePrefixes LItem: only llMain shuffles
		const auto kSeed1337Excluded = makeSlots({
			{ formD, 1, 1 },
			{ formA, 1, 2 },
			{ formB, 1, 3 },
			{ formC, 1, 4 },
			{ helmet, 2, 1 },
			{ chest, 1, 2 },
			{ assignedArmor, 1, 3 },
			{ sword, 1, 1 },
			{ bow, 1, 2 },
			{ sword2, 1, 3 },
		});
		//   seed 1337 + tierDrift 0 (strict bands; mock pool is small so the
		//   layout equals the default's — the invariant is the tier alignment)
		const auto kSeed1337Strict = makeSlots({
			{ formA, 1, 1 },
			{ formD, 1, 2 },
			{ formB, 1, 3 },
			{ formC, 1, 4 },
			{ assignedArmor, 2, 1 },
			{ chest, 1, 2 },
			{ helmet, 1, 3 },
			{ sword, 1, 1 },
			{ sword2, 1, 2 },
			{ bow, 1, 3 },
		});
		//   seed 1337 + enchantedLootRatio 0: the Ench* variant stays in its
		//   own list (level 4), the base weapons shuffle among the rest
		const auto kSeed1337NoEnch = makeSlots({
			{ formD, 1, 1 },
			{ formA, 1, 2 },
			{ formC, 1, 3 },
			{ formB, 1, 4 },
			{ assignedArmor, 2, 1 },
			{ helmet, 1, 2 },
			{ chest, 1, 3 },
			{ enchSword, 1, 1 },
			{ sword2, 1, 2 },
			{ sword, 1, 3 },
			{ bow, 1, 4 },
		});

		// The example now pulls its helpers in via require(); mirror the
		// loader's package.path behavior (script folder first) so the modules
		// resolve from the examples/ tree the harness actually runs.
		lua["package"]["path"] =
			"examples/EverythingRandomizer/?.lua;" + lua["package"]["path"].get<sol::optional<std::string>>().value_or("");

		// run 1: defaults (no config -> seed 1337)
		resetWorld();
		DoString(lua, example.c_str());
		std::vector<Slot> run1;
		snapshot(run1);
		Check(run1 == kSeed1337, "randomizer: seed 1337 layout");

		// run 2: same defaults again -> identical layout (determinism)
		resetWorld();
		DoString(lua, example.c_str());
		std::vector<Slot> run2;
		snapshot(run2);
		Check(run2 == run1, "randomizer: same seed -> identical layout");

		// run 3: different seed -> different layout
		writeConfig("return { seed = 4242 }\n");
		resetWorld();
		DoString(lua, example.c_str());
		std::vector<Slot> run3;
		snapshot(run3);
		Check(run3 == kSeed4242, "randomizer: seed 4242 layout");
		Check(run3 != run1, "randomizer: different seed -> different layout");

		// run 4: excludePrefixes protects LItem lists
		writeConfig("return { seed = 1337, excludePrefixes = { \"LItem\" } }\n");
		resetWorld();
		DoString(lua, example.c_str());
		std::vector<Slot> run4;
		snapshot(run4);
		Check(run4 == kSeed1337Excluded, "randomizer: excludePrefixes protect LItem lists");

		// run 5: strict bands (tierDrift 0) keep the difficulty curve: the
		// weakest weapon slot (level 3, sword2) can only receive the two weakest
		// weapons and the strongest slot (level 1, sword) the two strongest.
		writeConfig("return { seed = 1337, tierDrift = 0 }\n");
		resetWorld();
		DoString(lua, example.c_str());
		std::vector<Slot> run5;
		snapshot(run5);
		Check(run5 == kSeed1337Strict, "randomizer: strict bands layout");
		{
			RE::TESForm* lvl1 = nullptr;  // sword slot: never the weakest form
			RE::TESForm* lvl3 = nullptr;  // sword2 slot: never the strongest form
			for (const auto& e : lootLists[1]->entries) {
				if (e.level == 1)
					lvl1 = e.form;
				if (e.level == 3)
					lvl3 = e.form;
			}
			Check(lvl1 != sword2 && lvl3 != sword, "randomizer: strict bands keep tier alignment");
		}

		// run 6: enchantedLootRatio 0. NOTE: the mock's Ench* form is added
		// after the editorID cache was built, so isEnchantedVariant cannot
		// recognize it and it shuffles like any weapon (in the real game the
		// cache is complete at script time and the variant stays in place).
		writeConfig("return { seed = 1337, enchantedLootRatio = 0 }\n");
		resetWorld();
		lootLists[1]->entries.push_back(MakeEntry(enchSword, 1, 4));
		lootLists[1]->numEntries = 4;
		DoString(lua, example.c_str());
		std::vector<Slot> run6;
		snapshot(run6);
		Check(run6 == kSeed1337NoEnch, "randomizer: enchantedLootRatio 0 layout");

		fs::remove_all("Data");
	}

	// --- encounter zones: typed dispatch + difficulty swap ------------------
	// Regression: allEncounterZones() must push EncounterZone userdata (not
	// plain Form), or z.hasLevels raises "unknown property" at runtime.
	{
		auto* zoneWeak =
			AddForm<RE::BGSEncounterZone>(0x0300A000, RE::FormType::EncounterZone, "ZoneWeak", "Zone Weak", gearMod);
		zoneWeak->data.minLevel = 1;
		zoneWeak->data.maxLevel = 10;

		auto* zoneStrong =
			AddForm<RE::BGSEncounterZone>(0x0300A001, RE::FormType::EncounterZone, "ZoneStrong", "Zone Strong", gearMod);
		zoneStrong->data.minLevel = 40;
		zoneStrong->data.maxLevel = 60;

		auto* zoneUnset =
			AddForm<RE::BGSEncounterZone>(0x0300A002, RE::FormType::EncounterZone, "ZoneUnset", "Zone Unset", gearMod);
		// minLevel/maxLevel stay -1 (unset, falls back to location defaults)

		DoString(lua,
			"local zones = lua_patcher.allEncounterZones()\n"
			"assert(#zones == 3)\n"
			"local weak = lua_patcher.getForm(\"ZoneWeak\")\n"
			"assert(weak.type == \"EncounterZone\", \"typed dispatch\")\n"
			"assert(weak.hasLevels == true and weak.minLevel == 1 and weak.maxLevel == 10)\n"
			"local unset = lua_patcher.getForm(\"ZoneUnset\")\n"
			"assert(unset.hasLevels == false and unset.minLevel == -1 and unset.maxLevel == -1)\n");

		DoString(lua,
			"local shuffles = require(\"_randomizer_shuffles\")\n"
			"local ctx = { config = { zoneSwapWindow = 3 }, isExcluded = function() return false end, skipped = 0 }\n"
			"math.randomseed(1337)\n"
			"local changed = shuffles.encounterZones(ctx)\n"
			"local weak = lua_patcher.getForm(\"ZoneWeak\")\n"
			"local strong = lua_patcher.getForm(\"ZoneStrong\")\n"
			"local unset = lua_patcher.getForm(\"ZoneUnset\")\n"
			"assert(changed == 2 or changed == 4, \"each swapping pair counts two level changes\")\n"
			"assert(unset.minLevel == -1 and unset.maxLevel == -1, \"unset zone untouched\")\n"
			"assert(weak.minLevel <= weak.maxLevel and strong.minLevel <= strong.maxLevel, \"pairs stay ordered\")\n"
			"assert((weak.minLevel == 1 and strong.minLevel == 40) or (weak.minLevel == 40 and strong.minLevel == 1), "
			"\"level pair swapped whole\")\n"
			"local firstWeak, firstStrong = weak.minLevel, strong.minLevel\n"
			"-- reset and rerun: identical seed reproduces the layout\n"
			"weak.minLevel, weak.maxLevel = 1, 10\n"
			"strong.minLevel, strong.maxLevel = 40, 60\n"
			"math.randomseed(1337)\n"
			"local changed2 = shuffles.encounterZones(ctx)\n"
			"assert(changed2 == changed and weak.minLevel == firstWeak and strong.minLevel == firstStrong, "
			"\"same seed -> identical layout\")\n");
	}

	// --- constructible objects: typed dispatch + recipe shuffles ---------
	// The COBJ mocks are created after the randomizer determinism runs above:
	// the recipe shuffles are on by default, and empty pools consume no RNG,
	// so adding them here cannot disturb the seeded layout assertions.
	{
		auto* forge = AddForm<RE::BGSKeyword>(
			0x0300B000, RE::FormType::Keyword, "CraftingSmithingForge", "Crafting Smithing Forge", gearMod);
		auto* ingot = AddForm<RE::TESBoundObject>(0x0300B001, RE::FormType::MiscItem, "IronIngot", "Iron Ingot", gearMod);
		auto* dwarvenIngot =
			AddForm<RE::TESBoundObject>(0x0300B002, RE::FormType::MiscItem, "DwarvenIngot", "Dwarven Ingot", gearMod);
		auto* temperSet = AddForm<RE::BGSListForm>(
			0x0300B003, RE::FormType::FormList, "WeapMaterialTestSet", "WeapMaterialTestSet", gearMod);

		auto* recipeSword = AddForm<RE::BGSConstructibleObject>(
			0x0300B100, RE::FormType::ConstructibleObject, "CraftSword", "Craft Sword", gearMod);
		recipeSword->createdItem = sword;
		recipeSword->benchKeyword = forge;
		recipeSword->data.numConstructed = 1;
		recipeSword->requiredItems.AddObjectToContainer(ingot, 2, nullptr);

		auto* recipeSword2 = AddForm<RE::BGSConstructibleObject>(
			0x0300B101, RE::FormType::ConstructibleObject, "CraftSword2", "Craft Sword 2", gearMod);
		recipeSword2->createdItem = sword2;
		recipeSword2->benchKeyword = forge;
		recipeSword2->data.numConstructed = 1;
		recipeSword2->requiredItems.AddObjectToContainer(ingot, 2, nullptr);

		auto* recipeChest = AddForm<RE::BGSConstructibleObject>(
			0x0300B102, RE::FormType::ConstructibleObject, "CraftChest", "Craft Chest", gearMod);
		recipeChest->createdItem = chest;
		recipeChest->benchKeyword = forge;
		recipeChest->data.numConstructed = 1;
		recipeChest->requiredItems.AddObjectToContainer(ingot, 3, nullptr);
		recipeChest->requiredItems.AddObjectToContainer(dwarvenIngot, 1, nullptr);

		// tempering recipe: the required material set is a FormList (the
		// smithing material-keyword system); such recipes must never have
		// their ingredients rewritten (setRequiredItems accepts bound objects
		// only, mirroring the Container API)
		auto* recipeTemper = AddForm<RE::BGSConstructibleObject>(
			0x0300B103, RE::FormType::ConstructibleObject, "VendorTemperingTest", "Vendor Tempering Test", gearMod);
		recipeTemper->createdItem = helmet;
		recipeTemper->benchKeyword = forge;
		recipeTemper->data.numConstructed = 1;
		recipeTemper->requiredItems.containerObjects.push_back(new RE::ContainerObject{ temperSet, 1 });
		recipeTemper->requiredItems.numContainerObjects = 1;

		DoString(lua, R"LUA(
local recipes = lua_patcher.allConstructibleObjects()
assert(#recipes == 4, "allConstructibleObjects count")
local a = lua_patcher.getForm("CraftSword")
assert(a ~= nil and a.type == "ConstructibleObject", "typed dispatch")
assert(tostring(a) == "ConstructibleObject[0300B100]", "cobj tostring")
-- editorId is served from a one-time cache that predates the forms added by
-- this late harness section; assert on formId (resolved via the editorID
-- lookup table, which is always live) instead
local sword = lua_patcher.getForm("GearSword")
local forge = lua_patcher.getForm("CraftingSmithingForge")
assert(a.createdItem.formId == sword.formId, "createdItem getter")
assert(a.benchKeyword.formId == forge.formId, "benchKeyword getter")
assert(a.numConstructed == 1)
assert(a.numRequiredItems == 1)
local items = a:requiredItems()
assert(#items == 1 and items[1].form.formId == lua_patcher.getForm("IronIngot").formId and items[1].count == 2)
a.numConstructed = 3
assert(a.numConstructed == 3, "numConstructed write")
a.numConstructed = 0
assert(a.numConstructed == 1, "numConstructed clamps to >= 1")
a.createdItem = "GearBow"
assert(a.createdItem.formId == lua_patcher.getForm("GearBow").formId, "createdItem write")
a.createdItem = lua_patcher.getForm("GearSword")
assert(a.createdItem.formId == sword.formId, "createdItem write via form")
assert(a:hasRequiredItem("IronIngot") == true and a:hasRequiredItem("GearBow") == false)
a:addRequiredItem("DwarvenIngot", 1)
assert(a.numRequiredItems == 2 and a:hasRequiredItem("DwarvenIngot"))
assert(a:removeRequiredItem("DwarvenIngot") == true and a:removeRequiredItem("DwarvenIngot") == false)
a:setRequiredItems({ { form = "DwarvenIngot", count = 5 } })
assert(a.numRequiredItems == 1)
local only = a:requiredItems()
assert(only[1].form.formId == lua_patcher.getForm("DwarvenIngot").formId and only[1].count == 5, "setRequiredItems")
a:clearRequiredItems()
assert(a.numRequiredItems == 0, "clearRequiredItems")
local ok, err = pcall(function() a:setRequiredItems({ { form = "CraftSword" } }) end)
assert(not ok, "non-bound required item must fail")
ok, err = pcall(function() a.benchKeyword = "GearSword" end)
assert(not ok, "non-keyword benchKeyword must fail")
ok, err = pcall(function() return a.bogus end)
assert(not ok, "unknown cobj property must fail")
)LUA");

		// restore the vanilla layout for the shuffle tests (the API tests
		// above cleared CraftSword's required items)
		recipeSword->requiredItems.AddObjectToContainer(ingot, 2, nullptr);

		DoString(lua, R"LUA(
-- recipe shuffles: outputs swap within same-type pools (banded by power),
-- ingredient slots keep their counts and FormList material sets never leave
-- their recipe. All identity checks below use formId (the editorId cache
-- predates the forms added by this late harness section).
local shuffles = require("_randomizer_shuffles")
local ctx = { config = { shuffleRecipeOutputs = true, shuffleRecipeIngredients = true, tierBands = 4, tierDrift = 0 },
    isExcluded = function() return false end, skipped = 0 }

local function outputsSig()
    local s = {}
    for _, r in ipairs(lua_patcher.allConstructibleObjects()) do
        table.insert(s, r.createdItem.formId)
    end
    table.sort(s)
    return table.concat(s, ",")
end
local function ingredientsSig()
    local s = {}
    for _, r in ipairs(lua_patcher.allConstructibleObjects()) do
        for _, e in ipairs(r:requiredItems()) do
            table.insert(s, e.form.formId .. "x" .. e.count)
        end
    end
    table.sort(s)
    return table.concat(s, ",")
end
local function resetRecipes()
    local a = lua_patcher.getForm("CraftSword")
    local b = lua_patcher.getForm("CraftSword2")
    local c = lua_patcher.getForm("CraftChest")
    local t = lua_patcher.getForm("VendorTemperingTest")
    a.createdItem = "GearSword"
    b.createdItem = "GearSword2"
    c.createdItem = "GearChest"
    t.createdItem = "GearHelmet"
    a:setRequiredItems({ { form = "IronIngot", count = 2 } })
    b:setRequiredItems({ { form = "IronIngot", count = 2 } })
    c:setRequiredItems({ { form = "IronIngot", count = 3 }, { form = "DwarvenIngot", count = 1 } })
end

local beforeOut = outputsSig()
local beforeIng = ingredientsSig()
math.randomseed(1337)
local changed = shuffles.recipes(ctx)
assert(changed >= 0)
assert(outputsSig() == beforeOut, "output multiset conserved")
assert(ingredientsSig() == beforeIng, "ingredient multiset conserved")
local temper = lua_patcher.getForm("VendorTemperingTest")
assert(temper:requiredItems()[1].form.formId == lua_patcher.getForm("WeapMaterialTestSet").formId,
    "material set stays in place")

-- determinism: identical seed reproduces the identical layout
local afterOut, afterIng = outputsSig(), ingredientsSig()
resetRecipes()
math.randomseed(1337)
shuffles.recipes(ctx)
assert(outputsSig() == afterOut, "same seed -> same outputs")
assert(ingredientsSig() == afterIng, "same seed -> same ingredients")

-- exclusion: a protected output stays in place and never enters the pools
local excl = { config = { shuffleRecipeOutputs = true, shuffleRecipeIngredients = true, tierBands = 4, tierDrift = 0 },
    isExcluded = function(f) return f.formId == 0x03001001 end, skipped = 0 }
resetRecipes()
math.randomseed(1337)
shuffles.recipes(excl)
assert(lua_patcher.getForm("CraftSword2").createdItem.formId == 0x03001001, "protected output stays")
assert(lua_patcher.getForm("CraftSword").createdItem.formId == 0x03001000, "remaining pool form keeps its slot")
assert(excl.skipped > 0, "protected slots counted as skipped")
)LUA");

		Check(recipeTemper->requiredItems.containerObjects[0]->obj == temperSet,
			"recipe: tempering material set untouched");
		Check(recipeTemper->requiredItems.numContainerObjects == 1, "recipe: tempering entry count intact");
		Check(recipeSword->createdItem == sword && recipeSword2->createdItem == sword2,
			"recipe: protected outputs stay");
	}

	// --- shout spell slots: position preservation + protection ----------
	// Regression for shuffleShoutSpells: writing back only the filled
	// variation slots shifts words/spells on shouts with empty slots, and
	// consuming the pool for protected spells exhausts it (crash). Words
	// must never leave their slot; empty slots must stay empty.
	{
		auto* word3 =
			AddForm<RE::TESWordOfPower>(0x0300C000, RE::FormType::Keyword, "GearWord3", "Gear Word 3", gearMod);
		auto* shoutSpell2 =
			AddForm<RE::SpellItem>(0x0300C001, RE::FormType::Spell, "GearShoutSpell2", "Gear Shout Spell 2", gearMod);
		auto* shoutGap =
			AddForm<RE::TESShout>(0x0300C002, RE::FormType::Shout, "GearShoutGap", "Gear Shout Gap", gearMod);
		shoutGap->variations[0] = { word1, shoutSpell, 30.0F };
		shoutGap->variations[1] = { word2, nullptr, 40.0F };  // empty middle slot
		shoutGap->variations[2] = { word3, shoutSpell2, 50.0F };

		auto* shoutFull =
			AddForm<RE::TESShout>(0x0300C003, RE::FormType::Shout, "GearShoutFull", "Gear Shout Full", gearMod);
		shoutFull->variations[0] = { word1, shoutSpell, 30.0F };
		shoutFull->variations[1] = { word2, shoutSpell2, 40.0F };
		shoutFull->variations[2] = { word3, shoutSpell, 50.0F };

		DoString(lua, R"LUA(
local shuffles = require("_randomizer_shuffles")
local ctx = { config = {}, isExcluded = function() return false end, skipped = 0 }
local word1 = lua_patcher.getForm("GearWord1").formId
local word2 = lua_patcher.getForm("GearWord2").formId
local word3 = lua_patcher.getForm("GearWord3").formId
local spell1 = lua_patcher.getForm("GearShoutSpell").formId
local spell2 = lua_patcher.getForm("GearShoutSpell2").formId

local function layoutSig()
    local t = {}
    for _, s in ipairs(lua_patcher.allShouts()) do
        for _, v in ipairs(s:variations()) do
            table.insert(t, (v.word and v.word.formId or 0) .. ":" .. (v.spell and v.spell.formId or 0))
        end
    end
    return table.concat(t, ",")
end

math.randomseed(1337)
local changed = shuffles.shoutSpells(ctx, lua_patcher.allShouts())
assert(changed >= 0)
local sig1 = layoutSig()

-- words never leave their slots; the empty slot stays empty
local gap = lua_patcher.getForm("GearShoutGap")
assert(gap:variations()[1].word.formId == word1, "gap first word stays")
assert(gap:variations()[2].word.formId == word2, "gap middle word stays")
assert(gap:variations()[2].spell == nil, "empty variation slot stays empty")
assert(gap:variations()[3].word.formId == word3, "gap third word stays")
assert(gap:variations()[3].spell ~= nil, "gap third slot keeps a spell")
local full = lua_patcher.getForm("GearShoutFull")
assert(full:variations()[2].word.formId == word2 and full:variations()[2].spell ~= nil, "full middle slot intact")
assert(full:variations()[3].word.formId == word3 and full:variations()[3].spell ~= nil, "full third slot intact")

-- determinism: same seed reproduces the identical layout
local function resetShouts()
    local g = lua_patcher.getForm("GearShoutGap")
    local f = lua_patcher.getForm("GearShoutFull")
    local o = lua_patcher.getForm("GearShout")
    g:setVariations({ { word = "GearWord1", spell = "GearShoutSpell", recoveryTime = 30 },
        { word = "GearWord2", recoveryTime = 40 },
        { word = "GearWord3", spell = "GearShoutSpell2", recoveryTime = 50 } })
    f:setVariations({ { word = "GearWord1", spell = "GearShoutSpell", recoveryTime = 30 },
        { word = "GearWord2", spell = "GearShoutSpell2", recoveryTime = 40 },
        { word = "GearWord3", spell = "GearShoutSpell", recoveryTime = 50 } })
    o:setVariations({ { word = "GearWord1", spell = "GearShoutSpell", recoveryTime = 30 },
        { word = "GearWord2", spell = "GearShoutSpell", recoveryTime = 50 },
        { spell = "GearShoutSpell" } })
end
resetShouts()
math.randomseed(1337)
shuffles.shoutSpells(ctx, lua_patcher.allShouts())
assert(layoutSig() == sig1, "same seed -> same shout layout")

-- protected spells keep their slots and never consume the pool
local excl = { config = {}, isExcluded = function(f) return f.formId == spell1 end, skipped = 0 }
resetShouts()
math.randomseed(1337)
local ok, err = pcall(function() shuffles.shoutSpells(excl, lua_patcher.allShouts()) end)
assert(ok, "protected spells must not exhaust the pool: " .. tostring(err))
assert(excl.skipped > 0, "protected slots counted as skipped")
assert(lua_patcher.getForm("GearShout"):spell(1).formId == spell1, "protected spell stays in place")
assert(lua_patcher.getForm("GearShoutGap"):variations()[2].spell == nil, "empty slot untouched by protected run")
)LUA");
	}

	std::printf("All harness checks passed.\n");
	return 0;
}