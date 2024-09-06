#include "../Common/Color.hlsli"
#include "../Common/Constants.hlsli"
#include "../Common/DeferredShared.hlsli"
#include "../Common/FrameBuffer.hlsli"
#include "../Common/VR.hlsli"

RWTexture2DArray<float4> DynamicCubemap : register(u0);
RWTexture2DArray<float4> DynamicCubemapRaw : register(u1);
RWTexture2DArray<float4> DynamicCubemapPosition : register(u2);

Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> ColorTexture : register(t1);

SamplerState LinearSampler : register(s0);

// Calculate normalized sampling direction vector based on current fragment coordinates.
// This is essentially "inverse-sampling": we reconstruct what the sampling vector would be if we wanted it to "hit"
// this particular fragment in a cubemap.
float3 GetSamplingVector(uint3 ThreadID, in RWTexture2DArray<float4> OutputTexture)
{
	float width = 0.0f;
	float height = 0.0f;
	float depth = 0.0f;
	OutputTexture.GetDimensions(width, height, depth);

	float2 st = ThreadID.xy / float2(width, height);
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

	// Select vector based on cubemap face index.
	float3 result = float3(0.0f, 0.0f, 0.0f);
	switch (ThreadID.z) {
	case 0:
		result = float3(1.0, uv.y, -uv.x);
		break;
	case 1:
		result = float3(-1.0, uv.y, uv.x);
		break;
	case 2:
		result = float3(uv.x, 1.0, -uv.y);
		break;
	case 3:
		result = float3(uv.x, -1.0, uv.y);
		break;
	case 4:
		result = float3(uv.x, uv.y, 1.0);
		break;
	case 5:
		result = float3(-uv.x, uv.y, -1.0);
		break;
	}
	return normalize(result);
}

cbuffer UpdateData : register(b1)
{
	uint Reset;
	float3 CameraPreviousPosAdjust2;
}

bool IsSaturated(float value) { return value == saturate(value); }
bool IsSaturated(float2 value) { return IsSaturated(value.x) && IsSaturated(value.y); }

// Inverse project UV + raw depth into world space.
float3 InverseProjectUVZ(float2 uv, float z)
{
	uv.y = 1 - uv.y;
	float4 cp = float4(float3(uv, z) * 2 - 1, 1);
	float4 vp = mul(CameraViewProjInverse[0], cp);
	return float3(vp.xy, vp.z) / vp.w;
}

float smoothbumpstep(float edge0, float edge1, float x)
{
	x = 1.0 - abs(saturate((x - edge0) / (edge1 - edge0)) - 0.5) * 2.0;
	return x * x * (3.0 - x - x);
}

