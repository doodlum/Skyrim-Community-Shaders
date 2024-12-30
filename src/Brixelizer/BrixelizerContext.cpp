#include "BrixelizerContext.h"

#include <DDSTextureLoader.h>
#include <DirectXMesh.h>

#include "Deferred.h"
#include "Util.h"

void BrixelizerContext::InitBrixelizerContext()
{
	auto manager = RE::BSGraphics::Renderer::GetSingleton();
	auto device = reinterpret_cast<ID3D11Device*>(manager->GetRuntimeData().forwarder);

	stl::detour_vfunc<3, BrixelizerContext::Hooks::ID3D11Device_CreateBuffer>(device);
	stl::detour_vfunc<11, BrixelizerContext::Hooks::ID3D11Device_CreateInputLayout>(device);

	auto ffxDevice = ffxGetDeviceDX12(Brixelizer::GetSingleton()->d3d12Device.get());

	size_t scratchBufferSize = ffxGetScratchMemorySizeDX12(2);
	void* scratchBuffer = calloc(scratchBufferSize, 1);
	memset(scratchBuffer, 0, scratchBufferSize);

	if (ffxGetInterfaceDX12(&initializationParameters.backendInterface, ffxDevice, scratchBuffer, scratchBufferSize, FFX_FSR3UPSCALER_CONTEXT_COUNT) != FFX_OK)
		logger::critical("[Brixelizer] Failed to initialize Brixelizer backend interface!");

	initializationParameters.sdfCenter[0] = 0.0f;
	initializationParameters.sdfCenter[1] = 0.0f;
	initializationParameters.sdfCenter[2] = 0.0f;
	initializationParameters.flags = FFX_BRIXELIZER_CONTEXT_FLAG_ALL_DEBUG;
	initializationParameters.numCascades = NUM_BRIXELIZER_CASCADES;

	float voxelSize = 10.f;
	for (uint32_t i = 0; i < NUM_BRIXELIZER_CASCADES; ++i) {
		FfxBrixelizerCascadeDescription* cascadeDesc = &initializationParameters.cascadeDescs[i];
		cascadeDesc->flags = FfxBrixelizerCascadeFlag(FFX_BRIXELIZER_CASCADE_DYNAMIC | FFX_BRIXELIZER_CASCADE_STATIC);
		cascadeDesc->voxelSize = voxelSize;
		voxelSize *= 2.0f;
	}

	FfxErrorCode error = ffxBrixelizerContextCreate(&initializationParameters, &brixelizerContext);
	if (error != FFX_OK) {
		logger::critical("[Brixelizer] Failed to initialize Brixelizer context!");
	}

	D3D12_RESOURCE_DESC sdfAtlasDesc = {};
	sdfAtlasDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	sdfAtlasDesc.Width = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
	sdfAtlasDesc.Height = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
	sdfAtlasDesc.DepthOrArraySize = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
	sdfAtlasDesc.MipLevels = 1;
	sdfAtlasDesc.Format = DXGI_FORMAT_R8_UNORM;
	sdfAtlasDesc.SampleDesc.Count = 1;
	sdfAtlasDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	sdfAtlasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	Brixelizer::CreatedWrappedResource3D(sdfAtlasDesc, sdfAtlas);

	brickAABBs = CreateWrappedBuffer(FFX_BRIXELIZER_BRICK_AABBS_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	for (int i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; i++) {
		cascadeAABBTrees[i] = CreateWrappedBuffer(FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		cascadeBrickMaps[i] = CreateWrappedBuffer(FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	gpuScratchBuffer = CreateBuffer(GPU_SCRATCH_BUFFER_SIZE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	{
		auto state = State::GetSingleton();

		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = (uint)state->screenSize.x;
		texDesc.Height = (uint)state->screenSize.y;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

		Brixelizer::CreatedWrappedResource(texDesc, debugRenderTarget);
	}
}

winrt::com_ptr<ID3D12Resource> BrixelizerContext::CreateBuffer(UINT64 size, D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	winrt::com_ptr<ID3D12Resource> buffer;

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

	DX::ThrowIfFailed(Brixelizer::GetSingleton()->d3d12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)));

	return buffer;
}

Brixelizer::WrappedBuffer BrixelizerContext::CreateWrappedBuffer(UINT64 size, D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	Brixelizer::WrappedBuffer wrappedBuffer;

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

	DX::ThrowIfFailed(Brixelizer::GetSingleton()->d3d12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		resourceState,
		nullptr,
		IID_PPV_ARGS(&wrappedBuffer.buffer)));

	auto brixelizer = Brixelizer::GetSingleton();

	HANDLE sharedHandle = nullptr;
	DX::ThrowIfFailed(brixelizer->d3d12Device->CreateSharedHandle(wrappedBuffer.buffer.get(), nullptr, GENERIC_ALL, nullptr, &sharedHandle));

	DX::ThrowIfFailed(brixelizer->d3d11Device->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(&wrappedBuffer.buffer)));

	return wrappedBuffer;
}

void BrixelizerContext::BSTriShape_UpdateWorldData(RE::BSTriShape* This, RE::NiUpdateData* a_data)
{
	std::lock_guard lock{ mutex };

	RE::NiPoint3 pointA = This->world * RE::NiPoint3{ 1.0f, 1.0f, 1.0f };

	Hooks::BSTriShape_UpdateWorldData::func(This, a_data);

	RE::NiPoint3 pointB = This->world * RE::NiPoint3{ 1.0f, 1.0f, 1.0f };

	if (pointA.GetDistance(pointB) > 0.1f) {
		auto it = instances.find(This);
		if (it != instances.end()) {
			auto& instanceData = (*it).second;
			auto error = ffxBrixelizerDeleteInstances(&brixelizerContext, &instanceData.instanceID, 1);
			if (error != FFX_OK)
				logger::critical("error");
			instances.erase(it);
		}
	}
}

uint BrixelizerContext::GetBufferIndex(BufferData& a_bufferData)
{
	if (a_bufferData.registered)
		return a_bufferData.index;

	FfxResource ffxResource = ffxGetResourceDX12(a_bufferData.buffer.get(), ffxGetResourceDescriptionDX12(a_bufferData.buffer.get(), FFX_RESOURCE_USAGE_READ_ONLY), nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

	FfxBrixelizerBufferDescription brixelizerBufferDesc = {};
	brixelizerBufferDesc.buffer = ffxResource;
	brixelizerBufferDesc.outIndex = &a_bufferData.index;
	auto error = ffxBrixelizerRegisterBuffers(&brixelizerContext, &brixelizerBufferDesc, 1);
	if (error != FFX_OK)
		logger::critical("error");

	a_bufferData.registered = true;

	return a_bufferData.index;
}

DirectX::XMMATRIX GetXMFromNiTransform(const RE::NiTransform& Transform)
{
	DirectX::XMMATRIX temp;

	const RE::NiMatrix3& m = Transform.rotate;
	const float scale = Transform.scale;

	temp.r[0] = DirectX::XMVectorScale(DirectX::XMVectorSet(
										   m.entry[0][0],
										   m.entry[1][0],
										   m.entry[2][0],
										   0.0f),
		scale);

	temp.r[1] = DirectX::XMVectorScale(DirectX::XMVectorSet(
										   m.entry[0][1],
										   m.entry[1][1],
										   m.entry[2][1],
										   0.0f),
		scale);

	temp.r[2] = DirectX::XMVectorScale(DirectX::XMVectorSet(
										   m.entry[0][2],
										   m.entry[1][2],
										   m.entry[2][2],
										   0.0f),
		scale);

	temp.r[3] = DirectX::XMVectorSet(
		Transform.translate.x,
		Transform.translate.y,
		Transform.translate.z,
		1.0f);

	return temp;
}

void BrixelizerContext::AddInstance(RE::BSTriShape* a_geometry)
{
	const auto& transform = a_geometry->world;
	const auto& modelData = a_geometry->GetModelData().modelBound;
	if (a_geometry->worldBound.radius == 0)
		return;

	auto effect = a_geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect];
	auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect.get());

	if (!lightingShader)
		return;

	if (!lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferWrite, RE::BSShaderProperty::EShaderPropertyFlag::kZBufferTest))
		return;

	if (lightingShader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kLODObjects, RE::BSShaderProperty::EShaderPropertyFlag::kLODLandscape, RE::BSShaderProperty::EShaderPropertyFlag::kHDLODObjects))
		return;

	if (lightingShader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kSkinned))
		return;

	// TODO: FIX THIS
	if (lightingShader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape, RE::BSShaderProperty::EShaderPropertyFlag::kMultipleTextures, RE::BSShaderProperty::EShaderPropertyFlag::kMultiIndexSnow))
		return;

	const RE::NiPoint3 c = { modelData.center.x, modelData.center.y, modelData.center.z };
	const RE::NiPoint3 r = { modelData.radius, modelData.radius, modelData.radius };
	const RE::NiPoint3 aabbMinVec = c - r;
	const RE::NiPoint3 aabbMaxVec = c + r;
	const RE::NiPoint3 extents = aabbMaxVec - aabbMinVec;

	const RE::NiPoint3 aabbCorners[8] = {
		aabbMinVec + RE::NiPoint3(0.0f, 0.0f, 0.0f),
		aabbMinVec + RE::NiPoint3(extents.x, 0.0f, 0.0f),
		aabbMinVec + RE::NiPoint3(0.0f, 0.0f, extents.z),
		aabbMinVec + RE::NiPoint3(extents.x, 0.0f, extents.z),
		aabbMinVec + RE::NiPoint3(0.0f, extents.y, 0.0f),
		aabbMinVec + RE::NiPoint3(extents.x, extents.y, 0.0f),
		aabbMinVec + RE::NiPoint3(0.0f, extents.y, extents.z),
		aabbMinVec + RE::NiPoint3(extents.x, extents.y, extents.z),
	};

	float4 minExtents = float4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
	float4 maxExtents = float4(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (uint i = 0; i < 8; i++) {
		auto transformed = transform * aabbCorners[i];
		float4 transformedF = { transformed.x, transformed.y, transformed.z, 0 };

		minExtents = (float4)_mm_min_ps(minExtents, transformedF);
		maxExtents = (float4)_mm_max_ps(maxExtents, transformedF);
	}

	auto rendererData = a_geometry->GetGeometryRuntimeData().rendererData;

	if (!rendererData || !rendererData->vertexBuffer || !rendererData->indexBuffer)
		return;

	BufferData* vertexBuffer;
	BufferData* indexBuffer;

	{
		auto it2 = vertexBuffers.find((ID3D11Buffer*)rendererData->vertexBuffer);
		if (it2 == vertexBuffers.end())
			return;
		vertexBuffer = &it2->second;
	}

	{
		auto it2 = indexBuffers.find((ID3D11Buffer*)rendererData->indexBuffer);
		if (it2 == indexBuffers.end())
			return;
		indexBuffer = &it2->second;
	}

	auto vertexDesc = *(uint64_t*)&rendererData->vertexDesc;

	InputLayoutData* inputLayoutData;

	{
		auto it2 = vertexDescToInputLayout.find(vertexDesc);
		if (it2 == vertexDescToInputLayout.end())
			return;
		auto inputLayout = it2->second;
		auto it3 = inputLayouts.find(inputLayout);
		if (it3 == inputLayouts.end())
			return;
		inputLayoutData = &it3->second;
	}

	FfxBrixelizerInstanceDescription instanceDesc = {};

	{
		instanceDesc.aabb.min[0] = minExtents.x;
		instanceDesc.aabb.max[0] = maxExtents.x;

		instanceDesc.aabb.min[1] = minExtents.y;
		instanceDesc.aabb.max[1] = maxExtents.y;

		instanceDesc.aabb.min[2] = minExtents.z;
		instanceDesc.aabb.max[2] = maxExtents.z;
	}

	float4x4 xmmTransform = GetXMFromNiTransform(transform);

	for (uint row = 0; row < 3; ++row) {
		for (uint col = 0; col < 4; ++col) {
			instanceDesc.transform[row * 4 + col] = xmmTransform.m[col][row];
		}
	}

	instanceDesc.indexFormat = FFX_INDEX_TYPE_UINT16;
	instanceDesc.indexBuffer = GetBufferIndex(*indexBuffer);
	instanceDesc.indexBufferOffset = 0;
	instanceDesc.triangleCount = a_geometry->GetTrishapeRuntimeData().triangleCount;

	instanceDesc.vertexBuffer = GetBufferIndex(*vertexBuffer);
	instanceDesc.vertexStride = ((*(uint64_t*)&rendererData->vertexDesc) << 2) & 0x3C;
	instanceDesc.vertexBufferOffset = rendererData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);
	instanceDesc.vertexCount = a_geometry->GetTrishapeRuntimeData().vertexCount;
	instanceDesc.vertexFormat = FFX_SURFACE_FORMAT_R32G32B32_FLOAT;

	instanceDesc.vertexStride = inputLayoutData->vertexStride;
	instanceDesc.vertexBufferOffset = inputLayoutData->vertexBufferOffset;
	instanceDesc.vertexCount = a_geometry->GetTrishapeRuntimeData().vertexCount;
	instanceDesc.vertexFormat = inputLayoutData->vertexFormat;

	InstanceData instanceData{};
	instanceDesc.outInstanceID = &instanceData.instanceID;
	instanceDesc.flags = FFX_BRIXELIZER_INSTANCE_FLAG_NONE;

	FfxErrorCode error = ffxBrixelizerCreateInstances(&brixelizerContext, &instanceDesc, 1);
	if (error != FFX_OK)
		logger::critical("error");

	instances.insert({ a_geometry, instanceData });
}

