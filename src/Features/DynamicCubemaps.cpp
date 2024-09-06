#include "DynamicCubemaps.h"

#include "State.h"
#include "Util.h"

#include <DDSTextureLoader.h>
#include <DirectXTex.h>

constexpr auto MIPLEVELS = 8;

void DynamicCubemaps::DrawSettings()
{
	if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (REL::Module::IsVR()) {
			if (ImGui::BeginTable("##SettingsToggles", 3, ImGuiTableFlags_SizingStretchSame)) {
				for (const auto& settingName : iniVRCubeMapSettings) {
					if (auto setting = RE::INISettingCollection::GetSingleton()->GetSetting(settingName); setting) {
						ImGui::TableNextColumn();
						ImGui::Checkbox(settingName.c_str(), &setting->data.b);
						if (auto _tt = Util::HoverTooltipWrapper()) {
							//ImGui::Text(fmt::format(fmt::runtime("{} {0:x}"), settingName, &setting->data.b).c_str());
							ImGui::Text(settingName.c_str());
						}
					}
				}
				for (const auto& settingPair : hiddenVRCubeMapSettings) {
					const auto& settingName = settingPair.first;
					const auto address = REL::Offset{ settingPair.second }.address();
					bool* setting = reinterpret_cast<bool*>(address);
					ImGui::TableNextColumn();
					ImGui::Checkbox(settingName.c_str(), setting);
					if (auto _tt = Util::HoverTooltipWrapper()) {
						ImGui::Text(settingName.c_str());
						//ImGui::Text(fmt::format(fmt::runtime("{} {0:x}"), settingName, address).c_str());
					}
				}
				ImGui::EndTable();
			}
		}

		if (ImGui::TreeNodeEx("Dynamic Cubemap Creator", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("You must enable creator mode by adding the shader define CREATOR");
			ImGui::Checkbox("Enable Creator", reinterpret_cast<bool*>(&settings.Enabled));
			if (settings.Enabled) {
				ImGui::ColorEdit3("Color", reinterpret_cast<float*>(&settings.CubemapColor));
				ImGui::SliderFloat("Roughness", &settings.CubemapColor.w, 0.0f, 1.0f, "%.2f");
				if (ImGui::Button("Export")) {
					auto& device = State::GetSingleton()->device;
					auto& context = State::GetSingleton()->context;

					D3D11_TEXTURE2D_DESC texDesc{};
					texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					texDesc.Height = 1;
					texDesc.Width = 1;
					texDesc.ArraySize = 6;
					texDesc.MipLevels = 1;
					texDesc.SampleDesc.Count = 1;
					texDesc.Usage = D3D11_USAGE_DEFAULT;
					texDesc.BindFlags = 0;
					texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

					D3D11_SUBRESOURCE_DATA subresourceData[6];

					struct PixelData
					{
						uint8_t r, g, b, a;
					};

					static PixelData colorPixel{};

					colorPixel = { (uint8_t)((settings.CubemapColor.x * 255.0f) + 0.5f),
						(uint8_t)((settings.CubemapColor.y * 255.0f) + 0.5f),
						(uint8_t)((settings.CubemapColor.z * 255.0f) + 0.5f),
						std::min((uint8_t)254u, (uint8_t)((settings.CubemapColor.w * 255.0f) + 0.5f)) };

					static PixelData emptyPixel{};

					subresourceData[0].pSysMem = &colorPixel;
					subresourceData[0].SysMemPitch = sizeof(PixelData);
					subresourceData[0].SysMemSlicePitch = sizeof(PixelData);

					for (uint i = 1; i < 6; i++) {
						subresourceData[i].pSysMem = &emptyPixel;
						subresourceData[i].SysMemPitch = sizeof(PixelData);
						subresourceData[i].SysMemSlicePitch = sizeof(PixelData);
					}

					ID3D11Texture2D* tempTexture;
					DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, subresourceData, &tempTexture));

					DirectX::ScratchImage image;
					DX::ThrowIfFailed(CaptureTexture(device, context, tempTexture, image));

					std::string filename = std::format("Data\\Textures\\DynamicCubemaps\\{:.2f}{:.2f}{:.2f}R{:.2f}.dds", settings.CubemapColor.x, settings.CubemapColor.y, settings.CubemapColor.z, settings.CubemapColor.w);

					std::wstring wfilename = std::wstring(filename.begin(), filename.end());
					DX::ThrowIfFailed(SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, wfilename.c_str()));

					image.Release();
					tempTexture->Release();
				}
			}
			ImGui::TreePop();
		}

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::TreePop();
	}
}

