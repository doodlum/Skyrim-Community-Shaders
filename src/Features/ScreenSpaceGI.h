#pragma once

#include "Buffer.h"
#include "Feature.h"

struct ScreenSpaceGI : Feature
{
	static ScreenSpaceGI* GetSingleton()
	{
		static ScreenSpaceGI singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Screen Space GI"; }
	virtual inline std::string GetShortName() override { return "ScreenSpaceGI"; }

	virtual void RestoreDefaultSettings() override;
	virtual void DrawSettings() override;

	virtual void Load(json& o_json) override;
	virtual void Save(json& o_json) override;

	virtual inline void Reset() override{};
	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;
	void CompileComputeShaders();
	bool ShadersOK();

	virtual inline void Draw(const RE::BSShader*, const uint32_t) override{};

	void DrawSSGI(Texture2D* srcPrevAmbient);
	void UpdateSB();

	//////////////////////////////////////////////////////////////////////////////////

	bool recompileFlag = false;
	uint outputGIIdx = 0;

	struct Settings
	{
		bool Enabled = true;
		bool UseBitmask = true;
		bool EnableGI = true;
		// performance/quality
		uint NumSlices = 2;
		uint NumSteps = 4;
		bool HalfRate = true;
		float DepthMIPSamplingOffset = 3.3f;
		// visual
		float EffectRadius = 500.f;
		float EffectFalloffRange = .615f;
		float ThinOccluderCompensation = 0.f;
		float Thickness = 75.f;
		float2 DepthFadeRange = { 2e4, 3e4 };
		// gi
		bool CheckBackface = true;
		float BackfaceStrength = 0.f;
		bool EnableGIBounce = true;
		float GIBounceFade = 1.f;
		float GIDistanceCompensation = 1.f;
		float GICompensationMaxDist = 500;
		// mix
		float AOPower = 1.f;
		float GIStrength = 6.f;
		// denoise
		bool EnableTemporalDenoiser = true;
		bool EnableBlur = true;
		float DepthDisocclusion = .1f;
		float NormalDisocclusion = .1f;
		uint MaxAccumFrames = 16;
		float BlurRadius = 6.f;
		uint BlurPasses = 1;
		float DistanceNormalisation = 1.f;
	} settings;

	struct alignas(16) SSGICB
	{
		float4x4 PrevInvViewMat[2];
		float2 NDCToViewMul[2];
		float2 NDCToViewAdd[2];

		float2 TexDim;
		float2 RcpTexDim;  //
		float2 FrameDim;
		float2 RcpFrameDim;  //
		uint FrameIndex;

		uint NumSlices;
		uint NumSteps;
		float DepthMIPSamplingOffset;  //

		float EffectRadius;
		float EffectFalloffRange;
		float ThinOccluderCompensation;
		float Thickness;  //
		float2 DepthFadeRange;
		float DepthFadeScaleConst;

		float BackfaceStrength;  //
		float GIBounceFade;
		float GIDistanceCompensation;
		float GICompensationMaxDist;

		float AOPower;  //
		float GIStrength;

		float DepthDisocclusion;
		float NormalDisocclusion;
		uint MaxAccumFrames;  //

		float BlurRadius;
		float DistanceNormalisation;

		float pad[2];
	};
	eastl::unique_ptr<ConstantBuffer> ssgiCB;

	eastl::unique_ptr<Texture2D> texNoise = nullptr;
	eastl::unique_ptr<Texture2D> texWorkingDepth = nullptr;
	winrt::com_ptr<ID3D11UnorderedAccessView> uavWorkingDepth[5] = { nullptr };
	eastl::unique_ptr<Texture2D> texPrevGeo = nullptr;
	eastl::unique_ptr<Texture2D> texRadiance = nullptr;
	eastl::unique_ptr<Texture2D> texAccumFrames[2] = { nullptr };
	eastl::unique_ptr<Texture2D> texGI[2] = { nullptr };

	winrt::com_ptr<ID3D11SamplerState> linearClampSampler = nullptr;
	winrt::com_ptr<ID3D11SamplerState> pointClampSampler = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> prefilterDepthsCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> radianceDisoccCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> giCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> blurCompute = nullptr;
};