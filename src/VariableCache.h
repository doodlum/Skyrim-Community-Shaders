#pragma once

#include "Deferred.h"
#include "ShaderCache.h"
#include "State.h"

#include "Features/CloudShadows.h"
#include "Features/TerrainBlending.h"

#include "TruePBR.h"

class VariableCache
{
	static VariableCache instance;

public:
	static VariableCache* GetSingleton() { return &instance; };

	ID3D11DeviceContext* context = nullptr;
	RE::BSGraphics::PixelShader** currentPixelShader = nullptr;
	RE::BSGraphics::VertexShader** currentVertexShader = nullptr;
	stl::enumeration<RE::BSGraphics::ShaderFlags, uint32_t>* stateUpdateFlags = nullptr;
	State* state = nullptr;
	SIE::ShaderCache* shaderCache = nullptr;
	RE::BSGraphics::RendererShadowState* rendererShadowState = nullptr;
	Deferred* deferred = nullptr;
	TerrainBlending* terrainBlending = nullptr;
	CloudShadows* cloudShadows = nullptr;
	TruePBR* truePBR = nullptr;

	void InitializeVariables();
};

inline constinit VariableCache VariableCache::instance;