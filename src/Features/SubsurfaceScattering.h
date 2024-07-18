#pragma once

#include "Buffer.h"
#include "Feature.h"

#define SSSS_N_SAMPLES 21

struct SubsurfaceScattering : Feature
{
public:
	static SubsurfaceScattering* GetSingleton()
	{
		static SubsurfaceScattering singleton;
		return &singleton;
	}

	struct DiffusionProfile
	{
		float BlurRadius;
		float Thickness;
		float3 Strength;
		float3 Falloff;
	};

	struct Settings
	{
		uint EnableCharacterLighting = false;
		DiffusionProfile BaseProfile{ 1.0f, 1.0f, { 0.48f, 0.41f, 0.28f }, { 0.56f, 0.56f, 0.56f } };
		DiffusionProfile HumanProfile{ 1.0f, 1.0f, { 0.48f, 0.41f, 0.28f }, { 1.0f, 0.37f, 0.3f } };
	};

	Settings settings;

	struct alignas(16) Kernel
	{
		float4 Sample[SSSS_N_SAMPLES];
	};

	struct alignas(16) BlurCB
	{
		Kernel BaseKernel;
		Kernel HumanKernel;
		float4 BaseProfile;
		float4 HumanProfile;
		float SSSS_FOVY;
		uint pad[3];
	};

	ConstantBuffer* blurCB = nullptr;
	BlurCB blurCBData{};

	struct PerPass
	{
		uint IsBeastRace;
	};

	std::unique_ptr<Buffer> perPass = nullptr;

	bool validMaterial = true;
	bool updateKernels = true;
	bool validMaterials = false;

	Texture2D* blurHorizontalTemp = nullptr;

	ID3D11ComputeShader* horizontalSSBlur = nullptr;
	ID3D11ComputeShader* verticalSSBlur = nullptr;

	RE::RENDER_TARGET normalsMode = RE::RENDER_TARGET::kNONE;

	virtual inline std::string GetName() { return "Subsurface Scattering"; }
	virtual inline std::string GetShortName() { return "SubsurfaceScattering"; }
	inline std::string_view GetShaderDefineName() override { return "SSS"; }

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void SetupResources();
	virtual void Reset();
	virtual void RestoreDefaultSettings();

	virtual void DrawSettings();

	float3 Gaussian(DiffusionProfile& a_profile, float variance, float r);
	float3 Profile(DiffusionProfile& a_profile, float r);
	void CalculateKernel(DiffusionProfile& a_profile, Kernel& kernel);

	void DrawSSS();

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void ClearShaderCache();
	ID3D11ComputeShader* GetComputeShaderHorizontalBlur();
	ID3D11ComputeShader* GetComputeShaderVerticalBlur();

	virtual void PostPostLoad() override;

	void BSLightingShader_SetupSkin(RE::BSRenderPass* Pass);

	virtual void Prepass() override;

	struct Hooks
	{
		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				GetSingleton()->BSLightingShader_SetupSkin(Pass);
				func(This, Pass, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);
			logger::info("[SSS] Installed hooks");
		}
	};

	bool SupportsVR() override { return true; };
};
