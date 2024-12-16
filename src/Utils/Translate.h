#pragma once

#include <fmt/core.h>
#include <string>
#include <utility>

namespace Util
{
	std::string Translate(const std::string& key);

	struct Translatable
	{
		std::string_view key;

		template <typename... Args>
		std::string operator()(Args&&... args) const
		{
			return fmt::format(fmt::runtime(Translate(std::string(key))), std::forward<Args>(args)...);
		}

		explicit operator std::string() const noexcept
		{
			return Translate(std::string(key));
		}
	};
}

inline Util::Translatable operator"" _i18n(const char* key, std::size_t) noexcept
{
	return Util::Translatable{ std::string_view(key) };
}

inline const char* operator"" _i18n_cs(const char* key, std::size_t) noexcept
{
	thread_local std::string translation;
	translation = Util::Translate(std::string(key));
	return translation.c_str();
}
