#include <iostream>
#include <format>
#include <set>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <tinyfiledialogs.h>
#include "export.h"
#include "imageView.h"
#include "styles.h"
#include "ui.h"
#include "res/fontOpenSans.h"

namespace {
	enum ViewMode {
		ViewMode_Single = 0,
		ViewMode_ManualCompare = 1,
		ViewMode_AutoCompare = 2
	};

	struct InputState {
		bool leftClickDown = false;
		bool panningImage = false;
		int mouseActiveImage = -1;
		glm::ivec2 prevMousePosition{};
		glm::ivec2 leftClickStart{};
	};

	struct UIState {
		ViewMode viewMode = ViewMode_Single;
		bool imageViewMovementLocked = true;
		bool hideSkippedImages = true;
		bool exportInProgress = false;

		// Data used to defer UI updates until after frame is rendered
		bool openDirectoryPicker = false;
		bool updateViewMode = false;
		int newViewMode = -1;
		bool scrollToSelectedFile = false;
		bool scrollToTopOfFiles = false;
	};

	InputState inputState;
	UIState uiState;
}

UI::UI(GLFWwindow* window, const std::vector<ImageGroup>& groups, ImageCache* imageCache, GroupParameters& groupParameters)
	: window(window), imageTargetSize(glm::ivec2(1, 1)), groups(groups), imageCache(imageCache), groupParameters(groupParameters) {
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

	setGlobalStyles();

	imageViewer[0] = new ImageViewer(imageCache->getImages());
	imageViewer[1] = new ImageViewer(imageCache->getImages());

	io.Fonts->AddFontFromMemoryCompressedBase85TTF(OpenSans_compressed_data_base85, 16);
}

UI::~UI() {
	delete imageViewer[0];
	delete imageViewer[1];
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UI::inputClick(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			inputState.leftClickDown = true;
			double mouseX, mouseY;
			glfwGetCursorPos(window, &mouseX, &mouseY);
			inputState.leftClickStart = glm::ivec2(mouseX, mouseY);
			inputState.prevMousePosition = glm::ivec2(mouseX, mouseY);

			bool overlaps[2] = {
				mouseOverlappingImage(0),
				mouseOverlappingImage(1)
			};

			inputState.panningImage = overlaps[0] || overlaps[1];
			if (overlaps[0]) {
				inputState.mouseActiveImage = 0;
			} else if (overlaps[1]) {
				inputState.mouseActiveImage = 1;
			} else {
				inputState.mouseActiveImage = -1;
			}
		} else if (action == GLFW_RELEASE) {
			inputState.leftClickDown = false;
			inputState.panningImage = false;
			inputState.mouseActiveImage = -1;
		}
	}
}

void UI::inputMouseMove(double x, double y) {
	if (inputState.leftClickDown) {
		glm::ivec2 dragOffset = glm::ivec2(x, y) - inputState.prevMousePosition;
		if (inputState.panningImage) {
			if (uiState.imageViewMovementLocked) {
				imageViewer[0]->pan(dragOffset);
				imageViewer[1]->pan(dragOffset);
			} else {
				imageViewer[inputState.mouseActiveImage]->pan(dragOffset);
			}
		}
	}
	inputState.prevMousePosition = glm::ivec2(x, y);
}

void UI::inputScroll(int offset) {
	if (mouseOverlappingImage(0)) {
		imageViewer[0]->zoom(offset, inputState.prevMousePosition - imageTargetPositions[0]);
		if (uiState.imageViewMovementLocked) {
			imageViewer[1]->zoom(offset, inputState.prevMousePosition - imageTargetPositions[0]);
		}
	} else if (mouseOverlappingImage(1)) {
		imageViewer[1]->zoom(offset, inputState.prevMousePosition - imageTargetPositions[1]);
		if (uiState.imageViewMovementLocked) {
			imageViewer[0]->zoom(offset, inputState.prevMousePosition - imageTargetPositions[1]);
		}
	}
}

