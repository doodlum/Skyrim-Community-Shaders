///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2021, Intel Corporation
//
// SPDX-License-Identifier: MIT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// XeGTAO is based on GTAO/GTSO "Jimenez et al. / Practical Real-Time Strategies for Accurate Indirect Occlusion",
// https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
//
// Implementation:  Filip Strugar (filip.strugar@intel.com), Steve Mccalla <stephen.mccalla@intel.com>         (\_/)
// Version:         (see XeGTAO.h)                                                                            (='.'=)
// Details:         https://github.com/GameTechDev/XeGTAO                                                     (")_(")
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// with additional edits by FiveLimbedCat/ProfJack
//
// More references:
//
// Screen Space Indirect Lighting with Visibility Bitmask
//  https://arxiv.org/abs/2301.11376
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../Common/FastMath.hlsli"
#include "../Common/FrameBuffer.hlsl"
#include "../Common/GBuffer.hlsli"
#include "../Common/VR.hlsli"
#include "common.hlsli"

#define FP_Z (16.5)

#define PI (3.1415926535897932384626433832795)
#define HALF_PI (1.5707963267948966192313216916398)
#define RCP_PI (0.31830988618)

Texture2D<float> srcWorkingDepth : register(t0);
Texture2D<float4> srcNormal : register(t1);
Texture2D<float3> srcRadiance : register(t2);
Texture2D<unorm float2> srcNoise : register(t3);
Texture2D<unorm float> srcAccumFrames : register(t4);
Texture2D<float4> srcPrevGI : register(t5);

RWTexture2D<float4> outGI : register(u0);
RWTexture2D<unorm float2> outBentNormal : register(u1);
RWTexture2D<half3> outPrevGeo : register(u2);

float GetDepthFade(float depth)
{
	return saturate((depth - DepthFadeRange.x) * DepthFadeScaleConst);
}

// Engine-specific screen & temporal noise loader
float2 SpatioTemporalNoise(uint2 pixCoord, uint temporalIndex)  // without TAA, temporalIndex is always 0
{
	// noise texture from https://github.com/electronicarts/fastnoise
	// 128x128x64
	uint2 noiseCoord = (pixCoord % 128) + uint2(0, (temporalIndex % 64) * 128);
	return srcNoise.Load(uint3(noiseCoord, 0));
}

// HBIL pp.29
float IlIntegral(float2 integral_factor, float angle_prev, float angle_new)
{
	float sin_prev, cos_prev, sin_new, cos_new;
	sincos(angle_prev, sin_prev, cos_prev);
	sincos(angle_new, sin_new, cos_new);

	float delta_angle = angle_new - angle_prev;
	return max(0, integral_factor.x * (delta_angle + sin_prev * cos_prev - sin_new * cos_new) + integral_factor.y * (cos_prev * cos_prev - cos_new * cos_new));
}

void CalculateGI(
	uint2 dtid, float2 uv, float viewspaceZ, float3 viewspaceNormal,
	out float4 o_currGIAO, out float3 o_bentNormal)
{
	const float2 frameScale = FrameDim * RcpTexDim;

	uint eyeIndex = GetEyeIndexFromTexCoord(uv);
	float2 normalizedScreenPos = ConvertFromStereoUV(uv, eyeIndex);

	const float rcpNumSlices = rcp(NumSlices);
	const float rcpNumSteps = rcp(NumSteps);

	const float falloffRange = EffectFalloffRange * EffectRadius;
	const float rcpFalloffRange = rcp(falloffRange);
	const float falloffFrom = EffectRadius * (1 - EffectFalloffRange);
	const float falloffMul = -rcpFalloffRange;
	const float falloffAdd = falloffFrom * rcpFalloffRange + 1.0;

	// quality settings / tweaks / hacks
	// if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance
	const float pixelTooCloseThreshold = 1.3;
	// approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( uv.xy + ViewportSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
	const float2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * (eyeIndex == 0 ? NDCToViewMul.xy : NDCToViewMul.zw) * RcpFrameDim;

	float screenspaceRadius = EffectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;
	// this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
	const float minS = pixelTooCloseThreshold / screenspaceRadius;

	//////////////////////////////////////////////////////////////////

	const float2 localNoise = SpatioTemporalNoise(dtid, FrameIndex);
	const float noiseSlice = localNoise.x;
	const float noiseStep = localNoise.y;

	//////////////////////////////////////////////////////////////////

	const float3 pixCenterPos = ScreenToViewPosition(normalizedScreenPos, viewspaceZ, eyeIndex);
	const float3 viewVec = normalize(-pixCenterPos);

	float visibility = 0;
	float3 radiance = 0;
	float3 bentNormal = viewspaceNormal;

	for (uint slice = 0; slice < NumSlices; slice++) {
		float phi = (PI * rcpNumSlices) * (slice + noiseSlice);
		float3 directionVec = 0;
		sincos(phi, directionVec.y, directionVec.x);

		// convert to px units for later use
		float2 omega = float2(directionVec.x, -directionVec.y) * screenspaceRadius;

		const float3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);
		const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

		float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);
		float projectedNormalVecLength = length(projectedNormalVec);
		float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));
		float cosNorm = saturate(dot(projectedNormalVec, viewVec) / projectedNormalVecLength);

		float n = signNorm * ACos(cosNorm);

