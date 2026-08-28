#include "LuaApi.h"

#include <RE/C/Color.h>
#include <RE/T/TESObjectLIGH.h>

#include <cstdint>
#include <string>

namespace
{
	RE::TESObjectLIGH* ToLight(const LuaPatcher::LuaLight& a_form) { return a_form.form->As<RE::TESObjectLIGH>(); }

	sol::table PushColor(sol::state_view a_lua, const RE::Color& a_color)
	{
		sol::table row = a_lua.create_table(0, 3);
		row["r"] = a_color.red;
		row["g"] = a_color.green;
		row["b"] = a_color.blue;
		return row;
	}

	RE::Color ParseColor(const sol::object& a_value)
	{
		if (!a_value.is<sol::table>()) {
			throw sol::error{ "expected a { r, g, b } color table" };
		}
		const auto row = a_value.as<sol::table>();

		auto readChannel = [&](std::string_view a_key, std::uint8_t a_default) {
			const auto value = row.get<sol::optional<sol::object>>(a_key);
			if (!value) {
				return a_default;
			}
			if (!value->is<double>()) {
				throw sol::error{ fmt::format("bad color value '{}' (expected a number)", a_key) };
			}
			const auto channel = static_cast<lua_Integer>(value->as<double>());
			if (channel < 0 || channel > 255) {
				throw sol::error{ fmt::format("bad color value '{}' (expected 0..255)", a_key) };
			}
			return static_cast<std::uint8_t>(channel);
		};

		RE::Color result;
		result.red = readChannel("r", result.red);
		result.green = readChannel("g", result.green);
		result.blue = readChannel("b", result.blue);
		return result;
	}

	sol::object AllLights(sol::this_state a_state)
	{
		sol::state_view lua(a_state);
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& forms = dataHandler->GetFormArray<RE::TESObjectLIGH>();

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
	void RegisterLight(sol::state_view& a_lua)
	{
		a_lua.new_usertype<LuaLight>(
			"Light", sol::base_classes, sol::bases<LuaForm>(), sol::meta_function::index,
			sol::readonly_property(UnknownPropertyGetter<LuaLight>), sol::meta_function::to_string,
			[](const LuaLight& a_form) { return fmt::format("Light[{:08X}]", a_form.form->GetFormID()); }, "radius",
			sol::property([](const LuaLight& a_form) { return static_cast<lua_Integer>(ToLight(a_form)->data.radius); },
				[](LuaLight& a_form, lua_Integer a_value) {
					a_value = std::max<lua_Integer>(a_value, 0);
					a_value = std::min<lua_Integer>(a_value, 0xFFFF);
					ToLight(a_form)->data.radius = static_cast<std::uint32_t>(a_value);
				}),
			"color",
			sol::property(
				[](const LuaLight& a_form, sol::this_state a_state) {
					return PushColor(sol::state_view(a_state), ToLight(a_form)->data.color);
				},
				[](LuaLight& a_form, const sol::object& a_value) {
					ToLight(a_form)->data.color = ParseColor(a_value);
				}),
			"fov",
			sol::property([](const LuaLight& a_form) { return static_cast<double>(ToLight(a_form)->data.fov); },
				[](LuaLight& a_form, double a_value) { ToLight(a_form)->data.fov = static_cast<float>(a_value); }),
			"falloff",
			sol::property(
				[](const LuaLight& a_form) { return static_cast<double>(ToLight(a_form)->data.fallofExponent); },
				[](LuaLight& a_form, double a_value) {
					a_value = std::max<double>(a_value, 0);
					ToLight(a_form)->data.fallofExponent = static_cast<float>(a_value);
				}),
			"fade",
			sol::property([](const LuaLight& a_form) { return static_cast<double>(ToLight(a_form)->fade); },
				[](LuaLight& a_form, double a_value) {
					a_value = std::max<double>(a_value, 0);
					ToLight(a_form)->fade = static_cast<float>(a_value);
				}),
			"canCarry", sol::property([](const LuaLight& a_form) { return ToLight(a_form)->CanBeCarried(); }),
			"dynamic", sol::property([](const LuaLight& a_form) {
				return ToLight(a_form)->data.flags.all(RE::TES_LIGHT_FLAGS::kDynamic);
			}));

		sol::table patcher = a_lua["lua_patcher"].get<sol::table>();
		patcher["allLights"] = &AllLights;
	}
}