RE::BSFadeNode* FindBSFadeNode(RE::NiNode* a_niNode)
{
	if (auto fadeNode = a_niNode->AsFadeNode()) {
		return fadeNode;
	}
	return a_niNode->parent ? FindBSFadeNode(a_niNode->parent) : nullptr;
}

void BrixelizerContext::SeenInstance(RE::BSTriShape* a_geometry)
{
	auto& flags = a_geometry->GetFlags();
	if (flags.none(RE::NiAVObject::Flag::kHidden) && flags.all(RE::NiAVObject::Flag::kRenderUse)) {
		if (auto fadeNode = FindBSFadeNode((RE::NiNode*)a_geometry)) {
			if (auto extraData = fadeNode->GetExtraData("BSX")) {
				auto bsxFlags = (RE::BSXFlags*)extraData;
				auto value = static_cast<int32_t>(bsxFlags->value);
				if (value & (int32_t)RE::BSXFlags::Flag::kEditorMarker)
					return;
			}
		}

		auto it = instances.find(a_geometry);
		if (it != instances.end()) {
			auto& instanceData = (*it).second;
			instanceData.visibleState = visibleStateValue;
		} else {
			queuedInstances.insert(a_geometry);
		}
	}
}

void BrixelizerContext::RemoveInstance(RE::BSTriShape* This)
{
	std::lock_guard lock{ mutex };

	auto it = instances.find(This);
	if (it != instances.end()) {
		auto& instanceData = (*it).second;
		auto error = ffxBrixelizerDeleteInstances(&brixelizerContext, &instanceData.instanceID, 1);
		if (error != FFX_OK)
			logger::critical("error");
		instances.erase(it);
	}
}

