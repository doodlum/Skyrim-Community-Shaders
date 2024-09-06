#include "Hooks.h"

#include <detours/Detours.h>

#include "Menu.h"
#include "ShaderCache.h"
#include "State.h"
#include "TruePBR.h"
#include "Util.h"

#include "ShaderTools/BSShaderHooks.h"

std::unordered_map<void*, std::pair<std::unique_ptr<uint8_t[]>, size_t>> ShaderBytecodeMap;

void RegisterShaderBytecode(void* Shader, const void* Bytecode, size_t BytecodeLength)
{
	// Grab a copy since the pointer isn't going to be valid forever
	auto codeCopy = std::make_unique<uint8_t[]>(BytecodeLength);
	memcpy(codeCopy.get(), Bytecode, BytecodeLength);
	logger::debug(fmt::runtime("Saving shader at index {:x} with {} bytes:\t{:x}"), (std::uintptr_t)Shader, BytecodeLength, (std::uintptr_t)Bytecode);
	ShaderBytecodeMap.emplace(Shader, std::make_pair(std::move(codeCopy), BytecodeLength));
}

const std::pair<std::unique_ptr<uint8_t[]>, size_t>& GetShaderBytecode(void* Shader)
{
	logger::debug(fmt::runtime("Loading shader at index {:x}"), (std::uintptr_t)Shader);
	return ShaderBytecodeMap.at(Shader);
}

template <class ShaderType>
void DumpShader(const REX::BSShader* thisClass, const ShaderType* shader, const std::pair<std::unique_ptr<uint8_t[]>, size_t>& bytecode)
{
	static_assert(std::is_same_v<ShaderType, RE::BSGraphics::VertexShader> || std::is_same_v<ShaderType, RE::BSGraphics::PixelShader>);

	uint8_t* dxbcData = new uint8_t[bytecode.second];
	size_t dxbcLen = bytecode.second;
	memcpy(dxbcData, bytecode.first.get(), bytecode.second);

	constexpr auto shaderExtStr = std::is_same_v<ShaderType, RE::BSGraphics::VertexShader> ? "vs" : "ps";
	constexpr auto shaderTypeStr = std::is_same_v<ShaderType, RE::BSGraphics::VertexShader> ? "vertex" : "pixel";

	std::string dumpDir = std::format("Data\\ShaderDump\\{}\\{:X}.{}.bin", thisClass->m_LoaderType, shader->id, shaderExtStr);
	auto directoryPath = std::format("Data\\ShaderDump\\{}", thisClass->m_LoaderType);
	logger::debug(fmt::runtime("Dumping {} shader {} with id {:x} at {}"), shaderTypeStr, thisClass->m_LoaderType, shader->id, dumpDir);

	if (!std::filesystem::is_directory(directoryPath)) {
		try {
			std::filesystem::create_directories(directoryPath);
		} catch (std::filesystem::filesystem_error const& ex) {
			logger::error("Failed to create folder: {}", ex.what());
		}
	}

	if (FILE * file; fopen_s(&file, dumpDir.c_str(), "wb") == 0) {
		fwrite(dxbcData, 1, dxbcLen, file);
		fclose(file);
	}

	delete[] dxbcData;
}

void hk_BSShader_LoadShaders(RE::BSShader* shader, std::uintptr_t stream);

decltype(&hk_BSShader_LoadShaders) ptr_BSShader_LoadShaders;

void hk_BSShader_LoadShaders(RE::BSShader* shader, std::uintptr_t stream)
{
	(ptr_BSShader_LoadShaders)(shader, stream);
	auto& shaderCache = SIE::ShaderCache::Instance();

	if (shaderCache.IsDiskCache() || shaderCache.IsDump()) {
		if (shaderCache.IsDiskCache()) {
			TruePBR::GetSingleton()->GenerateShaderPermutations(shader);
		}

		for (const auto& entry : shader->vertexShaders) {
			if (entry->shader && shaderCache.IsDump()) {
				const auto& bytecode = GetShaderBytecode(entry->shader);
				DumpShader((REX::BSShader*)shader, entry, bytecode);
			}
			auto vertexShaderDesriptor = entry->id;
			auto pixelShaderDescriptor = entry->id;
			State::GetSingleton()->ModifyShaderLookup(*shader, vertexShaderDesriptor, pixelShaderDescriptor);
			shaderCache.GetVertexShader(*shader, vertexShaderDesriptor);
		}
		for (const auto& entry : shader->pixelShaders) {
			if (entry->shader && shaderCache.IsDump()) {
				const auto& bytecode = GetShaderBytecode(entry->shader);
				DumpShader((REX::BSShader*)shader, entry, bytecode);
			}
			auto vertexShaderDesriptor = entry->id;
			auto pixelShaderDescriptor = entry->id;
			State::GetSingleton()->ModifyShaderLookup(*shader, vertexShaderDesriptor, pixelShaderDescriptor);
			shaderCache.GetPixelShader(*shader, pixelShaderDescriptor);
			State::GetSingleton()->ModifyShaderLookup(*shader, vertexShaderDesriptor, pixelShaderDescriptor, true);
			shaderCache.GetPixelShader(*shader, pixelShaderDescriptor);
		}
	}
	BSShaderHooks::hk_LoadShaders((REX::BSShader*)shader, stream);
};

