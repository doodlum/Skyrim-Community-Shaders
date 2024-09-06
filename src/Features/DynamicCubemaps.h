#pragma once

#include "Buffer.h"
#include "Feature.h"

class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource);
	static bool Register();
};

struct DynamicCubemaps : Feature
{
public:
	static DynamicCubemaps* GetSingleton()
	{
		static DynamicCubemaps singleton;
		return &singleton;
	}

	// Specular irradiance

	ID3D11SamplerState* computeSampler = nullptr;

	struct alignas(16) SpecularMapFilterSettingsCB
	{
		float roughness;
		float pad[3];
	};

	ID3D11ComputeShader* specularIrradianceCS = nullptr;
	ConstantBuffer* spmapCB = nullptr;
	Texture2D* envTexture = nullptr;
	Texture2D* envReflectionsTexture = nullptr;
	ID3D11UnorderedAccessView* uavArray[7];
	ID3D11UnorderedAccessView* uavReflectionsArray[7];

	// Reflection capture

	struct alignas(16) UpdateCubemapCB
	{
		uint Reset;
		float3 CameraPreviousPosAdjust;
	};

	ID3D11ComputeShader* updateCubemapCS = nullptr;
	ConstantBuffer* updateCubemapCB = nullptr;

	ID3D11ComputeShader* inferCubemapCS = nullptr;
	ID3D11ComputeShader* inferCubemapReflectionsCS = nullptr;

	Texture2D* envCaptureTexture = nullptr;
	Texture2D* envCaptureRawTexture = nullptr;
	Texture2D* envCapturePositionTexture = nullptr;
	Texture2D* envInferredTexture = nullptr;

	ID3D11ShaderResourceView* defaultCubemap = nullptr;

	bool activeReflections = false;
	bool resetCapture = true;

	enum class NextTask
	{
		kCapture,
		kInferrence,
		kInferrence2,
		kIrradiance,
		kIrradiance2
	};

	NextTask nextTask = NextTask::kCapture;

	// Editor window

	struct Settings
	{
		uint Enabled = false;
		uint pad0[3]{};
		float4 CubemapColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	};

	Settings settings;

	void UpdateCubemap();

	void PostDeferred();

	virtual inline std::string GetName() override { return "Dynamic Cubemaps"; }
	virtual inline std::string GetShortName() override { return "DynamicCubemaps"; }
	virtual inline std::string_view GetShaderDefineName() override { return "DYNAMIC_CUBEMAPS"; }
	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void SetupResources() override;
	virtual void Reset() override;

	virtual void DrawSettings() override;
	virtual void DataLoaded() override;

	std::vector<std::string> iniVRCubeMapSettings{
		{ "bAutoWaterSilhouetteReflections:Water" },  //IniSettings 0x1eaa018
		{ "bForceHighDetailReflections:Water" },      //IniSettings 0x1eaa030
	};

	std::map<std::string, uintptr_t> hiddenVRCubeMapSettings{
		{ "bReflectExplosions:Water", 0x1eaa000 },
		{ "bReflectLODLand:Water", 0x1eaa060 },
		{ "bReflectLODObjects:Water", 0x1eaa078 },
		{ "bReflectLODTrees:Water", 0x1eaa090 },
		{ "bReflectSky:Water", 0x1eaa0a8 },
		{ "bUseWaterRefractions:Water", 0x1eaa0c0 },
	};

	virtual void ClearShaderCache() override;
	ID3D11ComputeShader* GetComputeShaderUpdate();
	ID3D11ComputeShader* GetComputeShaderInferrence();
	ID3D11ComputeShader* GetComputeShaderInferrenceReflections();
	ID3D11ComputeShader* GetComputeShaderSpecularIrradiance();

	void UpdateCubemapCapture();

	void Inferrence(bool a_reflections);

	void Irradiance(bool a_reflections);

	virtual bool SupportsVR() override { return true; };
};
