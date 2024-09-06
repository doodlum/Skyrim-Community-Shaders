#pragma once

#include "Buffer.h"
#include <nvapi.h>

class VariableRateShading
{
public:
	static VariableRateShading* GetSingleton()
	{
		static VariableRateShading singleton;
		return &singleton;
	}

	bool enableVRS = false;

	bool nvapiLoaded = false;
	bool vrsActive = false;
	bool vrsPass = false;
	bool notLandscape = true;
	bool temporal = false;
	ID3D11Texture2D* singleEyeVRSTex[2];
	ID3D11UnorderedAccessView* singleEyeVRSUAV[2];
	ID3D11NvShadingRateResourceView* singleEyeVRSView[2];

	std::unordered_set<uint> screenTargets;

	Texture2D* reductionData = nullptr;
	ID3D11ComputeShader* computeNASDataCS = nullptr;
	ID3D11ComputeShader* computeShadingRateCS = nullptr;

	void ClearShaderCache();
	ID3D11ComputeShader* GetComputeNASData();
	ID3D11ComputeShader* GetComputeShadingRate();

	void DrawSettings();

	void UpdateVRS();
	void ComputeShadingRate();

	void Setup();
	void SetupSingleEyeVRS(int eye, int width, int height);
	void UpdateViews(bool a_enable);

	struct Hooks
	{
		struct BSUtilityShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				if (Pass->shaderProperty)
					GetSingleton()->notLandscape = Pass->shaderProperty->flags.none(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape, RE::BSShaderProperty::EShaderPropertyFlag::kLODLandscape);
				else
					GetSingleton()->notLandscape = true;
				func(This, Pass, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				if (Pass->shaderProperty)
					GetSingleton()->notLandscape = Pass->shaderProperty->flags.none(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape, RE::BSShaderProperty::EShaderPropertyFlag::kLODLandscape);
				else
					GetSingleton()->notLandscape = true;
				func(This, Pass, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<0x6, BSUtilityShader_SetupGeometry>(RE::VTABLE_BSUtilityShader[0]);
			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);
			logger::info("[VRS] Installed hooks");
		}
	};
};