decltype(&Hooks::hk_BSShader_BeginTechnique) ptr_BSShader_BeginTechnique;

bool Hooks::hk_BSShader_BeginTechnique(RE::BSShader* shader, uint32_t vertexDescriptor, uint32_t pixelDescriptor, bool skipPixelShader)
{
	auto state = State::GetSingleton();

	state->updateShader = true;
	state->currentShader = shader;

	state->currentVertexDescriptor = vertexDescriptor;
	state->currentPixelDescriptor = pixelDescriptor;

	state->modifiedVertexDescriptor = vertexDescriptor;
	state->modifiedPixelDescriptor = pixelDescriptor;

	state->ModifyShaderLookup(*shader, state->modifiedVertexDescriptor, state->modifiedPixelDescriptor);

	bool shaderFound = (ptr_BSShader_BeginTechnique)(shader, vertexDescriptor, pixelDescriptor, skipPixelShader);

	if (!shaderFound) {
		auto& shaderCache = SIE::ShaderCache::Instance();
		RE::BSGraphics::VertexShader* vertexShader = shaderCache.GetVertexShader(*shader, state->modifiedVertexDescriptor);
		RE::BSGraphics::PixelShader* pixelShader = shaderCache.GetPixelShader(*shader, state->modifiedPixelDescriptor);
		if (vertexShader == nullptr || (!skipPixelShader && pixelShader == nullptr)) {
			shaderFound = false;
		} else {
			state->settingCustomShader = true;
			RE::BSGraphics::RendererShadowState::GetSingleton()->SetVertexShader(vertexShader);
			if (skipPixelShader) {
				pixelShader = nullptr;
			}
			RE::BSGraphics::RendererShadowState::GetSingleton()->SetPixelShader(pixelShader);
			state->settingCustomShader = false;
			shaderFound = true;
		}
	}

	state->lastModifiedVertexDescriptor = state->modifiedVertexDescriptor;
	state->lastModifiedPixelDescriptor = state->modifiedPixelDescriptor;

	return shaderFound;
}

decltype(&IDXGISwapChain::Present) ptr_IDXGISwapChain_Present;

HRESULT WINAPI hk_IDXGISwapChain_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
{
	State::GetSingleton()->Reset();
	Menu::GetSingleton()->DrawOverlay();
	auto retval = (This->*ptr_IDXGISwapChain_Present)(SyncInterval, Flags);
	TracyD3D11Collect(State::GetSingleton()->tracyCtx);
	return retval;
}

void hk_BSGraphics_SetDirtyStates(bool isCompute);

decltype(&hk_BSGraphics_SetDirtyStates) ptr_BSGraphics_SetDirtyStates;

void hk_BSGraphics_SetDirtyStates(bool isCompute)
{
	(ptr_BSGraphics_SetDirtyStates)(isCompute);
	State::GetSingleton()->Draw();
}

decltype(&ID3D11Device::CreateVertexShader) ptrCreateVertexShader;
decltype(&ID3D11Device::CreatePixelShader) ptrCreatePixelShader;

