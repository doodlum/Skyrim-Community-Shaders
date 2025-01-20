#include "VariableCache.h"

#define GET_INSTANCE_MEMBER_PTR(a_value, a_source) \
	&(!REL::Module::IsVR() ? a_source->GetRuntimeData().a_value : a_source->GetVRRuntimeData().a_value);

void VariableCache::InitializeVariables()
{
	renderer = RE::BSGraphics::Renderer::GetSingleton();

	device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
	context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);

	shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

	currentPixelShader = GET_INSTANCE_MEMBER_PTR(currentPixelShader, shadowState);
	currentVertexShader = GET_INSTANCE_MEMBER_PTR(currentVertexShader, shadowState);
	stateUpdateFlags = GET_INSTANCE_MEMBER_PTR(stateUpdateFlags, shadowState);

	state = State::GetSingleton();
	shaderCache = &SIE::ShaderCache::Instance();
	deferred = Deferred::GetSingleton();
	terrainBlending = TerrainBlending::GetSingleton();
	cloudShadows = CloudShadows::GetSingleton();
	truePBR = TruePBR::GetSingleton();
	graphicsState = RE::BSGraphics::State::GetSingleton();
	smState = &RE::BSShaderManager::State::GetSingleton();
	tes = RE::TES::GetSingleton();
	isVR = REL::Module::IsVR();
	memoryManager = RE::MemoryManager::GetSingleton();

	iniSettingCollection = RE::INISettingCollection::GetSingleton();
	bEnableLandFade = iniSettingCollection->GetSetting("bEnableLandFade:Display");
	bDrawLandShadows = iniSettingCollection->GetSetting("bDrawLandShadows:Display");

	bShadowsOnGrass = RE::GetINISetting("bShadowsOnGrass:Display");
	shadowMaskQuarter = RE::GetINISetting("iShadowMaskQuarter:Display");
}