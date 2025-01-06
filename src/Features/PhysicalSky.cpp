#include "PhysicalSky.h"
#include "Menu.h"

#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PhysicalSky::Settings,
	EnablePhysicalSky)

void PhysicalSky::SetupResources()
{
	logger::debug("Compiling shaders...");
	CompileComputeShaders();
}

void PhysicalSky::DrawSettings()
{
	ImGui::Checkbox("Enable Physical Sky", &settings.EnablePhysicalSky);
}

void PhysicalSky::LoadSettings(json& o_json)
{
	settings = o_json;
}

void PhysicalSky::SaveSettings(json& o_json)
{
	o_json = settings;
}

void PhysicalSky::RestoreDefaultSettings()
{
	settings = {};
}

void PhysicalSky::CompileComputeShaders()
{
	struct ShaderCompileInfo
	{
		winrt::com_ptr<ID3D11ComputeShader>* programPtr;
		std::string_view filename;
		std::vector<std::pair<const char*, const char*>> defines;
	};

	std::vector<ShaderCompileInfo>
		shaderInfos = {
			{ &indirectIrradianceCS, "IndirectIrradianceCS.hlsl", {} },
			{ &transmittanceLutCS, "TransmittanceLutCS.hlsl", {} },
			{ &multiScatteringLutCS, "MultiScatteringLutCS.hlsl", {} },
		};

	for (auto& [programPtr, filename, defines] : shaderInfos) {
		auto path = std::filesystem::path("Data\\Shaders\\PhysicalSky") / filename;
		if (const auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(path.c_str(), defines, "cs_5_0")))
			programPtr->attach(rawPtr);
	}
}