#include "VariableCache.h"

#define GET_INSTANCE_MEMBER_PTR(a_value, a_source) \
	&(!REL::Module::IsVR() ? a_source->GetRuntimeData().a_value : a_source->GetVRRuntimeData().a_value);

void VariableCache::InitializeVariables()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

	currentPixelShader = GET_INSTANCE_MEMBER_PTR(currentPixelShader, shadowState);
	currentVertexShader = GET_INSTANCE_MEMBER_PTR(currentVertexShader, shadowState);
	stateUpdateFlags = GET_INSTANCE_MEMBER_PTR(stateUpdateFlags, shadowState);

	state = State::GetSingleton();
	shaderCache = &SIE::ShaderCache::Instance();
	rendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	deferred = Deferred::GetSingleton();
	terrainBlending = TerrainBlending::GetSingleton();
	cloudShadows = CloudShadows::GetSingleton();
	truePBR = TruePBR::GetSingleton();
}