struct CollisionData
{
	float4 centre[2];
};

cbuffer GrassCollisionPerFrame : register(b5)
{
	CollisionData collisionData[256];
	uint numCollisions;
}

namespace GrassCollision
{
	float3 GetDisplacedPosition(float3 position, float alpha, uint eyeIndex = 0)
	{
		float3 worldPosition = mul(World[eyeIndex], float4(position, 1.0)).xyz;

		if (length(worldPosition) < 1024.0 && alpha > 0.0) {
			float3 displacement = 0.0;

			for (uint i = 0; i < numCollisions; i++) {
				float dist = distance(collisionData[i].centre[eyeIndex], worldPosition);
				float power = 1.0 - saturate(dist / collisionData[i].centre[0].w);
				float3 direction = worldPosition - collisionData[i].centre[eyeIndex];
				float3 shift = power * direction;
				displacement += shift;
				displacement.z -= length(shift.xy);
			}

			return displacement * alpha;
		}

		return 0.0;
	}
}
