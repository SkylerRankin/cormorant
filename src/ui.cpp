#include <iostream>
#include <format>
#include <set>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <tinyfiledialogs.h>
#include "ui.h"

UI::UI(GLFWwindow* window, std::array<GLuint, 2> textureIDs, const std::vector<std::vector<int>>& groups, ImageCache* imageCache, GroupParameters& groupParameters)
	: imageViewTextures(textureIDs), imageTargetSize(glm::ivec2(0, 0)), groups(groups), imageCache(imageCache), groupParameters(groupParameters) {
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

	// Disable ImGui saving the layout modifications to a .ini file
	io.IniFilename = nullptr;
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
}

UI::~UI() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UI::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open")) {
				openDirectoryPicker = true;
			}
			ImGui::MenuItem("Save");
            ImGui::MenuItem("Settings");
			ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Export")) {
			ImGui::MenuItem("Copy Selections");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			ImGui::MenuItem("About");
			ImGui::EndMenu();
		}
    }
    ImGui::EndMainMenuBar();

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("dockspace_window", nullptr, window_flags);
	ImGui::PopStyleVar();

	ImGui::PopStyleVar();

	if (ImGui::DockBuilderGetNode(ImGui::GetID("dockspace")) == nullptr) {
		// Clear any existing dockspace layout and add an empty node that will hold the image viewport
		ImGuiID dockspaceID = ImGui::GetID("dockspace");
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::DockBuilderRemoveNode(dockspaceID);
		ImGui::DockBuilderAddNode(dockspaceID);

		// Split into two dock nodes, the main viewporrt and the left panel
		ImGuiID dockRightID = dockspaceID;
		ImGuiID dockLeftID = ImGui::DockBuilderSplitNode(dockRightID, ImGuiDir_Left, 0.25f, nullptr, &dockRightID);

		// Remove the tab bar / title from each window
		ImGui::DockBuilderGetNode(dockRightID)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		ImGui::DockBuilderGetNode(dockLeftID)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

		ImGui::DockBuilderDockWindow("left_panel", dockLeftID);
		ImGui::DockBuilderDockWindow("right_panel", dockRightID);
		ImGui::DockBuilderFinish(dockspaceID);
	}

	ImGuiID dockspaceID = ImGui::GetID("dockspace");
	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f));
	ImGui::End();

	ImGui::Begin("left_panel", nullptr);

	switch (controlPanelState) {
	case ControlPanel_NothingLoaded:
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(90, 90, 90, 255));
		ImGui::TextWrapped("To open an image directory, use File > Open.");
		ImGui::PopStyleColor();
		break;
	case ControlPanel_ShowGroups:
		renderControlPanelGroups();
		break;
	case ControlPanel_ShowFiles:
		renderControlPanelFiles();
		break;
	}

	ImGui::End();

	if (viewMode == ViewMode_Single) {
		renderSingleImageView();
	} else if (viewMode == ViewMode_ManualCompare) {
		renderCompareImageView();
	} else if (viewMode == ViewMode_AutoCompare) {
		renderCompareImageView();
	}

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (openDirectoryPicker) {
		openDirectoryPicker = false;
		char const* result = tinyfd_selectFolderDialog("Image directory", nullptr);
		if (result != nullptr) {
			controlPanelState = ControlPanel_ShowGroups;
			onDirectoryOpened(result);
		}
	}

	if (updateViewMode) {
		viewMode = static_cast<ViewMode>(newViewMode);
		selectedImages.fill(-1);
		onViewModeUpdated(newViewMode);
		updateViewMode = false;
		newViewMode = -1;
	}
}

