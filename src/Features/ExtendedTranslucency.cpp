#include "ExtendedTranslucency.h"

#include "../Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	ExtendedTranslucency::MaterialParams,
	AlphaMode,
	AlphaReduction,
	AlphaSoftness);

const RE::BSFixedString ExtendedTranslucency::NiExtraDataName = "AnisotropicAlphaMaterial";

ExtendedTranslucency* ExtendedTranslucency::GetSingleton()
{
	static ExtendedTranslucency singleton;
	return &singleton;
}

void ExtendedTranslucency::SetupResources()
{
	// Default material model buffer only changes in settings UI
	materialDisableCB.emplace(ConstantBufferDesc<MaterialParams>());
	MaterialParams disabled{ 0, 0.f, 0.f, 0 };
	materialDisableCB->Update(disabled);
	// Default material model buffer only changes in settings UI
	materialDefaultCB.emplace(ConstantBufferDesc<MaterialParams>());
	materialDefaultCB->Update(settings);
	// Dynamic material model buffer changes when we read materials
	materialDynamicCB.emplace(ConstantBufferDesc<MaterialParams>(true));
}

void ExtendedTranslucency::BSLightingShader_SetupGeometry(RE::BSRenderPass* pass)
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto* transcluency = ExtendedTranslucency::GetSingleton();

	ID3D11DeviceContext* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
	ID3D11Buffer* buffers[1];
	if (auto* data = pass->geometry->GetExtraData(NiExtraDataName)) {
		auto params = transcluency->settings;
		const RE::NiRTTI* integerDataRTTI = REL::Relocation<const RE::NiRTTI*>{ RE::NiIntegerExtraData::Ni_RTTI }.get();
		if (data->GetRTTI() == integerDataRTTI) {
			params.AlphaMode = static_cast<RE::NiIntegerExtraData*>(data)->value;
		} else {
			params.AlphaMode = std::to_underlying(ExtendedTranslucency::MaterialModel::Default);
		}
		params.AlphaReduction = 0.f;

		buffers[0] = materialDynamicCB->CB();
		materialDynamicCB->Update(&params, sizeof(params));
		context->PSSetConstantBuffers(materialCBIndex, 1, buffers);
		//isDefaultCB = false;
	} else {
		//if (!isDefaultCB)
		//{
		//isDefaultCB = true;
		buffers[0] = materialDefaultCB->CB();
		context->PSSetConstantBuffers(materialCBIndex, 1, buffers);
		//}
	}
}

struct ExtendedTranslucency::Hooks
{
	struct BSLightingShader_SetupGeometry
	{
		static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
		{
			GetSingleton()->BSLightingShader_SetupGeometry(Pass);
			func(This, Pass, RenderFlags);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	static void Install()
	{
		stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);
		logger::info("[ExtendedTranslucency] Installed hooks");
	}
};

void ExtendedTranslucency::PostPostLoad()
{
	Hooks::Install();
}

void ExtendedTranslucency::DrawSettings()
{
	if (ImGui::TreeNodeEx("Anisotropic Translucent Material", ImGuiTreeNodeFlags_DefaultOpen)) {
		static const char* AlphaModeNames[4] = {
			"Disabled",
			"Rim Light",
			"Isotropic Fabric",
			"Anisotropic Fabric"
		};

		bool changed = false;
		if (ImGui::Combo("Default Material Model", (int*)&settings.AlphaMode, AlphaModeNames, 4)) {
			changed = true;
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"Anisotropic transluency will make the surface more opaque when you view it parallel to the surface.\n"
				"  - Disabled: No anisotropic transluency\n"
				"  - Rim Light: Naive rim light effect\n"
				"  - Isotropic Fabric: Imaginary fabric weaved from threads in one direction, respect normal map.\n"
				"  - Anisotropic Fabric: Common fabric weaved from tangent and birnormal direction, ignores normal map.\n");
		}

		if (ImGui::SliderFloat("Default Material Transparency Increase", &settings.AlphaReduction, 0.f, 1.f)) {
			changed = true;
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Anisotropic transluency will make the material more opaque on average, which could be different from the intent, reduce the alpha to counter this effect and increase the dynamic range of the output.");
		}

		if (ImGui::SliderFloat("Softness", &settings.AlphaSoftness, 0.0f, 1.0f)) {
			changed = true;
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Control the softness of the alpha increase, increase the softness reduce the increased amount of alpha.");
		}

		if (ImGui::SliderFloat("Strength", &settings.AlphaStrength, 0.0f, 1.0f)) {
			changed = true;
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Control the blend weight of the effect applied to the final result.");
		}

		if (changed && materialDefaultCB) {
			materialDefaultCB->Update(settings);
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}
}

void ExtendedTranslucency::LoadSettings(json& o_json)
{
	settings = o_json;
	if (materialDefaultCB) {
		materialDefaultCB->Update(settings);
	}
}

void ExtendedTranslucency::SaveSettings(json& o_json)
{
	o_json = settings;
}

void ExtendedTranslucency::RestoreDefaultSettings()
{
	settings = {};
	if (materialDefaultCB) {
		materialDefaultCB->Update(settings);
	}
}