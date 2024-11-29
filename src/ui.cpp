#include <iostream>
#include <format>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <tinyfiledialogs.h>
#include "ui.h"

UI::UI(GLFWwindow* window, GLuint imageTexture, const std::vector<std::vector<int>>& groups, ImageCache* imageCache, GroupParameters& groupParameters)
	: imageTexture(imageTexture), imageTargetSize(glm::ivec2(0, 0)), groups(groups), imageCache(imageCache), groupParameters(groupParameters) {
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

	ImGui::Begin("right_panel", nullptr);

	imageTargetSize.x = (int) ImGui::GetContentRegionAvail().x;
	imageTargetSize.y = (int) ImGui::GetContentRegionAvail().y;
	imageTargetPosition.x = (int) ImGui::GetWindowPos().x;
	imageTargetPosition.y = (int)ImGui::GetWindowPos().y;

	ImGui::Image(imageTexture, ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));
	ImGui::End();

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
			// Auto select the first image in the group
			selectedImage = groups[selectedGroup][0];
			onImageSelected(selectedImage);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
}

void UI::renderControlPanelFiles() {
	if (ImGui::Button("Return to groups")) {
		controlPanelState = ControlPanel_ShowGroups;
		selectedImage = -1;
	}

	ImGui::Checkbox("Hide skipped images", &hideSkippedImages);

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

		if (i == selectedImage) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));
		} else if (i == hoveredChildIndex) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("file_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("file_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
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

			ImGui::Text(image->filename.c_str());

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

			ImGui::EndTable();
		}

		ImGui::EndChild();

		if (i == hoveredChildIndex || i == selectedImage) {
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
			selectedImage = i;
			onImageSelected(i);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
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

int UI::getCurrentImageID() const {
	return selectedImage;
}

glm::ivec2 UI::getImageTargetSize() const {
	return imageTargetSize;
}

glm::ivec2 UI::getImageTargetPosition() const {
	return imageTargetPosition;
}

void UI::nextImage() {
	int index = -1;
	for (int i = 0; i < groups[selectedGroup].size() - 1; i++) {
		if (selectedImage == groups[selectedGroup][i]) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		return;
	}

	// Find next un-skipped image
	for (int i = index + 1; i < groups[selectedGroup].size(); i++) {
		if (!imageCache->getImage(groups[selectedGroup][i])->skipped) {
			selectedImage = i;
			onImageSelected(selectedImage);
			break;
		}
	}
}

void UI::previousImage() {
	int index = -1;
	for (int i = 1; i < groups[selectedGroup].size(); i++) {
		if (selectedImage == groups[selectedGroup][i]) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		return;
	}

	// Find previous un-skipped image
	for (int i = index - 1; i >= 0; i--) {
		if (!imageCache->getImage(groups[selectedGroup][i])->skipped) {
			selectedImage = i;
			onImageSelected(selectedImage);
			break;
		}
	}
}

void UI::skippedImage() {
	// Selected image is not skipped, so UI doesn't require update
	if (!imageCache->getImage(selectedImage)->skipped) {
		return;
	}

	int index = -1;
	for (int i = 0; i < groups[selectedGroup].size(); i++) {
		if (selectedImage == groups[selectedGroup][i]) {
			index = i;
			break;
		}
	}

	// Invalid selected group
	assert(index != -1 && std::format("UI::skippedImage invalid selectedImage = {}", selectedImage).c_str());

	// Search forward for a non-skipped image
	for (int i = index + 1; i < groups[selectedGroup].size(); i++) {
		if (!imageCache->getImage(groups[selectedGroup][i])->skipped) {
			selectedImage = i;
			onImageSelected(selectedImage);
			return;
		}
	}

	// Search backward for a non-skipped image
	for (int i = index - 1; i >= 0; i--) {
		if (!imageCache->getImage(groups[selectedGroup][i])->skipped) {
			selectedImage = i;
			onImageSelected(selectedImage);
			return;
		}
	}

	// No available images
	selectedImage = -1;
	onImageSelected(selectedImage);
}

void UI::setDirectoryOpenedCallback(std::function<void(std::string)> callback) {
	onDirectoryOpened = callback;
}

void UI::setGroupSelectedCallback(std::function<void(int)> callback) {
	onGroupSelected = callback;
}

void UI::setImageSelectedCallback(std::function<void(int)> callback) {
	onImageSelected = callback;
}

void UI::setRegenerateGroupsCallback(std::function<void()> callback) {
	onRegenerateGroups = callback;
}
