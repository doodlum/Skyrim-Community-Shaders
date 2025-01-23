#include "TerrainBlending.h"

#include "State.h"
#include "Util.h"

#include "VariableCache.h"

ID3D11VertexShader* TerrainBlending::GetTerrainVertexShader()
{
	if (!terrainVertexShader) {
		logger::debug("Compiling Utility.hlsl");
		terrainVertexShader = (ID3D11VertexShader*)Util::CompileShader(L"Data\\Shaders\\Utility.hlsl", { { "RENDER_DEPTH", "" } }, "vs_5_0");
	}
	return terrainVertexShader;
}

ID3D11VertexShader* TerrainBlending::GetTerrainOffsetVertexShader()
{
	if (!terrainOffsetVertexShader) {
		logger::debug("Compiling Utility.hlsl");
		terrainOffsetVertexShader = (ID3D11VertexShader*)Util::CompileShader(L"Data\\Shaders\\Utility.hlsl", { { "RENDER_DEPTH", "" }, { "OFFSET_DEPTH", "" } }, "vs_5_0");
	}
	return terrainOffsetVertexShader;
}

ID3D11ComputeShader* TerrainBlending::GetDepthBlendShader()
{
	if (!depthBlendShader) {
		logger::debug("Compiling DepthBlend.hlsl");
		depthBlendShader = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\TerrainBlending\\DepthBlend.hlsl", {}, "cs_5_0");
	}
	return depthBlendShader;
}

ID3D11ComputeShader* TerrainBlending::GetDepthFixShader()
{
	if (!depthFixShader) {
		logger::debug("Compiling DepthFix.hlsl");
		depthFixShader = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\TerrainBlending\\DepthFix.hlsl", {}, "cs_5_0");
	}
	return depthFixShader;
}

void TerrainBlending::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	{
		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc;
		mainDepth.texture->GetDesc(&texDesc);
		DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, NULL, &terrainDepth.texture));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		mainDepth.depthSRV->GetDesc(&srvDesc);
		DX::ThrowIfFailed(device->CreateShaderResourceView(terrainDepth.texture, &srvDesc, &terrainDepth.depthSRV));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		mainDepth.views[0]->GetDesc(&dsvDesc);
		DX::ThrowIfFailed(device->CreateDepthStencilView(terrainDepth.texture, &dsvDesc, &terrainDepth.views[0]));
	}

	{
		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

		mainDepth.texture->GetDesc(&texDesc);
		mainDepth.depthSRV->GetDesc(&srvDesc);
		mainDepth.views[0]->GetDesc(&dsvDesc);

		terrainDepthTexture = new Texture2D(texDesc);
		terrainDepthTexture->CreateSRV(srvDesc);
		terrainDepthTexture->CreateDSV(dsvDesc);
	}

	{
		auto main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		main.texture->GetDesc(&texDesc);
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		blendedDepthTexture = new Texture2D(texDesc);
		terrainOffsetTexture = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		main.SRV->GetDesc(&srvDesc);
		srvDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateSRV(srvDesc);
		terrainOffsetTexture->CreateSRV(srvDesc);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		main.RTV->GetDesc(&rtvDesc);
		rtvDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateRTV(rtvDesc);
		terrainOffsetTexture->CreateRTV(rtvDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		main.UAV->GetDesc(&uavDesc);
		uavDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateUAV(uavDesc);
		terrainOffsetTexture->CreateUAV(uavDesc);

		texDesc.Format = DXGI_FORMAT_R16_UNORM;
		srvDesc.Format = texDesc.Format;
		rtvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		blendedDepthTexture16 = new Texture2D(texDesc);
		blendedDepthTexture16->CreateSRV(srvDesc);
		blendedDepthTexture16->CreateRTV(rtvDesc);
		blendedDepthTexture16->CreateUAV(uavDesc);

		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
		depthSRVBackup = mainDepth.depthSRV;

		auto& zPrepassCopy = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
		prepassSRVBackup = zPrepassCopy.depthSRV;
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.StencilEnable = false;
		DX::ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, &terrainDepthStencilState));
	}
}

