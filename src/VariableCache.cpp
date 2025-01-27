#include "VariableCache.h"

#define GET_INSTANCE_MEMBER_PTR(a_value, a_source) \
	&(!REL::Module::IsVR() ? a_source->GetRuntimeData().a_value : a_source->GetVRRuntimeData().a_value);

void VariableCache::OnInit()
{
	renderer = RE::BSGraphics::Renderer::GetSingleton();

	device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
	context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);

	shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

	currentPixelShader = GET_INSTANCE_MEMBER_PTR(currentPixelShader, shadowState);
	currentVertexShader = GET_INSTANCE_MEMBER_PTR(currentVertexShader, shadowState);
	stateUpdateFlags = GET_INSTANCE_MEMBER_PTR(stateUpdateFlags, shadowState);

	state = State::GetSingleton();
	menu = Menu::GetSingleton();
	shaderCache = &SIE::ShaderCache::Instance();
	deferred = Deferred::GetSingleton();

	terrainBlending = TerrainBlending::GetSingleton();
	cloudShadows = CloudShadows::GetSingleton();
	truePBR = TruePBR::GetSingleton();
	lightLimitFix = LightLimitFix::GetSingleton();
	grassCollision = GrassCollision::GetSingleton();
	subsurfaceScattering = SubsurfaceScattering::GetSingleton();
	skylighting = Skylighting::GetSingleton();

	particleLights = ParticleLights::GetSingleton();

	cameraNear = (float*)(REL::RelocationID(517032, 403540).address() + 0x40);
	cameraFar = (float*)(REL::RelocationID(517032, 403540).address() + 0x44);

	graphicsState = RE::BSGraphics::State::GetSingleton();
	smState = &RE::BSShaderManager::State::GetSingleton();
	isVR = REL::Module::IsVR();
	memoryManager = RE::MemoryManager::GetSingleton();
	iniSettingCollection = RE::INISettingCollection::GetSingleton();
}

void VariableCache::OnDataLoaded()
{
	tes = RE::TES::GetSingleton();
	sky = RE::Sky::GetSingleton();

	utilityShader = RE::BSUtilityShader::GetSingleton();

	bEnableLandFade = iniSettingCollection->GetSetting("bEnableLandFade:Display");

	bShadowsOnGrass = RE::GetINISetting("bShadowsOnGrass:Display");
	shadowMaskQuarter = RE::GetINISetting("iShadowMaskQuarter:Display");
}
