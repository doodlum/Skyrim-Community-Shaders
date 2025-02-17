#include "Skin.h"
#include <DirectXTex.h>

#include "ShaderCache.h"
#include "State.h"
#include "Menu.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Skin::Settings,
    EnableSkin,
    SkinMainRoughness,
    SkinSecondRoughness,
    SkinSpecularTexMultiplier,
    SecondarySpecularStrength,
    Thickness,
    F0,
    SkinColorMultiplier,
    EnableSkinDetail,
    SkinDetailStrength,
    SkinDetailTiling,
    ApplySpecularToWetness,
    ExtraSkinWetness
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

    ImGui::SliderFloat("Fresnel F0", &settings.F0, 0.0f, 0.1f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Fresnel reflectance");
    }

    ImGui::SliderFloat("Skin Color Multiplier", &settings.SkinColorMultiplier, 0.0f, 5.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Multiplier for skin color (1.0 to keep original color)");
    }

    ImGui::Spacing();

    ImGui::SliderFloat("Extra Skin Wetness", &settings.ExtraSkinWetness, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Extra wetness for skin adding to wetness feature");
    }

    ImGui::Checkbox("Apply Specular to Wetness", &settings.ApplySpecularToWetness);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Applies specular texture to wetness feature instead of roughness (needs Wetness Effects enabled)");
    }

    ImGui::Checkbox("Enable Skin Detail", &settings.EnableSkinDetail);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Enable skin detail texture");
    }

    ImGui::SliderFloat("Skin Detail Strength", &settings.SkinDetailStrength, 0.0f, 1.0f);
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Strength of skin detail texture");
    }

    ImGui::SliderFloat("Skin Detail Tiling", &settings.SkinDetailTiling, 0.0f, 100.0f, "%.1f");
    if (auto _tt = Util::HoverTooltipWrapper()) {
        ImGui::Text("Tiling of skin detail texture");
    }

    if (ImGui::Button("Reload Skin Detail Texture")) {
        ReloadSkinDetail();
    }

    BUFFER_VIEWER_NODE(texSkinDetail, 1.0f)
}

void Skin::SetupResources()
{
	auto device = globals::d3d::device;

    logger::debug("Loading skin detail texture...");
    {
        DirectX::ScratchImage image;
        try {
            std::filesystem::path path{ "Data\\Shaders\\Skin\\skin_detail_n.dds" };

            DX::ThrowIfFailed(LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
		} catch (const DX::com_exception& e) {
			logger::error("{}", e.what());
			return;
		}

        ID3D11Resource* pResource = nullptr;
		try {
			DX::ThrowIfFailed(CreateTexture(device,
				image.GetImages(), image.GetImageCount(),
				image.GetMetadata(), &pResource));
		} catch (const DX::com_exception& e) {
			logger::error("{}", e.what());
			return;
		}

        texSkinDetail = eastl::make_unique<Texture2D>(reinterpret_cast<ID3D11Texture2D*>(pResource));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texSkinDetail->desc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1 }
		};
        texSkinDetail->CreateSRV(srvDesc);
    }
}

void Skin::ReloadSkinDetail()
{
    auto device = globals::d3d::device;

    logger::debug("Reloading skin detail texture...");
    {
        DirectX::ScratchImage image;
        try {
            std::filesystem::path path{ "Data\\Shaders\\Skin\\skin_detail_n.dds" };

            DX::ThrowIfFailed(LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
        } catch (const DX::com_exception& e) {
            logger::error("{}", e.what());
            return;
        }

        ID3D11Resource* pResource = nullptr;
        try {
            DX::ThrowIfFailed(CreateTexture(device,
                image.GetImages(), image.GetImageCount(),
                image.GetMetadata(), &pResource));
        } catch (const DX::com_exception& e) {
            logger::error("{}", e.what());
            return;
        }

        texSkinDetail = eastl::make_unique<Texture2D>(reinterpret_cast<ID3D11Texture2D*>(pResource));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
            .Format = texSkinDetail->desc.Format,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = 1 }
        };
        texSkinDetail->CreateSRV(srvDesc);
    }
}

void Skin::Prepass()
{
    auto context = globals::d3d::context;

    if (texSkinDetail) {
        ID3D11ShaderResourceView* srv = texSkinDetail->srv.get();
        context->PSSetShaderResources(72, 1, &srv);
    }
}

Skin::SkinData Skin::GetCommonBufferData()
{
    SkinData data{};
    data.skinParams = float4(settings.SkinMainRoughness, settings.SkinSecondRoughness, settings.SkinSpecularTexMultiplier, float(settings.EnableSkin));
    data.skinParams2 = float4(settings.SecondarySpecularStrength, settings.Thickness, settings.F0, settings.SkinColorMultiplier);
    data.skinDetailParams = float4(settings.SkinDetailTiling, settings.ExtraSkinWetness, settings.SkinDetailStrength, float(settings.EnableSkinDetail));
    data.ApplySpecularToWetness = uint(settings.ApplySpecularToWetness);
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