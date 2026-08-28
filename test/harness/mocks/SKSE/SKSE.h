#pragma once
// Minimal stand-ins for the SKSE/spdlog symbols the binding sources pull in
// through src/PCH.h.

#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>

namespace SKSE
{
	namespace stl
	{
		inline void report_and_fail(std::string_view) {}
	}

	namespace log
	{
		template <typename... Args>
		void trace(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[trace] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}

		template <typename... Args>
		void debug(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[debug] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}

		template <typename... Args>
		void info(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[info] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}

		template <typename... Args>
		void warn(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[warn] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}

		template <typename... Args>
		void error(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[error] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}

		template <typename... Args>
		void critical(fmt::format_string<Args...> a_fmt, Args&&... a_args)
		{
			fmt::print("[critical] ");
			fmt::print(a_fmt, std::forward<Args>(a_args)...);
			fmt::print("\n");
		}
	}
}