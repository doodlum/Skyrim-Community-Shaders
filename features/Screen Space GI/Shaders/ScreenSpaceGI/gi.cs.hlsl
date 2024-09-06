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
#include "../Common/FrameBuffer.hlsli"
#include "../Common/GBuffer.hlsli"
#include "../Common/VR.hlsli"
#include "common.hlsli"

#define PI (3.1415926535897932384626433832795)
#define HALF_PI (1.5707963267948966192313216916398)
#define RCP_PI (0.31830988618)

Texture2D<float> srcWorkingDepth : register(t0);
Texture2D<float4> srcNormalRoughness : register(t1);
Texture2D<float3> srcRadiance : register(t2);  // maybe half-res
Texture2D<unorm float2> srcNoise : register(t3);
Texture2D<unorm float> srcAccumFrames : register(t4);  // maybe half-res
Texture2D<float4> srcPrevGI : register(t5);            // maybe half-res
Texture2D<float4> srcPrevGISpecular : register(t6);    // maybe half-res

RWTexture2D<float4> outGI : register(u0);
RWTexture2D<float4> outGISpecular : register(u1);
RWTexture2D<unorm float2> outBentNormal : register(u2);
RWTexture2D<half3> outPrevGeo : register(u3);

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
	out float4 o_currGIAO, out float4 o_currGIAOSpecular, out float3 o_bentNormal)
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
	const float2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * (eyeIndex == 0 ? NDCToViewMul.xy : NDCToViewMul.zw) * RCP_OUT_FRAME_DIM;

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
	const float NoV = abs(dot(viewVec, viewspaceNormal));

	float visibility = 0;
	float visibilitySpecular = 0;
	float3 radiance = 0;
	float3 radianceSpecular = 0;
	float3 bentNormal = viewspaceNormal;

#ifdef GI_SPECULAR
	const float roughness = max(0.2, saturate(1 - FULLRES_LOAD(srcNormalRoughness, dtid, uv * frameScale, samplerLinearClamp).z));  // can't handle low roughness
#endif

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
#		ifdef GI_SPECULAR
		uint bitmaskGISpecular = 0;
		float3 domVec = getSpecularDominantDirection(viewspaceNormal, viewVec, roughness);
		float3 projectedDomVec = normalize(domVec - axisVec * dot(domVec, axisVec));
		float nDom = sign(dot(orthoDirectionVec, projectedDomVec)) * ACos(saturate(dot(projectedDomVec, viewVec)));
#		endif
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
				float2 sampleUV = samplePxCoord * RCP_OUT_FRAME_DIM;
				float2 sampleScreenPos = ConvertFromStereoUV(sampleUV, eyeIndex);
				[branch] if (any(sampleScreenPos > 1.0) || any(sampleScreenPos < 0.0)) break;

				float sampleOffsetLength = length(sampleOffset);
				float mipLevel = clamp(log2(sampleOffsetLength) - DepthMIPSamplingOffset, 0, 5);
#ifdef HALF_RES
				mipLevel = max(mipLevel, 1);
#endif

				float SZ = srcWorkingDepth.SampleLevel(samplerPointClamp, sampleUV * frameScale, mipLevel);

				float3 samplePos = ScreenToViewPosition(sampleScreenPos, SZ, eyeIndex);
				float3 sampleDelta = samplePos - pixCenterPos;
				float3 sampleHorizonVec = normalize(sampleDelta);

#ifdef BITMASK
				float3 sampleBackPos = samplePos - viewVec * Thickness;
				float3 sampleBackHorizonVec = normalize(sampleBackPos - pixCenterPos);

				float angleFront = ACos(dot(sampleHorizonVec, viewVec));  // either clamp or use float version for whatever reason
				float angleBack = ACos(dot(sampleBackHorizonVec, viewVec));
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
				float3 sampleBackPosGI = samplePos - viewVec * 300;
				float3 sampleBackHorizonVecGI = normalize(sampleBackPosGI - pixCenterPos);
				float angleBackGI = ACos(dot(sampleBackHorizonVecGI, viewVec));
				float2 angleRangeGI = -sideSign * (sideSign == -1 ? float2(angleFront, angleBackGI) : float2(angleBackGI, angleFront));

