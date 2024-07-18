#pragma once
#include <DirectXMath.h>
#include <d3d11.h>

#include "Buffer.h"
#include "Util.h"
#include <shared_mutex>

#include "Feature.h"
#include "ShaderCache.h"
#include <Features/LightLimitFix/ParticleLights.h>

struct LightLimitFix : Feature
{
public:
	static LightLimitFix* GetSingleton()
	{
		static LightLimitFix render;
		return &render;
	}

	virtual inline std::string GetName() { return "Light Limit Fix"; }
	virtual inline std::string GetShortName() { return "LightLimitFix"; }
	inline std::string_view GetShaderDefineName() override { return "LIGHT_LIMIT_FIX"; }

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	struct PositionOpt
	{
		float3 data;
		float pad0;
	};

	struct alignas(16) LightData
	{
		float3 color;
		float radius;
		PositionOpt positionWS[2];
		PositionOpt positionVS[2];
		uint firstPersonShadow;
		float pad0[3];
	};

	struct ClusterAABB
	{
		float4 minPoint;
		float4 maxPoint;
	};

	struct alignas(16) LightGrid
	{
		uint offset;
		uint lightCount;
		float pad0[2];
	};

	struct alignas(16) LightBuildingCB
	{
		float4x4 InvProjMatrix[2];
		float LightsNear;
		float LightsFar;
		float pad[2];
	};

	struct alignas(16) LightCullingCB
	{
		uint LightCount;
		float pad[3];
	};

	struct alignas(16) PerFrame
	{
		uint EnableContactShadows;
		uint EnableLightsVisualisation;
		uint LightsVisualisationMode;
		uint pad0;
	};

	PerFrame GetCommonBufferData();

	struct alignas(16) StrictLightData
	{
		LightData StrictLights[15];
		uint NumStrictLights;
		uint EnableGlobalLights;
		float LightsNear;
		float LightsFar;
	};

	StrictLightData strictLightDataTemp;

	struct CachedParticleLight
	{
		float grey;
		RE::NiPoint3 position;
		float radius;
	};

	std::unique_ptr<Buffer> strictLightData = nullptr;

	int eyeCount = !REL::Module::IsVR() ? 1 : 2;

	ID3D11ComputeShader* clusterBuildingCS = nullptr;
	ID3D11ComputeShader* clusterCullingCS = nullptr;

	ConstantBuffer* lightBuildingCB = nullptr;
	ConstantBuffer* lightCullingCB = nullptr;

	eastl::unique_ptr<Buffer> lights = nullptr;
	eastl::unique_ptr<Buffer> clusters = nullptr;
	eastl::unique_ptr<Buffer> lightCounter = nullptr;
	eastl::unique_ptr<Buffer> lightList = nullptr;
	eastl::unique_ptr<Buffer> lightGrid = nullptr;

	std::uint32_t lightCount = 0;

	struct ParticleLightInfo
	{
		RE::NiColorA color;
		ParticleLights::Config& config;
	};

	eastl::hash_map<RE::BSGeometry*, ParticleLightInfo> queuedParticleLights;
	eastl::hash_map<RE::BSGeometry*, ParticleLightInfo> particleLights;

	RE::NiPoint3 eyePositionCached[2]{};
	Matrix viewMatrixCached[2]{};
	Matrix viewMatrixInverseCached[2]{};

	virtual void SetupResources();
	virtual void Reset();

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();

	virtual void DrawSettings();
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void PostPostLoad() override;
	virtual void DataLoaded() override;

	float CalculateLightDistance(float3 a_lightPosition, float a_radius);
	void AddCachedParticleLights(eastl::vector<LightData>& lightsData, LightLimitFix::LightData& light, ParticleLights::Config* a_config = nullptr, RE::BSGeometry* a_geometry = nullptr, double timer = 0.0f);
	void SetLightPosition(LightLimitFix::LightData& a_light, RE::NiPoint3 a_initialPosition, bool a_cached = true);
	void UpdateLights();
	virtual void Prepass() override;

