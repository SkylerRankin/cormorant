#include <iostream>
#include <format>
#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <tinyfiledialogs.h>

UI::UI(GLFWwindow* window, GLuint imageTexture, const std::vector<Group>& groups, ImageCache* imageCache)
	: imageTexture(imageTexture), imageTargetSize(glm::ivec2(0, 0)), groups(groups), imageCache(imageCache) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

	// Disable ImGui saving the layout modifications to a .ini file
	io.IniFilename = nullptr;
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
		ImGuiID dockLeftID = ImGui::DockBuilderSplitNode(dockRightID, ImGuiDir_Left, 0.20f, nullptr, &dockRightID);

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
		static bool timeCheck = false;
		ImGui::Checkbox("Time", &timeCheck);
		ImGui::Spacing();

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		static int timeScale = 10;
		ImGui::SliderInt("##time", &timeScale, 0, 100, "%d sec", 0);

		// Row 1
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		static bool shutterCheck = false;
		ImGui::Checkbox("Shutter speed", &shutterCheck);
		ImGui::Spacing();

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		static int shutterSpeed = 10;
		ImGui::SliderInt("##shutterspeed", &shutterSpeed, 0, 100, "%d sec", 0);

		ImGui::EndTable();

		ImGui::Separator();

		ImGui::Button("Regenerate Groups");

		ImGui::Separator();
	}

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
		const Group& group = groups[i];
		if (hoveredChildIndex == i) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("group_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

		ImGui::BeginTable(std::format("group_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		unsigned int textureId = imageCache->getImage(group.startIndex)->previewTextureId;
		ImGui::Image(textureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

		ImGui::TableSetColumnIndex(1);
		ImGui::Text("Group %d", i);
		ImGui::Separator();
		ImGui::Text("%d images", group.endIndex - group.startIndex + 1);
		ImGui::EndTable();

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
			controlPanelState = ControlPanel_ShowFiles;
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

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Group %d", selectedGroup);
	ImGui::Spacing();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	for (int i = groups[selectedGroup].startIndex; i <= groups[selectedGroup].endIndex; i++) {
		if (i == selectedImage) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));
		} else if (i == hoveredChildIndex) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("file_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("file_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const Image* image = imageCache->getImage(i);
			// TODO: choose texture id based on image status. completed image is the preview texture,
			//	loading image is some loading texture, failed is some error texture
			ImGui::Image(image->previewTextureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text(image->filename.c_str());
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
				ImGui::Text("No date");
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

glm::ivec2 UI::getImageTargetSize() const {
	return imageTargetSize;
}

glm::ivec2 UI::getImageTargetPosition() const {
	return imageTargetPosition;
}

void UI::setDirectoryOpenedCallback(std::function<void(std::string)> callback) {
	onDirectoryOpened = callback;
}

void UI::setImageSelectedCallback(std::function<void(int)> callback) {
	onImageSelected = callback;
}
