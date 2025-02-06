#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "State.h"

struct Weather : Feature
{
public:
	static Weather* GetSingleton()
	{
		static Weather singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Weather"; }
	virtual inline std::string GetShortName() override { return "Weather"; }
	virtual inline std::string_view GetShaderDefineName() override { return "WEATHER"; }

	Texture2D* diffuseIBLCubemapTexture = nullptr;
	Texture2D* diffuseIBLCubemapCPUTexture = nullptr;

	Texture2D* diffuseIBLTexture = nullptr;
	ID3D11ComputeShader* diffuseIBLCS = nullptr;

	virtual void DrawSettings() override;

	void Bind();

	virtual void Prepass() override;
	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;

	ID3D11ComputeShader* GetDiffuseIBLCS();
};