void UI::renderControlPanelGroups() {
	if (ImGui::CollapsingHeader("Group Parameters")) {
		ImGui::BeginTable("parameters_table", 2);

		ImGui::TableSetupColumn("parameter_checkboxes", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("parameter_controls", ImGuiTableColumnFlags_WidthStretch);

		// Row 0
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Checkbox("Time", &groupParameters.timeEnabled);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(90, 90, 90, 255));
		ImGui::Text("?");
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("Maximum seconds between images within the same group.");
		ImGui::Spacing();

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		if (!groupParameters.timeEnabled) {
			ImGui::BeginDisabled();
		}
		ImGui::SliderInt("##time", &groupParameters.timeSeconds, 0, 60, "%d sec", 0);
		if (!groupParameters.timeEnabled) {
			ImGui::EndDisabled();
		}
		if (groupParameters.timeSeconds < 0) {
			groupParameters.timeSeconds = 0;
		} else if (groupParameters.timeSeconds > 60) {
			groupParameters.timeSeconds = 60;
		}

		// Row 1
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		static bool shutterCheck = false;
		ImGui::Checkbox("Shutter", &shutterCheck);
		ImGui::Spacing();

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		static int shutterSpeed = 10;
		ImGui::SliderInt("##shutterspeed", &shutterSpeed, 0, 100, "%d sec", 0);

		ImGui::EndTable();

		ImGui::Spacing();

		float tableWidth = ImGui::GetContentRegionAvail().x;
		if (ImGui::Button("Regenerate Groups")) {
			onRegenerateGroups();
		}
		ImGui::Spacing();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (groups.size() == 1) {
		ImGui::Text("1 Group");
	} else {
		ImGui::Text("%d Groups", groups.size());
	}

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(168, 143, 50, 255));
	ImGui::TextWrapped("? groups with 0 selections");
	ImGui::PopStyleColor();

	ImGui::Spacing();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	for (int i = 0; i < groups.size(); i++) {
		if (hoveredChildIndex == i) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("group_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("group_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			unsigned int textureId = imageCache->getImage(groups[i][0])->previewTextureId;
			ImGui::Image(textureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Group %d", i);
			ImGui::Separator();
			ImGui::Text("%d images", groups[i].size());
			ImGui::EndTable();
		}
		ImGui::EndChild();

		if (hoveredChildIndex == i) {
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}

		if (ImGui::IsItemHovered()) {
			hoveredChildIndex = i;
			anyChildHovered = true;
		}

		if (ImGui::IsItemClicked()) {
			selectedGroup = i;
			onGroupSelected(selectedGroup);
			controlPanelState = ControlPanel_ShowFiles;
			selectedImages.fill(-1);
			onImageSelected(0, -1);
			onImageSelected(1, -1);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
}

void UI::renderControlPanelFiles() {
	if (ImGui::Button("Return to groups")) {
		controlPanelState = ControlPanel_ShowGroups;
		selectedImages.fill(-1);
	}

	const char* viewingModes[] = {"Single Image", "Manual Compare", "Auto Compare"};

	if (ImGui::BeginCombo("##viewing_mode", viewingModes[(int) viewMode], 0)) {
		for (int i = 0; i < 3; i++) {
			const bool selected = i == static_cast<int>(viewMode);
			if (ImGui::Selectable(viewingModes[i], selected)) {
				updateViewMode = true;
				newViewMode = i;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(90, 90, 90, 255));
	ImGui::Text("?");
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip("info about viewing modes");

	ImGui::Checkbox("Hide skipped images", &hideSkippedImages);

	if (viewMode == ViewMode_Single) {
		ImGui::BeginDisabled();
	}

	static bool movementLocked = true;
	if (ImGui::Checkbox("Lock movement", &movementLocked)) {
		onMovementLock(movementLocked);
	}

	if (viewMode == ViewMode_Single) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Group %d", selectedGroup);
	ImGui::Spacing();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	for (int i : groups[selectedGroup]) {
		const Image* image = imageCache->getImage(i);

		if (image->skipped && hideSkippedImages) continue;

		if (i == selectedImages[0] || i == selectedImages[1]) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));
		} else if (i == hoveredChildIndex) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("file_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("file_table_{}", i).c_str(), 3, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			// TODO: choose texture id based on image status. completed image is the preview texture,
			//	loading image is some loading texture, failed is some error texture
			ImGui::Image(image->previewTextureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);

			if (image->saved) {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(69, 173, 73, 255));
			} else if (image->skipped) {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(60, 60, 60, 255));
			}

			if (viewMode == ViewMode_ManualCompare && selectedImages[0] == i) {
				ImGui::Text(std::format("{} - Left", image->filename).c_str());
			} else if (viewMode == ViewMode_ManualCompare && selectedImages[1] == i) {
				ImGui::Text(std::format("{} - Right", image->filename).c_str());
			} else {
				ImGui::Text(image->filename.c_str());
			}

			if (image->saved) {
				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			ImGui::Text(bytesToSizeString(image->filesize).c_str());
			if (image->fileInfoLoaded) {
				ImGui::Text("%d x %d", image->size.x, image->size.y);
			} else {
				ImGui::Text("Size unknown");
			}

			if (image->metadata.timestamp.has_value()) {
				ImageTimestamp t = image->metadata.timestamp.value();
				ImGui::Text(std::format("{}:{:0>2} {}/{}/{}", t.hour, t.minute, t.month, t.day, t.year).c_str());
			} else {
				ImGui::Text("Date unknown");
			}

			if (image->skipped) {
				ImGui::PopStyleColor();
			}
			
			if (i == hoveredChildIndex && viewMode == ViewMode_ManualCompare) {
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("Left", ImVec2(50, 20))) {
					selectedImages[0] = i;
					onImageSelected(0, selectedImages[0]);
				}

				if (ImGui::Button("Right", ImVec2(50, 20))) {
					selectedImages[1] = i;
					onImageSelected(1, selectedImages[1]);
				}
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();

		if (i == hoveredChildIndex || i == selectedImages[0] || i == selectedImages[1]) {
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			hoveredChildIndex = i;
			anyChildHovered = true;
		}

		if (ImGui::IsItemClicked() && viewMode == ViewMode_Single) {
			selectedImages[0] = i;
			onImageSelected(0, selectedImages[0]);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
}

void UI::renderSingleImageView() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("right_panel", nullptr);

	imageTargetSize.x = (int)ImGui::GetContentRegionAvail().x;
	imageTargetSize.y = (int)ImGui::GetContentRegionAvail().y;
	imageTargetPositions[0].x = (int)ImGui::GetWindowPos().x;
	imageTargetPositions[0].y = (int)ImGui::GetWindowPos().y;

	ImGui::Image(imageViewTextures[0], ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));

	bool showControls =
		controlPanelState == ControlPanel_ShowFiles &&
		selectedImages[0] != -1;

	if (showControls) {
		// Renders the image view overlay
		const float topMargin = 10;
		glm::vec2 position{
			imageTargetPositions[0].x + (imageTargetSize.x / 2),
			imageTargetPositions[0].y + topMargin
		};

		renderImageViewOverlay(0, position);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void UI::renderCompareImageView() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("right_panel", nullptr);

	imageTargetSize.x = (int)ImGui::GetContentRegionAvail().x / 2.0f;
	imageTargetSize.y = (int)ImGui::GetContentRegionAvail().y;
	imageTargetPositions[0].x = (int)ImGui::GetWindowPos().x;
	imageTargetPositions[0].y = (int)ImGui::GetWindowPos().y;
	imageTargetPositions[1].x = (int)ImGui::GetWindowPos().x + imageTargetSize.x;
	imageTargetPositions[1].y = (int)ImGui::GetWindowPos().y;

	ImVec2 halfSize = ImVec2(ImGui::GetContentRegionAvail().x / 2.0f, ImGui::GetContentRegionAvail().y);

	ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
	ImGui::Image(imageViewTextures[0], halfSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::SameLine();
	ImGui::Image(imageViewTextures[1], halfSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::PopStyleVar();

	// Render the overlay only for the image view overlapping the cursor
	ImVec2 mousePosition = ImGui::GetMousePos();
	bool leftImageHovered =
		mousePosition.x >= imageTargetPositions[0].x && mousePosition.x <= imageTargetPositions[0].x + imageTargetSize.x &&
		mousePosition.y >= imageTargetPositions[0].y && mousePosition.y <= imageTargetPositions[0].y + imageTargetSize.y;
	bool rightImageHovered =
		mousePosition.x >= imageTargetPositions[1].x && mousePosition.x <= imageTargetPositions[1].x + imageTargetSize.x &&
		mousePosition.y >= imageTargetPositions[1].y && mousePosition.y <= imageTargetPositions[1].y + imageTargetSize.y;

	if (controlPanelState == ControlPanel_ShowFiles && selectedImages[0] != -1 && leftImageHovered) {
		const float topMargin = 10;
		glm::vec2 position{
			imageTargetPositions[0].x + (imageTargetSize.x / 2),
			imageTargetPositions[0].y + topMargin
		};
		renderImageViewOverlay(0, position);
	} else if (controlPanelState == ControlPanel_ShowFiles && selectedImages[1] != -1 && rightImageHovered) {
		const float topMargin = 10;
		glm::vec2 position{
			imageTargetPositions[1].x + (imageTargetSize.x / 2),
			imageTargetPositions[1].y + topMargin
		};
		renderImageViewOverlay(1, position);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void UI::renderImageViewOverlay(int imageView, glm::vec2 position) {
	const ImVec2 buttonSize(75, 20);
	const int buttonSpacing = 10;
	const int buttonCount = 3;

	ImVec2 controlSize{ buttonCount * buttonSize.x + (buttonCount - 1) * buttonSpacing, buttonSize.y };
	position.x -= controlSize.x / 2.0f;

	ImGui::SetNextItemAllowOverlap();
	ImGui::SetNextWindowPos(ImVec2(position.x, position.y));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
	ImGui::BeginChild("right_controls", controlSize);

	ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, buttonSpacing);

	int id = selectedImages[imageView];
	const Image* image = imageCache->getImage(id);
	bool imageSaved = image->saved;
	bool imageSkipped = image->skipped;

	if (imageSaved) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(69, 173, 73, 255));
	if (ImGui::Button("Saved", buttonSize)) {
		onSaveImage(id);
	}
	if (imageSaved) ImGui::PopStyleColor();

	ImGui::SameLine();

	if (imageSkipped) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(69, 173, 73, 255));
	if (ImGui::Button("Skipped", buttonSize)) {
		onSkipImage(id);
	}
	if (imageSkipped) ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("Reset", buttonSize)) {
		onResetImageTransform(imageView);
	}

	ImGui::PopStyleVar();

	ImGui::PopStyleColor();
	ImGui::EndChild();
}

std::string UI::bytesToSizeString(int bytes) {
	if (bytes < 1024) {
		return std::format("{} bytes", bytes);
	} else if (bytes < 1024 * 1024) {
		return std::format("{} KB", floor(bytes / 1024));
	} else {
		return std::format("{} MB", floor(bytes / 1024 / 1024));
	}
}

int UI::getCurrentGroupIndex() const {
	return selectedGroup;
}

glm::ivec2 UI::getImageTargetSize() const {
	return imageTargetSize;
}

glm::ivec2 UI::getImageTargetPosition(int imageView) const {
	return imageTargetPositions[imageView];
}

void UI::goToNextUnskippedImage() {
	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && viewMode == ViewMode_Single) break;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup][i]) {
				index = i;
				break;
			}
		}

		if (index == -1) return;

		// Find the next image that is not skipped
		for (int i = index + 1; i < groups[selectedGroup].size(); i++) {
			int newImage = groups[selectedGroup][i];
			if (newImage == selectedImages[0] || newImage == selectedImages[1]) continue;
			if (!imageCache->getImage(newImage)->skipped) {
				selectedImages[imageView] = newImage;
				onImageSelected(imageView, newImage);
				break;
			}
		}
	}
}