HRESULT hk_CreateVertexShader(ID3D11Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)
{
	HRESULT hr = (This->*ptrCreateVertexShader)(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

	if (SUCCEEDED(hr))
		RegisterShaderBytecode(*ppVertexShader, pShaderBytecode, BytecodeLength);

	return hr;
}

HRESULT STDMETHODCALLTYPE hk_CreatePixelShader(ID3D11Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)
{
	HRESULT hr = (This->*ptrCreatePixelShader)(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

	if (SUCCEEDED(hr))
		RegisterShaderBytecode(*ppPixelShader, pShaderBytecode, BytecodeLength);

	return hr;
}

decltype(&D3D11CreateDeviceAndSwapChain) ptrD3D11CreateDeviceAndSwapChain;

HRESULT WINAPI hk_D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	[[maybe_unused]] const D3D_FEATURE_LEVEL* pFeatureLevels,
	[[maybe_unused]] UINT FeatureLevels,
	UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	[[maybe_unused]] D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext)
{
	logger::info("Upgrading D3D11 feature level to 11.1");

	const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;  // Create a device with only the latest feature level

#ifndef NDEBUG
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = (*ptrD3D11CreateDeviceAndSwapChain)(
		pAdapter,
		DriverType,
		Software,
		Flags,
		&featureLevel,
		1,
		SDKVersion,
		pSwapChainDesc,
		ppSwapChain,
		ppDevice,
		nullptr,
		ppImmediateContext);

	return hr;
}

void hk_BSShaderRenderTargets_Create();

decltype(&hk_BSShaderRenderTargets_Create) ptr_BSShaderRenderTargets_Create;

void hk_BSShaderRenderTargets_Create()
{
	(ptr_BSShaderRenderTargets_Create)();
	State::GetSingleton()->Setup();
}

static void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events);
static inline REL::Relocation<decltype(hk_PollInputDevices)> _InputFunc;

void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
{
	bool blockedDevice = true;
	auto menu = Menu::GetSingleton();

	if (a_events) {
		menu->ProcessInputEvents(a_events);

		if (*a_events) {
			if (auto device = (*a_events)->GetDevice()) {
				// Check that the device is not a Gamepad or VR controller. If it is, unblock input.
				bool vrDevice = false;
#ifdef ENABLE_SKYRIM_VR
				vrDevice = (REL::Module::IsVR() && ((device == RE::INPUT_DEVICES::INPUT_DEVICE::kVivePrimary) ||
													   (device == RE::INPUT_DEVICES::INPUT_DEVICE::kViveSecondary) ||
													   (device == RE::INPUT_DEVICES::INPUT_DEVICE::kOculusPrimary) ||
													   (device == RE::INPUT_DEVICES::INPUT_DEVICE::kOculusSecondary) ||
													   (device == RE::INPUT_DEVICES::INPUT_DEVICE::kWMRPrimary) ||
													   (device == RE::INPUT_DEVICES::INPUT_DEVICE::kWMRSecondary)));
#endif
				blockedDevice = !((device == RE::INPUT_DEVICES::INPUT_DEVICE::kGamepad) || vrDevice);
			}
		}
	}

	if (blockedDevice && menu->ShouldSwallowInput()) {  //the menu is open, eat all keypresses
		constexpr RE::InputEvent* const dummy[] = { nullptr };
		_InputFunc(a_dispatcher, dummy);
		return;
	}

	_InputFunc(a_dispatcher, a_events);
}

