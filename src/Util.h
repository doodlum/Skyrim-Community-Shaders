#pragma once

/**
 @def GET_INSTANCE_MEMBER
 @brief Set variable in current namespace based on instance member from GetRuntimeData or GetVRRuntimeData.
  
 @warning The class must have both a GetRuntimeData() and GetVRRuntimeData() function.
  
 @param a_value The instance member value to access (e.g., renderTargets).
 @param a_source The instance of the class (e.g., state).
 @result The a_value will be set as a variable in the current namespace. (e.g., auto& renderTargets = state->renderTargets;)
 */
#define GET_INSTANCE_MEMBER(a_value, a_source) \
	auto& a_value = !REL::Module::IsVR() ? a_source->GetRuntimeData().a_value : a_source->GetVRRuntimeData().a_value;

namespace Util
{
	void StoreTransform3x4NoScale(DirectX::XMFLOAT3X4& Dest, const RE::NiTransform& Source);
	ID3D11ShaderResourceView* GetSRVFromRTV(ID3D11RenderTargetView* a_rtv);
	ID3D11RenderTargetView* GetRTVFromSRV(ID3D11ShaderResourceView* a_srv);
	std::string GetNameFromSRV(ID3D11ShaderResourceView* a_srv);
	std::string GetNameFromRTV(ID3D11RenderTargetView* a_rtv);
	void SetResourceName(ID3D11DeviceChild* Resource, const char* Format, ...);
	ID3D11DeviceChild* CompileShader(const wchar_t* FilePath, const std::vector<std::pair<const char*, const char*>>& Defines, const char* ProgramType, const char* Program = "main");
	std::string DefinesToString(std::vector<std::pair<const char*, const char*>>& defines);
	std::string DefinesToString(std::vector<D3D_SHADER_MACRO>& defines);
	float TryGetWaterHeight(float offsetX, float offsetY);
	void DumpSettingsOptions();
	float4 GetCameraData();

	/**
	 * Usage:
	 * if (auto _tt = Util::HoverTooltipWrapper()){
	 *     ImGui::Text("What the tooltip says.");
	 * }
	*/
	class HoverTooltipWrapper
	{
	private:
		bool hovered;

	public:
		HoverTooltipWrapper();
		~HoverTooltipWrapper();
		inline operator bool() { return hovered; }
	};

	/**
	 * Usage:
	 * static FrameChecker frame_checker;
	 * if(frame_checker.isNewFrame())
	 *     ...
	*/
	class FrameChecker
	{
	private:
		uint32_t last_frame = UINT32_MAX;

	public:
		inline bool isNewFrame(uint32_t frame)
		{
			bool retval = last_frame != frame;
			last_frame = frame;
			return retval;
		}
		inline bool isNewFrame() { return isNewFrame(RE::BSGraphics::State::GetSingleton()->uiFrameCount); }
	};

	// for simple benchmarking
	struct CountedTimer
	{
		std::chrono::system_clock::time_point start_time;

		unsigned long long count = 0u;
		unsigned long long timer = 0u;

		inline float avgTime() const { return timer / (float)count; }
		inline void add(unsigned long long t)
		{
			timer += t;
			count++;
		}

		inline void start() { start_time = std::chrono::system_clock::now(); }
		inline void end() { add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - start_time).count()); }

		template <typename T>
		T run(std::function<T()> func)
		{
			if constexpr (std::is_same_v<T, void>) {
				start();
				func();
				end();
			} else {
				start();
				T retval = func();
				end();
				return retval;
			}
		}
	};
}

namespace nlohmann
{
	void to_json(json& j, const float2& v);
	void from_json(const json& j, float2& v);
	void to_json(json& j, const float3& v);
	void from_json(const json& j, float3& v);
	void to_json(json& j, const float4& v);
	void from_json(const json& j, float4& v);
};