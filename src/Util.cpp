#include "Util.h"
#include "State.h"

#include <d3dcompiler.h>

namespace Util
{
	void StoreTransform3x4NoScale(DirectX::XMFLOAT3X4& Dest, const RE::NiTransform& Source)
	{
		//
		// Shove a Matrix3+Point3 directly into a float[3][4] with no modifications
		//
		// Dest[0][#] = Source.m_Rotate.m_pEntry[0][#];
		// Dest[0][3] = Source.m_Translate.x;
		// Dest[1][#] = Source.m_Rotate.m_pEntry[1][#];
		// Dest[1][3] = Source.m_Translate.x;
		// Dest[2][#] = Source.m_Rotate.m_pEntry[2][#];
		// Dest[2][3] = Source.m_Translate.x;
		//
		static_assert(sizeof(RE::NiTransform::rotate) == 3 * 3 * sizeof(float));  // NiMatrix3
		static_assert(sizeof(RE::NiTransform::translate) == 3 * sizeof(float));   // NiPoint3
		static_assert(offsetof(RE::NiTransform, translate) > offsetof(RE::NiTransform, rotate));

		_mm_store_ps(Dest.m[0], _mm_loadu_ps(Source.rotate.entry[0]));
		_mm_store_ps(Dest.m[1], _mm_loadu_ps(Source.rotate.entry[1]));
		_mm_store_ps(Dest.m[2], _mm_loadu_ps(Source.rotate.entry[2]));
		Dest.m[0][3] = Source.translate.x;
		Dest.m[1][3] = Source.translate.y;
		Dest.m[2][3] = Source.translate.z;
	}

	ID3D11ShaderResourceView* GetSRVFromRTV(const ID3D11RenderTargetView* a_rtv)
	{
		if (a_rtv) {
			if (auto r = RE::BSGraphics::Renderer::GetSingleton()) {
				for (int i = 0; i < RE::RENDER_TARGETS::kTOTAL; i++) {
					auto rt = r->GetRuntimeData().renderTargets[i];
					if (a_rtv == rt.RTV) {
						return rt.SRV;
					}
				}
			}
		}
		return nullptr;
	}

	ID3D11RenderTargetView* GetRTVFromSRV(const ID3D11ShaderResourceView* a_srv)
	{
		if (a_srv) {
			if (auto r = RE::BSGraphics::Renderer::GetSingleton()) {
				for (int i = 0; i < RE::RENDER_TARGETS::kTOTAL; i++) {
					auto rt = r->GetRuntimeData().renderTargets[i];
					if (a_srv == rt.SRV || a_srv == rt.SRVCopy) {
						return rt.RTV;
					}
				}
			}
		}
		return nullptr;
	}

	std::string GetNameFromSRV(const ID3D11ShaderResourceView* a_srv)
	{
		using RENDER_TARGET = RE::RENDER_TARGETS::RENDER_TARGET;

		if (a_srv) {
			if (auto r = RE::BSGraphics::Renderer::GetSingleton()) {
				for (int i = 0; i < RENDER_TARGET::kTOTAL; i++) {
					auto rt = r->GetRuntimeData().renderTargets[i];
					if (a_srv == rt.SRV || a_srv == rt.SRVCopy) {
						return std::string(magic_enum::enum_name(static_cast<RENDER_TARGET>(i)));
					}
				}
			}
		}
		return "NONE";
	}

	std::string GetNameFromRTV(const ID3D11RenderTargetView* a_rtv)
	{
		using RENDER_TARGET = RE::RENDER_TARGETS::RENDER_TARGET;
		if (a_rtv) {
			if (auto r = RE::BSGraphics::Renderer::GetSingleton()) {
				for (int i = 0; i < RENDER_TARGET::kTOTAL; i++) {
					auto rt = r->GetRuntimeData().renderTargets[i];
					if (a_rtv == rt.RTV) {
						return std::string(magic_enum::enum_name(static_cast<RENDER_TARGET>(i)));
					}
				}
			}
		}
		return "NONE";
	}

	GUID WKPDID_D3DDebugObjectNameT = { 0x429b8c22, 0x9188, 0x4b0c, 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 };

	void SetResourceName(ID3D11DeviceChild* Resource, const char* Format, ...)
	{
		if (!Resource)
			return;

		char buffer[1024];
		va_list va;

		va_start(va, Format);
		int len = _vsnprintf_s(buffer, _TRUNCATE, Format, va);
		va_end(va);

		Resource->SetPrivateData(WKPDID_D3DDebugObjectNameT, len, buffer);
	}

