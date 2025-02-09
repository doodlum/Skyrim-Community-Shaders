#include "Common/PBR.hlsli"
#include "Common/Math.hlsli"

namespace Skin{
    struct SkinSurfaceProperties
    {
        float RoughnessPrimary;
        float RoughnessSecondary;
        float3 F0;
        float SecondarySpecIntensity;
        float CurvatureScale;
        float3 Albedo;
        float Thickness;
    };

    SkinSurfaceProperties InitSkinSurfaceProperties()
    {
        SkinSurfaceProperties skin;
        skin.RoughnessPrimary = 0.55;
        skin.RoughnessSecondary = 0.35;
        skin.F0 = float3(0.0277, 0.0277, 0.0277);
        skin.SecondarySpecIntensity = 0.15;
        skin.CurvatureScale = 1.0;
        skin.Albedo = float3(0.8, 0.6, 0.5);
        skin.Thickness = 0.15;
        return skin;
    }

    float CalculateCurvature(float3 N)
    {
        const float3 dNdx = ddx(N);
        const float3 dNdy = ddy(N);
        return length(float2(dot(dNdx, dNdx), dot(dNdy, dNdy)));
    }

    float DisneyDiffuse(float NdotV, float NdotL, float VdotH, float roughness)
    {
        const float FD90 = 0.5 + 2.0 * VdotH * VdotH * roughness;
        const float fdv = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotV, 5.0);
        const float fdl = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotL, 5.0);
        
        return fdv * fdl;
    }

    float3 GetDualSpecularGGX(float AverageRoughness ,float Lobe0Roughness, float Lobe1Roughness, float LobeMix, float3 SpecularColor, float NdotL, float NdotV, float NdotH, float VdotH, out float3 F) {

        float D = lerp(PBR::GetNormalDistributionFunctionGGX(Lobe0Roughness, NdotH), PBR::GetNormalDistributionFunctionGGX(Lobe1Roughness, NdotH), LobeMix);
        float G = PBR::GetVisibilityFunctionSmithJointApprox(AverageRoughness ,NdotV, NdotL);
        F = PBR::GetFresnelFactorSchlick(SpecularColor, VdotH);

        return D * G * F;
    }

    void SkinDirectLightInput(
        out float3 diffuse,
        out float3 specular,
        PBR::LightProperties light,
        SkinSurfaceProperties skin,
        float3 N, float3 V, float3 L)
    {
        diffuse = 0;
        specular = 0;

        const float3 H = normalize(V + L);
        const float NdotL = clamp(dot(N, L), 1e-5, 1.0);
        const float NdotV = saturate(abs(dot(N, V)) + 1e-5);
        const float NdotH = saturate(dot(N, H));
        const float VdotH = saturate(dot(V, H));

        float averageRoughness = lerp(skin.RoughnessPrimary, skin.RoughnessSecondary, skin.SecondarySpecIntensity);

        diffuse += light.LinearLightColor * NdotL * DisneyDiffuse(NdotV, NdotL, VdotH, averageRoughness) / Math::PI;

        float3 F;

        specular += GetDualSpecularGGX(averageRoughness, skin.RoughnessPrimary, skin.RoughnessSecondary, skin.SecondarySpecIntensity, skin.F0, NdotL, NdotV, NdotH, VdotH, F);

        float2 specularBRDF = PBR::GetEnvBRDFApproxLazarov(averageRoughness, NdotV);
        specular *= 1 + skin.F0 * (1 / (specularBRDF.x + specularBRDF.y) - 1);
    }

    void SkinIndirectLobeWeights(
        out float3 diffuseWeight,
        out float3 specularWeight,
        SkinSurfaceProperties skin,
        float3 N, float3 V, float3 VN)
    {
        const float NdotV = saturate(dot(N, V));
        
        float averageRoughness = lerp(skin.RoughnessPrimary, skin.RoughnessSecondary, skin.SecondarySpecIntensity);

        float2 specularBRDF = PBR::GetEnvBRDFApproxLazarov(averageRoughness, NdotV);
        specularWeight = skin.F0 * specularBRDF.x + specularBRDF.y;

        diffuseWeight = skin.Albedo * (1.0 - specularWeight);
        
        const float curvature = CalculateCurvature(N);
        specularWeight *= 1.0 - saturate(curvature * skin.CurvatureScale);

        specularWeight *= 1 + skin.F0 * (1 / (specularBRDF.x + specularBRDF.y) - 1);
        float3 R = reflect(-V, N);
        float horizon = min(1.0 + dot(R, VN), 1.0);
        horizon *= horizon;
        specularWeight *= horizon;
    }
}