[numthreads(8, 8, 1)] void main(uint3 ThreadID
								: SV_DispatchThreadID) {
	float3 captureDirection = -GetSamplingVector(ThreadID, DynamicCubemap);
	float3 viewDirection = WorldToView(captureDirection, false);
	float2 uv = ViewToUV(viewDirection, false);

	if (Reset) {
		DynamicCubemap[ThreadID] = 0.0;
		DynamicCubemapRaw[ThreadID] = 0.0;
		DynamicCubemapPosition[ThreadID] = 0.0;
		return;
	}

	if (IsSaturated(uv) && viewDirection.z < 0.0) {  // Check that the view direction exists in screenspace and that it is in front of the camera
		float2 PoissonDisc[64] = {
			float2(0.403302f, 0.139622f),
			float2(0.578204f, 0.624683f),
			float2(0.942412f, 0.973693f),
			float2(0.0789209f, 0.902676f),
			float2(0.904355f, 0.158025f),
			float2(0.0505387f, 0.403394f),
			float2(0.99176f, 0.587085f),
			float2(0.0210273f, 0.00875881f),
			float2(0.329905f, 0.460005f),
			float2(0.386853f, 0.985321f),
			float2(0.625721f, 0.34431f),
			float2(0.346782f, 0.687551f),
			float2(0.659139f, 0.843989f),
			float2(0.798029f, 0.469008f),
			float2(0.14127f, 0.19602f),
			float2(0.684011f, 0.0732444f),
			float2(0.0515458f, 0.672048f),
			float2(0.239662f, 0.00369274f),
			float2(0.985076f, 0.363414f),
			float2(0.191229f, 0.594928f),
			float2(0.820887f, 0.720725f),
			float2(0.508103f, 0.0209967f),
			float2(0.783654f, 0.302744f),
			float2(0.467269f, 0.82757f),
			float2(0.214911f, 0.809259f),
			float2(0.747703f, 0.986847f),
			float2(0.966033f, 0.00357067f),
			float2(0.447432f, 0.292367f),
			float2(0.954253f, 0.813837f),
			float2(0.209815f, 0.356304f),
			float2(0.561663f, 0.970794f),
			float2(0.334544f, 0.340678f),
			float2(0.461013f, 0.419691f),
			float2(0.229865f, 0.993164f),
			float2(0.797327f, 0.838832f),
			float2(0.578478f, 0.128452f),
			float2(0.265358f, 0.135685f),
			float2(0.495621f, 0.710837f),
			float2(0.71102f, 0.643971f),
			float2(0.0191046f, 0.2219f),
			float2(0.990265f, 0.240516f),
			float2(0.85403f, 0.589709f),
			float2(0.369488f, 0.0120548f),
			float2(0.478378f, 0.551805f),
			float2(0.664815f, 0.519913f),
			float2(0.843684f, 0.0224006f),
			float2(0.0827357f, 0.530961f),
			float2(0.116398f, 0.0798364f),
			float2(0.676931f, 0.221381f),
			float2(0.917447f, 0.472091f),
			float2(0.334819f, 0.836451f),
			float2(0.00308237f, 0.800134f),
			float2(0.565752f, 0.823206f),
			float2(0.874783f, 0.33668f),
			float2(0.336772f, 0.592364f),
			float2(0.151402f, 0.729484f),
			float2(0.706656f, 0.748802f),
			float2(0.723411f, 0.379315f),
			float2(0.805109f, 0.16715f),
			float2(0.853877f, 0.243294f),
			float2(0.91464f, 0.693289f),
			float2(0.846828f, 0.918821f),
			float2(0.188513f, 0.501236f),
			float2(0.0812708f, 0.993774f)
		};

		// Assume projection maps to one cubemap face, and randomly sample an area within the mapped resolution
		uv += (PoissonDisc[FrameCountAlwaysActive % 64] * 2.0 - 1.0) * BufferDim.zw * max(1.0, float2(BufferDim.x / 128.0, BufferDim.y / 128.0)) * 0.5;
		uv = saturate(uv);

		uv = GetDynamicResolutionAdjustedScreenPosition(uv);
		uv = ConvertToStereoUV(uv, 0);

		float depth = DepthTexture.SampleLevel(LinearSampler, uv, 0);
		float linearDepth = GetScreenDepth(depth);

		if (linearDepth > 16.5) {  // Ignore objects which are too close
			float3 color = ColorTexture.SampleLevel(LinearSampler, uv, 0);
			float4 output = float4(GammaToLinear(color), 1.0);
			float lerpFactor = 1.0 / 64.0;

			half4 positionCS = half4(2 * half2(uv.x, -uv.y + 1) - 1, depth, 1);
			positionCS = mul(CameraViewProjInverse[0], positionCS);
			positionCS.xyz = positionCS.xyz / positionCS.w;

			float4 position = float4(positionCS.xyz * 0.001, linearDepth < (4096.0 * 2.5));

			DynamicCubemapPosition[ThreadID] = lerp(DynamicCubemapPosition[ThreadID], position, lerpFactor);
			DynamicCubemapRaw[ThreadID] = max(0, lerp(DynamicCubemapRaw[ThreadID], output, lerpFactor));

			output *= sqrt(saturate(0.5 * length(position.xyz)));

			DynamicCubemap[ThreadID] = max(0, lerp(DynamicCubemap[ThreadID], output, lerpFactor));

			return;
		}
	}

	float4 position = DynamicCubemapPosition[ThreadID];
	position.xyz = (position.xyz + (CameraPreviousPosAdjust2.xyz * 0.001)) - (CameraPosAdjust[0].xyz * 0.001);  // Remove adjustment, add new adjustment
	DynamicCubemapPosition[ThreadID] = position;

	float4 color = DynamicCubemapRaw[ThreadID];

	float distanceFactor = sqrt(smoothbumpstep(0.0, 2.0, length(position.xyz)));
	color *= distanceFactor;

	DynamicCubemap[ThreadID] = max(0, color);
}
