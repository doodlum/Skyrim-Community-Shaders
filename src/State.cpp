#include "State.h"

#include <magic_enum.hpp>
#include <pystring/pystring.h>

#include "Menu.h"
#include "ShaderCache.h"

#include "Feature.h"
#include "Util.h"

void State::Draw()
{
	auto& shaderCache = SIE::ShaderCache::Instance();
	if (shaderCache.IsEnabled() && currentShader) {
		auto type = currentShader->shaderType.get();
		if (type > 0 && type < RE::BSShader::Type::Total) {
			if (enabledClasses[type - 1]) {
				ModifyShaderLookup(*currentShader, currentVertexDescriptor, currentPixelDescriptor);
				UpdateSharedData(currentShader, currentPixelDescriptor);

				auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

				if (auto vertexShader = shaderCache.GetVertexShader(*currentShader, currentVertexDescriptor)) {
					context->VSSetShader(vertexShader->shader, NULL, NULL);
				}

				if (auto pixelShader = shaderCache.GetPixelShader(*currentShader, currentPixelDescriptor)) {
					context->PSSetShader(pixelShader->shader, NULL, NULL);
				}

				for (auto* feature : Feature::GetFeatureList()) {
					if (feature->loaded) {
						feature->Draw(currentShader, currentPixelDescriptor);
					}
				}
			}
		}
	}

	currentShader = nullptr;
}

void State::DrawDeferred()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto context = renderer->GetRuntimeData().context;

	ID3D11ShaderResourceView* srvs[16];
	context->PSGetShaderResources(0, 16, srvs);

	ID3D11ShaderResourceView* srvsCS[16];
	context->CSGetShaderResources(0, 16, srvsCS);

	ID3D11UnorderedAccessView* uavsCS[16];
	context->CSGetUnorderedAccessViews(0, 16, uavsCS);

	ID3D11UnorderedAccessView* nullUavs[16] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	context->CSSetUnorderedAccessViews(0, 16, nullUavs, nullptr);

	ID3D11ShaderResourceView* nullSrvs[16] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	context->PSSetShaderResources(0, 16, nullSrvs);
	context->CSSetShaderResources(0, 16, nullSrvs);

	ID3D11RenderTargetView* views[8];
	ID3D11DepthStencilView* dsv;
	context->OMGetRenderTargets(8, views, &dsv);

	ID3D11RenderTargetView* nullViews[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11DepthStencilView* nullDsv = nullptr;
	context->OMSetRenderTargets(8, nullViews, nullDsv);

	for (auto* feature : Feature::GetFeatureList()) {
		if (feature->loaded) {
			feature->DrawDeferred();
		}
	}

	context->PSSetShaderResources(0, 16, srvs);
	context->CSSetShaderResources(0, 16, srvsCS);
	context->CSSetUnorderedAccessViews(0, 16, uavsCS, nullptr);
	context->OMSetRenderTargets(8, views, dsv);

	for (int i = 0; i < 16; i++) {
		if (srvs[i])
			srvs[i]->Release();
		if (srvsCS[i])
			srvsCS[i]->Release();
	}

	for (int i = 0; i < 8; i++) {
		if (views[i])
			views[i]->Release();
	}

	if (dsv)
		dsv->Release();
}

void State::Reset()
{
	lightingDataRequiresUpdate = true;
	for (auto* feature : Feature::GetFeatureList())
		if (feature->loaded)
			feature->Reset();
}

void State::Setup()
{
	for (auto* feature : Feature::GetFeatureList())
		if (feature->loaded)
			feature->SetupResources();
	SetupResources();
}

