#include "TerrainBlending.h"

#include "State.h"
#include "Util.h"

#include "VariableCache.h"
#include <DDSTextureLoader.h>

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

ID3D11VertexShader* TerrainBlending::GetTerrainBlendingVertexShader()
{
	if (!terrainBlendingVertexShader) {
		logger::debug("Compiling DummyVS.hlsl");
		terrainBlendingVertexShader = (ID3D11VertexShader*)Util::CompileShader(L"Data\\Shaders\\TerrainBlending\\FullscreenVS.hlsl", {}, "vs_5_0");
	}
	return terrainBlendingVertexShader;
}

ID3D11PixelShader* TerrainBlending::GetTerrainBlendingPixelShader()
{
	if (!terrainBlendingPixelShader) {
		logger::debug("Compiling DepthBlendAdv.hlsl");
		terrainBlendingPixelShader = (ID3D11PixelShader*)Util::CompileShader(L"Data\\Shaders\\TerrainBlending\\TerrainBlendingPS.hlsl", {}, "ps_5_0");
	}
	return terrainBlendingPixelShader;
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

		tempDepthTexture = new Texture2D(texDesc);
		tempDepthTexture->CreateSRV(srvDesc);
		tempDepthTexture->CreateDSV(dsvDesc);
	}

	{
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.DepthBias = 0;
		rasterDesc.DepthBiasClamp = 0.0f;
		rasterDesc.SlopeScaledDepthBias = 0.0f;
		rasterDesc.DepthClipEnable = false;
		rasterDesc.ScissorEnable = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;

		DX::ThrowIfFailed(device->CreateRasterizerState(&rasterDesc, &rasterState));
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.StencilEnable = false;

		DX::ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState));
	}

	{
		auto& context = State::GetSingleton()->context;
		DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\TerrainBlending\\SpatiotemporalBlueNoise\\stbn_vec1_2Dx1D_128x128x64.dds", nullptr, stbn_vec1_2Dx1D_128x128x64.put());
	}

	{
		auto main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		main.texture->GetDesc(&texDesc);
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		blendedDepthTexture = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		main.SRV->GetDesc(&srvDesc);
		srvDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateSRV(srvDesc);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		main.RTV->GetDesc(&rtvDesc);
		rtvDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateRTV(rtvDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		main.UAV->GetDesc(&uavDesc);
		uavDesc.Format = texDesc.Format;
		blendedDepthTexture->CreateUAV(uavDesc);

		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
		depthSRVBackup = mainDepth.depthSRV;

		auto& zPrepassCopy = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
		prepassSRVBackup = zPrepassCopy.depthSRV;
	}

	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		auto& rtDesc = blendDesc.RenderTarget[0];
		rtDesc.BlendEnable = true;
		rtDesc.SrcBlend = D3D11_BLEND_ONE;
		rtDesc.DestBlend = D3D11_BLEND_ZERO;
		rtDesc.BlendOp = D3D11_BLEND_OP_ADD;
		rtDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
		rtDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
		rtDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rtDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;

		DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &blendState));
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

void TerrainBlending::OverrideTerrainWorld()
{
	auto context = VariableCache::GetSingleton()->context;

	auto shadowState = VariableCache::GetSingleton()->shadowState;

	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	GET_INSTANCE_MEMBER(alphaBlendAlphaToCoverage, shadowState)
	GET_INSTANCE_MEMBER(alphaBlendModeExtra, shadowState)
	GET_INSTANCE_MEMBER(alphaBlendWriteMode, shadowState)

	// Enable alpha blending
	context->OMSetBlendState(blendStates->a[1][alphaBlendAlphaToCoverage][alphaBlendWriteMode][alphaBlendModeExtra], nullptr, 0xFFFFFFFF);

	////// Enable rendering for depth below the surface
	//context->OMSetDepthStencilState(terrainDepthStencilState, 0xFF);

	// Used to get the distance of the surface to the lowest depth
	ID3D11ShaderResourceView* views[2] = { depthSRVBackup, blendedDepthTexture->srv.get() };
	context->PSSetShaderResources(55, 2, views);

	ID3D11RenderTargetView* rtvs[8];
	ID3D11DepthStencilView* dsv;
	context->OMGetRenderTargets(8, rtvs, &dsv);

	dsv = tempDepthTexture->dsv.get();

	context->OMSetRenderTargets(8, rtvs, dsv);
}

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
	} else if (renderTerrainWorld) {
		OverrideTerrainWorld();
	}
}

