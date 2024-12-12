#include "Common/Color.hlsli"
#include "Common/FrameBuffer.hlsli"
#include "Common/GBuffer.hlsli"
#include "Common/Math.hlsli"
#include "Common/SharedData.hlsli"
#include "Common/Spherical Harmonics/SphericalHarmonics.hlsli"
#include "Common/VR.hlsli"

Texture2D<unorm half3> AlbedoTexture : register(t0);
Texture2D<unorm half3> NormalRoughnessTexture : register(t1);

#if defined(SKYLIGHTING)
#	include "Skylighting/Skylighting.hlsli"

Texture2D<unorm float> DepthTexture : register(t2);
Texture3D<sh2> SkylightingProbeArray : register(t3);
Texture2D<float4> IBLTexture : register(t8);

#endif

#if !defined(SKYLIGHTING) && defined(VR)  // VR also needs a depthbuffer
Texture2D<unorm float> DepthTexture : register(t2);
#endif

Texture2D<unorm half3> Masks2Texture : register(t4);

#if defined(SSGI)
Texture2D<half> SsgiAoTexture : register(t5);
Texture2D<half4> SsgiYTexture : register(t6);
Texture2D<half2> SsgiCoCgTexture : register(t7);
#endif

RWTexture2D<half3> MainRW : register(u0);
#if defined(SSGI)
RWTexture2D<half3> DiffuseAmbientRW : register(u1);
void SampleSSGI(uint2 pixCoord, float3 normalWS, out half ao, out half3 il)
{
	ao = 1 - SsgiAoTexture[pixCoord];
	half4 ssgiIlYSh = SsgiYTexture[pixCoord];
	// without ZH hallucination
	// half ssgiIlY = SphericalHarmonics::FuncProductIntegral(ssgiIlYSh, SphericalHarmonics::EvaluateCosineLobe(normalWS));
	half ssgiIlY = SphericalHarmonics::SHHallucinateZH3Irradiance(ssgiIlYSh, normalWS);
	half2 ssgiIlCoCg = SsgiCoCgTexture[pixCoord];
	il = max(0, Color::YCoCgToRGB(float3(ssgiIlY, ssgiIlCoCg)));
}
#endif

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	half2 uv = half2(dispatchID.xy + 0.5) * SharedData::BufferDim.zw;
	uint eyeIndex = Stereo::GetEyeIndexFromTexCoord(uv);
	uv *= FrameBuffer::DynamicResolutionParams2.xy;  // adjust for dynamic res
	uv = Stereo::ConvertFromStereoUV(uv, eyeIndex);

	half3 normalGlossiness = NormalRoughnessTexture[dispatchID.xy];
	half3 normalVS = GBuffer::DecodeNormal(normalGlossiness.xy);

	half3 diffuseColor = MainRW[dispatchID.xy];
	half3 albedo = AlbedoTexture[dispatchID.xy];
	half3 masks2 = Masks2Texture[dispatchID.xy];

	half pbrWeight = masks2.z;

	half3 normalWS = normalize(mul(FrameBuffer::CameraViewInverse[eyeIndex], half4(normalVS, 0)).xyz);

	half3 directionalAmbientColor = mul(SharedData::DirectionalAmbient, half4(normalWS, 1.0));

	half3 linAlbedo = Color::GammaToLinear(albedo) / Color::AlbedoPreMult;
	half3 linDirectionalAmbientColor = Color::GammaToLinear(directionalAmbientColor) / Color::LightPreMult;
	half3 linDiffuseColor = Color::GammaToLinear(diffuseColor);

	half3 linAmbient = lerp(Color::GammaToLinear(albedo * directionalAmbientColor), linAlbedo * linDirectionalAmbientColor, pbrWeight);

	half visibility = 1.0;
#if defined(SKYLIGHTING)
	float rawDepth = DepthTexture[dispatchID.xy];
	float4 positionCS = float4(2 * float2(uv.x, -uv.y + 1) - 1, rawDepth, 1);
	float4 positionMS = mul(FrameBuffer::CameraViewProjInverse[eyeIndex], positionCS);
	positionMS.xyz = positionMS.xyz / positionMS.w;
