#include "Skin.h"

#include "ShaderCache.h"
#include "State.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Skin::Settings,
    EnableSkin,
    SkinMainRoughness,
    SkinSecondRoughness,
    SkinSpecularTexMultiplier,
    SecondarySpecularStrength,
    Thickness,
    F01,
    F02
)

void Skin::DrawSettings()
{
    ImGui::Checkbox("Enable Skin", &settings.EnableSkin);

    ImGui::Text("Advanced Skin Shader using dual specular lobes.");

    ImGui::Spacing();
    ImGui::SliderFloat("Primary Roughness", &settings.SkinMainRoughness, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Controls microscopic roughness of stratum corneum layer");
    }

    ImGui::SliderFloat("Secondary Roughness", &settings.SkinSecondRoughness, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Smoothness of epidermal cell layer reflections");
        ImGui::BulletText("Should be 30-50%% lower than Primary");
    }

    ImGui::SliderFloat("Specular Texture Multiplier", &settings.SkinSpecularTexMultiplier, 0.0f, 10.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Multiplier for specular map");
        ImGui::BulletText("A multiplier for the vanilla specular map, applied to the first layer's roughness");
    }

    ImGui::SliderFloat("Secondary Specular Strength", &settings.SecondarySpecularStrength, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Intensity of secondary specular highlights");
    }

    ImGui::SliderFloat("Thickness", &settings.Thickness, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Optical thickness for energy compensation");
    }

    ImGui::SliderFloat("Primary Fresnel F0", &settings.F01, 0.0f, 0.1f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Fresnel reflectance for stratum corneum layer");
    }

    ImGui::SliderFloat("Secondary Fresnel F0", &settings.F02, 0.0f, 0.1f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Fresnel reflectance for epidermal cell layer");
    }
}

Skin::SkinData Skin::GetCommonBufferData()
{
    SkinData data{};
    data.skinParams = float4(settings.SkinMainRoughness, settings.SkinSecondRoughness, settings.SkinSpecularTexMultiplier, float(settings.EnableSkin));
    data.skinParams2 = float4(settings.SecondarySpecularStrength, settings.Thickness, settings.F01, settings.F02);
    return data;
}

void Skin::LoadSettings(json& o_json)
{
    settings = o_json;
}

void Skin::SaveSettings(json& o_json)
{
    o_json = settings;
}

void Skin::RestoreDefaultSettings()
{
    settings = {};
}