#include "EditorWindow.h"

#include "State.h"

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

	// Static variable to track renaming
	static int renameIndex = -1;
	static char renameBuffer[256] = "";

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
				if (!ContainsStringIgnoreCase(widgets[i]->GetEditorID(), filterBuffer) && renameIndex != i)
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

void EditorWindow::RenderUI()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& framebuffer = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER];
	auto& context = State::GetSingleton()->context;

	context->ClearRenderTargetView(framebuffer.RTV, (float*)&ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Close")) {
			open = false;
		}
		if (ImGui::BeginMenu("File")) {
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