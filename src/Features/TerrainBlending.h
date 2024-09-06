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

	ID3D11VertexShader* terrainVertexShader = nullptr;
	ID3D11VertexShader* terrainOffsetVertexShader = nullptr;

	ID3D11ComputeShader* GetDepthBlendShader();
	ID3D11ComputeShader* GetDepthFixShader();

	virtual void PostPostLoad() override;

	bool renderDepth = false;
	bool renderTerrainDepth = false;
	bool renderAltTerrain = false;
	bool renderWorld = false;
	bool renderTerrainWorld = false;

	void TerrainShaderHacks();

	void OverrideTerrainWorld();

	void OverrideTerrainDepth();
	void ResetDepth();
	void ResetTerrainDepth();
	void BlendPrepassDepths();
	void ResetTerrainWorld();

	Texture2D* terrainDepthTexture = nullptr;

	Texture2D* blendedDepthTexture = nullptr;
	Texture2D* blendedDepthTexture16 = nullptr;

	Texture2D* terrainOffsetTexture = nullptr;

	RE::BSGraphics::DepthStencilData terrainDepth;

	ID3D11DepthStencilState* terrainDepthStencilState = nullptr;

	ID3D11ShaderResourceView* depthSRVBackup = nullptr;
	ID3D11ShaderResourceView* prepassSRVBackup = nullptr;

	ID3D11ComputeShader* depthBlendShader = nullptr;
	ID3D11ComputeShader* depthFixShader = nullptr;

	virtual void ClearShaderCache() override;

	struct Hooks
	{
		struct Main_RenderDepth
		{
			static void thunk(bool a1, bool a2)
			{
				auto renderer = RE::BSGraphics::Renderer::GetSingleton();
				auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
				auto& zPrepassCopy = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

				static auto singleton = GetSingleton();

				if (SIE::ShaderCache::Instance().IsEnabled()) {
					mainDepth.depthSRV = singleton->blendedDepthTexture->srv.get();
					zPrepassCopy.depthSRV = singleton->blendedDepthTexture->srv.get();

					singleton->renderDepth = true;
					singleton->OverrideTerrainDepth();

					func(a1, a2);

					singleton->renderDepth = false;

					if (singleton->renderTerrainDepth) {
						singleton->renderTerrainDepth = false;
						singleton->ResetTerrainDepth();
					}

					singleton->BlendPrepassDepths();
				} else {
					mainDepth.depthSRV = singleton->depthSRVBackup;
					zPrepassCopy.depthSRV = singleton->prepassSRVBackup;
					func(a1, a2);
				}
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSBatchRenderer__RenderPassImmediately
		{
			static void thunk(RE::BSRenderPass* a_pass, uint32_t a_technique, bool a_alphaTest, uint32_t a_renderFlags)
			{
				if (SIE::ShaderCache::Instance().IsEnabled()) {
					static auto singleton = GetSingleton();
					if (singleton->renderDepth) {
						// Entering or exiting terrain depth section
						bool inTerrain = a_pass->shaderProperty && a_pass->shaderProperty->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape);
						if (singleton->renderTerrainDepth != inTerrain) {
							if (!inTerrain)
								singleton->ResetTerrainDepth();
							singleton->renderTerrainDepth = inTerrain;
						}

						if (inTerrain)
							func(a_pass, a_technique, a_alphaTest, a_renderFlags);  // Run terrain twice
					} else if (singleton->renderWorld) {
						// Entering or exiting terrain section
						bool inTerrain = a_pass->shaderProperty && a_pass->shaderProperty->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape);
						if (singleton->renderTerrainWorld != inTerrain) {
							if (!inTerrain)
								singleton->ResetTerrainWorld();
							singleton->renderTerrainWorld = inTerrain;
						}
					}
				}
				func(a_pass, a_technique, a_alphaTest, a_renderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Main_RenderWorld_RenderBatches
		{
			static void thunk(RE::BSBatchRenderer* This, uint32_t StartRange, uint32_t EndRange, uint32_t RenderFlags, int GeometryGroup)
			{
				if (SIE::ShaderCache::Instance().IsEnabled()) {
					static auto singleton = GetSingleton();

					singleton->renderWorld = true;

					func(This, StartRange, EndRange, RenderFlags, GeometryGroup);

					singleton->renderWorld = false;

					if (singleton->renderTerrainWorld) {
						singleton->renderTerrainWorld = false;
						singleton->ResetTerrainWorld();
					}

					auto renderer = RE::BSGraphics::Renderer::GetSingleton();
					auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

					mainDepth.depthSRV = singleton->depthSRVBackup;
				} else {
					func(This, StartRange, EndRange, RenderFlags, GeometryGroup);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			// To know when we are rendering z-prepass depth vs shadows depth
			stl::write_thunk_call<Main_RenderDepth>(REL::RelocationID(35560, 36559).address() + REL::Relocate(0x395, 0x395, 0x2EE));

			// To manipulate the depth buffer write, depth testing, alpha blending
			stl::write_thunk_call<BSBatchRenderer__RenderPassImmediately>(REL::RelocationID(100852, 107642).address() + REL::Relocate(0x29E, 0x28F));

			// To manipulate depth testing
			stl::write_thunk_call<Main_RenderWorld_RenderBatches>(REL::RelocationID(99938, 106583).address() + REL::Relocate(0x8E, 0x84));

			logger::info("[Terrain Blending] Installed hooks");
		}
	};
	virtual bool SupportsVR() override { return true; };
};
