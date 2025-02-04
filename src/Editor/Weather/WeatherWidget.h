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
		LoadWeatherValues();
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

		float lightningColor[3];

		bool inheritWindSettings;
		bool inheritSunSettings;
		bool inheritPrecipitationSettings;
		bool inheritLightningSettings;
		bool inheritVisualEffectSettings;
		bool inheritTransDeltaSettings;
	};

	Settings settings;

	~WeatherWidget();

	virtual void DrawWidget() override;
	virtual void LoadSettings() override;
	virtual void SaveSettings() override;

	WeatherWidget* GetParent();
	bool HasParent();
	const int GetSetting(const std::string&);
	void SetWeatherValues();
	void LoadWeatherValues();
};
