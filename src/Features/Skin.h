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

    virtual void Prepass() override;

    virtual void SetupResources() override;

    void ReloadSkinDetail();

    struct Settings
    {
        bool EnableSkin = true;
		float SkinMainRoughness = 0.7f;
		float SkinSecondRoughness = 0.35f;
		float SkinSpecularTexMultiplier = 1.0f;
        float SecondarySpecularStrength = 0.15f;
        float Thickness = 0.15f;
        float F0 = 0.028f;
        float SkinColorMultiplier = 1.5f;
        bool EnableSkinDetail = true;
        float SkinDetailStrength = 0.15f;
        float SkinDetailTiling = 10.0f;
        bool ApplySpecularToWetness = false;
    } settings;

    struct alignas(16) SkinData
    {
        float4 skinParams;
        float4 skinParams2;
        float4 skinDetailParams;
        uint ApplySpecularToWetness;
        uint pad0[3];
    };

    eastl::unique_ptr<Texture2D> texSkinDetail = nullptr;

    SkinData GetCommonBufferData();
};