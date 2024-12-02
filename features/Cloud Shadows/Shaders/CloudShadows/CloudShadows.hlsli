
TextureCube<float4> cloudShadowsTexture : register(t30);

#define CloudHeight (2e3f / 1.428e-2)
#define PlanetRadius (6371e3f / 1.428e-2)
#define RcpHPlusR (1.0 / (CloudHeight + PlanetRadius))

namespace CloudShadows
{
	float3 GetCloudShadowSampleDir(float3 rel_pos, float3 eye_to_sun)
	{
		float r = PlanetRadius;
		float3 p = (rel_pos + float3(0, 0, r)) * RcpHPlusR;
		float dotprod = dot(p, eye_to_sun);
		float lengthsqr = dot(p, p);
		float t = -dotprod + sqrt(dotprod * dotprod - dot(p, p) + 1);
		float3 v = (p + eye_to_sun * t) * (r + CloudHeight) - float3(0, 0, r);
		return v;
	}

	float GetCloudShadowMult(float3 worldPosition, SamplerState textureSampler)
	{
		float3 cloudSampleDir = GetCloudShadowSampleDir(worldPosition, DirLightDirectionShared.xyz).xyz;
		float cloudCubeSample = cloudShadowsTexture.SampleLevel(textureSampler, cloudSampleDir, 0).w;
		return 1.0 - saturate(cloudCubeSample);
	}
}
