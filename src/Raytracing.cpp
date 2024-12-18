#include "Raytracing.h"

#include "Deferred.h"

void Raytracing::CacheFramebuffer()
{
	auto frameBuffer = (FrameBuffer*)mappedFrameBuffer->pData;
	frameBufferCached = *frameBuffer;
	mappedFrameBuffer = nullptr;
}

void Raytracing::DrawSettings()
{
	ImGui::Text("Debug capture requires that PIX is attached.");
	if (ImGui::Button("Take Debug Capture") && !debugCapture) {
		debugCapture = true;
	}

	ImGui::SliderInt("Start Cascade", &m_StartCascadeIdx, 0, NUM_BRIXELIZER_CASCADES - 1);
	ImGui::SliderInt("End Cascade", &m_EndCascadeIdx, 0, NUM_BRIXELIZER_CASCADES - 1);

	ImGui::SliderFloat("SDF Solve Epsilon", &m_SdfSolveEps, 1e-6f, 1.0f);

	ImGui::Checkbox("Show Static Instance AABBs", &m_ShowStaticInstanceAABBs);
	ImGui::Checkbox("Show Dynamic Instance AABBs", &m_ShowDynamicInstanceAABBs);
	ImGui::Checkbox("Show Cascade AABBs", &m_ShowCascadeAABBs);
	ImGui::SliderInt("Show AABB Tree Index", &m_ShowAABBTreeIndex, -1, NUM_BRIXELIZER_CASCADES - 1);
}

void Raytracing::InitD3D12(IDXGIAdapter* a_adapter)
{
	DX::ThrowIfFailed(D3D12CreateDevice(a_adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&d3d12Device)));

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	DX::ThrowIfFailed(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

	DX::ThrowIfFailed(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

	DX::ThrowIfFailed(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.get(), nullptr, IID_PPV_ARGS(&commandList)));
	DX::ThrowIfFailed(commandList->Close());

	InitFenceAndEvent();

	debugAvailable = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&ga)) == S_OK;
}

// Helper function to create a committed resource
winrt::com_ptr<ID3D12Resource> Raytracing::CreateBuffer(UINT64 size, D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	winrt::com_ptr<ID3D12Resource> buffer;
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

	DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)));

	return buffer;
}

