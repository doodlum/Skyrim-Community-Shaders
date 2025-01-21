#include "Translate.h"

#include "SKSE/Translation.h"

namespace Util
{
	std::string Translate(const std::string& key)
	{
		std::string buffer;

		if (SKSE::Translation::Translate(key, buffer)) {
			return buffer;
		}

		return key;
	}
}