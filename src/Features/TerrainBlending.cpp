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
		// Create a rasterizer state description
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
		auto& context = State::GetSingleton()->context;
		DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\TerrainBlending\\SpatiotemporalBlueNoise\\stbn_vec1_2Dx1D_128x128x64.dds", nullptr, stbn_vec1_2Dx1D_128x128x64.put());
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
}

void TerrainBlending::OverrideTerrainDepth()
{
	auto context = VariableCache::GetSingleton()->context;

	auto dsv = terrainDepth.views[0];
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0u);
}

void TerrainBlending::ResetDepth()
{
	auto context = VariableCache::GetSingleton()->context;

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

void TerrainBlending::BlendPrepassDepths()
{
	auto context = VariableCache::GetSingleton()->context;

	auto renderer = VariableCache::GetSingleton()->renderer;
	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
	auto& prepassDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];

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

	// Set the shader resources
	ID3D11ShaderResourceView* srvs[3] = { tempDepthTexture->srv.get(), terrainDepth.depthSRV, stbn_vec1_2Dx1D_128x128x64.get() };
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

	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	context->CopyResource(prepassDepth.texture, mainDepth.texture);
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

	if (shaderCache->IsEnabled()) {
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
		}
	}
	func(a_pass, a_technique, a_alphaTest, a_renderFlags);
}
