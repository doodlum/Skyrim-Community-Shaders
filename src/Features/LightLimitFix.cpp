#include "LightLimitFix.h"

#include "Shadercache.h"
#include "State.h"
#include "Util.h"

static constexpr uint CLUSTER_MAX_LIGHTS = 128;
static constexpr uint MAX_LIGHTS = 1024;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	LightLimitFix::Settings,
	EnableContactShadows)

void LightLimitFix::DrawSettings()
{
	if (ImGui::TreeNodeEx("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Contact Shadows", &settings.EnableContactShadows);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("All lights cast small shadows. Performance impact.");
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (ImGui::TreeNodeEx("Light Limit Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Lights Visualisation", &settings.EnableLightsVisualisation);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Enables visualization of the light limit\n");
		}

		{
			static const char* comboOptions[] = { "Light Limit", "Strict Lights Count", "Clustered Lights Count", "Shadow Mask" };
			ImGui::Combo("Lights Visualisation Mode", (int*)&settings.LightsVisualisationMode, comboOptions, 4);
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					" - Visualise the light limit. Red when the \"strict\" light limit is reached (portal-strict lights).\n"
					" - Visualise the number of strict lights.\n"
					" - Visualise the number of clustered lights.\n"
					" - Visualize the Shadow Mask.\n");
			}
		}
		currentEnableLightsVisualisation = settings.EnableLightsVisualisation;
		if (previousEnableLightsVisualisation != currentEnableLightsVisualisation) {
			State::GetSingleton()->SetDefines(settings.EnableLightsVisualisation ? "LLFDEBUG" : "");
			shaderCache.Clear(RE::BSShader::Type::Lighting);
			previousEnableLightsVisualisation = currentEnableLightsVisualisation;
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text(std::format("Clustered Light Count : {}", lightCount).c_str());

		ImGui::TreePop();
	}
}

LightLimitFix::PerFrame LightLimitFix::GetCommonBufferData()
{
	PerFrame perFrame{};
	perFrame.EnableContactShadows = settings.EnableContactShadows;
	perFrame.EnableLightsVisualisation = settings.EnableLightsVisualisation;
	perFrame.LightsVisualisationMode = settings.LightsVisualisationMode;
	std::copy(clusterSize, clusterSize + 3, perFrame.ClusterSize);
	return perFrame;
}

void LightLimitFix::SetupResources()
{
	auto screenSize = Util::ConvertToDynamic(State::GetSingleton()->screenSize);
	if (REL::Module::IsVR())
		screenSize.x *= .5;
	clusterSize[0] = ((uint)screenSize.x + 63) / 64;
	clusterSize[1] = ((uint)screenSize.y + 63) / 64;
	clusterSize[2] = 32;
	uint clusterCount = clusterSize[0] * clusterSize[1] * clusterSize[2];

	{
		std::string clusterSizeStrs[3];
		for (int i = 0; i < 3; ++i)
			clusterSizeStrs[i] = std::format("{}", clusterSize[i]);

		std::vector<std::pair<const char*, const char*>> defines = {
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_X", clusterSizeStrs[0].c_str() },
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_Y", clusterSizeStrs[1].c_str() },
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_Z", clusterSizeStrs[2].c_str() }
		};

		clusterBuildingCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\LightLimitFix\\ClusterBuildingCS.hlsl", defines, "cs_5_0");
		clusterCullingCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\LightLimitFix\\ClusterCullingCS.hlsl", defines, "cs_5_0");

		lightBuildingCB = new ConstantBuffer(ConstantBufferDesc<LightBuildingCB>());
		lightCullingCB = new ConstantBuffer(ConstantBufferDesc<LightCullingCB>());
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

		std::uint32_t numElements = clusterCount;

		sbDesc.StructureByteStride = sizeof(ClusterAABB);
		sbDesc.ByteWidth = sizeof(ClusterAABB) * numElements;
		clusters = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		clusters->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		clusters->CreateUAV(uavDesc);

		numElements = 1;
		sbDesc.StructureByteStride = sizeof(uint32_t);
		sbDesc.ByteWidth = sizeof(uint32_t) * numElements;
		lightCounter = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightCounter->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightCounter->CreateUAV(uavDesc);

		numElements = clusterCount * CLUSTER_MAX_LIGHTS;
		sbDesc.StructureByteStride = sizeof(uint32_t);
		sbDesc.ByteWidth = sizeof(uint32_t) * numElements;
		lightList = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightList->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightList->CreateUAV(uavDesc);

		numElements = clusterCount;
		sbDesc.StructureByteStride = sizeof(LightGrid);
		sbDesc.ByteWidth = sizeof(LightGrid) * numElements;
		lightGrid = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightGrid->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightGrid->CreateUAV(uavDesc);
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(LightData);
		sbDesc.ByteWidth = sizeof(LightData) * MAX_LIGHTS;
		lights = eastl::make_unique<Buffer>(sbDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = MAX_LIGHTS;
		lights->CreateSRV(srvDesc);
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(StrictLightData);
		sbDesc.ByteWidth = sizeof(StrictLightData);
		strictLightData = std::make_unique<Buffer>(sbDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = 1;
		strictLightData->CreateSRV(srvDesc);
	}
}

void LightLimitFix::Reset()
{
}

void LightLimitFix::LoadSettings(json& o_json)
{
	settings = o_json;
}

void LightLimitFix::SaveSettings(json& o_json)
{
	o_json = settings;
}

void LightLimitFix::RestoreDefaultSettings()
{
	settings = {};
}

RE::NiNode* GetParentRoomNode(RE::NiAVObject* object)
{
	if (object == nullptr) {
		return nullptr;
	}

	static const auto* roomRtti = REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSMultiBoundRoom }.get();
	static const auto* portalRtti = REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSPortalSharedNode }.get();

	const auto* rtti = object->GetRTTI();
	if (rtti == roomRtti || rtti == portalRtti) {
		return static_cast<RE::NiNode*>(object);
	}

	return GetParentRoomNode(object->parent);
}