void TerrainBlending::PostPostLoad()
{
	Hooks::Install();
}

struct DepthStates
{
	ID3D11DepthStencilState* a[6][40];
};

struct BlendStates
{
	ID3D11BlendState* a[7][2][13][2];
};

void TerrainBlending::TerrainShaderHacks()
{
	if (renderTerrainDepth) {
		auto renderer = VariableCache::GetSingleton()->renderer;
		auto context = VariableCache::GetSingleton()->context;
		if (renderAltTerrain) {
			auto dsv = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN].views[0];
			context->OMSetRenderTargets(0, nullptr, dsv);
			context->VSSetShader(GetTerrainOffsetVertexShader(), NULL, NULL);
		} else {
			auto dsv = terrainDepth.views[0];
			context->OMSetRenderTargets(0, nullptr, dsv);
			auto currentVertexShader = *VariableCache::GetSingleton()->currentVertexShader;
			context->VSSetShader((ID3D11VertexShader*)currentVertexShader->shader, NULL, NULL);
		}
		renderAltTerrain = !renderAltTerrain;
	}

	if (renderTerrainWorld)
		OverrideTerrainWorld();
}

void TerrainBlending::OverrideTerrainWorld()
{
	auto context = VariableCache::GetSingleton()->context;

	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	// Enable rendering for depth below the surface
	context->OMSetDepthStencilState(terrainDepthStencilState, 0xFF);

	// Used to get the distance of the surface to the lowest depth
	auto view = terrainOffsetTexture->srv.get();
	context->PSSetShaderResources(55, 1, &view);
}

void TerrainBlending::OverrideTerrainDepth()
{
	auto context = VariableCache::GetSingleton()->context;

	auto rtv = blendedDepthTexture->rtv.get();
	FLOAT clearColor[4]{ 1, 0, 0, 0 };
	context->ClearRenderTargetView(rtv, clearColor);

	auto dsv = terrainDepth.views[0];
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0u);
}

void TerrainBlending::ResetDepth()
{
	auto context = VariableCache::GetSingleton()->context;

	auto rtv = blendedDepthTexture->rtv.get();
	FLOAT clearColor[4]{ 1, 0, 0, 0 };
	context->ClearRenderTargetView(rtv, clearColor);

	auto dsv = terrainDepth.views[0];
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0u);
}

void TerrainBlending::ResetTerrainDepth()
{
	auto renderer = VariableCache::GetSingleton()->renderer;
	auto context = VariableCache::GetSingleton()->context;

	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

	auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

	auto currentVertexShader = *VariableCache::GetSingleton()->currentVertexShader;

	context->VSSetShader((ID3D11VertexShader*)currentVertexShader->shader, NULL, NULL);

	context->CopyResource(terrainDepthTexture->resource.get(), mainDepth.texture);
}

