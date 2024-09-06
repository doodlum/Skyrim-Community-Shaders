#include "CloudShadows.h"

#include "State.h"

#include "Deferred.h"
#include "Util.h"

void CloudShadows::CheckResourcesSide(int side)
{
	static Util::FrameChecker frame_checker[6];
	if (!frame_checker[side].isNewFrame())
		return;

	auto& context = State::GetSingleton()->context;

	float black[4] = { 0, 0, 0, 0 };
	context->ClearRenderTargetView(cubemapCloudOccRTVs[side], black);
}

void CloudShadows::ModifySky(RE::BSRenderPass* Pass)
{
	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

	GET_INSTANCE_MEMBER(cubeMapRenderTarget, shadowState);

	if (cubeMapRenderTarget != RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS)
		return;

	auto skyProperty = static_cast<const RE::BSSkyShaderProperty*>(Pass->shaderProperty);

	if (skyProperty->uiSkyObjectType == RE::BSSkyShaderProperty::SkyObject::SO_CLOUDS) {
		auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto& context = State::GetSingleton()->context;

		auto reflections = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGET_CUBEMAP::kREFLECTIONS];

		// render targets
		ID3D11RenderTargetView* rtvs[4];
		ID3D11DepthStencilView* depthStencil;
		context->OMGetRenderTargets(3, rtvs, &depthStencil);

		int side = -1;
		for (int i = 0; i < 6; ++i)
			if (rtvs[0] == reflections.cubeSideRTV[i]) {
				side = i;
				break;
			}
		if (side == -1)
			return;

		CheckResourcesSide(side);

		rtvs[3] = cubemapCloudOccRTVs[side];
		context->OMSetRenderTargets(4, rtvs, depthStencil);
	}
}

void CloudShadows::Prepass()
{
	if ((RE::Sky::GetSingleton()->mode.get() != RE::Sky::Mode::kFull) ||
		!RE::Sky::GetSingleton()->currentClimate)
		return;

	auto& context = State::GetSingleton()->context;

	ID3D11ShaderResourceView* srv = texCubemapCloudOcc->srv.get();
	context->PSSetShaderResources(27, 1, &srv);
}

void CloudShadows::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	{
		auto reflections = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGET_CUBEMAP::kREFLECTIONS];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};

		reflections.texture->GetDesc(&texDesc);
		reflections.SRV->GetDesc(&srvDesc);

		texDesc.Format = srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		texCubemapCloudOcc = new Texture2D(texDesc);
		texCubemapCloudOcc->CreateSRV(srvDesc);

		for (int i = 0; i < 6; ++i) {
			reflections.cubeSideRTV[i]->GetDesc(&rtvDesc);
			rtvDesc.Format = texDesc.Format;
			DX::ThrowIfFailed(device->CreateRenderTargetView(texCubemapCloudOcc->resource.get(), &rtvDesc, cubemapCloudOccRTVs + i));
		}
	}
}
