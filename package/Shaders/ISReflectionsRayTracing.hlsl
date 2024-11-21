#include "Common/DummyVSTexCoord.hlsl"
#include "Common/FrameBuffer.hlsli"
#include "Common/VR.hlsli"

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

float3 ViewToUVDepthHelper(float3 x, uint eyeIndex)
{
	float4 newPosition = float4(x, 1.0);
	float4 uv = mul(CameraProj[eyeIndex], newPosition);
	uv.xyz /= uv.w;
	float3 uvDepth = float3(uv.xy * float2(0.5, -0.5) + 0.5, uv.z);
	uvDepth.xy = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uvDepth.xy);
	return uvDepth;
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;
	psout.Color = 0;

#	ifndef ENABLESSR
	// Disable SSR raymarch
	return psout;
#	endif
	uint eyeIndex = Stereo::GetEyeIndexFromTexCoord(input.TexCoord);
	float2 uvStart = input.TexCoord;
	float2 uvStartDR = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uvStart);

	float4 srcNormal = NormalTex.Sample(NormalSampler, uvStartDR);

	float depthStart = DepthTex.SampleLevel(DepthSampler, uvStartDR, 0).x;

#	ifdef VR
	[branch] if (depthStart == 0.0f)  // in VR mask
	{
		return psout;
	}
#	endif

	bool isDefaultNormal = srcNormal.z >= 1e-5;
	float ssrPower = max(srcNormal.z >= 1e-5, srcNormal.w);
	bool isSsrDisabled = ssrPower < 1e-5;

	[branch] if (isSsrDisabled)
	{
		psout.Color = 0;
		return psout;
	}

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
	uvDepthStart.xy = Stereo::ConvertFromStereoUV(uvStart.xy, eyeIndex, 0);
	float3 vsStart = UVDepthToView(uvDepthStart);

	float4 csStart = mul(CameraProjInverse[eyeIndex], float4(vsStart, 1));
	csStart /= csStart.w;
	float4 viewDirection = float4(normalize(-csStart.xyz), 0);
	float4 reflectedDirection = normalize(reflect(-viewDirection, normal));
	float4 csFinish = csStart + reflectedDirection;
	float4 vsFinish = mul(CameraProj[eyeIndex], csFinish);
	vsFinish.xyz /= vsFinish.w;

	float3 uvDepthFinish = ViewToUVDepth(vsFinish.xyz);
	float3 deltaUvDepth = uvDepthFinish - uvDepthStart;

	float3 uvDepthFinishDR = uvDepthStart + deltaUvDepth * SSRParams.x * rcp(length(deltaUvDepth.xy));
	uvDepthFinishDR.xy = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uvDepthFinishDR.xy);

#		ifdef VR
	uvStartDR.xy = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uvDepthStart.xy);
#		endif

	float3 uvDepthStartDR = float3(uvStartDR, vsStart.z);

	float3 uvDepthPreResultDR = uvDepthStartDR;
	float3 uvDepthResultDR = float3(uvDepthStartDR.xy, depthStart);

	int iterationIndex = 1;
#		ifdef VR
	bool fromOtherEye = false;
#		endif
	const int maxIterations =
#		ifdef MAX_ITERATIONS
		MAX_ITERATIONS
#		else
		16
#		endif
		;  // Adjust based on performance/quality tradeoff

	float ssrScale = 4.0;

	for (; iterationIndex < maxIterations; iterationIndex++) {
		float3 iterationUvDepthDR = ViewToUVDepthHelper(lerp(csStart.xyz, csFinish.xyz, (iterationIndex / (float)maxIterations) * SSRParams.x * ssrScale * rcp(length(deltaUvDepth.xy))), eyeIndex);
		float3 iterationUvDepthSampleDR =
#		ifdef VR
			// Apply dynamic resolution adjustments and stereo UV conversions
			FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(
				Stereo::ConvertStereoRayMarchUV(
					FrameBuffer::GetDynamicResolutionUnadjustedScreenPosition(iterationUvDepthDR),
					eyeIndex,
					fromOtherEye));
#		else
			// No VR adjustments, just use the raw UV coordinates
			iterationUvDepthDR;
#		endif
		float iterationDepth = DepthTex.SampleLevel(DepthSampler, iterationUvDepthSampleDR.xy, 0).x;
		uvDepthPreResultDR = uvDepthResultDR;
		uvDepthResultDR = iterationUvDepthDR;
		if (FrameBuffer::isOutsideFrame(iterationUvDepthDR.xy, true)
#		ifdef VR
				// In VR, it could be coming from the other eye
				&& !fromOtherEye ||
			(fromOtherEye && FrameBuffer::isOutsideFrame(Stereo::ConvertMonoUVToOtherEye(iterationUvDepthDR, eyeIndex, true).xy, true))
#		endif
		) {
			// out of screen, no ray ssr possible
			return psout;
		}
		if (iterationDepth < iterationUvDepthDR.z) {  // ray intersection detected
			break;
		}
	}

