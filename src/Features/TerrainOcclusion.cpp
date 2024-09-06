#include "TerrainOcclusion.h"
#include "Menu.h"

#include "Deferred.h"
#include "State.h"
#include "Util.h"

#include <filesystem>

#include <DirectXTex.h>
#include <pystring/pystring.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	TerrainOcclusion::Settings,
	AoDistance,
	SliceCount,
	SampleCount,
	EnableTerrainShadow,
	EnableTerrainAO,
	HeightBias,
	ShadowSofteningRadiusAngle,
	AOPower,
	AOFadeOutHeight)

void TerrainOcclusion::LoadSettings(json& o_json)
{
	settings = o_json;
}

void TerrainOcclusion::SaveSettings(json& o_json)
{
	o_json = settings;
}

void TerrainOcclusion::DrawSettings()
{
	ImGui::Checkbox("Enable Terrain Shadow", (bool*)&settings.EnableTerrainShadow);
	// ImGui::Checkbox("Enable Terrain AO", (bool*)&settings.EnableTerrainAO);

	ImGui::SliderFloat("Height Map Bias", &settings.HeightBias, -2000.f, 0.f, "%.0f units");
	if (auto _tt = Util::HoverTooltipWrapper())
		ImGui::Text("Moving the height map down to compensate for its inaccuracy.");

	// ImGui::SeparatorText("Shadow");
	{
		ImGui::SliderAngle("Softening", &settings.ShadowSofteningRadiusAngle, .1f, 10.f, "%.2f deg", ImGuiSliderFlags_AlwaysClamp);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Controls the solid angle of sunlight, making terrain shadows softer.");

		ImGui::SliderFloat2("Fade Distance", &settings.ShadowFadeDistance.x, 0, 10000.f, "%.0f units");
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Shadows around you are and should be handled by vanilla shadow maps.");
			if (auto settingCollection = RE::INIPrefSettingCollection::GetSingleton()) {
				auto gameShadowDist = settingCollection->GetSetting("fShadowDistance:Display")->GetFloat();
				ImGui::Text("Your fShadowDistance setting: %f", gameShadowDist);
			}
		}
	}

	// ImGui::SeparatorText("AO");
	// {
	// 	ImGui::SliderFloat("Mix", &settings.AOMix, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	// 	ImGui::SliderFloat("Power", &settings.AOPower, 0.2f, 5, "%.2f");
	// 	ImGui::SliderFloat("Fadeout Height", &settings.AOFadeOutHeight, 500, 5000, "%.0f units");
	// 	if (auto _tt = Util::HoverTooltipWrapper())
	// 		ImGui::Text("On the ground AO is the most prominent. Up to a certain height it will gradually fade out.");

	// 	if (ImGui::TreeNodeEx("Precomputation", ImGuiTreeNodeFlags_DefaultOpen)) {
	// 		ImGui::SliderFloat("Distance", &settings.AoDistance, 1.f / 32, 32, "%.2f cells");
	// 		ImGui::InputScalar("Slices", ImGuiDataType_U32, &settings.SliceCount);
	// 		ImGui::InputScalar("Samples", ImGuiDataType_U32, &settings.SampleCount);
	// 		if (ImGui::Button("Force Regenerate AO", { -1, 0 }))
	// 			needPrecompute = true;

	// 		ImGui::TreePop();
	// 	}
	// }

	if (ImGui::CollapsingHeader("Debug")) {
		std::string curr_worldspace = "N/A";
		std::string curr_worldspace_name = "N/A";
		auto tes = RE::TES::GetSingleton();
		if (tes) {
			auto worldspace = tes->GetRuntimeData2().worldSpace;
			if (worldspace) {
				curr_worldspace = worldspace->GetFormEditorID();
				curr_worldspace_name = worldspace->GetName();
			}
		}
		ImGui::Text(fmt::format("Current worldspace: {} ({})", curr_worldspace, curr_worldspace_name).c_str());
		ImGui::Text(fmt::format("Has height map: {}", heightmaps.contains(curr_worldspace)).c_str());

		ImGui::Separator();

		if (texOcclusion) {
			ImGui::BulletText("shadowUpdateCBData");
			ImGui::Indent();
			{
				ImGui::Text(fmt::format("LightPxDir: ({}, {})", shadowUpdateCBData.LightPxDir.x, shadowUpdateCBData.LightPxDir.y).c_str());
				ImGui::Text(fmt::format("LightDeltaZ: ({}, {})", shadowUpdateCBData.LightDeltaZ.x, shadowUpdateCBData.LightDeltaZ.y).c_str());
				ImGui::Text(fmt::format("StartPxCoord: {}", shadowUpdateCBData.StartPxCoord).c_str());
				ImGui::Text(fmt::format("PxSize: ({}, {})", shadowUpdateCBData.PxSize.x, shadowUpdateCBData.PxSize.y).c_str());
			}
			ImGui::Unindent();

			if (ImGui::TreeNode("Buffer Viewer")) {
				static float debugRescale = .1f;
				ImGui::SliderFloat("View Resize", &debugRescale, 0.f, 1.f);

				BUFFER_VIEWER_NODE_BULLET(texOcclusion, debugRescale)
				BUFFER_VIEWER_NODE_BULLET(texNormalisedHeight, debugRescale)
				BUFFER_VIEWER_NODE_BULLET(texShadowHeight, debugRescale)
				ImGui::TreePop();
			}
		}
	}
}

