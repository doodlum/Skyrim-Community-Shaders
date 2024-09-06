// depth-aware upsampling: https://gist.github.com/pixelmager/a4364ea18305ed5ca707d89ddc5f8743

#include "../Common/FastMath.hlsli"
#include "common.hlsli"

Texture2D<half> srcDepth : register(t0);
Texture2D<half4> srcGI : register(t1);  // half-res

RWTexture2D<half4> outGI : register(u0);

#define min4(v) min(min(v.x, v.y), min(v.z, v.w))
#define max4(v) max(max(v.x, v.y), max(v.z, v.w))

[numthreads(8, 8, 1)] void main(const uint2 dtid
								: SV_DispatchThreadID) {
	int2 px00 = (dtid >> 1) + (dtid & 1) - 1;
	int2 px10 = px00 + int2(1, 0);
	int2 px01 = px00 + int2(0, 1);
	int2 px11 = px00 + int2(1, 1);

	float4 d = float4(
		srcDepth.Load(int3(px00, 1)),
		srcDepth.Load(int3(px01, 1)),
		srcDepth.Load(int3(px10, 1)),
		srcDepth.Load(int3(px11, 1)));

	// note: edge-detection
	float mind = min4(d);
	float maxd = max4(d);
	float diffd = maxd - mind;
	float avg = dot(d, 0.25.xxxx);
	bool d_edge = (diffd / avg) < 0.1;

	float4 atten;

	[branch] if (d_edge)
	{
		float4 gisample0 = srcGI[px00];
		float4 gisample1 = srcGI[px01];
		float4 gisample2 = srcGI[px10];
		float4 gisample3 = srcGI[px11];

		float bgdepth = srcDepth[dtid];

		//note: depth weighing from https://www.ppsloan.org/publications/ProxyPG.pdf#page=5
		float4 dd = abs(d - bgdepth);
		float4 w = 1.0 / (dd + 0.00001);
		float sumw = w.x + w.y + w.z + w.w;

		atten = (gisample0 * w.x + gisample1 * w.y + gisample2 * w.z + gisample3 * w.w) / (sumw + 0.00001);
	}
	else
	{
		atten = srcGI.SampleLevel(samplerLinearClamp, (dtid + .5) * RcpFrameDim * OUT_FRAME_DIM * RcpTexDim, 0);
	}

	outGI[dtid] = atten;
}