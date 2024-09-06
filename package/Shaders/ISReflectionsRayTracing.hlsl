#include "Common/DummyVSTexCoord.hlsl"
#include "Common/FrameBuffer.hlsli"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	float4 Color : SV_Target0;
};

#if defined(PSHADER)
SamplerState NormalSampler : register(s0);
SamplerState ColorSampler : register(s1);
SamplerState DepthSampler : register(s2);
SamplerState AlphaSampler : register(s3);

Texture2D<float4> NormalTex : register(t0);
Texture2D<float4> ColorTex : register(t1);
Texture2D<float4> DepthTex : register(t2);
Texture2D<float4> AlphaTex : register(t3);

cbuffer PerGeometry : register(b2)
{
	float4 SSRParams : packoffset(c0);  // fReflectionRayThickness in x, fReflectionMarchingRadius in y, fAlphaWeight in z, 1 / fReflectionMarchingRadius in w
	float3 DefaultNormal : packoffset(c1);
};

float3 DecodeNormal(float2 encodedNormal)
{
	float2 fenc = 4 * encodedNormal - 2;
	float f = dot(fenc, fenc);

	float3 decodedNormal;
	decodedNormal.xy = fenc * sqrt(1 - f / 4);
	decodedNormal.z = -(1 - f / 2);

	return normalize(decodedNormal);
}

float3 UVDepthToView(float3 uv)
{
	//return float3(2 * uv.x - 1, 1 - 2 * uv.y, uv.z);
	return uv * float3(2, -2, 1) + float3(-1, 1, 0);
}

float3 ViewToUVDepth(float3 view)
{
	return float3(0.5 * view.x + 0.5, 0.5 - 0.5 * view.y, view.z);
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;
	psout.Color = 0;

	float2 uvStart = input.TexCoord;
	float2 uvStartDR = GetDynamicResolutionAdjustedScreenPosition(uvStart);

	float4 srcNormal = NormalTex.Sample(NormalSampler, uvStartDR);

	float depthStart = DepthTex.SampleLevel(DepthSampler, uvStartDR, 0).x;

	bool isDefaultNormal = srcNormal.z >= 1e-5;
	float ssrPower = max(srcNormal.z >= 1e-5, srcNormal.w);
	bool isSsrDisabled = ssrPower < 1e-5;

#	if defined(SPECMASK)
	float3 color = ColorTex.Sample(ColorSampler, uvStartDR).xyz;

	if (isSsrDisabled) {
		psout.Color.rgb = float3(0.5 * color.r, 0, 0);
	} else {
		psout.Color.rgb = color * (1 - float3(ssrPower, 0.5 * ssrPower, ssrPower));
	}
	psout.Color.a = 1;
#	else
	float3 decodedNormal = DecodeNormal(srcNormal.xy);
	float4 normal = float4(lerp(decodedNormal, DefaultNormal, isDefaultNormal), 0);

	float3 uvDepthStart = float3(uvStart, depthStart);
	float3 vsStart = UVDepthToView(uvDepthStart);

	float4 csStart = mul(CameraProjInverse[0], float4(vsStart, 1));
	csStart /= csStart.w;
	float4 viewDirection = float4(normalize(-csStart.xyz), 0);
	float4 reflectedDirection = reflect(-viewDirection, normal);
	float4 csFinish = csStart + reflectedDirection;
	float4 vsFinish = mul(CameraProj[0], csFinish);
	vsFinish.xyz /= vsFinish.w;

	float3 uvDepthFinish = ViewToUVDepth(vsFinish.xyz);
	float3 deltaUvDepth = uvDepthFinish - uvDepthStart;

	float3 uvDepthFinishDR = uvDepthStart + deltaUvDepth * (SSRParams.x * rcp(length(deltaUvDepth.xy)));
	uvDepthFinishDR.xy = GetDynamicResolutionAdjustedScreenPosition(uvDepthFinishDR.xy);

	float3 uvDepthStartDR = float3(uvStartDR, vsStart.z);
	float3 deltaUvDepthDR = uvDepthFinishDR - uvDepthStartDR;

	float3 uvDepthPreResultDR = uvDepthStartDR;
	float3 uvDepthResultDR = float3(uvDepthStartDR.xy, depthStart);

	float iterationIndex = 1;
	for (; iterationIndex < 16; iterationIndex += 1) {
		float3 iterationUvDepthDR = uvDepthStartDR + (iterationIndex / 16) * deltaUvDepthDR;
		float iterationDepth = DepthTex.SampleLevel(DepthSampler, iterationUvDepthDR.xy, 0).x;
		uvDepthPreResultDR = uvDepthResultDR;
		uvDepthResultDR = iterationUvDepthDR;
		if (iterationDepth < iterationUvDepthDR.z) {
			break;
		}
	}

	float3 uvDepthFinalDR = uvDepthResultDR;

	if (iterationIndex < 16) {
		iterationIndex = 0;
		uvDepthFinalDR = uvDepthPreResultDR;
		[unroll] for (; iterationIndex < 16; iterationIndex += 1)
		{
			uvDepthFinalDR = lerp(uvDepthPreResultDR, uvDepthResultDR, iterationIndex / 16);
			float subIterationDepth = DepthTex.SampleLevel(DepthSampler, uvDepthFinalDR.xy, 0).x;
			if (subIterationDepth < uvDepthFinalDR.z && uvDepthFinalDR.z < subIterationDepth + SSRParams.y) {
				break;
			}
		}
	}

	float2 uvFinal = GetDynamicResolutionUnadjustedScreenPosition(uvDepthFinalDR.xy);

	float2 previousUvFinalDR = GetPreviousDynamicResolutionAdjustedScreenPosition(uvFinal);
	float3 alpha = AlphaTex.Sample(AlphaSampler, previousUvFinalDR).xyz;

	float2 uvFinalDR = GetDynamicResolutionAdjustedScreenPosition(uvFinal);
	float3 color = ColorTex.Sample(ColorSampler, uvFinalDR).xyz;

	float3 ssrColor = SSRParams.z * alpha + color;

	[branch] if (isSsrDisabled)
	{
		return psout;
	}

	float VdotN = dot(viewDirection, reflectedDirection);
	[branch] if (reflectedDirection.z < 0 || 0 < VdotN)
	{
		return psout;
	}

	[branch] if (0 < deltaUvDepth.y || deltaUvDepth.z < 0)
	{
		return psout;
	}

	[branch] if (iterationIndex == 16)
	{
		return psout;
	}

	psout.Color.rgb = ssrColor;

	float2 deltaUv = uvFinal - uvStart;
	float ssrMarchingRadiusFadeFactor = 1 - length(deltaUv) * SSRParams.w;

	float2 uvResultScreenCenterOffset = uvFinal - 0.5;
	float centerDistance = min(1, 2 * length(uvResultScreenCenterOffset));
	float centerDistanceFadeFactor = 1 - centerDistance * centerDistance;

	psout.Color.a = ssrPower * ssrMarchingRadiusFadeFactor * centerDistanceFadeFactor;
#	endif

	return psout;
}
#endif
