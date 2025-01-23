#include "TerrainBlending/FullscreenVS.hlsl"
#include "Common/SharedData.hlsli"
#include "Common/Random.hlsli"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	//float4 Color : SV_Target0;
	float Depth : SV_Depth;
};

Texture2D<unorm float> MainDepthTexture : register(t0);
Texture2D<unorm float> MainDepthTextureAfterTerrain : register(t1);
Texture2D<unorm float> TerrainDepthTexture : register(t2);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;
	
	float mainDepth = MainDepthTexture.Load(int3(input.Position.xy, 0));
	float mainDepthAfterTerrain = MainDepthTextureAfterTerrain.Load(int3(input.Position.xy, 0));
	float terrainDepth = TerrainDepthTexture.Load(int3(input.Position.xy, 0));

	float fixedDepth = mainDepth;

	if (mainDepth < mainDepthAfterTerrain)
		fixedDepth = 1;
		
	float terrainOffsetTexture = mainDepth;

	float screenNoise = Random::InterleavedGradientNoise(input.Position.xy, SharedData::FrameCount);

	input.Position.z = terrainDepth;

	float depthSampled = terrainOffsetTexture;

	float depthSampledLinear = SharedData::GetScreenDepth(depthSampled);
	float depthPixelLinear = SharedData::GetScreenDepth(input.Position.z);

	float blendFactorTerrain = saturate((depthSampledLinear - depthPixelLinear) / 5.0);

	if (terrainDepth == 1.0)
		discard;

	if (blendFactorTerrain > 1 || blendFactorTerrain < screenNoise)
		discard;

	psout.Depth = input.Position.z;
	return psout;
}