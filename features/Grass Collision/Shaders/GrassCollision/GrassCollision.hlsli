namespace GrassCollision
{
	struct CollisionData
	{
		float4 centre[2];
	};

	cbuffer GrassCollisionPerFrame : register(b5)
	{
		CollisionData collisionData[256];
		uint numCollisions;
	}

	Texture2D<float2> Collision : register(t100);

	void ClampDisplacement(inout float3 displacement, float maxLength)
	{
		float lengthSq = displacement.x * displacement.x +
		                 displacement.y * displacement.y +
		                 displacement.z * displacement.z;

		if (lengthSq > maxLength * maxLength)  // Compare squared values for performance
		{
			float length = sqrt(lengthSq);
			float scale = maxLength / length;

			displacement.x *= scale;
			displacement.y *= scale;
			displacement.z *= scale;
		}
	}

	float2 SampleHeightField(float2 worldPosXY)
	{
		float2 uv = (worldPosXY / 2048.0) * 0.5 + 0.5;
		float2 texSize = float2(1024.0, 1024.0);
		
		// Convert UV to texture space
		float2 texCoord = uv * texSize;
		
		// Get integer and fractional parts
		int2 texel = int2(floor(texCoord));
		float2 f = frac(texCoord);

		// Sample four neighboring texels
		float2 v00 = Collision.Load(int3(texel, 0));
		float2 v10 = Collision.Load(int3(texel + int2(1, 0), 0));
		float2 v01 = Collision.Load(int3(texel + int2(0, 1), 0));
		float2 v11 = Collision.Load(int3(texel + int2(1, 1), 0));

		// Bilinear interpolation
		float2 v0 = lerp(v00, v10, f.x);
		float2 v1 = lerp(v01, v11, f.x);
		float2 v = lerp(v0, v1, f.y);

		return v;
	}

	float easeInOutElastic(float x) 
	{
		const float c5 = (2 * Math::PI) / 4.5;

		return x == 0
		? 0
		: x == 1
		? 1
		: x < 0.5
		? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
		: (pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1;
	}

	float SampleHeight(float2 worldPosXY, float2 offset = 0.0)
	{
		float2 uv = (worldPosXY / 2048.0) * 0.5 + 0.5;
		float2 texSize = float2(1024.0, 1024.0);
		
		// Convert UV to texture space
		float2 texCoord = uv * texSize;
		
		// Get integer and fractional parts
		int2 texel = int2(floor(texCoord));
		float2 f = frac(texCoord);

		// Sample four neighboring texels
		float2 v00 = Collision.Load(int3(texel + offset, 0));
		float2 v10 = Collision.Load(int3(texel + int2(1, 0) + offset, 0));
		float2 v01 = Collision.Load(int3(texel + int2(0, 1) + offset, 0));
		float2 v11 = Collision.Load(int3(texel + int2(1, 1) + offset, 0));

		// Bilinear interpolation
		float2 v0 = lerp(v00, v10, f.x);
		float2 v1 = lerp(v01, v11, f.x);
		float2 v = lerp(v0, v1, f.y);

		return v;
	}

	float3 ComputeNormal(float2 worldPosXY)
	{
		float hL = SampleHeight(worldPosXY, -float2(1, 0));
		float hR = SampleHeight(worldPosXY, +float2(1, 0));
		float hD = SampleHeight(worldPosXY, -float2(0, 1));
		float hU = SampleHeight(worldPosXY, +float2(0, 1));
		float3 normal = normalize(float3(hL - hR, hD - hU, 2.0));

		return normal;
	}

	float3 GetDisplacedPosition(VS_INPUT input, float3 position, uint eyeIndex = 0)
	{
		float3 worldPosition = mul(World[eyeIndex], float4(position, 1.0)).xyz;

		float alpha = saturate(input.Color.w * 10);

		if (length(worldPosition) < 2048.0 && alpha > 0.0) {
			float3 displacement = 0.0;

			float2 heightField = SampleHeightField(worldPosition.xy);
			
			const float fadeRate = 100.0;
			
			float collisionAge = saturate((heightField.y - heightField.x) / fadeRate);
			float collisionAnimation = easeInOutElastic(1.0 - collisionAge);

			float trampleAmount = max(0, worldPosition.z - heightField.x);

			float3 normal = ComputeNormal(worldPosition.xy);

			float bendability = saturate((position.z - input.InstanceData1.z) * 0.1);

			displacement -= normal * trampleAmount * collisionAnimation * bendability;

			return displacement * saturate(alpha * 10);
		}

		return 0.0;
	}
}
