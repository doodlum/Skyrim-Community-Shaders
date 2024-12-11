#include "Skylighting.h"

#include <ShaderCache.h>
#include <Deferred.h>
#include <DDSTextureLoader.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	Skylighting::Settings,
	MaxZenith,
	MinDiffuseVisibility,
	MinSpecularVisibility)

void Skylighting::LoadSettings(json& o_json)
{
	settings = o_json;
}

void Skylighting::SaveSettings(json& o_json)
{
	o_json = settings;
}

void Skylighting::RestoreDefaultSettings()
{
	settings = {};
}

void Skylighting::DrawSettings()
{
	if (auto _tt = Util::HoverTooltipWrapper())
		ImGui::Text(
			"Extra darkening depending on surface orientation.\n"
			"More physically correct, but may impact the intended visual of certain weathers.");

	ImGui::SliderFloat("Diffuse Min Visibility", &settings.MinDiffuseVisibility, 0.f, 1.f, "%.2f");
	ImGui::SliderFloat("Specular Min Visibility", &settings.MinSpecularVisibility, 0.f, 1.f, "%.2f");

	ImGui::Separator();

	if (ImGui::Button("Rebuild Skylighting")) {
		auto& context = State::GetSingleton()->context;
		UINT clr[1] = { 0 };
		context->ClearUnorderedAccessViewUint(texAccumFramesArray->uav.get(), clr);
		forceFrames = 255 * 4;
	}
	if (auto _tt = Util::HoverTooltipWrapper())
		ImGui::Text("Changes below require rebuilding, a loading screen, or moving away from the current location to apply.");

	ImGui::SliderAngle("Max Zenith Angle", &settings.MaxZenith, 0, 90);
	if (auto _tt = Util::HoverTooltipWrapper())
		ImGui::Text("Smaller angles creates more focused top-down shadow.");
}

void Skylighting::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	{
		auto& precipitationOcclusion = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPRECIPITATION_OCCLUSION_MAP];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

		precipitationOcclusion.texture->GetDesc(&texDesc);
		precipitationOcclusion.depthSRV->GetDesc(&srvDesc);
		precipitationOcclusion.views[0]->GetDesc(&dsvDesc);

		texOcclusion = new Texture2D(texDesc);
		texOcclusion->CreateSRV(srvDesc);
		texOcclusion->CreateDSV(dsvDesc);
	}

	{
		D3D11_TEXTURE3D_DESC texDesc{
			.Width = probeArrayDims[0],
			.Height = probeArrayDims[1],
			.Depth = probeArrayDims[2],
			.MipLevels = 1,
			.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
			.CPUAccessFlags = 0,
			.MiscFlags = 0
		};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D,
			.Texture3D = {
				.MostDetailedMip = 0,
				.MipLevels = texDesc.MipLevels }
		};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D,
			.Texture3D = {
				.MipSlice = 0,
				.FirstWSlice = 0,
				.WSize = texDesc.Depth }
		};

		texProbeArray = new Texture3D(texDesc);
		texProbeArray->CreateSRV(srvDesc);
		texProbeArray->CreateUAV(uavDesc);

		texDesc.Format = srvDesc.Format = uavDesc.Format = DXGI_FORMAT_R8_UINT;

		texAccumFramesArray = new Texture3D(texDesc);
		texAccumFramesArray->CreateSRV(srvDesc);
		texAccumFramesArray->CreateUAV(uavDesc);
	}

	{
		D3D11_SAMPLER_DESC samplerDesc = {
			.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
			.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
			.MaxAnisotropy = 1,
			.MinLOD = 0,
			.MaxLOD = D3D11_FLOAT32_MAX
		};
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, pointClampSampler.put()));
	}

	{
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.UAV->GetDesc(&uavDesc);

		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		srvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		texSkylighting = new Texture2D(texDesc);
		texSkylighting->CreateSRV(srvDesc);
		texSkylighting->CreateUAV(uavDesc);

		texSkylightingTemp = new Texture2D(texDesc);
		texSkylightingTemp->CreateSRV(srvDesc);
		texSkylightingTemp->CreateUAV(uavDesc);
	}

	{
		auto& context = State::GetSingleton()->context;

		DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\Skylighting\\bluenoise.dds", nullptr, &noiseView);
	}

	CompileComputeShaders();
}

