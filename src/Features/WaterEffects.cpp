#include "WaterEffects.h"

#include "State.h"
#include "Util.h"

#include <DDSTextureLoader.h>

void WaterEffects::SetupResources()
{
	auto& device = State::GetSingleton()->device;
	auto& context = State::GetSingleton()->context;

	DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\WaterEffects\\watercaustics.dds", nullptr, causticsView.put());
}

void WaterEffects::Prepass()
{
	auto& context = State::GetSingleton()->context;
	auto srv = causticsView.get();
	context->PSSetShaderResources(100, 1, &srv);
}

bool WaterEffects::HasShaderDefine(RE::BSShader::Type)
{
	return true;
}
