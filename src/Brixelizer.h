#pragma once

#include <shared_mutex>
#include <unordered_set>

#include <d3d12.h>
#include <eastl/set.h>

#include <d3d11_4.h>

#include <FidelityFX/host/backends/dx12/ffx_dx12.h>

#include <FidelityFX/host/ffx_fsr3.h>
#include <FidelityFX/host/ffx_interface.h>

#include <FidelityFX/host/backends/dx12/d3dx12.h>
#include <FidelityFX/host/ffx_brixelizer.h>
#include <FidelityFX/host/ffx_brixelizergi.h>

#include "Buffer.h"
#include "State.h"

interface DECLSPEC_UUID("9f251514-9d4d-4902-9d60-18988ab7d4b5") DECLSPEC_NOVTABLE

	IDXGraphicsAnalysis : public IUnknown
{
	STDMETHOD_(void, BeginCapture)
	() PURE;

	STDMETHOD_(void, EndCapture)
	() PURE;
};

// Brixelizer supports a maximum of 24 raw cascades
// In the sample each cascade level we build is created by building a static cascade,
// a dynamic cascade, and then merging those into a merged cascade. Hence we require
// 3 raw cascades per cascade level.
#define NUM_BRIXELIZER_CASCADES (FFX_BRIXELIZER_MAX_CASCADES / 3)
// Brixelizer makes use of a scratch buffer for calculating cascade updates. Hence in
// this sample we allocate a buffer to be used as scratch space. Here we have chosen
// a somewhat arbitrary large size for use as scratch space, in a real application this
// value should be tuned to what is required by Brixelizer.
constexpr UINT64 GPU_SCRATCH_BUFFER_SIZE = 1ull << 30;

class Brixelizer
{
public:
	static Brixelizer* GetSingleton()
	{
		static Brixelizer singleton;
		return &singleton;
	}

	bool update = true;

	struct FrameBuffer
	{
		Matrix CameraView;
		Matrix CameraProj;
		Matrix CameraViewProj;
		Matrix CameraViewProjUnjittered;
		Matrix CameraPreviousViewProjUnjittered;
		Matrix CameraProjUnjittered;
		Matrix CameraProjUnjitteredInverse;
		Matrix CameraViewInverse;
		Matrix CameraViewProjInverse;
		Matrix CameraProjInverse;
		float4 CameraPosAdjust;
		float4 CameraPreviousPosAdjust;
		float4 FrameParams;
		float4 DynamicResolutionParams1;
		float4 DynamicResolutionParams2;
	};

	D3D11_MAPPED_SUBRESOURCE* mappedFrameBuffer = nullptr;
	FrameBuffer frameBufferCached{};

	void CacheFramebuffer();

	winrt::com_ptr<IDXGraphicsAnalysis> ga;
	bool debugAvailable = false;
	bool debugCapture = false;

	winrt::com_ptr<ID3D12Device> d3d12Device;
	winrt::com_ptr<ID3D12CommandQueue> commandQueue;
	winrt::com_ptr<ID3D12CommandAllocator> commandAllocator;
	winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
	ID3D12CommandList* commandLists[1];

	winrt::com_ptr<ID3D11Device5> d3d11Device;
	winrt::com_ptr<ID3D11DeviceContext4> d3d11Context;
	winrt::com_ptr<ID3D11Fence> d3d11Fence;
	winrt::com_ptr<ID3D12Fence> d3d12Fence;
	UINT64 currentSharedFenceValue = 0;

	winrt::com_ptr<ID3D12Fence> d3d12OnlyFence;
	UINT64 d3d12FenceValue = 0;
	HANDLE fenceEvent = nullptr;

	struct WrappedResource
	{
		ID3D11Texture2D* resource11;
		ID3D11ShaderResourceView* srv;
		ID3D11UnorderedAccessView* uav;
		winrt::com_ptr<ID3D12Resource> resource;
	};

	struct WrappedResource3D
	{
		ID3D11Texture3D* resource11;
		ID3D11ShaderResourceView* srv;
		ID3D11UnorderedAccessView* uav;
		winrt::com_ptr<ID3D12Resource> resource;
	};

	struct WrappedBuffer
	{
		ID3D11Buffer* buffer11;
		winrt::com_ptr<ID3D12Resource> buffer;
	};

	void DrawSettings();

	void InitD3D12(IDXGIAdapter* a_adapter);
	void InitBrixelizer();

	static void CreatedWrappedResource(D3D11_TEXTURE2D_DESC a_texDesc, Brixelizer::WrappedResource& a_resource);
	static void CreatedWrappedResource3D(D3D12_RESOURCE_DESC a_texDesc, Brixelizer::WrappedResource3D& a_resource);

	void OpenSharedHandles();

	void ClearShaderCache();

	void InitializeSharedFence();

	ID3D12GraphicsCommandList* BeginCommandList(bool waitForFence);

