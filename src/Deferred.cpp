#include "Deferred.h"

#include "ShaderCache.h"
#include "State.h"
#include "Util.h"

#include "Features/DynamicCubemaps.h"
#include "Features/ScreenSpaceGI.h"
#include "Features/Skylighting.h"
#include "Features/SubsurfaceScattering.h"

struct DepthStates
{
	ID3D11DepthStencilState* a[6][40];
};

struct BlendStates
{
	ID3D11BlendState* a[7][2][13][2];
};

void SetupRenderTarget(RE::RENDER_TARGET target, D3D11_TEXTURE2D_DESC texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc, D3D11_RENDER_TARGET_VIEW_DESC rtvDesc, D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc, DXGI_FORMAT format)
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	texDesc.Format = format;
	srvDesc.Format = format;
	rtvDesc.Format = format;
	uavDesc.Format = format;

	auto& data = renderer->GetRuntimeData().renderTargets[target];
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, nullptr, &data.texture));
	DX::ThrowIfFailed(device->CreateShaderResourceView(data.texture, &srvDesc, &data.SRV));
	DX::ThrowIfFailed(device->CreateRenderTargetView(data.texture, &rtvDesc, &data.RTV));
	DX::ThrowIfFailed(device->CreateUnorderedAccessView(data.texture, &uavDesc, &data.UAV));
}

void Deferred::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	{
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.RTV->GetDesc(&rtvDesc);
		main.UAV->GetDesc(&uavDesc);

		// Available targets:
		// MAIN ONLY ALPHA
		// WATER REFLECTIONS
		// BLURFULL_BUFFER
		// LENSFLAREVIS
		// SAO DOWNSCALED
		// SAO CAMERAZ+MIP_LEVEL_0_ESRAM
		// SAO_RAWAO_DOWNSCALED
		// SAO_RAWAO_PREVIOUS_DOWNSCALDE
		// SAO_TEMP_BLUR_DOWNSCALED
		// INDIRECT
		// INDIRECT_DOWNSCALED
		// RAWINDIRECT
		// RAWINDIRECT_DOWNSCALED
		// RAWINDIRECT_PREVIOUS
		// RAWINDIRECT_PREVIOUS_DOWNSCALED
		// RAWINDIRECT_SWAP
		// VOLUMETRIC_LIGHTING_HALF_RES
		// VOLUMETRIC_LIGHTING_BLUR_HALF_RES
		// VOLUMETRIC_LIGHTING_QUARTER_RES
		// VOLUMETRIC_LIGHTING_BLUR_QUARTER_RES
		// TEMPORAL_AA_WATER_1
		// TEMPORAL_AA_WATER_2

		// Albedo
		SetupRenderTarget(ALBEDO, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Specular
		SetupRenderTarget(SPECULAR, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R11G11B10_FLOAT);
		// Reflectance
		SetupRenderTarget(REFLECTANCE, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Normal + Roughness
		SetupRenderTarget(NORMALROUGHNESS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Masks
		SetupRenderTarget(MASKS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Additional Masks
		SetupRenderTarget(MASKS2, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
	}

	{
		deferredCB = new ConstantBuffer(ConstantBufferDesc<DeferredCB>());
	}

	{
		auto& device = State::GetSingleton()->device;

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &linearSampler));
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags = 0;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;

		std::uint32_t numElements = 1;

		sbDesc.StructureByteStride = sizeof(PerGeometry);
		sbDesc.ByteWidth = sizeof(PerGeometry) * numElements;
		perShadow = new Buffer(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		perShadow->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		perShadow->CreateUAV(uavDesc);

		copyShadowCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\ShadowTest\\CopyShadowData.hlsl", {}, "cs_5_0");
	}

	{
		waterCB = new ConstantBuffer(ConstantBufferDesc<WaterCB>());
	}

	{
		D3D11_TEXTURE2D_DESC texDesc;
		auto mainTex = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];
		mainTex.texture->GetDesc(&texDesc);

		texDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1 }
		};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = { .MipSlice = 0 }
		};

		prevDiffuseAmbientTexture = new Texture2D(texDesc);
		prevDiffuseAmbientTexture->CreateSRV(srvDesc);
		prevDiffuseAmbientTexture->CreateUAV(uavDesc);
	}
}

