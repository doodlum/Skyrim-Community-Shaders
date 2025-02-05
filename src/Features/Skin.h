#pragma once

struct Skin : Feature
{
    static Skin* GetSingleton()
    {
        static Skin singleton;
        return &singleton;
    }

    virtual inline std::string GetName() override { return "Skin"; }
    virtual inline std::string GetShortName() override { return "Skin"; }
    virtual inline std::string_view GetShaderDefineName() override { return "PBR_SKIN"; }
    virtual inline bool HasShaderDefine(RE::BSShader::Type t) override
	{
		return t == RE::BSShader::Type::Lighting;
	};

    virtual inline bool SupportsVR() { return true; }

    virtual void RestoreDefaultSettings() override;
	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

    struct Settings
    {
        bool EnableSkin = true;
		float SkinMainRoughness = 0.6f;
		float SkinSecondRoughness = 0.4f;
		float SkinSpecularTexMultiplier = 1.7f;
        float SecondarySpecularStrength = 0.25f;
        float Thickness = 0.15f;
        float F01 = 0.028f;
        float F02 = 0.028f;
    } settings;

    struct alignas(16) SkinData
    {
        float4 skinParams;
        float4 skinParams2;
    };

    SkinData GetCommonBufferData();
};