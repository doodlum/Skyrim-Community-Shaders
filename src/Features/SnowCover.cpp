#include "SnowCover.h"

#include "Util.h"

const float MIN_START_PERCENTAGE = 0.05f;
const float DEFAULT_TRANSITION_PERCENTAGE = 1.0f;
const float TRANSITION_CURVE_MULTIPLIER = 2.0f;
const float TRANSITION_DENOMINATOR = 256.0f;
const float DRY_WETNESS = 0.0f;
const float RAIN_DELTA_PER_SECOND = 2.0f / 3600.0f;
const float SNOWY_DAY_DELTA_PER_SECOND = -0.0f / 3600.0f;  // Only doing evaporation until snow wetness feature is added
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
	SnowCover::Settings,
	EnableSnowCover
	)

void SnowCover::DrawSettings()
{
	if (ImGui::TreeNodeEx("Snow Cover", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Snow Cover", (bool*)&settings.EnableSnowCover);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx("...", ImGuiTreeNodeFlags_DefaultOpen)) {
		
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

}

float SnowCover::CalculateWeatherTransitionPercentage(float skyCurrentWeatherPct, float beginFade, bool fadeIn)
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

void SnowCover::CalculateWetness(RE::TESWeather* weather, RE::Sky* sky, float seconds, float& weatherWetnessDepth, float& weatherPuddleDepth)
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

void SnowCover::Draw(const RE::BSShader*, const uint32_t)
{
}

SnowCover::PerFrame SnowCover::GetCommonBufferData()
{
	PerFrame data{};
	data.SnowAmount = DRY_WETNESS;
	data.SnowpileAmount = DRY_WETNESS;
	currentWeatherID = 0;
	uint32_t previousLastWeatherID = lastWeatherID;
	lastWeatherID = 0;
	float currentWeatherRaining = 0.0f;
	float lastWeatherRaining = 0.0f;
	float weatherTransitionPercentage = previousWeatherTransitionPercentage;

	if (settings.EnableSnowCover) {
		if (auto sky = RE::Sky::GetSingleton()) {
			if (sky->mode.get() == RE::Sky::Mode::kFull) {
				if (auto currentWeather = sky->currentWeather) {
					if (currentWeather->precipitationData && currentWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kSnow)) {
						float rainDensity = currentWeather->precipitationData->data[static_cast<int>(RE::BGSShaderParticleGeometryData::DataID::kParticleDensity)].f;
						float rainGravity = currentWeather->precipitationData->data[static_cast<int>(RE::BGSShaderParticleGeometryData::DataID::kGravityVelocity)].f;
						currentWeatherRaining = std::clamp(((rainDensity * rainGravity) / AVERAGE_RAIN_VOLUME), MIN_RAINDROP_CHANCE_MULTIPLIER, MAX_RAINDROP_CHANCE_MULTIPLIER);
					}
					currentWeatherID = currentWeather->GetFormID();
					if (auto calendar = RE::Calendar::GetSingleton()) {
						data.Month = calendar->GetMonth();
						float currentWeatherWetnessDepth = wetnessDepth;
						float currentWeatherPuddleDepth = puddleDepth;
						float currentGameTime = calendar->GetCurrentGameTime() * SECONDS_IN_A_DAY;
						lastGameTimeValue = lastGameTimeValue == 0 ? currentGameTime : lastGameTimeValue;
						float seconds = currentGameTime - lastGameTimeValue;
						lastGameTimeValue = currentGameTime;

						if (abs(seconds) >= MAX_TIME_DELTA) {
							// If too much time has passed, snap wetness depths to the current weather.
							seconds = 0.0f;
							currentWeatherWetnessDepth = 0.0f;
							currentWeatherPuddleDepth = 0.0f;
							weatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
							CalculateWetness(currentWeather, sky, 1.0f, currentWeatherWetnessDepth, currentWeatherPuddleDepth);
							wetnessDepth = currentWeatherWetnessDepth > 0 ? MAX_WETNESS_DEPTH : 0.0f;
							puddleDepth = currentWeatherPuddleDepth > 0 ? MAX_PUDDLE_DEPTH : 0.0f;
						}

						if (seconds > 0 || (seconds < 0 && (wetnessDepth > 0 || puddleDepth > 0))) {
							weatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
							float lastWeatherWetnessDepth = wetnessDepth;
							float lastWeatherPuddleDepth = puddleDepth;
							seconds *= MIN_WEATHER_TRANSITION_SPEED + (MAX_WEATHER_TRANSITION_SPEED - MIN_WEATHER_TRANSITION_SPEED) / 2.0f;
							CalculateWetness(currentWeather, sky, seconds, currentWeatherWetnessDepth, currentWeatherPuddleDepth);
							// If there is a lastWeather, figure out what type it is and set the wetness
							if (auto lastWeather = sky->lastWeather) {
								lastWeatherID = lastWeather->GetFormID();
								CalculateWetness(lastWeather, sky, seconds, lastWeatherWetnessDepth, lastWeatherPuddleDepth);
								// If it was raining, wait to transition until precipitation ends, otherwise use the current weather's fade in
								if (lastWeather->precipitationData && lastWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
									float rainDensity = lastWeather->precipitationData->data[static_cast<int>(RE::BGSShaderParticleGeometryData::DataID::kParticleDensity)].f;
									float rainGravity = lastWeather->precipitationData->data[static_cast<int>(RE::BGSShaderParticleGeometryData::DataID::kGravityVelocity)].f;
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
						data.SnowAmount = std::min(wetnessDepth, MAX_WETNESS);
						data.SnowpileAmount = std::min(puddleDepth, MAX_PUDDLE_WETNESS);
						data.Snowing = std::lerp(lastWeatherRaining, currentWeatherRaining, weatherTransitionPercentage);
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


	return data;
}

void SnowCover::SetupResources()
{
}

void SnowCover::Reset()
{
	requiresUpdate = true;
}

void SnowCover::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void SnowCover::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

void SnowCover::RestoreDefaultSettings()
{
	settings = {};
}