void Deferred::CopyShadowData()
{
	auto& context = State::GetSingleton()->context;

	ID3D11UnorderedAccessView* uavs[1]{ perShadow->uav.get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	ID3D11Buffer* buffers[1];
	context->PSGetConstantBuffers(2, 1, buffers);
	context->CSSetConstantBuffers(0, 1, buffers);

	context->PSGetConstantBuffers(12, 1, buffers);
	context->CSSetConstantBuffers(1, 1, buffers);

	context->PSGetConstantBuffers(0, 1, buffers);
	context->CSSetConstantBuffers(2, 1, buffers);

	context->PSGetShaderResources(4, 1, &shadowView);

	context->CSSetShader(copyShadowCS, nullptr, 0);

	context->Dispatch(1, 1, 1);

	uavs[0] = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	buffers[0] = nullptr;
	context->CSSetConstantBuffers(0, 1, buffers);
	context->CSSetConstantBuffers(1, 1, buffers);
	context->CSSetConstantBuffers(2, 1, buffers);

	context->CSSetShader(nullptr, nullptr, 0);

	{
		ID3D11ShaderResourceView* srvs[2]{
			shadowView,
			perShadow->srv.get(),
		};

		context->PSSetShaderResources(25, ARRAYSIZE(srvs), srvs);
	}
}

void Deferred::UpdateConstantBuffer()
{
	DeferredCB data{};

	auto state = State::GetSingleton();

	data.BufferDim.x = state->screenSize.x;
	data.BufferDim.y = state->screenSize.y;
	data.BufferDim.z = 1.0f / data.BufferDim.x;
	data.BufferDim.w = 1.0f / data.BufferDim.y;

	data.CameraData = Util::GetCameraData();

	auto& shaderManager = RE::BSShaderManager::State::GetSingleton();
	RE::NiTransform& dalcTransform = shaderManager.directionalAmbientTransform;
	Util::StoreTransform3x4NoScale(data.DirectionalAmbient, dalcTransform);

	auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();

	auto useTAA = !REL::Module::IsVR() ? imageSpaceManager->GetRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled : imageSpaceManager->GetVRRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled;
	data.FrameCount = useTAA ? RE::BSGraphics::State::GetSingleton()->frameCount : 0;

	deferredCB->Update(data);
}

void Deferred::PrepassPasses()
{
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	for (auto* feature : Feature::GetFeatureList()) {
		if (feature->loaded) {
			feature->Prepass();
		}
	}
}

void Deferred::StartDeferred()
{
	if (!inWorld)
		return;

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	State::GetSingleton()->UpdateSharedData();

	static bool setup = false;
	if (!setup) {
		auto& device = State::GetSingleton()->device;

		static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

		{
			forwardBlendStates[0] = blendStates->a[0][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[0]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[0]));
		}

		{
			forwardBlendStates[1] = blendStates->a[0][0][10][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[1]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[1]));
		}

		{
			forwardBlendStates[2] = blendStates->a[1][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[2]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[2]));
		}

		{
			forwardBlendStates[3] = blendStates->a[1][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[3]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[3]));
		}

		{
			forwardBlendStates[4] = blendStates->a[2][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[4]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[4]));
		}

		{
			forwardBlendStates[5] = blendStates->a[2][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[5]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[5]));
		}

		{
			forwardBlendStates[6] = blendStates->a[3][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[6]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[6]));
		}
		setup = true;
	}

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(setRenderTargetMode, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// Backup original render targets
	for (uint i = 0; i < 4; i++) {
		forwardRenderTargets[i] = renderTargets[i];
	}

	RE::RENDER_TARGET targets[8]{
		RE::RENDER_TARGET::kMAIN,
		RE::RENDER_TARGET::kMOTION_VECTOR,
		NORMALROUGHNESS,
		ALBEDO,
		SPECULAR,
		REFLECTANCE,
		MASKS,
		MASKS2
	};

	for (uint i = 2; i < 8; i++) {
		renderTargets[i] = targets[i];                                             // We must use unused targets to be indexable
		setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR;  // Dirty from last frame, this calls ClearRenderTargetView once
	}

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	deferredPass = true;

	PrepassPasses();
}

