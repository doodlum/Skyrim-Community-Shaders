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

	float axisSampleCount = 32;

	// Accumulate coefficients according to surounding direction/color tuples.
	for (float az = 0.5; az < axisSampleCount; az += 1.0)
		for (float ze = 0.5; ze < axisSampleCount; ze += 1.0) {
			float3 rayDir = SphericalHarmonics::GetUniformSphereSample(az / axisSampleCount, ze / axisSampleCount);

			float3 color = ReflectionTexture.SampleLevel(LinearSampler, rayDir, 0);

			color = Color::GammaToLinear(color);

			float luminance = Color::RGBToLuminance(color);

			sh2 shSample = SphericalHarmonics::Evaluate(rayDir);

			shLuminance = SphericalHarmonics::Add(shLuminance, SphericalHarmonics::Scale(shSample, luminance));
		}

	// Integrating over a sphere so each sample has a weight of 4*PI/samplecount (uniform solid angle, for each sample)
	float shFactor = 4.0 * Math::PI / (axisSampleCount * axisSampleCount);

	shLuminance = SphericalHarmonics::Scale(shLuminance, shFactor);

	float maxSH = 0.0;

	for (float az = 0.5; az < axisSampleCount; az += 1.0)
		for (float ze = 0.5; ze < axisSampleCount; ze += 1.0) {
			float3 rayDir = SphericalHarmonics::GetUniformSphereSample(az / axisSampleCount, ze / axisSampleCount);
			maxSH = max(maxSH, SphericalHarmonics::Unproject(shLuminance, rayDir));
		}

	if (maxSH <= 0.0)
		shLuminance = float4(sqrt(4.0 * Math::PI), 0, 0, 0);
	else
		shLuminance = SphericalHarmonics::Scale(shLuminance, 1.0 / maxSH);

	IBLTexture[dispatchID.xy] = shLuminance;
}
