#pragma once

#include <shared_mutex>
#include <unordered_set>

#include <d3d12.h>
#include <eastl/set.h>

#include "Brixelizer.h"

#include <DDSTextureLoader12.h>

class BrixelizerContext
{
public:
	static BrixelizerContext* GetSingleton()
	{
		static BrixelizerContext singleton;
		return &singleton;
	}

	FfxBrixelizerContextDescription initializationParameters = {};
	FfxBrixelizerContext brixelizerContext = {};
	FfxBrixelizerBakedUpdateDescription brixelizerBakedUpdateDesc = {};
	FfxBrixelizerStats stats = {};
	FfxBrixelizerBakedUpdateDescription bakedUpdateDesc = {};

	std::shared_mutex mutex;

	Brixelizer::WrappedResource3D sdfAtlas;
	Brixelizer::WrappedBuffer brickAABBs;
	Brixelizer::WrappedBuffer cascadeAABBTrees[FFX_BRIXELIZER_MAX_CASCADES];
	Brixelizer::WrappedBuffer cascadeBrickMaps[FFX_BRIXELIZER_MAX_CASCADES];

	winrt::com_ptr<ID3D12Resource> gpuScratchBuffer;

	Brixelizer::WrappedResource debugRenderTarget;

	FfxBrixelizerTraceDebugModes m_DebugMode = FFX_BRIXELIZER_TRACE_DEBUG_MODE_GRAD;

	int m_StartCascadeIdx = 0;
	int m_EndCascadeIdx = NUM_BRIXELIZER_CASCADES - 1;

	float m_TMin = 0.0f;
	float m_TMax = 10000.0f;
	float m_SdfSolveEps = 0.5f;

	bool m_ShowStaticInstanceAABBs = false;
	bool m_ShowDynamicInstanceAABBs = false;
	bool m_ShowCascadeAABBs = false;
	int m_ShowAABBTreeIndex = -1;

	float m_RayPushoff = 0.25f;

	winrt::com_ptr<ID3D12Resource> CreateBuffer(UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS flags);

	Brixelizer::WrappedBuffer CreateWrappedBuffer(UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS flags);

	void InitBrixelizerContext();

	struct BufferData
	{
		winrt::com_ptr<ID3D12Resource> buffer;
		bool registered = false;
		uint width;
		uint index;
	};

	eastl::hash_map<ID3D11Buffer*, BufferData> vertexBuffers;
	eastl::hash_map<ID3D11Buffer*, BufferData> indexBuffers;

	struct InputLayoutData
	{
		uint32_t vertexStride;
		uint32_t vertexBufferOffset;
		FfxSurfaceFormat vertexFormat;
	};

	eastl::hash_map<ID3D11InputLayout*, InputLayoutData> inputLayouts;
	eastl::hash_map<uint64_t, ID3D11InputLayout*> vertexDescToInputLayout;

	struct InstanceData
	{
		FfxBrixelizerInstanceID instanceID;
		bool visibleState = false;
	};

	bool visibleStateValue = true;

	eastl::hash_set<RE::BSTriShape*> queuedInstances;
	eastl::hash_map<RE::BSTriShape*, InstanceData> instances;

	BufferData AllocateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);

	void RegisterVertexBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);
	void RegisterIndexBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);
	void RegisterInputLayout(ID3D11InputLayout* ppInputLayout, D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements);

	void UnregisterVertexBuffer(ID3D11Buffer* ppBuffer);
	void UnregisterIndexBuffer(ID3D11Buffer* ppBuffer);

	uint GetBufferIndex(BufferData& a_bufferData);

	void AddInstance(RE::BSTriShape* geometry);
	void SeenInstance(RE::BSTriShape* geometry);
	void RemoveInstance(RE::BSTriShape* a_geometry);

	void TransitionResources(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	FfxBrixelizerDebugVisualizationDescription GetDebugVisualization();

	void UpdateBrixelizerContext(ID3D12GraphicsCommandList* cmdList);
	void UpdateScene();
	void BSTriShape_UpdateWorldData(RE::BSTriShape* This, RE::NiUpdateData* a_data);

	struct Hooks
	{
		struct ID3D11Device_CreateBuffer
		{
			static HRESULT thunk(ID3D11Device* This, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
			{
				HRESULT hr = func(This, pDesc, pInitialData, ppBuffer);

				if (pInitialData) {
					if (pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER) {
						GetSingleton()->RegisterVertexBuffer(pDesc, pInitialData, ppBuffer);
					} else if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER) {
						GetSingleton()->RegisterIndexBuffer(pDesc, pInitialData, ppBuffer);
					}
				}

				return hr;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ID3D11Device_CreateInputLayout
		{
			static HRESULT thunk(ID3D11Device* This, D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)
			{
				HRESULT hr = func(This, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);

				if (pInputElementDescs && NumElements > 0) {
					GetSingleton()->RegisterInputLayout(*ppInputLayout, pInputElementDescs, NumElements);
				}

				return hr;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSTriShape_UpdateWorldData
		{
			static void thunk(RE::BSTriShape* This, RE::NiUpdateData* a_data)
			{
				GetSingleton()->BSTriShape_UpdateWorldData(This, a_data);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DirtyStates_CreateInputLayoutFromVertexDesc
		{
			static ID3D11InputLayout* thunk(uint64_t a_vertexDesc)
			{
				auto inputLayout = func(a_vertexDesc);
				GetSingleton()->vertexDescToInputLayout.insert({ a_vertexDesc, inputLayout });
				return inputLayout;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			//if (REL::Module::IsAE()) {
			//	stl::write_vfunc<0x31, BSTriShape_UpdateWorldData>(RE::VTABLE_BSTriShape[0]);
			//} else {
			//	stl::write_vfunc<0x30, BSTriShape_UpdateWorldData>(RE::VTABLE_BSTriShape[0]);
			//}
			stl::write_thunk_call<DirtyStates_CreateInputLayoutFromVertexDesc>(REL::RelocationID(75580, 75580).address() + REL::Relocate(0x465, 0x465));

			logger::info("[BrixelizerContext] Installed hooks");
		}
	};
};