void State::Load(bool a_test)
{
	auto& shaderCache = SIE::ShaderCache::Instance();

	std::string configPath = a_test ? testConfigPath : userConfigPath;
	std::ifstream i(configPath);
	if (!i.is_open()) {
		logger::info("Unable to open user config file ({}); trying default ({})", configPath, defaultConfigPath);
		configPath = defaultConfigPath;
		i.open(configPath);
		if (!i.is_open()) {
			logger::error("Error opening config file ({})\n", configPath);
			return;
		}
	}
	logger::info("Loading config file ({})", configPath);

	json settings;
	try {
		i >> settings;
	} catch (const nlohmann::json::parse_error& e) {
		logger::error("Error parsing json config file ({}) : {}\n", configPath, e.what());
		return;
	}

	if (settings["Menu"].is_object()) {
		Menu::GetSingleton()->Load(settings["Menu"]);
	}

	if (settings["Advanced"].is_object()) {
		json& advanced = settings["Advanced"];
		if (advanced["Dump Shaders"].is_boolean())
			shaderCache.SetDump(advanced["Dump Shaders"]);
		if (advanced["Log Level"].is_number_integer()) {
			logLevel = static_cast<spdlog::level::level_enum>((int)advanced["Log Level"]);
			//logLevel = static_cast<spdlog::level::level_enum>(max(spdlog::level::trace, min(spdlog::level::off, (int)advanced["Log Level"])));
		}
		if (advanced["Shader Defines"].is_string())
			SetDefines(advanced["Shader Defines"]);
		if (advanced["Compiler Threads"].is_number_integer())
			shaderCache.compilationThreadCount = std::clamp(advanced["Compiler Threads"].get<int32_t>(), 1, static_cast<int32_t>(std::thread::hardware_concurrency()));
		if (advanced["Background Compiler Threads"].is_number_integer())
			shaderCache.backgroundCompilationThreadCount = std::clamp(advanced["Background Compiler Threads"].get<int32_t>(), 1, static_cast<int32_t>(std::thread::hardware_concurrency()));
	}

	if (settings["General"].is_object()) {
		json& general = settings["General"];

		if (general["Enable Shaders"].is_boolean())
			shaderCache.SetEnabled(general["Enable Shaders"]);

		if (general["Enable Disk Cache"].is_boolean())
			shaderCache.SetDiskCache(general["Enable Disk Cache"]);

		if (general["Enable Async"].is_boolean())
			shaderCache.SetAsync(general["Enable Async"]);
	}

	if (settings["Replace Original Shaders"].is_object()) {
		json& originalShaders = settings["Replace Original Shaders"];
		for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
			auto name = magic_enum::enum_name((RE::BSShader::Type)(classIndex + 1));
			if (originalShaders[name].is_boolean()) {
				enabledClasses[classIndex] = originalShaders[name];
			}
		}
	}

	for (auto* feature : Feature::GetFeatureList())
		feature->Load(settings);
	i.close();
	if (settings["Version"].is_string() && settings["Version"].get<std::string>() != Plugin::VERSION.string()) {
		logger::info("Found older config for version {}; upgrading to {}", (std::string)settings["Version"], Plugin::VERSION.string());
		Save();
	}
}

void State::Save(bool a_test)
{
	auto& shaderCache = SIE::ShaderCache::Instance();
	std::ofstream o{ a_test ? testConfigPath : userConfigPath };
	json settings;

	Menu::GetSingleton()->Save(settings);

	json advanced;
	advanced["Dump Shaders"] = shaderCache.IsDump();
	advanced["Log Level"] = logLevel;
	advanced["Shader Defines"] = shaderDefinesString;
	advanced["Compiler Threads"] = shaderCache.compilationThreadCount;
	advanced["Background Compiler Threads"] = shaderCache.backgroundCompilationThreadCount;
	settings["Advanced"] = advanced;

	json general;
	general["Enable Shaders"] = shaderCache.IsEnabled();
	general["Enable Disk Cache"] = shaderCache.IsDiskCache();
	general["Enable Async"] = shaderCache.IsAsync();

	settings["General"] = general;

	json originalShaders;
	for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
		originalShaders[magic_enum::enum_name((RE::BSShader::Type)(classIndex + 1))] = enabledClasses[classIndex];
	}
	settings["Replace Original Shaders"] = originalShaders;

	settings["Version"] = Plugin::VERSION.string();

	for (auto* feature : Feature::GetFeatureList())
		feature->Save(settings);

	o << settings.dump(1);
	logger::info("Saving settings to {}", userConfigPath);
}

void State::PostPostLoad()
{
	upscalerLoaded = GetModuleHandle(L"Data\\SKSE\\Plugins\\SkyrimUpscaler.dll");
	if (upscalerLoaded)
		logger::info("Skyrim Upscaler detected");
	else
		logger::info("Skyrim Upscaler not detected");
}

bool State::ValidateCache(CSimpleIniA& a_ini)
{
	bool valid = true;
	for (auto* feature : Feature::GetFeatureList())
		valid = valid && feature->ValidateCache(a_ini);
	return valid;
}

