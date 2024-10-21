#include "FeatureBuffer.h"

#include "Features/DynamicCubemaps.h"
#include "Features/ExtendedMaterials.h"
#include "Features/GrassLighting.h"
#include "Features/LightLimitFix.h"
#include "Features/Skylighting.h"
#include "Features/TerrainShadows.h"
#include "Features/WetnessEffects.h"
#include "Features/ExtendedTransclucency.h"

#include "TruePBR.h"

template <typename Feature>
auto GetFeatureBuffer() { return Feature::GetSingleton()->settings; }
template <> auto GetFeatureBuffer<TerrainShadows>() { return TerrainShadows::GetSingleton()->GetCommonBufferData(); }
template <> auto GetFeatureBuffer<LightLimitFix>() { return LightLimitFix::GetSingleton()->GetCommonBufferData(); }
template <> auto GetFeatureBuffer<WetnessEffects>() { return WetnessEffects::GetSingleton()->GetCommonBufferData(); }
template <> auto GetFeatureBuffer<Skylighting>() { return Skylighting::GetSingleton()->GetCommonBufferData(); }

template <typename Feature>
using FeatureBufferType = decltype(GetFeatureBuffer<Feature>());

struct alignas(16) FeatureBuffer
{
	FeatureBufferType<GrassLighting> GrassLightingSettings;
	FeatureBufferType<ExtendedMaterials> ExtendedMaterialsSettings;
	FeatureBufferType<DynamicCubemaps> DynamicCubemapsSettings;
	FeatureBufferType<TerrainShadows> TerrainShadowsSettings;
	FeatureBufferType<LightLimitFix> LightLimitFixSettings;
	FeatureBufferType<ExtendedTransclucency> ExtendedTransclucencySettings;
	FeatureBufferType<WetnessEffects> WetnessEffectsSettings;
	FeatureBufferType<Skylighting> SkylightingSettings;
	FeatureBufferType<TruePBR> TruePBRSettings;
};

std::pair<unsigned char*, size_t> GetFeatureBufferData()
{
	static /*thread_local*/ FeatureBuffer Buffer;
	Buffer = 
	{
		GetFeatureBuffer<GrassLighting>(),
		GetFeatureBuffer<ExtendedMaterials>(),
		GetFeatureBuffer<DynamicCubemaps>(),
		GetFeatureBuffer<TerrainShadows>(),
		GetFeatureBuffer<LightLimitFix>(),
		GetFeatureBuffer<ExtendedTransclucency>(),
		GetFeatureBuffer<WetnessEffects>(),
		GetFeatureBuffer<Skylighting>(),
		GetFeatureBuffer<TruePBR>(),
	};
	return { reinterpret_cast<unsigned char*>(&Buffer), sizeof(FeatureBuffer) };
}