void TerrainOcclusion::ClearShaderCache()
{
	if (occlusionProgram) {
		occlusionProgram->Release();
		occlusionProgram = nullptr;
	}
	if (shadowUpdateProgram) {
		shadowUpdateProgram->Release();
		shadowUpdateProgram = nullptr;
	}

	CompileComputeShaders();
}

void TerrainOcclusion::SetupResources()
{
	logger::debug("Listing height maps...");
	{
		std::filesystem::path texture_dir{ L"Data\\textures\\heightmaps\\" };
		for (auto const& dir_entry : std::filesystem::directory_iterator{ texture_dir }) {
			auto filename = dir_entry.path().filename();
			if (filename.extension() != ".dds")
				continue;

			logger::debug("Found dds: {}", filename.string());

			auto splitstr = pystring::split(filename.stem().string(), ".");

			if (splitstr.size() != 10)
				logger::warn("{} has incorrect number ({} instead of 10) of fields", filename.string(), splitstr.size());

			if (splitstr[1] == "HeightMap") {
				HeightMapMetadata metadata;
				try {
					metadata.worldspace = splitstr[0];
					metadata.pos0.x = std::stoi(splitstr[2]) * 4096.f;
					metadata.pos1.y = std::stoi(splitstr[3]) * 4096.f;
					metadata.pos1.x = (std::stoi(splitstr[4]) + 1) * 4096.f;
					metadata.pos0.y = (std::stoi(splitstr[5]) + 1) * 4096.f;
					metadata.pos0.z = std::stoi(splitstr[6]) * 8.f;
					metadata.pos1.z = std::stoi(splitstr[7]) * 8.f;
					metadata.zRange.x = std::stoi(splitstr[8]) * 8.f;
					metadata.zRange.y = std::stoi(splitstr[9]) * 8.f;
				} catch (std::exception& e) {
					logger::warn("Failed to parse {}. Error: {}", filename.string(), e.what());
					continue;
				}

				metadata.dir = dir_entry.path().parent_path().wstring();
				metadata.filename = filename.string();

				if (heightmaps.contains(metadata.worldspace)) {
					logger::warn("{} has more than one height maps!", metadata.worldspace);
				} else {
					heightmaps[metadata.worldspace] = metadata;
				}
			} else if (splitstr[1] != "AO" && splitstr[1] != "Cone")
				logger::warn("{} has unknown type ({})", filename.string(), splitstr[1]);
		}
	}

	logger::debug("Creating structured buffers...");
	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(AOGenBuffer);
		sbDesc.ByteWidth = sizeof(AOGenBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = 1;

		aoGenBuffer = std::make_unique<Buffer>(sbDesc);
		aoGenBuffer->CreateSRV(srvDesc);
	}

	logger::debug("Creating constant buffers...");
	{
		shadowUpdateCB = std::make_unique<ConstantBuffer>(ConstantBufferDesc<ShadowUpdateCB>());
	}

	CompileComputeShaders();
}