void DynamicCubemaps::DataLoaded()
{
	if (REL::Module::IsVR()) {
		// enable cubemap settings in VR
		for (const auto& settingName : iniVRCubeMapSettings) {
			if (auto setting = RE::INISettingCollection::GetSingleton()->GetSetting(settingName); setting) {
				if (!setting->data.b) {
					logger::info("[DC] Changing {} from {} to {} to support Dynamic Cubemaps", settingName, setting->data.b, true);
					setting->data.b = true;
				}
			}
		}
		for (const auto& settingPair : hiddenVRCubeMapSettings) {
			const auto& settingName = settingPair.first;
			const auto address = REL::Offset{ settingPair.second }.address();
			bool* setting = reinterpret_cast<bool*>(address);
			if (!*setting) {
				logger::info("[DC] Changing {} from {} to {} to support Dynamic Cubemaps", settingName, *setting, true);
				*setting = true;
			}
		}
	}
	MenuOpenCloseEventHandler::Register();
}

RE::BSEventNotifyControl MenuOpenCloseEventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	// When entering a new cell, reset the capture
	if (a_event->menuName == RE::LoadingMenu::MENU_NAME) {
		if (!a_event->opening)
			DynamicCubemaps::GetSingleton()->resetCapture = true;
	}
	return RE::BSEventNotifyControl::kContinue;
}

bool MenuOpenCloseEventHandler::Register()
{
	static MenuOpenCloseEventHandler singleton;
	auto ui = RE::UI::GetSingleton();

	if (!ui) {
		logger::error("UI event source not found");
		return false;
	}

	ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(&singleton);

	logger::info("Registered {}", typeid(singleton).name());

	return true;
}

void DynamicCubemaps::ClearShaderCache()
{
	if (updateCubemapCS) {
		updateCubemapCS->Release();
		updateCubemapCS = nullptr;
	}
	if (inferCubemapCS) {
		inferCubemapCS->Release();
		inferCubemapCS = nullptr;
	}
	if (inferCubemapReflectionsCS) {
		inferCubemapReflectionsCS->Release();
		inferCubemapReflectionsCS = nullptr;
	}
	if (specularIrradianceCS) {
		specularIrradianceCS->Release();
		specularIrradianceCS = nullptr;
	}
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderUpdate()
{
	if (!updateCubemapCS) {
		logger::debug("Compiling UpdateCubemapCS");
		updateCubemapCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\UpdateCubemapCS.hlsl", {}, "cs_5_0"));
	}
	return updateCubemapCS;
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderInferrence()
{
	if (!inferCubemapCS) {
		logger::debug("Compiling InferCubemapCS");
		inferCubemapCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\InferCubemapCS.hlsl", {}, "cs_5_0"));
	}
	return inferCubemapCS;
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderInferrenceReflections()
{
	if (!inferCubemapReflectionsCS) {
		logger::debug("Compiling InferCubemapCS REFLECTIONS");
		inferCubemapReflectionsCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\InferCubemapCS.hlsl", { { "REFLECTIONS", "" } }, "cs_5_0"));
	}
	return inferCubemapReflectionsCS;
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderSpecularIrradiance()
{
	if (!specularIrradianceCS) {
		logger::debug("Compiling SpecularIrradianceCS");
		specularIrradianceCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\SpecularIrradianceCS.hlsl", {}, "cs_5_0"));
	}
	return specularIrradianceCS;
}

void DynamicCubemaps::UpdateCubemapCapture()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto& context = State::GetSingleton()->context;

	auto& depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	ID3D11ShaderResourceView* srvs[2] = { depth.depthSRV, main.SRV };
	context->CSSetShaderResources(0, 2, srvs);

	ID3D11UnorderedAccessView* uavs[3] = { envCaptureTexture->uav.get(), envCaptureRawTexture->uav.get(), envCapturePositionTexture->uav.get() };
	context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);

	ID3D11Buffer* buffers[2];
	context->PSGetConstantBuffers(12, 1, buffers);

	UpdateCubemapCB updateData{};
	updateData.Reset = resetCapture;

	static float3 cameraPreviousPosAdjust = { 0, 0, 0 };
	updateData.CameraPreviousPosAdjust = cameraPreviousPosAdjust;

	auto eyePosition = Util::GetEyePosition(0);

	cameraPreviousPosAdjust = { eyePosition.x, eyePosition.y, eyePosition.z };

	updateCubemapCB->Update(updateData);
	buffers[1] = updateCubemapCB->CB();

	context->CSSetConstantBuffers(0, 2, buffers);

	resetCapture = false;

	context->CSSetSamplers(0, 1, &computeSampler);

	context->CSSetShader(GetComputeShaderUpdate(), nullptr, 0);
	context->Dispatch((uint32_t)std::ceil(envCaptureTexture->desc.Width / 8.0f), (uint32_t)std::ceil(envCaptureTexture->desc.Height / 8.0f), 6);

	uavs[0] = nullptr;
	uavs[1] = nullptr;
	uavs[2] = nullptr;
	context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);

	srvs[0] = nullptr;
	srvs[1] = nullptr;
	context->CSSetShaderResources(0, 2, srvs);

	buffers[0] = nullptr;
	buffers[1] = nullptr;
	context->CSSetConstantBuffers(0, 2, buffers);

	context->CSSetShader(nullptr, nullptr, 0);

	ID3D11SamplerState* nullSampler = { nullptr };
	context->CSSetSamplers(0, 1, &nullSampler);
}

void DynamicCubemaps::Inferrence(bool a_reflections)
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& context = State::GetSingleton()->context;

	// Infer local reflection information
	ID3D11UnorderedAccessView* uav = envInferredTexture->uav.get();

	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	context->GenerateMips(envCaptureTexture->srv.get());

	auto& cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	ID3D11ShaderResourceView* srvs[3] = { envCaptureTexture->srv.get(), cubemap.SRV, defaultCubemap };
	context->CSSetShaderResources(0, 3, srvs);

	context->CSSetSamplers(0, 1, &computeSampler);

	context->CSSetShader(a_reflections ? GetComputeShaderInferrenceReflections() : GetComputeShaderInferrence(), nullptr, 0);

	context->Dispatch((uint32_t)std::ceil(envCaptureTexture->desc.Width / 8.0f), (uint32_t)std::ceil(envCaptureTexture->desc.Height / 8.0f), 6);

	srvs[0] = nullptr;
	srvs[1] = nullptr;
	srvs[2] = nullptr;
	context->CSSetShaderResources(0, 3, srvs);

	uav = nullptr;

	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	context->CSSetShader(nullptr, 0, 0);

	ID3D11SamplerState* sampler = nullptr;
	context->CSSetSamplers(0, 1, &sampler);
}

