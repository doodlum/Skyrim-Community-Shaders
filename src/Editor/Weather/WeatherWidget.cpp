#include "WeatherWidget.h"

#include "../EditorWindow.h"
#include <algorithm>

#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeatherWidget::Settings,
	currentParentBuffer,
	windSpeed,
	windDirection,
	sunGlare,
	sunDamage,
	transDelta,
	sunGlare,
	sunDamage,
	precipitationBeginFadeIn,
	precipitationEndFadeOut,
	thunderLightningBeginFadeIn,
	thunderLightningEndFadeOut,
	thunderLightningFrequency,
	visualEffectBegin,
	visualEffectEnd,
	windDirection,
	windDirectionRange,

	lightningColor[3],

	inheritWindSettings,
	inheritSunSettings,
	inheritPrecipitationSettings,
	inheritLightningSettings,
	inheritVisualEffectSettings,
	inheritTransDeltaSettings)

WeatherWidget::~WeatherWidget()
{
}

void WeatherWidget::DrawWidget()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(600, 0), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::Begin(GetEditorID().c_str(), &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
		DrawMenu();

		auto editorWindow = EditorWindow::GetSingleton();
		auto& widgets = editorWindow->weatherWidgets;

		// Sets the parent widget if settings have been loaded.
		if (settings.currentParentBuffer != "None") {
			parent = GetParent();
			if (parent == nullptr)
				settings.currentParentBuffer = "None";
			strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
		}

		if (ImGui::BeginCombo("Parent", currentParentBuffer)) {
			// Option for "None"
			if (ImGui::Selectable("None", parent == nullptr)) {
				parent = nullptr;
				settings.currentParentBuffer = "None";
				strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
			}

			for (int i = 0; i < widgets.size(); i++) {
				auto& widget = widgets[i];

				// Skip self-selection
				if (widget == this)
					continue;

				// Option for each widget
				if (ImGui::Selectable(widget->GetEditorID().c_str(), parent == widget)) {
					parent = (WeatherWidget*)widget;
					settings.currentParentBuffer = widget->GetEditorID();
					strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer) - 1);
					currentParentBuffer[sizeof(currentParentBuffer) - 1] = '\0';  // Ensure null-termination
				}

				// Set default focus to the current parent
				if (parent == widget) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (parent && !parent->IsOpen()) {
			ImGui::SameLine();
			if (ImGui::Button("Open"))
				parent->SetOpen(true);
		}

		if (ImGui::CollapsingHeader("Sun settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			ImGui::Checkbox("Inherit From Parent##sun", &settings.inheritSunSettings);

			if (settings.inheritSunSettings && HasParent()) {
				settings.sunGlare = GetParent()->settings.sunGlare;
				settings.sunDamage = GetParent()->settings.sunDamage;
			} else {
				settings.inheritSunSettings = false;
				ImGui::SliderInt("Sun Glare", &settings.sunGlare, -128, 127);
				ImGui::SliderInt("Sun Damage", &settings.sunDamage, -128, 127);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui:
		if (ImGui::CollapsingHeader("Wind settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			ImGui::Checkbox("Inherit From Parent##wind", &settings.inheritWindSettings);

			if (settings.inheritWindSettings && HasParent()) {
				settings.windSpeed = GetParent()->settings.windSpeed;
				settings.windDirection = GetParent()->settings.windDirection;
				settings.windDirectionRange = GetParent()->settings.windDirectionRange;
			} else {
				settings.inheritWindSettings = false;
				ImGui::SliderInt("Wind Direction", &settings.windDirection, -128, 127);
				ImGui::SliderInt("Wind Direction Range", &settings.windDirectionRange, -128, 127);
				ImGui::SliderInt("Wind Speed", &settings.windSpeed, 0, 255);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Precipitation settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			ImGui::Checkbox("Inherit From Parent##precipitation", &settings.inheritPrecipitationSettings);

			if (settings.inheritPrecipitationSettings && HasParent()) {
				settings.precipitationBeginFadeIn = GetParent()->settings.precipitationBeginFadeIn;
				settings.precipitationEndFadeOut = GetParent()->settings.precipitationEndFadeOut;
			} else {
				settings.inheritPrecipitationSettings = false;
				ImGui::SliderInt("Precipitation Begin Fade In", &settings.precipitationBeginFadeIn, -128, 127);
				ImGui::SliderInt("Precipitation End Fade Out", &settings.precipitationEndFadeOut, -128, 127);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Lightning settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			ImGui::Checkbox("Inherit From Parent##lightning", &settings.inheritLightningSettings);

			if (settings.inheritLightningSettings && HasParent()) {
				settings.thunderLightningBeginFadeIn = GetParent()->settings.thunderLightningBeginFadeIn;
				settings.thunderLightningEndFadeOut = GetParent()->settings.thunderLightningEndFadeOut;
				settings.thunderLightningFrequency = GetParent()->settings.thunderLightningFrequency;

				settings.lightningColor[0] = GetParent()->settings.lightningColor[0];
				settings.lightningColor[1] = GetParent()->settings.lightningColor[1];
				settings.lightningColor[2] = GetParent()->settings.lightningColor[2];
			} else {
				settings.inheritLightningSettings = false;
				ImGui::SliderInt("Thunder Lightning Begin Fade In", &settings.thunderLightningBeginFadeIn, -128, 127);
				ImGui::SliderInt("Thunder Lightning End Fade Out", &settings.thunderLightningEndFadeOut, -128, 127);
				ImGui::SliderInt("Thunder Lightning Frequency", &settings.thunderLightningFrequency, -128, 127);
				ImGui::ColorEdit3("Lightning Color", settings.lightningColor);
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Visual Effect settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			ImGui::Checkbox("Inherit From Parent##visualEffect", &settings.inheritVisualEffectSettings);

			if (settings.inheritVisualEffectSettings && HasParent()) {
				settings.visualEffectBegin = GetParent()->settings.visualEffectBegin;
				settings.visualEffectBegin = GetParent()->settings.visualEffectEnd;
			} else {
				settings.inheritVisualEffectSettings = false;
				ImGui::SliderInt("Visual Effect Begin", &settings.visualEffectBegin, -128, 127);
				ImGui::SliderInt("Visual Effect End", &settings.visualEffectEnd, -128, 127);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Checkbox("Inherit From Parent##transDelta", &settings.inheritTransDeltaSettings);
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::SliderInt("Trans Delta", &settings.transDelta, -128, 127);

		if (settings.inheritTransDeltaSettings && HasParent()) {
			settings.transDelta = GetParent()->settings.transDelta;
		}

		SetWeatherValues();
	}
	ImGui::End();
}

void WeatherWidget::LoadSettings()
{
	if (!j.empty()) {
		settings = j;
		strncpy(currentParentBuffer, settings.currentParentBuffer.c_str(), sizeof(settings.currentParentBuffer));
		currentParentBuffer[sizeof(currentParentBuffer) - 1] = '\0';  // Ensure null-termination
		SetWeatherValues();
	}
}

void WeatherWidget::SaveSettings()
{
	j = settings;
}

WeatherWidget* WeatherWidget::GetParent()
{
	auto editorWindow = EditorWindow::GetSingleton();
	auto& widgets = editorWindow->weatherWidgets;

	auto temp = std::find_if(widgets.begin(), widgets.end(), [&](Widget* w) { return w->GetEditorID() == settings.currentParentBuffer; });
	if (temp != widgets.end())
		return (WeatherWidget*)*temp;

	return nullptr;
}

bool WeatherWidget::HasParent()
{
	return settings.currentParentBuffer != "None";
}

const int WeatherWidget::GetSetting(const std::string& settingName)
{
	j = settings;

	try {
		int setting = j[setting];

		if (setting == -128) {
			return GetParent()->GetSetting(settingName);
		}
		return setting;
	} catch (const nlohmann::json::parse_error&) {
		logger::warn("{} setting does not exist in WeatherWidget", settingName);
	}
}

void WeatherWidget::SetWeatherValues()
{
	weather->data.windSpeed = (uint8_t)settings.windSpeed;
	weather->data.transDelta = (int8_t)settings.transDelta;
	weather->data.sunGlare = (int8_t)settings.sunGlare;
	weather->data.sunDamage = (int8_t)settings.sunDamage;
	weather->data.precipitationBeginFadeIn = (int8_t)settings.precipitationBeginFadeIn;
	weather->data.precipitationEndFadeOut = (int8_t)settings.precipitationEndFadeOut;
	weather->data.thunderLightningBeginFadeIn = (int8_t)settings.thunderLightningBeginFadeIn;
	weather->data.thunderLightningEndFadeOut = (int8_t)settings.thunderLightningEndFadeOut;
	weather->data.thunderLightningFrequency = (int8_t)settings.thunderLightningFrequency;
	weather->data.visualEffectBegin = (int8_t)settings.visualEffectBegin;
	weather->data.visualEffectEnd = (int8_t)settings.visualEffectEnd;
	weather->data.windDirection = (int8_t)settings.windDirection;
	weather->data.windDirectionRange = (int8_t)settings.windDirectionRange;
}

void WeatherWidget::LoadWeatherValues()
{
	settings.windSpeed = weather->data.windSpeed;
	settings.transDelta = weather->data.transDelta;
	settings.sunGlare = weather->data.sunGlare;
	settings.sunDamage = weather->data.sunDamage;
	settings.precipitationBeginFadeIn = weather->data.precipitationBeginFadeIn;
	settings.precipitationEndFadeOut = weather->data.precipitationEndFadeOut;
	settings.thunderLightningBeginFadeIn = weather->data.thunderLightningBeginFadeIn;
	settings.thunderLightningEndFadeOut = weather->data.thunderLightningEndFadeOut;
	settings.thunderLightningFrequency = weather->data.thunderLightningFrequency;
	settings.visualEffectBegin = weather->data.visualEffectBegin;
	settings.visualEffectEnd = weather->data.visualEffectEnd;
	settings.windDirection = weather->data.windDirection;
	settings.windDirectionRange = weather->data.windDirectionRange;
}