BrixelizerContext::BufferData BrixelizerContext::AllocateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	BufferData data;

	// Allocate the memory on the CPU and GPU
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = pDesc->ByteWidth;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.SampleDesc.Quality = 0;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		DX::ThrowIfFailed(Brixelizer::GetSingleton()->d3d12Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&data.buffer)));

		data.width = pDesc->ByteWidth;
	}

	// Map the buffer to CPU memory and copy the vertex data
	{
		void* pVertexData = nullptr;
		data.buffer->Map(0, nullptr, &pVertexData);
		memcpy(pVertexData, pInitialData->pSysMem, pDesc->ByteWidth);
		data.buffer->Unmap(0, nullptr);
	}

	return data;
}

struct ID3D11Buffer_Release
{
	static void thunk(ID3D11Buffer* This)
	{
		BrixelizerContext::GetSingleton()->UnregisterVertexBuffer(This);
		BrixelizerContext::GetSingleton()->UnregisterIndexBuffer(This);
		func(This);
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

bool hooked = false;

void BrixelizerContext::RegisterVertexBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	BufferData data = AllocateBuffer(pDesc, pInitialData);
	vertexBuffers.insert({ *ppBuffer, data });
	if (!hooked) {
		stl::detour_vfunc<2, ID3D11Buffer_Release>(*ppBuffer);
		hooked = true;
	}
}

void BrixelizerContext::RegisterIndexBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	BufferData data = AllocateBuffer(pDesc, pInitialData);
	indexBuffers.insert({ *ppBuffer, data });
	if (!hooked) {
		stl::detour_vfunc<2, ID3D11Buffer_Release>(*ppBuffer);
		hooked = true;
	}
}