#		ifdef GI_SPECULAR
				// thank u Olivier!
				float coneHalfAngles = max(5e-2, specularLobeHalfAngle(roughness));  // not too small
				float2 angleRangeSpecular = clamp((angleRangeGI + nDom) * 0.5 / coneHalfAngles, -1, 1) * 0.5 + 0.5;

				// Experimental method using importance sampling
				// https://agraphicsguynotes.com/posts/sample_microfacet_brdf/
				// float2 angleRangeSpecular = angleBackGI;
				// float2 specularSigns = sign(angleRangeSpecular);
				// angleRangeSpecular = saturate(cos(angleRangeSpecular)) * (roughness2 - 1);
				// angleRangeSpecular = roughness2 / (angleRangeSpecular * angleRangeSpecular + roughness2 - 1) - 1 / (roughness2 - 1);
				// angleRangeSpecular = saturate((angleRangeSpecular * specularSigns) * 0.5 + 0.5);

				uint2 bitsRangeGISpecular = uint2(round(angleRangeSpecular.x * 32u), round((angleRangeSpecular.y - angleRangeSpecular.x) * 32u));
				uint maskedBitsGISpecular = ((1 << bitsRangeGISpecular.y) - 1) << bitsRangeGISpecular.x;
#		endif

				angleRangeGI = smoothstep(0, 1, (angleRangeGI + n) * RCP_PI + .5);  // https://discord.com/channels/586242553746030596/586245736413528082/1102228968247144570

				uint2 bitsRangeGI = uint2(round(angleRangeGI.x * 32u), round((angleRangeGI.y - angleRangeGI.x) * 32u));
				uint maskedBitsGI = ((1 << bitsRangeGI.y) - 1) << bitsRangeGI.x;

				uint overlappedBits = maskedBitsGI & ~bitmaskGI;
				bool checkGI = overlappedBits;
#		ifdef GI_SPECULAR
				uint overlappedBitsSpecular = maskedBitsGISpecular & ~bitmaskGISpecular;
				checkGI = checkGI || overlappedBitsSpecular;
#		endif
#	else
				bool checkGI = shc > horizonCos;