void TerrainOcclusion::CompileComputeShaders()
{
	logger::debug("Compiling shaders...");
	{
		auto program_ptr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\TerrainOcclusion\\AOGen.cs.hlsl", {}, "cs_5_0"));
		if (program_ptr)
			occlusionProgram.attach(program_ptr);

		program_ptr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\TerrainOcclusion\\ShadowUpdate.cs.hlsl", {}, "cs_5_0"));
		if (program_ptr)
			shadowUpdateProgram.attach(program_ptr);
	}
}

bool TerrainOcclusion::IsHeightMapReady()
{
	if (auto tes = RE::TES::GetSingleton())
		if (auto worldspace = tes->GetRuntimeData2().worldSpace)
			return cachedHeightmap && cachedHeightmap->worldspace == worldspace->GetFormEditorID();
	return false;
}

TerrainOcclusion::PerFrame TerrainOcclusion::GetCommonBufferData()
{
	bool isHeightmapReady = IsHeightMapReady();

	PerFrame data = {
		.EnableTerrainShadow = settings.EnableTerrainShadow && isHeightmapReady,
		.EnableTerrainAO = settings.EnableTerrainAO && isHeightmapReady,
		.HeightBias = settings.HeightBias,
		.ShadowSofteningRadiusAngle = settings.ShadowSofteningRadiusAngle,
		.ShadowFadeDistance = settings.ShadowFadeDistance,
		.AOMix = settings.AOMix,
		.AOPower = settings.AOPower,
		.AOFadeOutHeightRcp = 1.f / settings.AOFadeOutHeight,
	};

	if (isHeightmapReady) {
		data.InvScale = cachedHeightmap->pos1 - cachedHeightmap->pos0;
		data.Scale = float3(1.f, 1.f, 1.f) / data.InvScale;
		data.Offset = -cachedHeightmap->pos0 * data.Scale;
		data.ZRange = cachedHeightmap->zRange;
	}

	return data;
}

void TerrainOcclusion::LoadHeightmap()
{
	auto tes = RE::TES::GetSingleton();
	if (!tes)
		return;
	auto worldspace = tes->GetRuntimeData2().worldSpace;
	if (!worldspace)
		return;
	std::string worldspace_name = worldspace->GetFormEditorID();
	if (!heightmaps.contains(worldspace_name))  // no height map for that, but we don't remove cache
		return;
	if (cachedHeightmap && cachedHeightmap->worldspace == worldspace_name)  // already cached
		return;

	auto& device = State::GetSingleton()->device;

	logger::debug("Loading height map...");
	{
		auto& target_heightmap = heightmaps[worldspace_name];

		DirectX::ScratchImage image;
		try {
			std::filesystem::path path{ target_heightmap.dir };
			path /= target_heightmap.filename;

			DX::ThrowIfFailed(LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
		} catch (const DX::com_exception& e) {
			logger::error("{}", e.what());
			return;
		}

		ID3D11Resource* pResource = nullptr;
		try {
			DX::ThrowIfFailed(CreateTexture(device,
				image.GetImages(), image.GetImageCount(),
				image.GetMetadata(), &pResource));
		} catch (const DX::com_exception& e) {
			logger::error("{}", e.what());
			return;
		}

		texHeightMap.release();
		texHeightMap = std::make_unique<Texture2D>(reinterpret_cast<ID3D11Texture2D*>(pResource));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texHeightMap->desc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1 }
		};
		texHeightMap->CreateSRV(srvDesc);

		cachedHeightmap = &heightmaps[worldspace_name];
	}

	shadowUpdateIdx = 0;
	needPrecompute = true;
}