	static inline float3 Saturation(float3 color, float saturation);
	static inline bool IsValidLight(RE::BSLight* a_light);
	static inline bool IsGlobalLight(RE::BSLight* a_light);

	struct Settings
	{
		bool EnableContactShadows = false;
		bool EnableFirstPersonShadows = false;
		bool EnableLightsVisualisation = false;
		uint LightsVisualisationMode = 0;
		bool EnableParticleLights = true;
		bool EnableParticleLightsCulling = true;
		bool EnableParticleLightsDetection = true;
		float ParticleLightsSaturation = 1.0f;
		float ParticleBrightness = 1.0f;
		float ParticleRadius = 1.0f;
		float BillboardBrightness = 1.0f;
		float BillboardRadius = 1.0f;
		bool EnableParticleLightsOptimization = true;
		uint ParticleLightsOptimisationClusterRadius = 32;
	};

	float lightsNear = 0.0f;
	float lightsFar = 16384.0f;

	Settings settings;

	using ConfigPair = std::pair<ParticleLights::Config*, ParticleLights::GradientConfig*>;
	std::optional<ConfigPair> GetParticleLightConfigs(RE::BSRenderPass* a_pass);
	bool AddParticleLight(RE::BSRenderPass* a_pass, ConfigPair a_config);
	bool CheckParticleLights(RE::BSRenderPass* a_pass, uint32_t a_technique);