void Raytracing::InitBrixelizer()
{
	auto ffxDevice = ffxGetDeviceDX12(d3d12Device.get());

	size_t scratchBufferSize = ffxGetScratchMemorySizeDX12(2);
	void* scratchBuffer = calloc(scratchBufferSize, 1);
	memset(scratchBuffer, 0, scratchBufferSize);

	if (ffxGetInterfaceDX12(&initializationParameters.backendInterface, ffxDevice, scratchBuffer, scratchBufferSize, FFX_FSR3UPSCALER_CONTEXT_COUNT) != FFX_OK)
		logger::critical("[Raytracing] Failed to initialize Brixelizer backend interface!");

	initializationParameters.sdfCenter[0] = 0.0f;
	initializationParameters.sdfCenter[1] = 0.0f;
	initializationParameters.sdfCenter[2] = 0.0f;
	initializationParameters.flags = FFX_BRIXELIZER_CONTEXT_FLAG_ALL_DEBUG;
	initializationParameters.numCascades = NUM_BRIXELIZER_CASCADES;

	float voxelSize = 0.2f;
	for (uint32_t i = 0; i < NUM_BRIXELIZER_CASCADES; ++i) {
		FfxBrixelizerCascadeDescription* cascadeDesc = &initializationParameters.cascadeDescs[i];
		cascadeDesc->flags = FFX_BRIXELIZER_CASCADE_DYNAMIC;
		cascadeDesc->voxelSize = voxelSize;
		voxelSize *= 2.0f;
	}

	FfxErrorCode error = ffxBrixelizerContextCreate(&initializationParameters, &brixelizerContext);
	if (error != FFX_OK) {
		logger::critical("[Raytracing] Failed to initialize Brixelizer context!");
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

	{
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&sdfAtlasDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&sdfAtlas)));
	}

	brickAABBs = CreateBuffer(FFX_BRIXELIZER_BRICK_AABBS_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	gpuScratchBuffer = CreateBuffer(GPU_SCRATCH_BUFFER_SIZE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	for (int i = 0; i < NUM_BRIXELIZER_CASCADES; i++) {
		cascadeAABBTrees.push_back(CreateBuffer(FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
		cascadeBrickMaps.push_back(CreateBuffer(FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	}

	{
		auto manager = RE::BSGraphics::Renderer::GetSingleton();
		auto d3d11Device = reinterpret_cast<ID3D11Device*>(manager->GetRuntimeData().forwarder);

		D3D11_TEXTURE2D_DESC textureDesc;
		textureDesc.Width = 1920;
		textureDesc.Height = 1080;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

		DX::ThrowIfFailed(d3d11Device->CreateTexture2D(&textureDesc, nullptr, &debugRenderTargetd3d11));

		IDXGIResource1* dxgiResource = nullptr;
		DX::ThrowIfFailed(debugRenderTargetd3d11->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

		HANDLE sharedHandle = nullptr;
		DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(
			nullptr,
			DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
			nullptr,
			&sharedHandle));

		DX::ThrowIfFailed(d3d12Device->OpenSharedHandle(
			sharedHandle,
			IID_PPV_ARGS(&debugRenderTarget)));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		DX::ThrowIfFailed(d3d11Device->CreateShaderResourceView(debugRenderTargetd3d11, &srvDesc, &debugSRV));
	}
}

// Function to check for shared NT handle support and convert to D3D12 resource
Raytracing::RenderTargetDataD3D12 Raytracing::ConvertD3D11TextureToD3D12(RE::BSGraphics::RenderTargetData* rtData)
{
	RenderTargetDataD3D12 renderTargetData{};

	if (!rtData->texture)
		return renderTargetData;

	D3D11_TEXTURE2D_DESC texDesc{};
	rtData->texture->GetDesc(&texDesc);

	if (!(texDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE))
		return renderTargetData;

	// Query the DXGIResource1 interface to access shared NT handle
	winrt::com_ptr<IDXGIResource1> dxgiResource1;
	DX::ThrowIfFailed(rtData->texture->QueryInterface(IID_PPV_ARGS(&dxgiResource1)));

	// Create the shared NT handle
	HANDLE sharedNtHandle = nullptr;
	DX::ThrowIfFailed(dxgiResource1->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &sharedNtHandle));

	// Open the shared handle in D3D12
	winrt::com_ptr<ID3D12Resource> d3d12Resource;
	DX::ThrowIfFailed(d3d12Device->OpenSharedHandle(sharedNtHandle, IID_PPV_ARGS(&d3d12Resource)));
	CloseHandle(sharedNtHandle);  // Close the handle after opening it in D3D12

	return renderTargetData;
}

void Raytracing::OpenSharedHandles()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	for (int i = 0; i < RE::RENDER_TARGET::kTOTAL; i++) {
		renderTargetsD3D12[i] = ConvertD3D11TextureToD3D12(&renderer->GetRuntimeData().renderTargets[i]);
	}
}

uint Raytracing::GetBufferIndex(BufferData& a_bufferData)
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

struct VertexPosition
{
	float data[3];
};

void Raytracing::UpdateGeometry(RE::BSGeometry* a_geometry)
{
	if (Deferred::GetSingleton()->inWorld) {
		auto it = geometries.find(a_geometry);
		if (it == geometries.end()) {
			geometries.insert(a_geometry);

			const auto& transform = a_geometry->world;
			const auto modelBound = a_geometry->GetModelData().modelBound;
			const RE::NiPoint3 c = { modelBound.center.x, modelBound.center.y, modelBound.center.z };
			const RE::NiPoint3 r = { modelBound.radius, modelBound.radius, modelBound.radius };
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

			auto triShape = a_geometry->AsTriShape();
			if (!triShape)
				return;

			BufferData* vertexBuffer;
			BufferData* indexBuffer;

			{
				auto it2 = vertexBuffers.find((ID3D11Buffer*)rendererData->vertexBuffer);
				if (it2 == vertexBuffers.end()) {
					uint32_t vertexSize = rendererData->vertexDesc.GetSize();
					uint32_t offset = rendererData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

					std::vector<VertexPosition> convertedData;

					for (int v = 0; v < triShape->GetTrishapeRuntimeData().vertexCount; v++) {
						VertexPosition* vertex = reinterpret_cast<VertexPosition*>(&rendererData->rawVertexData[vertexSize * v + offset]);
						convertedData.push_back(*vertex);
					}
					
					BufferData data;

					// Allocate the memory on the CPU and GPU
					CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(convertedData.size(), D3D12_RESOURCE_FLAG_NONE);
					{
						CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
						DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
							&heapProperties,
							D3D12_HEAP_FLAG_NONE,
							&bufferDesc,
							D3D12_RESOURCE_STATE_COPY_DEST,
							nullptr,
							IID_PPV_ARGS(&data.buffer)));
					}

					// Create temporary upload buffer
					winrt::com_ptr<ID3D12Resource> uploadBuffer;
					{
						CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
						DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
							&heapProperties,
							D3D12_HEAP_FLAG_NONE,
							&bufferDesc,
							D3D12_RESOURCE_STATE_GENERIC_READ,
							nullptr,
							IID_PPV_ARGS(&uploadBuffer)));
					}

					// Map the buffer to CPU memory and copy the vertex data
					{
						void* pVertexData = nullptr;
						DX::ThrowIfFailed(uploadBuffer->Map(0, nullptr, &pVertexData));
						memcpy(pVertexData, convertedData.data(), convertedData.size());
						uploadBuffer->Unmap(0, nullptr);
					}

					{
						// Transition the upload buffer to the source of a copy operation
						CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
							uploadBuffer.get(),
							D3D12_RESOURCE_STATE_GENERIC_READ,
							D3D12_RESOURCE_STATE_COPY_SOURCE);
						commandList->ResourceBarrier(1, &resourceBarrier);
					}

					// Record the copy command.
					commandList->CopyResource(data.buffer.get(), uploadBuffer.get());

					{
						// Transition the buffer to a state suitable for shader access.
						CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
							data.buffer.get(),
							D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
						commandList->ResourceBarrier(1, &resourceBarrier);
					}
					

					vertexBuffers.insert({ (ID3D11Buffer*)rendererData->vertexBuffer, data });
					it2 = vertexBuffers.find((ID3D11Buffer*)rendererData->vertexBuffer);
					vertexBuffer = &it2->second;
				} else {
					vertexBuffer = &it2->second;
				}
			}

			{
				auto it2 = indexBuffers.find((ID3D11Buffer*)rendererData->indexBuffer);
				if (it2 == indexBuffers.end())
					return;
				indexBuffer = &it2->second;
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
			instanceDesc.triangleCount = triShape->GetTrishapeRuntimeData().triangleCount;

			instanceDesc.vertexBuffer = GetBufferIndex(*vertexBuffer);
			instanceDesc.vertexStride = rendererData->vertexDesc.GetSize();
			instanceDesc.vertexBufferOffset = 0;
			instanceDesc.vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
			instanceDesc.vertexFormat = FFX_SURFACE_FORMAT_R32G32B32_FLOAT;

			uint outInstanceID;
			instanceDesc.outInstanceID = &outInstanceID;
			instanceDesc.flags = FFX_BRIXELIZER_INSTANCE_FLAG_NONE;

			// Create instances for a given context with the ffxBrixelizerCreateInstance function.
			// Static instances are persistent across frames. Dynamic instances are discarded after a single frame.
			FfxErrorCode error = ffxBrixelizerCreateInstances(&brixelizerContext, &instanceDesc, 1);
			if (error != FFX_OK)
				logger::critical("error");
		}
	}
}

