#include "SnowCover/FastNoiseLite.hlsl"

float MyHash11(float p)
{
	return frac(sin(p) * 1e4);
}

// https://blog.selfshadow.com/publications/blending-in-detail/
// for when s = (0,0,1)
float3 MyReorientNormal(float3 n1, float3 n2)
{
	n1 += float3(0, 0, 1);
	n2 *= float3(-1, -1, 1);

	return n1 * dot(n1, n2) / n1.z - n2;
}

// stolen from wetness effects
float SnowNoise(float3 pos)
{
	// https://github.com/BelmuTM/Noble/blob/master/LICENSE.txt

	const float3 step = float3(110.0, 241.0, 171.0);
	float3 i = floor(pos);
	float3 f = frac(pos);
	float n = dot(i, step);

	float3 u = f * f * (3.0 - 2.0 * f);
	return lerp(lerp(lerp(MyHash11(n + dot(step, float3(0.0, 0.0, 0.0))), MyHash11(n + dot(step, float3(1.0, 0.0, 0.0))), u.x),
					lerp(MyHash11(n + dot(step, float3(0.0, 1.0, 0.0))), MyHash11(n + dot(step, float3(1.0, 1.0, 0.0))), u.x), u.y),
		lerp(lerp(MyHash11(n + dot(step, float3(0.0, 0.0, 1.0))), MyHash11(n + dot(step, float3(1.0, 0.0, 1.0))), u.x),
			lerp(MyHash11(n + dot(step, float3(0.0, 1.0, 1.0))), MyHash11(n + dot(step, float3(1.0, 1.0, 1.0))), u.x), u.y),
		u.z);
}

void ApplySnowSimple(inout float3 color, inout float3 worldNormal, inout float glossiness, inout float shininess, float3 worldPos, float skylight)
{
	fnl_state noise = fnlCreateState();
	noise.noise_type = FNL_NOISE_VALUE_CUBIC;
	float v = fnlGetNoise2D(noise, worldPos.x * 512, worldPos.y * 512);
	noise.octaves = 1;
	float mult = saturate(pow(abs(worldNormal.z), 0.5) - 0.25 * abs(v)) * skylight;
	//float mult = skylight;
	color = lerp(color, 0.35 + v * 0.05, mult);
	//color = worldNormal*0.5+0.5;
	glossiness = lerp(glossiness, 0.5 * pow(v, 3.0), mult);
	shininess = lerp(shininess, max(1, pow(1 - v, 3.0) * 100), mult);
	worldNormal = normalize(lerp(worldNormal, float3(0, 0, 1.0), mult));
}

float ApplySnowBase(inout float3 color, inout float3 worldNormal, float3 worldPos, float skylight, float3 viewPos, out float vnoise, out float snoise){
	float viewDist = max(1, (viewPos.z+(sin(viewPos.x*7+viewPos.z*13)))/512);
	fnl_state noise = fnlCreateState();
	noise.noise_type = FNL_NOISE_VALUE_CUBIC;
	noise.fractal_type = FNL_FRACTAL_PINGPONG;
	noise.ping_pong_strength = 1.0;
	noise.octaves = max(1, (2 / viewDist));
	float v = fnlGetNoise2D(noise, worldPos.x * 512, worldPos.y * 512) / viewDist;
	noise.fractal_type = FNL_FRACTAL_FBM;
	noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
	noise.octaves = max(1, (5 / viewDist));
	float simplex_scale = 1;
	float s = fnlGetNoise2D(noise, worldPos.x*simplex_scale, worldPos.y*simplex_scale)/viewDist;
	float sx = fnlGetNoise2D(noise, worldPos.x*simplex_scale+1, worldPos.y*simplex_scale)/viewDist;
	float sy = fnlGetNoise2D(noise, worldPos.x*simplex_scale, worldPos.y*simplex_scale+1)/viewDist;
	float mult = saturate(pow(worldNormal.z,0.5)-0.15*sx)*skylight;
	//float mult = 1;
	vnoise = (v)*0.5 + 0.5;
	snoise = s * 0.5 + 0.5;
	color = lerp(color, 0.35 + v * 0.05 + s * 0.001, mult);
	//color = 1/viewDist;
	//color = worldNormal*0.5+0.5;
	worldNormal = normalize(lerp(worldNormal, float3(sx - s, sy - s, 1.0), mult));
	//worldNormal = float3(0,0,1);
	//worldNormal = normalize(lerp(worldNormal, MyReorientNormal(worldNormal, float3(sx-s, sy-s,1.0)), mult));
	return mult;
}
#	if defined(TRUE_PBR)
void ApplySnowPBR(inout float3 color, inout float3 worldNormal, inout PBRSurfaceProperties prop, float3 worldPos, float skylight, float3 viewPos){
	float r;
	float s;
	color = sRGB2Lin(color);//makes no sense but matches vanilla better lol
	float mult = ApplySnowBase(color, worldNormal, worldPos, skylight, viewPos, r, s);
	color = Lin2sRGB(color);
	prop.Metallic *= mult;
	prop.Roughness = lerp(prop.Roughness, 0.9 - 0.6 * pow(r * s, 3.0), mult);
	prop.F0 = lerp(prop.F0, 0.02 + 0.02 * pow(1 - r, 3.0), mult);
	prop.AO = lerp(prop.AO, saturate(max(pow(0.5 * s, 0.5) + 0.5, r)), mult);
}
#	else
void ApplySnow(inout float3 color, inout float3 worldNormal, inout float glossiness, inout float shininess, float3 worldPos, float skylight, float3 viewPos){
	float r;
	float s;
	//color = sRGB2Lin(color);
	float mult = ApplySnowBase(color, worldNormal, worldPos, skylight, viewPos, r, s);
	//color = Lin2sRGB(color);
	glossiness = lerp(glossiness, 0.5 * pow(r * s, 3.0), mult);
	shininess = lerp(shininess, max(1, pow(1 - r, 3.0) * 100), mult);
}
#endif