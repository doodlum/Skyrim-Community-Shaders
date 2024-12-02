Texture2D<float2> TexShadowHeight : register(t90);

namespace TerrainShadows
{
	float2 GetTerrainShadowUV(float2 xy)
	{
		return xy * terraOccSettings.Scale.xy + terraOccSettings.Offset.xy;
	}

	float GetTerrainZ(float norm_z)
	{
		return lerp(terraOccSettings.ZRange.x, terraOccSettings.ZRange.y, norm_z) - 1024;
	}

	float2 GetTerrainZ(float2 norm_z)
	{
		return float2(GetTerrainZ(norm_z.x), GetTerrainZ(norm_z.y));
	}

	float GetTerrainShadow(const float3 worldPos, SamplerState samp)
	{
		float2 terraOccUV = GetTerrainShadowUV(worldPos.xy);

		[flatten] if (terraOccSettings.EnableTerrainShadow)
		{
			float2 shadowHeight = GetTerrainZ(TexShadowHeight.SampleLevel(samp, terraOccUV, 0));
			float shadowFraction = saturate((worldPos.z - shadowHeight.y) / (shadowHeight.x - shadowHeight.y));
			return shadowFraction;
		}

		return 1.0;
	}
}