	ID3D11DeviceChild* CompileShader(const wchar_t* FilePath, const std::vector<std::pair<const char*, const char*>>& Defines, const char* ProgramType, const char* Program)
	{
		auto& device = State::GetSingleton()->device;

		// Build defines (aka convert vector->D3DCONSTANT array)
		std::vector<D3D_SHADER_MACRO> macros;

		for (auto& i : Defines)
			macros.push_back({ i.first, i.second });

		if (REL::Module::IsVR())
			macros.push_back({ "VR", "" });
		if (State::GetSingleton()->IsDeveloperMode()) {
			macros.push_back({ "D3DCOMPILE_SKIP_OPTIMIZATION", "" });
			macros.push_back({ "D3DCOMPILE_DEBUG", "" });
		}
		auto shaderDefines = State::GetSingleton()->GetDefines();
		if (!shaderDefines->empty()) {
			for (unsigned int i = 0; i < shaderDefines->size(); i++)
				macros.push_back({ shaderDefines->at(i).first.c_str(), shaderDefines->at(i).second.c_str() });
		}
		if (!_stricmp(ProgramType, "ps_5_0"))
			macros.push_back({ "PSHADER", "" });
		else if (!_stricmp(ProgramType, "vs_5_0"))
			macros.push_back({ "VSHADER", "" });
		else if (!_stricmp(ProgramType, "hs_5_0"))
			macros.push_back({ "HULLSHADER", "" });
		else if (!_stricmp(ProgramType, "ds_5_0"))
			macros.push_back({ "DOMAINSHADER", "" });
		else if (!_stricmp(ProgramType, "cs_5_0"))
			macros.push_back({ "COMPUTESHADER", "" });
		else if (!_stricmp(ProgramType, "cs_4_0"))
			macros.push_back({ "COMPUTESHADER", "" });
		else if (!_stricmp(ProgramType, "cs_5_1"))
			macros.push_back({ "COMPUTESHADER", "" });
		else
			return nullptr;

		// Add null terminating entry
		macros.push_back({ "WINPC", "" });
		macros.push_back({ "DX11", "" });
		macros.push_back({ nullptr, nullptr });

		// Compiler setup
		uint32_t flags = !State::GetSingleton()->IsDeveloperMode() ? (D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3) : D3DCOMPILE_DEBUG;

		ID3DBlob* shaderBlob;
		ID3DBlob* shaderErrors;

		std::string str;
		std::wstring path{ FilePath };
		std::transform(path.begin(), path.end(), std::back_inserter(str), [](wchar_t c) {
			return (char)c;
		});
		if (!std::filesystem::exists(FilePath)) {
			logger::error("Failed to compile shader; {} does not exist", str);
			return nullptr;
		}
		logger::debug("Compiling {} with {}", str, DefinesToString(macros));
		if (FAILED(D3DCompileFromFile(FilePath, macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, Program, ProgramType, flags, 0, &shaderBlob, &shaderErrors))) {
			logger::warn("Shader compilation failed:\n\n{}", shaderErrors ? static_cast<char*>(shaderErrors->GetBufferPointer()) : "Unknown error");
			return nullptr;
		}
		if (shaderErrors)
			logger::debug("Shader logs:\n{}", static_cast<char*>(shaderErrors->GetBufferPointer()));
		if (!_stricmp(ProgramType, "ps_5_0")) {
			ID3D11PixelShader* regShader;
			device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		} else if (!_stricmp(ProgramType, "vs_5_0")) {
			ID3D11VertexShader* regShader;
			device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		} else if (!_stricmp(ProgramType, "hs_5_0")) {
			ID3D11HullShader* regShader;
			device->CreateHullShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		} else if (!_stricmp(ProgramType, "ds_5_0")) {
			ID3D11DomainShader* regShader;
			device->CreateDomainShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		} else if (!_stricmp(ProgramType, "cs_5_0")) {
			ID3D11ComputeShader* regShader;
			DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader));
			return regShader;
		} else if (!_stricmp(ProgramType, "cs_4_0")) {
			ID3D11ComputeShader* regShader;
			DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader));
			return regShader;
		}

		return nullptr;
	}

	std::string DefinesToString(const std::vector<std::pair<const char*, const char*>>& defines)
	{
		std::string result;
		for (const auto& def : defines) {
			if (def.first != nullptr) {
				result += def.first;
				if (def.second != nullptr && !std::string(def.second).empty()) {
					result += "=";
					result += def.second;
				}
				result += ' ';
			} else {
				break;
			}
		}
		return result;
	}
	std::string DefinesToString(const std::vector<D3D_SHADER_MACRO>& defines)
	{
		std::string result;
		for (const auto& def : defines) {
			if (def.Name != nullptr) {
				result += def.Name;
				if (def.Definition != nullptr && !std::string(def.Definition).empty()) {
					result += "=";
					result += def.Definition;
				}
				result += ' ';
			} else {
				break;
			}
		}
		return result;
	}

	float4 TryGetWaterData(float offsetX, float offsetY)
	{
		if (RE::BSGraphics::RendererShadowState::GetSingleton()) {
			if (auto tes = RE::TES::GetSingleton()) {
				auto position = GetEyePosition(0);
				position.x += offsetX;
				position.y += offsetY;
				if (auto cell = tes->GetCell(position)) {
					float4 data = float4(1.0f, 1.0f, 1.0f, -FLT_MAX);

					bool extraCellWater = false;

					if (auto extraCellWaterType = cell->extraList.GetByType<RE::ExtraCellWaterType>()) {
						if (auto water = extraCellWaterType->water) {
							{
								data = { float(water->data.deepWaterColor.red) + float(water->data.shallowWaterColor.red),
									float(water->data.deepWaterColor.green) + float(water->data.shallowWaterColor.green),
									float(water->data.deepWaterColor.blue) + float(water->data.shallowWaterColor.blue) };

								data.x /= 255.0f;
								data.y /= 255.0f;
								data.z /= 255.0f;

								data.x *= 0.5;
								data.y *= 0.5;
								data.z *= 0.5;
								extraCellWater = true;
							}
						}
					}

					if (!extraCellWater) {
						if (auto worldSpace = tes->GetRuntimeData2().worldSpace) {
							if (auto water = worldSpace->worldWater) {
								data = { float(water->data.deepWaterColor.red) + float(water->data.shallowWaterColor.red),
									float(water->data.deepWaterColor.green) + float(water->data.shallowWaterColor.green),
									float(water->data.deepWaterColor.blue) + float(water->data.shallowWaterColor.blue) };

								data.x /= 255.0f;
								data.y /= 255.0f;
								data.z /= 255.0f;

								data.x *= 0.5;
								data.y *= 0.5;
								data.z *= 0.5;
							}
						}
					}

					if (auto sky = RE::Sky::GetSingleton()) {
						const auto& color = sky->skyColor[RE::TESWeather::ColorTypes::kWaterMultiplier];
						data.x *= color.red;
						data.y *= color.green;
						data.z *= color.blue;
					}

					data.w = cell->GetExteriorWaterHeight() - position.z;

					return data;
				}
			}
		}
		return float4(1.0f, 1.0f, 1.0f, -FLT_MAX);
	}

	void DumpSettingsOptions()
	{
		std::vector<RE::SettingCollectionList<RE::Setting>*> collections = {
			RE::INISettingCollection::GetSingleton(),
			RE::INIPrefSettingCollection::GetSingleton(),
		};
		auto count = 0;
		for (const auto& collection : collections) {
			for (const auto set : collection->settings)
				logger::info("Setting[{}] {}", count, set->GetName());
			count++;
		}
		auto game = RE::GameSettingCollection::GetSingleton();
		for (const auto& set : game->settings) {
			logger::info("Game Setting {} {}", set.first, set.second->GetName());
		}
	}

	float4 GetCameraData()
	{
		float4 cameraData{};
		if (auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator()) {
			if (accumulator->kCamera) {
				cameraData.x = accumulator->kCamera->GetRuntimeData2().viewFrustum.fFar;
				cameraData.y = accumulator->kCamera->GetRuntimeData2().viewFrustum.fNear;
				cameraData.z = accumulator->kCamera->GetRuntimeData2().viewFrustum.fFar - accumulator->kCamera->GetRuntimeData2().viewFrustum.fNear;
				cameraData.w = accumulator->kCamera->GetRuntimeData2().viewFrustum.fFar * accumulator->kCamera->GetRuntimeData2().viewFrustum.fNear;
			}
		}
		return cameraData;
	}

	float2 ConvertToDynamic(float2 size)
	{
		auto viewport = RE::BSGraphics::State::GetSingleton();

		return float2(
			size.x * viewport->GetRuntimeData().dynamicResolutionWidthRatio,
			size.y * viewport->GetRuntimeData().dynamicResolutionHeightRatio);
	}

	DispatchCount GetScreenDispatchCount()
	{
		float2 resolution = ConvertToDynamic(State::GetSingleton()->screenSize);
		uint dispatchX = (uint)std::ceil(resolution.x / 8.0f);
		uint dispatchY = (uint)std::ceil(resolution.y / 8.0f);

		return { dispatchX, dispatchY };
	}

	bool GetTemporal()
	{
		auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
		return (!REL::Module::IsVR() ? imageSpaceManager->GetRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled : imageSpaceManager->GetVRRuntimeData().BSImagespaceShaderISTemporalAA->taaEnabled) || State::GetSingleton()->upscalerLoaded;
	}

	HoverTooltipWrapper::HoverTooltipWrapper()
	{
		hovered = ImGui::IsItemHovered();
		if (hovered) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		}
	}

	HoverTooltipWrapper::~HoverTooltipWrapper()
	{
		if (hovered) {
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

namespace nlohmann
{
	void to_json(json& j, const float2& v)
	{
		j = json{ v.x, v.y };
	}

	void from_json(const json& j, float2& v)
	{
		std::array<float, 2> temp = j;
		v = { temp[0], temp[1] };
	}

	void to_json(json& j, const float3& v)
	{
		j = json{ v.x, v.y, v.z };
	}

	void from_json(const json& j, float3& v)
	{
		std::array<float, 3> temp = j;
		v = { temp[0], temp[1], temp[2] };
	}

	void to_json(json& j, const float4& v)
	{
		j = json{ v.x, v.y, v.z, v.w };
	}

	void from_json(const json& j, float4& v)
	{
		std::array<float, 4> temp = j;
		v = { temp[0], temp[1], temp[2], temp[3] };
	}
}