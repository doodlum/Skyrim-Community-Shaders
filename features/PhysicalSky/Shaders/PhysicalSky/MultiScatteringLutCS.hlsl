#include "./Common.hlsli"

#define SAMPLE_COUNT 64

groupshared float3 gsRadianceMS[SAMPLE_COUNT];
groupshared float3 gsRadiance[SAMPLE_COUNT];

RWTexture2D<float3> MultiScatteringLUT : register(u0);
Texture2D<float3> Transmittance : register(t0);

SamplerState TransmittanceSampler;

void ComputeIntegrateResult(float3 ro, float3 rd, float end, float3 lightDir,
	out float3 skyMultiScattering, out float3 skyColor, out float3 skyTransmittance)
{
	skyColor = 0.0f;
	skyTransmittance = 1.0f;
	skyMultiScattering = 0.0f;

	const uint sampleCount = 16;

	for (uint s = 0; s < sampleCount; s++) {
		float t0 = (s) / (float)sampleCount, t1 = (s + 1.0f) / (float)sampleCount;
		t0 = t0 * t0 * end;
		t1 = t1 * t1 * end;
		float t = lerp(t0, t1, 0.5f);
		float dt = t1 - t0;

		const float3 p = ro + t * rd;
		const float r = max(length(p), PlanetRadius);
		const float3 n = p * rcp(r);
		const float h = r - PlanetRadius;

		const float3 sigmaE = ComputeExtinction(h);
		const float3 sigmaS = RayleighScattering.rgb * exp(-h / RayleighHeight) + MieScattering.rgb * exp(-h / MieHeight);
		const float3 transmittance = exp(-sigmaE * dt);

		skyMultiScattering += skyTransmittance * ((sigmaS - sigmaS * transmittance) / sigmaE);

		float cosTheta = dot(n, lightDir);
		float sinThetaH = PlanetRadius * rcp(r);
		float cosThetaH = -sqrt(saturate(1 - sinThetaH * sinThetaH));
		if (cosTheta >= cosThetaH)  // Above horizon
		{
			const float3 phaseScatter = sigmaS * rcp(4.0 * Math::PI);  // Isotropic
			float3 ms = SampleTransmittance(Transmittance, TransmittanceSampler, cosTheta, r) * phaseScatter;
			skyColor += skyTransmittance * ((ms - ms * transmittance) / sigmaE);
		}

		skyTransmittance *= transmittance;
	}
}

float3 DrawPlanet(float3 pos, float3 lightDir)
{
	float3 n = normalize(pos);

	float3 albedo = GroundAlbedo.xyz;
	float3 gBrdf = albedo * rcp(Math::PI);

	float cosTheta = dot(n, lightDir);

	float3 intensity = 0.0f;
	if (cosTheta >= 0) {
		intensity = SampleTransmittance(Transmittance, TransmittanceSampler, cosTheta, PlanetRadius);
	}

	return gBrdf * (saturate(cosTheta) * intensity);
}

void ParallelSum(uint threadIdx, inout float3 radiance, inout float3 radianceMS)
{
	gsRadiance[threadIdx] = radiance;
	gsRadianceMS[threadIdx] = radianceMS;
	GroupMemoryBarrierWithGroupSync();

	[unroll] for (uint s = SAMPLE_COUNT / 2u; s > 0u; s >>= 1u)
	{
		if (threadIdx < s) {
			gsRadiance[threadIdx] += gsRadiance[threadIdx + s];
			gsRadianceMS[threadIdx] += gsRadianceMS[threadIdx + s];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	radiance = gsRadiance[0];
	radianceMS = gsRadianceMS[0];
}

[numthreads(1, 1, SAMPLE_COUNT)] void main(uint3 coord
										   : SV_DispatchThreadID) {
	const float2 data = UnmapMultipleScattering(coord.xy);
	const float2 uv = coord.xy / (float2(MULTI_SCATTERING_LUT_WIDTH, MULTI_SCATTERING_LUT_HEIGHT) - 1);
	const uint threadIdx = coord.z;

	float3 ld = float3(0.0, data.x, sqrt(saturate(1 - data.x * data.x)));
	float3 ro = float3(0.0f, data.y, 0.0f);

	float2 sample = Hammersley2d(threadIdx, SAMPLE_COUNT);
	float3 rd = SampleSphereUniform(sample.x, sample.y);

	float3 n;
	float r;
	float2 t = IntersectAtmosphere(ro, -rd, n, r);
	float entry = t.x, exit = t.y;

	float cosChi = dot(n, rd);
	float sinThetaH = PlanetRadius * rcp(r);
	float cosThetaH = -sqrt(saturate(1 - sinThetaH * sinThetaH));

	bool targetGround = entry >= 0 && cosChi < cosThetaH;

	if (targetGround)
		exit = entry + IntersectSphere(PlanetRadius, cosChi, r).x;

	float3 skyMultiScattering = 0.0f, skyColor = 0.0f, skyTransmittance = 1.0f;
	if (exit > 0.0f)
		ComputeIntegrateResult(ro, rd, exit, ld, skyMultiScattering, skyColor, skyTransmittance);

	if (targetGround)
		skyColor += DrawPlanet(ro + exit * rd, ld) * skyTransmittance;

	const float dS = 1 / SAMPLE_COUNT;
	float3 radiance = skyColor * dS;
	float3 radianceMS = skyMultiScattering * dS;

	ParallelSum(threadIdx, radiance, radianceMS);
	if (threadIdx > 0)
		return;

	const float3 fms = 1.0f * rcp(1.0 - radianceMS);  // Equation 9
	const float3 ms = radiance * fms;                 // Equation 10

	MultiScatteringLUT[coord.xy] = ms;
}