void Raytracing::RemoveGeometry(RE::BSGeometry* a_geometry)
{
	if (geometries.erase(a_geometry)) {
		// Remove Event
	}
}

Raytracing::BufferData Raytracing::AllocateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	BufferData data;

	// Allocate the memory on the CPU and GPU
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(pDesc->ByteWidth, D3D12_RESOURCE_FLAG_NONE);
	{
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&data.buffer)));
	}

	// Create temporary upload buffer
	winrt::com_ptr<ID3D12Resource> uploadBuffer;
	{
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		DX::ThrowIfFailed(d3d12Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));
	}

	// Map the buffer to CPU memory and copy the vertex data
	{
		void* pVertexData = nullptr;
		DX::ThrowIfFailed(uploadBuffer->Map(0, nullptr, &pVertexData));
		memcpy(pVertexData, pInitialData->pSysMem, pDesc->ByteWidth);
		uploadBuffer->Unmap(0, nullptr);
	}

	{
		// Transition the upload buffer to the source of a copy operation
		CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			uploadBuffer.get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &resourceBarrier);
	}

	// Record the copy command.
	commandList->CopyResource(data.buffer.get(), uploadBuffer.get());

	{
		// Transition the buffer to a state suitable for shader access.
		CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			data.buffer.get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &resourceBarrier);
	}

	uploadBuffer->Release();

	return data;
}

