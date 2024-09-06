#include "Common/DummyVSTexCoord.hlsl"
#include "Common/FrameBuffer.hlsli"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	float4 Color : SV_Target0;
};

#if defined(PSHADER)
SamplerState ImageSampler : register(s0);

Texture2D<float4> ImageTex : register(t0);

cbuffer PerGeometry : register(b2)
{
	float4 RotationMatrix : packoffset(c0);
	float4 ColorSelect : packoffset(c1);
	float4 ScaleBias : packoffset(c2);
};

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

#	if !defined(DISABLE_DYNAMIC)
	float2 screenPosition = GetDynamicResolutionAdjustedScreenPosition(input.TexCoord);
#	else
	float2 screenPosition = input.TexCoord;
#	endif

	float4 color = ImageTex.Sample(ImageSampler, screenPosition);

#	if defined(GREYSCALE)
	color = float4(dot(color, ColorSelect).xxx, color.w);
#	elif defined(MASK)
	color.w = 1 - color.x;
#	endif

	psout.Color = color;

	return psout;
}
#endif