void Deferred::DeferredPasses()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& context = State::GetSingleton()->context;

	UpdateConstantBuffer();

	{
		REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };
		ID3D11Buffer* buffers[2] = { deferredCB->CB(), *perFrame.get() };

		context->CSSetConstantBuffers(11, 2, buffers);
	}

	auto specular = renderer->GetRuntimeData().renderTargets[SPECULAR];
	auto albedo = renderer->GetRuntimeData().renderTargets[ALBEDO];
	auto normalRoughness = renderer->GetRuntimeData().renderTargets[NORMALROUGHNESS];
	auto masks = renderer->GetRuntimeData().renderTargets[MASKS];
	auto masks2 = renderer->GetRuntimeData().renderTargets[MASKS2];

	auto main = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[0]];
	auto normals = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[2]];
	auto snow = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[3]];

	auto depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto reflectance = renderer->GetRuntimeData().renderTargets[REFLECTANCE];

	bool interior = true;
	if (auto sky = RE::Sky::GetSingleton())
		interior = sky->mode.get() != RE::Sky::Mode::kFull;

	auto skylighting = Skylighting::GetSingleton();

	if (skylighting->loaded)
		skylighting->Compute();

	auto ssgi = ScreenSpaceGI::GetSingleton();

	if (ssgi->loaded)
		ssgi->DrawSSGI(prevDiffuseAmbientTexture);

	auto dispatchCount = Util::GetScreenDispatchCount();

	// Ambient Composite
	{
		ID3D11ShaderResourceView* srvs[4]{
			albedo.SRV,
			normalRoughness.SRV,
			skylighting->loaded ? skylighting->skylightingTexture->srv.get() : nullptr,
			ssgi->loaded ? ssgi->texGI[ssgi->outputGIIdx]->srv.get() : nullptr,
		};

		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

		ID3D11UnorderedAccessView* uavs[2]{ main.UAV, prevDiffuseAmbientTexture->uav.get() };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		auto shader = interior ? GetComputeAmbientCompositeInterior() : GetComputeAmbientComposite();
		context->CSSetShader(shader, nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	auto sss = SubsurfaceScattering::GetSingleton();
	if (sss->loaded)
		sss->DrawSSS();

	auto dynamicCubemaps = DynamicCubemaps::GetSingleton();
	if (dynamicCubemaps->loaded)
		dynamicCubemaps->UpdateCubemap();

	// Deferred Composite
	{
		ID3D11ShaderResourceView* srvs[10]{
			specular.SRV,
			albedo.SRV,
			normalRoughness.SRV,
			masks.SRV,
			masks2.SRV,
			dynamicCubemaps->loaded ? depth.depthSRV : nullptr,
			dynamicCubemaps->loaded ? reflectance.SRV : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envReflectionsTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded && skylighting->loaded ? skylighting->skylightingTexture->srv.get() : nullptr
		};

		if (dynamicCubemaps->loaded)
			context->CSSetSamplers(0, 1, &linearSampler);

		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

		ID3D11UnorderedAccessView* uavs[3]{ main.UAV, normals.UAV, snow.UAV };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		auto shader = interior ? GetComputeMainCompositeInterior() : GetComputeMainComposite();
		context->CSSetShader(shader, nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// Clear
	{
		ID3D11ShaderResourceView* views[10]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[3]{ nullptr, nullptr, nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11Buffer* buffer = nullptr;
		context->CSSetConstantBuffers(0, 1, &buffer);

		context->CSSetShader(nullptr, nullptr, 0);
	}

	if (dynamicCubemaps->loaded)
		dynamicCubemaps->PostDeferred();
}

void Deferred::EndDeferred()
{
	if (!inWorld)
		return;

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// Do not render to our targets past this point
	for (uint i = 0; i < 4; i++) {
		renderTargets[i] = forwardRenderTargets[i];
	}

	for (uint i = 4; i < 8; i++) {
		renderTargets[i] = RE::RENDER_TARGET::kNONE;
	}

	auto& context = State::GetSingleton()->context;
	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	DeferredPasses();  // Perform deferred passes and composite forward buffers

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	deferredPass = false;

	{
		ID3D11Buffer* buffer = waterCB->CB();
		context->PSSetConstantBuffers(7, 1, &buffer);
	}
}

void Deferred::OverrideBlendStates()
{
	static bool setup = false;
	if (!setup) {
		auto& device = State::GetSingleton()->device;

		static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

		{
			forwardBlendStates[0] = blendStates->a[0][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[0]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[0]));
		}

		{
			forwardBlendStates[1] = blendStates->a[0][0][10][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[1]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[1]));
		}

		{
			forwardBlendStates[2] = blendStates->a[1][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[2]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[2]));
		}

		{
			forwardBlendStates[3] = blendStates->a[1][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[3]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[3]));
		}

		{
			forwardBlendStates[4] = blendStates->a[2][0][1][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[4]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[4]));
		}

		{
			forwardBlendStates[5] = blendStates->a[2][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[5]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[5]));
		}

		{
			forwardBlendStates[6] = blendStates->a[3][0][11][0];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[6]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[6]));
		}
		setup = true;
	}

	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	// Set modified blend states
	blendStates->a[0][0][1][0] = deferredBlendStates[0];
	blendStates->a[0][0][10][0] = deferredBlendStates[1];
	blendStates->a[1][0][1][0] = deferredBlendStates[2];
	blendStates->a[1][0][11][0] = deferredBlendStates[3];
	blendStates->a[2][0][1][0] = deferredBlendStates[4];
	blendStates->a[2][0][11][0] = deferredBlendStates[5];
	blendStates->a[3][0][11][0] = deferredBlendStates[6];

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