void UI::renderFrame() {
	imageViewer[0]->renderFrame(imageTargetSize);
	if (uiState.viewMode != ViewMode_Single) {
		imageViewer[1]->renderFrame(imageTargetSize);
	}

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open")) {
				uiState.openDirectoryPicker = true;
			}
			ImGui::MenuItem("Save");
            ImGui::MenuItem("Settings");
			ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Export")) {
			if (imageCache->getImages().size() == 0) {
				ImGui::BeginDisabled();
			}
			if (ImGui::MenuItem("Copy saved images")) {
				controlPanelState = ControlPanel_SaveImages;
			}
			if (ImGui::MenuItem("Save filenames")) {
				controlPanelState = ControlPanel_SaveFilenames;
			}
			if (imageCache->getImages().size() == 0) {
				ImGui::EndDisabled();
			}
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

	prevControlPanelState = controlPanelState;
	ImGui::Begin("left_panel", nullptr);

	switch (controlPanelState) {
	case ControlPanel_NothingLoaded:
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::TextWrapped("To open an image directory, use File > Open.");
		ImGui::PopStyleColor();
		break;
	case ControlPanel_DirectoryLoading:
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::TextWrapped("Searching for images in directory...");
		ImGui::PopStyleColor();
		break;
	case ControlPanel_ShowGroups:
		renderControlPanelGroups();
		break;
	case ControlPanel_ShowFiles:
		renderControlPanelFiles();
		break;
	case ControlPanel_SaveFilenames:
		renderControlPanelSaveFilenames();
		break;
	case ControlPanel_SaveImages:
		renderControlPanelSaveImages();
		break;
	}

	ImGui::End();

	if (uiState.viewMode == ViewMode_Single) {
		renderSingleImageView();
	} else if (uiState.viewMode == ViewMode_ManualCompare) {
		renderCompareImageView();
	} else if (uiState.viewMode == ViewMode_AutoCompare) {
		renderCompareImageView();
	}

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (uiState.openDirectoryPicker) {
		uiState.openDirectoryPicker = false;
		char const* result = tinyfd_selectFolderDialog("Image directory", nullptr);
		if (result != nullptr) {
			directoryPath = std::filesystem::path(result);
			onDirectoryOpened(result);
		}
	}

	if (uiState.updateViewMode) {
		uiState.viewMode = static_cast<ViewMode>(uiState.newViewMode);
		selectedImages.fill(-1);
		uiState.updateViewMode = false;
		uiState.newViewMode = -1;

		// Clear out both image views
		imageViewer[0]->setImage(-1);
		imageViewer[1]->setImage(-1);
		imageViewer[0]->resetTransform();
		imageViewer[1]->resetTransform();
	}

	// UI updates when leaving a control panel state
	if (prevControlPanelState != controlPanelState) {
		switch (prevControlPanelState) {
		case ControlPanel_ShowFiles:
			uiState.scrollToTopOfFiles = true;
			break;
		case ControlPanel_ShowGroups:
			break;
		}
	}
}