	void BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* a_pass);

	enum class Space
	{
		World = 0,
		Model = 1,
	};

	void BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights(RE::BSRenderPass* a_pass, DirectX::XMMATRIX& Transform, uint32_t, uint32_t, float WorldScale, Space RenderSpace);

	void BSLightingShader_SetupGeometry_After(RE::BSRenderPass* a_pass);

	std::shared_mutex cachedParticleLightsMutex;
	eastl::vector<CachedParticleLight> cachedParticleLights;
	std::uint32_t particleLightsDetectionHits = 0;

	float CalculateLuminance(CachedParticleLight& light, RE::NiPoint3& point);
	void AddParticleLightLuminance(RE::NiPoint3& targetPosition, int& numHits, float& lightLevel);

	struct Hooks
	{
		struct ValidLight1
		{
			static bool thunk(RE::BSShaderProperty* a_property, RE::BSLight* a_light)
			{
				return func(a_property, a_light) && ((REL::Module::IsVR() && !netimmerse_cast<RE::BSLightingShaderProperty*>(a_property)) || (a_light->portalStrict || !a_light->portalGraph || skyrim_cast<RE::BSShadowLight*>(a_light)));
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ValidLight2
		{
			static bool thunk(RE::BSShaderProperty* a_property, RE::BSLight* a_light)
			{
				return func(a_property, a_light) && ((REL::Module::IsVR() && !netimmerse_cast<RE::BSLightingShaderProperty*>(a_property)) || (a_light->portalStrict || !a_light->portalGraph || skyrim_cast<RE::BSShadowLight*>(a_light)));
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ValidLight3
		{
			static bool thunk(RE::BSShaderProperty* a_property, RE::BSLight* a_light)
			{
				return func(a_property, a_light) && ((REL::Module::IsVR() && !netimmerse_cast<RE::BSLightingShaderProperty*>(a_property)) || (a_light->portalStrict || !a_light->portalGraph || skyrim_cast<RE::BSShadowLight*>(a_light)));
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSBatchRenderer__RenderPassImmediately1
		{
			static void thunk(RE::BSRenderPass* Pass, uint32_t Technique, bool AlphaTest, uint32_t RenderFlags)
			{
				if (GetSingleton()->CheckParticleLights(Pass, Technique))
					func(Pass, Technique, AlphaTest, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSBatchRenderer__RenderPassImmediately2
		{
			static void thunk(RE::BSRenderPass* Pass, uint32_t Technique, bool AlphaTest, uint32_t RenderFlags)
			{
				if (GetSingleton()->CheckParticleLights(Pass, Technique))
					func(Pass, Technique, AlphaTest, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSBatchRenderer__RenderPassImmediately3
		{
			static void thunk(RE::BSRenderPass* Pass, uint32_t Technique, bool AlphaTest, uint32_t RenderFlags)
			{
				if (GetSingleton()->CheckParticleLights(Pass, Technique))
					func(Pass, Technique, AlphaTest, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				func(This, Pass, RenderFlags);
				GetSingleton()->BSLightingShader_SetupGeometry_After(Pass);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct AIProcess_CalculateLightValue_GetLuminance
		{
			static float thunk(RE::ShadowSceneNode* shadowSceneNode, RE::NiPoint3& targetPosition, int& numHits, float& sunLightLevel, float& lightLevel, RE::NiLight& refLight, int32_t shadowBitMask)
			{
				auto ret = func(shadowSceneNode, targetPosition, numHits, sunLightLevel, lightLevel, refLight, shadowBitMask);
				GetSingleton()->AddParticleLightLuminance(targetPosition, numHits, ret);
				return ret;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights
		{
			static void thunk(RE::BSGraphics::PixelShader* PixelShader, RE::BSRenderPass* Pass, DirectX::XMMATRIX& Transform, uint32_t LightCount, uint32_t ShadowLightCount, float WorldScale, Space RenderSpace)
			{
				GetSingleton()->BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights(Pass, Transform, LightCount, ShadowLightCount, WorldScale, RenderSpace);
				func(PixelShader, Pass, Transform, LightCount, ShadowLightCount, WorldScale, RenderSpace);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<ValidLight1>(REL::RelocationID(100994, 107781).address() + 0x92);
			stl::write_thunk_call<ValidLight2>(REL::RelocationID(100997, 107784).address() + REL::Relocate(0x139, 0x12A));
			stl::write_thunk_call<ValidLight3>(REL::RelocationID(101296, 108283).address() + REL::Relocate(0xB7, 0x7E));

			stl::write_thunk_call<BSBatchRenderer__RenderPassImmediately1>(REL::RelocationID(100877, 107673).address() + REL::Relocate(0x1E5, 0x1EE));
			stl::write_thunk_call<BSBatchRenderer__RenderPassImmediately2>(REL::RelocationID(100852, 107642).address() + REL::Relocate(0x29E, 0x28F));
			stl::write_thunk_call<BSBatchRenderer__RenderPassImmediately3>(REL::RelocationID(100871, 107667).address() + REL::Relocate(0xEE, 0xED));

			stl::write_thunk_call<AIProcess_CalculateLightValue_GetLuminance>(REL::RelocationID(38900, 39946).address() + REL::Relocate(0x1C9, 0x1D3));

			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);

			logger::info("[LLF] Installed hooks");

			stl::write_thunk_call<BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights>(REL::RelocationID(100565, 107300).address() + REL::Relocate(0x523, 0xB0E, 0x5fe));
		}
	};

	bool SupportsVR() override { return true; };
};

template <>
struct fmt::formatter<LightLimitFix::LightData>
{
	// Presentation format: 'f' - fixed.
	char presentation = 'f';

	// Parses format specifications of the form ['f'].
	constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
	{
		auto it = ctx.begin(), end = ctx.end();
		if (it != end && (*it == 'f'))
			presentation = *it++;

		// Check if reached the end of the range:
		if (it != end && *it != '}')
			throw_format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	// Formats the point p using the parsed format specification (presentation)
	// stored in this formatter.
	auto format(const LightLimitFix::LightData& l, format_context& ctx) const -> format_context::iterator
	{
		// ctx.out() is an output iterator to write to.
		return fmt::format_to(ctx.out(), "{{address {:x} color {} radius {} posWS {} {} posVS {} {} first-person shadow {}}}",
			reinterpret_cast<uintptr_t>(&l),
			(Vector3)l.color,
			l.radius,
			(Vector3)l.positionWS[0].data, (Vector3)l.positionWS[1].data,
			(Vector3)l.positionVS[0].data, (Vector3)l.positionVS[1].data,
			l.firstPersonShadow);
	}
};