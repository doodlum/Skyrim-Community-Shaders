#include "Common/Random.hlsli"
#include "Common/SharedData.hlsli"
#include "TerrainBlending/FullscreenVS.hlsl"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	float Color : SV_Target;
	float Depth : SV_Depth;
};

Texture2D<unorm float> MainDepthTexture : register(t0);
Texture2D<unorm float> TerrainDepthTexture : register(t1);
Texture2DArray<unorm float> BlueNoise : register(t2);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

	float terrainOffset = MainDepthTexture.Load(int3(input.Position.xy, 0));
	float terrainDepth = TerrainDepthTexture.Load(int3(input.Position.xy, 0));
	float screenNoise = BlueNoise[int3(input.Position.xy % 128, SharedData::FrameCount % 64)];

	float terrainOffsetLinear = SharedData::GetScreenDepth(terrainOffset);
	float terrainDepthLinear = SharedData::GetScreenDepth(terrainDepth);

	float blendFactorTerrain = saturate((terrainOffsetLinear - terrainDepthLinear) / 10.0);

	psout.Color = min(terrainOffset, terrainDepth);

	bool useOffset = (blendFactorTerrain > 1 || blendFactorTerrain < screenNoise);

	psout.Depth = useOffset ? terrainOffset : min(terrainDepth, terrainOffset);
	return psout;
}