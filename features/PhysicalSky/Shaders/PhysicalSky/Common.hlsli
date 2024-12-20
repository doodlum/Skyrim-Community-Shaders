#ifndef FAKE_SKY_COMMON
#define FAKE_SKY_COMMON

#include "Common/Math.hlsli"

cbuffer AtmosphereCB : register(b0)
{
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
}

float RayleighPhase(float nu)
{
	// BN08
	return 3.0 / (16.0 * Math::PI) * (1.0 + nu * nu);
}

float3 MiePhase(float nu)
{
	// GK99
	float g = MieAnisotropy;
	float k = 3.0 / (8.0 * Math::PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}

float OzoneDensity(float h)
{
	// Bru17a,17b
	return saturate(1.0 - 0.5 * abs(h - OzoneCenterAltitude) / OzoneWidth);
}

float3 Extinction(float h)
{
	float3 e = RayleighAbsorption * exp(-h / RayleighHeight)
				+ MieAbsorption.rgb * exp(-h / MieHeight)
				+ OzoneAbsorption.rgb * OzoneDensity(h);

	return max(e, FLT_MIN);
}

float2 IntersectSphere(float r, float mu, float radialDistance)
{
	const float s = r * rcp(radialDistance);
	float d = s * s - saturate(1.0 - mu * mu);

	return d < 0 ? d : radialDistance * float2(-mu - sqrt(d), -mu + sqrt(d));  // the closest hit & the farthest hit
}

float2 IntersectAtmosphere(float3 o, float3 v, out float3 n, out float r)
{
	n = normalize(o);
	r = length(o);

	float2 t = IntersectSphere(AtmosphericRadius, dot(n, -v), r);

	if (t.y >= 0)
	{
		t.x = max(t.x, 0.0);

		if (t.x > 0)
		{
			float3 p = o + t.x * -v;
			n = normalize(p);
			r = AtmosphericRadius;
		}
	}

	return t;
}

#endif