void BrixelizerContext::RegisterInputLayout(ID3D11InputLayout* ppInputLayout, D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements)
{
	InputLayoutData data = {};
	for (UINT i = 0; i < NumElements; i++) {
		if (strcmp(pInputElementDescs[i].SemanticName, "POSITION") == 0) {
			data.vertexStride = 0;

			for (UINT k = 0; k < NumElements; k++) {
				data.vertexStride += (UINT)DirectX::BytesPerElement(pInputElementDescs[k].Format);
			}

			data.vertexBufferOffset = 0;

			for (UINT k = 0; k < i; k++) {
				data.vertexBufferOffset += (UINT)DirectX::BytesPerElement(pInputElementDescs[k].Format);
			}

			auto format = pInputElementDescs[i].Format;
			if (format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT) {
				data.vertexFormat = FFX_SURFACE_FORMAT_R32G32B32_FLOAT;
			} else if (format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
				data.vertexFormat = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
			} else {
				return;
			}

			inputLayouts.insert({ ppInputLayout, data });
		}
	}
}

void BrixelizerContext::UnregisterVertexBuffer(ID3D11Buffer* ppBuffer)
{
	vertexBuffers.erase(ppBuffer);
}

void BrixelizerContext::UnregisterIndexBuffer(ID3D11Buffer* ppBuffer)
{
	indexBuffers.erase(ppBuffer);
}

