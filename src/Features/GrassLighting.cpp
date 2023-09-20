#include "GrassLighting.h"

#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	GrassLighting::Settings,
	Glossiness,
	SpecularStrength,
	SubsurfaceScatteringAmount,
	EnableDirLightFix,
	Brightness,
	Saturation)

enum class GrassShaderTechniques
{
	RenderDepth = 8,
};

void GrassLighting::DrawSettings()
{
	if (ImGui::TreeNodeEx("Complex Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("Specular highlights for complex grass");
		ImGui::SliderFloat("Glossiness", &settings.Glossiness, 1.0f, 100.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Specular highlight glossiness.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SliderFloat("Specular Strength", &settings.SpecularStrength, 0.0f, 1.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Specular highlight strength.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("SSS Amount", &settings.SubsurfaceScatteringAmount, 0.0f, 2.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Subsurface Scattering (SSS) amount\n");
			ImGui::Text("Soft lighting controls how evenly lit an object is.\n");
			ImGui::Text("Back lighting illuminates the back face of an object.\n");
			ImGui::Text("Combined to model the transport of light through the surface.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Directional Light Fix", (bool*)&settings.EnableDirLightFix);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Fix for grass not being affected by sunlight scale.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TextWrapped("Grass Color");
		ImGui::SliderFloat("Brightness", &settings.Brightness, 0.0f, 1.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Darkens the grass textures to look better with the new lighting");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SliderFloat("Saturation", &settings.Saturation, 1.0f, 2.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Boosts the vibrancy of textures");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::TreePop();
	}
}

void GrassLighting::ModifyGrass(const RE::BSShader*, const uint32_t descriptor)
{
	const auto technique = descriptor & 0b1111;
	if (technique != static_cast<uint32_t>(GrassShaderTechniques::RenderDepth)) {
		if (updatePerFrame) {
			auto& state = RE::BSShaderManager::State::GetSingleton();
			RE::NiTransform& dalcTransform = state.directionalAmbientTransform;
			auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();

			PerFrame perFrameData{};
			ZeroMemory(&perFrameData, sizeof(perFrameData));
			Util::StoreTransform3x4NoScale(perFrameData.DirectionalAmbient, dalcTransform);

			perFrameData.SunlightScale = !REL::Module::IsVR() ?
			                                 imageSpaceManager->GetRuntimeData().data.baseData.hdr.sunlightScale :
			                                 imageSpaceManager->GetVRRuntimeData().data.baseData.hdr.sunlightScale;
			perFrameData.Settings = settings;
			perFrame->Update(perFrameData);

			updatePerFrame = false;
		}

		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

		ID3D11Buffer* buffers[2];
		context->VSGetConstantBuffers(2, 1, buffers);  // buffers[0]
		buffers[1] = perFrame->CB();
		context->VSSetConstantBuffers(2, ARRAYSIZE(buffers), buffers);
		context->PSSetConstantBuffers(3, ARRAYSIZE(buffers), buffers);
	}
}

void GrassLighting::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	switch (shader->shaderType.get()) {
	case RE::BSShader::Type::Grass:
		ModifyGrass(shader, descriptor);
		break;
	}
}

void GrassLighting::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void GrassLighting::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

void GrassLighting::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
}

void GrassLighting::Reset()
{
	updatePerFrame = true;
}