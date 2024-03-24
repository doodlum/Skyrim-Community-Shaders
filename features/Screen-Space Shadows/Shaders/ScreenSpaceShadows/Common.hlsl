#include "../Common/VR.hlsl"

RWTexture2D<unorm half> ShadowMaskRW : register(u0);

SamplerState LinearSampler : register(s0);

Texture2D<float> DepthTexture : register(t0);

cbuffer PerFrame : register(b0)
{
	float2 BufferDim;
	float2 RcpBufferDim;
	float4x4 ProjMatrix[2];
	float4x4 InvProjMatrix[2];
	float4 CameraData;
	float4 DynamicRes;
	float4 InvDirLightDirectionVS;
	float ShadowDistance;
	uint MaxSamples;
	float FarDistanceScale;
	float FarThicknessScale;
	float FarHardness;
	float NearDistance;
	float NearThickness;
	float NearHardness;
	bool Enabled;
};

// Get a raw depth from the depth buffer.
float GetDepth(float2 uv)
{
	return DepthTexture[uv * DynamicRes.xy * BufferDim];
}

// Inverse project UV + raw depth into the view space.
float3 InverseProjectUVZ(float2 uv, float z, uint a_eyeIndex)
{
	uv.y = 1 - uv.y;
	float4 cp = float4(uv * 2 - 1, z, 1);
	float4 vp = mul(InvProjMatrix[a_eyeIndex], cp);
	return vp.xyz / vp.w;
}

float2 ViewToUV(float3 position, bool is_position, uint a_eyeIndex)
{
	float4 uv = mul(ProjMatrix[a_eyeIndex], float4(position, (float)is_position));
	return (uv.xy / uv.w) * float2(0.5f, -0.5f) + 0.5f;
}

uint GetEyeIndexFromTexCoord(float2 texCoord)
{
#ifdef VR
	return (texCoord.x >= 0.5) ? 1 : 0;
#endif  // VR
	return 0;
}