void Deferred::ResetBlendStates()
{
	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	// Restore modified blend states
	blendStates->a[0][0][1][0] = forwardBlendStates[0];
	blendStates->a[0][0][10][0] = forwardBlendStates[1];
	blendStates->a[1][0][1][0] = forwardBlendStates[2];
	blendStates->a[1][0][11][0] = forwardBlendStates[3];
	blendStates->a[2][0][1][0] = forwardBlendStates[4];
	blendStates->a[2][0][11][0] = forwardBlendStates[5];
	blendStates->a[3][0][11][0] = forwardBlendStates[6];

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

void Deferred::ClearShaderCache()
{
	if (ambientCompositeCS) {
		ambientCompositeCS->Release();
		ambientCompositeCS = nullptr;
	}
	if (ambientCompositeInteriorCS) {
		ambientCompositeInteriorCS->Release();
		ambientCompositeInteriorCS = nullptr;
	}
	if (mainCompositeCS) {
		mainCompositeCS->Release();
		mainCompositeCS = nullptr;
	}
	if (mainCompositeInteriorCS) {
		mainCompositeInteriorCS->Release();
		mainCompositeInteriorCS = nullptr;
	}
}

ID3D11ComputeShader* Deferred::GetComputeAmbientComposite()
{
	if (!ambientCompositeCS) {
		logger::debug("Compiling AmbientCompositeCS");

		std::vector<std::pair<const char*, const char*>> defines;

		auto skylighting = Skylighting::GetSingleton();
		if (skylighting->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		auto ssgi = ScreenSpaceGI::GetSingleton();
		if (ssgi->loaded)
			defines.push_back({ "SSGI", nullptr });

		ambientCompositeCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0");
	}
	return ambientCompositeCS;
}

ID3D11ComputeShader* Deferred::GetComputeAmbientCompositeInterior()
{
	if (!ambientCompositeInteriorCS) {
		logger::debug("Compiling AmbientCompositeCS INTERIOR");

		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		auto ssgi = ScreenSpaceGI::GetSingleton();
		if (ssgi->loaded)
			defines.push_back({ "SSGI", nullptr });

		ambientCompositeInteriorCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0");
	}
	return ambientCompositeInteriorCS;
}

ID3D11ComputeShader* Deferred::GetComputeMainComposite()
{
	if (!mainCompositeCS) {
		logger::debug("Compiling DeferredCompositeCS");

		std::vector<std::pair<const char*, const char*>> defines;

		auto dynamicCubemaps = DynamicCubemaps::GetSingleton();
		if (dynamicCubemaps->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		auto skylighting = Skylighting::GetSingleton();
		if (skylighting->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		mainCompositeCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0");
	}
	return mainCompositeCS;
}

ID3D11ComputeShader* Deferred::GetComputeMainCompositeInterior()
{
	if (!mainCompositeInteriorCS) {
		logger::debug("Compiling DeferredCompositeCS INTERIOR");

		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		auto dynamicCubemaps = DynamicCubemaps::GetSingleton();
		if (dynamicCubemaps->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		mainCompositeInteriorCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0");
	}
	return mainCompositeInteriorCS;
}

void Deferred::UpdateWaterMaterial(RE::BSWaterShaderMaterial* a_material)
{
	WaterCB updateData{};
	updateData.ShallowColor = { a_material->shallowWaterColor.red, a_material->shallowWaterColor.green, a_material->shallowWaterColor.blue };
	updateData.DeepColor = { a_material->deepWaterColor.red, a_material->deepWaterColor.green, a_material->deepWaterColor.blue };
	waterCB->Update(updateData);
}