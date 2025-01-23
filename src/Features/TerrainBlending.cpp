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

ID3D11VertexShader* TerrainBlending::GetTerrainBlendingVertexShader()
{
	if (!terrainBlendingVertexShader) {
		logger::debug("Compiling DummyVS.hlsl");
		terrainBlendingVertexShader = (ID3D11VertexShader*)Util::CompileShader(L"Data\\Shaders\\Common\\DummyVS.hlsl", {}, "vs_5_0");
	}
	return terrainBlendingVertexShader;
}

ID3D11PixelShader* TerrainBlending::GetTerrainBlendingPixelShader()
{
	if (!terrainBlendingPixelShader) {
		logger::debug("Compiling DepthBlendAdv.hlsl");
		terrainBlendingPixelShader = (ID3D11PixelShader*)Util::CompileShader(L"Data\\Shaders\\TerrainBlending\\DepthBlendAdv.hlsl", {}, "ps_5_0");
	}
	return terrainBlendingPixelShader;
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

		tempDepthTexture = new Texture2D(texDesc);
		tempDepthTexture->CreateSRV(srvDesc);
		tempDepthTexture->CreateDSV(dsvDesc);
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

	{
		// Create a fullscreen quad vertex buffer
		Vertex vertices[] = {
			{ -1.0f, 1.0f, 0.0f },
			{ 1.0f, 1.0f, 0.0f },
			{ -1.0f, -1.0f, 0.0f },
			{ 1.0f, -1.0f, 0.0f },
		};

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = sizeof(vertices);
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vertices;

		DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, &initData, &vertexBuffer));

        D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		auto vertexShader = Util::CompileShaderBlob(L"Data\\Shaders\\Common\\DummyVS.hlsl", {}, "vs_5_0");

		DX::ThrowIfFailed(device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShader->GetBufferPointer(), vertexShader->GetBufferSize(), &inputLayout));
		
		vertexShader->Release();
	}

	// Add new depth stencil state that does not cull but writes depth out
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		DX::ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, &terrainDepthStencilState2));
	}

    {
		auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

        // Create a depth stencil view that can be written to
        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
        depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Flags = 0;

        DX::ThrowIfFailed(device->CreateDepthStencilView(mainDepth.texture, &depthStencilViewDesc, &writeableView));
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
	//auto context = VariableCache::GetSingleton()->context;

	//static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	//// Enable rendering for depth below the surface
	//context->OMSetDepthStencilState(terrainDepthStencilState, 0xFF);

	//// Used to get the distance of the surface to the lowest depth
	//auto view = terrainOffsetTexture->srv.get();
	//context->PSSetShaderResources(55, 1, &view);
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

void TerrainBlending::SetupFullscreenPass()
{
	auto context = VariableCache::GetSingleton()->context;

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

	context->CopyResource(tempDepthTexture->resource.get(), mainDepth.texture);

	// Set the vertex buffer
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	
	// Set the input layout
	context->IASetInputLayout(inputLayout);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	context->VSSetShader(GetTerrainBlendingVertexShader(), nullptr, 0);
	context->PSSetShader(GetTerrainBlendingPixelShader(), nullptr, 0);

	//// Set the render target
	//ID3D11RenderTargetView* renderTargets[1] = { nullptr };
	//context->OMSetRenderTargets(1, renderTargets, writeableView);

	//// Set the viewport
	//D3D11_VIEWPORT viewport;
	//viewport.Width = static_cast<float>(2560);
	//viewport.Height = static_cast<float>(1440);
	//viewport.MinDepth = 0.0f;
	//viewport.MaxDepth = 1.0f;
	//viewport.TopLeftX = 0;
	//viewport.TopLeftY = 0;
	//context->RSSetViewports(1, &viewport);

	//// Set the shader resources
	//ID3D11ShaderResourceView* srvs[2] = { tempDepthTexture->srv.get(), terrainDepthTexture->srv.get() };
	//context->PSSetShaderResources(0, 2, srvs);

	//context->OMSetDepthStencilState(terrainDepthStencilState2, 0);

	//// Draw the fullscreen quad
	context->Draw(4, 0);

	//// Cleanup
	//srvs[0] = nullptr;
	//srvs[1] = nullptr;
	//context->PSSetShaderResources(0, 2, srvs);

	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, state)
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_PRIMITIVE_TOPO);
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_VERTEX_DESC);
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
}

void TerrainBlending::BlendPrepassDepths()
{
	//auto context = VariableCache::GetSingleton()->context;
//	context->OMSetRenderTargets(0, nullptr, nullptr);

	//auto dispatchCount = Util::GetScreenDispatchCount();

	//{
	//	ID3D11ShaderResourceView* views[2] = { depthSRVBackup, terrainDepth.depthSRV };
	//	context->CSSetShaderResources(0, ARRAYSIZE(views), views);

	//	ID3D11UnorderedAccessView* uavs[2] = { blendedDepthTexture->uav.get(), blendedDepthTexture16->uav.get() };
	//	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

	//	context->CSSetShader(GetDepthBlendShader(), nullptr, 0);

	//	context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	//}

	//{
	//	ID3D11ShaderResourceView* views[2] = { depthSRVBackup, terrainDepthTexture->srv.get() };
	//	context->CSSetShaderResources(0, ARRAYSIZE(views), views);

	//	ID3D11UnorderedAccessView* uavs[2] = { terrainOffsetTexture->uav.get(), nullptr };
	//	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

	//	context->CSSetShader(GetDepthFixShader(), nullptr, 0);

	//	context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	//}

	//ID3D11ShaderResourceView* views[2] = { nullptr, nullptr };
	//context->CSSetShaderResources(0, ARRAYSIZE(views), views);

	//ID3D11UnorderedAccessView* uavs[2] = { nullptr, nullptr };
	//context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

	//ID3D11ComputeShader* shader = nullptr;
	//context->CSSetShader(shader, nullptr, 0);


	{
		SetupFullscreenPass();

		//auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		//auto& mainDepth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];

	//	context->CopyResource(mainDepth.texture, tempDepthTexture->resource.get());
	}

	//auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	//GET_INSTANCE_MEMBER(stateUpdateFlags, state)
	//stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
}

void TerrainBlending::ResetTerrainWorld()
{
	//auto stateUpdateFlags = VariableCache::GetSingleton()->stateUpdateFlags;
	//stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	//stateUpdateFlags->set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
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
	if (terrainBlendingPixelShader) {
		terrainBlendingPixelShader->Release();
		terrainBlendingPixelShader = nullptr;
	}
	if (terrainBlendingVertexShader) {
		terrainBlendingVertexShader->Release();
		terrainBlendingVertexShader = nullptr;
	}
}
