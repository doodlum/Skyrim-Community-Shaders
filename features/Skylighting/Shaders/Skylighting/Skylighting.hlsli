// Define SL_INCL_STRUCT and SL_INCL_METHODS to include different parts
// Because this file is included by both forward and deferred shaders

#ifdef SL_INCL_STRUCT
struct SkylightingSettings
{
	row_major float4x4 OcclusionViewProj;
	float4 OcclusionDir;

	float3 PosOffset;  // xyz: cell origin in camera model space
	uint pad0;
	uint3 ArrayOrigin;  // xyz: array origin, w: max accum frames
	uint pad1;
	int4 ValidMargin;

	float4 MixParams;  // x: min diffuse visibility, y: diffuse mult, z: min specular visibility, w: specular mult

	uint DirectionalDiffuse;
	uint3 pad2;
};

#endif

#ifdef SL_INCL_METHODS

#	include "Common/Random.hlsli"
#	include "Common/Spherical Harmonics/SphericalHarmonics.hlsli"

#	ifdef PSHADER
Texture3D<sh2> SkylightingProbeArray : register(t29);
#	endif

namespace Skylighting
{
	const static uint3 ARRAY_DIM = uint3(128, 128, 64);
	const static float3 ARRAY_SIZE = float3(10000, 10000, 10000 * 0.5);
	const static float3 CELL_SIZE = ARRAY_SIZE / ARRAY_DIM;

	float getFadeOutFactor(float3 positionMS)
	{
		float3 uvw = saturate(positionMS / ARRAY_SIZE + .5);
		float3 dists = min(uvw, 1 - uvw);
		float edgeDist = min(dists.x, min(dists.y, dists.z));
		return saturate(edgeDist * 20);
	}

	float mixDiffuse(SkylightingSettings params, float visibility)
	{
		return lerp(params.MixParams.x, 1, pow(saturate(visibility), params.MixParams.y));
	}

	float mixSpecular(SkylightingSettings params, float visibility)
	{
		return lerp(params.MixParams.z, 1, pow(saturate(visibility), params.MixParams.w));
	}

	sh2 sample(SkylightingSettings params, Texture3D<sh2> probeArray, float3 positionMS, float3 normalWS)
	{
		const static sh2 unitSH = float4(sqrt(4 * shPI), 0, 0, 0);
		sh2 scaledUnitSH = unitSH / (params.MixParams.y + 1e-10);

		float3 positionMSAdjusted = positionMS - params.PosOffset;
		float3 uvw = positionMSAdjusted / ARRAY_SIZE + .5;

		if (any(uvw < 0) || any(uvw > 1))
			return scaledUnitSH;

		float3 cellVxCoord = uvw * ARRAY_DIM;
		int3 cell000 = floor(cellVxCoord - 0.5);
		float3 trilinearPos = cellVxCoord - 0.5 - cell000;

		sh2 sum = 0;
		float wsum = 0;
		[unroll] for (int i = 0; i < 2; i++)
			[unroll] for (int j = 0; j < 2; j++)
				[unroll] for (int k = 0; k < 2; k++)
		{
			int3 offset = int3(i, j, k);
			int3 cellID = cell000 + offset;

			if (any(cellID < 0) || any(cellID >= ARRAY_DIM))
				continue;

			float3 cellCentreMS = cellID + 0.5 - ARRAY_DIM / 2;
			cellCentreMS = cellCentreMS * CELL_SIZE;

			// https://handmade.network/p/75/monter/blog/p/7288-engine_work__global_illumination_with_irradiance_probes
			// basic tangent checks
			float tangentWeight = dot(normalize(cellCentreMS - positionMSAdjusted), normalWS);
			if (tangentWeight <= 0.0)
				continue;
			tangentWeight = sqrt(tangentWeight);

			float3 trilinearWeights = 1 - abs(offset - trilinearPos);
			float w = trilinearWeights.x * trilinearWeights.y * trilinearWeights.z * tangentWeight;

			uint3 cellTexID = (cellID + params.ArrayOrigin.xyz) % ARRAY_DIM;
			sh2 probe = shScale(probeArray[cellTexID], w);

			sum = shAdd(sum, probe);
			wsum += w;
		}

		sh2 result = shScale(sum, rcp(wsum + 1e-10));

		// fade out
		result = lerp(scaledUnitSH, result, getFadeOutFactor(positionMS));

		return result;
	}

	float getVL(SkylightingSettings params, Texture3D<sh2> probeArray, float3 startPosWS, float3 endPosWS, float2 pxCoord)
	{
		const static uint nSteps = 16;
		const static float step = 1.0 / float(nSteps);

		float3 worldDir = endPosWS - startPosWS;
		float3 worldDirNormalised = normalize(worldDir);

		float noise = InterleavedGradientNoise(pxCoord, FrameCount);

		float vl = 0;

		for (uint i = 0; i < nSteps; ++i) {
			float t = saturate(i * step);

			float shadow = 0;
			{
				float3 samplePositionWS = startPosWS + worldDir * t;

				sh2 skylighting = Skylighting::sample(params, probeArray, samplePositionWS, float3(0, 0, 1));

				shadow += Skylighting::mixDiffuse(params, shUnproject(skylighting, worldDirNormalised));
			}
			vl += shadow;
		}
		return vl * step;
	}

	sh2 fauxSpecularLobeSH(float3 N, float3 V, float roughness)
	{
		// https://www.gdcvault.com/play/1026701/Fast-Denoising-With-Self-Stabilizing
		// get dominant ggx reflection direction
		float f = (1 - roughness) * (sqrt(1 - roughness) + roughness);
		float3 R = reflect(-V, N);
		float3 D = lerp(N, R, f);
		float3 dominantDir = normalize(D);

		// lobe half angle
		// credit: Olivier Therrien
		float roughness2 = roughness * roughness;
		float halfAngle = clamp(4.1679 * roughness2 * roughness2 - 9.0127 * roughness2 * roughness + 4.6161 * roughness2 + 1.7048 * roughness + 0.1, 0, 1.57079632679);
		float lerpFactor = halfAngle / 1.57079632679;
		sh2 directional = shEvaluate(dominantDir);
		sh2 cosineLobe = shEvaluateCosineLobe(dominantDir) / shPI;
		sh2 result = shAdd(shScale(directional, lerpFactor), shScale(cosineLobe, 1 - lerpFactor));

		return result;
	}
}

#endif