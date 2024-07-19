
Texture2D<float4> TerrainBlendingMaskTexture : register(t35);

float2 ShiftTerrain(float blendFactor, float2 coords, float3 viewDir, float3x3 tbn)
{
	float3 viewDirTS = mul(viewDir, tbn).xyz;
	return -viewDirTS.xy * 0.01 + coords.xy;
}

// Get a raw depth from the depth buffer. [0,1] in uv space
float GetTerrainOffsetDepth(float2 uv, uint a_eyeIndex = 0)
{
	return TerrainBlendingMaskTexture.Load(ConvertUVToSampleCoord(uv, a_eyeIndex));
}