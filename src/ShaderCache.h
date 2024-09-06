#pragma once

#include <RE/B/BSShader.h>

#include "BS_thread_pool.hpp"
#include "efsw/efsw.hpp"
#include <chrono>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

static constexpr REL::Version SHADER_CACHE_VERSION = { 0, 0, 0, 20 };

using namespace std::chrono;

namespace ShaderConstants
{
	struct LightingPS
	{
		static const LightingPS& Get()
		{
			static LightingPS instance = REL::Module::IsVR() ? GetVR() : GetFlat();
			return instance;
		}

		static LightingPS GetFlat()
		{
			return LightingPS{};
		}

		static LightingPS GetVR()
		{
			return LightingPS{
				.AmbientColor = 24,
				.FogColor = 25,
				.ColourOutputClamp = 26,
				.EnvmapData = 27,
				.ParallaxOccData = 28,
				.TintColor = 29,
				.LODTexParams = 30,
				.SpecularColor = 31,
				.SparkleParams = 32,
				.MultiLayerParallaxData = 33,
				.LightingEffectParams = 34,
				.IBLParams = 35,
				.LandscapeTexture1to4IsSnow = 36,
				.LandscapeTexture5to6IsSnow = 37,
				.LandscapeTexture1to4IsSpecPower = 38,
				.LandscapeTexture5to6IsSpecPower = 39,
				.SnowRimLightParameters = 40,
				.CharacterLightParams = 41,
				.PBRFlags = 44,
				.PBRParams1 = 45,
				.LandscapeTexture2PBRParams = 46,
				.LandscapeTexture3PBRParams = 47,
				.LandscapeTexture4PBRParams = 48,
				.LandscapeTexture5PBRParams = 49,
				.LandscapeTexture6PBRParams = 50,
				.PBRParams2 = 51,
				.LandscapeTexture1GlintParameters = 52,
				.LandscapeTexture2GlintParameters = 53,
				.LandscapeTexture3GlintParameters = 54,
				.LandscapeTexture4GlintParameters = 55,
				.LandscapeTexture5GlintParameters = 56,
				.LandscapeTexture6GlintParameters = 57,

				.ShadowSampleParam = 18,
				.EndSplitDistances = 19,
				.StartSplitDistances = 20,
				.DephBiasParam = 21,
				.ShadowLightParam = 22,
				.ShadowMapProj = 23,
				.InvWorldMat = 42,
				.PreviousWorldMat = 43,
			};
		}

		const int32_t NumLightNumShadowLight = 0;
		const int32_t PointLightPosition = 1;
		const int32_t PointLightColor = 2;
		const int32_t DirLightDirection = 3;
		const int32_t DirLightColor = 4;
		const int32_t DirectionalAmbient = 5;
		const int32_t AmbientSpecularTintAndFresnelPower = 6;
		const int32_t MaterialData = 7;
		const int32_t EmitColor = 8;
		const int32_t AlphaTestRef = 9;
		const int32_t ShadowLightMaskSelect = 10;
		const int32_t VPOSOffset = 11;
		const int32_t ProjectedUVParams = 12;
		const int32_t ProjectedUVParams2 = 13;
		const int32_t ProjectedUVParams3 = 14;
		const int32_t SplitDistance = 15;
		const int32_t SSRParams = 16;
		const int32_t WorldMapOverlayParametersPS = 17;
		const int32_t AmbientColor = 18;
		const int32_t FogColor = 19;
		const int32_t ColourOutputClamp = 20;
		const int32_t EnvmapData = 21;
		const int32_t ParallaxOccData = 22;
		const int32_t TintColor = 23;
		const int32_t LODTexParams = 24;
		const int32_t SpecularColor = 25;
		const int32_t SparkleParams = 26;
		const int32_t MultiLayerParallaxData = 27;
		const int32_t LightingEffectParams = 28;
		const int32_t IBLParams = 29;
		const int32_t LandscapeTexture1to4IsSnow = 30;
		const int32_t LandscapeTexture5to6IsSnow = 31;
		const int32_t LandscapeTexture1to4IsSpecPower = 32;
		const int32_t LandscapeTexture5to6IsSpecPower = 33;
		const int32_t SnowRimLightParameters = 34;
		const int32_t CharacterLightParams = 35;
		const int32_t PBRFlags = 36;
		const int32_t PBRParams1 = 37;
		const int32_t LandscapeTexture2PBRParams = 38;
		const int32_t LandscapeTexture3PBRParams = 39;
		const int32_t LandscapeTexture4PBRParams = 40;
		const int32_t LandscapeTexture5PBRParams = 41;
		const int32_t LandscapeTexture6PBRParams = 42;
		const int32_t PBRParams2 = 43;
		const int32_t LandscapeTexture1GlintParameters = 44;
		const int32_t LandscapeTexture2GlintParameters = 45;
		const int32_t LandscapeTexture3GlintParameters = 46;
		const int32_t LandscapeTexture4GlintParameters = 47;
		const int32_t LandscapeTexture5GlintParameters = 48;
		const int32_t LandscapeTexture6GlintParameters = 49;

