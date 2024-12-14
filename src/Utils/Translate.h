#pragma once

#include <string>
#include <fmt/core.h>

namespace Util
{
	std::string Translate(const std::string& key);

	template <typename... Args>
	std::string Translate(const std::string& key, Args&&... args) {
		const auto translation = Translate(key);
		return fmt::format(fmt::runtime(translation), std::forward<Args>(args)...);
	}
}
