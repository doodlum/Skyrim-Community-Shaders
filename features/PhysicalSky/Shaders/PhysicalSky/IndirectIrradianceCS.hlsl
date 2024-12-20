#include "./Common.hlsli"

RWTexture3D<float3> SingleRayleighScatteringTexture : register(u0);
RWTexture3D<float3> SingleMieScatteringTexture : register(u1);
RWTexture3D<float3> MultipleScatteringTexture : register(u2);

[numthreads(4, 4, 4)] void main(uint3 coord
								: SV_DispatchThreadID) {

}