void LightLimitFix::BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* a_pass)
{
	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	strictLightDataTemp.NumStrictLights = 0;

	strictLightDataTemp.RoomIndex = -1;
	if (!roomNodes.empty()) {
		if (RE::NiNode* roomNode = GetParentRoomNode(a_pass->geometry)) {
			if (auto it = roomNodes.find(roomNode); it != roomNodes.cend()) {
				strictLightDataTemp.RoomIndex = it->second;
			}
		}
	}
}

void LightLimitFix::BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights(RE::BSRenderPass* a_pass, DirectX::XMMATRIX&, uint32_t, uint32_t, float, Space)
{
	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
	bool inWorld = accumulator->GetRuntimeData().activeShadowSceneNode == RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	strictLightDataTemp.NumStrictLights = inWorld ? 0 : (a_pass->numLights - 1);

	for (uint32_t i = 0; i < strictLightDataTemp.NumStrictLights; i++) {
		auto bsLight = a_pass->sceneLights[i + 1];
		auto niLight = bsLight->light.get();

		auto& runtimeData = niLight->GetLightRuntimeData();

		LightData light{};
		light.color = { runtimeData.diffuse.red, runtimeData.diffuse.green, runtimeData.diffuse.blue };
		light.color *= runtimeData.fade;
		light.color *= bsLight->lodDimmer;

		light.radius = runtimeData.radius.x;

		SetLightPosition(light, niLight->world.translate, inWorld);

		if (bsLight->IsShadowLight()) {
			auto* shadowLight = static_cast<RE::BSShadowLight*>(bsLight);
			GET_INSTANCE_MEMBER(shadowLightIndex, shadowLight);
			light.shadowMaskIndex = shadowLightIndex;
			light.lightFlags.set(LightFlags::Shadow);
		}

		strictLightDataTemp.StrictLights[i] = light;
	}
}

