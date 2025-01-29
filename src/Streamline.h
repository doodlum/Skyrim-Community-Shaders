#pragma once

#include "Buffer.h"
#include "State.h"

#define NV_WINDOWS
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>
#include <sl_dlss_g.h>
#include <sl_matrix_helpers.h>
#include <sl_reflex.h>

#pragma warning(push)
#pragma warning(disable: 4244)
#include "latencyflex.h"
#pragma warning(pop)

struct PresentInfo
{
	ID3D11DeviceContext* context = nullptr;
	ID3D11Query* fenceQuery = nullptr;
	uint64_t frame_id = 0;
};

class FenceWaitThread
{
public:
	FenceWaitThread();

	~FenceWaitThread();

	void Push(PresentInfo&& info)
	{
		std::scoped_lock l(local_lock_);
		queue_.push_back(info);
		notify_.notify_all();
	}

private:
	void Worker();

	std::thread thread_;
	std::mutex local_lock_;
	std::condition_variable notify_;
	std::deque<PresentInfo> queue_;
	bool running_ = true;
};

class Streamline
{
public:
	static Streamline* GetSingleton()
	{
		static Streamline singleton;
		return &singleton;
	}

	inline std::string GetShortName() { return "Streamline"; }

	bool enabledAtBoot = false;
	bool initialized = false;

	bool featureDLSS = false;
	bool featureDLSSG = false;
	bool featureReflex = false;

	int latencyFlexMode = 1;
	int frameLimitMode = 60;
	int frameLimit = 60;

	sl::ViewportHandle viewport{ 0 };
	sl::FrameToken* frameToken;

	sl::DLSSGMode frameGenerationMode = sl::DLSSGMode::eOn;

	HMODULE interposer = NULL;

	//std::map<void*, std::unique_ptr<FenceWaitThread>> wait_threads;

	// SL Interposer Functions
	PFun_slInit* slInit{};
	PFun_slShutdown* slShutdown{};
	PFun_slIsFeatureSupported* slIsFeatureSupported{};
	PFun_slIsFeatureLoaded* slIsFeatureLoaded{};
	PFun_slSetFeatureLoaded* slSetFeatureLoaded{};
	PFun_slEvaluateFeature* slEvaluateFeature{};
	PFun_slAllocateResources* slAllocateResources{};
	PFun_slFreeResources* slFreeResources{};
	PFun_slSetTag* slSetTag{};
	PFun_slGetFeatureRequirements* slGetFeatureRequirements{};
	PFun_slGetFeatureVersion* slGetFeatureVersion{};
	PFun_slUpgradeInterface* slUpgradeInterface{};
	PFun_slSetConstants* slSetConstants{};
	PFun_slGetNativeInterface* slGetNativeInterface{};
	PFun_slGetFeatureFunction* slGetFeatureFunction{};
	PFun_slGetNewFrameToken* slGetNewFrameToken{};
	PFun_slSetD3DDevice* slSetD3DDevice{};

	// DLSS specific functions
	PFun_slDLSSGetOptimalSettings* slDLSSGetOptimalSettings{};
	PFun_slDLSSGetState* slDLSSGetState{};
	PFun_slDLSSSetOptions* slDLSSSetOptions{};

	// DLSSG specific functions
	PFun_slDLSSGGetState* slDLSSGGetState{};
	PFun_slDLSSGSetOptions* slDLSSGSetOptions{};

	// Reflex specific functions
	PFun_slReflexGetState* slReflexGetState{};
	PFun_slReflexSetMarker* slReflexSetMarker{};
	PFun_slReflexSleep* slReflexSleep{};
	PFun_slReflexSetOptions* slReflexSetOptions{};

	Texture2D* colorBufferShared;
	Texture2D* depthBufferShared;

	ID3D11ComputeShader* copyDepthToSharedBufferCS;

	lfx::LatencyFleX manager;
	std::mutex global_lock;
	FenceWaitThread* waitThread = nullptr;

	void BeginFrame();
	void EndFrame();

	void DrawSettings();

	void LoadInterposer();
	void Initialize();
	void PostDevice(ID3D11Device* device);

	HRESULT CreateDXGIFactory(REFIID riid, void** ppFactory);

	HRESULT CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter,
		D3D_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		const D3D_FEATURE_LEVEL* pFeatureLevels,
		UINT FeatureLevels,
		UINT SDKVersion,
		const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		IDXGISwapChain** ppSwapChain,
		ID3D11Device** ppDevice,
		D3D_FEATURE_LEVEL* pFeatureLevel,
		ID3D11DeviceContext** ppImmediateContext);

	void SetupResources();

	void CopyResourcesToSharedBuffers();
	void Present();

	void Upscale(Texture2D* a_color, Texture2D* a_alphaMask, sl::DLSSPreset a_preset);
	void UpdateConstants();

	void DestroyDLSSResources();

	struct Main_RenderWorld
	{
		static void thunk(bool a1)
		{
			auto state = State::GetSingleton();
			if (!state->isVR || !state->upscalerLoaded) {
				// With upscaler, VR hangs on this function, specifically at slSetConstants
				GetSingleton()->UpdateConstants();
			}
			func(a1);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct MenuManagerDrawInterfaceStartHook
	{
		static void thunk(int64_t a1)
		{
			GetSingleton()->CopyResourcesToSharedBuffers();
			func(a1);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_Update_Start
	{
		static void thunk(INT64 a_unk)
		{
			GetSingleton()->BeginFrame();
			func(a_unk);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	static void InstallHooks()
	{
		stl::write_thunk_call<Main_RenderWorld>(REL::RelocationID(35560, 36559).address() + REL::Relocate(0x831, 0x841, 0x791));
		stl::write_thunk_call<MenuManagerDrawInterfaceStartHook>(REL::RelocationID(79947, 82084).address() + REL::Relocate(0x7E, 0x83, 0x97));
		stl::write_thunk_call<Main_Update_Start>(REL::RelocationID(35565, 36564).address() + REL::Relocate(0x1E, 0x3E, 0x33));
	}
};
