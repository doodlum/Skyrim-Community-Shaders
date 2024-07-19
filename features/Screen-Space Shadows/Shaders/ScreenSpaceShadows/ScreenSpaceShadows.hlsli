Texture2D<unorm half> ScreenSpaceShadowsTexture : register(t17);

float GetScreenSpaceShadow(float2 a_uv, float a_noise, float3 a_viewPosition, uint a_eyeIndex)
{
	a_noise *= 2.0 * M_PI;

	half2x2 rotationMatrix = half2x2(cos(a_noise), sin(a_noise), -sin(a_noise), cos(a_noise));

	float weight = 0;
	float shadow = 0;

	static const float2 BlurOffsets[4] = {
		float2(0.381664f, 0.89172f),
		float2(0.491409f, 0.216926f),
		float2(0.937803f, 0.734825f),
		float2(0.00921659f, 0.0562151f),
	};

	for (uint i = 0; i < 4; i++) {
		float2 offset = mul(BlurOffsets[i], rotationMatrix) * 0.0025;

		float2 sampleUV = a_uv + offset;
		int3 sampleCoord = ConvertUVToSampleCoord(sampleUV, a_eyeIndex);

		float rawDepth = TexDepthSampler.Load(sampleCoord).x;
		float linearDepth = GetScreenDepth(rawDepth);

		float attenuation = 1.0 - saturate(100.0 * abs(linearDepth - a_viewPosition.z) / a_viewPosition.z);
		if (attenuation > 0.0) {
			shadow += ScreenSpaceShadowsTexture.Load(sampleCoord).x * attenuation;
			weight += attenuation;
		}
	}

	if (weight > 0.0)
		shadow /= weight;
	else
		shadow = ScreenSpaceShadowsTexture.Load(ConvertUVToSampleCoord(a_uv, a_eyeIndex)).x;

	return shadow;
}