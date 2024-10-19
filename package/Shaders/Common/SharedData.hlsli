#ifndef SHARED_DATA
#define SHARED_DATA

#include "Common/FrameBuffer.hlsli"
#include "Common/VR.hlsli"

#if defined(PSHADER) || defined(COMPUTESHADER)
cbuffer SharedData : register(b5)
{
	float4 WaterData[25];
	row_major float3x4 DirectionalAmbientShared;
	float4 DirLightDirectionShared;
	float4 DirLightColorShared;
	float4 CameraData;
	float4 BufferDim;
	float Timer;
	uint FrameCount;
	uint FrameCountAlwaysActive;
	bool InInterior;  // If the area lacks a directional shadow light e.g. the sun or moon
	bool InMapMenu;   // If the world/local map is open (note that the renderer is still deferred here)
	float3 pad0;
};

struct GrassLightingSettings
{
	float Glossiness;
	float SpecularStrength;
	float SubsurfaceScatteringAmount;
	bool OverrideComplexGrassSettings;

	float BasicGrassBrightness;
	float3 pad0;
};

struct CPMSettings
{
	bool EnableComplexMaterial;
	bool EnableParallax;
	bool EnableTerrainParallax;
	bool EnableHeightBlending;
	bool EnableShadows;
	bool ExtendShadows;
	float2 pad0;
};

struct CubemapCreatorSettings
{
	uint Enabled;
	float3 pad0;

	float4 CubemapColor;
};

struct TerraOccSettings
{
	bool EnableTerrainShadow;
	float3 Scale;
	float2 ZRange;
	float2 Offset;
};

struct LightLimitFixSettings
{
	uint EnableContactShadows;
	uint EnableLightsVisualisation;
	uint LightsVisualisationMode;
	float LightsFar;
	uint4 ClusterSize;
};

struct WetnessEffectsSettings
{
	float Time;
	float Raining;
	float Wetness;
	float PuddleWetness;

	bool EnableWetnessEffects;
	float MaxRainWetness;
	float MaxPuddleWetness;
	float MaxShoreWetness;

	uint ShoreRange;
	float PuddleRadius;
	float PuddleMaxAngle;
	float PuddleMinWetness;

	float MinRainWetness;
	float SkinWetness;
	float WeatherTransitionSpeed;
	bool EnableRaindropFx;

	bool EnableSplashes;
	bool EnableRipples;
	bool EnableChaoticRipples;
	float RaindropFxRange;

	float RaindropGridSizeRcp;
	float RaindropIntervalRcp;
	float RaindropChance;
	float SplashesLifetime;

	float SplashesStrength;
	float SplashesMinRadius;
	float SplashesMaxRadius;
	float RippleStrength;

	float RippleRadius;
	float RippleBreadth;
	float RippleLifetimeRcp;
	float ChaoticRippleStrength;

	float ChaoticRippleScaleRcp;
	float ChaoticRippleSpeed;
};

struct SkylightingSettings
{
	row_major float4x4 OcclusionViewProj;
	float4 OcclusionDir;

	float4 PosOffset;   // xyz: cell origin in camera model space
	uint4 ArrayOrigin;  // xyz: array origin
	int4 ValidMargin;

	float MinDiffuseVisibility;
	float MinSpecularVisibility;
	uint pad0[2];
};

struct PBRSettings
{
	bool UseMultipleScattering;
	bool UseMultiBounceAO;
	uint pad0[2];
};

struct TransclucencySettings
{
	uint TransclucencyMethod;
	float AlphaFactor;
	float AlphaMax;
	uint pad0;
}

cbuffer FeatureData : register(b6)
{
	GrassLightingSettings grassLightingSettings;
	CPMSettings extendedMaterialSettings;
	CubemapCreatorSettings cubemapCreatorSettings;
	TerraOccSettings terraOccSettings;
	LightLimitFixSettings lightLimitFixSettings;
	WetnessEffectsSettings wetnessEffectsSettings;
	SkylightingSettings skylightingSettings;
	PBRSettings pbrSettings;
	TransclucencySettings transclucencySettings;
};

Texture2D<float4> TexDepthSampler : register(t20);

namespace SharedData
{

	// Get a int3 to be used as texture sample coord. [0,1] in uv space
	int3 ConvertUVToSampleCoord(float2 uv, uint a_eyeIndex)
	{
		uv = Stereo::ConvertToStereoUV(uv, a_eyeIndex);
		uv = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uv);
		return int3(uv * BufferDim.xy, 0);
	}

	// Get a raw depth from the depth buffer. [0,1] in uv space
	float GetDepth(float2 uv, uint a_eyeIndex = 0)
	{
		return TexDepthSampler.Load(ConvertUVToSampleCoord(uv, a_eyeIndex)).x;
	}

	float GetScreenDepth(float depth)
	{
		return (CameraData.w / (-depth * CameraData.z + CameraData.x));
	}

	float4 GetScreenDepths(float4 depths)
	{
		return (CameraData.w / (-depths * CameraData.z + CameraData.x));
	}

	float GetScreenDepth(float2 uv, uint a_eyeIndex = 0)
	{
		float depth = GetDepth(uv, a_eyeIndex);
		return GetScreenDepth(depth);
	}

	float4 GetWaterData(float3 worldPosition)
	{
		float2 cellF = (((worldPosition.xy + CameraPosAdjust[0].xy)) / 4096.0) + 64.0;  // always positive
		int2 cellInt;
		float2 cellFrac = modf(cellF, cellInt);

		cellF = worldPosition.xy / float2(4096.0, 4096.0);  // remap to cell scale
		cellF += 2.5;                                       // 5x5 cell grid
		cellF -= cellFrac;                                  // align to cell borders
		cellInt = round(cellF);

		uint waterTile = (uint)clamp(cellInt.x + (cellInt.y * 5), 0, 24);  // remap xy to 0-24

		float4 waterData = float4(1.0, 1.0, 1.0, -2147483648);

		[flatten] if (cellInt.x < 5 && cellInt.x >= 0 && cellInt.y < 5 && cellInt.y >= 0)
			waterData = WaterData[waterTile];
		return waterData;
	}

}

#endif  // PSHADER

#endif  // SHARED_DATA