namespace Hooks
{
	struct BSGraphics_Renderer_Init_InitD3D
	{
		static void thunk()
		{
			logger::info("Calling original Init3D");

			func();

			logger::info("Accessing render device information");

			auto manager = RE::BSGraphics::Renderer::GetSingleton();

			auto context = reinterpret_cast<ID3D11DeviceContext*>(manager->GetRuntimeData().context);
			auto swapchain = reinterpret_cast<IDXGISwapChain*>(manager->GetRuntimeData().renderWindows->swapChain);
			auto device = reinterpret_cast<ID3D11Device*>(manager->GetRuntimeData().forwarder);

			logger::info("Detouring virtual function tables");

			*(uintptr_t*)&ptr_IDXGISwapChain_Present = Detours::X64::DetourClassVTable(*(uintptr_t*)swapchain, &hk_IDXGISwapChain_Present, 8);

			auto& shaderCache = SIE::ShaderCache::Instance();
			if (shaderCache.IsDump()) {
				*(uintptr_t*)&ptrCreateVertexShader = Detours::X64::DetourClassVTable(*(uintptr_t*)device, &hk_CreateVertexShader, 12);
				*(uintptr_t*)&ptrCreatePixelShader = Detours::X64::DetourClassVTable(*(uintptr_t*)device, &hk_CreatePixelShader, 15);
			}
			Menu::GetSingleton()->Init(swapchain, device, context);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct WndProcHandler_Hook
	{
		static LRESULT thunk(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam)
		{
			if (a_msg == WM_KILLFOCUS) {
				Menu::GetSingleton()->OnFocusLost();
				auto& io = ImGui::GetIO();
				io.ClearInputKeys();
				io.ClearEventsQueue();
			}
			return func(a_hwnd, a_msg, a_wParam, a_lParam);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct RegisterClassA_Hook
	{
		static ATOM thunk(WNDCLASSA* a_wndClass)
		{
			WndProcHandler_Hook::func = reinterpret_cast<uintptr_t>(a_wndClass->lpfnWndProc);
			a_wndClass->lpfnWndProc = &WndProcHandler_Hook::thunk;

			return func(a_wndClass);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateRenderTarget_Main
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::RENDER_TARGETS::RENDER_TARGET a_target, RE::BSGraphics::RenderTargetProperties* a_properties)
		{
			State::GetSingleton()->ModifyRenderTarget(a_target, a_properties);
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateRenderTarget_Normals
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::RENDER_TARGETS::RENDER_TARGET a_target, RE::BSGraphics::RenderTargetProperties* a_properties)
		{
			State::GetSingleton()->ModifyRenderTarget(a_target, a_properties);
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateRenderTarget_NormalsSwap
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::RENDER_TARGETS::RENDER_TARGET a_target, RE::BSGraphics::RenderTargetProperties* a_properties)
		{
			State::GetSingleton()->ModifyRenderTarget(a_target, a_properties);
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateRenderTarget_Snow
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::RENDER_TARGETS::RENDER_TARGET a_target, RE::BSGraphics::RenderTargetProperties* a_properties)
		{
			State::GetSingleton()->ModifyRenderTarget(a_target, a_properties);
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSShader__BeginTechnique_SetVertexShader
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::BSGraphics::VertexShader* a_vertexShader)
		{
			func(This, a_vertexShader);  // TODO: Remove original call
			auto state = State::GetSingleton();
			if (!state->settingCustomShader) {
				auto& shaderCache = SIE::ShaderCache::Instance();
				if (shaderCache.IsEnabled()) {
					auto currentShader = state->currentShader;
					auto type = currentShader->shaderType.get();
					if (type > 0 && type < RE::BSShader::Type::Total) {
						if (state->enabledClasses[type - 1]) {
							RE::BSGraphics::VertexShader* vertexShader = shaderCache.GetVertexShader(*currentShader, state->modifiedVertexDescriptor);
							if (vertexShader) {
								state->context->VSSetShader(reinterpret_cast<ID3D11VertexShader*>(vertexShader->shader), NULL, NULL);
								auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
								GET_INSTANCE_MEMBER(currentVertexShader, shadowState)
								currentVertexShader = a_vertexShader;
								return;
							}
						}
					}
				}
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSShader__BeginTechnique_SetPixelShader
	{
		static void thunk(RE::BSGraphics::Renderer* This, RE::BSGraphics::PixelShader* a_pixelShader)
		{
			auto state = State::GetSingleton();
			if (!state->settingCustomShader) {
				auto& shaderCache = SIE::ShaderCache::Instance();
				if (shaderCache.IsEnabled()) {
					auto currentShader = state->currentShader;
					auto type = currentShader->shaderType.get();
					if (type > 0 && type < RE::BSShader::Type::Total) {
						if (state->enabledClasses[type - 1]) {
							RE::BSGraphics::PixelShader* pixelShader = shaderCache.GetPixelShader(*currentShader, state->modifiedPixelDescriptor);
							if (pixelShader) {
								state->context->PSSetShader(reinterpret_cast<ID3D11PixelShader*>(pixelShader->shader), NULL, NULL);
								auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
								GET_INSTANCE_MEMBER(currentPixelShader, shadowState)
								currentPixelShader = a_pixelShader;
								return;
							}
						}
					}
				}
			}
			func(This, a_pixelShader);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateDepthStencil_PrecipitationMask
	{
		static void thunk(RE::BSGraphics::Renderer* This, uint32_t a_target, RE::BSGraphics::DepthStencilTargetProperties* a_properties)
		{
			a_properties->use16BitsDepth = true;
			a_properties->stencil = false;
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CreateCubemapRenderTarget_Reflections
	{
		static void thunk(RE::BSGraphics::Renderer* This, uint32_t a_target, RE::BSGraphics::CubeMapRenderTargetProperties* a_properties)
		{
			a_properties->height = 128;
			a_properties->width = 128;
			func(This, a_target, a_properties);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

#ifdef TRACY_ENABLE
	struct Main_Update
	{
		static void thunk(RE::Main* a_this, float a2)
		{
			func(a_this, a2);
			FrameMark;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
#endif

	namespace CSShadersSupport
	{
		RE::BSImagespaceShader* CurrentlyDispatchedShader = nullptr;
		RE::BSComputeShader* CurrentlyDispatchedComputeShader = nullptr;
		uint32_t CurrentComputeShaderTechniqueId = 0;

		RE::BSImagespaceShader* vlGenerateShader = nullptr;
		RE::BSImagespaceShader* vlRaymarchShader = nullptr;

		RE::BSImagespaceShader* CreateVLShader(const std::string_view& name, const std::string_view& fileName, RE::BSComputeShader* computeShader)
		{
			auto shader = RE::BSImagespaceShader::Create();
			shader->shaderType = RE::BSShader::Type::ImageSpace;
			shader->fxpFilename = fileName.data();
			shader->name = name.data();
			shader->originalShaderName = fileName.data();
			shader->computeShader = computeShader;
			shader->isComputeShader = true;
			return shader;
		}

		RE::BSImagespaceShader* GetOrCreateVLGenerateShader(RE::BSComputeShader* computeShader)
		{
			if (vlGenerateShader == nullptr) {
				vlGenerateShader = CreateVLShader("BSImagespaceShaderVolumetricLightingGenerateCS", "ISVolumetricLightingGenerateCS", computeShader);
			}
			return vlGenerateShader;
		}

		RE::BSImagespaceShader* GetOrCreateVLRaymarchShader(RE::BSComputeShader* computeShader)
		{
			if (vlRaymarchShader == nullptr) {
				vlRaymarchShader = CreateVLShader("BSImagespaceShaderVolumetricLightingRaymarchCS", "ISVolumetricLightingRaymarchCS", computeShader);
			}
			return vlRaymarchShader;
		}

		void hk_BSImagespaceShader_DispatchComputeShader(RE::BSImagespaceShader* shader, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);
		decltype(&hk_BSImagespaceShader_DispatchComputeShader) ptr_BSImagespaceShader_DispatchComputeShader;
		void hk_BSImagespaceShader_DispatchComputeShader(RE::BSImagespaceShader* shader, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
		{
			CurrentlyDispatchedShader = shader;
			(ptr_BSImagespaceShader_DispatchComputeShader)(shader, threadGroupCountX, threadGroupCountY, threadGroupCountZ);
			CurrentlyDispatchedShader = nullptr;
		}

		struct BSComputeShader_Dispatch
		{
			static void thunk(RE::BSComputeShader* shader, uint32_t techniqueId, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
			{
				CurrentlyDispatchedComputeShader = shader;
				CurrentComputeShaderTechniqueId = techniqueId;
				func(shader, techniqueId, threadGroupCountX, threadGroupCountY, threadGroupCountZ);
				CurrentlyDispatchedComputeShader = nullptr;
				CurrentComputeShaderTechniqueId = 0;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void hk_Renderer_DispatchCSShader(RE::BSGraphics::Renderer* renderer, RE::BSGraphics::ComputeShader* shader, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);
		decltype(&hk_Renderer_DispatchCSShader) ptr_Renderer_DispatchCSShader;
		void hk_Renderer_DispatchCSShader(RE::BSGraphics::Renderer* renderer, RE::BSGraphics::ComputeShader* shader, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
		{
			auto state = State::GetSingleton();
			if (state->enabledClasses[RE::BSShader::Type::ImageSpace]) {
				auto& shaderCache = SIE::ShaderCache::Instance();
				RE::BSImagespaceShader* isShader = CurrentlyDispatchedShader;
				uint32_t techniqueId = CurrentComputeShaderTechniqueId;
				if (CurrentlyDispatchedShader == nullptr) {
					techniqueId = 0;
					if (CurrentlyDispatchedComputeShader->name == std::string_view("ISVolumetricLightingGenerateCS")) {
						isShader = GetOrCreateVLGenerateShader(CurrentlyDispatchedComputeShader);
					} else if (CurrentlyDispatchedComputeShader->name == std::string_view("ISVolumetricLightingRaymarchCS")) {
						isShader = GetOrCreateVLRaymarchShader(CurrentlyDispatchedComputeShader);
					}
				}
				if (isShader != nullptr) {
					if (auto* computeShader = shaderCache.GetComputeShader(*isShader, techniqueId)) {
						shader = computeShader;
					}
				}
			}
			(ptr_Renderer_DispatchCSShader)(renderer, shader, threadGroupCountX, threadGroupCountY, threadGroupCountZ);
		}
	}

	void Install()
	{
		SKSE::AllocTrampoline(14);
		auto& trampoline = SKSE::GetTrampoline();
		logger::info("Hooking BSInputDeviceManager::PollInputDevices");
		_InputFunc = trampoline.write_call<5>(REL::RelocationID(67315, 68617).address() + REL::Relocate(0x7B, 0x7B, 0x81), hk_PollInputDevices);  //BSInputDeviceManager::PollInputDevices -> Inputfunc

		logger::info("Hooking BSShader::LoadShaders");
		*(uintptr_t*)&ptr_BSShader_LoadShaders = Detours::X64::DetourFunction(REL::RelocationID(101339, 108326).address(), (uintptr_t)&hk_BSShader_LoadShaders);
		logger::info("Hooking BSShader::BeginTechnique");
		*(uintptr_t*)&ptr_BSShader_BeginTechnique = Detours::X64::DetourFunction(REL::RelocationID(101341, 108328).address(), (uintptr_t)&Hooks::hk_BSShader_BeginTechnique);

		stl::write_thunk_call<BSShader__BeginTechnique_SetVertexShader>(REL::RelocationID(101341, 108328).address() + REL::Relocate(0xC3, 0xD5));
		stl::write_thunk_call<BSShader__BeginTechnique_SetPixelShader>(REL::RelocationID(101341, 108328).address() + REL::Relocate(0xD7, 0xEB));

		logger::info("Hooking BSGraphics::SetDirtyStates");
		*(uintptr_t*)&ptr_BSGraphics_SetDirtyStates = Detours::X64::DetourFunction(REL::RelocationID(75580, 77386).address(), (uintptr_t)&hk_BSGraphics_SetDirtyStates);

		logger::info("Hooking BSGraphics::Renderer::InitD3D");
		stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));

		logger::info("Hooking WndProcHandler");
		stl::write_thunk_call_6<RegisterClassA_Hook>(REL::VariantID(75591, 77226, 0xDC4B90).address() + REL::VariantOffset(0x8E, 0x15C, 0x99).offset());

		//logger::info("Hooking D3D11CreateDeviceAndSwapChain");
		//*(FARPROC*)&ptrD3D11CreateDeviceAndSwapChain = GetProcAddress(GetModuleHandleA("d3d11.dll"), "D3D11CreateDeviceAndSwapChain");
		//SKSE::PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");

		logger::info("Hooking BSShaderRenderTargets::Create");
		*(uintptr_t*)&ptr_BSShaderRenderTargets_Create = Detours::X64::DetourFunction(REL::RelocationID(100458, 107175).address(), (uintptr_t)&hk_BSShaderRenderTargets_Create);

		logger::info("Hooking BSShaderRenderTargets::Create::CreateRenderTarget(s)");
		stl::write_thunk_call<CreateRenderTarget_Main>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0x3F0, 0x3F3, 0x548));
		stl::write_thunk_call<CreateRenderTarget_Normals>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0x458, 0x45B, 0x5B0));
		stl::write_thunk_call<CreateRenderTarget_NormalsSwap>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0x46B, 0x46E, 0x5C3));
		stl::write_thunk_call<CreateRenderTarget_Snow>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0x406, 0x409, 0x55e));
		stl::write_thunk_call<CreateDepthStencil_PrecipitationMask>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0x1245, 0x123B, 0x1917));
		stl::write_thunk_call<CreateCubemapRenderTarget_Reflections>(REL::RelocationID(100458, 107175).address() + REL::Relocate(0xA25, 0xA25, 0xCD2));

#ifdef TRACY_ENABLE
		stl::write_thunk_call<Main_Update>(REL::RelocationID(35551, 36544).address() + REL::Relocate(0x11F, 0x160));
#endif

		logger::info("Hooking BSImagespaceShader");
		*(uintptr_t*)&CSShadersSupport::ptr_BSImagespaceShader_DispatchComputeShader = Detours::X64::DetourFunction(REL::RelocationID(100952, 107734).address(), (uintptr_t)&CSShadersSupport::hk_BSImagespaceShader_DispatchComputeShader);

		logger::info("Hooking BSComputeShader");
		stl::write_vfunc<0x02, CSShadersSupport::BSComputeShader_Dispatch>(RE::VTABLE_BSComputeShader[0]);

		logger::info("Hooking Renderer::DispatchCSShader");
		*(uintptr_t*)&CSShadersSupport::ptr_Renderer_DispatchCSShader = Detours::X64::DetourFunction(REL::RelocationID(75532, 77329).address(), (uintptr_t)&CSShadersSupport::hk_Renderer_DispatchCSShader);
	}
}