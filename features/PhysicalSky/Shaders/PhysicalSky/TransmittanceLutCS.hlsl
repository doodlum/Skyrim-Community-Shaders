#include "./Common.hlsli"

#define SAMPLE_COUNT 500

RWTexture2D<float3> TransmittanceLUT : register(u0);

[numthreads(8, 8, 1)] void main(uint2 coord
								: SV_DispatchThreadID) {
    const float2 data = UnmapTransmittance(float2(coord));
    float cosTheta = data.x, r = data.y;

    float3 ro = float3(0, r, 0);
    float3 rd = float3(sqrt(1 - cosTheta * cosTheta), cosTheta, 0);

    float2 t = IntersectSphere(AtmosphericRadius, cosTheta, r);

    float3 transmittance = 1.0f;
	if (t.y > 0.0)
	{
		t.x = max(t.x, 0.0);
        float3 start = ro + rd * t.x;
        float3 end = ro + rd * t.y;
	    float len = t.y - t.x;

        float3 h = 0.0f;
        for(int i = 0; i < SAMPLE_COUNT; ++i)
        {
            float3 p = lerp(start, end, float(i) / SAMPLE_COUNT);
            float e = ComputeExtinction(length(p) - PlanetRadius);
            
            h += e * (i == 0 || i == SAMPLE_COUNT) ? 0.5f : 1.0f;
        }

        transmittance = exp(-h * (len / SAMPLE_COUNT));
    }

	TransmittanceLUT[coord.xy] = transmittance;
}