#ifdef BITMASK
		uint bitmask = 0;
#	ifdef GI
		uint bitmaskGI = 0;
#	endif
#else
		// this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
		float2 sincos_n;
		sincos(n, sincos_n.x, sincos_n.y);
		float lowHorizonCos1 = sincos_n.x;
		const float lowHorizonCos0 = -lowHorizonCos1;

		float horizonCos0 = lowHorizonCos0;  //-1;
		float horizonCos1 = lowHorizonCos1;  //-1;

		float3 sampleRadiance = 0;
#endif  // BITMASK

		// R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
		float stepNoise = frac(noiseStep + slice * 0.6180339887498948482);

		[unroll] for (int sideSign = -1; sideSign <= 1; sideSign += 2)
		{
			[loop] for (uint step = 0; step < NumSteps; step++)
			{
				float s = (step + stepNoise) * rcpNumSteps;
				s *= s;     // default 2 is fine
				s += minS;  // avoid sampling center pixel

				float2 sampleOffset = s * omega;

				float2 samplePxCoord = dtid + .5 + sampleOffset * sideSign;
				float2 sampleUV = samplePxCoord * RcpFrameDim;
				float2 sampleScreenPos = ConvertFromStereoUV(sampleUV, eyeIndex);
				[branch] if (any(sampleScreenPos > 1.0) || any(sampleScreenPos < 0.0)) break;

				float sampleOffsetLength = length(sampleOffset);
				float mipLevel = clamp(log2(sampleOffsetLength) - DepthMIPSamplingOffset, 0, 5);

				float SZ = srcWorkingDepth.SampleLevel(samplerPointClamp, sampleUV * frameScale, mipLevel);
				[branch] if (SZ > DepthFadeRange.y || SZ < FP_Z) continue;

				float3 samplePos = ScreenToViewPosition(sampleScreenPos, SZ, eyeIndex);
				float3 sampleDelta = samplePos - float3(pixCenterPos);
				float3 sampleHorizonVec = normalize(sampleDelta);

#ifdef BITMASK
				float3 sampleBackPos = samplePos - viewVec * Thickness;
				float3 sampleBackHorizonVec = normalize(sampleBackPos - pixCenterPos);

				float angleFront = FastACos(dot(sampleHorizonVec, viewVec));  // either clamp or use float version for whatever reason
				float angleBack = FastACos(dot(sampleBackHorizonVec, viewVec));
				float2 angleRange = -sideSign * (sideSign == -1 ? float2(angleFront, angleBack) : float2(angleBack, angleFront));
				angleRange = smoothstep(0, 1, (angleRange + n) * RCP_PI + .5);  // https://discord.com/channels/586242553746030596/586245736413528082/1102228968247144570

				uint2 bitsRange = uint2(round(angleRange.x * 32u), round((angleRange.y - angleRange.x) * 32u));
				uint maskedBits = ((1 << bitsRange.y) - 1) << bitsRange.x;
#else
				// this is our own thickness heuristic that relies on sooner discarding samples behind the center
				float falloffBase = length(float3(sampleDelta) * float3(1, 1, 1 + ThinOccluderCompensation));
				float weight = saturate(falloffBase * falloffMul + falloffAdd);

				// sample horizon cos
				float shc = dot(sampleHorizonVec, viewVec);

				// discard unwanted samples
				shc = lerp(sideSign == -1 ? lowHorizonCos1 : lowHorizonCos0, shc, weight);
				float horizonCos = sideSign == -1 ? horizonCos1 : horizonCos0;
#endif

#ifdef GI
#	ifdef BITMASK
				float3 sampleBackPosGI = samplePos - viewVec * 500;
				float3 sampleBackHorizonVecGI = normalize(sampleBackPosGI - pixCenterPos);
				float angleBackGI = FastACos(dot(sampleBackHorizonVecGI, viewVec));
				float2 angleRangeGI = -sideSign * (sideSign == -1 ? float2(angleFront, angleBackGI) : float2(angleBackGI, angleFront));
				angleRangeGI = smoothstep(0, 1, (angleRangeGI + n) * RCP_PI + .5);  // https://discord.com/channels/586242553746030596/586245736413528082/1102228968247144570

				uint2 bitsRangeGI = uint2(round(angleRangeGI.x * 32u), round((angleRangeGI.y - angleRangeGI.x) * 32u));
				uint maskedBitsGI = ((1 << bitsRangeGI.y) - 1) << bitsRangeGI.x;
				bool checkGI = maskedBitsGI;
#	else
				bool checkGI = shc > horizonCos;
#	endif

				if (checkGI) {
					float giBoost = 1 + GIDistanceCompensation * smoothstep(0, GICompensationMaxDist, s * EffectRadius);

					// IL
					float frontBackMult = 1.f;
#	ifdef BACKFACE
					if (dot(DecodeNormal(srcNormal.SampleLevel(samplerPointClamp, sampleUV * frameScale, 0).xy), sampleHorizonVec) > 0)  // backface
						frontBackMult = BackfaceStrength;
#	endif

#	ifdef BITMASK
					if (frontBackMult > 0.f) {
						float3 sampleRadiance = srcRadiance.SampleLevel(samplerPointClamp, sampleUV * frameScale, mipLevel).rgb * frontBackMult * giBoost;

						sampleRadiance *= countbits(maskedBitsGI & ~bitmaskGI) * 0.03125;  // 1/32
						sampleRadiance *= dot(viewspaceNormal, sampleHorizonVec);
						sampleRadiance = max(0, sampleRadiance);

						radiance += sampleRadiance;
					}
#	else
					if (frontBackMult > 0.f) {
						float3 newSampleRadiance = srcRadiance.SampleLevel(samplerPointClamp, sampleUV * frameScale, mipLevel).rgb * frontBackMult * giBoost;

						float anglePrev = n + sideSign * HALF_PI - FastACos(horizonCos);  // float version is closest acos
						float angleCurr = n + sideSign * HALF_PI - FastACos(shc);
						float2 integralFactor = 0.5 * float2(dot(directionVec.xy, viewspaceNormal.xy) * sideSign, viewspaceNormal.z);
						newSampleRadiance *= IlIntegral(integralFactor, anglePrev, angleCurr);

						// depth filtering. HBIL pp.38
						float t = smoothstep(0, 1, dot(viewspaceNormal, sampleHorizonVec));
						sampleRadiance = lerp(sampleRadiance, newSampleRadiance, t);

						radiance += max(0, sampleRadiance);
					}
					horizonCos = shc;
#	endif
				}
#else
#	ifndef BITMASK
				// 					// thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
				// #if 0    // (disabled, not used) this should match the paper
				//                 float newhorizonCos = max( horizonCos, shc );

				//                 horizonCos = (horizonCos > shc)? lerp( newhorizonCos, shc, ThinOccluderCompensation ) :newhorizonCos ;
				// #elif 0  // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
				// 				horizonCos = lerp(max(horizonCos, shc), shc, ThinOccluderCompensation);
				// #else  // this is a version where thicknessHeuristic is completely disabled
				horizonCos = max(horizonCos, shc);
// #endif
#	endif
#endif  // GI

#ifdef BITMASK
				bitmask |= maskedBits;
#	ifdef GI
				bitmaskGI |= maskedBitsGI;
#	endif
#else
				if (sideSign == -1)
					horizonCos1 = horizonCos;
				else
					horizonCos0 = horizonCos;
#endif
			}
		}

