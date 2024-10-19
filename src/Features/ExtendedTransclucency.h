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
		Disable = 0,			// Disable view dependent transcluency
		RimEdge = 1,			// Naive rim edge model
		FabricIsotropic = 2,	// 1D fabric model
		FabricAnisotropic = 3	// 2D fabric model
	};

	struct alignas(16) Settings
	{
		uint	TransclucencyMethod;
		float	AlphaFactor = 0.85f;
		float	AlphaMax = 1.6f;
		uint	pad;
	};

	Settings settings;

	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;

	virtual bool SupportsVR() override { return true; };
};
