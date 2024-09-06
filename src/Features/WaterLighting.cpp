#include "WaterLighting.h"

#include "State.h"
#include "Util.h"

#include <DDSTextureLoader.h>

void WaterLighting::SetupResources()
{
	auto& device = State::GetSingleton()->device;
	auto& context = State::GetSingleton()->context;

	DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\WaterLighting\\watercaustics.dds", nullptr, causticsView.put());
}

void WaterLighting::Prepass()
{
	auto& context = State::GetSingleton()->context;
	auto srv = causticsView.get();
	context->PSSetShaderResources(70, 1, &srv);
}

bool WaterLighting::HasShaderDefine(RE::BSShader::Type)
{
	return true;
}
