#include "Translate.h"

namespace Util
{
	const std::string& Translate(const std::string& key) {
		static std::string buffer;
		buffer.clear();

		if (SKSE::Translation::Translate(key, buffer)) {
			return buffer;
		}

		return key;
	}
}