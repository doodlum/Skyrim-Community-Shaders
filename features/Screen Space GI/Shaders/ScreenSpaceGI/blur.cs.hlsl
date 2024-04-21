// FAST DENOISING WITH SELF-STABILIZING RECURRENT BLURS
// 	https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s22699-fast-denoising-with-self-stabilizing-recurrent-blurs.pdf

#include "../Common/FastMath.hlsli"
#include "../Common/GBuffer.hlsli"
#include "../Common/VR.hlsli"
#include "common.hlsli"

Texture2D<lpfloat4> srcGI : register(t0);              // maybe half-res
Texture2D<unorm float> srcAccumFrames : register(t1);  // maybe half-res
Texture2D<half> srcDepth : register(t2);
Texture2D<half4> srcNormal : register(t3);

RWTexture2D<lpfloat4> outGI : register(u0);

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

[numthreads(8, 8, 1)] void main(const uint2 dtid : SV_DispatchThreadID) {
	const float srcScale = SrcFrameDim * RcpTexDim;
	const float outScale = OutFrameDim * RcpTexDim;

	float radius = BlurRadius;
#ifdef TEMPORAL_DENOISER
	radius /= (srcAccumFrames[dtid] * 255);
#endif
	const uint numSamples = 8;

	const float2 uv = (dtid + .5) * RcpOutFrameDim;
	uint eyeIndex = GET_EYE_IDX(uv);
	const float2 screenPos = ConvertToStereoUV(uv, eyeIndex);

	float depth = READ_DEPTH(srcDepth, dtid);
	float3 pos = ScreenToViewPosition(screenPos, depth, eyeIndex);
	float3 normal = DecodeNormal(FULLRES_LOAD(srcNormal, dtid, uv, samplerLinearClamp).xy);

	lpfloat4 sum = srcGI[dtid];
	float4 wsum = 1;
	for (uint i = 0; i < numSamples; i++) {
		float w = g_Poisson8[i].z;

		float2 pxOffset = radius * g_Poisson8[i].xy;
		float2 pxSample = dtid + .5 + pxOffset;
		float2 uvSample = pxSample * RcpOutFrameDim;
		float2 screenPosSample = ConvertToStereoUV(uvSample, eyeIndex);

		if (any(screenPosSample < 0) || any(screenPosSample > 1))
			continue;

		float depthSample = srcDepth.SampleLevel(samplerLinearClamp, uvSample * srcScale, 0);
		float3 posSample = ScreenToViewPosition(screenPosSample, depthSample, eyeIndex);

		float3 normalSample = DecodeNormal(srcNormal.SampleLevel(samplerLinearClamp, uvSample * srcScale, 0).xy);

		// geometry weight
		w *= saturate(1 - abs(dot(normal, posSample - pos)) * DistanceNormalisation);
		// normal weight
		w *= 1 - saturate(acosFast4(saturate(dot(normalSample, normal))) / fsl_HALF_PI * 2);

		lpfloat4 gi = srcGI.SampleLevel(samplerLinearClamp, uvSample * outScale, 0);

		sum += gi * w;
		wsum += w;
	}

	outGI[dtid] = sum / wsum;
}