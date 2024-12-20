#include "./Common.hlsli"

#define MULTI_SCATTERING_LUT_WIDTH 32
#define MULTI_SCATTERING_LUT_HEIGHT 32
#define SAMPLE_COUNT 64

RWTexture2D<float3> MultiScatteringLUT : register(u0);

[numthreads(1, 1, SAMPLE_COUNT)] void main(uint3 coord
											: SV_DispatchThreadID) {
}