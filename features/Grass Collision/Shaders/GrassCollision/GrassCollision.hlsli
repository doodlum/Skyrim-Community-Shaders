namespace GrassCollision
{
	struct Capsule
	{
		float4 TopPosRadius;
		float4 BottomPos;
	};

	cbuffer GrassCollisionPerFrame : register(b5)
	{
		Capsule Capsules[256];
		float4 CapsuleInfo;
	}
	
	float3 GetDisplacedPosition(float3 position, float alpha, uint eyeIndex = 0)
	{
		float3 worldPosition = mul(World[eyeIndex], float4(position, 1.0)).xyz;

		if (length(worldPosition) < 1024.0 && alpha > 0.0) {
			float3 displacement = 0.0;

			for (uint i = 0; i < CapsuleInfo.w; i++) {
				// Compute distance to the capsule center line
				float3 A = Capsules[i].BottomPos.xyz;
				float3 B = Capsules[i].TopPosRadius.xyz;
				float radius = Capsules[i].TopPosRadius.w * 4;
				
				// Compute closest point on the capsule center line
				float3 AP = worldPosition - A;
				float3 AB = B - A;
				float ab2 = dot(AB, AB);
				float ap_ab = dot(AP, AB);
				float t = clamp(ap_ab / ab2, 0.0, 1.0);
				float3 closestPoint = A + t * AB;

				// Compute distance from the closest point on the capsule
				float dist = distance(closestPoint, worldPosition);

				// Compute power based on capsule radius
				float power = 1.0 - saturate(dist / radius);

				// Compute displacement
				float3 direction = worldPosition - closestPoint;
				float3 shift = power * direction;
				displacement += shift;
				displacement.z -= length(shift.xy);
			}

			return displacement * sqrt(alpha);
		}

		return 0.0;
	}
}