void Skylighting::ClearShaderCache()
{
	static const std::vector<winrt::com_ptr<ID3D11ComputeShader>*> shaderPtrs = {
		&probeUpdateCompute,
		&skylightingCompute,
		&skylightingBlurCompute,
		&skylightingBlurComputeFlip
	};

	for (auto shader : shaderPtrs)
		shader = nullptr;

	CompileComputeShaders();
}

void Skylighting::CompileComputeShaders()
{
	struct ShaderCompileInfo
	{
		winrt::com_ptr<ID3D11ComputeShader>* programPtr;
		std::string_view filename;
		std::vector<std::pair<const char*, const char*>> defines;
	};

	std::vector<ShaderCompileInfo>
		shaderInfos = {
			{ &probeUpdateCompute, "UpdateProbesCS.hlsl", {} },
			{ &skylightingCompute, "SkylightingCS.hlsl", {} },
			{ &skylightingBlurCompute, "SkylightingBlurCS.hlsl", {} },
			{ &skylightingBlurComputeFlip, "SkylightingBlurCS.hlsl", { { "FLIP", "" } } },
		};

	for (auto& info : shaderInfos) {
		auto path = std::filesystem::path("Data\\Shaders\\Skylighting") / info.filename;
		if (auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(path.c_str(), info.defines, "cs_5_0")))
			info.programPtr->attach(rawPtr);
	}
}

Skylighting::SkylightingCB Skylighting::GetCommonBufferData()
{
	static float3 prevCellID = { 0, 0, 0 };

	auto eyePosNI = Util::GetEyePosition(0);
	auto eyePos = float3{ eyePosNI.x, eyePosNI.y, eyePosNI.z };

	float3 cellSize = {
		occlusionDistance / probeArrayDims[0],
		occlusionDistance / probeArrayDims[1],
		occlusionDistance * .5f / probeArrayDims[2]
	};
	auto cellID = eyePos / cellSize;
	cellID = { round(cellID.x), round(cellID.y), round(cellID.z) };
	auto cellOrigin = cellID * cellSize;
	float3 cellIDDiff = prevCellID - cellID;
	prevCellID = cellID;

	return {
		.OcclusionViewProj = OcclusionTransform,
		.OcclusionDir = OcclusionDir,
		.PosOffset = cellOrigin - eyePos,
		.ArrayOrigin = {
			((int)cellID.x - probeArrayDims[0] / 2) % probeArrayDims[0],
			((int)cellID.y - probeArrayDims[1] / 2) % probeArrayDims[1],
			((int)cellID.z - probeArrayDims[2] / 2) % probeArrayDims[2] },
		.ValidMargin = { (int)cellIDDiff.x, (int)cellIDDiff.y, (int)cellIDDiff.z },
		.MinDiffuseVisibility = settings.MinDiffuseVisibility,
		.MinSpecularVisibility = settings.MinSpecularVisibility
	};
}