void UI::renderControlPanelGroups() {
	ImVec2 previousWindowPadding = ImGui::GetStyle().WindowPadding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::BeginChild("group_parameter_window", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
	if (ImGui::CollapsingHeader("Group Parameters")) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, previousWindowPadding);

		ImGui::BeginTable("parameters_table", 2);

		ImGui::TableSetupColumn("parameter_checkboxes", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("parameter_controls", ImGuiTableColumnFlags_WidthStretch);

		// Row 0
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Checkbox("Time", &groupParameters.timeEnabled);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
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
		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (showPreviewProgress) {
		renderPreviewProgress();
	}

	if (groups.size() == 1) {
		ImGui::Text("1 Group");
	} else {
		ImGui::Text("%d Groups", groups.size());
	}

	int countGroupsWithNoSaves = 0;
	for (const ImageGroup& group : groups) {
		if (group.savedCount == 0 && group.skippedCount < group.ids.size()) {
			countGroupsWithNoSaves++;
		}
	}

	if (countGroupsWithNoSaves == 0) {
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::green);
		ImGui::TextWrapped("All groups have at least 1 saved image");
		ImGui::PopStyleColor();
	} else {
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::yellow);
		ImGui::TextWrapped("%d group%s with no saved images", countGroupsWithNoSaves, countGroupsWithNoSaves == 1 ? "" : "s");
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	ImGui::BeginChild("groups_scroll_window", ImVec2(-1, -1), 0, 0);
	for (int i = 0; i < groups.size(); i++) {
		if (hoveredChildIndex == i) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("group_child_{}", i).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("group_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			unsigned int textureId = imageCache->getImage(groups[i].ids[0])->previewTextureId;
			ImGui::Image(textureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Group %d", i + 1);
			ImGui::Separator();
			ImGui::Text("%d image%s", groups[i].ids.size(), groups[i].ids.size() == 1 ? "" : "s");

			if (groups[i].skippedCount == groups[i].ids.size()) {
				ImGui::Text("All images skipped");
			} else {
				bool noSavedImages = groups[i].savedCount == 0;
				if (noSavedImages) ImGui::PushStyleColor(ImGuiCol_Text, Colors::yellow);
				ImGui::Text("%d saved, %d skipped", groups[i].savedCount, groups[i].skippedCount);
				if (noSavedImages) ImGui::PopStyleColor();
			}

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
			selectImage(0, -1);
			selectImage(1, -1);
		}
	}

	ImGui::EndChild();

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
}

void UI::renderControlPanelFiles() {
	if (ImGui::Button("Return to groups", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
		controlPanelState = ControlPanel_ShowGroups;
		selectedImages.fill(-1);
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	ImGui::Spacing();

	// The "CollapsingHeader" widget extends its width in both directions by `window->WindowPadding.x * 0.5f`. I'd
	// prefer the width to match other widgets, so placing the widget within a child window with 0 padding removes
	// this additional width.
	ImVec2 previousWindowPadding = ImGui::GetStyle().WindowPadding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::BeginChild("file_options_window", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
	if (ImGui::CollapsingHeader("Options")) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, previousWindowPadding);

		const char* viewingModes[] = { "Single Image", "Manual Compare", "Auto Compare" };

		if (ImGui::BeginCombo("##viewing_mode", viewingModes[(int)uiState.viewMode], 0)) {
			for (int i = 0; i < 3; i++) {
				const bool selected = i == static_cast<int>(uiState.viewMode);
				if (ImGui::Selectable(viewingModes[i], selected)) {
					uiState.updateViewMode = true;
					uiState.newViewMode = i;
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

		ImGui::Checkbox("Hide skipped images", &uiState.hideSkippedImages);

		if (uiState.viewMode == ViewMode_Single) {
			ImGui::BeginDisabled();
		}

		static bool movementLocked = true;
		if (ImGui::Checkbox("Lock movement", &movementLocked)) {
			uiState.imageViewMovementLocked = movementLocked;
			imageViewer[0]->resetTransform();
			imageViewer[1]->resetTransform();
		}

		if (uiState.viewMode == ViewMode_Single) {
			ImGui::EndDisabled();
		}

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (showPreviewProgress) {
		renderPreviewProgress();
	}

	ImGui::Text("Group %d", selectedGroup + 1);
	ImGui::Text("%d saved, %d skipped", groups[selectedGroup].savedCount, groups[selectedGroup].skippedCount);
	ImGui::Spacing();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	if (uiState.scrollToTopOfFiles) {
		ImGui::SetNextWindowScroll(ImVec2(0, 0));
		uiState.scrollToTopOfFiles = false;
	}

	ImGui::BeginChild("files_scroll_window", ImVec2(-1, -1), 0, 0);
	for (int imageID : groups[selectedGroup].ids) {
		const Image* image = imageCache->getImage(imageID);

		if (image->skipped && uiState.hideSkippedImages) continue;

		if (imageID == selectedImages[0] || imageID == selectedImages[1]) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));
		} else if (imageID == hoveredChildIndex) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 30, 255));
		}

		ImGui::BeginChild(std::format("file_child_{}", imageID).c_str(), ImVec2(-1.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("file_table_{}", imageID).c_str(), 3, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			// TODO: choose texture id based on image status. completed image is the preview texture,
			//	loading image is some loading texture, failed is some error texture
			ImGui::Image(image->previewTextureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);

			if (image->saved) {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(69, 173, 73, 255));
			} else if (image->skipped) {
				ImGui::PushStyleColor(ImGuiCol_Text, Colors::textDisabled);
			}

			if (uiState.viewMode == ViewMode_ManualCompare && selectedImages[0] == imageID) {
				ImGui::Text(std::format("{} - Left", image->filename).c_str());
			} else if (uiState.viewMode == ViewMode_ManualCompare && selectedImages[1] == imageID) {
				ImGui::Text(std::format("{} - Right", image->filename).c_str());
			} else {
				ImGui::Text(image->filename.c_str());
			}

			if (image->saved) {
				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			ImGui::Text("%s, %d x %d", bytesToSizeString(image->filesize).c_str(), image->size.x, image->size.y);

			if (image->metadata.timestamp.has_value()) {
				ImageTimestamp t = image->metadata.timestamp.value();
				ImGui::Text(std::format("{}:{:0>2} {}/{}/{}", t.hour, t.minute, t.month, t.day, t.year).c_str());
			} else {
				ImGui::Text("Date unknown");
			}

			if (image->skipped) {
				ImGui::PopStyleColor();
			}
			
			if (imageID == hoveredChildIndex && uiState.viewMode == ViewMode_ManualCompare) {
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("Left", ImVec2(50, 20))) {
					selectedImages[0] = imageID;
					selectImage(0, selectedImages[0]);
				}

				if (ImGui::Button("Right", ImVec2(50, 20))) {
					selectedImages[1] = imageID;
					selectImage(1, selectedImages[1]);
				}
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();

		if (uiState.scrollToSelectedFile && imageID == selectedImages[0]) {
			uiState.scrollToSelectedFile = false;
			ImGui::ScrollToItem();
		}

		if (imageID == hoveredChildIndex || imageID == selectedImages[0] || imageID == selectedImages[1]) {
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			hoveredChildIndex = imageID;
			anyChildHovered = true;
		}

		if (ImGui::IsItemClicked() && uiState.viewMode == ViewMode_Single) {
			selectedImages[0] = imageID;
			selectImage(0, selectedImages[0]);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}

	ImGui::EndChild();
}

void UI::renderControlPanelSaveFilenames() {
	ImGui::Text("Save filenames");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (uiState.exportInProgress) {
		ImGui::TextWrapped("Export in progress...");
		ImGui::ProgressBar(Export::exportProgress());
		if (!Export::exportInProgress()) {
			uiState.exportInProgress = false;
			controlPanelState = ControlPanel_ShowGroups;
		}
	} else {
		ImGui::TextWrapped("Creates a file containing the filenames of all saved images.");
		std::string outputPath = Export::filenameOutputPath(directoryPath).string();
		ImGui::TextWrapped("File location: %s", outputPath.c_str());
		ImGui::Spacing();
		if (ImGui::Button("Save", ImVec2(100, 20))) {
			Export::exportFilenames(directoryPath, imageCache->getImages());
			uiState.exportInProgress = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 20))) {
			controlPanelState = ControlPanel_ShowGroups;
		}
	}
}

void UI::renderControlPanelSaveImages() {
	ImGui::Text("Save images");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (uiState.exportInProgress) {
		ImGui::TextWrapped("Export in progress...");
		ImGui::ProgressBar(Export::exportProgress());
		if (!Export::exportInProgress()) {
			uiState.exportInProgress = false;
			controlPanelState = ControlPanel_ShowGroups;
		}
	} else {
		std::string outputPath = Export::imageOutputPath(directoryPath).string();

		ImGui::TextWrapped("Copies all saved images into %s.", outputPath.c_str());
		ImGui::Spacing();
		if (ImGui::Button("Save", ImVec2(100, 20))) {
			Export::exportImages(directoryPath, imageCache->getImages());
			uiState.exportInProgress = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 20))) {
			controlPanelState = ControlPanel_ShowGroups;
		}
	}
}

