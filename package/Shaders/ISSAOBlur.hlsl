#include "Common/DummyVSTexCoord.hlsl"
#include "Common/FrameBuffer.hlsli"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	float4 Color : SV_Target0;
};

#if defined(PSHADER)
SamplerState sourceSampler : register(s0);

Texture2D<float4> sourceTex : register(t0);

cbuffer PerGeometry : register(b2)
{
	float4 g_ScreenInfos : packoffset(c0);
};

float GetBlurredComponent(float3 color)
{
#	if defined(AXIS_H)
	return color.x;
#	elif defined(AXIS_V)
	return color.y;
#	endif
}

float2 GetNonBlurredComponents(float3 color)
{
#	if defined(AXIS_H)
	return color.yz;
#	elif defined(AXIS_V)
	return color.xz;
#	endif
}

float3 SetBlurredComponent(float3 color, float blurredComponent)
{
#	if defined(AXIS_H)
	return float3(blurredComponent, color.yz);
#	elif defined(AXIS_V)
	return float3(color.x, blurredComponent, color.z);
#	endif
}

float GetBlurFactor(float3 color)
{
	return dot(GetNonBlurredComponents(color), float2(0.996108949, 0.00389105058));
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

	const uint sampleCount = 7;
#	if defined(AXIS_H)
	const float2 texCoordOffsets[] = {
		float2(0, 0), float2(-6, -0), float2(-4, -0), float2(-2, -0), float2(2, 0), float2(4, 0), float2(6, 0)
	};
#	elif defined(AXIS_V)
	const float2 texCoordOffsets[] = {
		float2(0, 0), float2(-0, -6), float2(-0, -4), float2(-0, -2), float2(0, 2), float2(0, 4), float2(0, 6)
	};
#	endif
	float3 offsetColors[sampleCount];
	[unroll] for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		float2 offsetTexCoord = g_ScreenInfos.zw * texCoordOffsets[sampleIndex] + input.TexCoord;
		float2 adjustedOffsetTexCoord = GetDynamicResolutionAdjustedScreenPosition(offsetTexCoord);
		offsetColors[sampleIndex] = sourceTex.Sample(sourceSampler, adjustedOffsetTexCoord).xyz;
	}

	float3 centralColor = offsetColors[0];
	float centralBlurFactor = GetBlurFactor(centralColor);
	if (centralBlurFactor == 1.0) {
		psout.Color = float4(SetBlurredComponent(centralColor, 0), 0);
		return psout;
	}

	float weight = 0;
	float weightedComponent = 0;
	const float blurParameters[] = { 0.153170004, 0.392902017, 0.422649026, 0.444893003, 0.444893003, 0.422649026, 0.392902017 };
	[unroll] for (uint offsetIndex = 0; offsetIndex < sampleCount; ++offsetIndex)
	{
		float3 offsetColor = offsetColors[offsetIndex];
		float blurFactor = GetBlurFactor(offsetColor);
		float offsetWeight = blurParameters[offsetIndex] * max(0, 1 - 2000 * abs(blurFactor - centralBlurFactor));
		weightedComponent += offsetWeight * GetBlurredComponent(offsetColor);
		weight += offsetWeight;
	}

	psout.Color = float4(SetBlurredComponent(offsetColors[0], weightedComponent / (1e-4 + weight)), 0);

	return psout;
}
#endif
