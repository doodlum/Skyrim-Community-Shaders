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

	bool SupportsVR() override { return false; };

	virtual inline std::string GetName() { return "Skylighting"; }
	virtual inline std::string GetShortName() { return "Skylighting"; }
	inline std::string_view GetShaderDefineName() override { return "SKYLIGHTING"; }
	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual inline void RestoreDefaultSettings() override{};
	virtual void DrawSettings() override;

	virtual void Load(json& o_json) override;
	virtual void Save(json& o_json) override;

	virtual inline void Reset() override{};
	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;
	void CompileComputeShaders();

	virtual inline void Draw(const RE::BSShader*, const uint32_t) override{};

	virtual void Prepass() override;

	virtual void PostPostLoad() override;

	void UpdateDepthStencilView(RE::BSRenderPass* a_pass);

	//////////////////////////////////////////////////////////////////////////////////

	struct Settings
	{
		float MaxZenith = 3.1415926f / 3.f;  // 60 deg
		float MinDiffuseVisibility = 0.1;
		float DiffuseBrightness = 5;
		float MinSpecularVisibility = 0;
		float SpecularBrightness = 4;
	} settings;

	struct SkylightingCB
	{
		REX::W32::XMFLOAT4X4 OcclusionViewProj;
		float4 OcclusionDir;

		float3 PosOffset;  // cell origin in camera model space
		float _pad0;
		uint ArrayOrigin[4];
		int ValidMargin[4];

		float4 MixParams;  // x: min diffuse visibility, y: diffuse mult, z: min specular visibility, w: specular mult
	} cbData;
	static_assert(sizeof(SkylightingCB) % 16 == 0);
	eastl::unique_ptr<ConstantBuffer> skylightingCB = nullptr;

	winrt::com_ptr<ID3D11SamplerState> pointClampSampler = nullptr;

	Texture2D* texOcclusion = nullptr;
	Texture3D* texProbeArray = nullptr;
	Texture3D* texAccumFramesArray = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> probeUpdateCompute = nullptr;

	// misc parameters
	bool doOcclusion = true;
	uint probeArrayDims[3] = { 128, 128, 64 };
	float occlusionDistance = 8192.f;
	bool renderTrees = false;
	float boundSize = 128;

	// cached variables
	bool inOcclusion = false;
	RE::NiPoint3 eyePosition{};
	REX::W32::XMFLOAT4X4 OcclusionTransform;
	float4 OcclusionDir;

	//////////////////////////////////////////////////////////////////////////////////

	// Hooks
	struct BSLightingShaderProperty_GetPrecipitationOcclusionMapRenderPassesImpl
	{
		static void* thunk(RE::BSLightingShaderProperty* property, RE::BSGeometry* geometry, uint32_t renderMode, RE::BSGraphics::BSShaderAccumulator* accumulator);
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