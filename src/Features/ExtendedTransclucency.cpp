#include "ExtendedTransclucency.h"

#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	ExtendedTransclucency::Settings,
	ViewDependentAlphaMode,
	AlphaReduction,
	AlphaSoftness)

void ExtendedTransclucency::DrawSettings()
{
	if (ImGui::TreeNodeEx("View Dependent Transcluency", ImGuiTreeNodeFlags_DefaultOpen)) {
		static const char* AlphaModeNames[4] = {
			"Anisotropic Fabric",
			"Isotropic Fabric",
			"Rim Light",
			"Disabled"
		};
		ImGui::Combo("Material Model", (int*)&settings.ViewDependentAlphaMode, AlphaModeNames, 4);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"View dependent transparency will make a transcluent surface more opaque when you view it parallel to the surface.\n"
				"  - Anisotropic Fabric: Common fabric weaved from tangent and birnormal direction, ignores normal map.\n"
				"  - Isotropic Fabric: Imaginary fabric weaved from threads in one direction, respect normal map.\n"
				"  - Rim Light: Naive rim light effect\n"
				"  - Disabled: No view dependent transparency");
		}

		ImGui::SliderFloat("Increase Transparency", &settings.AlphaReduction, 0.f, 1.f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("VDT will make the material more opaque on average, which could be different from the intent, reduce the alpha to counter this effect and increase the dynamic range of the output.");
		}

		ImGui::SliderFloat("Softness", &settings.AlphaSoftness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Control the softness of VDT's alpha increase, reducing the increased amount of alpha.");
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}
}

void ExtendedTransclucency::LoadSettings(json& o_json)
{
	settings = o_json;
}

void ExtendedTransclucency::SaveSettings(json& o_json)
{
	o_json = settings;
}

void ExtendedTransclucency::RestoreDefaultSettings()
{
	settings = {};
}