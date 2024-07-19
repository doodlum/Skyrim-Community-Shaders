#include "VariableRateShading.h"
#include "State.h"
#include <Util.h>

HMODULE hNVAPI_DLL;

void VariableRateShading::ClearShaderCache()
{
	if (computeNASDataCS) {
		computeNASDataCS->Release();
		computeNASDataCS = nullptr;
	}
	if (computeShadingRateCS) {
		computeShadingRateCS->Release();
		computeShadingRateCS = nullptr;
	}
}

ID3D11ComputeShader* VariableRateShading::GetComputeNASData()
{
	if (!computeNASDataCS) {
		logger::debug("Compiling ComputeNASData");
		computeNASDataCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\VariableRateShading\\ComputeNASData.hlsl", {}, "cs_5_0");
	}
	return computeNASDataCS;
}

ID3D11ComputeShader* VariableRateShading::GetComputeShadingRate()
{
	if (!computeShadingRateCS) {
		logger::debug("Compiling ComputeShadingRate");
		computeShadingRateCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\VariableRateShading\\ComputeShadingRate.hlsl", {}, "cs_5_0");
	}
	return computeShadingRateCS;
}

void VariableRateShading::DrawSettings()
{
	ImGui::Checkbox("Enable Variable Rate Shading", &enableVRS);

	ImGui::Spacing();
}

void VariableRateShading::UpdateVRS()
{
	if (!vrsActive)
		return;

	const auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
	auto bTAA = !REL::Module::IsVR() ? imageSpaceManager->GetRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled :
	                                   imageSpaceManager->GetVRRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled;

	temporal = bTAA || State::GetSingleton()->upscalerLoaded;

	if (enableVRS && temporal && !RE::UI::GetSingleton()->GameIsPaused()) {
		auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto& context = State::GetSingleton()->context;

		ID3D11RenderTargetView* views[8];
		ID3D11DepthStencilView* dsv;
		context->OMGetRenderTargets(8, views, &dsv);

		ID3D11RenderTargetView* nullViews[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		ID3D11DepthStencilView* nullDsv = nullptr;
		context->OMSetRenderTargets(8, nullViews, nullDsv);

		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kMAIN];
		auto& motionVectors = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kMOTION_VECTOR];

		ID3D11ShaderResourceView* srvs[2]{
			main.SRV,
			motionVectors.SRV
		};

		context->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[1]{ reductionData->uav.get() };
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		auto shader = GetComputeNASData();
		context->CSSetShader(shader, nullptr, 0);

		context->Dispatch(reductionData->desc.Width, reductionData->desc.Height, 1);

		srvs[0] = nullptr;
		srvs[1] = nullptr;
		context->CSSetShaderResources(0, 1, srvs);

		uavs[0] = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		context->CSSetShader(nullptr, nullptr, 0);

		ComputeShadingRate();

		context->OMSetRenderTargets(8, views, dsv);

		for (int i = 0; i < 8; i++) {
			if (views[i])
				views[i]->Release();
		}

		if (dsv)
			dsv->Release();
	}
}

void VariableRateShading::ComputeShadingRate()
{
	auto& context = State::GetSingleton()->context;

	ID3D11ShaderResourceView* srvs[1]{
		reductionData->srv.get()
	};

	context->CSSetShaderResources(0, 1, srvs);

	ID3D11UnorderedAccessView* uavs[1]{ singleEyeVRSUAV[0] };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	auto shader = GetComputeShadingRate();
	context->CSSetShader(shader, nullptr, 0);

	float resolutionX = (float)reductionData->desc.Width;
	float resolutionY = (float)reductionData->desc.Height;

	uint32_t dispatchX = (uint32_t)std::ceil(resolutionX / 8.0f);
	uint32_t dispatchY = (uint32_t)std::ceil(resolutionY / 8.0f);

	context->Dispatch(dispatchX, dispatchY, 1);

	srvs[0] = nullptr;
	context->CSSetShaderResources(0, 1, srvs);

	uavs[0] = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	context->CSSetShader(nullptr, nullptr, 0);
}

std::vector<uint8_t> CreateSingleEyeFixedFoveatedVRSPattern(int width, int height)
{
	std::vector<uint8_t> data(width * height);

	enum class ShadingRate
	{
		k1x1,
		k2x2dir,
		k2x2,
		k4x4dir,
		k4x4,
	};

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			data[y * width + x] = 0;
		}
	}

	return data;
}

void VariableRateShading::Setup()
{
	Hooks::Install();

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	logger::info("Trying to load NVAPI...");

	hNVAPI_DLL = LoadLibraryA("nvapi64.dll");

	if (!hNVAPI_DLL)
		return;

	NvAPI_Status result = NvAPI_Initialize();
	if (result != NVAPI_OK) {
		return;
	}

	NV_D3D1x_GRAPHICS_CAPS caps;
	memset(&caps, 0, sizeof(NV_D3D1x_GRAPHICS_CAPS));
	NvAPI_Status status = NvAPI_D3D1x_GetGraphicsCapabilities(device, NV_D3D1x_GRAPHICS_CAPS_VER, &caps);
	if (status != NVAPI_OK || !caps.bVariablePixelRateShadingSupported) {
		logger::info("Variable rate shading is not available.");
		return;
	}

	vrsActive = true;
	logger::info("Successfully initialized NVAPI; Variable Rate Shading is available.");

	auto screenSize = State::GetSingleton()->screenSize;
	SetupSingleEyeVRS(0, (int)screenSize.x, (int)screenSize.y);

	{
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.UAV->GetDesc(&uavDesc);

		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		texDesc.Width /= 16;
		texDesc.Height /= 16;

		reductionData = new Texture2D(texDesc);
		reductionData->CreateSRV(srvDesc);
		reductionData->CreateUAV(uavDesc);
	}

	{
		for (uint i = 0; i < static_cast<uint>(REL::Relocate(RE::RENDER_TARGET::kTOTAL, RE::RENDER_TARGET::k116, RE::RENDER_TARGET::kVRTOTAL)); i++) {
			auto& target = renderer->GetRuntimeData().renderTargets[i];
			if (target.texture) {
				D3D11_TEXTURE2D_DESC texDesc{};
				target.texture->GetDesc(&texDesc);

				if (texDesc.Width == screenSize.x && texDesc.Height == screenSize.y) {
					screenTargets.insert(i);
				}
			}
		}
	}
}