void TerrainOcclusion::Precompute()
{
	if (!cachedHeightmap)
		return;

	auto& context = State::GetSingleton()->context;

	logger::info("Creating occlusion texture...");
	{
		texOcclusion.release();
		texNormalisedHeight.release();
		texShadowHeight.release();

		D3D11_TEXTURE2D_DESC texDesc = {
			.Width = texHeightMap->desc.Width,
			.Height = texHeightMap->desc.Height,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT_R8_UNORM,
			.SampleDesc = { .Count = 1 },
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
		};
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

		texOcclusion = std::make_unique<Texture2D>(texDesc);
		texOcclusion->CreateSRV(srvDesc);
		texOcclusion->CreateUAV(uavDesc);

		texDesc.Format = srvDesc.Format = uavDesc.Format = DXGI_FORMAT_R16_FLOAT;
		texNormalisedHeight = std::make_unique<Texture2D>(texDesc);
		texNormalisedHeight->CreateSRV(srvDesc);
		texNormalisedHeight->CreateUAV(uavDesc);

		texDesc.Format = srvDesc.Format = uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		texShadowHeight = std::make_unique<Texture2D>(texDesc);
		texShadowHeight->CreateSRV(srvDesc);
		texShadowHeight->CreateUAV(uavDesc);
	}

	{
		AOGenBuffer data = {
			.AoDistance = settings.AoDistance * 4096.f,
			.SliceCount = settings.SliceCount,
			.SampleCount = settings.SampleCount,
			.pos0 = cachedHeightmap->pos0,
			.pos1 = cachedHeightmap->pos1,
			.zRange = cachedHeightmap->zRange
		};

		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(aoGenBuffer->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(AOGenBuffer);
		memcpy_s(mapped.pData, bytes, &data, bytes);
		context->Unmap(aoGenBuffer->resource.get(), 0);
	}

	/* ---- BACKUP ---- */
	struct ShaderState
	{
		ID3D11ShaderResourceView* srvs[2] = { nullptr };
		ID3D11ComputeShader* shader = nullptr;
		ID3D11UnorderedAccessView* uavs[2] = { nullptr };
		ID3D11ClassInstance* instance = nullptr;
		ID3D11SamplerState* samplers[1] = { nullptr };
		UINT numInstances;
	} old, newer;
	context->CSGetShaderResources(0, ARRAYSIZE(old.srvs), old.srvs);
	context->CSGetShader(&old.shader, &old.instance, &old.numInstances);
	context->CSGetUnorderedAccessViews(0, ARRAYSIZE(old.uavs), old.uavs);
	context->CSGetSamplers(0, ARRAYSIZE(old.samplers), old.samplers);

	/* ---- DISPATCH ---- */
	logger::info("Precomputation...");
	newer.srvs[0] = aoGenBuffer->srv.get();
	newer.srvs[1] = texHeightMap->srv.get();
	newer.uavs[0] = texOcclusion->uav.get();
	newer.uavs[1] = texNormalisedHeight->uav.get();

	context->CSSetSamplers(0, ARRAYSIZE(newer.samplers), newer.samplers);
	context->CSSetShaderResources(0, ARRAYSIZE(newer.srvs), newer.srvs);
	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(newer.uavs), newer.uavs, nullptr);
	context->CSSetShader(occlusionProgram.get(), nullptr, 0);
	context->Dispatch(((texOcclusion->desc.Width - 1) >> 5) + 1, ((texOcclusion->desc.Height - 1) >> 5) + 1, 1);

	/* ---- RESTORE ---- */
	context->CSSetShaderResources(0, ARRAYSIZE(old.srvs), old.srvs);
	context->CSSetShader(old.shader, &old.instance, old.numInstances);
	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(old.uavs), old.uavs, nullptr);
	context->CSSetSamplers(0, ARRAYSIZE(old.samplers), old.samplers);

	needPrecompute = false;
}

