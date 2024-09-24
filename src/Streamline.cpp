#include "Streamline.h"

#include <dxgi.h>

#include "Util.h"

void LoggingCallback(sl::LogType type, const char* msg)
{
	switch (type) {
	case sl::LogType::eInfo:
		logger::info("{}", msg);
		break;
	case sl::LogType::eWarn:
		logger::warn("{}", msg);
		break;
	case sl::LogType::eError:
		logger::error("{}", msg);
		break;
	}
}

ID3D11ComputeShader* Streamline::GetRCASComputeShader()
{
	static auto currentSharpness = sharpness;

	if (currentSharpness != sharpness)
	{
		currentSharpness = sharpness;

		if (rcasCS) {
			rcasCS->Release();
			rcasCS = nullptr;
		}
	}

	if (!rcasCS) {
		logger::debug("Compiling Utility.hlsl");
		rcasCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\RCAS\\RCAS.hlsl", { { "SHARPNESS", std::format("{}", sharpness).c_str() } }, "cs_5_0");
	}
	return rcasCS;
}

void Streamline::ClearShaderCache()
{
	if (rcasCS) {
		rcasCS->Release();
		rcasCS = nullptr;
	}
}

void Streamline::DrawSettings()
{
	if (!REL::Module::IsVR()) {
		if (ImGui::CollapsingHeader("NVIDIA DLSS", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			if (featureDLSSG) {
				ImGui::Text("Frame Generation uses a D3D11 to D3D12 proxy");
				ImGui::Text("Frame Generation always defaults to Auto");
				ImGui::Text("To disable Frame Generation, disable it in your mod manager");

				const char* frameGenerationModes[] = { "Off", "On", "Auto" };
				ImGui::SliderInt("Frame Generation", (int*)&frameGenerationMode, 0, 2, std::format("{}", frameGenerationModes[(uint)frameGenerationMode]).c_str());
				frameGenerationMode = (sl::DLSSGMode)std::min(2u, (uint)frameGenerationMode);
			} else {
				ImGui::Text("Frame Generation uses a D3D11 to D3D12 proxy");
				ImGui::Text("To enable Frame Generation, enable it in your mod manager and use a compatible GPU");
			}

			ImGui::Checkbox("DLAA", &enableDLAA);
			ImGui::SliderFloat("Sharpness", &sharpness, 0.0f, 1.0f, "%.1f");
			sharpness = std::clamp(sharpness, 0.0f, 1.0f);
		}
	}
}

void Streamline::Initialize_preDevice()
{
	logger::info("[Streamline] Initializing Streamline");

	interposer = LoadLibraryW(L"Data/SKSE/Plugins/Streamline/sl.interposer.dll");

	sl::Preferences pref;

	sl::Feature featuresToLoad[] = { sl::kFeatureDLSS, sl::kFeatureDLSS_G, sl::kFeatureReflex };
	pref.featuresToLoad = featuresToLoad;
	pref.numFeaturesToLoad = _countof(featuresToLoad);

	pref.logLevel = sl::LogLevel::eOff;
	pref.logMessageCallback = LoggingCallback;
	pref.showConsole = false;

	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "1.0.0";
	pref.projectId = "f8776929-c969-43bd-ac2b-294b4de58aac";
	pref.flags |= sl::PreferenceFlags::eUseManualHooking;

	pref.renderAPI = sl::RenderAPI::eD3D11;

	// Hook up all of the functions exported by the SL Interposer Library
	slInit = (PFun_slInit*)GetProcAddress(interposer, "slInit");
	slShutdown = (PFun_slShutdown*)GetProcAddress(interposer, "slShutdown");
	slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(interposer, "slIsFeatureSupported");
	slIsFeatureLoaded = (PFun_slIsFeatureLoaded*)GetProcAddress(interposer, "slIsFeatureLoaded");
	slSetFeatureLoaded = (PFun_slSetFeatureLoaded*)GetProcAddress(interposer, "slSetFeatureLoaded");
	slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(interposer, "slEvaluateFeature");
	slAllocateResources = (PFun_slAllocateResources*)GetProcAddress(interposer, "slAllocateResources");
	slFreeResources = (PFun_slFreeResources*)GetProcAddress(interposer, "slFreeResources");
	slSetTag = (PFun_slSetTag*)GetProcAddress(interposer, "slSetTag");
	slGetFeatureRequirements = (PFun_slGetFeatureRequirements*)GetProcAddress(interposer, "slGetFeatureRequirements");
	slGetFeatureVersion = (PFun_slGetFeatureVersion*)GetProcAddress(interposer, "slGetFeatureVersion");
	slUpgradeInterface = (PFun_slUpgradeInterface*)GetProcAddress(interposer, "slUpgradeInterface");
	slSetConstants = (PFun_slSetConstants*)GetProcAddress(interposer, "slSetConstants");
	slGetNativeInterface = (PFun_slGetNativeInterface*)GetProcAddress(interposer, "slGetNativeInterface");
	slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(interposer, "slGetFeatureFunction");
	slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(interposer, "slGetNewFrameToken");
	slSetD3DDevice = (PFun_slSetD3DDevice*)GetProcAddress(interposer, "slSetD3DDevice");

	if (SL_FAILED(res, slInit(pref, sl::kSDKVersion))) {
		logger::critical("[Streamline] Failed to initialize Streamline");
	} else {
		logger::info("[Streamline] Sucessfully initialized Streamline");
	}

	initialized = true;
}

void Streamline::Initialize_postDevice()
{
	// Hook up all of the feature functions using the sl function slGetFeatureFunction

	if (featureDLSS) {
		slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", (void*&)slDLSSGetOptimalSettings);
		slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetState", (void*&)slDLSSGetState);
		slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", (void*&)slDLSSSetOptions);
	}

	if (featureDLSSG) {
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGGetState", (void*&)slDLSSGGetState);
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", (void*&)slDLSSGSetOptions);
	}

	if (featureReflex) {
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexGetState", (void*&)slReflexGetState);
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetMarker", (void*&)slReflexSetMarker);
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexSleep", (void*&)slReflexSleep);
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", (void*&)slReflexSetOptions);
	}
}

HRESULT Streamline::CreateDXGIFactory(REFIID riid, void** ppFactory)
{
	if (!initialized)
		Initialize_preDevice();

	logger::info("[Streamline] Proxying CreateDXGIFactory");

	auto slCreateDXGIFactory1 = reinterpret_cast<decltype(&CreateDXGIFactory1)>(GetProcAddress(interposer, "CreateDXGIFactory1"));

	return slCreateDXGIFactory1(riid, ppFactory);
}

extern decltype(&D3D11CreateDeviceAndSwapChain) ptrD3D11CreateDeviceAndSwapChain;

HRESULT Streamline::CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext)
{
	DXGI_ADAPTER_DESC adapterDesc;
	pAdapter->GetDesc(&adapterDesc);

	sl::AdapterInfo adapterInfo;
	adapterInfo.deviceLUID = (uint8_t*)&adapterDesc.AdapterLuid;
	adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);

	slIsFeatureLoaded(sl::kFeatureDLSS, featureDLSS);
	if (featureDLSS) {
		logger::info("[Streamline] DLSS feature is loaded");
		featureDLSS = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo) == sl::Result::eOk;
	} else {
		logger::info("[Streamline] DLSS feature is not loaded");
		sl::FeatureRequirements featureRequirements;
		sl::Result result = slGetFeatureRequirements(sl::kFeatureDLSS, featureRequirements);
		if (result != sl::Result::eOk) {
			logger::info("[Streamline] DLSS feature failed to load due to: {}", magic_enum::enum_name(result));
		}
	}

	slIsFeatureLoaded(sl::kFeatureDLSS_G, featureDLSSG);
	if (featureDLSSG) {
		logger::info("[Streamline] DLSSG feature is loaded");
		featureDLSSG = slIsFeatureSupported(sl::kFeatureDLSS_G, adapterInfo) == sl::Result::eOk;
	} else {
		logger::info("[Streamline] DLSSG feature is not loaded");
		sl::FeatureRequirements featureRequirements;
		sl::Result result = slGetFeatureRequirements(sl::kFeatureDLSS_G, featureRequirements);
		if (result != sl::Result::eOk) {
			logger::info("[Streamline] DLSSG feature failed to load due to: {}", magic_enum::enum_name(result));
		}
	}

	slIsFeatureLoaded(sl::kFeatureReflex, featureReflex);
	if (featureReflex) {
		logger::info("[Streamline] Reflex feature is loaded");
		featureReflex = slIsFeatureSupported(sl::kFeatureReflex, adapterInfo) == sl::Result::eOk;
	} else {
		logger::info("[Streamline] Reflex feature is not loaded");
		sl::FeatureRequirements featureRequirements;
		sl::Result result = slGetFeatureRequirements(sl::kFeatureReflex, featureRequirements);
		if (result != sl::Result::eOk) {
			logger::info("[Streamline] Reflex feature failed to load due to: {}", magic_enum::enum_name(result));
		}
	}

	logger::info("[Streamline] DLSS {} available", featureDLSS ? "is" : "is not");
	logger::info("[Streamline] DLSSG {} available", featureDLSSG ? "is" : "is not");
	logger::info("[Streamline] Reflex {} available", featureReflex ? "is" : "is not");

	HRESULT hr = 0;

	if (featureDLSSG) {
		logger::info("[Streamline] Proxying D3D11CreateDeviceAndSwapChain to add D3D12 swapchain");

		auto slD3D11CreateDeviceAndSwapChain = reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(GetProcAddress(interposer, "D3D11CreateDeviceAndSwapChain"));

		hr = slD3D11CreateDeviceAndSwapChain(
			pAdapter,
			DriverType,
			Software,
			Flags,
			pFeatureLevels,
			FeatureLevels,
			SDKVersion,
			pSwapChainDesc,
			ppSwapChain,
			ppDevice,
			pFeatureLevel,
			ppImmediateContext);
	} else {
		hr = ptrD3D11CreateDeviceAndSwapChain(
			pAdapter,
			DriverType,
			Software,
			Flags,
			pFeatureLevels,
			FeatureLevels,
			SDKVersion,
			pSwapChainDesc,
			ppSwapChain,
			ppDevice,
			pFeatureLevel,
			ppImmediateContext);

		slSetD3DDevice(*ppDevice);
	}

	Initialize_postDevice();

	return hr;
}

