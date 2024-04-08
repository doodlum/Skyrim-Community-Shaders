#include "Feature.h"

#include "FeatureVersions.h"
#include "Features/CloudShadows.h"
#include "Features/DistantTreeLighting.h"
#include "Features/DynamicCubemaps.h"
#include "Features/ExtendedMaterials.h"
#include "Features/GrassCollision.h"
#include "Features/GrassLighting.h"
#include "Features/LightLimitFix.h"
#include "Features/ScreenSpaceGI.h"
#include "Features/ScreenSpaceShadows.h"
#include "Features/SubsurfaceScattering.h"
#include "Features/TerrainBlending.h"
#include "Features/TerrainOcclusion.h"
#include "Features/WaterBlending.h"
#include "Features/WaterCaustics.h"
#include "Features/WaterParallax.h"
#include "Features/WetnessEffects.h"

void Feature::Load(json&)
{
	// convert string to wstring
	auto ini_filename = std::format("{}.ini", GetShortName());
	std::wstring ini_filename_w;
	std::ranges::copy(ini_filename, std::back_inserter(ini_filename_w));
	auto ini_path = L"Data\\Shaders\\Features\\" + ini_filename_w;

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(ini_path.c_str());
	if (auto value = ini.GetValue("Info", "Version")) {
		REL::Version featureVersion(std::regex_replace(value, std::regex("-"), "."));

		auto& minimalFeatureVersion = FeatureVersions::FEATURE_MINIMAL_VERSIONS.at(GetShortName());

		bool oldFeature = featureVersion.compare(minimalFeatureVersion) == std::strong_ordering::less;
		bool majorVersionMismatch = minimalFeatureVersion.major() < featureVersion.major();

		if (!oldFeature && !majorVersionMismatch) {
			loaded = true;
			logger::info("{} {} successfully loaded", ini_filename, value);
		} else {
			loaded = false;

			std::string minimalVersionString = minimalFeatureVersion.string();
			minimalVersionString = minimalVersionString.substr(0, minimalVersionString.size() - 2);

			if (majorVersionMismatch) {
				failedLoadedMessage = std::format("{} {} requires a newer version of community shaders, the feature version should be {}", GetShortName(), value, minimalVersionString);
			} else {
				failedLoadedMessage = std::format("{} {} is an old feature version, required: {}", GetShortName(), value, minimalVersionString);
			}
			logger::warn("{}", failedLoadedMessage);
		}

		version = value;
	} else {
		loaded = false;
		failedLoadedMessage = std::format("{} missing version info; not successfully loaded", ini_filename);
		logger::warn("{}", failedLoadedMessage);
	}
}

bool Feature::ValidateCache(CSimpleIniA& a_ini)
{
	auto name = GetName();
	auto ini_name = GetShortName();

	logger::info("Validating {}", name);

	auto enabledInCache = a_ini.GetBoolValue(ini_name.c_str(), "Enabled", false);
	if (enabledInCache && !loaded) {
		logger::info("Feature was uninstalled");
		return false;
	}
	if (!enabledInCache && loaded) {
		logger::info("Feature was installed");
		return false;
	}

	if (loaded) {
		auto versionInCache = a_ini.GetValue(ini_name.c_str(), "Version");
		if (strcmp(versionInCache, version.c_str()) != 0) {
			logger::info("Change in version detected. Installed {} but {} in Disk Cache", version, versionInCache);
			return false;
		} else {
			logger::info("Installed version and cached version match.");
		}
	}

	logger::info("Cached feature is valid");
	return true;
}

void Feature::WriteDiskCacheInfo(CSimpleIniA& a_ini)
{
	auto ini_name = GetShortName();
	a_ini.SetBoolValue(ini_name.c_str(), "Enabled", loaded);
	a_ini.SetValue(ini_name.c_str(), "Version", version.c_str());
}

const std::vector<Feature*>& Feature::GetFeatureList()
{
	// Cat: essentially load order i guess
	static std::vector<Feature*> features = {
		GrassLighting::GetSingleton(),
		DistantTreeLighting::GetSingleton(),
		GrassCollision::GetSingleton(),
		ScreenSpaceShadows::GetSingleton(),
		ExtendedMaterials::GetSingleton(),
		WaterBlending::GetSingleton(),
		WetnessEffects::GetSingleton(),
		LightLimitFix::GetSingleton(),
		DynamicCubemaps::GetSingleton(),
		CloudShadows::GetSingleton(),
		TerrainBlending::GetSingleton(),
		WaterParallax::GetSingleton(),
		WaterCaustics::GetSingleton(),
		SubsurfaceScattering::GetSingleton(),
		TerrainOcclusion::GetSingleton(),
		ScreenSpaceGI::GetSingleton()
	};

	static std::vector<Feature*> featuresVR = {
		DynamicCubemaps::GetSingleton(),
		GrassLighting::GetSingleton(),
		GrassCollision::GetSingleton(),
		ScreenSpaceShadows::GetSingleton(),
		ExtendedMaterials::GetSingleton(),
		WetnessEffects::GetSingleton(),
		LightLimitFix::GetSingleton(),
		TerrainBlending::GetSingleton(),
		WaterCaustics::GetSingleton(),
		SubsurfaceScattering::GetSingleton(),
		ScreenSpaceGI::GetSingleton()
	};

	return REL::Module::IsVR() ? featuresVR : features;
}