		const int32_t ShadowSampleParam = -1;
		const int32_t EndSplitDistances = -1;
		const int32_t StartSplitDistances = -1;
		const int32_t DephBiasParam = -1;
		const int32_t ShadowLightParam = -1;
		const int32_t ShadowMapProj = -1;
		const int32_t InvWorldMat = -1;
		const int32_t PreviousWorldMat = -1;
	};

	struct GrassPS
	{
		static const GrassPS& Get()
		{
			static GrassPS instance = REL::Module::IsVR() ? GetVR() : GetFlat();
			return instance;
		}

		static GrassPS GetFlat()
		{
			return GrassPS{};
		}

		static GrassPS GetVR()
		{
			return GrassPS{};
		}

		const int32_t PBRFlags = 0;
		const int32_t PBRParams1 = 1;
		const int32_t PBRParams2 = 2;
	};
}

namespace SIE
{
	enum class ShaderClass
	{
		Vertex,
		Pixel,
		Compute,
		Total,
	};

	class ShaderCompilationTask
	{
	public:
		enum Status
		{
			Pending,
			Failed,
			Completed
		};
		ShaderCompilationTask(ShaderClass shaderClass, const RE::BSShader& shader,
			uint32_t descriptor);
		void Perform() const;

		size_t GetId() const;
		std::string GetString() const;

		bool operator==(const ShaderCompilationTask& other) const;

	protected:
		ShaderClass shaderClass;
		const RE::BSShader& shader;
		uint32_t descriptor;
	};
}

template <>
struct std::hash<SIE::ShaderCompilationTask>
{
	std::size_t operator()(const SIE::ShaderCompilationTask& task) const noexcept
	{
		return task.GetId();
	}
};

namespace SIE
{
	class CompilationSet
	{
	public:
		std::optional<ShaderCompilationTask> WaitTake(std::stop_token stoken);
		void Add(const ShaderCompilationTask& task);
		void Complete(const ShaderCompilationTask& task);
		void Clear();
		std::string GetHumanTime(double a_totalms);
		double GetEta();
		std::string GetStatsString(bool a_timeOnly = false);
		std::atomic<uint64_t> completedTasks = 0;
		std::atomic<uint64_t> totalTasks = 0;
		std::atomic<uint64_t> failedTasks = 0;
		std::atomic<uint64_t> cacheHitTasks = 0;  // number of compiles of a previously seen shader combo
		std::mutex compilationMutex;

	private:
		std::unordered_set<ShaderCompilationTask> availableTasks;
		std::unordered_set<ShaderCompilationTask> tasksInProgress;
		std::unordered_set<ShaderCompilationTask> processedTasks;  // completed or failed
		std::condition_variable_any conditionVariable;
		std::chrono::steady_clock::time_point lastReset = high_resolution_clock::now();
		std::chrono::steady_clock::time_point lastCalculation = high_resolution_clock::now();
		double totalMs = (double)duration_cast<std::chrono::milliseconds>(lastReset - lastReset).count();
	};

	struct ShaderCacheResult
	{
		ID3DBlob* blob;
		ShaderCompilationTask::Status status;
		system_clock::time_point compileTime = system_clock::now();
	};

	class UpdateListener;

	class ShaderCache
	{
	public:
		static ShaderCache& Instance()
		{
			static ShaderCache instance;
			return instance;
		}