void Streamline::SetupResources()
{
	if (featureDLSS) {
		auto state = State::GetSingleton();

		sl::DLSSOptions dlssOptions{};
		dlssOptions.mode = sl::DLSSMode::eMaxQuality;
		dlssOptions.outputWidth = (uint)state->screenSize.x;
		dlssOptions.outputHeight = (uint)state->screenSize.y;
		dlssOptions.colorBuffersHDR = sl::Boolean::eFalse;
		dlssOptions.preExposure = 1.0f;
		dlssOptions.sharpness = 0.0f;
		dlssOptions.dlaaPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.balancedPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.qualityPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.ultraPerformancePreset = sl::DLSSPreset::ePresetE;
		dlssOptions.performancePreset = sl::DLSSPreset::ePresetE;

		if (SL_FAILED(result, slDLSSSetOptions(viewport, dlssOptions))) {
			logger::critical("[Streamline] Could not enable DLSS");
		} else {
			logger::info("[Streamline] Successfully enabled DLSS");
		}
	}

	if (featureDLSSG) {
		sl::DLSSGOptions options{};
		options.mode = sl::DLSSGMode::eAuto;
		options.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;

		if (SL_FAILED(result, slDLSSGSetOptions(viewport, options))) {
			logger::critical("[Streamline] Could not enable DLSSG");
		} else {
			logger::info("[Streamline] Successfully enabled DLSSG");
		}
	}

	if (featureReflex) {
		sl::ReflexOptions reflexOptions{};
		reflexOptions.mode = sl::ReflexMode::eLowLatencyWithBoost;
		reflexOptions.useMarkersToOptimize = false;
		reflexOptions.virtualKey = 0;
		reflexOptions.frameLimitUs = 0;

		if (SL_FAILED(res, slReflexSetOptions(reflexOptions))) {
			logger::error("[Streamline] Failed to set reflex options");
		} else {
			logger::info("[Streamline] Sucessfully set reflex options");
		}
	}

	if (featureDLSS || featureDLSSG) {
		logger::info("[Streamline] Creating resources");

		auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.RTV->GetDesc(&rtvDesc);
		main.UAV->GetDesc(&uavDesc);

		if (featureDLSSG) {
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

			texDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.Format = texDesc.Format;
			rtvDesc.Format = texDesc.Format;
			uavDesc.Format = texDesc.Format;

			depthBufferShared = new Texture2D(texDesc);
			depthBufferShared->CreateSRV(srvDesc);
			depthBufferShared->CreateRTV(rtvDesc);
			depthBufferShared->CreateUAV(uavDesc);

			copyDepthToSharedBufferCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\Streamline\\CopyDepthToSharedBufferCS.hlsl", {}, "cs_5_0");
		}

		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Format = texDesc.Format;
		rtvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		colorBufferShared = new Texture2D(texDesc);
		colorBufferShared->CreateSRV(srvDesc);
		colorBufferShared->CreateRTV(rtvDesc);
		colorBufferShared->CreateUAV(uavDesc);		
	}
}

