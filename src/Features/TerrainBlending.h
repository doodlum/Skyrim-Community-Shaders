#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "ShaderCache.h"

struct TerrainBlending : Feature
{
public:
	static TerrainBlending* GetSingleton()
	{
		static TerrainBlending singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Terrain Blending"; }
	virtual inline std::string GetShortName() override { return "TerrainBlending"; }
	virtual inline std::string_view GetShaderDefineName() override { return "TERRAIN_BLENDING"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type) override { return true; }

	virtual void SetupResources() override;

	ID3D11VertexShader* GetTerrainVertexShader();
	ID3D11VertexShader* GetTerrainOffsetVertexShader();

	ID3D11VertexShader* GetTerrainBlendingVertexShader();
	ID3D11PixelShader* GetTerrainBlendingPixelShader();

	ID3D11VertexShader* terrainBlendingVertexShader = nullptr;
	ID3D11PixelShader* terrainBlendingPixelShader = nullptr;

	ID3D11VertexShader* terrainVertexShader = nullptr;
	ID3D11VertexShader* terrainOffsetVertexShader = nullptr;

	virtual void PostPostLoad() override;

	ID3D11RasterizerState* rasterState = nullptr;
	ID3D11DepthStencilState* depthStencilState = nullptr;
	ID3D11BlendState* blendState = nullptr;

	bool renderDepth = false;
	bool renderTerrainDepth = false;
	bool renderAltTerrain = false;

	void TerrainShaderHacks();

	void OverrideTerrainDepth();
	void ResetDepth();
	void ResetTerrainDepth();
	void BlendPrepassDepths();

	Texture2D* terrainDepthTexture = nullptr;
	Texture2D* tempDepthTexture = nullptr;
	Texture2D* blendedDepthTexture = nullptr;

	Texture2D* terrainOffsetTexture = nullptr;

	winrt::com_ptr<ID3D11ShaderResourceView> stbn_vec1_2Dx1D_128x128x64;

	RE::BSGraphics::DepthStencilData terrainDepth;

	ID3D11ShaderResourceView* depthSRVBackup = nullptr;
	ID3D11ShaderResourceView* prepassSRVBackup = nullptr;

	virtual void ClearShaderCache() override;

	struct Hooks
	{
		struct Main_RenderDepth
		{
			static void thunk(bool a1, bool a2);
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSBatchRenderer__RenderPassImmediately
		{
			static void thunk(RE::BSRenderPass* a_pass, uint32_t a_technique, bool a_alphaTest, uint32_t a_renderFlags);
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Main_RenderWorld_RenderBatches
		{
			static void thunk(RE::BSBatchRenderer* This, uint32_t StartRange, uint32_t EndRange, uint32_t RenderFlags, int GeometryGroup);
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			// To know when we are rendering z-prepass depth vs shadows depth
			stl::write_thunk_call<Main_RenderDepth>(REL::RelocationID(35560, 36559).address() + REL::Relocate(0x395, 0x395, 0x2EE));

			// To manipulate the depth buffer write
			stl::write_thunk_call<BSBatchRenderer__RenderPassImmediately>(REL::RelocationID(100852, 107642).address() + REL::Relocate(0x29E, 0x28F));
			
			// To manipulate depth testing
			stl::write_thunk_call<Main_RenderWorld_RenderBatches>(REL::RelocationID(99938, 106583).address() + REL::Relocate(0x8E, 0x84));

			logger::info("[Terrain Blending] Installed hooks");
		}
	};
	virtual bool SupportsVR() override { return true; };
};
