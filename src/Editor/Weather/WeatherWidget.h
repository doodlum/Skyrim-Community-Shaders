#pragma once

#include "../Widget.h"

class WeatherWidget : public Widget
{
public:
	WeatherWidget* parent = nullptr;
	char currentParentBuffer[256] = "None";

	RE::TESWeather* weather = nullptr;

	WeatherWidget(RE::TESWeather* a_weather)
	{
		form = a_weather;
		weather = a_weather;
	}

	struct Settings
	{
		std::string currentParentBuffer;

		int windSpeed; // underlying type uint8_t
		
		// underlying type int8_t
		int transDelta;
		int sunGlare;
		int sunDamage;
		int precipitationBeginFadeIn;
		int precipitationEndFadeOut;
		int thunderLightningBeginFadeIn;
		int thunderLightningEndFadeOut;
		int thunderLightningFrequency;
		int visualEffectBegin;
		int visualEffectEnd;
		int windDirection;
		int windDirectionRange;

		float3 lightningColor;
	};

	Settings settings;

	~WeatherWidget();

	virtual void DrawWidget() override;
	virtual void LoadSettings() override;
	virtual void SaveSettings() override;

	WeatherWidget* GetParent();
	const int GetSetting(const std::string&);
};
