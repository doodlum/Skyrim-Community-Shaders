#pragma once
#include <DirectXMath.h>
#include <d3d11.h>

#include "Buffer.h"
#include "Util.h"
#include <shared_mutex>

#include "Feature.h"
#include "ShaderCache.h"

struct LightLimitFix : Feature
{
public:
	static LightLimitFix* GetSingleton()
	{
		static LightLimitFix render;
		return &render;
	}

	virtual inline std::string GetName() override { return "Light Limit Fix"; }
	virtual inline std::string GetShortName() override { return "LightLimitFix"; }
	virtual inline std::string_view GetShaderDefineName() override { return "LIGHT_LIMIT_FIX"; }

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	enum class LightFlags : std::uint32_t
	{
		PortalStrict = (1 << 0),
		Shadow = (1 << 1),
	};

	struct PositionOpt
	{
		float3 data;
		uint pad0;
	};

	struct alignas(16) LightData
	{
		float3 color;
		float radius;
		PositionOpt positionWS[2];
		PositionOpt positionVS[2];
		uint128_t roomFlags = uint32_t(0);
		stl::enumeration<LightFlags> lightFlags;
		uint32_t shadowMaskIndex = 0;
		float pad0[2];
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
		uint pad0[2];
	};

	struct alignas(16) LightBuildingCB
	{
		float4x4 InvProjMatrix[2];
		float LightsNear;
		float LightsFar;
		uint pad0[2];
	};

	struct alignas(16) LightCullingCB
	{
		uint LightCount;
		uint pad[3];
	};

	struct alignas(16) PerFrame
	{
		uint EnableContactShadows;
		uint EnableLightsVisualisation;
		uint LightsVisualisationMode;
		float pad0;
		uint ClusterSize[4];
	};

	PerFrame GetCommonBufferData();

	struct alignas(16) StrictLightData
	{
		LightData StrictLights[15];
		uint NumStrictLights;
		int RoomIndex;
		uint pad0[2];
	};

	StrictLightData strictLightDataTemp;

	std::unique_ptr<Buffer> strictLightData = nullptr;

	int eyeCount = !REL::Module::IsVR() ? 1 : 2;
	bool previousEnableLightsVisualisation = settings.EnableLightsVisualisation;
	bool currentEnableLightsVisualisation = settings.EnableLightsVisualisation;

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
	float lightsNear = 1;
	float lightsFar = 16384;

	RE::NiPoint3 eyePositionCached[2]{};
	Matrix viewMatrixCached[2]{};
	Matrix viewMatrixInverseCached[2]{};

	virtual void SetupResources() override;
	virtual void Reset() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;

	virtual void DrawSettings() override;

	virtual void PostPostLoad() override;
	virtual void DataLoaded() override;

	float CalculateLightDistance(float3 a_lightPosition, float a_radius);
	void SetLightPosition(LightLimitFix::LightData& a_light, RE::NiPoint3 a_initialPosition, bool a_cached = true);
	void UpdateLights();
	virtual void Prepass() override;

	static inline float3 Saturation(float3 color, float saturation);
	static inline bool IsValidLight(RE::BSLight* a_light);
	static inline bool IsGlobalLight(RE::BSLight* a_light);

	struct Settings
	{
		bool EnableContactShadows = false;
		bool EnableLightsVisualisation = false;
		uint LightsVisualisationMode = 0;
	};

	uint clusterSize[3] = { 16 };

	Settings settings;

	void BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* a_pass);

	enum class Space
	{
		World = 0,
		Model = 1,
	};

	void BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights(RE::BSRenderPass* a_pass, DirectX::XMMATRIX& Transform, uint32_t, uint32_t, float WorldScale, Space RenderSpace);
	void BSLightingShader_SetupGeometry_After(RE::BSRenderPass* a_pass);

	eastl::hash_map<RE::NiNode*, uint8_t> roomNodes;

	struct Hooks
	{
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

		struct BSEffectShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				func(This, Pass, RenderFlags);
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				GetSingleton()->BSLightingShader_SetupGeometry_After(Pass);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSWaterShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				func(This, Pass, RenderFlags);
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				GetSingleton()->BSLightingShader_SetupGeometry_After(Pass);
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

		struct BSLightingShaderProperty_GetRenderPasses
		{
			static RE::BSShaderProperty::RenderPassArray* thunk(RE::BSLightingShaderProperty* property, RE::BSGeometry* geometry, std::uint32_t renderFlags, RE::BSShaderAccumulator* accumulator)
			{
				auto renderPasses = func(property, geometry, renderFlags, accumulator);
				if (renderPasses == nullptr) {
					return renderPasses;
				}

				auto currentPass = renderPasses->head;
				while (currentPass != nullptr) {
					if (currentPass->shader->shaderType == RE::BSShader::Type::Lighting) {
						constexpr uint32_t LightingTechniqueStart = 0x4800002D;
						// So that we always have shadow mask bound.
						currentPass->passEnum = ((currentPass->passEnum - LightingTechniqueStart) | static_cast<uint32_t>(SIE::ShaderCache::LightingShaderFlags::DefShadow)) + LightingTechniqueStart;
					}
					currentPass = currentPass->next;
				}

				return renderPasses;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			//stl::write_vfunc<0x2A, BSLightingShaderProperty_GetRenderPasses>(RE::VTABLE_BSLightingShaderProperty[0]);

			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);
			stl::write_vfunc<0x6, BSEffectShader_SetupGeometry>(RE::VTABLE_BSEffectShader[0]);
			stl::write_vfunc<0x6, BSWaterShader_SetupGeometry>(RE::VTABLE_BSWaterShader[0]);

			stl::write_thunk_call<BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights>(REL::RelocationID(100565, 107300).address() + REL::Relocate(0x523, 0xB0E, 0x5fe));
			
			logger::info("[LLF] Installed hooks");
		}
	};

	virtual bool SupportsVR() override { return true; };
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
		return fmt::format_to(ctx.out(), "{{address {:x} color {} radius {} posWS {} {} posVS {} {}}}",
			reinterpret_cast<uintptr_t>(&l),
			(Vector3)l.color,
			l.radius,
			(Vector3)l.positionWS[0].data, (Vector3)l.positionWS[1].data,
			(Vector3)l.positionVS[0].data, (Vector3)l.positionVS[1].data);
	}
};