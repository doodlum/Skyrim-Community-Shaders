#ifndef FAKE_SKY_COMMON
#define FAKE_SKY_COMMON

#include "Common/Math.hlsli"

#define TRANSMITTANCE_LUT_WIDTH 256
#define TRANSMITTANCE_LUT_HEIGHT 64
#define MULTI_SCATTERING_LUT_WIDTH 32
#define MULTI_SCATTERING_LUT_HEIGHT 32

cbuffer AtmosphereCB : register(b0)
{
	float PlanetRadius;
	float AtmosphericRadius;
	float AtmosphericDepth;

	float4 RayleighScattering;
	float4 RayleighAbsorption;
	float RayleighHeight;

	float4 MieScattering;
	float MieAbsorption;
	float MieHeight;
	float MieAnisotropy;

	float4 OzoneAbsorption;
	float OzoneCenterAltitude;
	float OzoneWidth;

    float3 GroundAlbedo;
}

float RayleighPhaseFunction(float cosTheta)
{
	// BN08
	return 3.0 / (16.0 * Math::PI) * (1.0 + cosTheta * cosTheta);
}

float3 MiePhaseFunction(float cosTheta)
{
	// GK99
	float g = MieAnisotropy;
	float k = 3.0 / (8.0 * Math::PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5);
}

float OzoneDensity(float h)
{
	// Bru17a,17b
	return saturate(1.0 - 0.5 * abs(h - OzoneCenterAltitude) / OzoneWidth);
}

float3 ComputeExtinction(float h)
{
	float3 e = RayleighAbsorption.rgb * exp(-h / RayleighHeight)
				+ MieAbsorption * exp(-h / MieHeight)
				+ OzoneAbsorption.rgb * OzoneDensity(h);

	return max(e, float3(1e-6));
}

float2 IntersectSphere(float radius, float cosTheta, float r)
{
	const float s = radius * rcp(r);
	float dis = s * s - saturate(1.0 - cosTheta * cosTheta);

	return dis < 0 ? dis : r * float2(-cosTheta - sqrt(dis), -cosTheta + sqrt(dis));
}

float2 IntersectAtmosphere(float3 ro, float3 rd, out float3 n, out float r)
{
	n = normalize(ro);
	r = length(ro);

	float2 t = IntersectSphere(AtmosphericRadius, dot(n, -rd), r);

	if (t.y >= 0)
	{
		t.x = max(t.x, 0.0);

		if (t.x > 0)
		{
			n = normalize(ro + t.x * -rd);
			r = AtmosphericRadius;
		}
	}

	return t;
}

// Mapping
float UnitRangeToTextureCoord(float x, int textureSize)
{
	return 0.5f / float(textureSize) + x * (1.0f - 1.0f / float(textureSize));
}

float TextureCoordToUnitRange(float u, int textureSize)
{
	return (u - 0.5f / float(textureSize)) / (1.0f - 1.0f / float(textureSize));
}

float3 SphericalToCartesian(float phi, float cosTheta)
{
	float sinPhi, cosPhi;
	sincos(phi, sinPhi, cosPhi);
	float sinTheta = sqrt(saturate(1 - cosPhi * cosPhi));

	return float3(float2(cosPhi, sinPhi) * sinTheta, cosTheta);
}

float3 SampleSphereUniform(float u1, float u2)
{
	const float phi = Math::TAU * u2;
	const float cosTheta = 1.0 - 2.0 * u1;

	return SphericalToCartesian(phi, cosTheta);
}

float VanDerCorpus(uint bits)
{
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	return bits * rcp(4294967296.0);
}

float Hammersley2d(uint i, uint sampleCount)
{
	return float2(float(i) / float(sampleCount), VanDerCorpus(i));
}

float2 MapTransmittance(float cosTheta, float r)
{
    float h = sqrt(AtmosphericRadius * AtmosphericRadius - PlanetRadius * PlanetRadius);
    float rho = sqrt(max(r * r - PlanetRadius * PlanetRadius, 0.0));

    float d = max(0, IntersectSphere(AtmosphericRadius, cosTheta, r).x);
    float dMin = AtmosphericRadius - r;
    float dMax = rho + h;

    float u = UnitRangeToTextureCoord((d - dMin) / (dMax - dMin), TRANSMITTANCE_LUT_WIDTH);
    float v = UnitRangeToTextureCoord(rho / h, TRANSMITTANCE_LUT_HEIGHT);

    return float2(u, v);
}

float2 UnmapTransmittance(float2 coord)
{
    float h = sqrt(AtmosphericRadius * AtmosphericRadius - PlanetRadius * PlanetRadius);
    float rho = h * TextureCoordToUnitRange(coord.y, TRANSMITTANCE_LUT_HEIGHT);

    float r = sqrt(rho * rho + PlanetRadius * PlanetRadius);
	
    float dMin = AtmosphericRadius - r;
    float dMax = rho + h;
    float d = dMin + TextureCoordToUnitRange(coord.x, TRANSMITTANCE_LUT_WIDTH) * (dMax - dMin);
    float cosTheta = d == 0.0 ? 1.0 : clamp((h * h - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);

    return float2(cosTheta, r);
}

float3 SampleTransmittance(Texture2D<float3> t, SamplerState s, float cosTheta, float r)
{
    return t.SampleLevel(s, MapTransmittance(cosTheta, r), 0);
}

float2 MapMultipleScattering(float cosTheta, float r)
{
    return saturate(float2(cosTheta * 0.5f + 0.5f, r / AtmosphericDepth));
}

float2 UnmapMultipleScattering(float2 coord)
{
    const float2 uv = coord / (float2(MULTI_SCATTERING_LUT_WIDTH, MULTI_SCATTERING_LUT_HEIGHT) - 1.0);

    return float2(uv.x * 2.0 - 1.0, lerp(PlanetRadius, AtmosphericRadius, uv.y));
}

float3 SampleMultipleScattering(Texture2D<float3> t, SamplerState s, float cosTheta, float r)
{
    return t.SampleLevel(s, MapMultipleScattering(cosTheta, r), 0);
}

#endif