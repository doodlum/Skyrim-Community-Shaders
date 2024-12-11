
#include "Common/Color.hlsli"
#include "Common/FrameBuffer.hlsli"
#include "Common/GBuffer.hlsli"
#include "Common/Math.hlsli"
#include "Common/MotionBlur.hlsli"
#include "Common/Random.hlsli"
#include "Common/SharedData.hlsli"
#include "Common/Spherical Harmonics/SphericalHarmonics.hlsli"
#include "Common/VR.hlsli"

#include "Skylighting/Skylighting.hlsli"

Texture2D<float> DepthTexture : register(t0);
Texture2D<sh2> SkylightingInput : register(t1);
Texture2DArray<float> BlueNoise : register(t2);

RWTexture2D<sh2> SkylightingOutputRW : register(u0);

SamplerState LinearSampler : register(s0);

float GetBlueNoise(float2 uv)
{
	return BlueNoise[uint3(uv % 128, SharedData::FrameCount % 64)];
}

// samples = 8, min distance = 0.5, average samples on radius = 2
static const float3 g_Poisson8[8] = {
	float3(-0.4706069, -0.4427112, +0.6461146),
	float3(-0.9057375, +0.3003471, +0.9542373),
	float3(-0.3487388, +0.4037880, +0.5335386),
	float3(+0.1023042, +0.6439373, +0.6520134),
	float3(+0.5699277, +0.3513750, +0.6695386),
	float3(+0.2939128, -0.1131226, +0.3149309),
	float3(+0.7836658, -0.4208784, +0.8895339),
	float3(+0.1564120, -0.8198990, +0.8346850)
};

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	float2 uv = float2(dispatchID.xy + 0.5) * SharedData::BufferDim.zw;
	uint eyeIndex = Stereo::GetEyeIndexFromTexCoord(uv);
	uv *= FrameBuffer::DynamicResolutionParams2.xy;  // Adjust for dynamic res
	uv = Stereo::ConvertFromStereoUV(uv, eyeIndex);

	float rawDepth = DepthTexture[dispatchID.xy];

	if (rawDepth == 1.0) {
		return;
	}

	float depth = SharedData::GetScreenDepth(rawDepth);

	float2 rotation;
	sincos(Math::TAU * GetBlueNoise(dispatchID.xy), rotation.y, rotation.x);
	float2x2 rotationMatrix = float2x2(rotation.x, rotation.y, -rotation.y, rotation.x);

	sh2 skylighting = 0;
	float weight = 0;

	// [unroll] for (uint i = 0; i < 8; i++)
	// {
	// 	#if defined(FLIP)
	// 			float2 sampleOffset = mul(g_Poisson8[i].xy, rotationMatrix) * 16;
	// 	#else
	// 			float2 sampleOffset = mul(g_Poisson8[i].yx, rotationMatrix) * 8;
	// 	#endif

	// 	float2 testUV = uv + sampleOffset.xy * SharedData::BufferDim.zw;

	// 	if (any(testUV < 0) || any(testUV > 1))
	// 		continue;

	// 	float sampleDepth = SharedData::GetScreenDepth(DepthTexture.SampleLevel(LinearSampler, testUV, 0));
	// 	float attenuation =  1.0 - saturate(0.05 * abs(sampleDepth - depth));

	// 	[branch] if (attenuation > 0.0)
	// 	{
	// 		skylighting += SkylightingInput.SampleLevel(LinearSampler, testUV, 0) * attenuation;
	// 		weight += attenuation;
	// 	}
	// }

	if (weight == 0) {
		skylighting = SkylightingInput[dispatchID.xy];
		weight = 1;
	}

	SkylightingOutputRW[dispatchID.xy] = skylighting / weight;
}