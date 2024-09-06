#include "Menu.h"
#include "Util.h"

#ifndef DIRECTINPUT_VERSION
#	define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <imgui_stdlib.h>
#include <magic_enum.hpp>

#include "ShaderCache.h"
#include "State.h"

#include "Feature.h"
#include "Features/LightLimitFix/ParticleLights.h"

#include "Deferred.h"
#include "TruePBR.h"

#include "VariableRateShading.h"

#define SETTING_MENU_TOGGLEKEY "Toggle Key"
#define SETTING_MENU_SKIPKEY "Skip Compilation Key"
#define SETTING_MENU_FONTSCALE "Font Scale"
#define SETTING_MENU_TOGGLE_SHADERS_KEY "Toggle Effects Key"

void SetupImGuiStyle()
{
	auto& style = ImGui::GetStyle();
	auto& colors = style.Colors;

	// Theme from https://github.com/ArranzCNL/ImprovedCameraSE-NG

	style.WindowTitleAlign = ImVec2(0.5, 0.5);
	style.FramePadding = ImVec2(4, 4);

	// Rounded slider grabber
	style.GrabRounding = 12.0f;

	// Window
	colors[ImGuiCol_WindowBg] = ImVec4{ 0.118f, 0.118f, 0.118f, 0.784f };
	colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.2f, 0.2f, 0.2f, 0.5f };
	colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.3f, 0.3f, 0.3f, 0.75f };
	colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };

	// Header
	colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
	colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.3f, 0.3f, 1.0f };
	colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
	colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };

	// Frame Background
	colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
	colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.3f, 0.3f, 1.0f };
	colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };

	// Button
	colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
	colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.3f, 0.3f, 1.0f };
	colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };

	// Tab
	colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
	colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.38f, 0.38f, 1.0f };
	colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.28f, 0.28f, 1.0f };
	colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
}

bool IsEnabled = false;
ImVec4 TextColor = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };

Menu::~Menu()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Menu::Load(json& o_json)
{
	if (o_json[SETTING_MENU_TOGGLEKEY].is_number_unsigned()) {
		toggleKey = o_json[SETTING_MENU_TOGGLEKEY];
	}
	if (o_json[SETTING_MENU_SKIPKEY].is_number_unsigned()) {
		skipCompilationKey = o_json[SETTING_MENU_SKIPKEY];
	}
	if (o_json[SETTING_MENU_FONTSCALE].is_number_float()) {
		fontScale = o_json[SETTING_MENU_FONTSCALE];
	}
	if (o_json[SETTING_MENU_TOGGLE_SHADERS_KEY].is_number_unsigned()) {
		effectToggleKey = o_json[SETTING_MENU_TOGGLE_SHADERS_KEY];
	}
}

void Menu::Save(json& o_json)
{
	json menu;
	menu[SETTING_MENU_TOGGLEKEY] = toggleKey;
	menu[SETTING_MENU_SKIPKEY] = skipCompilationKey;
	menu[SETTING_MENU_FONTSCALE] = fontScale;
	menu[SETTING_MENU_TOGGLE_SHADERS_KEY] = effectToggleKey;

	o_json["Menu"] = menu;
}

#define IM_VK_KEYPAD_ENTER (VK_RETURN + 256)

void Menu::Init(IDXGISwapChain* swapchain, ID3D11Device* device, ID3D11DeviceContext* context)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& imgui_io = ImGui::GetIO();

	imgui_io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	imgui_io.Fonts->AddFontFromFileTTF("Data\\Interface\\CommunityShaders\\Fonts\\Atkinson-Hyperlegible-Regular-102.ttf", 24);

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(desc.OutputWindow);
	ImGui_ImplDX11_Init(device, context);

	float trueScale = exp2(fontScale);
	auto& style = ImGui::GetStyle();
	style.ScaleAllSizes(trueScale);
	style.MouseCursorScale = 1.f;
	auto& io = ImGui::GetIO();
	io.FontGlobalScale = trueScale;
}

