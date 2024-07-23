#pragma once

#include "Feature.h"

struct GrassLighting : Feature
{
	static GrassLighting* GetSingleton()
	{
		static GrassLighting singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Grass Lighting"; }
	virtual inline std::string GetShortName() override { return "GrassLighting"; }

	struct alignas(16) Settings
	{
		float Glossiness = 20.0f;
		float SpecularStrength = 0.5f;
		float SubsurfaceScatteringAmount = 1.0f;
		uint OverrideComplexGrassSettings = false;
		float BasicGrassBrightness = 1.0f;
		uint pad[3];
	};

	Settings settings;

	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;

	virtual bool SupportsVR() override { return true; };
};
