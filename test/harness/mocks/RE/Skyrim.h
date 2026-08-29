#pragma once
// Minimal stand-ins for the CommonLibSSE-NG RE types used by the LuaPatcher
// binding code, so the real src/*.cpp files can be compiled and smoke-tested
// on Linux (where Skyrim's engine types do not exist).

#include <cstdint>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RE
{
	using FormID = std::uint32_t;

	enum class FormType : std::uint8_t
	{
		GameSetting = 3,
		MagicEffect = 18,
		Spell = 22,
		Enchantment = 25,
		Armor = 26,
		Container = 28,
		Ingredient = 30,
		Light = 31,
		FormList = 40,
		Weapon = 41,
		NPC = 43,
		Global = 44,
		AlchemyItem = 46,
		Keyword = 47,
		LeveledItem = 49,
		LeveledSpell = 51,
		LeveledNPC = 52,
		EncounterZone = 67,
		Shout = 119,
	};

	inline std::string_view FormTypeToString(FormType a_formType) noexcept
	{
		switch (a_formType) {
		case FormType::Keyword:
			return "Keyword";
		case FormType::Global:
			return "Global";
		case FormType::GameSetting:
			return "GameSetting";
		case FormType::LeveledItem:
			return "LeveledItem";
		case FormType::LeveledSpell:
			return "LeveledSpell";
		case FormType::LeveledNPC:
			return "LeveledCharacter";
		case FormType::MagicEffect:
			return "MagicEffect";
		case FormType::Spell:
			return "Spell";
		case FormType::Armor:
			return "Armor";
		case FormType::Enchantment:
			return "Enchantment";
		case FormType::Weapon:
			return "Weapon";
		case FormType::FormList:
			return "FormList";
		case FormType::Ingredient:
			return "Ingredient";
		case FormType::AlchemyItem:
			return "Potion";
		case FormType::Container:
			return "Container";
		case FormType::NPC:
			return "Actor";
		case FormType::Light:
			return "Light";
		case FormType::Shout:
			return "Shout";
		case FormType::EncounterZone:
			return "EncounterZone";
		default:
			return "Unknown";
		}
	}

	class TESFile
	{
	public:
		std::string fileName;
		bool light = false;
		std::uint8_t compileIndex = 0;
		std::uint8_t smallFileCompileIndex = 0;

		[[nodiscard]] std::string_view GetFilename() const noexcept { return { fileName }; }
		[[nodiscard]] constexpr bool IsLight() const noexcept { return light; }
		[[nodiscard]] std::uint8_t GetCompileIndex() const noexcept { return compileIndex; }
		[[nodiscard]] std::uint8_t GetSmallFileCompileIndex() const noexcept { return smallFileCompileIndex; }
	};

	class BaseFormComponent
	{
	public:
		virtual ~BaseFormComponent() = default;
	};

	// Minimal stand-in for REX::EnumSet supporting what the binding uses:
	// any(E), underlying() and comparison against a single enum value.
	template <class E, class U>
	class EnumSet
	{
	public:
		constexpr EnumSet() = default;
		constexpr EnumSet(E a_value) : _impl(static_cast<U>(a_value)) {}

		constexpr EnumSet& operator=(E a_value)
		{
			_impl = static_cast<U>(a_value);
			return *this;
		}

		[[nodiscard]] constexpr U underlying() const noexcept { return _impl; }

		template <class E2>
		[[nodiscard]] constexpr bool any(E2 a_value) const noexcept
		{
			return (_impl & static_cast<U>(a_value)) != 0;
		}

		template <class E2>
		[[nodiscard]] constexpr bool all(E2 a_value) const noexcept
		{
			return (_impl & static_cast<U>(a_value)) == static_cast<U>(a_value);
		}

		template <class E2>
		[[nodiscard]] constexpr bool none(E2 a_value) const noexcept
		{
			return (_impl & static_cast<U>(a_value)) == 0;
		}

		[[nodiscard]] constexpr E get() const noexcept { return static_cast<E>(_impl); }

		friend constexpr bool operator==(EnumSet a_lhs, E a_rhs) noexcept
		{
			return a_lhs._impl == static_cast<U>(a_rhs);
		}
		friend constexpr bool operator==(E a_lhs, EnumSet a_rhs) noexcept { return a_rhs == a_lhs; }

	private:
		U _impl = 0;
	};

	enum class ActorValue
	{
		kNone = -1,
		kOneHanded = 6,
		kTwoHanded = 7,
		kArchery = 8,
		kAlteration = 18,
		kConjuration = 19,
		kDestruction = 20,
		kIllusion = 21,
		kRestoration = 22,
		kEnchanting = 23,
	};

	enum WEAPON_TYPE : std::uint32_t
	{
		kHandToHandMelee = 0,
		kOneHandSword = 1,
		kOneHandDagger = 2,
		kOneHandAxe = 3,
		kOneHandMace = 4,
		kTwoHandSword = 5,
		kTwoHandAxe = 6,
		kBow = 7,
		kStaff = 8,
		kCrossbow = 9,
	};

	class TESForm;
	inline std::unordered_map<std::string, TESForm*>& MockFormsByEditorID()
	{
		static std::unordered_map<std::string, TESForm*> map;
		return map;
	}
	inline std::unordered_map<FormID, TESForm*>& MockFormsByID()
	{
		static std::unordered_map<FormID, TESForm*> map;
		return map;
	}

	// The game keeps editorIDs in a global table (TESForm::GetAllFormsByEditorID),
	// NOT as per-form members: the virtual GetFormEditorID() returns "" for most
	// classes. The mock mirrors that: tests must go through the table.
	using BSFixedString = std::string;

	class BSTHashMap
	{
	public:
		using Map = std::unordered_map<BSFixedString, TESForm*>;

		Map& GetMap() { return map; }
		auto begin() noexcept { return map.begin(); }
		auto end() noexcept { return map.end(); }
		const auto begin() const noexcept { return map.begin(); }
		const auto end() const noexcept { return map.end(); }

	private:
		Map map;
	};

	struct BSReadWriteLock
	{};
	struct BSReadLockGuard
	{
		explicit BSReadLockGuard(const BSReadWriteLock&) {}
	};

	inline BSTHashMap& MockEditorIdMap()
	{
		static BSTHashMap map;
		return map;
	}
	inline BSReadWriteLock& MockEditorIdLock()
	{
		static BSReadWriteLock lock;
		return lock;
	}

	class TESForm
	{
	public:
		virtual ~TESForm() = default;

		struct RecordFlags
		{
			enum RecordFlag : std::uint32_t
			{
				kNonPlayable = 1 << 2,
			};
		};

		FormID formID = 0;
		FormType formType = FormType::Keyword;
		std::string editorId;
		std::string name;
		std::vector<TESFile*> sourceFiles;
		std::uint32_t formFlags = 0;

		[[nodiscard]] static TESForm* LookupByEditorID(const std::string_view& a_editorID)
		{
			const auto it = MockFormsByEditorID().find(std::string(a_editorID));
			return it != MockFormsByEditorID().end() ? it->second : nullptr;
		}

		// Mirrors the game: most form classes do not override this and it
		// returns "" -- editorIDs live in the GetAllFormsByEditorID table.
		[[nodiscard]] static auto GetAllFormsByEditorID()
		{
			return std::pair<BSTHashMap*, BSReadWriteLock&>{ &MockEditorIdMap(), std::ref(MockEditorIdLock()) };
		}

		[[nodiscard]] FormID GetFormID() const noexcept { return formID; }
		[[nodiscard]] FormType GetFormType() const noexcept { return formType; }
		[[nodiscard]] std::uint32_t GetFormFlags() const noexcept { return formFlags; }
		[[nodiscard]] const char* GetName() const { return name.c_str(); }
		[[nodiscard]] virtual const char* GetFormEditorID() const { return ""; }

		[[nodiscard]] TESFile* GetFile(std::int32_t a_idx = -1) const
		{
			if (sourceFiles.empty()) {
				return nullptr;
			}
			if (a_idx >= 0 && static_cast<std::size_t>(a_idx) < sourceFiles.size()) {
				return sourceFiles[a_idx];
			}
			return sourceFiles.back();
		}

		[[nodiscard]] FormID GetLocalFormID() const
		{
			auto* file = GetFile(0);
			if (!file) {
				return formID;
			}
			return file->IsLight() ? formID & 0xFFF : formID & 0xFFFFFF;
		}

		template <class T, class = std::enable_if_t<std::is_class_v<T>>>
		[[nodiscard]] T* As() noexcept
		{
			return dynamic_cast<T*>(this);
		}
	};

	// Mirrors RE::TESBoundObject (base of all placeable items; leveled lists
	// and equipment derive from it, and container entries must be bound objects).
	class TESBoundObject : public TESForm
	{};

	class BGSKeyword;
	class BGSKeywordForm
	{
	public:
		virtual ~BGSKeywordForm() = default;

		BGSKeyword** keywords = nullptr;
		std::uint32_t numKeywords = 0;

		[[nodiscard]] std::span<BGSKeyword*> GetKeywords() { return { keywords, numKeywords }; }

		[[nodiscard]] std::span<BGSKeyword* const> GetKeywords() const { return { keywords, numKeywords }; }

		[[nodiscard]] virtual bool HasKeyword(const BGSKeyword* a_keyword) const
		{
			for (std::uint32_t i = 0; i < numKeywords; ++i) {
				if (keywords[i] == a_keyword) {
					return true;
				}
			}
			return false;
		}

		// Mirrors BGSKeywordForm::AddKeyword/RemoveKeyword (real CLib semantics:
		// append if absent / shift down if present).
		virtual bool AddKeyword(BGSKeyword* a_keyword)
		{
			if (HasKeyword(a_keyword)) {
				return false;
			}
			auto* grown = new BGSKeyword*[numKeywords + 1];
			for (std::uint32_t i = 0; i < numKeywords; ++i) {
				grown[i] = keywords[i];
			}
			grown[numKeywords] = a_keyword;
			delete[] keywords;
			keywords = grown;
			++numKeywords;
			return true;
		}

		virtual bool RemoveKeyword(BGSKeyword* a_keyword)
		{
			for (std::uint32_t i = 0; i < numKeywords; ++i) {
				if (keywords[i] == a_keyword) {
					for (std::uint32_t j = i; j + 1 < numKeywords; ++j) {
						keywords[j] = keywords[j + 1];
					}
					--numKeywords;
					return true;
				}
			}
			return false;
		}
	};

	class BGSKeyword : public TESForm, public BGSKeywordForm
	{};

	// Mirrors BGSListForm (real CLib semantics): AddForm appends, HasForm is a
	// set membership test. There is no RemoveForm in the real API either —
	// removal goes through the snapshot-write-back cycle like leveled lists.
	class BGSListForm : public TESForm
	{
	public:
		std::vector<TESForm*> forms;

		void AddForm(TESForm* a_form) { forms.push_back(a_form); }

		[[nodiscard]] bool HasForm(const TESForm* a_form) const
		{
			return std::ranges::find(forms, a_form) != forms.end();
		}

		[[nodiscard]] bool HasForm(FormID a_formID) const
		{
			return std::ranges::find_if(forms, [a_formID](const TESForm* f) { return f->formID == a_formID; }) !=
			       forms.end();
		}
	};

	class TESGlobal : public TESForm
	{
	public:
		enum class Type
		{
			kFloat = 'f',
			kLong = 'l',
			kShort = 's'
		};

		EnumSet<Type, std::uint8_t> type = Type::kFloat;
		float value = 0.0F;
	};

	// Mirrors RE::TESWordOfPower (referenced by TESShout variations).
	class TESWordOfPower : public TESForm
	{};

	class TESRace : public TESForm
	{};

	class TESClass : public TESForm
	{};

	namespace MagicSystem
	{
		enum class SpellType : std::uint32_t
		{
			kSpell = 0,
			kDisease = 1,
			kPower = 2,
			kLesserPower = 3,
			kAbility = 4,
			kPoison = 5,
			kEnchantment = 6,
			kPotion = 7,
			kIngredient = 8,
			kLeveledSpell = 9,
			kAddiction = 10,
			kVoicePower = 11,
		};

		enum class CastingType : std::uint32_t
		{
			kConstantEffect = 0,
			kFireAndForget = 1,
			kConcentration = 2,
			kScroll = 3,
		};

		enum class Delivery : std::uint32_t
		{
			kSelf = 0,
			kTouch = 1,
			kAimed = 2,
			kTargetActor = 3,
			kTargetLocation = 4,
		};
	}

	namespace EffectArchetypes
	{
		enum class ArchetypeID : std::uint32_t
		{
			kValueModifier = 0,
			kScript = 1,
			kDispel = 2,
			kCureDisease = 3,
			kAbsorb = 4,
			kDualValueModifier = 5,
			kCalm = 6,
			kDemoralize = 7,
			kFrenzy = 8,
			kDisarm = 9,
			kCommandSummoned = 10,
			kInvisibility = 11,
			kLight = 12,
		};
	}

	class EffectSetting;

	class TESValueForm : public BaseFormComponent
	{
	public:
		std::int32_t value = 0;
	};

	class TESWeightForm : public BaseFormComponent
	{
	public:
		float weight = 0.0f;
	};

	// Mirrors RE::Color (used by TESObjectLIGH::OBJ_LIGH).
	struct Color
	{
		std::uint8_t red = 0;
		std::uint8_t green = 0;
		std::uint8_t blue = 0;
		std::uint8_t alpha = 0xFF;
	};

	// Mirrors RE::Effect (a MagicItem effect slot; real CLib keeps these as
	// heap Effect objects in BSTArray<Effect*>).
	struct Effect
	{
		struct EffectItem
		{
			float magnitude = 0.0F;
			std::uint32_t area = 0;
			std::uint32_t duration = 0;
		};
		EffectItem effectItem;
		EffectSetting* baseEffect = nullptr;
		float cost = 0.0F;
	};

	// Mirrors RE::MagicItem: the shared base of IngredientItem, AlchemyItem,
	// EnchantmentItem (and SpellItem/ScrollItem in the real engine).
	class MagicItem : public TESForm, public BGSKeywordForm
	{
	public:
		std::vector<Effect*> effects;
	};

	class IngredientItem : public MagicItem, public TESValueForm, public TESWeightForm
	{
	public:
		struct Data
		{
			std::int32_t costOverride = 0;
		};
		Data data;
	};

	class AlchemyItem : public MagicItem, public TESWeightForm
	{
	public:
		struct Data
		{
			std::int32_t costOverride = 0;
			std::uint32_t flags = 0;  // bit 1 = kFoodItem, bit 17 = kPoison
		};
		Data data;

		[[nodiscard]] bool IsFood() const { return ((data.flags >> 1) & 1) != 0 && !IsPoison(); }
		[[nodiscard]] bool IsPoison() const { return (data.flags & (1u << 17)) != 0; }
	};

	class EnchantmentItem : public MagicItem
	{
	public:
		struct Data
		{
			std::int32_t costOverride = 0;
			MagicSystem::CastingType castingType = MagicSystem::CastingType::kConstantEffect;
			MagicSystem::Delivery delivery = MagicSystem::Delivery::kSelf;
			std::int32_t chargeOverride = 0;
			float chargeTime = 0.0F;
			EnchantmentItem* baseEnchantment = nullptr;
		};
		Data data;
	};

	// Minimal stand-ins for the magic types (field names mirror Magic.cpp usage).
	class SpellItem : public TESForm, public BGSKeywordForm
	{
	public:
		struct Data
		{
			std::int32_t costOverride = 0;
			MagicSystem::SpellType spellType = MagicSystem::SpellType::kSpell;
			MagicSystem::CastingType castingType = MagicSystem::CastingType::kFireAndForget;
			MagicSystem::Delivery delivery = MagicSystem::Delivery::kSelf;
			float chargeTime = 0.0F;
			float castDuration = 0.0F;
			float range = 0.0F;
		};
		Data data;
	};

	class EffectSetting : public TESForm, public BGSKeywordForm
	{
	public:
		struct Data
		{
			float baseCost = 0.0F;
			std::int32_t minimumSkill = 0;
			std::int32_t spellmakingArea = 0;
			float spellmakingChargeTime = 0.0F;
			float taperWeight = 0.0F;
			float taperCurve = 0.0F;
			float skillUsageMult = 0.0F;
			ActorValue associatedSkill = ActorValue::kNone;
			ActorValue resistVariable = ActorValue::kNone;
			MagicSystem::CastingType castingType = MagicSystem::CastingType::kFireAndForget;
			MagicSystem::Delivery delivery = MagicSystem::Delivery::kSelf;
			EffectArchetypes::ArchetypeID archetype = EffectArchetypes::ArchetypeID::kValueModifier;
		};
		Data data;

		[[nodiscard]] bool IsHostile() const { return false; }
		[[nodiscard]] bool IsDetrimental() const { return false; }
	};

	class EnchantmentItem;
	class TESEnchantableForm : public BaseFormComponent
	{
	public:
		EnchantmentItem* formEnchanting = nullptr;
	};

	class TESAttackDamageForm : public BaseFormComponent
	{
	public:
		std::uint16_t attackDamage = 0;

		[[nodiscard]] virtual std::uint16_t GetAttackDamage() const { return attackDamage; }
	};

	struct BIPED_MODEL
	{
		enum class BipedObjectSlot : std::uint32_t
		{
			kNone = 0,
			kHead = 1 << 0,
			kHair = 1 << 1,
			kBody = 1 << 2,
			kHands = 1 << 3,
			kForearms = 1 << 4,
			kAmulet = 1 << 5,
			kRing = 1 << 6,
			kFeet = 1 << 7,
			kCalves = 1 << 8,
			kShield = 1 << 9,
			kTail = 1 << 10,
			kLongHair = 1 << 11,
			kCirclet = 1 << 12,
			kEars = 1 << 13,
			kFX01 = 1u << 31
		};

		enum class ArmorType
		{
			kLightArmor,
			kHeavyArmor,
			kClothing
		};
	};

	class BGSBipedObjectForm : public BaseFormComponent
	{
	public:
		EnumSet<BIPED_MODEL::BipedObjectSlot, std::uint32_t> bipedObjectSlots;
		EnumSet<BIPED_MODEL::ArmorType, std::uint32_t> armorType;

		[[nodiscard]] BIPED_MODEL::ArmorType GetArmorType() const
		{
			return static_cast<BIPED_MODEL::ArmorType>(armorType.underlying());
		}

		[[nodiscard]] EnumSet<BIPED_MODEL::BipedObjectSlot, std::uint32_t> GetSlotMask() const
		{
			return bipedObjectSlots;
		}
	};

	class TESObjectWEAP :
		public TESBoundObject,
		public TESValueForm,
		public TESWeightForm,
		public TESEnchantableForm,
		public TESAttackDamageForm,
		public BGSKeywordForm
	{
	public:
		struct Data
		{
			float speed = 0.0f;
			float reach = 0.0f;
			float staggerValue = 0.0f;
			EnumSet<ActorValue, std::uint32_t> skill;
			WEAPON_TYPE animationType = kOneHandSword;
			std::uint8_t flags = 0;  // bit 7 = kNonPlayable
		};

		struct CriticalData
		{
			std::uint16_t damage = 0;
		};

		Data weaponData;
		CriticalData criticalData;

		[[nodiscard]] float GetSpeed() const { return weaponData.speed; }
		[[nodiscard]] float GetReach() const { return weaponData.reach; }
		[[nodiscard]] float GetStagger() const { return weaponData.staggerValue; }
		[[nodiscard]] std::uint16_t GetCritDamage() const { return criticalData.damage; }
		[[nodiscard]] WEAPON_TYPE GetWeaponType() const { return weaponData.animationType; }
		[[nodiscard]] bool GetPlayable() const { return ((weaponData.flags >> 7) & 1) == 0; }
		[[nodiscard]] bool IsMelee() const { return GetWeaponType() <= kTwoHandAxe; }
		[[nodiscard]] bool IsRanged() const { return !IsMelee(); }
		[[nodiscard]] bool IsBow() const { return GetWeaponType() == kBow; }
		[[nodiscard]] bool IsStaff() const { return GetWeaponType() == kStaff; }
		[[nodiscard]] bool IsCrossbow() const { return GetWeaponType() == kCrossbow; }
	};

	class TESObjectARMO :
		public TESBoundObject,
		public TESValueForm,
		public TESWeightForm,
		public TESEnchantableForm,
		public BGSBipedObjectForm,
		public BGSKeywordForm
	{
	public:
		std::uint32_t armorRating = 0;  // CK value * 100

		[[nodiscard]] float GetArmorRating() { return static_cast<float>(armorRating) / 100.0f; }
	};

	struct LEVELED_OBJECT
	{
		TESForm* form = nullptr;
		std::uint16_t count = 0;
		std::uint16_t level = 0;
		std::uint32_t pad0C = 0;
		void* itemExtra = nullptr;
	};

	class TESLeveledList : public BaseFormComponent
	{
	public:
		enum Flag : std::uint8_t
		{
			kCalculateFromAllLevelsLTOrEqPCLevel = 1 << 0,
			kCalculateForEachItemInCount = 1 << 1,
			kUseAll = 1 << 2,
			kSpecialLoot = 1 << 3
		};

		std::vector<LEVELED_OBJECT> entries;
		std::int8_t chanceNone = 0;
		Flag llFlags = static_cast<Flag>(0);
		std::uint8_t numEntries = 0;
		TESGlobal* chanceGlobal = nullptr;
	};

	class TESLevItem : public TESBoundObject, public TESLeveledList
	{};

	class TESLevCharacter : public TESBoundObject, public TESLeveledList
	{};

	// Leveled spell lists: same entry layout as TESLevItem/Character (the real
	// engine stores them in the shared TESLeveledList::entries member).
	class TESLevSpell : public TESBoundObject, public TESLeveledList
	{};

	// Mirrors RE::TESContainer (component of TESObjectCONT): heap-allocated
	// ContainerObject entries with no set-size API, so mutations go through the
	// engine's Add/RemoveObjectTo/FromContainer methods.
	struct ContainerObject
	{
		TESBoundObject* obj = nullptr;
		std::int32_t count = 0;
	};

	class TESContainer : public BaseFormComponent
	{
	public:
		// Mirrors the real engine: a heap array of ContainerObject pointers.
		std::vector<ContainerObject*> containerObjects;
		std::uint32_t numContainerObjects = 0;  // mirrors the engine member; keep in sync with the vector
		bool allowStolenItems = false;

		// Mirrors the real engine methods used by src/Container.cpp
		// (AddObjectToContainer appends/accumulates, RemoveObjectFromContainer
		// deletes the first entry whose count matches exactly).
		void AddObjectToContainer(TESBoundObject* a_object, std::int32_t a_count, TESForm* a_owner)
		{
			(void)a_owner;
			for (auto* entry : containerObjects) {
				if (entry && entry->obj == a_object) {
					entry->count += a_count;
					return;
				}
			}
			containerObjects.push_back(new ContainerObject{ a_object, a_count });
			numContainerObjects = static_cast<std::uint32_t>(containerObjects.size());
		}

		bool RemoveObjectFromContainer(TESBoundObject* a_object, std::int32_t a_count)
		{
			for (auto it = containerObjects.begin(); it != containerObjects.end(); ++it) {
				if (*it && (*it)->obj == a_object && (*it)->count == a_count) {
					containerObjects.erase(it);
					numContainerObjects = static_cast<std::uint32_t>(containerObjects.size());
					return true;
				}
			}
			return false;
		}

		std::int32_t GetObjectCount(const TESBoundObject* a_object) const
		{
			std::int32_t count = 0;
			for (const auto* entry : containerObjects) {
				if (entry && entry->obj == a_object) {
					count += entry->count;
				}
			}
			return count;
		}
	};

	class TESObjectCONT : public TESForm, public TESContainer
	{};

	// Mirrors RE::TESRaceForm (component of TESNPC).
	class TESRaceForm : public BaseFormComponent
	{
	public:
		TESForm* race = nullptr;
	};

	// NPC_ records cover both NPCs and creatures (creatures are NPC_ records in
	// Skyrim; there is no separate TESCreature form type).
	class TESNPC : public TESForm, public TESRaceForm
	{
	public:
		struct Skills
		{
			enum
			{
				kOneHanded = 0,
				kTwoHanded = 1,
				kMarksman = 2,
				kBlock = 3,
				kSmithing = 4,
				kHeavyArmor = 5,
				kLightArmor = 6,
				kPickpocket = 7,
				kLockpicking = 8,
				kSneak = 9,
				kAlchemy = 10,
				kSpeechcraft = 11,
				kAlteration = 12,
				kConjuration = 13,
				kDestruction = 14,
				kIllusion = 15,
				kRestoration = 16,
				kEnchanting = 17,
				kTotal
			};

			std::uint8_t values[kTotal]{};
			std::uint8_t offsets[kTotal]{};
			std::uint16_t health = 0;
			std::uint16_t magicka = 0;
			std::uint16_t stamina = 0;
		};

		// Mirrors TESActorBaseData::actorData (level lives here in the real
		// engine; health/magicka/stamina are in playerSkills).
		struct ActorBaseData
		{
			std::uint16_t level = 0;  // 0 = scales with player level
		};

		Skills playerSkills;
		ActorBaseData actorData;
		TESForm* npcClass = nullptr;
	};

	// Mirrors RE::TESShout: three word-variations, each a word + spell pair.
	class TESShout : public TESForm
	{
	public:
		struct VariationIDs
		{
			enum VariationID : std::uint32_t
			{
				kOne = 0,
				kTwo,
				kThree,
				kTotal
			};
		};
		using VariationID = VariationIDs::VariationID;

		struct Variation
		{
			TESForm* word = nullptr;
			TESForm* spell = nullptr;
			float recoveryTime = 0.0F;
		};
		Variation variations[VariationIDs::kTotal]{};
	};

	// Mirrors RE::TESObjectLIGH (OBJ_LIGH DATA block + FNAM fade).
	enum class TES_LIGHT_FLAGS : std::uint32_t
	{
		kNone = 0,
		kDynamic = 1 << 0,
		kCanCarry = 1 << 1,
		kNegative = 1 << 2,
		kFlicker = 1 << 3,
		kOffByDefault = 1 << 5,
	};

	class TESObjectLIGH : public TESForm
	{
	public:
		struct OBJ_LIGH
		{
			std::uint32_t radius = 0;
			Color color;
			EnumSet<TES_LIGHT_FLAGS, std::uint32_t> flags;
			float fallofExponent = 1.0F;
			float fov = 0.0F;
		};
		OBJ_LIGH data;
		float fade = 1.0F;

		[[nodiscard]] bool CanBeCarried() const { return data.flags.all(TES_LIGHT_FLAGS::kCanCarry); }
	};

	class TESDataHandler
	{
	public:
		static TESDataHandler* GetSingleton()
		{
			static TESDataHandler handler;
			return &handler;
		}

		inline static std::unordered_map<std::string, TESFile*> mockMods;

		template <class T>
		static std::vector<T*>& MockForms()
		{
			static std::vector<T*> forms;
			return forms;
		}

		[[nodiscard]] const TESFile* LookupModByName(std::string_view a_modName) const
		{
			const auto it = mockMods.find(std::string(a_modName));
			return it != mockMods.end() ? it->second : nullptr;
		}

		[[nodiscard]] const TESFile* LookupLoadedModByName(std::string_view a_modName) const
		{
			return LookupModByName(a_modName);
		}

		[[nodiscard]] const TESFile* LookupLoadedLightModByName(std::string_view a_modName) const
		{
			return LookupModByName(a_modName);
		}

		[[nodiscard]] TESForm* LookupForm(FormID a_localFormID, std::string_view a_modName) const
		{
			const auto* file = LookupModByName(a_modName);
			if (!file) {
				return nullptr;
			}
			FormID full = 0;
			if (file->IsLight()) {
				full = 0xFE000000u | (static_cast<FormID>(file->smallFileCompileIndex) << 12) | (a_localFormID & 0xFFF);
			} else {
				full = (static_cast<FormID>(file->compileIndex) << 24) | (a_localFormID & 0xFFFFFF);
			}
			const auto it = MockFormsByID().find(full);
			return it != MockFormsByID().end() ? it->second : nullptr;
		}

		template <class T>
		const std::vector<T*>& GetFormArray() const
		{
			return MockForms<T>();
		}
	};

	// Mirrors RE::BGSEncounterZone (ENCOUNTER_ZONE records; -1 levels mean
	// "unset" and fall back to the location defaults).
	class BGSEncounterZone : public TESForm
	{
	public:
		struct Data
		{
			std::int8_t minLevel = -1;
			std::int8_t maxLevel = -1;
		};
		Data data;
	};

	// Mirrors RE::ObjectRefHandle (a nullable object reference handle).
	class ObjectRefHandle
	{
	public:
		std::uint64_t handle = 0;

		[[nodiscard]] std::uint64_t native_handle() const noexcept { return handle; }
	};

	// Mirrors RE::BGSBaseAlias (quest alias base; the concrete class is
	// identified through the virtual GetTypeString()).
	class BGSBaseAlias
	{
	public:
		enum class FILL_TYPE : std::uint8_t
		{
			kNull = 0,
			kLocationRefType = 1,
			kCreated = 2,
			kExternal = 3,
			kUniqueActor = 4,
			kNearAlias = 5,
			kForced = 6,
		};

		struct FillData
		{
			struct ForcedData
			{
				ObjectRefHandle forcedRef;
			};
			struct CreatedData
			{
				TESForm* object = nullptr;
			};
			struct UniqueActorData
			{
				TESForm* uniqueActor = nullptr;
			};
			ForcedData forced;
			CreatedData created;
			UniqueActorData uniqueActor;
		};

		EnumSet<FILL_TYPE, std::uint8_t> fillType = FILL_TYPE::kNull;
		FillData fillData;

		[[nodiscard]] virtual std::string_view GetTypeString() const { return ""; }
		virtual ~BGSBaseAlias() = default;
	};

	class BGSRefAlias : public BGSBaseAlias
	{
	public:
		[[nodiscard]] std::string_view GetTypeString() const override { return std::string_view("Ref"); }
	};

	class TESQuest : public TESForm
	{
	public:
		std::vector<BGSBaseAlias*> aliases;
	};
}