void Menu::DrawSettings()
{
	ImGuiStyle oldStyle = ImGui::GetStyle();
	SetupImGuiStyle();

	ImGui::DockSpaceOverViewport(NULL, ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::SetNextWindowSize({ 1000, 1000 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_FirstUseEver);
	if (ImGui::Begin(std::format("Skyrim Community Shaders {}", Plugin::VERSION.string(".")).c_str(), &IsEnabled)) {
		ImGui::PushStyleColor(ImGuiCol_Text, TextColor);
		auto& shaderCache = SIE::ShaderCache::Instance();

		if (ImGui::BeginTable("##LeButtons", 4, ImGuiTableFlags_SizingStretchSame)) {
			ImGui::TableNextColumn();
			if (ImGui::Button("Save Settings", { -1, 0 })) {
				State::GetSingleton()->Save();
			}

			ImGui::TableNextColumn();
			if (ImGui::Button("Load Settings", { -1, 0 })) {
				State::GetSingleton()->Load();
				ParticleLights::GetSingleton()->GetConfigs();
			}

			ImGui::TableNextColumn();
			if (ImGui::Button("Clear Shader Cache", { -1, 0 })) {
				shaderCache.Clear();
				Deferred::GetSingleton()->ClearShaderCache();
				VariableRateShading::GetSingleton()->ClearShaderCache();
				for (auto* feature : Feature::GetFeatureList()) {
					if (feature->loaded) {
						feature->ClearShaderCache();
					}
				}
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"The Shader Cache is the collection of compiled shaders which replace the vanilla shaders at runtime. "
					"Clearing the shader cache will mean that shaders are recompiled only when the game re-encounters them. "
					"This is only needed for hot-loading shaders for development purposes. ");
			}

			ImGui::TableNextColumn();
			if (ImGui::Button("Clear Disk Cache", { -1, 0 })) {
				shaderCache.DeleteDiskCache();
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"The Disk Cache is a collection of compiled shaders on disk, which are automatically created when shaders are added to the Shader Cache. "
					"If you do not have a Disk Cache, or it is outdated or invalid, you will see \"Compiling Shaders\" in the upper-left corner. "
					"After this has completed you will no longer see this message apart from when loading from the Disk Cache. "
					"Only delete the Disk Cache manually if you are encountering issues. ");
			}

			if (shaderCache.GetFailedTasks()) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (ImGui::Button("Toggle Error Message", { -1, 0 })) {
					shaderCache.ToggleErrorMessages();
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text(
						"Hide or show the shader failure message. "
						"Your installation is broken and will likely see errors in game. "
						"Please double check you have updated all features and that your load order is correct. "
						"See CommunityShaders.log for details and check the NexusMods page or Discord server. ");
				}
			}
			ImGui::EndTable();
		}

		ImGui::Spacing();

		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			bool useCustomShaders = shaderCache.IsEnabled();
			if (ImGui::BeginTable("##GeneralToggles", 3, ImGuiTableFlags_SizingStretchSame)) {
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Enable Shaders", &useCustomShaders)) {
					shaderCache.SetEnabled(useCustomShaders);
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text("Disabling this effectively disables all features.");
				}

				bool useDiskCache = shaderCache.IsDiskCache();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Enable Disk Cache", &useDiskCache)) {
					shaderCache.SetDiskCache(useDiskCache);
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text("Disabling this stops shaders from being loaded from disk, as well as stops shaders from being saved to it.");
				}

				bool useAsync = shaderCache.IsAsync();
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Enable Async", &useAsync)) {
					shaderCache.SetAsync(useAsync);
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text("Skips a shader being replaced if it hasn't been compiled yet. Also makes compilation blazingly fast!");
				}

				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("Menu", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			if (settingToggleKey) {
				ImGui::Text("Press any key to set as toggle key...");
			} else {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Toggle Key:");
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", KeyIdToString(toggleKey));

				ImGui::AlignTextToFramePadding();
				ImGui::SameLine();
				if (ImGui::Button("Change##toggle")) {
					settingToggleKey = true;
				}
			}
			if (settingsEffectsToggle) {
				ImGui::Text("Press any key to set as a toggle key for all effects...");
			} else {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Effect Toggle Key:");
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", KeyIdToString(effectToggleKey));

				ImGui::AlignTextToFramePadding();
				ImGui::SameLine();
				if (ImGui::Button("Change##EffectToggle")) {
					settingsEffectsToggle = true;
				}
			}
			if (settingSkipCompilationKey) {
				ImGui::Text("Press any key to set as Skip Compilation Key...");
			} else {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Skip Compilation Key:");
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", KeyIdToString(skipCompilationKey));

				ImGui::AlignTextToFramePadding();
				ImGui::SameLine();
				if (ImGui::Button("Change##skip")) {
					settingSkipCompilationKey = true;
				}
			}

			if (ImGui::SliderFloat("Font Scale", &fontScale, -2.f, 2.f, "%.2f")) {
				float trueScale = exp2(fontScale);
				auto& style = ImGui::GetStyle();
				style.ScaleAllSizes(trueScale);
				style.MouseCursorScale = 1.f;
				auto& io = ImGui::GetIO();
				io.FontGlobalScale = trueScale;
			}
		}

		if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			bool useDump = shaderCache.IsDump();
			if (ImGui::Checkbox("Dump Shaders", &useDump)) {
				shaderCache.SetDump(useDump);
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text("Dump shaders at startup. This should be used only when reversing shaders. Normal users don't need this.");
			}
			spdlog::level::level_enum logLevel = State::GetSingleton()->GetLogLevel();
			const char* items[] = {
				"trace",
				"debug",
				"info",
				"warn",
				"err",
				"critical",
				"off"
			};
			static int item_current = static_cast<int>(logLevel);
			if (ImGui::Combo("Log Level", &item_current, items, IM_ARRAYSIZE(items))) {
				ImGui::SameLine();
				State::GetSingleton()->SetLogLevel(static_cast<spdlog::level::level_enum>(item_current));
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text("Log level. Trace is most verbose. Default is info.");
			}

			auto& shaderDefines = State::GetSingleton()->shaderDefinesString;
			if (ImGui::InputText("Shader Defines", &shaderDefines)) {
				State::GetSingleton()->SetDefines(shaderDefines);
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text("Defines for Shader Compiler. Semicolon \";\" separated. Clear with space. Rebuild shaders after making change. Compute Shaders require a restart to recompile.");
			}
			ImGui::Spacing();
			ImGui::SliderInt("Compiler Threads", &shaderCache.compilationThreadCount, 1, static_cast<int32_t>(std::thread::hardware_concurrency()));
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"Number of threads to use to compile shaders. "
					"The more threads the faster compilation will finish but may make the system unresponsive. ");
			}
			ImGui::SliderInt("Background Compiler Threads", &shaderCache.backgroundCompilationThreadCount, 1, static_cast<int32_t>(std::thread::hardware_concurrency()));
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"Number of threads to use to compile shaders while playing game. "
					"This is activated if the startup compilation is skipped. "
					"The more threads the faster compilation will finish but may make the system unresponsive. ");
			}

			if (ImGui::SliderInt("Test Interval", reinterpret_cast<int*>(&testInterval), 0, 10)) {
				if (testInterval == 0) {
					inTestMode = false;
					logger::info("Disabling test mode.");
					State::GetSingleton()->Load(State::ConfigMode::TEST);  // restore last settings before entering test mode
				} else if (!inTestMode) {
					logger::info("Saving current settings for test mode and starting test with interval {}.", testInterval);
					State::GetSingleton()->Save(State::ConfigMode::TEST);
					inTestMode = true;
				} else {
					logger::info("Setting new interval {}.", testInterval);
				}
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"Sets number of seconds before toggling between default USER and TEST config. "
					"0 disables. Non-zero will enable testing mode. "
					"Enabling will save current settings as TEST config. "
					"This has no impact if no settings are changed. ");
			}
			bool useFileWatcher = shaderCache.UseFileWatcher();
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("Enable File Watcher", &useFileWatcher)) {
				shaderCache.SetFileWatcher(useFileWatcher);
			}
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					"Automatically recompile shaders on file change. "
					"Intended for developing.");
			}

			if (ImGui::Button("Dump Ini Settings", { -1, 0 })) {
				Util::DumpSettingsOptions();
			}
			if (!shaderCache.blockedKey.empty()) {
				auto blockingButtonString = std::format("Stop Blocking {} Shaders", shaderCache.blockedIDs.size());
				if (ImGui::Button(blockingButtonString.c_str(), { -1, 0 })) {
					shaderCache.DisableShaderBlocking();
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text(
						"Stop blocking Community Shaders shader. "
						"Blocking is helpful when debugging shader errors in game to determine which shader has issues. "
						"Blocking is enabled if in developer mode and pressing PAGEUP and PAGEDOWN. "
						"Specific shader will be printed to logfile. ");
				}
			}
			if (ImGui::TreeNodeEx("Addresses")) {
				auto Renderer = RE::BSGraphics::Renderer::GetSingleton();
				auto BSShaderAccumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
				auto RendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
				ADDRESS_NODE(Renderer)
				ADDRESS_NODE(BSShaderAccumulator)
				ADDRESS_NODE(RendererShadowState)
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text(std::format("Shader Compiler : {}", shaderCache.GetShaderStatsString()).c_str());
				ImGui::TreePop();
			}
			ImGui::Checkbox("Extended Frame Annotations", &State::GetSingleton()->extendedFrameAnnotations);
			TruePBR::GetSingleton()->DrawSettings();
		}

		if (ImGui::CollapsingHeader("Replace Original Shaders", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			auto state = State::GetSingleton();
			if (ImGui::BeginTable("##ReplaceToggles", 3, ImGuiTableFlags_SizingStretchSame)) {
				for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
					ImGui::TableNextColumn();

					auto type = (RE::BSShader::Type)(classIndex + 1);
					if (!(SIE::ShaderCache::IsSupportedShader(type) || state->IsDeveloperMode())) {
						ImGui::BeginDisabled();
						ImGui::Checkbox(std::format("{}", magic_enum::enum_name(type)).c_str(), &state->enabledClasses[classIndex]);
						ImGui::EndDisabled();
					} else
						ImGui::Checkbox(std::format("{}", magic_enum::enum_name(type)).c_str(), &state->enabledClasses[classIndex]);
				}
				if (state->IsDeveloperMode()) {
					ImGui::Checkbox("Vertex", &state->enableVShaders);
					if (auto _tt = Util::HoverTooltipWrapper()) {
						ImGui::Text(
							"Replace Vertex Shaders. "
							"When false, will disable the custom Vertex Shaders for the types above. "
							"For developers to test whether CS shaders match vanilla behavior. ");
					}
					ImGui::Checkbox("Pixel", &state->enablePShaders);
					if (auto _tt = Util::HoverTooltipWrapper()) {
						ImGui::Text(
							"Replace Pixel Shaders. "
							"When false, will disable the custom Pixel Shaders for the types above. "
							"For developers to test whether CS shaders match vanilla behavior. ");
					}
					ImGui::Checkbox("Compute", &state->enableCShaders);
					if (auto _tt = Util::HoverTooltipWrapper()) {
						ImGui::Text(
							"Replace Compute Shaders. "
							"When false, will disable the custom Compute Shaders for the types above. "
							"For developers to test whether CS shaders match vanilla behavior. ");
					}
				}
				ImGui::EndTable();
			}
		}

		ImGui::Separator();

		if (ImGui::BeginTable("Feature Table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("##ListOfFeatures", 0, 3);
			ImGui::TableSetupColumn("##FeatureConfig", 0, 7);

			static size_t selectedFeature = SIZE_T_MAX;
			auto& featureList = Feature::GetFeatureList();
			auto sortedList{ featureList };  // need a copy so the load order is not lost
			std::sort(sortedList.begin(), sortedList.end(), [](Feature* a, Feature* b) {
				return a->GetName() < b->GetName();
			});

			ImGui::TableNextColumn();
			if (ImGui::BeginListBox("##FeatureList", { -FLT_MIN, -FLT_MIN })) {
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));  // Selected feature header color
				for (size_t i = 0; i < sortedList.size(); i++)
					if (sortedList[i]->loaded) {
						if (ImGui::Selectable(fmt::format(" {} ", sortedList[i]->GetName()).c_str(), selectedFeature == i, ImGuiSelectableFlags_SpanAllColumns))
							selectedFeature = i;
						ImGui::SameLine();
						ImGui::TextDisabled(fmt::format("({})", sortedList[i]->version).c_str());
					} else if (!sortedList[i]->version.empty()) {
						ImGui::TextDisabled(fmt::format(" {} ({})", sortedList[i]->GetName(), sortedList[i]->version).c_str());
						if (auto _tt = Util::HoverTooltipWrapper()) {
							ImGui::Text(sortedList[i]->failedLoadedMessage.c_str());
						}
					}
				ImGui::PopStyleColor();
				ImGui::EndListBox();
			}

			ImGui::TableNextColumn();

			bool shownFeature = selectedFeature < sortedList.size();
			if (shownFeature) {
				if (ImGui::Button("Restore Defaults", { -1, 0 })) {
					sortedList[selectedFeature]->RestoreDefaultSettings();
				}
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text(
						"Restores the feature's settings back to their default values. "
						"You will still need to Save Settings to make these changes permanent. ");
				}
			}

			if (ImGui::BeginChild("##FeatureConfigFrame", { 0, 0 }, true)) {
				if (shownFeature)
					sortedList[selectedFeature]->DrawSettings();
				else
					ImGui::TextDisabled("Please select a feature on the left.");
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}
		ImGui::PopStyleColor(1);
	}

	ImGui::End();
	ImGuiStyle& style = ImGui::GetStyle();
	style = oldStyle;

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Menu::DrawOverlay()
{
	ProcessInputEventQueue();  //Synchronize Inputs to frame

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	uint64_t totalShaders = 0;
	uint64_t compiledShaders = 0;

	auto& shaderCache = SIE::ShaderCache::Instance();

	compiledShaders = shaderCache.GetCompletedTasks();
	totalShaders = shaderCache.GetTotalTasks();

	auto state = State::GetSingleton();

	auto failed = shaderCache.GetFailedTasks();
	auto hide = shaderCache.IsHideErrors();
	auto progressTitle = fmt::format("{}Compiling Shaders: {}",
		shaderCache.backgroundCompilation ? "Background " : "",
		shaderCache.GetShaderStatsString(!state->IsDeveloperMode()).c_str());
	auto percent = (float)compiledShaders / (float)totalShaders;
	auto progressOverlay = fmt::format("{}/{} ({:2.1f}%)", compiledShaders, totalShaders, 100 * percent);
	if (shaderCache.IsCompiling()) {
		ImGui::SetNextWindowBgAlpha(1);
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		if (!ImGui::Begin("ShaderCompilationInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
			ImGui::End();
			return;
		}
		ImGui::TextUnformatted(progressTitle.c_str());
		ImGui::ProgressBar(percent, ImVec2(0.0f, 0.0f), progressOverlay.c_str());
		if (!shaderCache.backgroundCompilation && shaderCache.menuLoaded) {
			auto skipShadersText = fmt::format(
				"Press {} to proceed without completing shader compilation. "
				"WARNING: Uncompiled shaders will have visual errors or cause stuttering when loading.",
				KeyIdToString(skipCompilationKey));
			ImGui::TextUnformatted(skipShadersText.c_str());
		}

		ImGui::End();
	} else if (failed) {
		TextColor = ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f };
		if (!hide) {
			ImGui::SetNextWindowBgAlpha(1);
			ImGui::SetNextWindowPos(ImVec2(10, 10));
			if (!ImGui::Begin("ShaderCompilationInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
				ImGui::End();
				return;
			}

			ImGui::TextColored(TextColor, "ERROR: %d shaders failed to compile. Check installation and CommunityShaders.log", failed, totalShaders);
			ImGui::End();
		}
	} else {
		// Done compiling with no failures
		TextColor = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
	}

	if (IsEnabled) {
		ImGui::GetIO().MouseDrawCursor = true;
		DrawSettings();
	} else {
		ImGui::GetIO().MouseDrawCursor = false;
	}

	if (inTestMode) {  // In test mode
		float seconds = (float)duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - lastTestSwitch).count() / 1000;
		auto remaining = (float)testInterval - seconds;
		if (remaining < 0) {
			usingTestConfig = !usingTestConfig;
			logger::info("Swapping mode to {}", usingTestConfig ? "test" : "user");
			State::GetSingleton()->Load(usingTestConfig ? State::ConfigMode::TEST : State::ConfigMode::USER);
			lastTestSwitch = high_resolution_clock::now();
		}
		ImGui::SetNextWindowBgAlpha(1);
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		if (!ImGui::Begin("Testing", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
			ImGui::End();
			return;
		}
		ImGui::Text(fmt::format("{} Mode : {:.1f} seconds left", usingTestConfig ? "Test" : "User", remaining).c_str());
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

const ImGuiKey Menu::VirtualKeyToImGuiKey(WPARAM vkKey)
{
	switch (vkKey) {
	case VK_TAB:
		return ImGuiKey_Tab;
	case VK_LEFT:
		return ImGuiKey_LeftArrow;
	case VK_RIGHT:
		return ImGuiKey_RightArrow;
	case VK_UP:
		return ImGuiKey_UpArrow;
	case VK_DOWN:
		return ImGuiKey_DownArrow;
	case VK_PRIOR:
		return ImGuiKey_PageUp;
	case VK_NEXT:
		return ImGuiKey_PageDown;
	case VK_HOME:
		return ImGuiKey_Home;
	case VK_END:
		return ImGuiKey_End;
	case VK_INSERT:
		return ImGuiKey_Insert;
	case VK_DELETE:
		return ImGuiKey_Delete;
	case VK_BACK:
		return ImGuiKey_Backspace;
	case VK_SPACE:
		return ImGuiKey_Space;
	case VK_RETURN:
		return ImGuiKey_Enter;
	case VK_ESCAPE:
		return ImGuiKey_Escape;
	case VK_OEM_7:
		return ImGuiKey_Apostrophe;
	case VK_OEM_COMMA:
		return ImGuiKey_Comma;
	case VK_OEM_MINUS:
		return ImGuiKey_Minus;
	case VK_OEM_PERIOD:
		return ImGuiKey_Period;
	case VK_OEM_2:
		return ImGuiKey_Slash;
	case VK_OEM_1:
		return ImGuiKey_Semicolon;
	case VK_OEM_PLUS:
		return ImGuiKey_Equal;
	case VK_OEM_4:
		return ImGuiKey_LeftBracket;
	case VK_OEM_5:
		return ImGuiKey_Backslash;
	case VK_OEM_6:
		return ImGuiKey_RightBracket;
	case VK_OEM_3:
		return ImGuiKey_GraveAccent;
	case VK_CAPITAL:
		return ImGuiKey_CapsLock;
	case VK_SCROLL:
		return ImGuiKey_ScrollLock;
	case VK_NUMLOCK:
		return ImGuiKey_NumLock;
	case VK_SNAPSHOT:
		return ImGuiKey_PrintScreen;
	case VK_PAUSE:
		return ImGuiKey_Pause;
	case VK_NUMPAD0:
		return ImGuiKey_Keypad0;
	case VK_NUMPAD1:
		return ImGuiKey_Keypad1;
	case VK_NUMPAD2:
		return ImGuiKey_Keypad2;
	case VK_NUMPAD3:
		return ImGuiKey_Keypad3;
	case VK_NUMPAD4:
		return ImGuiKey_Keypad4;
	case VK_NUMPAD5:
		return ImGuiKey_Keypad5;
	case VK_NUMPAD6:
		return ImGuiKey_Keypad6;
	case VK_NUMPAD7:
		return ImGuiKey_Keypad7;
	case VK_NUMPAD8:
		return ImGuiKey_Keypad8;
	case VK_NUMPAD9:
		return ImGuiKey_Keypad9;
	case VK_DECIMAL:
		return ImGuiKey_KeypadDecimal;
	case VK_DIVIDE:
		return ImGuiKey_KeypadDivide;
	case VK_MULTIPLY:
		return ImGuiKey_KeypadMultiply;
	case VK_SUBTRACT:
		return ImGuiKey_KeypadSubtract;
	case VK_ADD:
		return ImGuiKey_KeypadAdd;
	case IM_VK_KEYPAD_ENTER:
		return ImGuiKey_KeypadEnter;
	case VK_LSHIFT:
		return ImGuiKey_LeftShift;
	case VK_LCONTROL:
		return ImGuiKey_LeftCtrl;
	case VK_LMENU:
		return ImGuiKey_LeftAlt;
	case VK_LWIN:
		return ImGuiKey_LeftSuper;
	case VK_RSHIFT:
		return ImGuiKey_RightShift;
	case VK_RCONTROL:
		return ImGuiKey_RightCtrl;
	case VK_RMENU:
		return ImGuiKey_RightAlt;
	case VK_RWIN:
		return ImGuiKey_RightSuper;
	case VK_APPS:
		return ImGuiKey_Menu;
	case '0':
		return ImGuiKey_0;
	case '1':
		return ImGuiKey_1;
	case '2':
		return ImGuiKey_2;
	case '3':
		return ImGuiKey_3;
	case '4':
		return ImGuiKey_4;
	case '5':
		return ImGuiKey_5;
	case '6':
		return ImGuiKey_6;
	case '7':
		return ImGuiKey_7;
	case '8':
		return ImGuiKey_8;
	case '9':
		return ImGuiKey_9;
	case 'A':
		return ImGuiKey_A;
	case 'B':
		return ImGuiKey_B;
	case 'C':
		return ImGuiKey_C;
	case 'D':
		return ImGuiKey_D;
	case 'E':
		return ImGuiKey_E;
	case 'F':
		return ImGuiKey_F;
	case 'G':
		return ImGuiKey_G;
	case 'H':
		return ImGuiKey_H;
	case 'I':
		return ImGuiKey_I;
	case 'J':
		return ImGuiKey_J;
	case 'K':
		return ImGuiKey_K;
	case 'L':
		return ImGuiKey_L;
	case 'M':
		return ImGuiKey_M;
	case 'N':
		return ImGuiKey_N;
	case 'O':
		return ImGuiKey_O;
	case 'P':
		return ImGuiKey_P;
	case 'Q':
		return ImGuiKey_Q;
	case 'R':
		return ImGuiKey_R;
	case 'S':
		return ImGuiKey_S;
	case 'T':
		return ImGuiKey_T;
	case 'U':
		return ImGuiKey_U;
	case 'V':
		return ImGuiKey_V;
	case 'W':
		return ImGuiKey_W;
	case 'X':
		return ImGuiKey_X;
	case 'Y':
		return ImGuiKey_Y;
	case 'Z':
		return ImGuiKey_Z;
	case VK_F1:
		return ImGuiKey_F1;
	case VK_F2:
		return ImGuiKey_F2;
	case VK_F3:
		return ImGuiKey_F3;
	case VK_F4:
		return ImGuiKey_F4;
	case VK_F5:
		return ImGuiKey_F5;
	case VK_F6:
		return ImGuiKey_F6;
	case VK_F7:
		return ImGuiKey_F7;
	case VK_F8:
		return ImGuiKey_F8;
	case VK_F9:
		return ImGuiKey_F9;
	case VK_F10:
		return ImGuiKey_F10;
	case VK_F11:
		return ImGuiKey_F11;
	case VK_F12:
		return ImGuiKey_F12;
	default:
		return ImGuiKey_None;
	};
}

inline const uint32_t Menu::DIKToVK(uint32_t DIK)
{
	switch (DIK) {
	case DIK_LEFTARROW:
		return VK_LEFT;
	case DIK_RIGHTARROW:
		return VK_RIGHT;
	case DIK_UPARROW:
		return VK_UP;
	case DIK_DOWNARROW:
		return VK_DOWN;
	case DIK_DELETE:
		return VK_DELETE;
	case DIK_END:
		return VK_END;
	case DIK_HOME:
		return VK_HOME;  // pos1
	case DIK_PRIOR:
		return VK_PRIOR;  // page up
	case DIK_NEXT:
		return VK_NEXT;  // page down
	case DIK_INSERT:
		return VK_INSERT;
	case DIK_NUMPAD0:
		return VK_NUMPAD0;
	case DIK_NUMPAD1:
		return VK_NUMPAD1;
	case DIK_NUMPAD2:
		return VK_NUMPAD2;
	case DIK_NUMPAD3:
		return VK_NUMPAD3;
	case DIK_NUMPAD4:
		return VK_NUMPAD4;
	case DIK_NUMPAD5:
		return VK_NUMPAD5;
	case DIK_NUMPAD6:
		return VK_NUMPAD6;
	case DIK_NUMPAD7:
		return VK_NUMPAD7;
	case DIK_NUMPAD8:
		return VK_NUMPAD8;
	case DIK_NUMPAD9:
		return VK_NUMPAD9;
	case DIK_DECIMAL:
		return VK_DECIMAL;
	case DIK_NUMPADENTER:
		return IM_VK_KEYPAD_ENTER;
	case DIK_RMENU:
		return VK_RMENU;  // right alt
	case DIK_RCONTROL:
		return VK_RCONTROL;  // right control
	case DIK_LWIN:
		return VK_LWIN;  // left win
	case DIK_RWIN:
		return VK_RWIN;  // right win
	case DIK_APPS:
		return VK_APPS;
	default:
		return DIK;
	}
}

void Menu::ProcessInputEventQueue()
{
	std::unique_lock<std::shared_mutex> mutex(_inputEventMutex);
	ImGuiIO& io = ImGui::GetIO();

	for (auto& event : _keyEventQueue) {
		if (event.eventType == RE::INPUT_EVENT_TYPE::kChar) {
			io.AddInputCharacter(event.keyCode);
			continue;
		}

		if (event.device == RE::INPUT_DEVICE::kMouse) {
			logger::trace("Detect mouse scan code {} value {} pressed: {}", event.keyCode, event.value, event.IsPressed());
			if (event.keyCode > 7) {  // middle scroll
				io.AddMouseWheelEvent(0, event.value * (event.keyCode == 8 ? 1 : -1));
			} else {
				if (event.keyCode > 5)
					event.keyCode = 5;
				io.AddMouseButtonEvent(event.keyCode, event.IsPressed());
			}
		}

		if (event.device == RE::INPUT_DEVICE::kKeyboard) {
			uint32_t key = DIKToVK(event.keyCode);
			logger::trace("Detected key code {} ({})", event.keyCode, key);
			if (key == event.keyCode)
				key = MapVirtualKeyEx(event.keyCode, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
			if (!event.IsPressed()) {
				if (settingToggleKey) {
					toggleKey = key;
					settingToggleKey = false;
				} else if (settingSkipCompilationKey) {
					skipCompilationKey = key;
					settingSkipCompilationKey = false;
				} else if (settingsEffectsToggle) {
					effectToggleKey = key;
					settingsEffectsToggle = false;
				} else if (key == toggleKey) {
					IsEnabled = !IsEnabled;
				} else if (key == skipCompilationKey) {
					auto& shaderCache = SIE::ShaderCache::Instance();
					shaderCache.backgroundCompilation = true;
				} else if (key == effectToggleKey) {
					auto& shaderCache = SIE::ShaderCache::Instance();
					shaderCache.SetEnabled(!shaderCache.IsEnabled());
				} else if (key == priorShaderKey && State::GetSingleton()->IsDeveloperMode()) {
					auto& shaderCache = SIE::ShaderCache::Instance();
					shaderCache.IterateShaderBlock();
				} else if (key == nextShaderKey && State::GetSingleton()->IsDeveloperMode()) {
					auto& shaderCache = SIE::ShaderCache::Instance();
					shaderCache.IterateShaderBlock(false);
				}
			}

			io.AddKeyEvent(VirtualKeyToImGuiKey(key), event.IsPressed());

			if (key == VK_LCONTROL || key == VK_RCONTROL)
				io.AddKeyEvent(ImGuiMod_Ctrl, event.IsPressed());
			else if (key == VK_LSHIFT || key == VK_RSHIFT)
				io.AddKeyEvent(ImGuiMod_Shift, event.IsPressed());
			else if (key == VK_LMENU || key == VK_RMENU)
				io.AddKeyEvent(ImGuiMod_Alt, event.IsPressed());
		}
	}

	_keyEventQueue.clear();
}

void Menu::addToEventQueue(KeyEvent e)
{
	std::unique_lock<std::shared_mutex> mutex(_inputEventMutex);
	_keyEventQueue.emplace_back(e);
}

void Menu::OnFocusLost()
{
	std::unique_lock<std::shared_mutex> mutex(_inputEventMutex);
	_keyEventQueue.clear();
}

void Menu::ProcessInputEvents(RE::InputEvent* const* a_events)
{
	for (auto it = *a_events; it; it = it->next) {
		if (it->GetEventType() != RE::INPUT_EVENT_TYPE::kButton && it->GetEventType() != RE::INPUT_EVENT_TYPE::kChar)  // we do not care about non button or char events
			continue;

		auto event = it->GetEventType() == RE::INPUT_EVENT_TYPE::kButton ? KeyEvent(static_cast<RE::ButtonEvent*>(it)) : KeyEvent(static_cast<CharEvent*>(it));

		addToEventQueue(event);
	}
}

bool Menu::ShouldSwallowInput()
{
	return IsEnabled;
}

const char* Menu::KeyIdToString(uint32_t key)
{
	if (key >= 256)
		return "";

	static const char* keyboard_keys_international[256] = {
		"", "Left Mouse", "Right Mouse", "Cancel", "Middle Mouse", "X1 Mouse", "X2 Mouse", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
		"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
		"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
		"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
		"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "Apps", "", "Sleep",
		"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
		"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
		"Num Lock", "Scroll Lock", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"Left Shift", "Right Shift", "Left Control", "Right Control", "Left Menu", "Right Menu", "Browser Back", "Browser Forward", "Browser Refresh", "Browser Stop", "Browser Search", "Browser Favorites", "Browser Home", "Volume Mute", "Volume Down", "Volume Up",
		"Next Track", "Previous Track", "Media Stop", "Media Play/Pause", "Mail", "Media Select", "Launch App 1", "Launch App 2", "", "", "OEM ;", "OEM +", "OEM ,", "OEM -", "OEM .", "OEM /",
		"OEM ~", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "", "", "", "", "", "OEM [", "OEM \\", "OEM ]", "OEM '", "OEM 8",
		"", "", "OEM <", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "Attn", "CrSel", "ExSel", "Erase EOF", "Play", "Zoom", "", "PA1", "OEM Clear", ""
	};

	return keyboard_keys_international[key];
}