void TerrainOcclusion::UpdateShadow()
{
	if (!IsHeightMapReady())
		return;

	// don't forget to change NTHREADS in shader!
	constexpr uint updateLength = 128u;
	constexpr uint logUpdateLength = std::bit_width(128u) - 1;  // integer log2, https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c

	auto& context = State::GetSingleton()->context;
	auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
	auto sunLight = skyrim_cast<RE::NiDirectionalLight*>(accumulator->GetRuntimeData().activeShadowSceneNode->GetRuntimeData().sunLight->light.get());
	if (!sunLight)
		return;

	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Terrain Occlusion - Update Shadows");

	/* ---- UPDATE CB ---- */
	uint width = texNormalisedHeight->desc.Width;
	uint height = texNormalisedHeight->desc.Height;

	// only update direction at the start of each cycle
	static uint edgePxCoord;
	static int signDir;
	static uint maxUpdates;
	if (shadowUpdateIdx == 0) {
		auto direction = sunLight->GetWorldDirection();
		float3 dirLightDir = { direction.x, direction.y, direction.z };
		if (dirLightDir.z > 0)
			dirLightDir = -dirLightDir;

		// in UV
		float3 invScale = cachedHeightmap->pos1 - cachedHeightmap->pos0;
		invScale.z = cachedHeightmap->zRange.y - cachedHeightmap->zRange.x;
		float3 dirLightPxDir = dirLightDir / invScale;
		dirLightPxDir.x *= width;
		dirLightPxDir.y *= height;

		float stepMult;
		if (abs(dirLightPxDir.x) >= abs(dirLightPxDir.y)) {
			stepMult = 1.f / abs(dirLightPxDir.x);
			edgePxCoord = dirLightPxDir.x > 0 ? 0 : (width - 1);
			signDir = dirLightPxDir.x > 0 ? 1 : -1;
			maxUpdates = (width + updateLength - 1) >> logUpdateLength;
		} else {
			stepMult = 1.f / abs(dirLightPxDir.y);
			edgePxCoord = dirLightPxDir.y > 0 ? 0 : height - 1;
			signDir = dirLightPxDir.y > 0 ? 1 : -1;
			maxUpdates = (height + updateLength - 1) >> logUpdateLength;
		}
		dirLightPxDir *= stepMult;

		shadowUpdateCBData.LightPxDir = { dirLightPxDir.x, dirLightPxDir.y };

		// soft shadow angles
		float lenUV = float2{ dirLightDir.x, dirLightDir.y }.Length();
		float dirLightAngle = atan2(-dirLightDir.z, lenUV);
		float upperAngle = std::max(0.f, dirLightAngle - settings.ShadowSofteningRadiusAngle);
		float lowerAngle = std::min(RE::NI_HALF_PI - 1e-2f, dirLightAngle + settings.ShadowSofteningRadiusAngle);

		shadowUpdateCBData.LightDeltaZ = -(lenUV / invScale.z * stepMult) * float2{ std::tan(upperAngle), std::tan(lowerAngle) };
	}

	shadowUpdateCBData.StartPxCoord = edgePxCoord + signDir * shadowUpdateIdx * updateLength;
	shadowUpdateCBData.PxSize = { 1.f / texNormalisedHeight->desc.Width, 1.f / texNormalisedHeight->desc.Height };

	shadowUpdateCB->Update(shadowUpdateCBData);

	shadowUpdateIdx = (shadowUpdateIdx + 1) % maxUpdates;

	/* ---- BACKUP ---- */
	struct ShaderState
	{
		ID3D11ShaderResourceView* srvs[1] = { nullptr };
		ID3D11ComputeShader* shader = nullptr;
		ID3D11UnorderedAccessView* uavs[1] = { nullptr };
		ID3D11Buffer* buffer = nullptr;
	} old, newer;

	/* ---- DISPATCH ---- */
	newer.srvs[0] = texNormalisedHeight->srv.get();
	newer.uavs[0] = texShadowHeight->uav.get();
	newer.buffer = shadowUpdateCB->CB();

	context->CSSetShaderResources(0, ARRAYSIZE(newer.srvs), newer.srvs);
	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(newer.uavs), newer.uavs, nullptr);
	context->CSSetConstantBuffers(1, 1, &newer.buffer);
	context->CSSetShader(shadowUpdateProgram.get(), nullptr, 0);
	context->Dispatch(abs(shadowUpdateCBData.LightPxDir.x) >= abs(shadowUpdateCBData.LightPxDir.y) ? height : width, 1, 1);

	/* ---- RESTORE ---- */
	context->CSSetShaderResources(0, ARRAYSIZE(old.srvs), old.srvs);
	context->CSSetShader(old.shader, nullptr, 0);
	context->CSSetUnorderedAccessViews(0, ARRAYSIZE(old.uavs), old.uavs, nullptr);
	context->CSSetConstantBuffers(1, 1, &old.buffer);
}

void TerrainOcclusion::Prepass()
{
	LoadHeightmap();

	if (!settings.EnableTerrainShadow && !settings.EnableTerrainAO)
		return;

	if (needPrecompute)
		Precompute();
	if (settings.EnableTerrainShadow)
		UpdateShadow();

	{
		auto context = State::GetSingleton()->context;

		std::array<ID3D11ShaderResourceView*, 3> srvs = { nullptr };
		if (texOcclusion)
			srvs.at(0) = texOcclusion->srv.get();
		if (texNormalisedHeight)
			srvs.at(1) = texNormalisedHeight->srv.get();
		if (texShadowHeight)
			srvs.at(2) = texShadowHeight->srv.get();
		context->PSSetShaderResources(40, (uint)srvs.size(), srvs.data());
	}
}