void Streamline::CopyResourcesToSharedBuffers()
{
	if (!featureDLSSG || frameGenerationMode == sl::DLSSGMode::eOff)
		return;

	auto& context = State::GetSingleton()->context;
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	if (RE::UI::GetSingleton()->GameIsPaused()) {
		float clearColor[4] = { 0, 0, 0, 0 };
		auto& motionVectorsBuffer = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::RENDER_TARGET::kMOTION_VECTOR];
		context->ClearRenderTargetView(motionVectorsBuffer.RTV, clearColor);
	}

	ID3D11RenderTargetView* backupViews[8];
	ID3D11DepthStencilView* backupDsv;
	context->OMGetRenderTargets(8, backupViews, &backupDsv);  // Backup bound render targets
	context->OMSetRenderTargets(0, nullptr, nullptr);         // Unbind all bound render targets

	auto& swapChain = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];

	ID3D11Resource* swapChainResource;
	swapChain.SRV->GetResource(&swapChainResource);

	context->CopyResource(colorBufferShared->resource.get(), swapChainResource);

	{
		auto& depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

		{
			auto dispatchCount = Util::GetScreenDispatchCount(true);

			ID3D11ShaderResourceView* views[1] = { depth.depthSRV };
			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[1] = { depthBufferShared->uav.get() };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(copyDepthToSharedBufferCS, nullptr, 0);

			context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
		}

		ID3D11ShaderResourceView* views[1] = { nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11ComputeShader* shader = nullptr;
		context->CSSetShader(shader, nullptr, 0);
	}

	context->OMSetRenderTargets(8, backupViews, backupDsv);  // Restore all bound render targets

	for (int i = 0; i < 8; i++) {
		if (backupViews[i])
			backupViews[i]->Release();
	}

	if (backupDsv)
		backupDsv->Release();
}