void UI::renderSingleImageView() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("right_panel", nullptr);

	imageTargetSize.x = (int)ImGui::GetContentRegionAvail().x;
	imageTargetSize.y = (int)ImGui::GetContentRegionAvail().y;
	imageTargetPositions[0].x = (int)ImGui::GetWindowPos().x;
	imageTargetPositions[0].y = (int)ImGui::GetWindowPos().y;

	ImGui::Image(imageViewer[0]->getTextureId(), ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));

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
	ImGui::Image(imageViewer[0]->getTextureId(), halfSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::SameLine();
	ImGui::Image(imageViewer[1]->getTextureId(), halfSize, ImVec2(0, 1), ImVec2(1, 0));
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
	const ImVec2 buttonSize(75, 25);
	const int buttonSpacing = 10;
	const int buttonCount = 3;

	ImVec2 controlSize{ buttonCount * buttonSize.x + (buttonCount - 1) * buttonSpacing, buttonSize.y };
	position.x -= controlSize.x / 2.0f;

	ImGui::SetNextItemAllowOverlap();
	ImGui::SetNextWindowPos(ImVec2(position.x, position.y));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::transparent);
	ImGui::PushStyleColor(ImGuiCol_Button, Colors::gray3Transparent);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::gray4Transparent);

	ImGui::BeginChild("right_controls", controlSize);

	ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, buttonSpacing);

	int id = selectedImages[imageView];
	const Image* image = imageCache->getImage(id);
	bool imageSaved = image->saved;
	bool imageSkipped = image->skipped;

	if (imageSaved) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
	if (ImGui::Button("Saved", buttonSize)) {
		onSaveImage(id);
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (imageSaved) ImGui::PopStyleColor();

	ImGui::SameLine();

	if (imageSkipped) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
	if (ImGui::Button("Skipped", buttonSize)) {
		onSkipImage(id);
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (imageSkipped) ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("Reset", buttonSize)) {
		if (uiState.imageViewMovementLocked) {
			imageViewer[0]->resetTransform();
			imageViewer[1]->resetTransform();
		} else {
			imageViewer[imageView]->resetTransform();
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	ImGui::PopStyleVar();

	ImGui::PopStyleColor(3);
	ImGui::EndChild();
}

void UI::renderPreviewProgress() {
	ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
	ImGui::Text("Loading image information");
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Colors::green);
	ImGui::ProgressBar(imageCache->getPreviewLoadProgress());
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void UI::selectImage(int imageView, int id) {
	onImageSelected(id);
	imageViewer[imageView]->setImage(id);
	if (uiState.viewMode == ViewMode_Single) {
		imageViewer[imageView]->resetTransform();
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

void UI::setControlPanelState(ControlPanelState newState) {
	controlPanelState = newState;
}

void UI::goToNextUnskippedImage() {
	if (uiState.viewMode != ViewMode_Single) return;

	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && uiState.viewMode == ViewMode_Single) break;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].ids.size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup].ids[i]) {
				index = i;
				break;
			}
		}

		if (index == -1) return;

		// Find the next image that is not skipped
		for (int i = index + 1; i < groups[selectedGroup].ids.size(); i++) {
			int newImage = groups[selectedGroup].ids[i];
			if (newImage == selectedImages[0] || newImage == selectedImages[1]) continue;
			if (!imageCache->getImage(newImage)->skipped) {
				selectedImages[imageView] = newImage;
				selectImage(imageView, newImage);
				uiState.scrollToSelectedFile = true;
				break;
			}
		}
	}
}

