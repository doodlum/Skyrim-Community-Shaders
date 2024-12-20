#pragma once
#include <DirectXMath.h>
#include <d3d11.h>

#include "Buffer.h"
#include "Feature.h"

struct PhysicalSky : Feature
{
	static PhysicalSky* GetSingleton()
	{
		static PhysicalSky singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "PhysicalSky"; }
	virtual inline std::string GetShortName() override { return "PhysicalSky"; }
	virtual inline std::string_view GetShaderDefineName() override { return "PHYSICAL_SKY"; }

	virtual bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	struct Settings
	{
		bool EnablePhysicalSky;
	} settings;

	struct alignas(16) AtmosphereCB
	{
		float AtmosphericRadius;
		float AtmosphericDepth;

		float4 RayleighScattering;
		float4 RayleighAbsorption;
		float RayleighHeight;

		float4 MieScattering;
		float MieAbsorption;
		float MieHeight;
		float MieAnisotropy;

		float4 OzoneAbsorption;
		float OzoneCenterAltitude;
		float OzoneWidth;
	};
	eastl::unique_ptr<ConstantBuffer> atmosphereCB = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> indirectIrradianceCS = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> multiScatteringLutCS = nullptr;

	virtual bool SupportsVR() override { return true; };

	virtual void SetupResources() override;

	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;

	void CompileComputeShaders();

	struct Hooks
	{
		static void Install()
		{
			logger::info("[PHYSKY] Installed hooks");
		}
	};
};
