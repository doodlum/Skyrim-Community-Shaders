#include "Common/DummyVSTexCoord.hlsl"
#include "Common/FrameBuffer.hlsli"
#include "Common/VR.hlsli"
#include "Common/MotionBlur.hlsli"
#include "Common/SharedData.hlsli"

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

static const int iterations = 32.0;
static const float rayLength = 0.5;

float2 ConvertRaySample(float2 raySample, uint eyeIndex)
{
	return FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(Stereo::ConvertToStereoUV(raySample, eyeIndex));
}

float2 ConvertRaySamplePrevious(float2 raySample, uint eyeIndex)
{
	return FrameBuffer::GetPreviousDynamicResolutionAdjustedScreenPosition(Stereo::ConvertToStereoUV(raySample, eyeIndex));
}

float4 GetReflectionColor(
	float3 projReflectionDirection, 
	float3 projPosition,
	uint eyeIndex)
{
	float3 prevRaySample;
	float3 raySample = projPosition;
    
	for (int i = 0; i <= iterations; i++)
	{
		prevRaySample = raySample;	
		raySample = projPosition + (float(i) / float(iterations)) * projReflectionDirection;

		if (FrameBuffer::isOutsideFrame(raySample.xy, true))
			return 0.0;

		float iterationDepth = DepthTex.SampleLevel(DepthSampler, ConvertRaySample(raySample.xy, eyeIndex), 0);
				
		if (raySample.z > iterationDepth)
		{
			float3 binaryMinRaySample = prevRaySample;
			float3 binaryMaxRaySample = raySample;
			float3 binaryRaySample;
			float depthThicknessFactor;

			for (int k = 0; k < iterations; k++)
			{
				binaryRaySample = lerp(binaryMinRaySample, binaryMaxRaySample, 0.5);

				iterationDepth = DepthTex.SampleLevel(DepthSampler, ConvertRaySample(binaryRaySample.xy, eyeIndex), 0);			
				
				// Compute expected depth vs actual depth
				depthThicknessFactor = 1.0 - smoothstep(0.0, 0.5, abs(binaryRaySample.z - iterationDepth) / SSRParams.y);
				
				// Early exit
				if (depthThicknessFactor == 1.0)
					break;

				if (iterationDepth < binaryRaySample.z)
					binaryMaxRaySample = binaryRaySample;
				else
					binaryMinRaySample = binaryRaySample;
			}

			// SSR Marching Radius Fade Factor (based on ray length)
			float ssrMarchingRadiusFadeFactor = 1.0 - saturate(length(binaryRaySample.xy - projPosition.xy) / rayLength);

			// Screen Center Distance Fade Factor
			float2 uvResultScreenCenterOffset = binaryRaySample.xy - 0.5;

#		ifdef VR
			float centerDistance = min(1.0, 2.0 * length(uvResultScreenCenterOffset.xy));

			// Make VR fades consistent by taking the closer of the two eyes
			// Based on concepts from https://cuteloong.github.io/publications/scssr24/
			float2 otherEyeUvResultScreenCenterOffset = Stereo::ConvertMonoUVToOtherEye(float3(binaryRaySample.xy, iterationDepth), eyeIndex).xy - 0.5;
			centerDistance = min(centerDistance, 2.0 * length(otherEyeUvResultScreenCenterOffset));
#		else
			float centerDistance = min(1.0, 2.0 * length(uvResultScreenCenterOffset.xy));
#		endif
			
			// Fade out around 10% of screen area
			float centerDistanceFadeFactor = smoothstep(0.0, 0.1, 1.0 - centerDistance);
			float fadeFactor = depthThicknessFactor * ssrMarchingRadiusFadeFactor * centerDistanceFadeFactor;

			if (fadeFactor > 0.0)
			{
				float3 color = ColorTex.SampleLevel(ColorSampler, ConvertRaySample(binaryRaySample.xy, eyeIndex), 0);

				// Final sample to world-space
				float4 positionWS = float4(float2(binaryRaySample.x, 1.0 - binaryRaySample.y) * 2.0 - 1.0, iterationDepth, 1.0);
				positionWS = mul(CameraViewProjInverse[eyeIndex], positionWS);
				positionWS.xyz = positionWS.xyz / positionWS.w;
				positionWS.w = 1.0;

				// Compute camera motion vector
				float2 cameraMotionVector = MotionBlur::GetSSMotionVector(positionWS, positionWS, eyeIndex);

				// Reproject alpha from previous frame
				float2 reprojectedRaySample = binaryRaySample.xy + cameraMotionVector;
				float4 alpha = 0.0;

				// Check that the reprojected data is within the frame
				if (!FrameBuffer::isOutsideFrame(reprojectedRaySample.xy, true))
					alpha = float4(AlphaTex.SampleLevel(AlphaSampler, ConvertRaySamplePrevious(reprojectedRaySample.xy, eyeIndex), 0).xyz, 1.0);

				float3 reflectionColor = color + SSRParams.z * alpha.xyz * alpha.w;
				return float4(reflectionColor, fadeFactor);
			}

			return 0.0;
		}
	}

	return 0.0;
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;
	psout.Color = 0;

	uint eyeIndex = Stereo::GetEyeIndexFromTexCoord(input.TexCoord);
	float2 uv = input.TexCoord;
	float2 screenPosition = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uv);

	uv = Stereo::ConvertFromStereoUV(uv, eyeIndex);

	[branch] if (NormalTex.Sample(NormalSampler, screenPosition).z <= 0)
	{
		return psout;
	}

	float3 viewNormal = DefaultNormal;

	float depth = DepthTex.SampleLevel(DepthSampler, screenPosition, 0).x;

	float4 positionVS = float4(float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0, depth, 1.0);
	positionVS = mul(CameraProjInverse[eyeIndex], positionVS);
	positionVS.xyz = positionVS.xyz / positionVS.w;

	float3 viewPosition = positionVS;
	float3 viewDirection = normalize(viewPosition);
	
	float3 reflectionDirection = reflect(viewDirection, viewNormal);
	float VdotN = dot(-viewDirection, reflectionDirection);
	[branch] if (reflectionDirection.z < 0 || 0 < VdotN)
	{
		return psout;
	}

	float4 reflectionPosition = float4(viewPosition + reflectionDirection, 1.0);
	float4 projReflectionPosition = mul(CameraProj[eyeIndex], reflectionPosition);
	projReflectionPosition /= projReflectionPosition.w;
	projReflectionPosition.xy = projReflectionPosition.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

	float3 projPosition = float3(uv, depth);
	float3 projReflectionDirection = normalize(projReflectionPosition.xyz - projPosition) * rayLength;

	psout.Color = GetReflectionColor(projReflectionDirection, projPosition, eyeIndex);

	return psout;
}
#endif