void State::WriteDiskCacheInfo(CSimpleIniA& a_ini)
{
	for (auto* feature : Feature::GetFeatureList())
		feature->WriteDiskCacheInfo(a_ini);
}

void State::SetLogLevel(spdlog::level::level_enum a_level)
{
	logLevel = a_level;
	spdlog::set_level(logLevel);
	spdlog::flush_on(logLevel);
	logger::info("Log Level set to {} ({})", magic_enum::enum_name(logLevel), static_cast<int>(logLevel));
}

spdlog::level::level_enum State::GetLogLevel()
{
	return logLevel;
}

void State::SetDefines(std::string a_defines)
{
	shaderDefines.clear();
	shaderDefinesString = "";
	std::string name = "";
	std::string definition = "";
	auto defines = pystring::split(a_defines, ";");
	for (const auto& define : defines) {
		auto cleanedDefine = pystring::strip(define);
		auto token = pystring::split(cleanedDefine, "=");
		if (token.empty() || token[0].empty())
			continue;
		if (token.size() > 2) {
			logger::warn("Define string has too many '='; ignoring {}", define);
			continue;
		}
		name = pystring::strip(token[0]);
		if (token.size() == 2) {
			definition = pystring::strip(token[1]);
		}
		shaderDefinesString += pystring::strip(define) + ";";
		shaderDefines.push_back(std::pair(name, definition));
	}
	shaderDefinesString = shaderDefinesString.substr(0, shaderDefinesString.size() - 1);
	logger::debug("Shader Defines set to {}", shaderDefinesString);
}

std::vector<std::pair<std::string, std::string>>* State::GetDefines()
{
	return &shaderDefines;
}

bool State::ShaderEnabled(const RE::BSShader::Type a_type)
{
	auto index = static_cast<uint32_t>(a_type) + 1;
	if (index && index < sizeof(enabledClasses)) {
		return enabledClasses[index];
	}
	return false;
}

bool State::IsShaderEnabled(const RE::BSShader& a_shader)
{
	return ShaderEnabled(a_shader.shaderType.get());
}

bool State::IsDeveloperMode()
{
	return GetLogLevel() <= spdlog::level::debug;
}

