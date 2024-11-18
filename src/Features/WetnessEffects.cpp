#include "WetnessEffects.h"

#include "Util.h"

const float MIN_START_PERCENTAGE = 0.05f;
const float DEFAULT_TRANSITION_PERCENTAGE = 1.0f;
const float TRANSITION_CURVE_MULTIPLIER = 2.0f;
const float TRANSITION_DENOMINATOR = 256.0f;
const float DRY_WETNESS = 0.0f;
const float RAIN_DELTA_PER_SECOND = 2.0f / 3600.0f;
const float SNOWY_DAY_DELTA_PER_SECOND = -0.489f / 3600.0f;  // Only doing evaporation until snow wetness feature is added
const float CLOUDY_DAY_DELTA_PER_SECOND = -0.735f / 3600.0f;
const float CLEAR_DAY_DELTA_PER_SECOND = -1.518f / 3600.0f;
const float WETNESS_SCALE = 2.0;  // Speed at which wetness builds up and drys.
const float PUDDLE_SCALE = 1.0;   // Speed at which puddles build up and dry
const float MAX_PUDDLE_DEPTH = 3.0f;
const float MAX_WETNESS_DEPTH = 2.0f;
const float MAX_PUDDLE_WETNESS = 1.0f;
const float MAX_WETNESS = 1.0f;
const float SECONDS_IN_A_DAY = 86400;
const float MAX_TIME_DELTA = SECONDS_IN_A_DAY - 30;
const float MIN_WEATHER_TRANSITION_SPEED = 0.0f;
const float MAX_WEATHER_TRANSITION_SPEED = 500.0f;
const float AVERAGE_RAIN_VOLUME = 4000.0f;
const float MIN_RAINDROP_CHANCE_MULTIPLIER = 0.1f;
const float MAX_RAINDROP_CHANCE_MULTIPLIER = 2.0f;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	WetnessEffects::Settings,
	EnableWetnessEffects,
	MaxRainWetness,
	MaxPuddleWetness,
	MaxShoreWetness,
	ShoreRange,
	PuddleRadius,
	PuddleMaxAngle,
	PuddleMinWetness,
	MinRainWetness,
	SkinWetness,
	WeatherTransitionSpeed,
	EnableRaindropFx,
	EnableSplashes,
	EnableRipples,
	EnableChaoticRipples,
	RaindropFxRange,
	RaindropGridSize,
	RaindropInterval,
	RaindropChance,
	SplashesLifetime,
	SplashesStrength,
	SplashesMinRadius,
	SplashesMaxRadius,
	RippleStrength,
	RippleRadius,
	RippleBreadth,
	RippleLifetime,
	ChaoticRippleStrength,
	ChaoticRippleScale,
	ChaoticRippleSpeed)