void TerrainBlending::BlendPrepassDepths()
{
	auto context = VariableCache::GetSingleton()->context;
	context->OMSetRenderTargets(0, nullptr, nullptr);

	auto dispatchCount = Util::GetScreenDispatchCount();

	{
		ID3D11ShaderResourceView* views[2] = { depthSRVBackup, terrainDepth.depthSRV };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[2] = { blendedDepthTexture->uav.get(), blendedDepthTexture16->uav.get() };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		context->CSSetShader(GetDepthBlendShader(), nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	{
		ID3D11ShaderResourceView* views[2] = { depthSRVBackup, terrainDepthTexture->srv.get() };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[2] = { terrainOffsetTexture->uav.get(), nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		context->CSSetShader(GetDepthFixShader(), nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	ID3D11ShaderResourceView* views[2] = { nullptr, nullptr };
	context->CSSetShaderResources(0, ARRAYSIZE(views), views);

	ID3D11UnorderedAccessView* uavs[2] = { nullptr, nullptr };
	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

	ID3D11ComputeShader* shader = nullptr;
	context->CSSetShader(shader, nullptr, 0);

	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, state)
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
}

void TerrainBlending::ResetTerrainWorld()
{
	auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
}

void TerrainBlending::ClearShaderCache()
{
	if (terrainVertexShader) {
		terrainVertexShader->Release();
		terrainVertexShader = nullptr;
	}
	if (terrainOffsetVertexShader) {
		terrainOffsetVertexShader->Release();
		terrainOffsetVertexShader = nullptr;
	}
	if (depthBlendShader) {
		depthBlendShader->Release();
		depthBlendShader = nullptr;
	}
	if (depthFixShader) {
		depthFixShader->Release();
		depthFixShader = nullptr;
	}
}

void TerrainBlending::Hooks::Main_RenderDepth::thunk(bool a1, bool a2)
{
	auto renderer = VariableCache::GetSingleton()->renderer;
	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
	auto& zPrepassCopy = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

	auto singleton = VariableCache::GetSingleton()->terrainBlending;
	auto shaderCache = VariableCache::GetSingleton()->shaderCache;

	if (shaderCache->IsEnabled()) {
		mainDepth.depthSRV = singleton->blendedDepthTexture->srv.get();
		zPrepassCopy.depthSRV = singleton->blendedDepthTexture->srv.get();

		singleton->renderDepth = true;
		singleton->OverrideTerrainDepth();

		func(a1, a2);

		singleton->renderDepth = false;

		if (singleton->renderTerrainDepth) {
			singleton->renderTerrainDepth = false;
			singleton->ResetTerrainDepth();
		}

		singleton->BlendPrepassDepths();
	} else {
		mainDepth.depthSRV = singleton->depthSRVBackup;
		zPrepassCopy.depthSRV = singleton->prepassSRVBackup;
		func(a1, a2);
	}
}

void TerrainBlending::Hooks::BSBatchRenderer__RenderPassImmediately::thunk(RE::BSRenderPass* a_pass, uint32_t a_technique, bool a_alphaTest, uint32_t a_renderFlags)
{
	auto singleton = VariableCache::GetSingleton()->terrainBlending;
	auto shaderCache = VariableCache::GetSingleton()->shaderCache;

	if (shaderCache->IsEnabled()) {
		if (singleton->renderDepth) {
			// Entering or exiting terrain depth section
			bool inTerrain = a_pass->shaderProperty && a_pass->shaderProperty->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape);
			if (singleton->renderTerrainDepth != inTerrain) {
				if (!inTerrain)
					singleton->ResetTerrainDepth();
				singleton->renderTerrainDepth = inTerrain;
			}

			if (inTerrain)
				func(a_pass, a_technique, a_alphaTest, a_renderFlags);  // Run terrain twice
		} else if (singleton->renderWorld) {
			// Entering or exiting terrain section
			bool inTerrain = a_pass->shaderProperty && a_pass->shaderProperty->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape);
			if (singleton->renderTerrainWorld != inTerrain) {
				if (!inTerrain)
					singleton->ResetTerrainWorld();
				singleton->renderTerrainWorld = inTerrain;
			}
		}
	}
}

void TerrainBlending::Hooks::Main_RenderWorld_RenderBatches::thunk(RE::BSBatchRenderer* This, uint32_t StartRange, uint32_t EndRange, uint32_t RenderFlags, int GeometryGroup)
{
	auto singleton = VariableCache::GetSingleton()->terrainBlending;
	auto shaderCache = VariableCache::GetSingleton()->shaderCache;
	auto renderer = VariableCache::GetSingleton()->renderer;

	if (shaderCache->IsEnabled()) {
		singleton->renderWorld = true;

		func(This, StartRange, EndRange, RenderFlags, GeometryGroup);

		singleton->renderWorld = false;

		if (singleton->renderTerrainWorld) {
			singleton->renderTerrainWorld = false;
			singleton->ResetTerrainWorld();
		}

		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

		mainDepth.depthSRV = singleton->depthSRVBackup;
	} else {
		func(This, StartRange, EndRange, RenderFlags, GeometryGroup);
	}
}