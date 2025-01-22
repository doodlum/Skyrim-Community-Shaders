#pragma once

#include "Deferred.h"
#include "ShaderCache.h"
#include "State.h"

#include "Features/CloudShadows.h"
#include "Features/TerrainBlending.h"
#include "Features/LightLimitFix.h"
#include "Features/GrassCollision.h"

#include "TruePBR.h"
#include "Features/LightLimitFix/ParticleLights.h"

class VariableCache
{
	static VariableCache instance;

public:
	static VariableCache* GetSingleton() { return &instance; };

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	RE::BSGraphics::PixelShader** currentPixelShader = nullptr;
	RE::BSGraphics::VertexShader** currentVertexShader = nullptr;
	stl::enumeration<RE::BSGraphics::ShaderFlags, uint32_t>* stateUpdateFlags = nullptr;
	State* state = nullptr;
	SIE::ShaderCache* shaderCache = nullptr;
	RE::BSGraphics::RendererShadowState* shadowState = nullptr;

	Deferred* deferred = nullptr;

	TerrainBlending* terrainBlending = nullptr;
	CloudShadows* cloudShadows = nullptr;
	TruePBR* truePBR = nullptr;
	LightLimitFix* lightLimitFix = nullptr;
	ParticleLights* particleLights = nullptr;
	GrassCollision* grassCollision = nullptr;

	float* cameraNear = nullptr;
	float* cameraFar = nullptr;

	RE::BSGraphics::State* graphicsState = nullptr;
	RE::BSGraphics::Renderer* renderer = nullptr;
	RE::BSShaderManager::State* smState = nullptr;
	RE::TES* tes = nullptr;
	bool isVR = false;
	RE::MemoryManager* memoryManager = nullptr;
	RE::INISettingCollection* iniSettingCollection = nullptr;
	RE::Setting* bEnableLandFade = nullptr;
	RE::Setting* bShadowsOnGrass = nullptr;
	RE::Setting* shadowMaskQuarter = nullptr;

	void OnInit();
	void OnDataLoaded();
};

inline constinit VariableCache VariableCache::instance;