void State::SetupResources()
{
	D3D11_BUFFER_DESC sbDesc{};
	sbDesc.Usage = D3D11_USAGE_DYNAMIC;
	sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbDesc.StructureByteStride = sizeof(PerShader);
	sbDesc.ByteWidth = sizeof(PerShader);
	shaderDataBuffer = std::make_unique<Buffer>(sbDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 1;
	shaderDataBuffer->CreateSRV(srvDesc);

	sbDesc.StructureByteStride = sizeof(LightingData);
	sbDesc.ByteWidth = sizeof(LightingData);
	lightingDataBuffer = std::make_unique<Buffer>(sbDesc);
	lightingDataBuffer->CreateSRV(srvDesc);
}

void State::ModifyShaderLookup(const RE::BSShader& a_shader, uint& a_vertexDescriptor, uint& a_pixelDescriptor)
{
	if (a_shader.shaderType.get() == RE::BSShader::Type::Lighting || a_shader.shaderType.get() == RE::BSShader::Type::Water) {
		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

		if (a_vertexDescriptor != lastVertexDescriptor || a_pixelDescriptor != lastPixelDescriptor) {
			PerShader data{};
			data.VertexShaderDescriptor = a_vertexDescriptor;
			data.PixelShaderDescriptor = a_pixelDescriptor;

			D3D11_MAPPED_SUBRESOURCE mapped;
			DX::ThrowIfFailed(context->Map(shaderDataBuffer->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			size_t bytes = sizeof(PerShader);
			memcpy_s(mapped.pData, bytes, &data, bytes);
			context->Unmap(shaderDataBuffer->resource.get(), 0);

			lastVertexDescriptor = a_vertexDescriptor;
			lastPixelDescriptor = a_pixelDescriptor;
		}

		if (a_shader.shaderType.get() == RE::BSShader::Type::Lighting) {
			a_vertexDescriptor &= ~((uint32_t)SIE::ShaderCache::LightingShaderFlags::AdditionalAlphaMask |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::AmbientSpecular |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::DoAlphaTest |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::ShadowDir |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::DefShadow |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::CharacterLight |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::RimLighting |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::SoftLighting |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::BackLighting |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::Specular |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::AnisoLighting |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::BaseObjectIsSnow |
									(uint32_t)SIE::ShaderCache::LightingShaderFlags::Snow);

			a_pixelDescriptor &= ~((uint32_t)SIE::ShaderCache::LightingShaderFlags::AmbientSpecular |
								   (uint32_t)SIE::ShaderCache::LightingShaderFlags::ShadowDir |
								   (uint32_t)SIE::ShaderCache::LightingShaderFlags::DefShadow |
								   (uint32_t)SIE::ShaderCache::LightingShaderFlags::CharacterLight);

			static auto enableImprovedSnow = RE::GetINISetting("bEnableImprovedSnow:Display");
			static bool vr = REL::Module::IsVR();

			if (vr || !enableImprovedSnow->GetBool())
				a_pixelDescriptor &= ~((uint32_t)SIE::ShaderCache::LightingShaderFlags::Snow);

			{
				uint32_t technique = 0x3F & (a_vertexDescriptor >> 24);
				if (technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::Glowmap ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::Parallax ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::Facegen ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::FacegenRGBTint ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::LODObjects ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::LODObjectHD ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::MultiIndexSparkle ||
					technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::Hair)
					a_vertexDescriptor &= ~(0x3F << 24);
			}

			{
				uint32_t technique = 0x3F & (a_pixelDescriptor >> 24);
				if (technique == (uint32_t)SIE::ShaderCache::LightingShaderTechniques::Glowmap)
					a_pixelDescriptor &= ~(0x3F << 24);
			}

		} else {
			a_vertexDescriptor &= ~((uint32_t)SIE::ShaderCache::WaterShaderFlags::Reflections |
									(uint32_t)SIE::ShaderCache::WaterShaderFlags::Cubemap |
									(uint32_t)SIE::ShaderCache::WaterShaderFlags::Interior |
									(uint32_t)SIE::ShaderCache::WaterShaderFlags::Reflections);

			a_pixelDescriptor &= ~((uint32_t)SIE::ShaderCache::WaterShaderFlags::Reflections |
								   (uint32_t)SIE::ShaderCache::WaterShaderFlags::Cubemap |
								   (uint32_t)SIE::ShaderCache::WaterShaderFlags::Interior);
		}

		ID3D11ShaderResourceView* view = shaderDataBuffer->srv.get();
		context->PSSetShaderResources(127, 1, &view);
	}
}

void State::UpdateSharedData(const RE::BSShader* a_shader, const uint32_t)
{
	if (a_shader->shaderType.get() == RE::BSShader::Type::Lighting) {
		bool updateBuffer = false;

		auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

		bool currentReflections = (!REL::Module::IsVR() ?
										  RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().cubeMapRenderTarget :
										  RE::BSGraphics::RendererShadowState::GetSingleton()->GetVRRuntimeData().cubeMapRenderTarget) == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS;

		if (lightingData.Reflections != currentReflections) {
			updateBuffer = true;
			lightingDataRequiresUpdate = true;
		}

		lightingData.Reflections = currentReflections;

		if (lightingDataRequiresUpdate) {
			lightingDataRequiresUpdate = false;
			for (int i = -2; i < 3; i++) {
				for (int k = -2; k < 3; k++) {
					int waterTile = (i + 2) + ((k + 2) * 5);
					auto position = !REL::Module::IsVR() ? shadowState->GetRuntimeData().posAdjust.getEye() : shadowState->GetVRRuntimeData().posAdjust.getEye();
					lightingData.WaterHeight[waterTile] = Util::TryGetWaterHeight((float)i * 4096.0f, (float)k * 4096.0f) - position.z;
				}
			}
			updateBuffer = true;
		}

		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

		if (updateBuffer) {
			D3D11_MAPPED_SUBRESOURCE mapped;
			DX::ThrowIfFailed(context->Map(lightingDataBuffer->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			size_t bytes = sizeof(LightingData);
			memcpy_s(mapped.pData, bytes, &lightingData, bytes);
			context->Unmap(lightingDataBuffer->resource.get(), 0);
		}

		ID3D11ShaderResourceView* view = lightingDataBuffer->srv.get();
		context->PSSetShaderResources(126, 1, &view);
	}
}
