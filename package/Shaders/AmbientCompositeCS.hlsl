#include "Common/Color.hlsli"
#include "Common/DeferredShared.hlsli"
#include "Common/FrameBuffer.hlsli"
#include "Common/GBuffer.hlsli"
#include "Common/VR.hlsli"

Texture2D<unorm half3> AlbedoTexture : register(t0);
Texture2D<unorm half3> NormalRoughnessTexture : register(t1);

#if defined(SKYLIGHTING)
#	define SL_INCL_STRUCT
#	define SL_INCL_METHODS
#	include "Skylighting/Skylighting.hlsli"

cbuffer SkylightingCB : register(b1)
{
	SkylightingSettings skylightingSettings;
};

Texture2D<unorm float> DepthTexture : register(t2);
Texture3D<sh2> SkylightingProbeArray : register(t3);
#endif

#if defined(SSGI)
Texture2D<half4> SSGITexture : register(t4);
#endif

Texture2D<unorm half3> Masks2Texture : register(t5);

RWTexture2D<half3> MainRW : register(u0);
#if defined(SSGI)
RWTexture2D<half3> DiffuseAmbientRW : register(u1);
#endif

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	half2 uv = half2(dispatchID.xy + 0.5) * BufferDim.zw * DynamicResolutionParams2.xy;
	uint eyeIndex = GetEyeIndexFromTexCoord(uv);
	uv = ConvertFromStereoUV(uv, eyeIndex);

	half3 normalGlossiness = NormalRoughnessTexture[dispatchID.xy];
	half3 normalVS = DecodeNormal(normalGlossiness.xy);

	half3 diffuseColor = MainRW[dispatchID.xy];
	half3 albedo = AlbedoTexture[dispatchID.xy];
	half3 masks2 = Masks2Texture[dispatchID.xy];

	half pbrWeight = masks2.z;

	half3 normalWS = normalize(mul(CameraViewInverse[eyeIndex], half4(normalVS, 0)).xyz);

	half3 directionalAmbientColor = mul(DirectionalAmbient, half4(normalWS, 1.0));

	half3 linAlbedo = GammaToLinear(albedo);
	half3 linDirectionalAmbientColor = GammaToLinear(directionalAmbientColor);
	half3 linDiffuseColor = GammaToLinear(diffuseColor);

	half3 linAmbient = lerp(GammaToLinear(albedo * directionalAmbientColor), linAlbedo * linDirectionalAmbientColor, pbrWeight);

	half visibility = 1.0;
#if defined(SKYLIGHTING)
	float rawDepth = DepthTexture[dispatchID.xy];
	float4 positionCS = float4(2 * float2(uv.x, -uv.y + 1) - 1, rawDepth, 1);
	float4 positionMS = mul(CameraViewProjInverse[eyeIndex], positionCS);
	positionMS.xyz = positionMS.xyz / positionMS.w;
#	if defined(VR)
	positionMS.xyz += CameraPosAdjust[eyeIndex] - CameraPosAdjust[0];
#	endif

	sh2 skylighting = Skylighting::sample(skylightingSettings, SkylightingProbeArray, positionMS.xyz, normalWS);
	half skylightingDiffuse = shFuncProductIntegral(skylighting, shEvaluateCosineLobe(skylightingSettings.DirectionalDiffuse ? normalWS : float3(0, 0, 1))) / shPI;
	skylightingDiffuse = Skylighting::mixDiffuse(skylightingSettings, skylightingDiffuse);

	visibility = skylightingDiffuse;
#endif

#if defined(SSGI)
	half4 ssgiDiffuse = SSGITexture[dispatchID.xy];
	ssgiDiffuse.rgb *= linAlbedo;
	ssgiDiffuse.a = 1 - ssgiDiffuse.a;

	visibility *= ssgiDiffuse.a;

	DiffuseAmbientRW[dispatchID.xy] = linAlbedo * linDirectionalAmbientColor + ssgiDiffuse.rgb;

#	if defined(INTERIOR)
	linDiffuseColor *= ssgiDiffuse.a;
#	endif
	linDiffuseColor += ssgiDiffuse.rgb;
#endif

	linAmbient *= visibility;
	diffuseColor = LinearToGamma(linDiffuseColor);
	directionalAmbientColor = LinearToGamma(linDirectionalAmbientColor * visibility);

	diffuseColor = lerp(diffuseColor + directionalAmbientColor * albedo, LinearToGamma(linDiffuseColor + linAmbient), pbrWeight);

	MainRW[dispatchID.xy] = diffuseColor;
};