#pragma once

#include "../Widget.h"

class WeatherWidget : public Widget
{
public:
	RE::TESWeather* weather = nullptr;
	float color[3];

	WeatherWidget(RE::TESWeather* a_weather)
	{
		weather = a_weather;

		auto& sunlight = weather->colorData[RE::TESWeather::ColorTypes::kSunlight][RE::TESWeather::ColorTimes::kDay];
		color[0] = float(sunlight.red) / 255.0f;
		color[1] = float(sunlight.green) / 255.0f;
		color[2] = float(sunlight.blue) / 255.0f;
	}

	virtual std::string GetName() override;

	virtual void DrawWidget() override;
};