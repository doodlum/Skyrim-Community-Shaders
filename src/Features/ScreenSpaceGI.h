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

	void DrawSSGI(Texture2D* outGI);
	void GenerateHilbertLUT();
	void UpdateSB();

	//////////////////////////////////////////////////////////////////////////////////

	bool hilbertLutGenFlag = false;
	bool recompileFlag = false;

	struct Settings
	{
		bool Enabled = true;
		bool UseBitmask = true;
		bool EnableGI = true;
		// performance/quality
		uint NumSlices = 2;
		uint NumSteps = 5;
		bool HalfRes = true;
		// float SampleDistributionPower = 1.f;
		float DepthMIPSamplingOffset = 3.3f;
		// visual
		float EffectRadius = 200.f;  // world (viewspace) maximum size of the shadow
		float EffectFalloffRange = .615f;
		float ThinOccluderCompensation = 0.f;
		float Thickness = 50.f;
		float2 DepthFadeRange = { 2e4, 3e4 };
		// gi
		bool CheckBackface = true;
		float BackfaceStrength = 0.1f;
		bool EnableGIBounce = true;
		float GIBounceFade = 0.8f;
		float GIDistanceCompensation = 1;
		float GICompensationMaxDist = 200;
		// mix
		float AOPower = 1.f;
		float GIStrength = 8.f;
		// denoise
		bool EnableTemporalDenoiser = true;
		bool EnableBlur = true;
		float DepthDisocclusion = 50.f;
		float NormalDisocclusion = .8f;
		uint MaxAccumFrames = 16;
		float BlurRadius = 4.f;
		uint BlurPasses = 1;
		float DistanceNormalisation = .05f;
	} settings;

	struct alignas(16) SSGICB
	{
		float4x4 PrevInvViewMat[2];
		float2 NDCToViewMul[2];
		float2 NDCToViewAdd[2];
		float2 NDCToViewMul_x_PixelSize[2];

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

	eastl::unique_ptr<Texture2D> texHilbertLUT = nullptr;
	eastl::unique_ptr<Texture2D> texWorkingDepth = nullptr;
	winrt::com_ptr<ID3D11UnorderedAccessView> uavWorkingDepth[5] = { nullptr };
	eastl::unique_ptr<Texture2D> texPrevGeo = nullptr;
	eastl::unique_ptr<Texture2D> texRadiance = nullptr;
	eastl::unique_ptr<Texture2D> texAccumFrames[2] = { nullptr };
	eastl::unique_ptr<Texture2D> texGI[2] = { nullptr };
	eastl::unique_ptr<Texture2D> texPrevGIAlbedo = { nullptr };

	winrt::com_ptr<ID3D11SamplerState> linearClampSampler = nullptr;
	winrt::com_ptr<ID3D11SamplerState> pointClampSampler = nullptr;

	winrt::com_ptr<ID3D11ComputeShader> hilbertLutCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> prefilterDepthsCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> radianceDisoccCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> giCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> blurCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> upsampleCompute = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> outputCompute = nullptr;
};