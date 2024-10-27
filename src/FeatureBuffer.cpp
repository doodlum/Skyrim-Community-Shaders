#include "FeatureBuffer.h"

#include "Features/DynamicCubemaps.h"
#include "Features/ExtendedMaterials.h"
#include "Features/GrassLighting.h"
#include "Features/LightLimitFix.h"
#include "Features/Skylighting.h"
#include "Features/TerrainShadows.h"
#include "Features/WetnessEffects.h"

#include "TruePBR.h"

template <class... Ts>
std::pair<unsigned char*, size_t> _GetFeatureBufferData(Ts... feat_datas)
{
	size_t totalSize = (... + sizeof(Ts));
	auto data = new unsigned char[totalSize];
	size_t offset = 0;

	([&] {
		*((decltype(feat_datas)*)(data + offset)) = feat_datas;
		offset += sizeof(decltype(feat_datas));
	}(),
		...);

	return std::make_pair(data, totalSize);
}

std::pair<unsigned char*, size_t> GetFeatureBufferData()
{
	return _GetFeatureBufferData(
		GrassLighting::GetSingleton()->settings,
		ExtendedMaterials::GetSingleton()->settings,
		DynamicCubemaps::GetSingleton()->settings,
		TerrainShadows::GetSingleton()->GetCommonBufferData(),
		LightLimitFix::GetSingleton()->GetCommonBufferData(),
		WetnessEffects::GetSingleton()->GetCommonBufferData(),
		Skylighting::GetSingleton()->GetCommonBufferData(),
		TruePBR::GetSingleton()->settings);
}