		inline static bool IsSupportedShader(const RE::BSShader::Type type)
		{
			if (!REL::Module::IsVR())
				return type == RE::BSShader::Type::Lighting ||
				       type == RE::BSShader::Type::BloodSplatter ||
				       type == RE::BSShader::Type::DistantTree ||
				       type == RE::BSShader::Type::Sky ||
				       type == RE::BSShader::Type::Grass ||
				       type == RE::BSShader::Type::Particle ||
				       type == RE::BSShader::Type::Water ||
				       type == RE::BSShader::Type::Effect ||
				       type == RE::BSShader::Type::Utility ||
				       type == RE::BSShader::Type::ImageSpace;
			return type == RE::BSShader::Type::Lighting ||
			       type == RE::BSShader::Type::BloodSplatter ||
			       type == RE::BSShader::Type::DistantTree ||
			       type == RE::BSShader::Type::Sky ||
			       type == RE::BSShader::Type::Grass ||
			       type == RE::BSShader::Type::Particle ||
			       type == RE::BSShader::Type::Water ||
			       type == RE::BSShader::Type::Effect ||
			       type == RE::BSShader::Type::Utility ||
			       type == RE::BSShader::Type::ImageSpace;
		}

		inline static bool IsSupportedShader(const RE::BSShader& shader)
		{
			return IsSupportedShader(shader.shaderType.get());
		}

		inline static bool IsShaderSourceAvailable(const RE::BSShader& shader);

		bool IsCompiling();
		bool IsEnabled() const;
		void SetEnabled(bool value);
		bool IsAsync() const;
		void SetAsync(bool value);
		bool IsDump() const;
		void SetDump(bool value);

		bool IsDiskCache() const;
		void SetDiskCache(bool value);
		void DeleteDiskCache();
		void ValidateDiskCache();
		void WriteDiskCacheInfo();
		bool UseFileWatcher() const;
		void SetFileWatcher(bool value);

		void StartFileWatcher();
		void StopFileWatcher();

		/** @brief Update the RE::BSShader::Type timestamp based on timestamp.
		@param  a_type Case insensitive string for the type of shader. E.g., Lighting
		@return True if the shader for the type (i.e., Lighting.hlsl) timestamp was updated
		*/
		bool UpdateShaderModifiedTime(std::string a_type);
		/** @brief Whether the ShaderFile for RE::BSShader::Type has been modified since the timestamp.
		@param  a_type Case insensitive string for the type of shader. E.g., Lighting
		@param  a_current The current time in system_clock::time_point.
		@return True if the shader for the type (i.e., Lighting.hlsl) has been modified since the timestamp
		*/
		bool ShaderModifiedSince(std::string a_type, system_clock::time_point a_current);

		void Clear();
		void Clear(RE::BSShader::Type a_type);

		bool AddCompletedShader(ShaderClass shaderClass, const RE::BSShader& shader, uint32_t descriptor, ID3DBlob* a_blob);
		ID3DBlob* GetCompletedShader(const std::string& a_key);
		ID3DBlob* GetCompletedShader(const SIE::ShaderCompilationTask& a_task);
		ID3DBlob* GetCompletedShader(ShaderClass shaderClass, const RE::BSShader& shader, uint32_t descriptor);
		ShaderCompilationTask::Status GetShaderStatus(const std::string& a_key);
		std::string GetShaderStatsString(bool a_timeOnly = false);

		RE::BSGraphics::VertexShader* GetVertexShader(const RE::BSShader& shader, uint32_t descriptor);
		RE::BSGraphics::PixelShader* GetPixelShader(const RE::BSShader& shader,
			uint32_t descriptor);
		RE::BSGraphics::ComputeShader* GetComputeShader(const RE::BSShader& shader,
			uint32_t descriptor);

		RE::BSGraphics::VertexShader* MakeAndAddVertexShader(const RE::BSShader& shader,
			uint32_t descriptor);
		RE::BSGraphics::PixelShader* MakeAndAddPixelShader(const RE::BSShader& shader,
			uint32_t descriptor);
		RE::BSGraphics::ComputeShader* MakeAndAddComputeShader(const RE::BSShader& shader,
			uint32_t descriptor);

		static std::string GetDefinesString(const RE::BSShader& shader, uint32_t descriptor);

		uint64_t GetCachedHitTasks();
		uint64_t GetCompletedTasks();
		uint64_t GetFailedTasks();
		uint64_t GetTotalTasks();
		void IncCacheHitTasks();
		void ToggleErrorMessages();
		void DisableShaderBlocking();
		void IterateShaderBlock(bool a_forward = true);
		bool IsHideErrors();