void UI::goToPreviousUnskippedImage() {
	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && viewMode == ViewMode_Single) break;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup][i]) {
				index = i;
				break;
			}
		}

		if (index == -1) return;

		// Find the previous image that is not skipped
		for (int i = index - 1; i >= 0; i--) {
			int newImage = groups[selectedGroup][i];
			if (newImage == selectedImages[0] || newImage == selectedImages[1]) continue;
			if (!imageCache->getImage(newImage)->skipped) {
				selectedImages[imageView] = newImage;
				onImageSelected(imageView, newImage);
				break;
			}
		}
	}
}

void UI::skippedImage() {
	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && viewMode == ViewMode_Single) break;

		// The image for this view isn't skipped, so no update required
		if (!imageCache->getImage(selectedImages[imageView])->skipped) continue;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup][i]) {
				index = i;
				break;
			}
		}

		if (index == -1) continue;

		// Search forward for a non-skipped image
		for (int i = index + 1; i < groups[selectedGroup].size(); i++) {
			int newImage = groups[selectedGroup][i];
			if (!imageCache->getImage(newImage)->skipped && newImage != selectedImages[0] && newImage != selectedImages[1]) {
				selectedImages[imageView] = newImage;
				onImageSelected(imageView, newImage);
				return;
			}
		}

		// Search backward for a non-skipped image
		for (int i = index - 1; i >= 0; i--) {
			int newImage = groups[selectedGroup][i];
			if (!imageCache->getImage(groups[selectedGroup][i])->skipped && newImage != selectedImages[0] && newImage != selectedImages[1]) {
				selectedImages[imageView] = newImage;
				onImageSelected(imageView, newImage);
				return;
			}
		}

		// No available images
		int newImage = -1;
		selectedImages[imageView] = newImage;
		onImageSelected(imageView, newImage);
	}
}
