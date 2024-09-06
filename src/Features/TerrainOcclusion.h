#pragma once

#include "Buffer.h"
#include "Feature.h"

struct TerrainOcclusion : public Feature
{
	static TerrainOcclusion* GetSingleton()
	{
		static TerrainOcclusion singleton;
		return std::addressof(singleton);
	}

	virtual inline std::string GetName() override { return "Terrain Occlusion"; }
	virtual inline std::string GetShortName() override { return "TerrainOcclusion"; }
	virtual inline std::string_view GetShaderDefineName() override { return "TERRA_OCC"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type) override { return true; }

	struct Settings
	{
		uint EnableTerrainShadow = true;
		uint EnableTerrainAO = false;

		float HeightBias = -1000.f;  // in game unit
		float ShadowSofteningRadiusAngle = 1.f * RE::NI_PI / 180.f;
		float2 ShadowFadeDistance = { 1000.f, 2000.f };

		float AOMix = 1.f;
		float AOPower = 1.f;
		float AOFadeOutHeight = 2000;

		float AoDistance = 12;  // in cells
		uint SliceCount = 60;
		uint SampleCount = 60;
	} settings;

	bool needPrecompute = false;
	uint shadowUpdateIdx = 0;

	struct HeightMapMetadata
	{
		std::wstring dir;
		std::string filename;
		std::string worldspace;
		float3 pos0, pos1;  // left-top-z=0 vs right-bottom-z=1
		float2 zRange;
	};
	std::unordered_map<std::string, HeightMapMetadata> heightmaps;
	HeightMapMetadata* cachedHeightmap;

	struct AOGenBuffer
	{
		float AoDistance;  // in game unit
		uint SliceCount;
		uint SampleCount;

		float3 pos0;
		float3 pos1;
		float2 zRange;
	};
	std::unique_ptr<Buffer> aoGenBuffer = nullptr;

	struct ShadowUpdateCB
	{
		float2 LightPxDir;   // direction on which light descends, from one pixel to next via dda
		float2 LightDeltaZ;  // per LightUVDir, upper penumbra and lower, should be negative
		uint StartPxCoord;
		float2 PxSize;

		float pad;
	} shadowUpdateCBData;
	static_assert(sizeof(ShadowUpdateCB) % 16 == 0);
	std::unique_ptr<ConstantBuffer> shadowUpdateCB = nullptr;

	struct alignas(16) PerFrame
	{
		uint EnableTerrainShadow;
		uint EnableTerrainAO;
		float HeightBias;
		float ShadowSofteningRadiusAngle;

		float2 ZRange;
		float2 ShadowFadeDistance;

		float AOMix;
		float3 Scale;

		float AOPower;
		float3 InvScale;

		float AOFadeOutHeightRcp;
		float3 Offset;
	};
	PerFrame GetCommonBufferData();

	winrt::com_ptr<ID3D11ComputeShader> occlusionProgram = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> shadowUpdateProgram = nullptr;

	std::unique_ptr<Texture2D> texHeightMap = nullptr;
	std::unique_ptr<Texture2D> texOcclusion = nullptr;
	std::unique_ptr<Texture2D> texNormalisedHeight = nullptr;
	std::unique_ptr<Texture2D> texShadowHeight = nullptr;

	bool IsHeightMapReady();

	virtual void SetupResources() override;
	void CompileComputeShaders();

	virtual void DrawSettings() override;

	virtual void Prepass() override;
	void LoadHeightmap();
	void Precompute();
	void UpdateShadow();

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual inline void RestoreDefaultSettings() override { settings = {}; }
	virtual void ClearShaderCache() override;
	virtual bool SupportsVR() override { return true; };
};