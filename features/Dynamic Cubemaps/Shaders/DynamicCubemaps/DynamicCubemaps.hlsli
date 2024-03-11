TextureCube<float4> specularTexture : register(t64);

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
float2 EnvBRDFApprox(float3 F0, float Roughness, float NoV)
{
	const float4 c0 = { -1, -0.0275, -0.572, 0.022 };
	const float4 c1 = { 1, 0.0425, 1.04, -0.04 };
	float4 r = Roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
	return AB;
}

#if !defined(WATER)
float3 GetDynamicCubemap(float2 uv, float3 N, float3 VN, float3 V, float roughness, float3 F0, float complexMaterial)
{
	float3 R = reflect(-V, N);
	float NoV = saturate(dot(N, V));

	float level = roughness * 9.0;

	float3 specularIrradiance = specularTexture.SampleLevel(SampColorSampler, R, level);
	specularIrradiance = sRGB2Lin(specularIrradiance);

	F0 = sRGB2Lin(F0);

	float2 specularBRDF = EnvBRDFApprox(F0, roughness, NoV);

	// Horizon specular occlusion
	// https://marmosetco.tumblr.com/post/81245981087
	float horizon = min(1.0 + dot(R, VN), 1.0);
	specularIrradiance *= horizon * horizon;

	// Roughness dependent fresnel
	// https://www.jcgt.org/published/0008/01/03/paper.pdf
	float3 Fr = max(1.0.xxx - roughness.xxx, F0) - F0;
	float3 S = Fr * pow(1.0 - NoV, 5.0);

	return lerp(specularIrradiance * F0, specularIrradiance * ((F0 * S) * specularBRDF.x + specularBRDF.y), complexMaterial);
}
#endif
