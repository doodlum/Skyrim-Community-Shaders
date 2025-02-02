#include "WeatherWidget.h"

#include "../EditorWindow.h"
#include <algorithm>

#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeatherWidget::Settings, currentParentBuffer, windSpeed)

WeatherWidget::~WeatherWidget()
{
}

void WeatherWidget::DrawWidget()
{
	if (ImGui::Begin(GetEditorID().c_str(), &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
		DrawMenu();

		auto editorWindow = EditorWindow::GetSingleton();
		auto& widgets = editorWindow->weatherWidgets;

		// Sets the parent widget if settings have been loaded.
		if (settings.currentParentBuffer != "None") {
			parent = GetParent();
			if (parent == nullptr)
				settings.currentParentBuffer = "None";
			strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
		}

		if (ImGui::BeginCombo("Parent", currentParentBuffer)) {
			// Option for "None"
			if (ImGui::Selectable("None", parent == nullptr)) {
				parent = nullptr;
				settings.currentParentBuffer = "None";
				strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
			}

			for (int i = 0; i < widgets.size(); i++) {
				auto& widget = widgets[i];

				// Skip self-selection
				if (widget == this)
					continue;

				// Option for each widget
				if (ImGui::Selectable(widget->GetEditorID().c_str(), parent == widget)) {
					parent = (WeatherWidget*)widget;
					settings.currentParentBuffer = widget->GetEditorID();
					strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer) - 1);
					currentParentBuffer[sizeof(currentParentBuffer) - 1] = '\0';  // Ensure null-termination
				}

				// Set default focus to the current parent
				if (parent == widget) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (parent && !parent->IsOpen()) {
			ImGui::SameLine();
			if (ImGui::Button("Open"))
				parent->SetOpen(true);
		}
	}
	ImGui::End();
}

void WeatherWidget::LoadSettings()
{
	if (!j.empty()) {
		settings = j;
		strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
		currentParentBuffer[sizeof(currentParentBuffer) - 1] = '\0';  // Ensure null-termination
	}
}

void WeatherWidget::SaveSettings()
{
	j = settings;
}

WeatherWidget* WeatherWidget::GetParent()
{
	auto editorWindow = EditorWindow::GetSingleton();
	auto& widgets = editorWindow->weatherWidgets;

	auto temp = std::find_if(widgets.begin(), widgets.end(), [&](Widget* w) { return w->GetEditorID() == settings.currentParentBuffer; });
	if (temp != widgets.end())
		return (WeatherWidget*)*temp;

	return nullptr;
}

const int WeatherWidget::GetSetting(const std::string& settingName)
{
	j = settings;

	try {
		int setting = j[setting];

		if (setting == -128) {
			return GetParent()->GetSetting(settingName);
		}
		return setting;
	} catch (const nlohmann::json::parse_error& e) {
		logger::warn("{} setting does not exist in WeatherWidget", settingName);
	}
}