void VariableRateShading::SetupSingleEyeVRS(int eye, int width, int height)
{
	auto& device = State::GetSingleton()->device;

	int vrsWidth = width / NV_VARIABLE_PIXEL_SHADING_TILE_WIDTH;
	int vrsHeight = height / NV_VARIABLE_PIXEL_SHADING_TILE_HEIGHT;

	logger::info("Creating VRS pattern texture for eye {} at {} x {}", eye, width, height);

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = vrsWidth;
	texDesc.Height = vrsHeight;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8_UINT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	texDesc.MipLevels = 1;
	auto data = CreateSingleEyeFixedFoveatedVRSPattern(vrsWidth, vrsHeight);
	D3D11_SUBRESOURCE_DATA subresourceData;
	subresourceData.pSysMem = data.data();
	subresourceData.SysMemPitch = vrsWidth;
	subresourceData.SysMemSlicePitch = 0;
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, &subresourceData, &singleEyeVRSTex[eye]));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = texDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	DX::ThrowIfFailed(device->CreateUnorderedAccessView(singleEyeVRSTex[eye], &uavDesc, &singleEyeVRSUAV[eye]));

	logger::info("Creating shading rate resource view for eye {}", eye);
	NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC vd = {};
	vd.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
	vd.Format = texDesc.Format;
	vd.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
	vd.Texture2D.MipSlice = 0;
	NvAPI_Status status = NvAPI_D3D11_CreateShadingRateResourceView(device, singleEyeVRSTex[eye], &vd, &singleEyeVRSView[eye]);
	if (status != NVAPI_OK) {
		logger::info("Failed to create VRS pattern view for eye {}", eye);
		return;
	}
}

void VariableRateShading::UpdateViews(bool a_enable)
{
	if (!vrsActive)
		return;

	auto& context = State::GetSingleton()->context;

	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	GET_INSTANCE_MEMBER(renderTargets, state);
	GET_INSTANCE_MEMBER(depthStencil, state);

	vrsPass = screenTargets.contains(renderTargets[0]);

	if (depthStencil == RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN || depthStencil == RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY) {
		vrsPass = true;
	}

	vrsPass = enableVRS && vrsPass && temporal && a_enable && notLandscape && !RE::UI::GetSingleton()->GameIsPaused();

	static bool currentVRS = false;

	if (currentVRS == vrsPass)
		return;

	currentVRS = vrsPass;

	if (vrsPass) {
		ID3D11NvShadingRateResourceView* shadingRateView = singleEyeVRSView[0];

		NvAPI_Status statusSRRV = NvAPI_D3D11_RSSetShadingRateResourceView(context, shadingRateView);
		if (statusSRRV != NVAPI_OK)
			logger::info("Setting the shading rate resource view failed");
	}

	NV_D3D11_VIEWPORT_SHADING_RATE_DESC vsrd[8];
	for (uint i = 0; i < 8; i++) {
		vsrd[i].enableVariablePixelShadingRate = false;
		memset(vsrd[i].shadingRateTable, NV_PIXEL_X0_CULL_RASTER_PIXELS, sizeof(vsrd[0].shadingRateTable));
	}

	uint viewportCount = 8;

	if (vrsPass) {
		for (uint i = 0; i < viewportCount; i++) {
			vsrd[i].enableVariablePixelShadingRate = true;
			memset(vsrd[i].shadingRateTable, NV_PIXEL_X1_PER_RASTER_PIXEL, sizeof(vsrd[i].shadingRateTable));
			vsrd[i].shadingRateTable[0] = NV_PIXEL_X1_PER_RASTER_PIXEL;
			vsrd[i].shadingRateTable[1] = NV_PIXEL_X1_PER_2X1_RASTER_PIXELS;
			vsrd[i].shadingRateTable[2] = NV_PIXEL_X1_PER_1X2_RASTER_PIXELS;
			vsrd[i].shadingRateTable[3] = NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;
			vsrd[i].shadingRateTable[4] = NV_PIXEL_X1_PER_4X2_RASTER_PIXELS;
			vsrd[i].shadingRateTable[5] = NV_PIXEL_X1_PER_2X4_RASTER_PIXELS;
			vsrd[i].shadingRateTable[6] = NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;
		}
	}

	NV_D3D11_VIEWPORTS_SHADING_RATE_DESC srd;
	srd.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
	srd.numViewports = viewportCount;
	srd.pViewports = vsrd;

	{
		NvAPI_Status statusVPSR = NvAPI_D3D11_RSSetViewportsPixelShadingRates(context, &srd);
		if (statusVPSR != NVAPI_OK)
			logger::info("Setting the viewport pixel shading rate failed");
	}
}