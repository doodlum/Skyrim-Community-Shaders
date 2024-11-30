#pragma once

#include <winrt/base.h>

#include "Feature.h"

struct WaterLighting : Feature
{
public:
	static WaterLighting* GetSingleton()
	{
		static WaterLighting singleton;
		return &singleton;
	}

	winrt::com_ptr<ID3D11ShaderResourceView> causticsView;

	virtual inline std::string GetName() override { return "Water Lighting"; }
	virtual inline std::string GetShortName() override { return "WaterLighting"; }
	virtual inline std::string_view GetShaderDefineName() override { return "WATER_LIGHTING"; }

	bool HasShaderDefine(RE::BSShader::Type shaderType) override;

	virtual void SetupResources() override;

	virtual void Prepass() override;

	virtual bool SupportsVR() override { return true; };
};