		void InsertModifiedShaderMap(std::string a_shader, std::chrono::time_point<std::chrono::system_clock> a_time);
		std::chrono::time_point<std::chrono::system_clock> GetModifiedShaderMapTime(const std::string& a_shader);

		int32_t compilationThreadCount = std::max({ static_cast<int32_t>(std::thread::hardware_concurrency()) - 4, static_cast<int32_t>(std::thread::hardware_concurrency()) * 3 / 4, 1 });
		int32_t backgroundCompilationThreadCount = std::max(static_cast<int32_t>(std::thread::hardware_concurrency()) / 2, 1);
		BS::thread_pool compilationPool{};
		bool backgroundCompilation = false;
		bool menuLoaded = false;

		enum class LightingShaderTechniques
		{
			None = 0,
			Envmap = 1,
			Glowmap = 2,
			Parallax = 3,
			Facegen = 4,
			FacegenRGBTint = 5,
			Hair = 6,
			ParallaxOcc = 7,
			MTLand = 8,
			LODLand = 9,
			Snow = 10,  // unused
			MultilayerParallax = 11,
			TreeAnim = 12,
			LODObjects = 13,
			MultiIndexSparkle = 14,
			LODObjectHD = 15,
			Eye = 16,
			Cloud = 17,  // unused
			LODLandNoise = 18,
			MTLandLODBlend = 19,
		};

		enum class LightingShaderFlags
		{
			VC = 1 << 0,
			Skinned = 1 << 1,
			ModelSpaceNormals = 1 << 2,
			// flags 3 to 8 are unused by vanilla
			// Community Shaders start
			TruePbr = 1 << 3,
			Deferred = 1 << 4,
			// Community Shaders end
			Specular = 1 << 9,
			SoftLighting = 1 << 10,
			RimLighting = 1 << 11,
			BackLighting = 1 << 12,
			ShadowDir = 1 << 13,
			DefShadow = 1 << 14,
			ProjectedUV = 1 << 15,
			AnisoLighting = 1 << 16,  // Reused for glint with PBR
			AmbientSpecular = 1 << 17,
			WorldMap = 1 << 18,
			BaseObjectIsSnow = 1 << 19,
			DoAlphaTest = 1 << 20,
			Snow = 1 << 21,
			CharacterLight = 1 << 22,
			AdditionalAlphaMask = 1 << 23
		};

		enum class BloodSplatterShaderTechniques
		{
			Splatter = 0,
			Flare = 1,
		};

		enum class DistantTreeShaderTechniques
		{
			DistantTreeBlock = 0,
			Depth = 1,
		};

		enum class DistantTreeShaderFlags
		{
			Deferred = 1 << 8,
			AlphaTest = 1 << 16,
		};

		enum class SkyShaderTechniques
		{
			SunOcclude = 0,
			SunGlare = 1,
			MoonAndStarsMask = 2,
			Stars = 3,
			Clouds = 4,
			CloudsLerp = 5,
			CloudsFade = 6,
			Texture = 7,
			Sky = 8,
		};

		enum class GrassShaderTechniques
		{
			RenderDepth = 8,
			TruePbr = 9,
		};

		enum class GrassShaderFlags
		{
			AlphaTest = 0x10000,
		};

		enum class ParticleShaderTechniques
		{
			Particles = 0,
			ParticlesGryColor = 1,
			ParticlesGryAlpha = 2,
			ParticlesGryColorAlpha = 3,
			EnvCubeSnow = 4,
			EnvCubeRain = 5,
		};

		enum class WaterShaderTechniques
		{
			Underwater = 8,
			Lod = 9,
			Stencil = 10,
			Simple = 11,
		};

		enum class WaterShaderFlags
		{
			Vc = 1 << 0,
			NormalTexCoord = 1 << 1,
			Reflections = 1 << 2,
			Refractions = 1 << 3,
			Depth = 1 << 4,
			Interior = 1 << 5,
			Wading = 1 << 6,
			VertexAlphaDepth = 1 << 7,
			Cubemap = 1 << 8,
			Flowmap = 1 << 9,
			BlendNormals = 1 << 10,
		};