#ifdef BITMASK
		visibility += 1.0 - countbits(bitmask) * 0.03125;

		// TODO: bent normal for bitmask?
#else
#	if 1  // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
		projectedNormalVecLength = lerp(projectedNormalVecLength, 1, 0.05);
#	endif

		// line ~27, unrolled
		float h0 = -FastACos(horizonCos1);  // same, breaks stuff
		float h1 = FastACos(horizonCos0);
#	if 0  // we can skip clamping for a tiny little bit more performance
            h0 = n + clamp( h0-n, -HALF_PI, HALF_PI );
            h1 = n + clamp( h1-n, -HALF_PI, HALF_PI );
#	endif
		float iarc0 = (cosNorm + 2 * h0 * sincos_n.x - cos(2 * h0 - n));
		float iarc1 = (cosNorm + 2 * h1 * sincos_n.x - cos(2 * h1 - n));
		float localVisibility = projectedNormalVecLength * (iarc0 + iarc1) * .25;
		visibility += localVisibility;

#	ifdef BENT_NORMAL
		// see "Algorithm 2 Extension that computes bent normals b."
		float2 sincos_3h0mn, sincos_3h1mn, sincos_h0pn, sincos_h1pn;
		sincos(3 * h0 - n, sincos_3h0mn.x, sincos_3h0mn.y);
		sincos(3 * h1 - n, sincos_3h1mn.x, sincos_3h1mn.y);
		sincos(h0 + n, sincos_h0pn.x, sincos_h0pn.y);
		sincos(h1 + n, sincos_h1pn.x, sincos_h1pn.y);

		float t0 = (6 * sin(h0 - n) - sincos_3h0mn.x + 6 * sin(h1 - n) - sincos_3h1mn.x + 16 * sincos_n.x - 3 * (sincos_h0pn.x + sincos_h1pn.x)) * 0.08333333333;  // 1/12
		float t1 = (-sincos_3h0mn.y - sincos_3h1mn.y + 8 * sincos_n.y - 3 * (sincos_h0pn.y + sincos_h1pn.y)) * 0.08333333333;
		float3 localBentNormal = float3(directionVec.x * t0, directionVec.y * t0, -t1);
		localBentNormal = mul(RotFromToMatrix(float3(0, 0, -1), viewVec), localBentNormal) * projectedNormalVecLength;
		bentNormal += localBentNormal;
