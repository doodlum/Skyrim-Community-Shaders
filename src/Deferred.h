#pragma once

#include "Buffer.h"
#include "State.h"
#include "Util.h"

#define ALBEDO RE::RENDER_TARGETS::kINDIRECT
#define SPECULAR RE::RENDER_TARGETS::kINDIRECT_DOWNSCALED
#define REFLECTANCE RE::RENDER_TARGETS::kRAWINDIRECT
#define NORMALROUGHNESS RE::RENDER_TARGETS::kRAWINDIRECT_DOWNSCALED
#define MASKS RE::RENDER_TARGETS::kRAWINDIRECT_PREVIOUS
#define MASKS2 RE::RENDER_TARGETS::kRAWINDIRECT_PREVIOUS_DOWNSCALED

class Deferred
{
public:
	static Deferred* GetSingleton()
	{
		static Deferred singleton;
		return &singleton;
	}

	void SetupResources();
	void CopyShadowData();
	void StartDeferred();
	void OverrideBlendStates();
	void ResetBlendStates();
	void DeferredPasses();
	void EndDeferred();

	void PrepassPasses();

	void ClearShaderCache();
	ID3D11ComputeShader* GetComputeAmbientComposite();
	ID3D11ComputeShader* GetComputeAmbientCompositeInterior();
	ID3D11ComputeShader* GetComputeMainComposite();

	ID3D11ComputeShader* GetComputeMainCompositeInterior();

	ID3D11BlendState* deferredBlendStates[7][2][13][2];
	ID3D11BlendState* forwardBlendStates[7][2][13][2];

	RE::RENDER_TARGET forwardRenderTargets[4];

	ID3D11ComputeShader* ambientCompositeCS = nullptr;
	ID3D11ComputeShader* ambientCompositeInteriorCS = nullptr;

	ID3D11ComputeShader* mainCompositeCS = nullptr;
	ID3D11ComputeShader* mainCompositeInteriorCS = nullptr;

	bool inWorld = false;
	bool inDecals = false;
	bool deferredPass = false;

	Texture2D* prevDiffuseAmbientTexture = nullptr;

	ID3D11SamplerState* linearSampler = nullptr;

	struct alignas(16) PerGeometry
	{
		float4 VPOSOffset;
		float4 ShadowSampleParam;    // fPoissonRadiusScale / iShadowMapResolution in z and w
		float4 EndSplitDistances;    // cascade end distances int xyz, cascade count int z
		float4 StartSplitDistances;  // cascade start ditances int xyz, 4 int z
		float4 FocusShadowFadeParam;
		float4 DebugColor;
		float4 PropertyColor;
		float4 AlphaTestRef;
		float4 ShadowLightParam;  // Falloff in x, ShadowDistance squared in z
		DirectX::XMFLOAT4X3 FocusShadowMapProj[4];
		// Since PerGeometry is passed between c++ and hlsl, can't have different defines due to strong typing
		DirectX::XMFLOAT4X3 ShadowMapProj[2][3];
		DirectX::XMFLOAT4X3 CameraViewProjInverse[2];
	};

	ID3D11ComputeShader* copyShadowCS = nullptr;
	Buffer* perShadow = nullptr;
	ID3D11ShaderResourceView* shadowView = nullptr;

	struct Hooks
	{
		struct Main_RenderWorld
		{
			static void thunk(bool a1)
			{
				GetSingleton()->inWorld = true;
				func(a1);
				GetSingleton()->inWorld = false;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Main_RenderWorld_Start
		{
			static void thunk(RE::BSBatchRenderer* This, uint32_t StartRange, uint32_t EndRanges, uint32_t RenderFlags, int GeometryGroup)
			{
				// Here is where the first opaque objects start rendering
				GetSingleton()->StartDeferred();
				func(This, StartRange, EndRanges, RenderFlags, GeometryGroup);  // RenderBatches
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Main_RenderWorld_Decals
		{
			static void thunk(RE::BSShaderAccumulator* This, uint32_t RenderFlags)
			{
				GetSingleton()->EndDeferred();
				// After this point, blended decals start rendering
				GetSingleton()->inDecals = true;
				func(This, RenderFlags);
				GetSingleton()->inDecals = false;
				// After this point, water starts rendering
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<Main_RenderWorld>(REL::RelocationID(35560, 36559).address() + REL::Relocate(0x831, 0x841, 0x791));
			stl::write_thunk_call<Main_RenderWorld_Start>(REL::RelocationID(99938, 106583).address() + REL::Relocate(0x8E, 0x84));
			stl::write_thunk_call<Main_RenderWorld_Decals>(REL::RelocationID(99938, 106583).address() + REL::Relocate(0x319, 0x308, 0x321));
			logger::info("[Deferred] Installed hooks");
		}
	};
};