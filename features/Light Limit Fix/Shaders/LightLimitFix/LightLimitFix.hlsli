struct LightGrid
{
	uint offset;
	uint lightCount;
	float pad0[2];
};

struct StructuredLight
{
	float3 color;
	float radius;
	float4 positionWS[2];
	float4 positionVS[2];
};

struct StrictLightData
{
	StructuredLight StrictLights[15];
	uint NumStrictLights;
	float LightsNear;
	float LightsFar;
	uint pad0;
};

StructuredBuffer<StructuredLight> lights : register(t50);
StructuredBuffer<uint> lightList : register(t51);       //MAX_CLUSTER_LIGHTS * 16^3
StructuredBuffer<LightGrid> lightGrid : register(t52);  //16^3
StructuredBuffer<StrictLightData> strictLights : register(t53);

namespace LightLimitFix
{
	bool GetClusterIndex(in float2 uv, in float z, out uint clusterIndex)
	{
		const uint3 clusterSize = lightLimitFixSettings.ClusterSize.xyz;

		if (z < strictLights[0].LightsNear || z > strictLights[0].LightsFar)
			return false;

		float clampedZ = clamp(z, strictLights[0].LightsNear, strictLights[0].LightsFar);
		uint clusterZ = uint(max((log2(z) - log2(strictLights[0].LightsNear)) * clusterSize.z / log2(strictLights[0].LightsFar / strictLights[0].LightsNear), 0.0));
		uint3 cluster = uint3(uint2(uv * clusterSize.xy), clusterZ);

		clusterIndex = cluster.x + (clusterSize.x * cluster.y) + (clusterSize.x * clusterSize.y * cluster.z);
		return true;
	}

	bool IsSaturated(float value)
	{
		return value == saturate(value);
	}

	bool IsSaturated(float2 value)
	{
		return IsSaturated(value.x) && IsSaturated(value.y);
	}

	float ContactShadows(float3 viewPosition, float noise2D, float3 lightDirectionVS, uint contactShadowSteps, uint a_eyeIndex = 0)
	{
		if (contactShadowSteps == 0)
			return 1.0;

		float2 depthDeltaMult = float2(1.0, 0.05);

		// Extend contact shadow distance
		lightDirectionVS *= 2.0;

		// Offset starting position with interleaved gradient noise
		viewPosition += lightDirectionVS * noise2D;

		// Accumulate samples
		float contactShadow = 0.0;
		for (uint i = 0; i < contactShadowSteps; i++) {
			// Step the ray
			viewPosition += lightDirectionVS;

			float2 rayUV = ViewToUV(viewPosition, true, a_eyeIndex);

			// Ensure the UV coordinates are inside the screen
			if (!IsSaturated(rayUV))
				break;

			// Compute the difference between the ray's and the camera's depth
			float rayDepth = GetScreenDepth(rayUV, a_eyeIndex);

			// Difference between the current ray distance and the marched light
			float depthDelta = viewPosition.z - rayDepth;
			if (rayDepth > 16.5)  // First person
				contactShadow += saturate(depthDelta * depthDeltaMult.x) - saturate(depthDelta * depthDeltaMult.y);
		}

		return 1.0 - saturate(contactShadow * 0.5);
	}

	// Copyright 2019 Google LLC.
	// SPDX-License-Identifier: Apache-2.0

	// Polynomial approximation in GLSL for the Turbo colormap
	// Original LUT: https://gist.github.com/mikhailov-work/ee72ba4191942acecc03fe6da94fc73f

	// Authors:
	//   Colormap Design: Anton Mikhailov (mikhailov@google.com)
	//   GLSL Approximation: Ruofei Du (ruofei@google.com)

	// See also: https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html

	float3 TurboColormap(float x)
	{
		const float4 kRedVec4 = float4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
		const float4 kGreenVec4 = float4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
		const float4 kBlueVec4 = float4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
		const float2 kRedVec2 = float2(-152.94239396, 59.28637943);
		const float2 kGreenVec2 = float2(4.27729857, 2.82956604);
		const float2 kBlueVec2 = float2(-89.90310912, 27.34824973);

		x = saturate(x);
		float4 v4 = float4(1.0, x, x * x, x * x * x);
		float2 v2 = v4.zw * v4.z;
		return float3(
			dot(v4, kRedVec4) + dot(v2, kRedVec2),
			dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
			dot(v4, kBlueVec4) + dot(v2, kBlueVec2));
	}
}