void LightLimitFix::BSLightingShader_SetupGeometry_After(RE::BSRenderPass*)
{
	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	static auto& context = State::GetSingleton()->context;
	auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();

	static bool wasEmpty = false;
	static bool wasWorld = false;
	static int previousRoomIndex = -1;

	static auto shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	const bool isEmpty = strictLightDataTemp.NumStrictLights == 0;
	const bool isWorld = accumulator->GetRuntimeData().activeShadowSceneNode == shadowSceneNode;
	const int roomIndex = strictLightDataTemp.RoomIndex;

	if (!isEmpty || (isEmpty && !wasEmpty) || isWorld != wasWorld || previousRoomIndex != roomIndex) {
		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(strictLightData->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(StrictLightData);
		memcpy_s(mapped.pData, bytes, &strictLightDataTemp, bytes);
		context->Unmap(strictLightData->resource.get(), 0);

		wasEmpty = isEmpty;
		wasWorld = isWorld;
		previousRoomIndex = roomIndex;
	}

	static Util::FrameChecker frameChecker;
	if (frameChecker.IsNewFrame()) {
		ID3D11ShaderResourceView* view = strictLightData->srv.get();
		context->PSSetShaderResources(53, 1, &view);
	}
}

void LightLimitFix::SetLightPosition(LightLimitFix::LightData& a_light, RE::NiPoint3 a_initialPosition, bool a_cached)
{
	for (int eyeIndex = 0; eyeIndex < eyeCount; eyeIndex++) {
		RE::NiPoint3 eyePosition;
		Matrix viewMatrix;

		if (a_cached) {
			eyePosition = eyePositionCached[eyeIndex];
			viewMatrix = viewMatrixCached[eyeIndex];
		} else {
			eyePosition = Util::GetEyePosition(eyeIndex);
			viewMatrix = Util::GetCameraData(eyeIndex).viewMat;
		}

		auto worldPos = a_initialPosition - eyePosition;
		a_light.positionWS[eyeIndex].data.x = worldPos.x;
		a_light.positionWS[eyeIndex].data.y = worldPos.y;
		a_light.positionWS[eyeIndex].data.z = worldPos.z;
		a_light.positionVS[eyeIndex].data = DirectX::SimpleMath::Vector3::Transform(a_light.positionWS[eyeIndex].data, viewMatrix);
	}
}

void LightLimitFix::Prepass()
{
	static auto& context = State::GetSingleton()->context;

	UpdateLights();

	ID3D11ShaderResourceView* views[3]{};
	views[0] = lights->srv.get();
	views[1] = lightList->srv.get();
	views[2] = lightGrid->srv.get();
	context->PSSetShaderResources(50, ARRAYSIZE(views), views);
}

bool LightLimitFix::IsValidLight(RE::BSLight* a_light)
{
	return a_light && !a_light->light->GetFlags().any(RE::NiAVObject::Flag::kHidden);
}

bool LightLimitFix::IsGlobalLight(RE::BSLight* a_light)
{
	return !(a_light->portalStrict || !a_light->portalGraph);
}

void LightLimitFix::PostPostLoad()
{
	Hooks::Install();
}

void LightLimitFix::DataLoaded()
{
	auto iMagicLightMaxCount = RE::GameSettingCollection::GetSingleton()->GetSetting("iMagicLightMaxCount");
	iMagicLightMaxCount->data.i = MAXINT32;
	logger::info("[LLF] Unlocked magic light limit");
}

float LightLimitFix::CalculateLightDistance(float3 a_lightPosition, float a_radius)
{
	return (a_lightPosition.x * a_lightPosition.x) + (a_lightPosition.y * a_lightPosition.y) + (a_lightPosition.z * a_lightPosition.z) - (a_radius * a_radius);
}

void LightLimitFix::UpdateLights()
{
	static float& cameraNear = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x40));
	static float& cameraFar = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x44));

	lightsNear = cameraNear;
	lightsFar = cameraFar;

	static auto shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	// Cache data since cameraData can become invalid in first-person

	for (int eyeIndex = 0; eyeIndex < eyeCount; eyeIndex++) {
		eyePositionCached[eyeIndex] = Util::GetEyePosition(eyeIndex);
		viewMatrixCached[eyeIndex] = Util::GetCameraData(eyeIndex).viewMat;
		viewMatrixCached[eyeIndex].Invert(viewMatrixInverseCached[eyeIndex]);
	}

	eastl::vector<LightData> lightsData{};
	lightsData.reserve(MAX_LIGHTS);

	// Process point lights

	roomNodes.empty();

	auto addRoom = [&](void* nodePtr, LightData& light) {
		uint8_t roomIndex = 0;
		auto* node = static_cast<RE::NiNode*>(nodePtr);
		if (auto it = roomNodes.find(node); it == roomNodes.cend()) {
			roomIndex = static_cast<uint8_t>(roomNodes.size());
			roomNodes.insert_or_assign(node, roomIndex);
		} else {
			roomIndex = it->second;
		}
		light.roomFlags.SetBit(roomIndex, 1);
	};

	auto addLight = [&](const RE::NiPointer<RE::BSLight>& e) {
		if (auto bsLight = e.get()) {
			if (auto niLight = bsLight->light.get()) {
				if (IsValidLight(bsLight)) {
					auto& runtimeData = niLight->GetLightRuntimeData();

					LightData light{};
					light.color = { runtimeData.diffuse.red, runtimeData.diffuse.green, runtimeData.diffuse.blue };
					light.color *= runtimeData.fade;
					light.color *= bsLight->lodDimmer;

					light.radius = runtimeData.radius.x;

					if (!IsGlobalLight(bsLight)) {
						// List of BSMultiBoundRooms affected by a light
						for (const auto& roomPtr : bsLight->unk0D8) {
							addRoom(roomPtr, light);
						}
						// List of BSPortals affected by a light
						for (const auto& portalPtr : bsLight->unk0F0) {
							struct BSPortal
							{
								uint8_t data[0x128];
								void* portalSharedNode;
							};
							addRoom(static_cast<BSPortal*>(portalPtr)->portalSharedNode, light);
						}
						light.lightFlags.set(LightFlags::PortalStrict);
					}

					if (bsLight->IsShadowLight()) {
						auto* shadowLight = static_cast<RE::BSShadowLight*>(bsLight);
						GET_INSTANCE_MEMBER(shadowLightIndex, shadowLight);
						light.shadowMaskIndex = shadowLightIndex;
						light.lightFlags.set(LightFlags::Shadow);
					}

					SetLightPosition(light, niLight->world.translate);

					if ((light.color.x + light.color.y + light.color.z) > 1e-4 && light.radius > 1e-4) {
						lightsData.push_back(light);
					}
				}
			}
		}
	};

	for (auto& e : shadowSceneNode->GetRuntimeData().activeLights) {
		addLight(e);
	}

	for (auto& e : shadowSceneNode->GetRuntimeData().activeShadowLights) {
		addLight(e);
	}

	static auto& context = State::GetSingleton()->context;

	{
		auto projMatrixUnjittered = Util::GetCameraData(0).projMatrixUnjittered;
		float fov = atan(1.0f / static_cast<float4x4>(projMatrixUnjittered).m[0][0]) * 2.0f * (180.0f / 3.14159265359f);

		static float _lightsNear = 0.0f, _lightsFar = 0.0f, _fov = 0.0f;
		if (fabs(_fov - fov) > 1e-4 || fabs(_lightsNear - lightsNear) > 1e-4 || fabs(_lightsFar - lightsFar) > 1e-4) {
			LightBuildingCB updateData{};
			updateData.InvProjMatrix[0] = DirectX::XMMatrixInverse(nullptr, projMatrixUnjittered);
			if (eyeCount == 1)
				updateData.InvProjMatrix[1] = updateData.InvProjMatrix[0];
			else
				updateData.InvProjMatrix[1] = DirectX::XMMatrixInverse(nullptr, Util::GetCameraData(1).projMatrixUnjittered);
			updateData.LightsNear = lightsNear;
			updateData.LightsFar = lightsFar;

			lightBuildingCB->Update(updateData);

			ID3D11Buffer* buffer = lightBuildingCB->CB();
			context->CSSetConstantBuffers(0, 1, &buffer);

			ID3D11UnorderedAccessView* clusters_uav = clusters->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &clusters_uav, nullptr);

			context->CSSetShader(clusterBuildingCS, nullptr, 0);
			context->Dispatch(clusterSize[0], clusterSize[1], clusterSize[2]);

			ID3D11UnorderedAccessView* null_uav = nullptr;
			context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);

			_fov = fov;
			_lightsNear = lightsNear;
			_lightsFar = lightsFar;
		}
	}

	{
		lightCount = std::min((uint)lightsData.size(), MAX_LIGHTS);

		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(lights->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(LightData) * lightCount;
		memcpy_s(mapped.pData, bytes, lightsData.data(), bytes);
		context->Unmap(lights->resource.get(), 0);

		LightCullingCB updateData{};
		updateData.LightCount = lightCount;
		lightCullingCB->Update(updateData);

		ID3D11Buffer* buffer = lightCullingCB->CB();
		context->CSSetConstantBuffers(0, 1, &buffer);

		ID3D11ShaderResourceView* srvs[] = { clusters->srv.get(), lights->srv.get() };
		context->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { lightCounter->uav.get(), lightList->uav.get(), lightGrid->uav.get() };
		context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);

		context->CSSetShader(clusterCullingCS, nullptr, 0);
		context->Dispatch((clusterSize[0] + 15) / 16, (clusterSize[1] + 15) / 16, (clusterSize[2] + 3) / 4);
	}

	context->CSSetShader(nullptr, nullptr, 0);

	ID3D11Buffer* null_buffer = nullptr;
	context->CSSetConstantBuffers(0, 1, &null_buffer);

	ID3D11ShaderResourceView* null_srvs[2] = { nullptr };
	context->CSSetShaderResources(0, 2, null_srvs);

	ID3D11UnorderedAccessView* null_uavs[3] = { nullptr };
	context->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
}