	void EndCommandList();

	void FrameUpdate();

	void PostFrameUpdate();

	struct RenderTargetDataD3D12
	{
		// D3D12 Resource
		winrt::com_ptr<ID3D12Resource> d3d12Resource;
	};

	RenderTargetDataD3D12 renderTargetsD3D12[RE::RENDER_TARGET::kTOTAL];
	RenderTargetDataD3D12 renderTargetsCubemapD3D12[RE::RENDER_TARGETS_CUBEMAP::kTOTAL];

	RenderTargetDataD3D12 ConvertD3D11TextureToD3D12(RE::BSGraphics::RenderTargetData* rtData);

	struct Hooks
	{
		struct ID3D11DeviceContext_Map
		{
			static HRESULT thunk(ID3D11DeviceContext* This, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
			{
				HRESULT hr = func(This, pResource, Subresource, MapType, MapFlags, pMappedResource);

				static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };

				if (*perFrame.get() == pResource)
					GetSingleton()->mappedFrameBuffer = pMappedResource;

				return hr;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ID3D11DeviceContext_Unmap
		{
			static void thunk(ID3D11DeviceContext* This, ID3D11Resource* pResource, UINT Subresource)
			{
				static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };

				if (*perFrame.get() == pResource && GetSingleton()->mappedFrameBuffer)
					GetSingleton()->CacheFramebuffer();

				func(This, pResource, Subresource);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void SetMiscFlags(RE::RENDER_TARGET a_targetIndex, D3D11_TEXTURE2D_DESC* a_pDesc)
		{
			a_pDesc->MiscFlags = 0;  // Original code we wrote over

			if (a_targetIndex == RE::RENDER_TARGET::kMOTION_VECTOR ||
				a_targetIndex == RE::RENDER_TARGET::kNORMAL_TAAMASK_SSRMASK ||
				a_targetIndex == RE::RENDER_TARGET::kNORMAL_TAAMASK_SSRMASK_SWAP ||
				a_targetIndex == RE::RENDER_TARGET::kMAIN) {
				a_pDesc->MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
			}
		}

		static void PatchCreateRenderTarget()
		{
			static REL::Relocation<uintptr_t> func{ REL::VariantID(75467, 77253, 0xDBC440) };  // D6A870, DA6200, DBC440

			uint8_t patchNop7[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

			auto& trampoline = SKSE::GetTrampoline();

			struct Patch : Xbyak::CodeGenerator
			{
				explicit Patch(uintptr_t a_funcAddr)
				{
					Xbyak::Label originalLabel;

					// original code we wrote over
					if (REL::Module::IsAE()) {
						mov(ptr[rbp - 0xC], esi);
					} else {
						mov(ptr[rbp - 0x10], eax);
					}

					// push all volatile to be safe
					push(rax);
					push(rcx);
					push(rdx);
					push(r8);
					push(r9);
					push(r10);
					push(r11);

					// scratch space
					sub(rsp, 0x20);

					// call our function
					if (REL::Module::IsAE()) {
						mov(rcx, edx);  // target index
					} else {
						mov(rcx, r12d);  // target index
					}
					lea(rdx, ptr[rbp - 0x30]);  // D3D11_TEXTURE2D_DESC*
					mov(rax, a_funcAddr);
					call(rax);

					add(rsp, 0x20);

					pop(r11);
					pop(r10);
					pop(r9);
					pop(r8);
					pop(rdx);
					pop(rcx);
					pop(rax);

					jmp(ptr[rip + originalLabel]);

					L(originalLabel);
					if (REL::Module::IsAE()) {
						dq(func.address() + 0x8B);
					} else if (REL::Module::IsVR()) {
						dq(func.address() + 0x9E);
					} else {
						dq(func.address() + 0x9F);
					}
				}
			};

			Patch patch(reinterpret_cast<uintptr_t>(SetMiscFlags));
			patch.ready();
			SKSE::AllocTrampoline(8 + patch.getSize());
			if (REL::Module::IsAE()) {  // AE code is 6 bytes anyway
				REL::safe_write<uint8_t>(func.address() + REL::VariantOffset(0x98, 0x85, 0x97).offset(), patchNop7);
			}
			trampoline.write_branch<6>(func.address() + REL::VariantOffset(0x98, 0x85, 0x97).offset(), trampoline.allocate(patch));
		}

		static void Install()
		{
			PatchCreateRenderTarget();

			//static REL::Relocation<uintptr_t> func{ REL::VariantID(75471, 77257, 0xDBCCD0) };  // D6B0C0, DA69B0, DBCCD0
			//auto offset = REL::VariantOffset(0x7B, 0x7B, 0x73);
			//REL::safe_write<uint8_t>(func.address() + offset.offset(), 0x8);  // add the D3D11_RESOURCE_MISC_SHARED_NTHANDLE flag

			logger::info("[Brixelizer] Installed hooks");
		}
	};
};