#	endif
#endif  // BITMASK
	}

	float depthFade = GetDepthFade(viewspaceZ);

	visibility *= rcpNumSlices;
	visibility = lerp(saturate(visibility), 1, depthFade);
	visibility = pow(abs(visibility), AOPower);

#ifdef GI
	radiance *= rcpNumSlices;
	radiance = lerp(radiance, 0, depthFade);
	radiance *= GIStrength;
#endif

#ifdef BENT_NORMAL
	bentNormal = normalize(bentNormal);
#endif

	o_currGIAO = float4(radiance, visibility);
	o_bentNormal = bentNormal;
}

[numthreads(8, 8, 1)] void main(const uint2 dtid
								: SV_DispatchThreadID) {
	const float2 frameScale = FrameDim * RcpTexDim;

	uint2 pxCoord = dtid;
#if defined(HALF_RATE)
	const uint halfWidth = uint(FrameDim.x) >> 1;
	const bool useHistory = dtid.x >= halfWidth;
	pxCoord.x = (pxCoord.x % halfWidth) * 2 + (dtid.y + FrameIndex + useHistory) % 2;
#endif

	float2 uv = (pxCoord + .5f) * RcpFrameDim;
	uint eyeIndex = GetEyeIndexFromTexCoord(uv);

	float viewspaceZ = srcWorkingDepth[pxCoord];

	float2 normalSample = srcNormal[pxCoord].xy;
	float3 viewspaceNormal = DecodeNormal(normalSample);

	half2 encodedWorldNormal = EncodeNormal(ViewToWorldVector(viewspaceNormal, CameraViewInverse[eyeIndex]));
	outPrevGeo[pxCoord] = half3(viewspaceZ, encodedWorldNormal);

// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
#if USE_HALF_FLOAT_PRECISION == 1
	viewspaceZ *= 0.99920h;  // this is good for FP16 depth buffer
#else
	viewspaceZ *= 0.99999;  // this is good for FP32 depth buffer
#endif

	float4 currGIAO = float4(0, 0, 0, 1);
	float3 bentNormal = viewspaceNormal;

	bool needGI = viewspaceZ > FP_Z && viewspaceZ < DepthFadeRange.y;
#if defined(HALF_RATE)
	needGI = needGI && !useHistory;
#endif
	[branch] if (needGI)
		CalculateGI(
			pxCoord, uv, viewspaceZ, viewspaceNormal,
			currGIAO, bentNormal);

#ifdef BENT_NORMAL
	outBentNormal[pxCoord] = EncodeNormal(bentNormal);
#endif

#ifdef TEMPORAL_DENOISER
	if (viewspaceZ < DepthFadeRange.y) {
		float lerpFactor = 0;
#	if defined(HALF_RATE)
		if (!useHistory)
#	endif
			lerpFactor = rcp(srcAccumFrames[pxCoord] * 255);

		float4 prevGIAO = srcPrevGI[pxCoord];
		currGIAO = lerp(prevGIAO, currGIAO, lerpFactor);
	}
#endif

	currGIAO = any(ISNAN(currGIAO)) ? float4(0, 0, 0, 1) : currGIAO;

	outGI[pxCoord] = currGIAO;
}