void WetnessEffects::DrawSettings()
{
	if (ImGui::TreeNodeEx("Wetness Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Wetness", (bool*)&settings.EnableWetnessEffects);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Enables a wetness effect near water and when it is raining.");
		}

		ImGui::SliderFloat("Rain Wetness", &settings.MaxRainWetness, 0.0f, 1.0f);
		ImGui::SliderFloat("Puddle Wetness", &settings.MaxPuddleWetness, 0.0f, 4.0f);
		ImGui::SliderFloat("Shore Wetness", &settings.MaxShoreWetness, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx("Raindrop Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Raindrop Effects", (bool*)&settings.EnableRaindropFx);

		ImGui::BeginDisabled(!settings.EnableRaindropFx);

		ImGui::Checkbox("Enable Splashes", (bool*)&settings.EnableSplashes);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Enables small splashes of wetness on dry surfaces.");
		ImGui::Checkbox("Enable Ripples", (bool*)&settings.EnableRipples);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Enables circular ripples on puddles, and to a less extent other wet surfaces");
		ImGui::Checkbox("Enable Chaotic Ripples", (bool*)&settings.EnableChaoticRipples);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Enables an additional layer of disturbance to wet surfaces.");

		ImGui::SliderFloat("Effect Range", &settings.RaindropFxRange, 1e2f, 2e3f, "%.0f game unit(s)");

		if (ImGui::TreeNodeEx("Raindrops")) {
			ImGui::BulletText(
				"At every interval, a raindrop is placed within each grid cell.\n"
				"Only a set portion of raindrops will actually trigger splashes and ripples.\n"
				"Chaotic ripples are not affected by raindrop settings.");

			ImGui::SliderFloat("Grid Size", &settings.RaindropGridSize, 1.f, 10.f, "%.1f game unit(s)");
			ImGui::SliderFloat("Interval", &settings.RaindropInterval, 0.1f, 2.f, "%.1f sec");
			ImGui::SliderFloat("Chance", &settings.RaindropChance, 0.0f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			if (auto _tt = Util::HoverTooltipWrapper())
				ImGui::Text("Portion of raindrops that will actually cause splashes and ripples.");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Splashes")) {
			ImGui::SliderFloat("Strength", &settings.SplashesStrength, 0.f, 2.f, "%.2f");
			ImGui::SliderFloat("Min Radius", &settings.SplashesMinRadius, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			if (auto _tt = Util::HoverTooltipWrapper())
				ImGui::Text("As portion of grid size.");
			ImGui::SliderFloat("Max Radius", &settings.SplashesMaxRadius, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			if (auto _tt = Util::HoverTooltipWrapper())
				ImGui::Text("As portion of grid size.");
			ImGui::SliderFloat("Lifetime", &settings.SplashesLifetime, 0.1f, 20.f, "%.1f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Ripples")) {
			ImGui::SliderFloat("Strength", &settings.RippleStrength, 0.f, 2.f, "%.2f");
			ImGui::SliderFloat("Radius", &settings.RippleRadius, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			if (auto _tt = Util::HoverTooltipWrapper())
				ImGui::Text("As portion of grid size.");
			ImGui::SliderFloat("Breadth", &settings.RippleBreadth, 0.f, 1.f, "%.2f");
			ImGui::SliderFloat("Lifetime", &settings.RippleLifetime, 0.f, settings.RaindropInterval, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Chaotic Ripples")) {
			ImGui::SliderFloat("Strength", &settings.ChaoticRippleStrength, 0.f, .5f, "%.2f");
			ImGui::SliderFloat("Scale", &settings.ChaoticRippleScale, 0.1f, 5.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Speed", &settings.ChaoticRippleSpeed, 0.f, 50.f, "%.1f");
			ImGui::TreePop();
		}

		ImGui::EndDisabled();

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Weather transition speed", &settings.WeatherTransitionSpeed, 0.5f, 5.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"How fast wetness appears when raining and how quickly it dries "
				"after rain has stopped. ");
		}

		ImGui::SliderFloat("Min Rain Wetness", &settings.MinRainWetness, 0.0f, 0.9f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The minimum amount an object gets wet from rain. ");
		}

		ImGui::SliderFloat("Skin Wetness", &settings.SkinWetness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"How wet character skin and hair get during rain. ");
		}

		ImGui::SliderInt("Shore Range", (int*)&settings.ShoreRange, 1, 64);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The maximum distance from a body of water that Shore Wetness affects. ");
		}

		ImGui::SliderFloat("Puddle Radius", &settings.PuddleRadius, 0.3f, 3.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The radius that is used to determine puddle size and location. ");
		}

		ImGui::SliderFloat("Puddle Max Angle", &settings.PuddleMaxAngle, 0.6f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"How flat a surface needs to be for puddles to form on it. ");
		}

		ImGui::SliderFloat("Puddle Min Wetness", &settings.PuddleMinWetness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The wetness value at which puddles start to form. ");
		}

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text(std::format("Wetness depth : {:.2f}", wetnessDepth / WETNESS_SCALE).c_str());
		ImGui::Text(std::format("Puddle depth : {:.2f}", puddleDepth / PUDDLE_SCALE).c_str());
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text(std::format("Current weather : {0:X}", currentWeatherID).c_str());
		ImGui::Text(std::format("Previous weather : {0:X}", lastWeatherID).c_str());
		ImGui::TreePop();
	}
}

float WetnessEffects::CalculateWeatherTransitionPercentage(float skyCurrentWeatherPct, float beginFade, bool fadeIn)
{
	float weatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
	// Correct if beginFade is zero or negative
	beginFade = beginFade > 0 ? beginFade : beginFade + TRANSITION_DENOMINATOR;
	// Wait to start transition until precipitation begins/ends
	float startPercentage = 1 - ((TRANSITION_DENOMINATOR - beginFade) * (1.0f / TRANSITION_DENOMINATOR));

	if (fadeIn) {
		float currentPercentage = (skyCurrentWeatherPct - startPercentage) / (1 - startPercentage);
		weatherTransitionPercentage = std::clamp(currentPercentage, 0.0f, 1.0f);
	} else {
		float currentPercentage = (startPercentage - skyCurrentWeatherPct) / (startPercentage);
		weatherTransitionPercentage = 1 - std::clamp(currentPercentage, 0.0f, 1.0f);
	}
	return weatherTransitionPercentage;
}

void WetnessEffects::CalculateWetness(RE::TESWeather* weather, RE::Sky* sky, float seconds, float& weatherWetnessDepth, float& weatherPuddleDepth)
{
	float wetnessDepthDelta = CLEAR_DAY_DELTA_PER_SECOND * WETNESS_SCALE * seconds;
	float puddleDepthDelta = CLEAR_DAY_DELTA_PER_SECOND * PUDDLE_SCALE * seconds;
	if (weather && sky) {
		// Figure out the weather type and set the wetness
		if (weather->precipitationData && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
			// Raining
			wetnessDepthDelta = RAIN_DELTA_PER_SECOND * WETNESS_SCALE * seconds;
			puddleDepthDelta = RAIN_DELTA_PER_SECOND * PUDDLE_SCALE * seconds;
		} else if (weather->precipitationData && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kSnow)) {
			wetnessDepthDelta = SNOWY_DAY_DELTA_PER_SECOND * WETNESS_SCALE * seconds;
			puddleDepthDelta = SNOWY_DAY_DELTA_PER_SECOND * PUDDLE_SCALE * seconds;
		} else if (weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kCloudy)) {
			wetnessDepthDelta = CLOUDY_DAY_DELTA_PER_SECOND * WETNESS_SCALE * seconds;
			puddleDepthDelta = CLOUDY_DAY_DELTA_PER_SECOND * PUDDLE_SCALE * seconds;
		}
	}

	weatherWetnessDepth = wetnessDepthDelta > 0 ? std::min(weatherWetnessDepth + wetnessDepthDelta, MAX_WETNESS_DEPTH) : std::max(weatherWetnessDepth + wetnessDepthDelta, 0.0f);
	weatherPuddleDepth = puddleDepthDelta > 0 ? std::min(weatherPuddleDepth + puddleDepthDelta, MAX_PUDDLE_DEPTH) : std::max(weatherPuddleDepth + puddleDepthDelta, 0.0f);
}

