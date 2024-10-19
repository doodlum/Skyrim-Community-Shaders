#include "ExtendedTransclucency.h"

#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	ExtendedTransclucency::Settings,
	TransclucencyMethod,
	AlphaFactor,
	AlphaMax)

void ExtendedTransclucency::DrawSettings()
{
	if (ImGui::TreeNodeEx("Complex Transcluency", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderInt("Enable View Dependent Transparency", (int*)&settings.TransclucencyMethod, 0, 3);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("View dependent transparency will make a transcluent surface more opaque when you view it parallel to the surface.");
		}

		ImGui::TextWrapped("Specular highlights for complex grass");
		ImGui::SliderFloat("Alpha Factor", &settings.AlphaFactor, 0.f, 1.f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("VDT will make the material more opaque on average, which could be different from the intent, reduce the alpha factor to counter this effect and increase the dynamic range of the output.");
		}

		ImGui::SliderFloat("Alpha Limit", &settings.AlphaMax, 0.0f, 2.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Control the softness of VDT's alpha increase, value greater than AlphaLimit/2 will be remapped.");
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