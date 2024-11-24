#include <iostream>
#include <format>
#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <tinyfiledialogs.h>

UI::UI(GLFWwindow* window, GLuint imageTexture, const std::vector<Group>& groups, const ImageCache* imageCache)
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
				openImageDirectory();
			}
            ImGui::MenuItem("Settings");
			ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }
    }
    ImGui::EndMainMenuBar();

	bool open = true;

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
	case ControlPanel_ShowGroups: {
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

		ImGui::Text("%d Groups", groups.size());

		ImGui::Spacing();

		static int hoveredChildIndex = -1;
		bool anyChildHovered = false;

		for (int i = 0; i < groups.size(); i++) {
			const Group& group = groups[i];
			if (hoveredChildIndex == i) {
				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 100));
			}

			ImGui::BeginChild(std::format("group_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
			if (ImGui::BeginTable(std::format("group_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				unsigned int textureId = imageCache->getImage(group.startIndex).previewTextureId;
				ImGui::Image(textureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("Group %d", i);
				ImGui::Separator();
				ImGui::Text("%d images", group.endIndex - group.startIndex + 1);
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
				std::cout << "Selected group " << i << std::endl;
			}
		}

		if (!anyChildHovered) {
			hoveredChildIndex = -1;
		}

		break;
	}
	case ControlPanel_ShowFiles:
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
}

glm::ivec2 UI::getImageTargetSize() const {
	return imageTargetSize;
}

glm::ivec2 UI::getImageTargetPosition() const {
	return imageTargetPosition;
}

void UI::openImageDirectory() {
	//char const* result = tinyfd_openFileDialog("Image directory", nullptr, 0, nullptr, nullptr, 1);
	char const* result = tinyfd_selectFolderDialog("Image directory", nullptr);
	
	if (result != nullptr) {
		std::cout << result << std::endl;
		onDirectoryOpened(result);
	}
}

void UI::setDirectoryOpenedCallback(std::function<void(std::string)> callback) {
	onDirectoryOpened = callback;
}
