#include "Weather.h"

#include "Deferred.h"
#include "Util.h"

static RE::Color dalcColors[6];

void Weather::DrawSettings()
{
	if (ImGui::Button("Update DALC values")) {
		auto& context = State::GetSingleton()->context;

		// Copy the cubemap to the staging texture
		context->CopyResource(diffuseIBLCubemapCPUTexture->resource.get(), diffuseIBLCubemapTexture->resource.get());

		// Read back data from each face (6 faces)
		for (UINT face = 0; face < 6; ++face) {
			D3D11_MAPPED_SUBRESOURCE mappedResource = {};
			auto hr = context->Map(diffuseIBLCubemapCPUTexture->resource.get(), face, D3D11_MAP_READ, 0, &mappedResource);
			if (SUCCEEDED(hr)) {
				// Assuming format is DXGI_FORMAT_R8G8B8A8_UNORM (adjust based on your format)
				uint32_t* pixelData = reinterpret_cast<uint32_t*>(mappedResource.pData);

				uint8_t r = (*pixelData >> 0) & 0xFF;
				uint8_t g = (*pixelData >> 8) & 0xFF;
				uint8_t b = (*pixelData >> 16) & 0xFF;

				dalcColors[face] = RE::Color{ r, g, b, 0 };

				context->Unmap(diffuseIBLCubemapCPUTexture->resource.get(), face);
			}
		}
	}

	ImGui::Text("DALC values");

	std::string faceStr[6]{ "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

	for (UINT face = 0; face < 6; ++face) {
		std::string rText = std::format("R {}", dalcColors[face].red);
		std::string gText = std::format("G {}", dalcColors[face].green);
		std::string bText = std::format("B {}", dalcColors[face].blue);

		ImGui::Text("%s:", faceStr[face].c_str());  // Display the face name

		// Make each color channel selectable and copy its value when clicked
		if (ImGui::Button(rText.c_str())) {
			ImGui::SetClipboardText(std::to_string(dalcColors[face].red).c_str());
		}
		ImGui::SameLine();

		if (ImGui::Button(gText.c_str())) {
			ImGui::SetClipboardText(std::to_string(dalcColors[face].green).c_str());
		}
		ImGui::SameLine();

		if (ImGui::Button(bText.c_str())) {
			ImGui::SetClipboardText(std::to_string(dalcColors[face].blue).c_str());
		}
		ImGui::Separator();
	}
}

void Weather::Bind()
{
	if (loaded) {
		auto& context = State::GetSingleton()->context;

		// Set PS shader resource
		{
			ID3D11ShaderResourceView* srv = diffuseIBLCubemapTexture->srv.get();
			context->PSSetShaderResources(100, 1, &srv);
		}
	}
}

void Weather::Prepass()
{
	auto& context = State::GetSingleton()->context;

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& reflections = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGET_CUBEMAP::kREFLECTIONS];

	std::array<ID3D11ShaderResourceView*, 1> srvs = { reflections.SRV };
	std::array<ID3D11UnorderedAccessView*, 1> uavs = { diffuseIBLCubemapTexture->uav.get() };
	std::array<ID3D11SamplerState*, 1> samplers = { Deferred::GetSingleton()->linearSampler };

	// Unset PS shader resource
	{
		ID3D11ShaderResourceView* srv = nullptr;
		context->PSSetShaderResources(100, 1, &srv);
	}

	// IBL
	{
		samplers[0] = Deferred::GetSingleton()->linearSampler;

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(GetDiffuseIBLCS(), nullptr, 0);
		context->Dispatch(1, 1, 6);
	}

	// Reset
	{
		srvs.fill(nullptr);
		uavs.fill(nullptr);
		samplers.fill(nullptr);

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
	}

	// Set PS shader resource
	{
		ID3D11ShaderResourceView* srv = diffuseIBLCubemapTexture->srv.get();
		context->PSSetShaderResources(100, 1, &srv);
	}
}

void Weather::SetupResources()
{
	GetDiffuseIBLCS();

	{
		D3D11_TEXTURE2D_DESC texDesc{
			.Width = 3,
			.Height = 1,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
			.SampleDesc = { 1, 0 },
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
			.CPUAccessFlags = 0,
			.MiscFlags = 0
		};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = texDesc.MipLevels }
		};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = { .MipSlice = 0 }
		};

		diffuseIBLTexture = new Texture2D(texDesc);
		diffuseIBLTexture->CreateSRV(srvDesc);
		diffuseIBLTexture->CreateUAV(uavDesc);
	}

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	{
		D3D11_TEXTURE2D_DESC texDesc;
		cubemap.texture->GetDesc(&texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		cubemap.SRV->GetDesc(&srvDesc);

		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Format = texDesc.Format;

		texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		texDesc.Width = 1;
		texDesc.Height = 1;

		// Create additional resources

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = texDesc.ArraySize;

		diffuseIBLCubemapTexture = new Texture2D(texDesc);
		diffuseIBLCubemapTexture->CreateSRV(srvDesc);
		diffuseIBLCubemapTexture->CreateUAV(uavDesc);

		texDesc.Usage = D3D11_USAGE_STAGING;
		texDesc.BindFlags = 0;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		texDesc.MiscFlags = 0;  // Remove cubemap flag

		diffuseIBLCubemapCPUTexture = new Texture2D(texDesc);
	}
}

void Weather::ClearShaderCache()
{
	if (diffuseIBLCS)
		diffuseIBLCS->Release();
	diffuseIBLCS = nullptr;
}

ID3D11ComputeShader* Weather::GetDiffuseIBLCS()
{
	if (!diffuseIBLCS)
		diffuseIBLCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\Weather\\IrmapCS.hlsl", {}, "cs_5_0"));
	return diffuseIBLCS;
}