void UI::goToPreviousUnskippedImage() {
	if (uiState.viewMode != ViewMode_Single) return;

	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && uiState.viewMode == ViewMode_Single) break;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].ids.size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup].ids[i]) {
				index = i;
				break;
			}
		}

		if (index == -1) return;

		// Find the previous image that is not skipped
		for (int i = index - 1; i >= 0; i--) {
			int newImage = groups[selectedGroup].ids[i];
			if (newImage == selectedImages[0] || newImage == selectedImages[1]) continue;
			if (!imageCache->getImage(newImage)->skipped) {
				selectedImages[imageView] = newImage;
				selectImage(imageView, newImage);
				uiState.scrollToSelectedFile = true;
				break;
			}
		}
	}
}

void UI::skippedImage() {
	for (int imageView = 0; imageView < selectedImages.size(); imageView++) {
		if (imageView > 0 && uiState.viewMode == ViewMode_Single) break;

		// The image for this view isn't skipped, so no update required
		if (!imageCache->getImage(selectedImages[imageView])->skipped) continue;

		// Find index of selected image within the group
		int index = -1;
		for (int i = 0; i < groups[selectedGroup].ids.size(); i++) {
			if (selectedImages[imageView] == groups[selectedGroup].ids[i]) {
				index = i;
				break;
			}
		}

		if (index == -1) continue;

		// Search forward for a non-skipped image
		for (int i = index + 1; i < groups[selectedGroup].ids.size(); i++) {
			int newImage = groups[selectedGroup].ids[i];
			if (!imageCache->getImage(newImage)->skipped && newImage != selectedImages[0] && newImage != selectedImages[1]) {
				selectedImages[imageView] = newImage;
				selectImage(imageView, newImage);
				return;
			}
		}

		// Search backward for a non-skipped image
		for (int i = index - 1; i >= 0; i--) {
			int newImage = groups[selectedGroup].ids[i];
			if (!imageCache->getImage(groups[selectedGroup].ids[i])->skipped && newImage != selectedImages[0] && newImage != selectedImages[1]) {
				selectedImages[imageView] = newImage;
				selectImage(imageView, newImage);
				return;
			}
		}

		// No available images
		int newImage = -1;
		selectedImages[imageView] = newImage;
		selectImage(imageView, newImage);
	}
}

void UI::setShowPreviewProgress(bool enabled) {
	showPreviewProgress = enabled;
}

bool UI::mouseOverlappingImage(int imageView) {
	// Prevent second image viewer from being interactable in single image mode.
	if (imageView == 1 && uiState.viewMode == ViewMode_Single) {
		return false;
	}

	glm::ivec2 imagePosition = imageTargetPositions[imageView];
	glm::ivec2 imageSize = imageTargetSize;

	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);

	return imagePosition.x <= mouseX && mouseX <= imagePosition.x + imageSize.x &&
		   imagePosition.y <= mouseY && mouseY <= imagePosition.y + imageSize.y;
}