void Raytracing::RegisterIndexBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	BufferData data = AllocateBuffer(pDesc, pInitialData);
	indexBuffers.insert({ *ppBuffer, data });
}

void Raytracing::TransitionResources(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	// Transition the SDF Atlas
	barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		sdfAtlas.get(),
		stateBefore,
		stateAfter));

	// Transition the Brick AABBs
	barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		brickAABBs.get(),
		stateBefore,
		stateAfter));

	// Transition each Cascade AABB Tree
	for (const auto aabbTree : cascadeAABBTrees) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			aabbTree.get(),
			stateBefore,
			stateAfter));
	}

	// Transition each Cascade Brick Map
	for (const auto brickMap : cascadeBrickMaps) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			brickMap.get(),
			stateBefore,
			stateAfter));
	}

	// Execute the resource barriers on the command list
	commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

	barriers.clear();
}

// Initialize Fence and Event
void Raytracing::InitFenceAndEvent()
{
	HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create fence.");
	}

	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!fenceEvent) {
		throw std::runtime_error("Failed to create fence event.");
	}
}

void Raytracing::WaitForD3D11()
{
	auto state = State::GetSingleton();

	D3D11_QUERY_DESC queryDesc = { .Query = D3D11_QUERY_EVENT, .MiscFlags = 0 };
	winrt::com_ptr<ID3D11Query> query;
	DX::ThrowIfFailed(state->device->CreateQuery(&queryDesc, query.put()));

	// https://github.com/niessner/VoxelHashing/blob/master/DepthSensingCUDA/Source/GlobalAppState.cpp
	state->context->Flush();
	state->context->End(query.get());
	state->context->Flush();

	while (state->context->GetData(query.get(), nullptr, 0, 0) != S_OK) {}
}