void TerrainBlending::OverrideTerrainDepth()
{
	auto context = VariableCache::GetSingleton()->context;

	auto dsv = terrainDepth.views[0];
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0u);

	auto rtv = blendedDepthTexture->rtv.get();
	FLOAT clearColor[4]{ 1, 0, 0, 0 };
	context->ClearRenderTargetView(rtv, clearColor);
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
	auto context = VariableCache::GetSingleton()->context;

	auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

	auto currentVertexShader = *VariableCache::GetSingleton()->currentVertexShader;

	context->VSSetShader((ID3D11VertexShader*)currentVertexShader->shader, NULL, NULL);
}

void TerrainBlending::ResetTerrainWorld()
{
	auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
}

void TerrainBlending::BlendPrepassDepths()
{
	auto context = VariableCache::GetSingleton()->context;

	auto renderer = VariableCache::GetSingleton()->renderer;
	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
	//auto& prepassDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

	context->CopyResource(tempDepthTexture->resource.get(), mainDepth.texture);

	// Set vertex data
	ID3D11Buffer* vbs[1] = { NULL };
	uint32_t strides[1] = { 0 };
	uint32_t offsets[1] = { 0 };
	context->IASetVertexBuffers(0, 1, vbs, strides, offsets);
	context->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(NULL);

	// Set the shaders
	context->VSSetShader(GetTerrainBlendingVertexShader(), nullptr, 0);
	context->PSSetShader(GetTerrainBlendingPixelShader(), nullptr, 0);
	context->RSSetState(rasterState);
	context->OMSetDepthStencilState(depthStencilState, 0);
	context->OMSetBlendState(blendState, nullptr, 0xffffffff);

	auto state = VariableCache::GetSingleton()->state;

	// Set the viewport
	D3D11_VIEWPORT viewport;
	viewport.Width = state->screenSize.x;
	viewport.Height = state->screenSize.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	context->RSSetViewports(1, &viewport);

	//	ID3D11ShaderResourceView* srv = nullptr;
	//context->PSSetShaderResources(17, 1, &srv);

	// Set the render targets
	auto rtv = blendedDepthTexture->rtv.get();
	auto dsv = tempDepthTexture->dsv.get();
	context->OMSetRenderTargets(1, &rtv, dsv);

	// Set the shader resources
	ID3D11ShaderResourceView* srvs[3] = { depthSRVBackup, terrainDepth.depthSRV, stbn_vec1_2Dx1D_128x128x64.get() };
	context->PSSetShaderResources(0, 3, srvs);

	// Draw
	context->Draw(3, 0);

	// Cleanup
	srvs[0] = nullptr;
	srvs[1] = nullptr;
	srvs[2] = nullptr;
	context->PSSetShaderResources(0, 3, srvs);

	auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_VERTEX_DESC);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_RASTER_CULL_MODE);
	stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);

	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets
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
	if (terrainBlendingPixelShader) {
		terrainBlendingPixelShader->Release();
		terrainBlendingPixelShader = nullptr;
	}
	if (terrainBlendingVertexShader) {
		terrainBlendingVertexShader->Release();
		terrainBlendingVertexShader = nullptr;
	}
}

void TerrainBlending::Hooks::Main_RenderDepth::thunk(bool a1, bool a2)
{
	auto singleton = VariableCache::GetSingleton()->terrainBlending;
	auto shaderCache = VariableCache::GetSingleton()->shaderCache;
	auto renderer = VariableCache::GetSingleton()->renderer;

	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
	auto& zPrepassCopy = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

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
	func(a_pass, a_technique, a_alphaTest, a_renderFlags);
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
