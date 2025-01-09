#include "WorldSpaceWidget.h"

WorldSpaceWidget::~WorldSpaceWidget()
{
}

void WorldSpaceWidget::DrawWidget()
{
	if (ImGui::Begin(GetWindowTitleWithID().c_str(), &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Menu")) {
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}
	ImGui::End();
}
