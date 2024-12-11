
#include "Common/Color.hlsli"
#include "Common/FrameBuffer.hlsli"
#include "Common/GBuffer.hlsli"
#include "Common/MotionBlur.hlsli"
#include "Common/Random.hlsli"
#include "Common/SharedData.hlsli"
#include "Common/Spherical Harmonics/SphericalHarmonics.hlsli"
#include "Common/VR.hlsli"
#include "Skylighting/Skylighting.hlsli"

Texture2D<float> DepthTexture : register(t0);
Texture3D<sh2> SkylightingProbeArray : register(t1);
Texture2D<unorm float3> NormalRoughnessTexture : register(t2);

RWTexture2D<sh2> SkylightingRW : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	float2 uv = float2(dispatchID.xy + 0.5) * SharedData::BufferDim.zw;
	uint eyeIndex = Stereo::GetEyeIndexFromTexCoord(uv);
	uv *= FrameBuffer::DynamicResolutionParams2.xy;  // Adjust for dynamic res
	uv = Stereo::ConvertFromStereoUV(uv, eyeIndex);

	float depth = DepthTexture[dispatchID.xy];

	if (depth == 1.0) {
		return;
	}

	float4 positionWS = float4(2 * float2(uv.x, -uv.y + 1) - 1, depth, 1);
	positionWS = mul(FrameBuffer::CameraViewProjInverse[eyeIndex], positionWS);
	positionWS.xyz = positionWS.xyz / positionWS.w;

	float3 normalGlossiness = NormalRoughnessTexture[dispatchID.xy];
	float3 normalVS = GBuffer::DecodeNormal(normalGlossiness.xy);
	float3 normalWS = normalize(mul(FrameBuffer::CameraViewInverse[eyeIndex], float4(normalVS, 0)).xyz);

#if defined(VR)
	float3 positionMS = positionWS.xyz + FrameBuffer::CameraPosAdjust[eyeIndex].xyz - FrameBuffer::CameraPosAdjust[0].xyz;
#else
	float3 positionMS = positionWS.xyz;
#endif

	uint3 seed = Random::pcg3d(uint3(dispatchID.xy, dispatchID.x * Math::PI));
	float3 rnd = Random::R3Modified(SharedData::FrameCount, seed / 4294967295.f);

	// https://stats.stackexchange.com/questions/8021/how-to-generate-uniformly-distributed-points-in-the-3-d-unit-ball
	float phi = rnd.x * Math::TAU;
	float cos_theta = rnd.y * 2 - 1;
	float sin_theta = sqrt(1 - cos_theta);
	float r = rnd.z;
	float4 sincos_phi;
	sincos(phi, sincos_phi.y, sincos_phi.x);

	float3 offset = float3(r * sin_theta * sincos_phi.x, r * sin_theta * sincos_phi.y, r * cos_theta);
	positionMS.xyz += offset * 64;

	SkylightingRW[dispatchID.xy] = Skylighting::sample(SharedData::skylightingSettings, SkylightingProbeArray, positionMS.xyz, normalWS);
}