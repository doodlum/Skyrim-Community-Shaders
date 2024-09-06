Texture2D<float4> WaterCaustics : register(t70);

namespace WaterLighting
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Caustics
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	float2 PanCausticsUV(float2 uv, float speed, float tiling)
	{
		return (float2(1, 0) * Timer * speed) + (uv * tiling);
	}

	float3 SampleCaustics(float2 uv)
	{
		return WaterCaustics.Sample(SampColorSampler, uv).r;
	}

	float3 ComputeCaustics(float2 uv)
	{
		float2 causticsUV = uv * 10.0;

		float2 causticsUV1 = PanCausticsUV(causticsUV, 0.5 * 0.2, 1.0);
		float2 causticsUV2 = PanCausticsUV(causticsUV, 1.0 * 0.2, -0.5);

		float3 caustics1 = SampleCaustics(causticsUV1);
		float3 caustics2 = SampleCaustics(causticsUV2);

		float3 caustics = min(caustics1, caustics2) * 5.0;

		return caustics;
	}

	float3 ComputeCaustics(float4 waterData, float3 worldPosition, float3 worldSpaceNormal)
	{
		float causticsDistToWater = waterData.w - worldPosition.z;
		float shoreFactorCaustics = saturate(causticsDistToWater / 64.0);
		float upAmount = worldSpaceNormal.z * 0.5 + 0.5;

		float causticsAmount = shoreFactorCaustics * upAmount;

		if (causticsAmount > 0.0) {
			float causticsFade = 1.0 - saturate(causticsDistToWater / 1024.0);
			causticsFade *= causticsFade;

			float2 causticsUV = (worldPosition.xyz + CameraPosAdjust[0].xyz) * 0.005;

			float2 causticsUV1 = PanCausticsUV(causticsUV, 0.5 * 0.2, 1.0);
			float2 causticsUV2 = PanCausticsUV(causticsUV, 1.0 * 0.2, -0.5);

			float3 caustics1 = SampleCaustics(causticsUV1);
			float3 caustics2 = SampleCaustics(causticsUV2);

			float3 causticsHigh = min(caustics1, caustics2) * 5.0;

			causticsUV *= 0.5;

			causticsUV1 = PanCausticsUV(causticsUV, 0.5 * 0.1, 1.0);
			causticsUV2 = PanCausticsUV(causticsUV, 1.0 * 0.1, -0.5);

			caustics1 = SampleCaustics(causticsUV1);
			caustics2 = SampleCaustics(causticsUV2);

			float3 causticsLow = sqrt(min(caustics1, caustics2));

			float3 caustics = lerp(causticsLow, causticsHigh, causticsFade);
			caustics = lerp(causticsLow, caustics, upAmount);

			return lerp(1.0, caustics, shoreFactorCaustics * upAmount);
		}

		return 1.0;
	}
}