// WaitForD3D12 Function
void Raytracing::WaitForD3D12()
{
	// Increment the fence value
	const UINT64 currentFenceValue = fenceValue;
	DX::ThrowIfFailed(commandQueue->Signal(fence.get(), currentFenceValue));
	fenceValue++;

	// Check if the fence has been reached
	if (fence->GetCompletedValue() < currentFenceValue) {
		DX::ThrowIfFailed(fence->SetEventOnCompletion(currentFenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

FfxBrixelizerDebugVisualizationDescription Raytracing::GetDebugVisualization()
{
	FfxBrixelizerDebugVisualizationDescription debugVisDesc{};

	auto cameraViewInverseAdjusted = frameBufferCached.CameraViewInverse.Transpose();
	cameraViewInverseAdjusted._41 += frameBufferCached.CameraPosAdjust.x;
	cameraViewInverseAdjusted._42 += frameBufferCached.CameraPosAdjust.y;
	cameraViewInverseAdjusted._43 += frameBufferCached.CameraPosAdjust.z;
	auto cameraProjInverse = frameBufferCached.CameraProjInverse.Transpose();

	memcpy(&debugVisDesc.inverseViewMatrix, &cameraViewInverseAdjusted, sizeof(debugVisDesc.inverseViewMatrix));
	memcpy(&debugVisDesc.inverseProjectionMatrix, &cameraProjInverse, sizeof(debugVisDesc.inverseProjectionMatrix));

	debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_GRAD;

	uint32_t cascadeIndexOffset = 0;

	debugVisDesc.startCascadeIndex = cascadeIndexOffset + m_StartCascadeIdx;
	debugVisDesc.endCascadeIndex = cascadeIndexOffset + m_EndCascadeIdx;

	debugVisDesc.tMin = m_TMin;
	debugVisDesc.tMax = m_TMax;
	debugVisDesc.sdfSolveEps = m_SdfSolveEps;
	debugVisDesc.renderWidth = 1920;
	debugVisDesc.renderHeight = 1080;
	debugVisDesc.output = ffxGetResourceDX12(debugRenderTarget, ffxGetResourceDescriptionDX12(debugRenderTarget, FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

	return debugVisDesc;
}

void Raytracing::PopulateCommandList()
{
	DX::ThrowIfFailed(commandAllocator->Reset());
	DX::ThrowIfFailed(commandList->Reset(commandAllocator.get(), nullptr));

	// Transition all resources to resource state expected by Brixelizer
	TransitionResources(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	FfxBrixelizerDebugVisualizationDescription debugVisDesc = GetDebugVisualization();

	// update desc
	auto eyePosition = Util::GetEyePosition(0);
	size_t scratchBufferSize = 0;

	FfxBrixelizerPopulateDebugAABBsFlags populateDebugAABBFlags = FFX_BRIXELIZER_POPULATE_AABBS_NONE;
	if (m_ShowStaticInstanceAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES);
	if (m_ShowDynamicInstanceAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_DYNAMIC_INSTANCES);
	if (m_ShowCascadeAABBs)
		populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_CASCADE_AABBS);

	FfxBrixelizerUpdateDescription updateDesc = {
		.frameIndex = RE::BSGraphics::State::GetSingleton()->frameCount,
		.sdfCenter = { eyePosition.x, eyePosition.y, eyePosition.z },
		.populateDebugAABBsFlags = populateDebugAABBFlags,
		.debugVisualizationDesc = &debugVisDesc,
		.maxReferences = 32 * (1 << 20),
		.triangleSwapSize = 300 * (1 << 20),
		.maxBricksPerBake = 1 << 14,
		.outScratchBufferSize = &scratchBufferSize,
		.outStats = &stats,
	};

	// Fill out the Brixelizer update description resources.
	// Pass in the externally created output resources as FfxResource objects.
	updateDesc.resources.sdfAtlas = ffxGetResourceDX12(sdfAtlas.get(), ffxGetResourceDescriptionDX12(sdfAtlas.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	updateDesc.resources.brickAABBs = ffxGetResourceDX12(brickAABBs.get(), ffxGetResourceDescriptionDX12(brickAABBs.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	for (uint32_t i = 0; i < NUM_BRIXELIZER_CASCADES; ++i) {
		updateDesc.resources.cascadeResources[i].aabbTree = ffxGetResourceDX12(cascadeAABBTrees[i].get(), ffxGetResourceDescriptionDX12(cascadeAABBTrees[i].get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		updateDesc.resources.cascadeResources[i].brickMap = ffxGetResourceDX12(cascadeBrickMaps[i].get(), ffxGetResourceDescriptionDX12(cascadeBrickMaps[i].get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	FfxErrorCode error = ffxBrixelizerBakeUpdate(&brixelizerContext, &updateDesc, &bakedUpdateDesc);
	if (error != FFX_OK)
		logger::critical("error");

	FfxResource ffxGpuScratchBuffer = ffxGetResourceDX12(gpuScratchBuffer.get(), ffxGetResourceDescriptionDX12(gpuScratchBuffer.get(), FFX_RESOURCE_USAGE_UAV), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	ffxGpuScratchBuffer.description.stride = sizeof(uint32_t);

	// Call frame update
	error = ffxBrixelizerUpdate(&brixelizerContext, &bakedUpdateDesc, ffxGpuScratchBuffer, ffxGetCommandListDX12(commandList.get()));
	if (error != FFX_OK)
		logger::critical("error");

	// Transition all resources to the Non-Pixel Shader Resource state after the Brixelizer
	TransitionResources(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	DX::ThrowIfFailed(commandList->Close());
}

void Raytracing::FrameUpdate()
{
	WaitForD3D11();

	if (debugAvailable && debugCapture)
		ga->BeginCapture();

	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { commandList.get() };
	commandQueue->ExecuteCommandLists(1, ppCommandLists);

	if (debugAvailable && debugCapture)
		ga->EndCapture();

	// Wait for the GPU to finish executing the commands
	WaitForD3D12();

	debugCapture = false;
}