#		ifdef DEBUG_SSR_RAYMARCH_ITERATIONS
	// Visualize the number of raymarch iterations used for each pixel. This is for the initial
	// attempt to find the intersection using linear search.
	// Blue = 0, Red = max, should move through purple
	float iterationColor = float(iterationIndex) / float(maxIterations);
	psout.Color = float4(iterationColor, 0, 1 - iterationColor, 1);
	return psout;
#		endif

#		ifdef DEBUG_SSR_RAYMARCH_FIRST_HIT
	// Visualize the position of the first hit for each pixel during raymarching.
	// This shows where the initial intersection was found in the linear search.
	// Red intensity represents the depth of the hit:
	// Dark red = hit close to the camera (small number of iterations)
	// Bright red = hit far from the camera (large number of iterations)
	// Black = no hit found within max iterations
	float hitDepth = (float)iterationIndex / (float)maxIterations;
	psout.Color = float4(hitDepth, 0, 0, 1);
	return psout;
#		endif

#		ifdef DEBUG_SSR_RAYMARCH_DETAILED
	// Visualize the start and end positions of the reflection ray
	// Red channel: start position X
	// Green channel: start position Y
	// Blue channel: end position X
	// Alpha channel: end position Y

	// Color interpretation:
	// - Start position dominant (yellow/greenish-yellow): shorter ray or less travel
	// - End position dominant (blue/purple): longer ray or more travel
	// - Equal mix (brown/gray): medium-length ray
	// - Brighter colors: stronger reflection or more direct hit
	// - Darker colors: weaker reflection or more glancing hit

	psout.Color = float4(uvDepthStartDR.xy, uvDepthFinishDR.xy);
	return psout;
#		endif

	// Handling the final result
	float3 uvDepthFinalDR = uvDepthResultDR;

	if (iterationIndex < maxIterations) {
		// refine the hit by searching between the start and hit boundary
		iterationIndex = 0;
		uvDepthFinalDR = uvDepthPreResultDR;
		for (; iterationIndex < maxIterations; iterationIndex++) {
			uvDepthFinalDR = (uvDepthPreResultDR + uvDepthResultDR) * 0.5;
			float3 uvDepthFinalSampleDR =
#		ifdef VR
				// Apply dynamic resolution adjustments and stereo UV conversions
				FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(
					Stereo::ConvertStereoRayMarchUV(
						FrameBuffer::GetDynamicResolutionUnadjustedScreenPosition(uvDepthFinalDR),
						eyeIndex,
						fromOtherEye));
#		else
				// No VR adjustments, just use the raw UV coordinates
				uvDepthFinalDR;
#		endif
			float subIterationDepth = DepthTex.SampleLevel(DepthSampler, uvDepthFinalSampleDR.xy, 0).x;
			if (subIterationDepth < uvDepthFinalDR.z && uvDepthFinalDR.z < subIterationDepth + SSRParams.y) {
				break;
			}
			if (subIterationDepth < uvDepthFinalDR.z) {
				// If intersection is closer, move towards uvDepthPreResultDR (lower half)
				uvDepthResultDR = uvDepthFinalDR;
			} else {
				// Otherwise, move towards uvDepthResultDR (uppser half)
				uvDepthPreResultDR = uvDepthFinalDR;
			}
		}
	}

	float2 uvFinal = FrameBuffer::GetDynamicResolutionUnadjustedScreenPosition(uvDepthFinalDR.xy);
	uvFinal = Stereo::ConvertToStereoUV(uvFinal, eyeIndex);
	float2 previousUvFinalDR = FrameBuffer::GetPreviousDynamicResolutionAdjustedScreenPosition(uvFinal);
	float3 alpha = AlphaTex.Sample(AlphaSampler, previousUvFinalDR).xyz;

	float2 uvFinalDR = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(uvFinal);

