#include "Common.hlsl"
Texture2D<float> ShadowTexture : register(t1);

// https://www.shadertoy.com/view/Xt23zV
float smoothbumpstep(float edge0, float edge1, float x)
{
	x = 1.0 - abs(clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0) - .5) * 2.0;
	return x * x * (3.0 - x - x);
}

// Derived from the interleaved gradient function from Jimenez 2014 http://goo.gl/eomGso
float InterleavedGradientNoise(float2 uv)
{
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(uv, magic.xy)));
}

float ScreenSpaceShadowsUV(float2 texcoord, float3 lightDirectionVS, uint a_eyeIndex = 0)
{
	// Ignore the sky
	float startDepth = GetDepth(texcoord);
	if (startDepth >= 1)
		return 1;

	// Compute ray position in view-space
	float3 rayPos = InverseProjectUVZ(texcoord, startDepth, a_eyeIndex);

	// Blends effect variables between near, mid and far field
	float blendFactorFar = smoothstep(ShadowDistance / 3, ShadowDistance / 2, rayPos.z);
	float blendFactorMid = smoothbumpstep(0, ShadowDistance / 2, rayPos.z);

	// Max shadow length, longer shadows are less accurate
	float maxDistance = lerp(NearDistance, rayPos.z * FarDistanceScale, blendFactorFar);

	// Max ray steps, affects quality and performance
	uint maxSteps = max(1, (uint)((float)MaxSamples * (1 - blendFactorMid)));

	// How far to move each sample each step
	float stepLength = maxDistance / (float)maxSteps;

	// Compute ray step
	float3 rayStep = lightDirectionVS * stepLength;

	// Offset starting position with interleaved gradient noise
	float offset = InterleavedGradientNoise(texcoord * BufferDim);
	rayPos += rayStep * offset;

	float thickness = lerp(NearThickness, rayPos.z * FarThicknessScale, blendFactorFar);

	// Accumulate samples
	float shadow = 0.0f;
	uint samples = 0;

	float2 rayUV = 0.0f;
	for (uint i = 0; i < maxSteps; i++) {
		samples++;

		// Step the ray
		rayPos += rayStep;
		rayUV = ViewToUV(rayPos, true, a_eyeIndex);

		// Ensure the UV coordinates are inside the screen
		if (!IsSaturated(rayUV))
			break;

		// Compute the difference between the ray's and the camera's depth
		float rayDepth = InverseProjectUV(rayUV, a_eyeIndex).z;

		// Difference between the current ray distance and the marched light
		float depthDelta = rayPos.z - rayDepth;

		// Distant shadows simulate real shadows whereas near shadows are only intended for small objects
		float rayShadow = depthDelta / thickness;

		// Check if the depth difference is considered a shadow
		if (rayShadow > 0.0f && rayShadow <= 1.0f)
			shadow += rayShadow;
	}

	// Average samples
	shadow /= samples;

	// Intensity and sharpness of shadows
	shadow *= lerp(NearHardness, FarHardness, blendFactorFar);

	// Convert to visibility
	return 1 - saturate(shadow);
}

[numthreads(32, 32, 1)] void main(uint3 DTid
								  : SV_DispatchThreadID) {
	float2 TexCoord = (DTid.xy + 0.5) * RcpBufferDim * DynamicRes.zw;

#ifdef VR
	uint eyeIndex = (TexCoord.x >= 0.5) ? 1 : 0;
#else
	uint eyeIndex = 0;
#endif  // VR

	OcclusionRW[DTid.xy] = ScreenSpaceShadowsUV(TexCoord, InvDirLightDirectionVS[eyeIndex].xyz, eyeIndex);
}