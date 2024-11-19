namespace WaterLighting
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PARALLAX
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// https://github.com/tgjones/slimshader-cpp/blob/master/src/Shaders/Sdk/Direct3D11/DetailTessellation11/POM.hlsl
	// https://github.com/alandtse/SSEShaderTools/blob/main/shaders_vr/ParallaxEffect.h

	// https://github.com/marselas/Zombie-Direct3D-Samples/blob/5f53dc2d6f7deb32eb2e5e438d6b6644430fe9ee/Direct3D/ParallaxOcclusionMapping/ParallaxOcclusionMapping.fx
	// http://www.diva-portal.org/smash/get/diva2:831762/FULLTEXT01.pdf
	// https://bartwronski.files.wordpress.com/2014/03/ac4_gdc.pdf

	float GetMipLevel(float2 coords, Texture2D<float4> tex)
	{
		// Compute the current gradients:
		float2 textureDims;
		tex.GetDimensions(textureDims.x, textureDims.y);

		textureDims /= 8.0;

		float2 texCoordsPerSize = coords * textureDims;

		float2 dxSize = ddx(texCoordsPerSize);
		float2 dySize = ddy(texCoordsPerSize);

		// Find min of change in u and v across quad: compute du and dv magnitude across quad
		float2 dTexCoords = dxSize * dxSize + dySize * dySize;

		// Standard mipmapping uses max here
		float minTexCoordDelta = max(dTexCoords.x, dTexCoords.y);

		// Compute the current mip level  (* 0.5 is effectively computing a square root before )
		float mipLevel = max(0.5 * log2(minTexCoordDelta), 0);

		return mipLevel;
	}

	float GetHeight(PS_INPUT input, float2 currentOffset, float3 normalScalesRcp, float3 mipLevels)
	{
		float3 heights;
		heights.x = Normals01Tex.SampleLevel(Normals01Sampler, input.TexCoord1.xy + currentOffset * normalScalesRcp.x, mipLevels.x).w;
		heights.y = Normals02Tex.SampleLevel(Normals02Sampler, input.TexCoord1.zw + currentOffset * normalScalesRcp.y, mipLevels.y).w;
		heights.z = Normals03Tex.SampleLevel(Normals03Sampler, input.TexCoord2.xy + currentOffset * normalScalesRcp.z, mipLevels.z).w;
		return 1.0 - length(heights);
	}

	// http://www.thetenthplanet.de/archives/1180
	float3x3 CalculateTBN(float3 N, float3 p, float2 uv)
	{
		// get edge vectors of the pixel triangle
		float3 dp1 = ddx_coarse(p);
		float3 dp2 = ddy_coarse(p);
		float2 duv1 = ddx_coarse(uv);
		float2 duv2 = ddy_coarse(uv);

		// solve the linear system
		float3 dp2perp = cross(dp2, N);
		float3 dp1perp = cross(N, dp1);
		float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
		float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

		// construct a scale-invariant frame
		float invmax = rsqrt(max(dot(T, T), dot(B, B)));
		return float3x3(T * invmax, B * invmax, N);
	}

	float2 GetParallaxOffset(PS_INPUT input, float3 normalScalesRcp, uint eyeIndex)
	{
#if defined(VC)
		float3 reflectPlaneWS = mul(CameraViewInverse[eyeIndex], float4(ReflectPlane[eyeIndex].xyz, 1.0));

		float3x3 tbn = CalculateTBN(reflectPlaneWS.xyz, -input.WPosition.xyz, input.TexCoord1.xy);

		float3 viewDirection = normalize(input.WPosition.xyz);
		float3 viewDirectionTS = normalize(mul(tbn, viewDirection));

		float2 parallaxOffsetTS = viewDirectionTS.xy / -viewDirectionTS.z;
#else
		float3 viewDirection = normalize(input.WPosition.xyz);
		float2 parallaxOffsetTS = viewDirection.xy / -viewDirection.z;
#endif
		// Parallax scale is also multiplied by normalScalesRcp
		parallaxOffsetTS *= 20.0;

		float3 mipLevels;
		mipLevels.x = GetMipLevel(input.TexCoord1.xy, Normals01Tex);
		mipLevels.y = GetMipLevel(input.TexCoord1.zw, Normals02Tex);
		mipLevels.z = GetMipLevel(input.TexCoord2.xy, Normals03Tex);
		mipLevels = mipLevels + 3;

		float stepSize = rcp(16);
		float currBound = 0.0;
		float currHeight = 1.0;
		float prevHeight = 1.0;

		[loop] while (currHeight > currBound)
		{
			prevHeight = currHeight;
			currBound += stepSize;
			currHeight = GetHeight(input, currBound * parallaxOffsetTS.xy, normalScalesRcp, mipLevels);
		}

		float prevBound = currBound - stepSize;

		float delta2 = prevBound - prevHeight;
		float delta1 = currBound - currHeight;
		float denominator = delta2 - delta1;
		float parallaxAmount = (currBound * delta2 - prevBound * delta1) / denominator;

		return parallaxOffsetTS.xy * parallaxAmount;
	}
}