		enum class EffectShaderFlags
		{
			Vc = 1 << 0,
			TexCoord = 1 << 1,
			TexCoordIndex = 1 << 2,
			Skinned = 1 << 3,
			Normals = 1 << 4,
			BinormalTangent = 1 << 5,
			Texture = 1 << 6,
			IndexedTexture = 1 << 7,
			Falloff = 1 << 8,
			AddBlend = 1 << 10,
			MultBlend = 1 << 11,
			Particles = 1 << 12,
			StripParticles = 1 << 13,
			Blood = 1 << 14,
			Membrane = 1 << 15,
			Lighting = 1 << 16,
			ProjectedUv = 1 << 17,
			Soft = 1 << 18,
			GrayscaleToColor = 1 << 19,
			GrayscaleToAlpha = 1 << 20,
			IgnoreTexAlpha = 1 << 21,
			MultBlendDecal = 1 << 22,
			AlphaTest = 1 << 23,
			SkyObject = 1 << 24,
			MsnSpuSkinned = 1 << 25,
			MotionVectorsNormals = 1 << 26,
			Deferred = 1 << 27
		};

		enum class UtilityShaderFlags : uint64_t
		{
			Vc = 1 << 0,
			Texture = 1 << 1,
			Skinned = 1 << 2,
			Normals = 1 << 3,
			BinormalTangent = 1 << 4,
			AlphaTest = 1 << 7,
			LodLandscape = 1 << 8,
			RenderNormal = 1 << 9,
			RenderNormalFalloff = 1 << 10,
			RenderNormalClamp = 1 << 11,
			RenderNormalClear = 1 << 12,
			RenderDepth = 1 << 13,
			RenderShadowmap = 1 << 14,
			RenderShadowmapClamped = 1 << 15,
			GrayscaleToAlpha = 1 << 15,
			RenderShadowmapPb = 1 << 16,
			AdditionalAlphaMask = 1 << 16,
			DepthWriteDecals = 1 << 17,
			DebugShadowSplit = 1 << 18,
			DebugColor = 1 << 19,
			GrayscaleMask = 1 << 20,
			RenderShadowmask = 1 << 21,
			RenderShadowmaskSpot = 1 << 22,
			RenderShadowmaskPb = 1 << 23,
			RenderShadowmaskDpb = 1 << 24,
			RenderBaseTexture = 1 << 25,
			TreeAnim = 1 << 26,
			LodObject = 1 << 27,
			LocalMapFogOfWar = 1 << 28,
			OpaqueEffect = 1 << 29,
		};

		uint blockedKeyIndex = (uint)-1;  // index in shaderMap; negative value indicates disabled
		std::string blockedKey = "";
		std::vector<uint32_t> blockedIDs;  // more than one descriptor could be blocked based on shader hash
		HANDLE managementThread = nullptr;

	private:
		ShaderCache();
		void ManageCompilationSet(std::stop_token stoken);
		void ProcessCompilationSet(std::stop_token stoken, SIE::ShaderCompilationTask task);

		~ShaderCache();

		std::array<eastl::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::VertexShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			vertexShaders;
		std::array<eastl::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::PixelShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			pixelShaders;
		std::array<eastl::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::ComputeShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			computeShaders;

		bool isEnabled = true;
		bool isDiskCache = true;
		bool isAsync = true;
		bool isDump = false;
		bool hideError = false;
		bool useFileWatcher = false;

		std::stop_source ssource;
		std::mutex vertexShadersMutex;
		std::mutex pixelShadersMutex;
		std::mutex computeShadersMutex;
		CompilationSet compilationSet;
		std::unordered_map<std::string, ShaderCacheResult> shaderMap{};
		std::mutex mapMutex;
		std::unordered_map<std::string, system_clock::time_point> modifiedShaderMap{};  // hashmap when a shader source file last modified
		std::mutex modifiedMapMutex;

		// efsw file watcher
		efsw::FileWatcher* fileWatcher = nullptr;
		efsw::WatchID watchID;
		UpdateListener* listener = nullptr;
	};

	// Inherits from the abstract listener class, and implements the the file action handler
	class UpdateListener : public efsw::FileWatchListener
	{
	public:
		void UpdateCache(const std::filesystem::path& filePath, SIE::ShaderCache& cache, bool& clearCache, bool& retFlag);
		void processQueue();
		void handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename, efsw::Action action, std::string) override;

	private:
		struct fileAction
		{
			efsw::WatchID watchID;
			std::string dir;
			std::string filename;
			efsw::Action action;
			std::string oldFilename;
		};
		std::mutex actionMutex;
		std::vector<fileAction> queue{};
		size_t lastQueueSize = queue.size();
	};
}
