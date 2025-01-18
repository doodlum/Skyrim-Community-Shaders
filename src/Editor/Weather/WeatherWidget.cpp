#include "WeatherWidget.h"

#include "../EditorWindow.h"
#include <algorithm>

#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WeatherWidget::Settings, currentParentBuffer)

WeatherWidget::~WeatherWidget()
{
}

void WeatherWidget::DrawWidget()
{
	if (ImGui::Begin(GetEditorID().c_str(), &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Menu")) {
				//Move save outside of menu
				if (ImGui::MenuItem("Save")) {
					j = settings;
					Save();
				}
				//Move save outside of menu
				if (ImGui::MenuItem("Load")) {
					Load();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		auto editorWindow = EditorWindow::GetSingleton();
		auto& widgets = editorWindow->weatherWidgets;

		// Sets the parent widget if settings have been loaded.
		if (settings.currentParentBuffer != "None") {
			auto temp = std::find_if(widgets.begin(), widgets.end(), [&](Widget* w) { return w->GetEditorID() == settings.currentParentBuffer; });
			if (temp != widgets.end())
				parent = (WeatherWidget*)*temp;
			else
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

				if (settings.currentParentBuffer == widget->GetEditorID()) {
					parent = (WeatherWidget*)widget;
					logger::info("here");
					continue;
				}

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
