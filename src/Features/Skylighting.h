#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "State.h"
#include "Util.h"

struct Skylighting : Feature
{
	static Skylighting* GetSingleton()
	{
		static Skylighting singleton;
		return &singleton;
	}

	virtual bool SupportsVR() override { return true; };

	virtual inline std::string GetName() override { return "Skylighting"; }
	virtual inline std::string GetShortName() override { return "Skylighting"; }
	virtual inline std::string_view GetShaderDefineName() override { return "SKYLIGHTING"; }
	virtual bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void RestoreDefaultSettings() override;
	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;
	void CompileComputeShaders();

	virtual void Prepass() override;

	virtual void PostPostLoad() override;

	ID3D11PixelShader* GetFoliagePS();
	void SkylightingShaderHacks();  // referenced in State.cpp

	//////////////////////////////////////////////////////////////////////////////////

	struct Settings
	{
		float MaxZenith = 3.1415926f / 4.f;  // 45 deg
		float MinDiffuseVisibility = 0.1f;
		float MinSpecularVisibility = 0.f;
		uint pad0;
	} settings;

	struct alignas(16) SkylightingCB
	{
		REX::W32::XMFLOAT4X4 OcclusionViewProj;
		float4 OcclusionDir;

		float3 PosOffset;  // cell origin in camera model space
		uint _pad0;
		uint ArrayOrigin[3];  // xyz: array origin, w: max accum frames
		uint _pad1;
		int ValidMargin[4];

		float MinDiffuseVisibility;
		float MinSpecularVisibility;
		uint _pad2[2];
	};
	static_assert(sizeof(SkylightingCB) % 16 == 0);

	SkylightingCB GetCommonBufferData();

	winrt::com_ptr<ID3D11SamplerState> pointClampSampler = nullptr;

	Texture2D* texOcclusion = nullptr;
	Texture3D* texProbeArray = nullptr;
	Texture3D* texAccumFramesArray = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> probeUpdateCompute = nullptr;

	ID3D11PixelShader* foliagePixelShader = nullptr;

	// misc parameters
	bool doOcclusion = true;
	uint probeArrayDims[3] = { 128, 128, 64 };
	float occlusionDistance = 10000.f;

	// cached variables
	bool inOcclusion = false;
	bool foliage = false;
	REX::W32::XMFLOAT4X4 OcclusionTransform;
	float4 OcclusionDir;
	uint forceFrames = 255 * 4;
	uint frameCount = 0;

	std::chrono::time_point<std::chrono::system_clock> lastUpdateTimer = std::chrono::system_clock::now();

	//////////////////////////////////////////////////////////////////////////////////

	// Hooks
	struct BSLightingShaderProperty_GetPrecipitationOcclusionMapRenderPassesImpl
	{
		static RE::BSLightingShaderProperty::Data* thunk(RE::BSLightingShaderProperty* property, RE::BSGeometry* geometry, uint32_t renderMode, RE::BSGraphics::BSShaderAccumulator* accumulator);
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_Precipitation_RenderOcclusion
	{
		static void thunk();
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSUtilityShader_SetupGeometry
	{
		static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags);
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct SetViewFrustum
	{
		static void thunk(RE::NiCamera* a_camera, RE::NiFrustum* a_frustum);
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Event handler
	class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
		{
			// When entering a new cell through a loadscreen, update every frame until completion
			if (a_event->menuName == RE::LoadingMenu::MENU_NAME) {
				if (!a_event->opening)
					Skylighting::GetSingleton()->forceFrames = 255 * 4;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		static bool Register()
		{
			static MenuOpenCloseEventHandler singleton;
			auto ui = RE::UI::GetSingleton();

			if (!ui) {
				logger::error("UI event source not found");
				return false;
			}

			ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(&singleton);

			logger::info("Registered {}", typeid(singleton).name());

			return true;
		}
	};
};
