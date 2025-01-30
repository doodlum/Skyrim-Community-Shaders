#pragma once

struct CloudShadows;
struct DynamicCubemaps;
struct ExtendedMaterials;
struct GrassCollision;
struct GrassLighting;
struct LightLimitFix;
struct ScreenSpaceGI;
struct ScreenSpaceShadows;
struct Skylighting;
struct SubsurfaceScattering;
struct TerrainBlending;
struct TerrainShadows;
struct VolumetricLighting;
struct WaterEffects;
struct WetnessEffects;

class ParticleLights;

class State;
class Deferred;
struct TruePBR;
class Menu;

namespace SIE
{
	class ShaderCache;
}

namespace globals
{
	namespace d3d
	{
		extern ID3D11Device* device;
		extern ID3D11DeviceContext* context;
		extern IDXGISwapChain* swapchain;
	}

	namespace features
	{
		extern CloudShadows* cloudShadows;
		extern DynamicCubemaps* dynamicCubemaps;
		extern ExtendedMaterials* extendedMaterials;
		extern GrassCollision* grassCollision;
		extern GrassLighting* grassLighting;
		extern LightLimitFix* lightLimitFix;
		extern ScreenSpaceGI* screenSpaceGI;
		extern ScreenSpaceShadows* screenSpaceShadows;
		extern Skylighting* skylighting;
		extern SubsurfaceScattering* subsurfaceScattering;
		extern TerrainBlending* terrainBlending;
		extern TerrainShadows* terrainShadows;
		extern VolumetricLighting* volumetricLighting;
		extern WaterEffects* waterEffects;
		extern WetnessEffects* wetnessEffects;

		extern ParticleLights* particleLights;
	}

	namespace game
	{
		extern RE::BSGraphics::RendererShadowState* shadowState;
		extern RE::BSGraphics::State* graphicsState;
		extern RE::BSGraphics::Renderer* renderer;
		extern RE::BSShaderManager::State* smState;
		extern RE::TES* tes;
		extern bool isVR;
		extern RE::MemoryManager* memoryManager;
		extern RE::INISettingCollection* iniSettingCollection;
		extern float* cameraNear;
		extern float* cameraFar;
		extern RE::BSUtilityShader* utilityShader;
		extern RE::Sky* sky;

		extern RE::BSGraphics::PixelShader** currentPixelShader;
		extern RE::BSGraphics::VertexShader** currentVertexShader;
		extern stl::enumeration<RE::BSGraphics::ShaderFlags, uint32_t>* stateUpdateFlags;

		extern RE::Setting* bEnableLandFade;
		extern RE::Setting* bShadowsOnGrass;
		extern RE::Setting* shadowMaskQuarter;
	}

	extern State* state;
	extern Deferred* deferred;
	extern TruePBR* truePBR;
	extern Menu* menu;
	extern SIE::ShaderCache* shaderCache;

	void OnInit();
	void OnDataLoaded();
}