#		ifdef DEBUG_SSR_UV
	// This helps identify whether the UV coordinates are being properly transformed, and whether they
	// are behaving as expected across the screen when the camera pitch changes.
	// When you run this, you should see a gradient across the screen, with colors changing smoothly from
	// one corner of the screen to the other.
	// Look for areas where the UVs become incorrect or discontinuous, especially as you tilt the camera
	// downwards. If the UVs start to distort or shift, this can explain why reflections are moving in
	// unexpected ways.
	psout.Color = float4(uvFinalDR, 0, 1);
	return psout;
#		endif

#		ifdef DEBUG_SSR_DEPTH
	// Sample depth at the current UV and return it as a grayscale value
	// Helps determine if the depth values are sampled correctly at each UV position.
	// You should see smooth gradients of depth across the screen. If the depth values
	// suddenly shift, it could explain why reflections are appearing in the wrong places.
	float depth = DepthTex.Sample(DepthSampler, uvFinalDR).x;

	// Output the depth as a grayscale color (depth values are expected to be between 0 and 1)
	psout.Color = float4(depth, depth, depth, 1);
	return psout;
#		endif

#		ifdef DEBUG_SSR_REFINE_ITERATIONS
	// Visualize the number of iterations used for each pixel in the refinement step
	// This is the second for loop using binary search
	// Blue = 0, Red = max, should move through purple
	float iterationColor = float(iterationIndex) / float(maxIterations);
	psout.Color = float4(iterationColor, 0, 1 - iterationColor, 1);
	return psout;
#		endif

#		ifdef DEBUG_SSR_REFINE_FIRST_HIT
	// Visualize the position of the first hit for each pixel during refinement.
	// This shows where the initial intersection was found in the binary search.
	// Red intensity represents the depth of the hit:
	// Dark red = hit close to the camera (small number of iterations)
	// Bright red = hit far from the camera (large number of iterations)
	// Black = no hit found within max iterations
	float hitDepth = (float)iterationIndex / (float)maxIterations;
	psout.Color = float4(hitDepth, 0, 0, 1);
	return psout;
#		endif

	float3 color = ColorTex.Sample(ColorSampler, uvFinalDR).xyz;

#		ifdef VR
	const bool useAlpha = false;
	// Because alpha is based on the prior frame, there will be a lag for showing clouds.
	// This is very obvious in VR. Hide clouds for now.
	alpha = useAlpha ? alpha : float3(0, 0, 0);

	// for fade calculation from eye center, need to adjust to monoUV
	uvFinal = Stereo::ConvertFromStereoUV(uvFinal, eyeIndex);
	uvStart = Stereo::ConvertFromStereoUV(uvStart, eyeIndex);
#		endif

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

	[branch] if (iterationIndex == maxIterations)
	{
		return psout;
	}
	else
	{
		psout.Color.rgb = ssrColor;
	}

	// Fade Calculations

	// SSR Marching Radius Fade Factor (based on ray distance)
	float2 deltaUv = uvFinal - uvStart;
	float ssrMarchingRadiusFadeFactor = 1 - pow(length(deltaUv) * SSRParams.w, 3);

	// Screen Center Distance Fade Factor
	float2 uvResultScreenCenterOffset = uvFinal - 0.5;
	float centerDistance = min(1, 2 * length(uvResultScreenCenterOffset));

#		ifdef VR
	// Make VR fades consistent by taking the closer of the two eyes
	// Based on concepts from https://cuteloong.github.io/publications/scssr24/
	float2 otherEyeUvResultScreenCenterOffset = Stereo::ConvertMonoUVToOtherEye(FrameBuffer::GetDynamicResolutionUnadjustedScreenPosition(uvDepthFinalDR), eyeIndex).xy - 0.5;
	centerDistance = min(centerDistance, 2 * length(otherEyeUvResultScreenCenterOffset));
#		endif

	float centerDistanceFadeFactor = 1 - pow(centerDistance, 5);

	// Final alpha calculation
	psout.Color.a = ssrMarchingRadiusFadeFactor * centerDistanceFadeFactor;
#	endif

	return psout;
}
#endif
