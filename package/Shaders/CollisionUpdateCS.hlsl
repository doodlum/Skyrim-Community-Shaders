cbuffer CollisionUpdateCB : register(b0)
{
	float3 eyePositionDelta;
	float timeDelta;
}

struct CollisionData
{
	float4 centre[2];
};

cbuffer PerFrameCB : register(b1)
{
	CollisionData collisionData[256];
	uint numCollisions;
	uint pad0[3];
}

Texture2D<float2> previousCollision : register(t0);

RWTexture2D<float2> currentCollision : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(8, 8, 1)] void main(
	uint3 groupId
	: SV_GroupID, uint3 dispatchThreadId
	: SV_DispatchThreadID, uint3 groupThreadId
	: SV_GroupThreadID, uint groupIndex
	: SV_GroupIndex) {

	const float textureSize = 1024;

	float2 uv = float2(dispatchThreadId.xy + 0.5) / textureSize;
	
	float2 positionXY = (uv * 2.0 - 1.0) * 2048.0;

	// Fade on age

	float2 collisions = 0;

	float2 previousUv = positionXY + eyePositionDelta.xy;
	previousUv = (previousUv / 2048.0) * 0.5 + 0.5;

	if (saturate(previousUv.x) == previousUv.x && saturate(previousUv.y) == previousUv.y){
		collisions = previousCollision.SampleLevel(LinearSampler, previousUv, 0);
		//collisions.y += timeDelta;
	}

	for (uint i = 0; i < numCollisions; i++) {
 		float3 collisionCentre = collisionData[i].centre[0].xyz;
        float radius = collisionData[i].centre[0].w;
		
		// Compute distance in the XY plane
		float2 deltaXY = positionXY - collisionCentre.xy;
		float distXY = length(deltaXY);

		// Check if within the sphere's projection
		if (distXY <= radius) {
			// Compute the furthest Z at this (x, y)
			float furthestZDistance = collisionCentre.z - sqrt(radius * radius - distXY * distXY);

			// Compare and update
			if (furthestZDistance <= collisions.x) {
				collisions.xy = furthestZDistance;
			}
   		}
	}

	currentCollision[dispatchThreadId.xy] = collisions;
}