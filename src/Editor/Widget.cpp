#include "Widget.h"
#include "State.h"
#include "Util.h"

void Widget::Save()
{
	const std::string filePath = std::format("{}\\{}s", State::GetSingleton()->folderPath, RE::FormTypeToString(form->GetFormType()));
	const std::string file = std::format("{}\\{}.json", filePath, GetEditorID());

	std::ofstream settingsFile(file);
	try {
		std::filesystem::create_directories(filePath);
	} catch (const std::filesystem::filesystem_error& e) {
		logger::warn("Error creating directory during Save ({}) : {}\n", filePath, e.what());
		return;
	}

	if (!settingsFile.good() || !settingsFile.is_open()) {
		logger::warn("Failed to open settings file: {}", file);
		return;
	}

	if (settingsFile.fail()) {
		logger::warn("Unable to create settings file: {}", file);
		settingsFile.close();
		return;
	}

	logger::info("Saving settings file: {}", file);

	try {
		settingsFile << j.dump(1);

		settingsFile.close();
	} catch (const nlohmann::json::parse_error& e) {
		logger::warn("Error parsing settings for settings file ({}) : {}\n", filePath, e.what());
		settingsFile.close();
	}
}

void Widget::Load()
{
	std::string filePath = std::format("{}\\{}s\\{}.json", State::GetSingleton()->folderPath, RE::FormTypeToString(form->GetFormType()), GetEditorID());

	std::ifstream settingsFile(filePath);

	if (!std::filesystem::exists(filePath)) {
		// Does not have any settings so just return.
		return;
	}

	if (!settingsFile.good() || !settingsFile.is_open()) {
		logger::warn("Failed to load settings file: {}", filePath);
		return;
	}

	try {
		j << settingsFile;
		settingsFile.close();
	} catch (const nlohmann::json::parse_error& e) {
		logger::warn("Error parsing settings for file ({}) : {}\n", filePath, e.what());
		settingsFile.close();
	}
	LoadSettings();
}
