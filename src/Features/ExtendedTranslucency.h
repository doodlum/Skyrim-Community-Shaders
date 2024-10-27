#pragma once

#include "../Buffer.h"
#include "../Feature.h"

struct ExtendedTranslucency final : Feature
{
	static ExtendedTranslucency* GetSingleton();

	virtual inline std::string GetName() override { return "Extended Translucency"; }
	virtual inline std::string GetShortName() override { return "ExtendedTranslucency"; }
	virtual inline std::string_view GetShaderDefineName() override { return "EXTENDED_TRANSLUCENCY"; }
	virtual bool HasShaderDefine(RE::BSShader::Type shaderType) override { return RE::BSShader::Type::Lighting == shaderType; };
	virtual void SetupResources() override;
	virtual void PostPostLoad() override;
	virtual void DrawSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void RestoreDefaultSettings() override;
	virtual bool SupportsVR() override { return true; };

	void BSLightingShader_SetupGeometry(RE::BSRenderPass* pass);

	struct Hooks;

	enum class MaterialModel : uint32_t
	{
		Disabled = 0,
		RimLight = 1,           // Similar effect like rim light
		IsotropicFabric = 2,    // 1D fabric model, respect normal map
		AnisotropicFabric = 3,  // 2D fabric model alone tangent and binormal, ignores normal map
		Default = AnisotropicFabric
	};

	struct alignas(16) MaterialParams
	{
		uint32_t AlphaMode = std::to_underlying(MaterialModel::Default);
		float AlphaReduction = 0.15f;
		float AlphaSoftness = 0.f;
		float AlphaStrength = 0.f;
	};

	MaterialParams settings;

	static const RE::BSFixedString NiExtraDataName;
	static constexpr int materialCBIndex = 7;

	std::optional<ConstantBuffer> materialDisableCB;
	std::optional<ConstantBuffer> materialDefaultCB;
	std::optional<ConstantBuffer> materialDynamicCB;
};
