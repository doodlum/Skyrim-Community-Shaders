// ExtendedTranslucency::MaterialModel
#define _ExtendedTranslucency_MaterialModel_Disabled 0
#define _ExtendedTranslucency_MaterialModel_RimLight 1
#define _ExtendedTranslucency_MaterialModel_IsotropicFabric 2
#define _ExtendedTranslucency_MaterialModel_AnisotropicFabric 3

// ExtendedTranslucency::MaterialParams
cbuffer ExtendedTranslucencyPerGeometry : register(b7)
{
	uint AnisotropicAlphaFlags;			// [0,1,2,3] The MaterialModel
	float AnisotropicAlphaReduction;	// [0, 1.0] The factor to reduce the transparency to matain the average transparency [0,1]
	float AnisotropicAlphaSoftness;		// [0, 2.0] The soft remap upper limit [0,2]
	float AnisotropicAlphaStrength;		// [0, 1.0] The inverse blend weight of the effect
};

namespace ExtendedTransclucency
{
	namespace MaterialModel
	{
		static const uint Disabled = 0;
		static const uint RimLight = 1;
		static const uint IsotropicFabric = 2;
		static const uint AnisotropicFabric = 3;
	}

	float GetViewDependentAlphaNaive(float alpha, float3 view, float3 normal)
	{
		return 1.0 - (1.0 - alpha) * dot(view, normal);
	}

	float GetViewDependentAlphaFabric1D(float alpha, float3 view, float3 normal)
	{
		return alpha / min(1.0, (abs(dot(view, normal)) + 0.001));
	}

	float GetViewDependentAlphaFabric2D(float alpha, float3 view, float3x3 tbnTr)
	{
		float3 t = tbnTr[0];
		float3 b = tbnTr[1];
		float3 n = tbnTr[2];
		float3 v = view;
		float a0 = 1 - sqrt(1.0 - alpha);
		return a0 * (length(cross(v, t)) + length(cross(v, b))) / (abs(dot(v, n)) + 0.001) - a0 * a0;
	}

	float SoftClamp(float alpha, float limit)
	{
		// soft clamp [alpha,1] and remap the transparency
		alpha = min(alpha, limit / (1 + exp(-4 * (alpha - limit * 0.5) / limit)));
		return saturate(alpha);
	}
}