void Streamline::Present()
{
	if (!featureDLSSG)
		return;

	UpdateConstants();

	static auto currentFrameGenerationMode = frameGenerationMode;

	if (currentFrameGenerationMode != frameGenerationMode) {
		currentFrameGenerationMode = frameGenerationMode;

		sl::DLSSGOptions options{};
		options.mode = frameGenerationMode;
		options.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;

		if (SL_FAILED(result, slDLSSGSetOptions(viewport, options))) {
			logger::error("[Streamline] Could not set DLSSG");
		}
	}

	if (featureReflex) {
		// Fake NVIDIA Reflex to prevent DLSSG errors
		slReflexSetMarker(sl::ReflexMarker::eInputSample, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::eSimulationStart, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::eSimulationEnd, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::eRenderSubmitStart, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::eRenderSubmitEnd, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::ePresentStart, *frameToken);
		slReflexSetMarker(sl::ReflexMarker::ePresentEnd, *frameToken);
	}

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto& motionVectorsBuffer = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::RENDER_TARGET::kMOTION_VECTOR];

	auto state = State::GetSingleton();

	sl::Extent fullExtent{ 0, 0, (uint)state->screenSize.x, (uint)state->screenSize.y };

	float2 dynamicScreenSize = Util::ConvertToDynamic(State::GetSingleton()->screenSize);
	sl::Extent dynamicExtent{ 0, 0, (uint)dynamicScreenSize.x, (uint)dynamicScreenSize.y };

	sl::Resource depth = { sl::ResourceType::eTex2d, depthBufferShared->resource.get(), 0 };
	sl::ResourceTag depthTag = sl::ResourceTag{ &depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &dynamicExtent };

	sl::Resource mvec = { sl::ResourceType::eTex2d, motionVectorsBuffer.texture, 0 };
	sl::ResourceTag mvecTag = sl::ResourceTag{ &mvec, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &dynamicExtent };

	sl::Resource hudLess = { sl::ResourceType::eTex2d, colorBufferShared->resource.get(), 0 };
	sl::ResourceTag hudLessTag = sl::ResourceTag{ &hudLess, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

	sl::Resource ui = { sl::ResourceType::eTex2d, nullptr, 0 };
	sl::ResourceTag uiTag = sl::ResourceTag{ &ui, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

	sl::ResourceTag inputs[] = { depthTag, mvecTag, hudLessTag, uiTag };
	slSetTag(viewport, inputs, _countof(inputs), state->context);
}

void BSGraphics_SetDirtyStates(bool isCompute);

extern decltype(&BSGraphics_SetDirtyStates) ptr_BSGraphics_SetDirtyStates;

void Streamline::Upscale()
{
	UpdateConstants();

	(ptr_BSGraphics_SetDirtyStates)(false);  // Our hook skips this call so we need to call manually

	auto state = State::GetSingleton();
	state->BeginPerfEvent("Upscaling");

	auto& context = state->context;

	ID3D11ShaderResourceView* inputTextureSRV;
	context->PSGetShaderResources(0, 1, &inputTextureSRV);

	inputTextureSRV->Release();

	ID3D11RenderTargetView* outputTextureRTV;
	ID3D11DepthStencilView* dsv;
	context->OMGetRenderTargets(1, &outputTextureRTV, &dsv);
	context->OMSetRenderTargets(0, nullptr, nullptr);

	outputTextureRTV->Release();

	if (dsv)
		dsv->Release();

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto& depthTexture = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto& motionVectorsTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMOTION_VECTOR];
	auto& transparencyMaskTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kTEMPORAL_AA_MASK];

	ID3D11Resource* inputTextureResource;
	inputTextureSRV->GetResource(&inputTextureResource);

	ID3D11Resource* outputTextureResource;
	outputTextureRTV->GetResource(&outputTextureResource);
	
	{
		sl::DLSSOptions dlssOptions{};
		dlssOptions.mode = sl::DLSSMode::eMaxQuality;
		dlssOptions.outputWidth = (uint)state->screenSize.x;
		dlssOptions.outputHeight = (uint)state->screenSize.y;
		dlssOptions.colorBuffersHDR = sl::Boolean::eFalse;
		dlssOptions.preExposure = 1.0f;
		dlssOptions.sharpness = 0.0f;
		dlssOptions.dlaaPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.balancedPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.qualityPreset = sl::DLSSPreset::ePresetE;
		dlssOptions.ultraPerformancePreset = sl::DLSSPreset::ePresetE;
		dlssOptions.performancePreset = sl::DLSSPreset::ePresetE;

		slDLSSSetOptions(viewport, dlssOptions);
	}

	{
		sl::Extent fullExtent{ 0, 0, (uint)state->screenSize.x, (uint)state->screenSize.y };

		sl::Resource colorIn = { sl::ResourceType::eTex2d, inputTextureResource, 0 };
		sl::Resource colorOut = { sl::ResourceType::eTex2d, colorBufferShared->resource.get(), 0 };
		sl::Resource depth = { sl::ResourceType::eTex2d, depthTexture.texture, 0 };
		sl::Resource mvec = { sl::ResourceType::eTex2d, motionVectorsTexture.texture, 0 };

		sl::ResourceTag colorInTag = sl::ResourceTag{ &colorIn, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow, &fullExtent };
		sl::ResourceTag colorOutTag = sl::ResourceTag{ &colorOut, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow, &fullExtent };
		sl::ResourceTag depthTag = sl::ResourceTag{ &depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };
		sl::ResourceTag mvecTag = sl::ResourceTag{ &mvec, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

		sl::Resource transparencyMask = { sl::ResourceType::eTex2d, transparencyMaskTexture.texture, 0 };
		sl::ResourceTag transparencyMaskTag = sl::ResourceTag{ &transparencyMask, sl::kBufferTypeTransparencyHint, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

		sl::ResourceTag resourceTags[] = { colorInTag, colorOutTag, depthTag, mvecTag, transparencyMaskTag };
		slSetTag(viewport, resourceTags, _countof(resourceTags), state->context);
	}
		
	sl::ViewportHandle view(viewport);
	const sl::BaseStructure* inputs[] = { &view };
	slEvaluateFeature(sl::kFeatureDLSS, *frameToken, inputs, _countof(inputs), state->context);
	
	context->CopyResource(inputTextureResource, colorBufferShared->resource.get());

	state->EndPerfEvent();

	state->BeginPerfEvent("Sharpening");

	{
		auto dispatchCount = Util::GetScreenDispatchCount(false);

		{
			ID3D11ShaderResourceView* views[1] = { inputTextureSRV };
			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[1] = { colorBufferShared->uav.get() };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(GetRCASComputeShader(), nullptr, 0);

			context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
		}

		ID3D11ShaderResourceView* views[1] = { nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11ComputeShader* shader = nullptr;
		context->CSSetShader(shader, nullptr, 0);
	}

	context->CopyResource(outputTextureResource, colorBufferShared->resource.get());

	state->EndPerfEvent();
}

void Streamline::UpdateConstants()
{
	static Util::FrameChecker frameChecker;
	if (!frameChecker.isNewFrame())
		return;

	auto state = State::GetSingleton();

	auto cameraData = Util::GetCameraData(0);
	auto eyePosition = Util::GetEyePosition(0);

	auto clipToCameraView = cameraData.viewMat.Invert();
	auto cameraToWorld = cameraData.viewProjMatrixUnjittered.Invert();
	auto cameraToWorldPrev = cameraData.previousViewProjMatrixUnjittered.Invert();

	auto gameViewport = RE::BSGraphics::State::GetSingleton();

	float4x4 cameraToPrevCamera;

	calcCameraToPrevCamera(*(sl::float4x4*)&cameraToPrevCamera, *(sl::float4x4*)&cameraToWorld, *(sl::float4x4*)&cameraToWorldPrev);

	float4x4 prevCameraToCamera = cameraToPrevCamera;

	prevCameraToCamera.Invert();

	sl::Constants slConstants = {};
	slConstants.cameraAspectRatio = state->screenSize.x / state->screenSize.y;
	slConstants.cameraFOV = Util::GetVerticalFOVRad();
	slConstants.cameraFar = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x44));
	slConstants.cameraMotionIncluded = sl::Boolean::eTrue;
	slConstants.cameraNear = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x40));
	slConstants.cameraPinholeOffset = { 0.f, 0.f };  
	slConstants.cameraPos = *(sl::float3*)&eyePosition;
	slConstants.cameraFwd = *(sl::float3*)&cameraData.viewForward;
	slConstants.cameraUp = *(sl::float3*)&cameraData.viewUp;
	slConstants.cameraRight = *(sl::float3*)&cameraData.viewRight;
	slConstants.cameraViewToClip = *(sl::float4x4*)&cameraData.viewMat;
	slConstants.clipToCameraView = *(sl::float4x4*)&clipToCameraView;
	slConstants.clipToPrevClip = *(sl::float4x4*)&cameraToPrevCamera;
	slConstants.depthInverted = sl::Boolean::eFalse;
	slConstants.jitterOffset = { gameViewport->projectionPosScaleX, gameViewport->projectionPosScaleY };
	slConstants.mvecScale = { 1, 1 };
	slConstants.prevClipToClip = *(sl::float4x4*)&prevCameraToCamera;
	slConstants.reset = sl::Boolean::eFalse;
	slConstants.motionVectors3D = sl::Boolean::eTrue;
	slConstants.motionVectorsInvalidValue = FLT_MIN;
	slConstants.orthographicProjection = sl::Boolean::eFalse;
	slConstants.motionVectorsDilated = sl::Boolean::eFalse;
	slConstants.motionVectorsJittered = sl::Boolean::eFalse;

	if (SL_FAILED(res, slGetNewFrameToken(frameToken, nullptr))) {
		logger::error("[Streamline] Could not get frame token");
	}

	if (SL_FAILED(res, slSetConstants(slConstants, *frameToken, viewport))) {
		logger::error("[Streamline] Could not set constants");
	}
}
