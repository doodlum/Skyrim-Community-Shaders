#include "Common/Math.hlsli"

Texture2D<unorm half> ScreenSpaceShadowsTexture : register(t17);

namespace ScreenSpaceShadows
{
	float4 GetBlurWeights(float4 depths, float centerDepth)
	{
		centerDepth += 1.0;
		float depthSharpness = saturate((1024.0 * 1024.0) / (centerDepth * centerDepth));
		float4 depthDifference = (depths - centerDepth) * depthSharpness;
		return exp2(-depthDifference * depthDifference);
	}

	float GetScreenSpaceShadow(float3 screenPosition, float2 uv, float noise, float3 viewPosition, uint eyeIndex)
	{
#if defined(VR)
		return ScreenSpaceShadowsTexture.Load(int3(screenPosition.xy, 0));
#else
		noise *= Math::TAU;

		half2x2 rotationMatrix = half2x2(cos(noise), sin(noise), -sin(noise), cos(noise));

		float4 shadowSamples = 0;
		float4 depthSamples = 0;

#if defined(DEFERRED) && !defined(DO_ALPHA_TEST)
		depthSamples[0] = screenPosition.z;
#else
		depthSamples[0] = TexDepthSampler.Load(int3(screenPosition.xy, 0));
#endif

		shadowSamples[0] = ScreenSpaceShadowsTexture.Load(int3(screenPosition.xy, 0));

		static const float2 BlurOffsets[3] = {
			float2(0.555528, 0.869625) * 2.0 - 1.0,
			float2(0.939970, 0.362499) * 2.0 - 1.0,
			float2(0.347453, 0.065981) * 2.0 - 1.0
		};

		[unroll] for (uint i = 1; i < 4; i++)
		{
			float2 offset = mul(BlurOffsets[i - 1], rotationMatrix) * 0.0025;

			float2 sampleUV = uv + offset;
			sampleUV = saturate(sampleUV);

			int3 sampleCoord = SharedData::ConvertUVToSampleCoord(sampleUV, eyeIndex);

			depthSamples[i] = TexDepthSampler.Load(sampleCoord).x;
			shadowSamples[i] = ScreenSpaceShadowsTexture.Load(sampleCoord);
		}

		depthSamples = SharedData::GetScreenDepths(depthSamples);

		float4 blurWeights = GetBlurWeights(depthSamples, viewPosition.z);
		float shadow = dot(shadowSamples, blurWeights);

		float blurWeightsTotal = dot(blurWeights, 1.0);
		[flatten] if (blurWeightsTotal > 0.0)
			shadow = shadow / blurWeightsTotal;

		return shadow;
#endif
	}
}
