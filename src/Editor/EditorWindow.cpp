#include "EditorWindow.h"

#include "State.h"
#include "features/Weather.h"

bool ContainsStringIgnoreCase(const std::string_view a_string, const std::string_view a_substring)
{
	const auto it = std::ranges::search(a_string, a_substring, [](const char a_a, const char a_b) {
		return std::tolower(a_a) == std::tolower(a_b);
	}).begin();
	return it != a_string.end();
}

void TextUnformattedDisabled(const char* a_text, const char* a_textEnd = nullptr)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextUnformatted(a_text, a_textEnd);
	ImGui::PopStyleColor();
}

void AddTooltip(const char* a_desc, ImGuiHoveredFlags a_flags = ImGuiHoveredFlags_DelayNormal)
{
	if (ImGui::IsItemHovered(a_flags)) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
		if (ImGui::BeginTooltip()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
			ImGui::TextUnformatted(a_desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		ImGui::PopStyleVar();
	}
}

inline void HelpMarker(const char* a_desc)
{
	ImGui::AlignTextToFramePadding();
	TextUnformattedDisabled("(?)");
	AddTooltip(a_desc, ImGuiHoveredFlags_DelayShort);
}

void EditorWindow::ShowObjectsWindow()
{
	ImGui::Begin("Object List");

	// Static variable to track the selected category
	static std::string selectedCategory = "Lighting Template";

	// Static variable for filtering objects
	static char filterBuffer[256] = "";

	// Create a table with two columns
	if (ImGui::BeginTable("ObjectTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner | ImGuiTableFlags_NoHostExtendX)) {
		// Set up column widths
		ImGui::TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthStretch, 0.3f);  // 30% width
		ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthStretch, 0.7f);     // 70% width

		ImGui::TableNextRow();

		// Left column: Categories
		ImGui::TableSetColumnIndex(0);

		// Begin a table for the categories list
		if (ImGui::BeginTable("CategoriesTable", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Categories");  // Label for the table

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			// List of categories
			const char* categories[] = { "Lighting Template", "Weather", "WorldSpace" };
			for (int i = 0; i < IM_ARRAYSIZE(categories); ++i) {
				// Highlight the selected category
				if (ImGui::Selectable(categories[i], selectedCategory == categories[i])) {
					selectedCategory = categories[i];  // Update selected category
				}
			}

			ImGui::EndTable();
		}

		// Right column: Objects
		ImGui::TableSetColumnIndex(1);

		ImGui::InputTextWithHint("##ObjectFilter", "Filter...", filterBuffer, sizeof(filterBuffer));

		ImGui::SameLine();
		HelpMarker("Type a part of an object name to filter the list.");

		// Create a table for the right column with "Name" and "ID" headers
		if (ImGui::BeginTable("DetailsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Editor ID", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Form ID", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);  // Added File column
			ImGui::TableHeadersRow();

			// Display objects based on the selected category
			auto& widgets = selectedCategory == "Weather"    ? weatherWidgets :
			                selectedCategory == "WorldSpace" ? worldSpaceWidgets :
			                                                   lightingTemplateWidgets;

			// Filtered display of widgets
			for (int i = 0; i < widgets.size(); ++i) {
				if (!ContainsStringIgnoreCase(widgets[i]->GetEditorID(), filterBuffer))
					continue;  // Skip widgets that don't match the filter

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);

				// Editor ID column
				if (ImGui::Selectable(widgets[i]->GetEditorID().c_str(), widgets[i]->IsOpen(), ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
					if (ImGui::IsMouseDoubleClicked(0)) {
						widgets[i]->SetOpen(true);
					}
				}

				// Form ID column
				ImGui::TableNextColumn();
				ImGui::Text(widgets[i]->GetFormID().c_str());

				// File column
				ImGui::TableNextColumn();
				ImGui::Text(widgets[i]->GetFilename().c_str());
			}

			ImGui::EndTable();
		}

		ImGui::EndTable();
	}

	// End the window
	ImGui::End();
}

void EditorWindow::ShowViewportWindow()
{
	ImGui::Begin("Viewport");

	// Top bar
	auto calendar = RE::Calendar::GetSingleton();
	if (calendar)
		ImGui::SliderFloat("##ViewportSlider", &calendar->gameHour->value, 0.0f, 23.99f, "Time: %.2f");

	// The size of the image in ImGui																														   // Get the available space in the current window
	ImVec2 availableSpace = ImGui::GetContentRegionAvail();

	// Calculate aspect ratio of the image
	float aspectRatio = ImGui::GetIO().DisplaySize.x / ImGui::GetIO().DisplaySize.y;

	// Determine the size to fit while preserving the aspect ratio
	ImVec2 imageSize;
	if (availableSpace.x / availableSpace.y < aspectRatio) {
		// Fit width
		imageSize.x = availableSpace.x;
		imageSize.y = availableSpace.x / aspectRatio;
	} else {
		// Fit height
		imageSize.y = availableSpace.y;
		imageSize.x = availableSpace.y * aspectRatio;
	}

	ImGui::Image((void*)tempTexture->srv.get(), imageSize);

	ImGui::End();
}

void EditorWindow::ShowWidgetWindow()
{
	for (int i = 0; i < (int)weatherWidgets.size(); i++) {
		auto widget = weatherWidgets[i];
		if (widget->IsOpen())
			widget->DrawWidget();
	}

	for (int i = 0; i < (int)worldSpaceWidgets.size(); i++) {
		auto widget = worldSpaceWidgets[i];
		if (widget->IsOpen())
			widget->DrawWidget();
	}

	for (int i = 0; i < (int)lightingTemplateWidgets.size(); i++) {
		auto widget = lightingTemplateWidgets[i];
		if (widget->IsOpen())
			widget->DrawWidget();
	}
}

void EditorWindow::ShowPreviewWindow()
{
	auto sky = RE::Sky::GetSingleton();
	ImGui::Begin("Preview Weathers");

	if (ImGui::BeginCombo("New Weather", weatherName1.c_str())) {
		for (int i = 0; i < weatherWidgets.size(); i++) {
			auto& widget = weatherWidgets[i];

			if (preview != nullptr && widget->GetEditorID() == preview->GetEditorID()) {
				widget->SetOpen(false);
			}

			// Option for each widget
			if (ImGui::Selectable(widget->GetEditorID().c_str())) {
				preview = (WeatherWidget*)widget;

				weatherName1 = widget->GetEditorID();
				widget->SetOpen();
				lerpValue = 0;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Old Weather", weatherName2.c_str())) {
		for (int i = 0; i < weatherWidgets.size(); i++) {
			auto& widget = weatherWidgets[i];

			if (previewWeather2 != nullptr && widget->GetEditorID() == previewWeather2->GetEditorID()) {
				widget->SetOpen(false);
			}

			// Option for each widget
			if (ImGui::Selectable(widget->GetEditorID().c_str())) {
				previewWeather2 = (WeatherWidget*)widget;
				weatherName2 = widget->GetEditorID();
				widget->SetOpen();
				lerpValue = 0;
			}
		}
		ImGui::EndCombo();
	}

	if (preview != nullptr && previewWeather2 != nullptr) {
		TESWeather* previewWeather = preview->weather;
		ImGui::SliderFloat("Lerp weathers", &lerpValue, 0.0f, 1.0f);
		ImGui::Text(std::format("Settings sun glare = {}", preview->settings.sunGlare).c_str());
		ImGui::Text(std::format("Sun Glare = {}", previewWeather->data.sunGlare).c_str());
		ImGui::Text(std::format("Sun Damage = {}", previewWeather->data.sunDamage).c_str());
		Weather::GetSingleton()->LerpWeather(previewWeather2->weather, previewWeather, lerpValue);

		ImGui::Text(std::format("Trans Delta = {}", previewWeather->data.transDelta).c_str());

		ImGui::Text(std::format("Settings sun glare = {}", preview->settings.sunGlare).c_str());
		ImGui::Text(std::format("Sun Glare = {}", previewWeather->data.sunGlare).c_str());
		ImGui::Text(std::format("Sun Damage = {}", previewWeather->data.sunDamage).c_str());

		ImGui::Text(std::format("Precipitation Begin Fade In = {}", previewWeather->data.precipitationBeginFadeIn).c_str());
		ImGui::Text(std::format("Precipitation End Fade Out = {}", previewWeather->data.precipitationEndFadeOut).c_str());

		ImGui::Text(std::format("Thunder Lightning Begin Fade In = {}", previewWeather->data.thunderLightningBeginFadeIn).c_str());
		ImGui::Text(std::format("Thunder Lightning End Fade Out = {}", previewWeather->data.thunderLightningEndFadeOut).c_str());
		ImGui::Text(std::format("Thunder Lightning Frequency = {}", previewWeather->data.thunderLightningFrequency).c_str());

		ImGui::Text(std::format("Visual Effect Begin = {}", previewWeather->data.visualEffectBegin).c_str());
		ImGui::Text(std::format("Visual Effect End = {}", previewWeather->data.visualEffectEnd).c_str());

		ImGui::Text(std::format("Wind Speed = {}", previewWeather->data.windSpeed).c_str());
		ImGui::Text(std::format("Wind Direction = {}", previewWeather->data.windDirection).c_str());
		ImGui::Text(std::format("Wind Direction Range = {}", previewWeather->data.windDirectionRange).c_str());

		std::string lightningColorStr = std::format("[{}, {}, {}]",
			previewWeather->data.lightningColor.red,
			previewWeather->data.lightningColor.green,
			previewWeather->data.lightningColor.blue);
		ImGui::Text(std::format("Lightning Color = {}", lightningColorStr).c_str());

		for (int i = 0; i < ColorTimes::kTotal; i++) {
			ImGui::Text(ColorTimeLabel(i).c_str());
			DisplayDALC("X", &previewWeather->directionalAmbientLightingColors[i].directional.x);
			DisplayDALC("Y", &previewWeather->directionalAmbientLightingColors[i].directional.y);
			DisplayDALC("Z", &previewWeather->directionalAmbientLightingColors[i].directional.z);
		}

		for (int i = 0; i < TESWeather::kTotalLayers; i++) {
			for (int j = 0; j < ColorTimes::kTotal; j++) {
				std::string cloudData = std::format("{} = [{}, {}, {}]",
					ColorTimeLabel(j),
					previewWeather->cloudColorData[i][j].red,
					previewWeather->cloudColorData[i][j].green,
					previewWeather->cloudColorData[i][j].blue);
				ImGui::Text(std::format("Layer {} \n Cloud Color = {}", i, cloudData).c_str());
			}
		}

		ImGui::Text(std::format("Fog Day Near = {}", previewWeather->fogData.dayNear).c_str());
		ImGui::Text(std::format("Fog Day Far = {}", previewWeather->fogData.dayFar).c_str());
		ImGui::Text(std::format("Fog Day Max = {}", previewWeather->fogData.dayMax).c_str());
		ImGui::Text(std::format("Fog Day Power = {}", previewWeather->fogData.dayPower).c_str());

		ImGui::Text(std::format("Fog Night Near = {}", previewWeather->fogData.nightNear).c_str());
		ImGui::Text(std::format("Fog Night Far = {}", previewWeather->fogData.nightFar).c_str());
		ImGui::Text(std::format("Fog Night Max = {}", previewWeather->fogData.nightMax).c_str());
		ImGui::Text(std::format("Fog Night Power = {}", previewWeather->fogData.nightPower).c_str());

		sky->lastWeather = previewWeather2->weather;
		sky->currentWeather = previewWeather;
		sky->currentWeatherPct = lerpValue;
	}

	ImGui::End();
}

void EditorWindow::DisplayDALC(std::string label, RE::BGSDirectionalAmbientLightingColors::Directional::MaxMin<RE::Color>* dalc)
{
	std::string dalcStr = std::format("\tMax = [{}, {}, {}]\n\tMin = [{}, {}, {}]",
		dalc->max.red,
		dalc->max.green,
		dalc->max.blue,
		dalc->min.red,
		dalc->min.green,
		dalc->min.blue);
	ImGui::Text(std::format("DALC {} \n {}", label, dalcStr).c_str());
}

void EditorWindow::RenderUI()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& framebuffer = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER];
	auto& context = State::GetSingleton()->context;
	auto sky = RE::Sky::GetSingleton();
	context->ClearRenderTargetView(framebuffer.RTV, (float*)&ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Close")) {
			open = false;
			sky->SetWeather(originalWeather, true, true);
			sky->currentWeatherPct = 100;
		}
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Save all"))
				SaveAll();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	auto width = ImGui::GetIO().DisplaySize.x;
	auto viewportWidth = width * 0.5f;                // Make the viewport take up 50% of the width
	auto sideWidth = (width - viewportWidth) / 2.0f;  // Divide the remaining width equally between the side windows

	ImGui::SetNextWindowSize(ImVec2(sideWidth, ImGui::GetIO().DisplaySize.y * 0.75f));
	ShowObjectsWindow();

	ImGui::SetNextWindowSize(ImVec2(viewportWidth, ImGui::GetIO().DisplaySize.y * 0.5f));
	ShowViewportWindow();

	ShowWidgetWindow();

	ShowPreviewWindow();
}

void EditorWindow::SetupResources()
{
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	auto& weatherArray = dataHandler->GetFormArray<RE::TESWeather>();

	for (auto weather : weatherArray) {
		auto widget = new WeatherWidget(weather);
		widget->Load();
		weatherWidgets.push_back(widget);
	}

	auto& worldSpaceArray = dataHandler->GetFormArray<RE::TESWorldSpace>();

	for (auto worldSpace : worldSpaceArray) {
		auto widget = new WorldSpaceWidget(worldSpace);
		widget->Load();
		worldSpaceWidgets.push_back(widget);
	}

	auto& lightingTemplateArray = dataHandler->GetFormArray<RE::BGSLightingTemplate>();

	for (auto lightingTemplate : lightingTemplateArray) {
		auto widget = new LightingTemplateWidget(lightingTemplate);
		widget->Load();
		lightingTemplateWidgets.push_back(widget);
	}

	auto sky = RE::Sky::GetSingleton();
	originalWeather = sky->currentWeather;
}

void EditorWindow::Draw()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& framebuffer = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER];

	ID3D11Resource* resource;
	framebuffer.SRV->GetResource(&resource);

	if (!tempTexture) {
		D3D11_TEXTURE2D_DESC texDesc{};
		((ID3D11Texture2D*)resource)->GetDesc(&texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		framebuffer.SRV->GetDesc(&srvDesc);

		tempTexture = new Texture2D(texDesc);
		tempTexture->CreateSRV(srvDesc);
	}

	auto& context = State::GetSingleton()->context;

	context->CopyResource(tempTexture->resource.get(), resource);

	RenderUI();
}

void EditorWindow::SaveAll()
{
	for (auto weather : weatherWidgets) {
		if (weather->IsOpen())
			weather->Save();
	}

	for (auto worldspace : worldSpaceWidgets) {
		if (worldspace->IsOpen())
			worldspace->Save();
	}

	for (auto lightingTemplate : lightingTemplateWidgets) {
		if (lightingTemplate->IsOpen())
			lightingTemplate->Save();
	}
}
