#include "../Common/GBuffer.hlsli"
#include "../Common/VR.hlsli"
#include "common.hlsli"

Texture2D<half4> srcDiffuse : register(t0);
Texture2D<half4> srcPrevGI : register(t1);  // maybe half-res
Texture2D<half> srcCurrDepth : register(t2);
Texture2D<half4> srcCurrNormal : register(t3);
Texture2D<half3> srcPrevGeo : register(t4);  // maybe half-res
Texture2D<float4> srcMotionVec : register(t5);
Texture2D<half4> srcPrevGIAlbedo : register(t6);       // maybe half-res
Texture2D<unorm float> srcAccumFrames : register(t7);  // maybe half-res

RWTexture2D<float3> outRadianceDisocc : register(u0);
RWTexture2D<unorm float> outAccumFrames : register(u1);
RWTexture2D<float4> outRemappedPrevGI : register(u2);

#if (defined(GI) && defined(GI_BOUNCE)) || defined(TEMPORAL_DENOISER)
#	define REPROJECTION
#endif

void readHistory(
	uint eyeIndex, float3 curr_pos, float3 curr_normal, int2 pixCoord, float bilinear_weight,
	inout half4 prev_gi, inout half4 prev_gi_albedo, inout float accum_frames, inout float wsum)
{
	const float2 uv = (pixCoord + .5) * RcpOutFrameDim;
	const float2 screen_pos = ConvertToStereoUV(uv, eyeIndex);
	if (any(screen_pos < 0) || any(screen_pos > 1))
		return;

	const half3 prev_geo = srcPrevGeo[pixCoord];
	const float prev_depth = prev_geo.x;
	const float3 prev_normal = DecodeNormal(prev_geo.yz);  // prev normal is already world
	float3 prev_pos = ScreenToViewPosition(screen_pos, prev_depth, eyeIndex);
	prev_pos = ViewToWorldPosition(prev_pos, PrevInvViewMat[eyeIndex]);

	float3 delta_pos = curr_pos - prev_pos;
	float normal_prod = dot(curr_normal, prev_normal);

	bool depth_pass = dot(delta_pos, delta_pos) < DepthDisocclusion * DepthDisocclusion;
	bool normal_pass = normal_prod * normal_prod > NormalDisocclusion;
	if (depth_pass && normal_pass) {
#if defined(GI) && defined(GI_BOUNCE)
		prev_gi_albedo += srcPrevGIAlbedo[pixCoord] * bilinear_weight;
#endif
#ifdef TEMPORAL_DENOISER
		prev_gi += srcPrevGI[pixCoord] * bilinear_weight;
		accum_frames += srcAccumFrames[pixCoord] * bilinear_weight;
#endif
		wsum += bilinear_weight;
	}
};

[numthreads(8, 8, 1)] void main(const uint2 pixCoord : SV_DispatchThreadID) {
	const float srcScale = SrcFrameDim * RcpTexDim;
	const float outScale = OutFrameDim * RcpTexDim;

	const float2 uv = (pixCoord + .5) * RcpOutFrameDim;
	uint eyeIndex = GET_EYE_IDX(uv);
	const float2 screen_pos = ConvertToStereoUV(uv, eyeIndex);

	float2 prev_uv = uv;
#ifdef REPROJECTION
	prev_uv += FULLRES_LOAD(srcMotionVec, pixCoord, uv * srcScale, samplerLinearClamp).xy;
#endif
	float2 prev_screen_pos = ConvertToStereoUV(prev_uv, eyeIndex);

	half4 prev_gi_albedo = 0;
	half4 prev_gi = 0;
	float accum_frames = 0;
	float wsum = 0;

	const float curr_depth = READ_DEPTH(srcCurrDepth, pixCoord);
#ifdef REPROJECTION
	if ((curr_depth <= DepthFadeRange.y) && !(any(prev_screen_pos < 0) || any(prev_screen_pos > 1))) {
		float3 curr_normal = DecodeNormal(FULLRES_LOAD(srcCurrNormal, pixCoord, uv * srcScale, samplerLinearClamp).xy);
		curr_normal = ViewToWorldVector(curr_normal, InvViewMatrix[eyeIndex]);
		float3 curr_pos = ScreenToViewPosition(screen_pos, curr_depth, eyeIndex);
		curr_pos = ViewToWorldPosition(curr_pos, InvViewMatrix[eyeIndex]);

		float2 prev_px_coord = prev_uv * OutFrameDim;
		int2 prev_px_lu = floor(prev_px_coord - 0.5);
		float2 bilinear_weights = prev_px_coord - 0.5 - prev_px_lu;
		{
			int2 px = prev_px_lu;
			float w = (1 - bilinear_weights.x) * (1 - bilinear_weights.y);
			readHistory(eyeIndex, curr_pos, curr_normal, px, w,
				prev_gi, prev_gi_albedo, accum_frames, wsum);
		}
		{
			int2 px = prev_px_lu + uint2(1, 0);
			float w = bilinear_weights.x * (1 - bilinear_weights.y);
			readHistory(eyeIndex, curr_pos, curr_normal, px, w,
				prev_gi, prev_gi_albedo, accum_frames, wsum);
		}
		{
			int2 px = prev_px_lu + uint2(0, 1);
			float w = (1 - bilinear_weights.x) * bilinear_weights.y;
			readHistory(eyeIndex, curr_pos, curr_normal, px, w,
				prev_gi, prev_gi_albedo, accum_frames, wsum);
		}
		{
			int2 px = prev_px_lu + uint2(1, 1);
			float w = bilinear_weights.x * bilinear_weights.y;
			readHistory(eyeIndex, curr_pos, curr_normal, px, w,
				prev_gi, prev_gi_albedo, accum_frames, wsum);
		}

		if (wsum > 1e-2) {
			float rcpWsum = rcp(wsum + 1e-2);
#	ifdef TEMPORAL_DENOISER
			prev_gi *= rcpWsum;
			accum_frames *= rcpWsum;
#	endif
#	if defined(GI) && defined(GI_BOUNCE)
			prev_gi_albedo *= rcpWsum;
#	endif
		}
	}
#endif

	half3 radiance = 0;
#ifdef GI
	radiance = FULLRES_LOAD(srcDiffuse, pixCoord, uv * srcScale, samplerLinearClamp).rgb;
#	ifdef GI_BOUNCE
	radiance += prev_gi_albedo.rgb * GIBounceFade;
#	endif
	outRadianceDisocc[pixCoord] = radiance;
#endif

#ifdef TEMPORAL_DENOISER
	accum_frames = min(accum_frames * 255 + 1, MaxAccumFrames);
	outAccumFrames[pixCoord] = accum_frames / 255.0;
	outRemappedPrevGI[pixCoord] = prev_gi;
#endif
}