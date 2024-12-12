#include "Common/Color.hlsli"
#include "Common/Math.hlsli"
#include "Skylighting/Skylighting.hlsli"

TextureCube<float4> ReflectionTexture : register(t0);
RWTexture2D<sh2> IBLTexture : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(1, 1, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	// Initialise sh to 0
	sh2 shLuminance = SphericalHarmonics::Zero();
	float luminanceSum = 0;

	float axisSampleCount = 512;

	// Accumulate coefficients according to surounding direction/color tuples.
	for (float az = 0.5; az < axisSampleCount; az += 1.0)
		for (float ze = 0.5; ze < axisSampleCount; ze += 1.0) {
			float3 rayDir = SphericalHarmonics::GetUniformSphereSample(az / axisSampleCount, ze / axisSampleCount);

			float3 color = ReflectionTexture.SampleLevel(LinearSampler, -rayDir, 0);

			color = Color::GammaToLinear(color);

			float luminance = Color::RGBToLuminance(color);

			luminanceSum += luminance;

			sh2 shSample = SphericalHarmonics::Evaluate(rayDir);

			shLuminance = SphericalHarmonics::Add(shLuminance, SphericalHarmonics::Scale(shSample, luminance));
		}

	//	shLuminance = SphericalHarmonics::Scale(shLuminance, rcp(luminanceSum));

	// Integrating over a sphere so each sample has a weight of 4*PI/samplecount (uniform solid angle, for each sample)
	float shFactor = 4.0 * Math::PI / (axisSampleCount * axisSampleCount);

	shLuminance = SphericalHarmonics::Scale(shLuminance, shFactor);

	IBLTexture[dispatchID.xy] = shLuminance;
}
