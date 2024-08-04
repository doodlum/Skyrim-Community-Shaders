#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "State.h"
#include "Util.h"

struct Skylighting : Feature
{
	static Skylighting* GetSingleton()
	{
		static Skylighting singleton;
		return &singleton;
	}

	virtual bool SupportsVR() override { return true; };

	virtual inline std::string GetName() override { return "Skylighting"; }
	virtual inline std::string GetShortName() override { return "Skylighting"; }
	virtual inline std::string_view GetShaderDefineName() override { return "SKYLIGHTING"; }
	virtual bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void RestoreDefaultSettings() override;
	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;
	void CompileComputeShaders();

	virtual void Prepass() override;

	virtual void PostPostLoad() override;

	ID3D11PixelShader* GetFoliagePS();
	void SkylightingShaderHacks();  // referenced in State.cpp

	//////////////////////////////////////////////////////////////////////////////////

	struct Settings
	{
		bool DirectionalDiffuse = true;
		float MaxZenith = 3.1415926f / 3.f;  // 60 deg
		int MaxFrames = 64;
		float MinDiffuseVisibility = 0.1f;
		float DiffusePower = 1.f;
		float MinSpecularVisibility = 0.f;
		float SpecularPower = 1.f;
	} settings;

	struct SkylightingCB
	{
		REX::W32::XMFLOAT4X4 OcclusionViewProj;
		float4 OcclusionDir;

		float3 PosOffset;  // cell origin in camera model space
		float _pad0;
		uint ArrayOrigin[4];  // xyz: array origin, w: max accum frames
		int ValidMargin[4];

		float4 MixParams;  // x: min diffuse visibility, y: diffuse mult, z: min specular visibility, w: specular mult

		uint DirectionalDiffuse;
		float3 _pad1;
	} cbData;
	static_assert(sizeof(SkylightingCB) % 16 == 0);
	eastl::unique_ptr<ConstantBuffer> skylightingCB = nullptr;

	winrt::com_ptr<ID3D11SamplerState> pointClampSampler = nullptr;

	Texture2D* texOcclusion = nullptr;
	Texture3D* texProbeArray = nullptr;
	Texture3D* texAccumFramesArray = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> probeUpdateCompute = nullptr;

	ID3D11PixelShader* foliagePixelShader = nullptr;

	// misc parameters
	bool doOcclusion = true;
	uint probeArrayDims[3] = { 128, 128, 64 };
	float occlusionDistance = 10000.f;
	bool renderTrees = false;
	float boundSize = 1;

	// cached variables
	bool inOcclusion = false;
	bool foliage = false;
	REX::W32::XMFLOAT4X4 OcclusionTransform;
	float4 OcclusionDir;

	std::chrono::time_point<std::chrono::system_clock> lastUpdateTimer = std::chrono::system_clock::now();

	//////////////////////////////////////////////////////////////////////////////////

	// Hooks
	struct BSLightingShaderProperty_GetPrecipitationOcclusionMapRenderPassesImpl
	{
		static RE::BSLightingShaderProperty::Data* thunk(RE::BSLightingShaderProperty* property, RE::BSGeometry* geometry, uint32_t renderMode, RE::BSGraphics::BSShaderAccumulator* accumulator);
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_Precipitation_RenderOcclusion
	{
		static void thunk();
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSUtilityShader_SetupGeometry
	{
		static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags);
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct SetViewFrustum
	{
		static void thunk(RE::NiCamera* a_camera, RE::NiFrustum* a_frustum);
		static inline REL::Relocation<decltype(thunk)> func;
	};
};