#	endif

				if (checkGI) {
					float giBoost = 1 + GIDistanceCompensation * smoothstep(0, GICompensationMaxDist, s * EffectRadius);

					// IL
					float frontBackMult = 1.f;
					float3 normalSample = DecodeNormal(srcNormalRoughness.SampleLevel(samplerPointClamp, sampleUV * frameScale, 0).xy);
					if (dot(normalSample, sampleHorizonVec) > 0)  // backface
						frontBackMult = BackfaceStrength;

#	ifdef BITMASK
					if (frontBackMult > 0.f) {
						float3 sampleRadiance = srcRadiance.SampleLevel(samplerPointClamp, sampleUV * OUT_FRAME_SCALE, mipLevel).rgb * frontBackMult * giBoost;

						// float sourceMult = saturate(-dot(normalSample, sampleHorizonVec));
						float NoL = clamp(dot(viewspaceNormal, sampleHorizonVec), 1e-5, 1);

						float3 diffuseRadiance = sampleRadiance * countbits(overlappedBits) * 0.03125;  // 1/32
						diffuseRadiance *= NoL;
						diffuseRadiance = max(0, diffuseRadiance);

						radiance += diffuseRadiance;

#		ifdef GI_SPECULAR
						float3 specularRadiance = sampleRadiance * countbits(overlappedBitsSpecular) * 0.03125;  // 1/32
						specularRadiance *= NoL;
						specularRadiance = max(0, specularRadiance);

						radianceSpecular += specularRadiance;
#		endif
					}
#	else
					if (frontBackMult > 0.f) {
						float3 newSampleRadiance = srcRadiance.SampleLevel(samplerPointClamp, sampleUV * OUT_FRAME_SCALE, mipLevel).rgb * frontBackMult * giBoost;

						float anglePrev = n + sideSign * HALF_PI - ACos(horizonCos);
						float angleCurr = n + sideSign * HALF_PI - ACos(shc);
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
#		ifdef GI_SPECULAR
				bitmaskGISpecular |= maskedBitsGISpecular;
#		endif
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
		visibility += countbits(bitmask) * 0.03125;

#	if defined(GI) && defined(GI_SPECULAR)
		visibilitySpecular += countbits(bitmaskGISpecular) * 0.03125;
#	endif

		// TODO: bent normal for bitmask?
#else
#	if 1  // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
		projectedNormalVecLength = lerp(projectedNormalVecLength, 1, 0.05);
#	endif

		// line ~27, unrolled
		float h0 = -ACos(horizonCos1);  // same, breaks stuff
		float h1 = ACos(horizonCos0);
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
	visibility = lerp(saturate(visibility), 0, depthFade);
	visibility = 1 - pow(abs(1 - visibility), AOPower);

#ifdef GI
	radiance *= rcpNumSlices;
	radiance = lerp(radiance, 0, depthFade);
#	ifdef GI_SPECULAR
	radianceSpecular *= rcpNumSlices;
	radianceSpecular = lerp(radianceSpecular, 0, depthFade);

	visibilitySpecular *= rcpNumSlices;
	visibilitySpecular = lerp(saturate(visibility), 0, depthFade);
#	endif
#endif

#if !defined(GI) || !defined(GI_SPECULAR)
	visibilitySpecular = 0.0;
#endif

#ifdef BENT_NORMAL
	bentNormal = normalize(bentNormal);
#endif

	o_currGIAO = float4(radiance, visibility);
	o_currGIAOSpecular = float4(radianceSpecular, visibilitySpecular);
	o_bentNormal = bentNormal;
}

[numthreads(8, 8, 1)] void main(const uint2 dtid
								: SV_DispatchThreadID) {
	const float2 frameScale = FrameDim * RcpTexDim;

	uint2 pxCoord = dtid;
#if defined(HALF_RATE)
	const uint halfWidth = uint(OUT_FRAME_DIM.x) >> 1;
	const bool useHistory = dtid.x >= halfWidth;
	pxCoord.x = (pxCoord.x % halfWidth) * 2 + (dtid.y + FrameIndex + useHistory) % 2;
#else
	const static bool useHistory = false;
#endif

	float2 uv = (pxCoord + .5) * RCP_OUT_FRAME_DIM;
	uint eyeIndex = GetEyeIndexFromTexCoord(uv);

	float viewspaceZ = READ_DEPTH(srcWorkingDepth, pxCoord);

	float2 normalSample = FULLRES_LOAD(srcNormalRoughness, pxCoord, uv * frameScale, samplerLinearClamp).xy;
	float3 viewspaceNormal = DecodeNormal(normalSample);

	half2 encodedWorldNormal = EncodeNormal(ViewToWorldVector(viewspaceNormal, CameraViewInverse[eyeIndex]));
	outPrevGeo[pxCoord] = half3(viewspaceZ, encodedWorldNormal);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
	viewspaceZ *= 0.99920h;  // this is good for FP16 depth buffer

	float4 currGIAO = float4(0, 0, 0, 0);
	float4 currGIAOSpecular = float4(0, 0, 0, 0);
	float3 bentNormal = viewspaceNormal;

	bool needGI = viewspaceZ > FP_Z && viewspaceZ < DepthFadeRange.y;
	if (needGI) {
		if (!useHistory)
			CalculateGI(
				pxCoord, uv, viewspaceZ, viewspaceNormal,
				currGIAO, currGIAOSpecular, bentNormal);

#ifdef TEMPORAL_DENOISER
		float lerpFactor = rcp(srcAccumFrames[pxCoord] * 255);
#	if defined(HALF_RATE)
		if (useHistory && lerpFactor != 1)
			lerpFactor = 0;
#	endif

		currGIAO = lerp(srcPrevGI[pxCoord], currGIAO, lerpFactor);
#	ifdef GI_SPECULAR
		currGIAOSpecular = lerp(srcPrevGISpecular[pxCoord], currGIAOSpecular, lerpFactor);
#	endif
#endif
	}
	currGIAO = any(ISNAN(currGIAO)) ? float4(0, 0, 0, 0) : currGIAO;
	currGIAOSpecular = any(ISNAN(currGIAOSpecular)) ? float4(0, 0, 0, 0) : currGIAOSpecular;

	outGI[pxCoord] = currGIAO;
#ifdef GI_SPECULAR
	outGISpecular[pxCoord] = currGIAOSpecular;
#endif
#ifdef BENT_NORMAL
	outBentNormal[pxCoord] = EncodeNormal(bentNormal);
#endif
}