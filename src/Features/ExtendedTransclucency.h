#pragma once

#include "Feature.h"

struct ExtendedTransclucency : Feature
{
	static ExtendedTransclucency* GetSingleton()
	{
		static ExtendedTransclucency singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Extended Transclucency"; }
	virtual inline std::string GetShortName() override { return "ExtendedTransclucency"; }
	virtual inline std::string_view GetShaderDefineName() override { return "EXTENDED_TRANSCLUCENCY"; }
	virtual bool HasShaderDefine(RE::BSShader::Type shaderType) override { return shaderType == RE::BSShader::Type::Lighting; };

	enum class TranscluencyMethod : uint
	{
		AnisotropicFabric = 0,  // 2D fabric model alone tangent and binormal, ignores normal map
		IsotropicFabric = 1,    // 1D fabric model, respect normal map
		RimLight = 2,           // Similar effect like rim light
		None = 3,				// Disable view dependent transcluency
	};

	struct alignas(16) Settings
	{
		uint	ViewDependentAlphaMode = 0;
		float	AlphaReduction = 0.f;
		float	AlphaSoftness = 0.f;
		uint	pad;
	};

	Settings settings;

	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;

	virtual bool SupportsVR() override { return true; };
};
