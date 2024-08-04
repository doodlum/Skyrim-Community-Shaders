#ifndef SHARED_DATA
#define SHARED_DATA

#include "Common/Constants.hlsli"
#include "Common/VR.hlsli"

#if defined(PSHADER)

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
	uint pad0b4[2];
};

struct GrassLightingSettings
{
	float Glossiness;
	float SpecularStrength;
	float SubsurfaceScatteringAmount;
	bool OverrideComplexGrassSettings;

	float BasicGrassBrightness;
	float3 pad;
};

struct CPMSettings
{
	bool EnableComplexMaterial;
	bool EnableParallax;
	bool EnableTerrainParallax;
	bool EnableShadows;
};

struct CubemapCreatorSettings
{
	uint Enabled;
	float3 pad0;

	float4 CubemapColor;
};

struct TerraOccSettings
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

struct WetnessEffects
{
	float Time;
	float Raining;
	float Wetness;
	float PuddleWetness;

	uint EnableWetnessEffects;
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

	uint EnableRaindropFx;
	uint EnableSplashes;
	uint EnableRipples;
	uint EnableChaoticRipples;
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

struct LightLimitFixSettings
{
	uint EnableContactShadows;
	uint EnableLightsVisualisation;
	uint LightsVisualisationMode;
	uint pad0;

	uint4 ClusterSize;
};

#	define SL_INCL_STRUCT
#	include "Skylighting/Skylighting.hlsli"
#	undef SL_INCL_STRUCT

struct PBRSettings
{
	float DirectionalLightColorMultiplier;
	float PointLightColorMultiplier;
	float AmbientLightColorMultiplier;
	bool UseMultipleScattering;
	bool UseMultiBounceAO;
};

cbuffer FeatureData : register(b6)
{
	GrassLightingSettings grassLightingSettings;
	CPMSettings extendedMaterialSettings;
	CubemapCreatorSettings cubemapCreatorSettings;
	TerraOccSettings terraOccSettings;
	WetnessEffects wetnessEffects;
	LightLimitFixSettings lightLimitFixSettings;
	SkylightingSettings skylightingSettings;
	PBRSettings pbrSettings;
};

Texture2D<float4> TexDepthSampler : register(t20);

// Get a int3 to be used as texture sample coord. [0,1] in uv space
int3 ConvertUVToSampleCoord(float2 uv, uint a_eyeIndex)
{
	uv = ConvertToStereoUV(uv, a_eyeIndex);
	uv = GetDynamicResolutionAdjustedScreenPosition(uv);
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

#endif

#endif