void DynamicCubemaps::Irradiance(bool a_reflections)
{
	auto& context = State::GetSingleton()->context;

	// Copy cubemap to other resources
	for (uint face = 0; face < 6; face++) {
		uint srcSubresourceIndex = D3D11CalcSubresource(0, face, MIPLEVELS);
		context->CopySubresourceRegion(a_reflections ? envReflectionsTexture->resource.get() : envTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, envInferredTexture->resource.get(), srcSubresourceIndex, nullptr);
	}

	// Compute pre-filtered specular environment map.
	{
		auto srv = envInferredTexture->srv.get();
		context->GenerateMips(srv);

		context->CSSetShaderResources(0, 1, &srv);
		context->CSSetSamplers(0, 1, &computeSampler);
		context->CSSetShader(GetComputeShaderSpecularIrradiance(), nullptr, 0);

		ID3D11Buffer* buffer = spmapCB->CB();
		context->CSSetConstantBuffers(0, 1, &buffer);

		float const delta_roughness = 1.0f / std::max(float(MIPLEVELS - 1), 1.0f);

		std::uint32_t size = std::max(envTexture->desc.Width, envTexture->desc.Height);

		for (std::uint32_t level = 1; level < MIPLEVELS; level++, size /= 2) {
			const UINT numGroups = (UINT)std::max(1u, size / 8);

			const SpecularMapFilterSettingsCB spmapConstants = { level * delta_roughness };
			spmapCB->Update(spmapConstants);

			auto uav = a_reflections ? uavReflectionsArray[level - 1] : uavArray[level - 1];

			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			context->Dispatch(numGroups, numGroups, 6);
		}
	}

	ID3D11ShaderResourceView* nullSRV = { nullptr };
	ID3D11SamplerState* nullSampler = { nullptr };
	ID3D11Buffer* nullBuffer = { nullptr };
	ID3D11UnorderedAccessView* nullUAV = { nullptr };

	context->CSSetShaderResources(0, 1, &nullSRV);
	context->CSSetSamplers(0, 1, &nullSampler);
	context->CSSetShader(nullptr, 0, 0);
	context->CSSetConstantBuffers(0, 1, &nullBuffer);
	context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}

