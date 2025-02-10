#pragma once

#include "Buffer.h"

#include "Weather/LightingTemplateWidget.h"
#include "Weather/WeatherWidget.h"
#include "Weather/WorldSpaceWidget.h"
#include "WeatherUtils.h"
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

	void ShowObjectsWindow();

	void ShowViewportWindow();

	void ShowWidgetWindow();

	void RenderUI();

	void SetupResources();

	void Draw();

private:
	void SaveAll();
};