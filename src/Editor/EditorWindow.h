#pragma once

#include "Buffer.h"

#include "WeatherUtils.h"
#include "Weather/LightingTemplateWidget.h"
#include "Weather/WeatherWidget.h"
#include "Weather/WorldSpaceWidget.h"
#include "Widget.h"

class EditorWindow
{
public:
	static EditorWindow* GetSingleton()
	{
		static EditorWindow singleton;
		return &singleton;
	}

	bool open = false;

	Texture2D* tempTexture;

	std::vector<Widget*> weatherWidgets;
	std::vector<Widget*> worldSpaceWidgets;
	std::vector<Widget*> lightingTemplateWidgets;

	RE::TESWeather* originalWeather;

	WeatherWidget* preview;
	WeatherWidget* previewWeather2;

	float lerpValue;
	
	std::string weatherName1 = "Select weather";
	std::string weatherName2 = "Select weather";


	void ShowObjectsWindow();

	void ShowViewportWindow();

	void ShowWidgetWindow();

	void ShowPreviewWindow();

	void DisplayDALC(std::string, RE::BGSDirectionalAmbientLightingColors::Directional::MaxMin<RE::Color>*);

	void RenderUI();

	void SetupResources();

	void Draw();

private:
	void SaveAll();
};