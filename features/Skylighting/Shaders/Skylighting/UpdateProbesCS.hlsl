#include "Common/Math.hlsli"
#include "Skylighting/Skylighting.hlsli"

Texture2D<unorm float> srcOcclusionDepth : register(t0);

RWTexture3D<sh2> outProbeArray : register(u0);
RWTexture3D<uint> outAccumFramesArray : register(u1);

SamplerState samplerPointClamp : register(s0);

[numthreads(8, 8, 1)] void main(uint3 dtid
								: SV_DispatchThreadID) {
	const float fadeInThreshold = 255;
	const static sh2 unitSH = float4(sqrt(4.0 * Math::PI), 0, 0, 0);
	const SkylightingSettings settings = skylightingSettings;

	uint3 cellID = (int3(dtid) - settings.ArrayOrigin.xyz) % Skylighting::ARRAY_DIM;
	bool isValid = all(cellID >= max(0, settings.ValidMargin.xyz)) && all(cellID <= Skylighting::ARRAY_DIM - 1 + min(0, settings.ValidMargin.xyz));  // check if the cell is newly added

	float3 cellCentreMS = cellID + 0.5 - Skylighting::ARRAY_DIM / 2;
	cellCentreMS = cellCentreMS / Skylighting::ARRAY_DIM * Skylighting::ARRAY_SIZE + settings.PosOffset.xyz;

	float3 cellCentreOS = mul(settings.OcclusionViewProj, float4(cellCentreMS, 1)).xyz;
	cellCentreOS.y = -cellCentreOS.y;
	float2 occlusionUV = cellCentreOS.xy * 0.5 + 0.5;

	if (all(occlusionUV > 0) && all(occlusionUV < 1)) {
		uint accumFrames = isValid ? (outAccumFramesArray[dtid] + 1) : 1;
		if (accumFrames < fadeInThreshold) {
			float occlusionDepth = srcOcclusionDepth.SampleLevel(samplerPointClamp, occlusionUV, 0);
			float visibility = saturate((occlusionDepth + 0.0005 - cellCentreOS.z) * 1024);

			sh2 occlusionSH = shScale(shEvaluate(settings.OcclusionDir.xyz), visibility * 4.0 * Math::PI);  // 4 pi from monte carlo
			if (isValid) {
				float lerpFactor = rcp(accumFrames);
				sh2 prevProbeSH = unitSH;
				if (accumFrames > 1)
					prevProbeSH += (outProbeArray[dtid] - unitSH) * fadeInThreshold / min(fadeInThreshold, accumFrames - 1);  // inverse confidence
				occlusionSH = shAdd(shScale(prevProbeSH, 1 - lerpFactor), shScale(occlusionSH, lerpFactor));
			}
			occlusionSH = lerp(unitSH, occlusionSH, min(fadeInThreshold, accumFrames) / fadeInThreshold);  // confidence fade in

			outProbeArray[dtid] = occlusionSH;
			outAccumFramesArray[dtid] = accumFrames;
		}
	} else if (!isValid) {
		outProbeArray[dtid] = unitSH;
		outAccumFramesArray[dtid] = 0;
	}
}