Texture2D<float> TexTerraOcc : register(t40);
Texture2D<float> TexNormalisedHeight : register(t41);
Texture2D<float2> TexShadowHeight : register(t42);

float2 GetTerrainOcclusionUV(float2 xy)
{
	return xy * terraOccSettings.Scale.xy + terraOccSettings.Offset.xy;
}

float2 GetTerrainOcclusionXY(float2 uv)
{
	return (uv - terraOccSettings.Offset.xy) * terraOccSettings.InvScale.xy;
}

float GetTerrainZ(float norm_z)
{
	return lerp(terraOccSettings.ZRange.x, terraOccSettings.ZRange.y, norm_z) + terraOccSettings.HeightBias;
}

float2 GetTerrainZ(float2 norm_z)
{
	return float2(GetTerrainZ(norm_z.x), GetTerrainZ(norm_z.y));
}

void GetTerrainOcclusion(
	const float3 worldPos, const float viewDistance, SamplerState samp,
	out float terrainShadow, out float terrainAo)
{
	terrainShadow = 1;
	terrainAo = 1;

	float2 terraOccUV = GetTerrainOcclusionUV(worldPos.xy);

	if (any(terraOccUV < 0) && any(terraOccUV > 1))
		return;

	[flatten] if (terraOccSettings.EnableTerrainShadow && (viewDistance > terraOccSettings.ShadowFadeDistance.x))
	{
		float fadeFactor = saturate((viewDistance - terraOccSettings.ShadowFadeDistance.x) / (terraOccSettings.ShadowFadeDistance.y - terraOccSettings.ShadowFadeDistance.x));
		float2 shadowHeight = GetTerrainZ(TexShadowHeight.SampleLevel(samp, terraOccUV, 0));
		float shadowFraction = saturate((worldPos.z - shadowHeight.y) / (shadowHeight.x - shadowHeight.y));
		terrainShadow = lerp(1, shadowFraction, fadeFactor);
	}
	[flatten] if (terraOccSettings.EnableTerrainAO)
	{
		float terrainHeight = GetTerrainZ(TexNormalisedHeight.SampleLevel(samp, terraOccUV, 0).x);
		terrainAo = TexTerraOcc.SampleLevel(samp, terraOccUV, 0).x;

		// power
		terrainAo = pow(abs(terrainAo), terraOccSettings.AOPower);

		// height fadeout
		float fadeOut = saturate((worldPos.z - terrainHeight) * terraOccSettings.AOFadeOutHeightRcp);
		terrainAo = lerp(terrainAo, 1, fadeOut);

		terrainAo = lerp(1, terrainAo, terraOccSettings.AOMix);
	}
}