#	if defined(VR)
	positionMS.xyz += FrameBuffer::CameraPosAdjust[eyeIndex].xyz - FrameBuffer::CameraPosAdjust[0].xyz;
#	endif

	uint3 seed = Random::pcg3d(uint3(dispatchID.xy, dispatchID.x * Math::PI));
	float3 rnd = Random::R3Modified(SharedData::FrameCount, seed / 4294967295.f);
	
	// https://stats.stackexchange.com/questions/8021/how-to-generate-uniformly-distributed-points-in-the-3-d-unit-ball
	float phi = rnd.x * Math::TAU;
	float cos_theta = rnd.y * 2 - 1;
	float sin_theta = sqrt(1 - cos_theta);
	float r = rnd.z;
	float4 sincos_phi;
	sincos(phi, sincos_phi.y, sincos_phi.x);

	float3 offset = float3(r * sin_theta * sincos_phi.x, r * sin_theta * sincos_phi.y, r * cos_theta);
	positionMS.xyz += offset * 64;

	sh2 ibl = IBLTexture[int2(0, 0)];

	sh2 skylighting = Skylighting::sample(SharedData::skylightingSettings, SkylightingProbeArray, positionMS.xyz, normalWS);
    skylighting = lerp(skylighting, SphericalHarmonics::Product(skylighting, ibl), 0.5);
  
    half skylightingDiffuse = SphericalHarmonics::FuncProductIntegral(skylighting, SphericalHarmonics::EvaluateCosineLobe(float3(normalWS.xy, normalWS.z * 0.5 + 0.5))) / Math::PI;
	skylightingDiffuse = lerp(1.0, skylightingDiffuse, Skylighting::getFadeOutFactor(positionMS.xyz));
	skylightingDiffuse = Skylighting::mixDiffuse(SharedData::skylightingSettings, skylightingDiffuse);
	
	visibility = skylightingDiffuse;
#endif

#if defined(SSGI)
#	if defined(VR)
	float3 uvF = float3((dispatchID.xy + 0.5) * SharedData::BufferDim.zw, DepthTexture[dispatchID.xy]);  // calculate high precision uv of initial eye
	float3 uv2 = Stereo::ConvertStereoUVToOtherEyeStereoUV(uvF, eyeIndex, false);                        // calculate other eye uv
	float3 uv1Mono = Stereo::ConvertFromStereoUV(uvF, eyeIndex);
	float3 uv2Mono = Stereo::ConvertFromStereoUV(uv2, (1 - eyeIndex));
	uint2 pixCoord2 = (uint2)(uv2.xy / SharedData::BufferDim.zw - 0.5);
#	endif

	half ssgiAo;
	half3 ssgiIl;
	SampleSSGI(dispatchID.xy, normalWS, ssgiAo, ssgiIl);

#	if defined(VR)
	half ssgiAo2;
	half3 ssgiIl2;
	SampleSSGI(pixCoord2, normalWS, ssgiAo2, ssgiIl2);

	half4 ssgiMixed = Stereo::BlendEyeColors(uv1Mono, float4(ssgiIl, ssgiAo), uv2Mono, float4(ssgiIl2, ssgiAo2));
	ssgiAo = ssgiMixed.a;
	ssgiIl = ssgiMixed.rgb;
#	endif

	visibility *= ssgiAo;
#	if defined(INTERIOR)
	linDiffuseColor *= ssgiAo;
#	endif

	ssgiIl *= linAlbedo;
	DiffuseAmbientRW[dispatchID.xy] = linAlbedo * linDirectionalAmbientColor + ssgiIl;
	linDiffuseColor += ssgiIl;
#endif

	linAmbient *= visibility;
	diffuseColor = Color::LinearToGamma(linDiffuseColor);
	directionalAmbientColor = Color::LinearToGamma(linDirectionalAmbientColor * visibility * Color::LightPreMult);

	diffuseColor = lerp(diffuseColor + directionalAmbientColor * albedo, Color::LinearToGamma(linDiffuseColor + linAmbient), pbrWeight);

	MainRW[dispatchID.xy] = diffuseColor;
	//	MainRW[dispatchID.xy] = Color::LinearToGamma(((ssgiIl / linAlbedo) + linDirectionalAmbientColor * visibility));

};