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
			auto [keyArgs, otherArgs] = filterArgs(std::forward<Args>(args)...);
			std::string preprocessedKey = preprocessKey(key, keyArgs);
			std::string translatedKey = Translate(preprocessedKey);
			return formatWithArgs(translatedKey, otherArgs);
		}

		explicit operator std::string() const noexcept
		{
			return Translate(std::string(key));
		}

	private:
		template <typename... Args>
		static std::pair<std::vector<std::string_view>, std::vector<std::string>> filterArgs(Args&&... args)
		{
			std::vector<std::string_view> keyArgs;
			std::vector<std::string> otherArgs;

			auto processArg = [&](auto&& arg) {
				if constexpr (std::is_convertible_v<decltype(arg), std::string_view>) {
					if (std::string_view argView(arg); argView.starts_with('$'))
						keyArgs.emplace_back(argView.substr(1));
					else
						otherArgs.emplace_back(argView);
				} else {
					otherArgs.emplace_back(fmt::format("{}", arg));
				}
			};

			(processArg(std::forward<Args>(args)), ...);
			return { std::move(keyArgs), std::move(otherArgs) };
		}

		static std::string preprocessKey(const std::string_view key, const std::vector<std::string_view>& keyArgs)
		{
			std::string result(key);
			size_t pos = 0;

			for (const auto& arg : keyArgs) {
				if ((pos = result.find("{}", pos)) != std::string::npos) {
					result.replace(pos, 2, "{" + std::string(arg) + "}");
					pos += arg.size() + 2;
				} else
					break;
			}

			return result;
		}

		static std::string formatWithArgs(const std::string& translation, const std::vector<std::string>& args)
		{
			std::vector<fmt::format_context::format_arg> formatArgs;
			for (const auto& arg : args) {
				formatArgs.push_back(fmt::detail::make_arg<fmt::format_context>(arg));
			}

			return vformat(translation, fmt::basic_format_args(formatArgs.data(), static_cast<int>(formatArgs.size())));
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