void BrixelizerContext::TransitionResources(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	// Transition the SDF Atlas
	barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		sdfAtlas.resource.get(),
		stateBefore,
		stateAfter));

	// Transition the Brick AABBs
	barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		brickAABBs.buffer.get(),
		stateBefore,
		stateAfter));

	// Transition each Cascade AABB Tree
	for (const auto aabbTree : cascadeAABBTrees) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			aabbTree.buffer.get(),
			stateBefore,
			stateAfter));
	}

	// Transition each Cascade Brick Map
	for (const auto brickMap : cascadeBrickMaps) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			brickMap.buffer.get(),
			stateBefore,
			stateAfter));
	}

	// Execute the resource barriers on the command list
	cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

FfxBrixelizerDebugVisualizationDescription BrixelizerContext::GetDebugVisualization()
{
	auto state = State::GetSingleton();

	FfxBrixelizerDebugVisualizationDescription debugVisDesc{};

	auto frameBufferCached = Brixelizer::GetSingleton()->frameBufferCached;

	auto cameraViewInverseAdjusted = frameBufferCached.CameraViewInverse.Transpose();
	cameraViewInverseAdjusted._41 += frameBufferCached.CameraPosAdjust.x;
	cameraViewInverseAdjusted._42 += frameBufferCached.CameraPosAdjust.y;
	cameraViewInverseAdjusted._43 += frameBufferCached.CameraPosAdjust.z;
	auto cameraProjInverse = frameBufferCached.CameraProjInverse.Transpose();

	memcpy(&debugVisDesc.inverseViewMatrix, &cameraViewInverseAdjusted, sizeof(debugVisDesc.inverseViewMatrix));
	memcpy(&debugVisDesc.inverseProjectionMatrix, &cameraProjInverse, sizeof(debugVisDesc.inverseProjectionMatrix));

	debugVisDesc.debugState = m_DebugMode;

	uint32_t cascadeIndexOffset = 0;

	debugVisDesc.startCascadeIndex = cascadeIndexOffset + m_StartCascadeIdx;
	debugVisDesc.endCascadeIndex = cascadeIndexOffset + m_EndCascadeIdx;

	debugVisDesc.tMin = m_TMin;
	debugVisDesc.tMax = m_TMax;
	debugVisDesc.sdfSolveEps = m_SdfSolveEps;
	debugVisDesc.renderWidth = (uint)state->screenSize.x;
	debugVisDesc.renderHeight = (uint)state->screenSize.y;
	debugVisDesc.output = ffxGetResourceDX12(debugRenderTarget.resource.get(), ffxGetResourceDescriptionDX12(debugRenderTarget.resource.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

	return debugVisDesc;
}

void BrixelizerContext::UpdateBrixelizerContext(ID3D12GraphicsCommandList* cmdList)
{
	std::lock_guard lock{ mutex };

	static auto shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	if (Brixelizer::GetSingleton()->update) {
		RE::BSVisit::TraverseScenegraphGeometries(shadowSceneNode, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
			if (auto triShape = a_geometry->AsTriShape()) {
				SeenInstance(triShape);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		for (auto it = instances.begin(); it != instances.end();) {
			if (it->second.visibleState != visibleStateValue) {
				auto error = ffxBrixelizerDeleteInstances(&brixelizerContext, &it->second.instanceID, 1);
				if (error != FFX_OK)
					logger::critical("error");

				it = instances.erase(it);

			} else {
				++it;
			}
		}

		for (auto& queuedInstance : queuedInstances) {
			AddInstance(queuedInstance);
		}
	}

	queuedInstances.clear();

	// Transition all resources to resource state expected by Brixelizer
	TransitionResources(cmdList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	//FfxBrixelizerDebugVisualizationDescription debugVisDesc = GetDebugVisualization();

	size_t scratchBufferSize = 0;

	FfxBrixelizerPopulateDebugAABBsFlags populateDebugAABBFlags = FFX_BRIXELIZER_POPULATE_AABBS_NONE;
	if (m_ShowStaticInstanceAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES);
	if (m_ShowDynamicInstanceAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_DYNAMIC_INSTANCES);
	if (m_ShowCascadeAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_CASCADE_AABBS);

	auto frameBufferCached = Brixelizer::GetSingleton()->frameBufferCached;

	FfxBrixelizerUpdateDescription updateDesc = {
		.frameIndex = RE::BSGraphics::State::GetSingleton()->frameCount,
		.sdfCenter = { frameBufferCached.CameraPosAdjust.x, frameBufferCached.CameraPosAdjust.y, frameBufferCached.CameraPosAdjust.z },
		.populateDebugAABBsFlags = populateDebugAABBFlags,
		.debugVisualizationDesc = nullptr,
		.maxReferences = 32 * (1 << 20),
		.triangleSwapSize = 300 * (1 << 20),
		.maxBricksPerBake = 1 << 14,
		.outScratchBufferSize = &scratchBufferSize,
		.outStats = &stats,
	};

	// Fill out the Brixelizer update description resources.
	// Pass in the externally created output resources as FfxResource objects.
	updateDesc.resources.sdfAtlas = ffxGetResourceDX12(sdfAtlas.resource.get(), ffxGetResourceDescriptionDX12(sdfAtlas.resource.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	updateDesc.resources.brickAABBs = ffxGetResourceDX12(brickAABBs.buffer.get(), ffxGetResourceDescriptionDX12(brickAABBs.buffer.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

	for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i) {
		updateDesc.resources.cascadeResources[i].aabbTree = ffxGetResourceDX12(cascadeAABBTrees[i].buffer.get(), ffxGetResourceDescriptionDX12(cascadeAABBTrees[i].buffer.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		updateDesc.resources.cascadeResources[i].brickMap = ffxGetResourceDX12(cascadeBrickMaps[i].buffer.get(), ffxGetResourceDescriptionDX12(cascadeBrickMaps[i].buffer.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	FfxErrorCode error = ffxBrixelizerBakeUpdate(&brixelizerContext, &updateDesc, &bakedUpdateDesc);
	if (error != FFX_OK)
		logger::critical("error");

	if (scratchBufferSize >= GPU_SCRATCH_BUFFER_SIZE)
		logger::critical("Required Brixelizer scratch memory size larger than available GPU buffer.");

	FfxResource ffxGpuScratchBuffer = ffxGetResourceDX12(gpuScratchBuffer.get(), ffxGetResourceDescriptionDX12(gpuScratchBuffer.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	ffxGpuScratchBuffer.description.stride = sizeof(uint32_t);

	// Call frame update
	error = ffxBrixelizerUpdate(&brixelizerContext, &bakedUpdateDesc, ffxGpuScratchBuffer, ffxGetCommandListDX12(cmdList));
	if (error != FFX_OK)
		logger::critical("error");

	// Transition all resources to the Non-Pixel Shader Resource state after the Brixelizer
	TransitionResources(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	visibleStateValue = !visibleStateValue;
}