void DynamicCubemaps::UpdateCubemap()
{
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Cubemap Update");

	switch (nextTask) {
	case NextTask::kInferrence:
		nextTask = NextTask::kIrradiance;
		Inferrence(false);
		break;

	case NextTask::kIrradiance:
		if (activeReflections)
			nextTask = NextTask::kInferrence2;
		else
			nextTask = NextTask::kCapture;
		Irradiance(false);
		break;

	case NextTask::kInferrence2:
		Inferrence(true);
		nextTask = NextTask::kIrradiance2;
		break;

	case NextTask::kIrradiance2:
		nextTask = NextTask::kCapture;
		Irradiance(true);
		break;

	case NextTask::kCapture:
		UpdateCubemapCapture();
		nextTask = NextTask::kInferrence;
		break;
	}
}

void DynamicCubemaps::PostDeferred()
{
	auto& context = State::GetSingleton()->context;

	ID3D11ShaderResourceView* views[2] = { (activeReflections ? envReflectionsTexture : envTexture)->srv.get(), envTexture->srv.get() };
	context->PSSetShaderResources(64, 2, views);
}

void DynamicCubemaps::SetupResources()
{
	GetComputeShaderUpdate();
	GetComputeShaderInferrence();
	GetComputeShaderInferrenceReflections();
	GetComputeShaderSpecularIrradiance();

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &computeSampler));
	}

	auto& cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	{
		D3D11_TEXTURE2D_DESC texDesc;
		cubemap.texture->GetDesc(&texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		cubemap.SRV->GetDesc(&srvDesc);

		texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

		// Create additional resources

		texDesc.MipLevels = MIPLEVELS;
		texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		srvDesc.TextureCube.MipLevels = MIPLEVELS;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = texDesc.ArraySize;

		envCaptureTexture = new Texture2D(texDesc);
		envCaptureTexture->CreateSRV(srvDesc);
		envCaptureTexture->CreateUAV(uavDesc);

		envCaptureRawTexture = new Texture2D(texDesc);
		envCaptureRawTexture->CreateSRV(srvDesc);
		envCaptureRawTexture->CreateUAV(uavDesc);

		envCapturePositionTexture = new Texture2D(texDesc);
		envCapturePositionTexture->CreateSRV(srvDesc);
		envCapturePositionTexture->CreateUAV(uavDesc);

		texDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		srvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		envTexture = new Texture2D(texDesc);
		envTexture->CreateSRV(srvDesc);
		envTexture->CreateUAV(uavDesc);

		envReflectionsTexture = new Texture2D(texDesc);
		envReflectionsTexture->CreateSRV(srvDesc);
		envReflectionsTexture->CreateUAV(uavDesc);

		envInferredTexture = new Texture2D(texDesc);
		envInferredTexture->CreateSRV(srvDesc);
		envInferredTexture->CreateUAV(uavDesc);

		updateCubemapCB = new ConstantBuffer(ConstantBufferDesc<UpdateCubemapCB>());
	}

	{
		spmapCB = new ConstantBuffer(ConstantBufferDesc<SpecularMapFilterSettingsCB>());
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = envTexture->desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;

		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = envTexture->desc.ArraySize;

		for (std::uint32_t level = 1; level < MIPLEVELS; ++level) {
			uavDesc.Texture2DArray.MipSlice = level;
			DX::ThrowIfFailed(device->CreateUnorderedAccessView(envTexture->resource.get(), &uavDesc, &uavArray[level - 1]));
		}

		for (std::uint32_t level = 1; level < MIPLEVELS; ++level) {
			uavDesc.Texture2DArray.MipSlice = level;
			DX::ThrowIfFailed(device->CreateUnorderedAccessView(envReflectionsTexture->resource.get(), &uavDesc, &uavReflectionsArray[level - 1]));
		}
	}

	{
		DirectX::CreateDDSTextureFromFile(device, L"Data\\Shaders\\DynamicCubemaps\\defaultcubemap.dds", nullptr, &defaultCubemap);
	}
}

void DynamicCubemaps::Reset()
{
	if (auto sky = RE::Sky::GetSingleton())
		activeReflections = sky->mode.get() == RE::Sky::Mode::kFull;
	else
		activeReflections = false;
}