WetnessEffects::PerFrame WetnessEffects::GetCommonBufferData()
{
	PerFrame data{};
	data.Wetness = DRY_WETNESS;
	data.PuddleWetness = DRY_WETNESS;
	currentWeatherID = 0;
	uint32_t previousLastWeatherID = lastWeatherID;
	lastWeatherID = 0;
	float currentWeatherRaining = 0.0f;
	float lastWeatherRaining = 0.0f;
	float weatherTransitionPercentage = previousWeatherTransitionPercentage;

	if (settings.EnableWetnessEffects) {
		if (auto sky = RE::Sky::GetSingleton()) {
			if (sky->mode.get() == RE::Sky::Mode::kFull) {
				if (auto currentWeather = sky->currentWeather) {
					if (currentWeather->precipitationData && currentWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
						float rainDensity = currentWeather->precipitationData->GetSettingValue(RE::BGSShaderParticleGeometryData::DataID::kParticleDensity).f;
						float rainGravity = currentWeather->precipitationData->GetSettingValue(RE::BGSShaderParticleGeometryData::DataID::kGravityVelocity).f;
						currentWeatherRaining = std::clamp(((rainDensity * rainGravity) / AVERAGE_RAIN_VOLUME), MIN_RAINDROP_CHANCE_MULTIPLIER, MAX_RAINDROP_CHANCE_MULTIPLIER);
					}
					currentWeatherID = currentWeather->GetFormID();
					if (auto calendar = RE::Calendar::GetSingleton()) {
						float currentWeatherWetnessDepth = wetnessDepth;
						float currentWeatherPuddleDepth = puddleDepth;
						float currentGameTime = calendar->GetCurrentGameTime() * SECONDS_IN_A_DAY;
						lastGameTimeValue = lastGameTimeValue == 0 ? currentGameTime : lastGameTimeValue;
						float seconds = currentGameTime - lastGameTimeValue;
						lastGameTimeValue = currentGameTime;

						if (seconds >= MAX_TIME_DELTA || seconds < 0) {
							// If too much time has passed, snap wetness depths to the current weather.
							seconds = 0.0f;
							currentWeatherWetnessDepth = 0.0f;
							currentWeatherPuddleDepth = 0.0f;
							weatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
							CalculateWetness(currentWeather, sky, 1.0f, currentWeatherWetnessDepth, currentWeatherPuddleDepth);
							wetnessDepth = currentWeatherWetnessDepth > 0 ? MAX_WETNESS_DEPTH : 0.0f;
							puddleDepth = currentWeatherPuddleDepth > 0 ? MAX_PUDDLE_DEPTH : 0.0f;
						}

						if (seconds > 0) {
							weatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
							float lastWeatherWetnessDepth = wetnessDepth;
							float lastWeatherPuddleDepth = puddleDepth;
							seconds *= std::clamp(settings.WeatherTransitionSpeed, MIN_WEATHER_TRANSITION_SPEED, MAX_WEATHER_TRANSITION_SPEED);
							CalculateWetness(currentWeather, sky, seconds, currentWeatherWetnessDepth, currentWeatherPuddleDepth);
							// If there is a lastWeather, figure out what type it is and set the wetness
							if (auto lastWeather = sky->lastWeather) {
								lastWeatherID = lastWeather->GetFormID();
								CalculateWetness(lastWeather, sky, seconds, lastWeatherWetnessDepth, lastWeatherPuddleDepth);
								// If it was raining, wait to transition until precipitation ends, otherwise use the current weather's fade in
								if (lastWeather->precipitationData && lastWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
									float rainDensity = lastWeather->precipitationData->GetSettingValue(RE::BGSShaderParticleGeometryData::DataID::kParticleDensity).f;
									float rainGravity = lastWeather->precipitationData->GetSettingValue(RE::BGSShaderParticleGeometryData::DataID::kGravityVelocity).f;
									lastWeatherRaining = std::clamp(((rainDensity * rainGravity) / AVERAGE_RAIN_VOLUME), MIN_RAINDROP_CHANCE_MULTIPLIER, MAX_RAINDROP_CHANCE_MULTIPLIER);
									weatherTransitionPercentage = CalculateWeatherTransitionPercentage(sky->currentWeatherPct, lastWeather->data.precipitationEndFadeOut, false);
								} else {
									weatherTransitionPercentage = CalculateWeatherTransitionPercentage(sky->currentWeatherPct, currentWeather->data.precipitationBeginFadeIn, true);
								}
							}

							// Transition between CurrentWeather and LastWeather depth values
							wetnessDepth = std::lerp(lastWeatherWetnessDepth, currentWeatherWetnessDepth, weatherTransitionPercentage);
							puddleDepth = std::lerp(lastWeatherPuddleDepth, currentWeatherPuddleDepth, weatherTransitionPercentage);
						} else {
							lastWeatherID = previousLastWeatherID;
						}

						// Calculate the wetness value from the water depth
						data.Wetness = std::min(wetnessDepth, MAX_WETNESS);
						data.PuddleWetness = std::min(puddleDepth, MAX_PUDDLE_WETNESS);
						data.Raining = std::lerp(lastWeatherRaining, currentWeatherRaining, weatherTransitionPercentage);
						previousWeatherTransitionPercentage = weatherTransitionPercentage;
					}
				}
			}
		}
	}

	static size_t rainTimer = 0;                                       // size_t for precision
	if (!RE::UI::GetSingleton()->GameIsPaused())                       // from lightlimitfix
		rainTimer += (size_t)(RE::GetSecondsSinceLastFrame() * 1000);  // BSTimer::delta is always 0 for some reason
	data.Time = rainTimer / 1000.f;

	data.settings = settings;
	// Disable Shore Wetness if Wetness Effects are Disabled
	data.settings.MaxShoreWetness = settings.EnableWetnessEffects ? settings.MaxShoreWetness : 0.0f;
	// calculating some parameters on cpu
	data.settings.RaindropChance *= data.Raining;
	data.settings.RaindropGridSize = 1.f / settings.RaindropGridSize;
	data.settings.RaindropInterval = 1.f / settings.RaindropInterval;
	data.settings.RippleLifetime = settings.RaindropInterval / settings.RippleLifetime;
	data.settings.ChaoticRippleStrength *= std::clamp(data.Raining, 0.f, 1.f);
	data.settings.ChaoticRippleScale = 1.f / settings.ChaoticRippleScale;

	return data;
}

void WetnessEffects::Reset()
{
	requiresUpdate = true;
}

void WetnessEffects::LoadSettings(json& o_json)
{
	settings = o_json;
}

void WetnessEffects::SaveSettings(json& o_json)
{
	o_json = settings;
}

void WetnessEffects::RestoreDefaultSettings()
{
	settings = {};
}