void Skylighting::Render()
{
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Skylighting - Update Probes");

	auto& context = State::GetSingleton()->context;

	std::array<ID3D11ShaderResourceView*, 3> srvs = { texOcclusion->srv.get(), nullptr, nullptr };
	std::array<ID3D11UnorderedAccessView*, 2> uavs = { texProbeArray->uav.get(), texAccumFramesArray->uav.get() };
	std::array<ID3D11SamplerState*, 1> samplers = { pointClampSampler.get() };

	// update probe array
	{
		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(probeUpdateCompute.get(), nullptr, 0);
		context->Dispatch((probeArrayDims[0] + 7u) >> 3, (probeArrayDims[1] + 7u) >> 3, probeArrayDims[2]);
	}

	// reset
	{
		srvs.fill(nullptr);
		uavs.fill(nullptr);
		samplers.fill(nullptr);

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
	}

	auto dispatchCount = Util::GetScreenDispatchCount();

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto normalRoughness = renderer->GetRuntimeData().renderTargets[NORMALROUGHNESS];

	{
		srvs[0] = depth.depthSRV;
		srvs[1] = texProbeArray->srv.get();
		srvs[2] = normalRoughness.SRV;
		uavs[0] = texSkylighting->uav.get();
		samplers[0] = Deferred::GetSingleton()->linearSampler;

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(skylightingCompute.get(), nullptr, 0);
		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// reset
	{
		srvs.fill(nullptr);
		uavs.fill(nullptr);
		samplers.fill(nullptr);

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
	}

	{
		srvs[0] = depth.depthSRV;
		srvs[1] = texSkylighting->srv.get();
		srvs[2] = noiseView;
		uavs[0] = texSkylightingTemp->uav.get();
		samplers[0] = Deferred::GetSingleton()->linearSampler;

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(skylightingBlurCompute.get(), nullptr, 0);
		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// reset
	{
		srvs.fill(nullptr);
		uavs.fill(nullptr);
		samplers.fill(nullptr);

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
	}

	{
		srvs[0] = depth.depthSRV;
		srvs[1] = texSkylightingTemp->srv.get();
		srvs[2] = noiseView;
		uavs[0] = texSkylighting->uav.get();
		samplers[0] = Deferred::GetSingleton()->linearSampler;

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(skylightingBlurComputeFlip.get(), nullptr, 0);
		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// reset
	{
		srvs.fill(nullptr);
		uavs.fill(nullptr);
		samplers.fill(nullptr);

		context->CSSetSamplers(0, (uint)samplers.size(), samplers.data());
		context->CSSetShaderResources(0, (uint)srvs.size(), srvs.data());
		context->CSSetUnorderedAccessViews(0, (uint)uavs.size(), uavs.data(), nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
	}

	// set PS shader resources
	{
		srvs[0] = texProbeArray->srv.get();
		srvs[1] = texSkylighting->srv.get();
		context->PSSetShaderResources(50, (uint)srvs.size(), srvs.data());
	}
}

void Skylighting::PostPostLoad()
{
	logger::info("[SKYLIGHTING] Hooking BSLightingShaderProperty::GetPrecipitationOcclusionMapRenderPassesImp");
	stl::write_vfunc<0x2D, BSLightingShaderProperty_GetPrecipitationOcclusionMapRenderPassesImpl>(RE::VTABLE_BSLightingShaderProperty[0]);
	stl::write_thunk_call<Main_Precipitation_RenderOcclusion>(REL::RelocationID(35560, 36559).address() + REL::Relocate(0x3A1, 0x3A1, 0x2FA));
	stl::write_thunk_call<SetViewFrustum>(REL::RelocationID(25643, 26185).address() + REL::Relocate(0x5D9, 0x59D, 0x5DC));
	MenuOpenCloseEventHandler::Register();
}

//////////////////////////////////////////////////////////////

struct BSParticleShaderRainEmitter
{
	void* vftable_BSParticleShaderRainEmitter_0;
	char _pad_8[4056];
};

enum class ShaderTechnique
{
	// Sky
	SkySunOcclude = 0x2,

	// Grass
	GrassNoAlphaDirOnlyFlatLit = 0x3,
	GrassNoAlphaDirOnlyFlatLitSlope = 0x5,
	GrassNoAlphaDirOnlyVertLitSlope = 0x6,
	GrassNoAlphaDirOnlyFlatLitBillboard = 0x13,
	GrassNoAlphaDirOnlyFlatLitSlopeBillboard = 0x14,

	// Utility
	UtilityGeneralStart = 0x2B,

	// Effect
	EffectGeneralStart = 0x4000002C,

	// Lighting
	LightingGeneralStart = 0x4800002D,

	// DistantTree
	DistantTreeDistantTreeBlock = 0x5C00002E,
	DistantTreeDepth = 0x5C00002F,

	// Grass
	GrassDirOnlyFlatLit = 0x5C000030,
	GrassDirOnlyFlatLitSlope = 0x5C000032,
	GrassDirOnlyVertLitSlope = 0x5C000033,
	GrassDirOnlyFlatLitBillboard = 0x5C000040,
	GrassDirOnlyFlatLitSlopeBillboard = 0x5C000041,
	GrassRenderDepth = 0x5C00005C,

	// Sky
	SkySky = 0x5C00005E,
	SkyMoonAndStarsMask = 0x5C00005F,
	SkyStars = 0x5C000060,
	SkyTexture = 0x5C000061,
	SkyClouds = 0x5C000062,
	SkyCloudsLerp = 0x5C000063,
	SkyCloudsFade = 0x5C000064,

	// Particle
	ParticleParticles = 0x5C000065,
	ParticleParticlesGryColorAlpha = 0x5C000066,
	ParticleParticlesGryColor = 0x5C000067,
	ParticleParticlesGryAlpha = 0x5C000068,
	ParticleEnvCubeSnow = 0x5C000069,
	ParticleEnvCubeRain = 0x5C00006A,

	// Water
	WaterSimple = 0x5C00006B,
	WaterSimpleVc = 0x5C00006C,
	WaterStencil = 0x5C00006D,
	WaterStencilVc = 0x5C00006E,
	WaterDisplacementStencil = 0x5C00006F,
	WaterDisplacementStencilVc = 0x5C000070,
	WaterGeneralStart = 0x5C000071,

	// Sky
	SkySunGlare = 0x5C006072,

	// BloodSplater
	BloodSplaterFlare = 0x5C006073,
	BloodSplaterSplatter = 0x5C006074,
};

//////////////////////////////////////////////////////////////

RE::BSLightingShaderProperty::Data* Skylighting::BSLightingShaderProperty_GetPrecipitationOcclusionMapRenderPassesImpl::thunk(
	RE::BSLightingShaderProperty* property,
	RE::BSGeometry* geometry,
	[[maybe_unused]] uint32_t renderMode,
	[[maybe_unused]] RE::BSGraphics::BSShaderAccumulator* accumulator)
{
	auto batch = accumulator->GetRuntimeData().batchRenderer;
	batch->geometryGroups[14]->flags &= ~1;

	using enum RE::BSShaderProperty::EShaderPropertyFlag;
	using enum RE::BSUtilityShader::Flags;

	auto* precipitationOcclusionMapRenderPassList = &property->unk0C8;

	precipitationOcclusionMapRenderPassList->Clear();
	if (GetSingleton()->inOcclusion) {
		if (property->flags.any(kSkinned) && property->flags.none(kTreeAnim))
			return precipitationOcclusionMapRenderPassList;
	} else {
		if (property->flags.any(kSkinned))
			return precipitationOcclusionMapRenderPassList;
	}

	if (property->flags.any(kZBufferWrite) && property->flags.none(kRefraction, kTempRefraction, kMultiTextureLandscape, kNoLODLandBlend, kLODLandscape, kEyeReflect, kDecal, kDynamicDecal, kAnisotropicLighting) && !(property->flags.any(kSkinned) && property->flags.none(kTreeAnim))) {
		if (geometry->worldBound.radius > 1) {
			stl::enumeration<RE::BSUtilityShader::Flags> technique;
			technique.set(RenderDepth);

			if (property->flags.any(kVertexColors)) {
				technique.set(Vc);
			}

			const auto alphaProperty = static_cast<RE::NiAlphaProperty*>(geometry->GetGeometryRuntimeData().properties[0].get());
			if (alphaProperty && alphaProperty->GetAlphaTesting()) {
				technique.set(Texture);
				technique.set(AlphaTest);
			}

			if (property->flags.any(kLODObjects, kHDLODObjects)) {
				technique.set(LodObject);
			}

			if (property->flags.any(kTreeAnim)) {
				technique.set(TreeAnim);
			}

			precipitationOcclusionMapRenderPassList->EmplacePass(
				RE::BSUtilityShader::GetSingleton(),
				property,
				geometry,
				technique.underlying() + static_cast<uint32_t>(ShaderTechnique::UtilityGeneralStart));
		}
	}
	return precipitationOcclusionMapRenderPassList;
}

void Skylighting::Main_Precipitation_RenderOcclusion::thunk()
{
	State::GetSingleton()->BeginPerfEvent("PRECIPITATION MASK");
	State::GetSingleton()->SetPerfMarker("PRECIPITATION MASK");

	auto& shaderCache = SIE::ShaderCache::Instance();
	auto singleton = GetSingleton();

	if (!singleton->doOcclusion || !shaderCache.IsEnabled()) {
		Main_Precipitation_RenderOcclusion::func();
		State::GetSingleton()->EndPerfEvent();
	}

	if (auto sky = RE::Sky::GetSingleton()) {
		if (sky->mode.get() == RE::Sky::Mode::kFull) {
			static bool doPrecip = false;

			auto precip = sky->precip;

			{
				doPrecip = false;

				auto precipObject = precip->currentPrecip;
				if (!precipObject) {
					precipObject = precip->lastPrecip;
				}
				if (precipObject) {
					precip->SetupMask();
					precip->SetupMask();  // Calling setup twice fixes an issue when it is raining
					auto effect = precipObject->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect];
					auto shaderProp = netimmerse_cast<RE::BSShaderProperty*>(effect.get());
					auto particleShaderProperty = netimmerse_cast<RE::BSParticleShaderProperty*>(shaderProp);
					auto rain = (RE::BSParticleShaderRainEmitter*)(particleShaderProperty->particleEmitter);

					precip->RenderMask(rain);
				}
			}

			{
				std::chrono::time_point<std::chrono::system_clock> currentTimer = std::chrono::system_clock::now();
				auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTimer - singleton->lastUpdateTimer).count();

				if (singleton->forceFrames || timePassed >= (1000.0f / 30.0f)) {
					singleton->forceFrames = (uint)std::max(0, (int)singleton->forceFrames - 1);
					singleton->lastUpdateTimer = currentTimer;
					singleton->frameCount++;

					auto renderer = RE::BSGraphics::Renderer::GetSingleton();
					auto& precipitation = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPRECIPITATION_OCCLUSION_MAP];
					RE::BSGraphics::DepthStencilData precipitationCopy = precipitation;

					precipitation.depthSRV = singleton->texOcclusion->srv.get();
					precipitation.texture = singleton->texOcclusion->resource.get();
					precipitation.views[0] = singleton->texOcclusion->dsv.get();

					static float& PrecipitationShaderCubeSize = (*(float*)REL::RelocationID(515451, 401590).address());
					float originalPrecipitationShaderCubeSize = PrecipitationShaderCubeSize;

					static RE::NiPoint3& PrecipitationShaderDirection = (*(RE::NiPoint3*)REL::RelocationID(515509, 401648).address());
					RE::NiPoint3 originalParticleShaderDirection = PrecipitationShaderDirection;

					singleton->inOcclusion = true;
					PrecipitationShaderCubeSize = singleton->occlusionDistance;

					float originaLastCubeSize = precip->lastCubeSize;
					precip->lastCubeSize = PrecipitationShaderCubeSize;

					float2 vPoint;
					{
						constexpr float rcpRandMax = 1.f / RAND_MAX;
						static int randSeed = std::rand();
						static uint randFrameCount = 0;

						// r2 sequence
						vPoint = float2(randSeed * rcpRandMax) + (float)randFrameCount * float2(0.245122333753f, 0.430159709002f);
						vPoint.x -= static_cast<unsigned long long>(vPoint.x);
						vPoint.y -= static_cast<unsigned long long>(vPoint.y);

						randFrameCount++;
						if (randFrameCount == 1000) {
							randFrameCount = 0;
							randSeed = std::rand();
						}

						// disc transformation
						vPoint.x = sqrt(vPoint.x * sin(singleton->settings.MaxZenith));
						vPoint.y *= 6.28318530718f;

						vPoint = { vPoint.x * cos(vPoint.y), vPoint.x * sin(vPoint.y) };
					}

					float3 PrecipitationShaderDirectionF = -float3{ vPoint.x, vPoint.y, sqrt(1 - vPoint.LengthSquared()) };
					PrecipitationShaderDirectionF.Normalize();

					PrecipitationShaderDirection = { PrecipitationShaderDirectionF.x, PrecipitationShaderDirectionF.y, PrecipitationShaderDirectionF.z };

					precip->SetupMask();
					precip->SetupMask();  // Calling setup twice fixes an issue when it is raining

					BSParticleShaderRainEmitter* rain = new BSParticleShaderRainEmitter;
					{
						TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Skylighting - Render Height Map");
						precip->RenderMask((RE::BSParticleShaderRainEmitter*)rain);
					}
					singleton->inOcclusion = false;

					singleton->OcclusionDir = -float4{ PrecipitationShaderDirectionF.x, PrecipitationShaderDirectionF.y, PrecipitationShaderDirectionF.z, 0 };
					singleton->OcclusionTransform = ((RE::BSParticleShaderRainEmitter*)rain)->occlusionProjection;

					delete rain;

					PrecipitationShaderCubeSize = originalPrecipitationShaderCubeSize;
					precip->lastCubeSize = originaLastCubeSize;

					PrecipitationShaderDirection = originalParticleShaderDirection;

					precipitation = precipitationCopy;
				}
			}
		}
	}
	State::GetSingleton()->EndPerfEvent();
}

void Skylighting::SetViewFrustum::thunk(RE::NiCamera* a_camera, RE::NiFrustum* a_frustum)
{
	if (GetSingleton()->inOcclusion) {
		uint corner = GetSingleton()->frameCount % 4;

		a_frustum->fBottom = (corner == 0 || corner == 1) ? -5000.0f : 0.0f;

		a_frustum->fLeft = (corner == 0 || corner == 2) ? -5000.0f : 0.0f;
		a_frustum->fRight = (corner == 1 || corner == 3) ? 5000.0f : 0.0f;

		a_frustum->fTop = (corner == 2 || corner == 3) ? 5000.0f : 0.0f;
	}

	func(a_camera, a_frustum);
}