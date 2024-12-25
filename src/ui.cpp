#include <iostream>
#include <format>
#include <set>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <tinyfiledialogs.h>
#include "export.h"
#include "imageView.h"
#include "glCommon.h"
#include "styles.h"
#include "ui.h"
#include "res/fontFA6.h"
#include "res/fontOpenSans.h"
#include "version.h"

namespace {
	struct InputState {
		bool leftClickDown = false;
		double lastClickTime = 0;
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

		std::string glVersionString;
	};

	InputState inputState;
	UIState uiState;

	std::string bytesToSizeString(unsigned long long bytes);
}

UI::UI(GLFWwindow* window, const Config& config, const std::vector<ImageGroup>& groups, ImageCache* imageCache, GroupParameters& groupParameters, Monitor* monitor)
	: window(window), config(config), imageTargetSize(glm::ivec2(1, 1)), groups(groups), imageCache(imageCache), groupParameters(groupParameters), monitor(monitor) {
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImPlot::CreateContext();

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

	static ImFontConfig fontConfig;
	fontConfig.MergeMode = true;
	fontConfig.GlyphOffset.y = 2; // 2 pixel y offset for icon font size 18
	static const ImWchar rangesIcons[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromMemoryCompressedBase85TTF(FA6_compressed_data_base85, 18, &fontConfig, rangesIcons);
}

UI::~UI() {
	delete imageViewer[0];
	delete imageViewer[1];
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}

void UI::inputKey(int key) {
	if (controlPanelState != ControlPanel_ShowFiles) return;

	if (config.keyToAction.contains(key)) {
		switch (config.keyToAction.at(key)) {
		case KeyAction_Next:
			if (uiState.viewMode == ViewMode_Single) {
				goToNextUnskippedImage(0);
			} else if (uiState.viewMode == ViewMode_ManualCompare) {
				if (mouseOverlappingImage(0)) {
					goToNextUnskippedImage(0);
				} else if (mouseOverlappingImage(1)) {
					goToNextUnskippedImage(1);
				}
			}
			break;
		case KeyAction_Previous:
			if (uiState.viewMode == ViewMode_Single) {
				goToPreviousUnskippedImage(0);
			} else if (uiState.viewMode == ViewMode_ManualCompare) {
				if (mouseOverlappingImage(0)) {
					goToPreviousUnskippedImage(0);
				} else if (mouseOverlappingImage(1)) {
					goToPreviousUnskippedImage(1);
				}
			}
			break;
		case KeyAction_Reset:
			if (uiState.viewMode == ViewMode_Single || mouseOverlappingImage(0)) {
				imageViewer[0]->resetTransform();
				if (uiState.imageViewMovementLocked) {
					imageViewer[1]->resetTransform();
				}
			} else if (mouseOverlappingImage(1)) {
				imageViewer[1]->resetTransform();
				if (uiState.imageViewMovementLocked) {
					imageViewer[0]->resetTransform();
				}
			}
			break;
		case KeyAction_Save:
			if (uiState.viewMode == ViewMode_Single || mouseOverlappingImage(0)) {
				onSaveImage(selectedImages[0]);
			} else if (mouseOverlappingImage(1)) {
				onSaveImage(selectedImages[1]);
			}
			break;
		case KeyAction_Skip:
			if (uiState.viewMode == ViewMode_Single || mouseOverlappingImage(0)) {
				onSkipImage(selectedImages[0]);
			} else if (mouseOverlappingImage(1)) {
				onSkipImage(selectedImages[1]);
			}
			break;
		case KeyAction_ToggleHideSkipped:
			uiState.hideSkippedImages = !uiState.hideSkippedImages;
			break;
		case KeyAction_ToggleLockMovement:
			if (uiState.viewMode == ViewMode_ManualCompare) {
				uiState.imageViewMovementLocked = !uiState.imageViewMovementLocked;
				if (uiState.imageViewMovementLocked) {
					imageViewer[0]->resetTransform();
					imageViewer[1]->resetTransform();
				}
			}
			break;
		}
	}
}

void UI::inputClick(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			inputState.leftClickDown = true;
			double elapsed = glfwGetTime() - inputState.lastClickTime;
			double mouseX, mouseY;
			glfwGetCursorPos(window, &mouseX, &mouseY);

			if (elapsed <= doubleClickSeconds && mouseX == inputState.leftClickStart.x && mouseY == inputState.leftClickStart.y) {
				if (mouseOverlappingImage(0)) {
					imageViewer[0]->resetTransform();
					if (uiState.viewMode == ViewMode_ManualCompare && uiState.imageViewMovementLocked) {
						imageViewer[1]->resetTransform();
					}
				} else if (mouseOverlappingImage(1)) {
					imageViewer[1]->resetTransform();
					if (uiState.viewMode == ViewMode_ManualCompare && uiState.imageViewMovementLocked) {
						imageViewer[0]->resetTransform();
					}
				}
			}

			inputState.leftClickStart = glm::ivec2(mouseX, mouseY);
			inputState.prevMousePosition = glm::ivec2(mouseX, mouseY);
			inputState.lastClickTime = glfwGetTime();

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

void UI::renderFrame(double elapsed) {
	imageViewer[0]->renderFrame(elapsed, imageTargetSize);
	if (uiState.viewMode != ViewMode_Single) {
		imageViewer[1]->renderFrame(elapsed, imageTargetSize);
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	float menuBarHeight;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open")) {
				uiState.openDirectoryPicker = true;
			}
			if (ImGui::MenuItem("Close")) {
				onDirectoryClosed();
			}
			if (ImGui::MenuItem("Settings")) {
				showSettingsWindow = true;
				settingsWindowFirstOpen = true;
				showStatsWindow = false;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				// exit
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Export")) {
			if (!allowExports) {
				ImGui::BeginDisabled();
			}
			if (ImGui::MenuItem("Export saved images")) {
				controlPanelState = ControlPanel_SaveImages;
			}
			if (ImGui::MenuItem("Export filenames")) {
				controlPanelState = ControlPanel_SaveFilenames;
			}
			if (!allowExports) {
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			ImGui::MenuItem("About");
			if (ImGui::MenuItem("Info")) {
				showStatsWindow = true;
				showSettingsWindow = false;
			}
			ImGui::EndMenu();
		}
	}
	menuBarHeight = ImGui::GetFrameHeight();
	ImGui::EndMainMenuBar();

	ImVec2 windowSize(
		ImGui::GetMainViewport()->Size.x,
		ImGui::GetMainViewport()->Size.y - menuBarHeight
	);

	ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
	ImGui::SetNextWindowSize(windowSize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("main window", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

	ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarHeight));
	ImGui::BeginChild("left panel", ImVec2(controlWidth[uiState.viewMode], windowSize.y));

	prevControlPanelState = controlPanelState;
	switch (controlPanelState) {
	case ControlPanel_NothingLoaded:
		beginControlPanelSection("nothing_loaded");
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::TextWrapped("To open an image directory, use File > Open.");
		ImGui::PopStyleColor();
		endSection();
		break;
	case ControlPanel_DirectoryLoading:
		beginControlPanelSection("directory_loaded");
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::TextWrapped("Searching for images in directory...");
		ImGui::PopStyleColor();
		endSection();
		break;
	case ControlPanel_ShowGroups:
		renderControlPanelGroupOptions();
		ImGui::Spacing();
		renderControlPanelGroupsList();
		break;
	case ControlPanel_ShowFiles:
		renderControlPanelFiles();
		break;
	case ControlPanel_SaveFilenames:
		beginControlPanelSection("save_filenames");
		renderControlPanelSaveFilenames();
		endSection();
		break;
	case ControlPanel_SaveImages:
		beginControlPanelSection("save_images");
		renderControlPanelSaveImages();
		endSection();
		break;
	}

	ImGui::EndChild();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowPos(ImVec2(controlWidth[uiState.viewMode], menuBarHeight));
	ImGui::BeginChild("image_view_window", ImVec2(windowSize.x - controlWidth[uiState.viewMode], windowSize.y));

	if (uiState.viewMode == ViewMode_Single) {
		renderSingleImageView();
	} else if (uiState.viewMode == ViewMode_ManualCompare) {
		renderCompareImageView();
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
	ImGui::PopStyleVar(2);

	if (showStatsWindow) renderStatsWindow();
	if (showSettingsWindow) renderSettingsWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (uiState.openDirectoryPicker) {
		uiState.openDirectoryPicker = false;
		monitor->pauseTimers();
		char const* result = tinyfd_selectFolderDialog("Image directory", nullptr);
		monitor->resumeTimers();
		if (result != nullptr) {
			directoryPath = std::filesystem::path(result);
			onDirectoryOpened(result);
		}
	}

	if (uiState.updateViewMode) {
		uiState.viewMode = static_cast<ViewMode>(uiState.newViewMode);
		uiState.updateViewMode = false;
		uiState.newViewMode = -1;

		imageViewer[0]->resetTransform();
		imageViewer[1]->resetTransform();

		if (uiState.viewMode == ViewMode_Single) {
			// Updated from compare to single, so let left image become the single image automatically.
			// The right image is deselected.
			selectedImages[1] = -1;
		} else if (uiState.viewMode == ViewMode_ManualCompare) {
			// Updated from single to compare, so the single image becomes the left image automatically.
			// Select the next non-skipped image for the right image.
			selectImage(1, selectedImages[0]);
			goToNextUnskippedImage(1);
		}
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

void UI::renderControlPanelGroupOptions() {
	beginSection("group_parameter_padding_window", controlPadding, controlWidth[uiState.viewMode]);

	ImVec2 previousWindowPadding = ImGui::GetStyle().WindowPadding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::BeginChild("group_parameter_window", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
	if (ImGui::CollapsingHeader("Group Parameters")) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, previousWindowPadding);
		if (!allowGroupInteraction) ImGui::BeginDisabled();

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
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(tooltipPadding, tooltipPadding));
		ImGui::SetItemTooltip("Maximum seconds between images within the same group.");
		ImGui::PopStyleVar();
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

		ImGui::EndTable();

		ImGui::Spacing();

		if (ImGui::Button("Regenerate Groups")) {
			onRegenerateGroups();
		}
		if (!allowGroupInteraction) ImGui::EndDisabled();
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

	if (countGroupsWithNoSaves > 0) {
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::yellow);
		ImGui::TextWrapped("%d with no saved images", countGroupsWithNoSaves);
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	
	endSection();
}

void UI::renderControlPanelGroupsList() {
	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	ImGui::BeginChild("groups_scroll_window", ImVec2(-1, -1), 0, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(controlPadding, controlPadding));
	float groupItemWidth = ImGui::GetContentRegionAvail().x - controlPadding * 2;

	for (int i = 0; i < groups.size(); i++) {
		if (hoveredChildIndex == i && allowGroupInteraction) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::gray2);
		}

		ImGui::SetCursorPosX(controlPadding);
		ImGui::BeginChild(std::format("group_child_{}", i).c_str(), ImVec2(groupItemWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("group_table_{}", i).c_str(), 2, ImGuiTableFlags_NoBordersInBody)) {
			// Wrap the call to TableNextRow with zero cell padding to avoid the padding added on top of the row.
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {0, 0});
			ImGui::TableNextRow();
			ImGui::PopStyleVar();

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

		// Group right click popup
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::gray1);
		if (ImGui::BeginPopupContextItem()) {
			ImGui::BeginChild(std::format("group popup {}", i).c_str(), ImVec2(rightClickMenuWidth, 0), ImGuiChildFlags_AutoResizeY);
			ImGui::TextWrapped("Group %d", i + 1);
			ImGui::Separator();
			if (ImGui::Selectable("Save all images")) {
				onSaveGroup(i);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Selectable("Skip all images")) {
				onSkipGroup(i);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Selectable("Reset all images")) {
				onResetGroup(i);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndChild();
			ImGui::EndPopup();
		}
		ImGui::PopStyleColor();

		ImGui::Spacing();

		if (allowGroupInteraction) {
			if (hoveredChildIndex == i) {
				ImGui::PopStyleColor();
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				hoveredChildIndex = i;
				anyChildHovered = true;
			}

			if (ImGui::IsItemClicked()) {
				controlPanelState = ControlPanel_ShowFiles;
				uiState.viewMode = ViewMode_Single;
				selectedGroup = i;
				onGroupSelected(selectedGroup);

				// Select first non-skipped image
				int firstID = groups.at(selectedGroup).ids.at(0);
				selectImage(0, firstID);
				selectImage(1, -1);
				skippedImage();
			}
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}
}

void UI::renderControlPanelFiles() {
	beginSection("files_options_padding_window", controlPadding, controlWidth[uiState.viewMode]);

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

		const char* viewingModes[] = { "Single Image", "Manual Compare" };

		ImGui::SetNextItemWidth(viewModeComboWidth);
		if (ImGui::BeginCombo("##viewing_mode", viewingModes[(int)uiState.viewMode], 0)) {
			for (int i = 0; i < 2; i++) {
				const bool selected = i == static_cast<int>(uiState.viewMode);
				if (ImGui::Selectable(viewingModes[i], selected)) {
					if (uiState.viewMode != i) {
						uiState.updateViewMode = true;
						uiState.newViewMode = i;
					}
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Checkbox("Hide skipped images", &uiState.hideSkippedImages);

		if (uiState.viewMode == ViewMode_Single) {
			ImGui::BeginDisabled();
		}

		if (ImGui::Checkbox("Lock movement", &uiState.imageViewMovementLocked)) {
			if (uiState.imageViewMovementLocked) {
				imageViewer[0]->resetTransform();
				imageViewer[1]->resetTransform();
			}
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
	ImGui::Text("%d images, %d saved, %d skipped", groups[selectedGroup].ids.size(), groups[selectedGroup].savedCount, groups[selectedGroup].skippedCount);
	ImGui::Spacing();

	endSection();

	static int hoveredChildIndex = -1;
	bool anyChildHovered = false;

	if (uiState.scrollToTopOfFiles) {
		ImGui::SetNextWindowScroll(ImVec2(0, 0));
		uiState.scrollToTopOfFiles = false;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(controlPadding, controlPadding));

	ImGui::BeginChild("files_scroll_window", ImVec2(-1, -1), 0, 0);
	const float fileItemWidth = ImGui::GetContentRegionAvail().x - controlPadding * 2;
	for (int imageID : groups[selectedGroup].ids) {
		const Image* image = imageCache->getImage(imageID);

		if (image->skipped && uiState.hideSkippedImages) continue;

		bool setChildBackgroundColor = false;
		if (imageID == selectedImages[0] || imageID == selectedImages[1]) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::gray3);
			setChildBackgroundColor = true;
		} else if (imageID == hoveredChildIndex) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::gray2);
			setChildBackgroundColor = true;
		}

		ImGui::SetCursorPosX(controlPadding);
		ImGui::BeginChild(std::format("file_child_{}", imageID).c_str(), ImVec2(fileItemWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		if (ImGui::BeginTable(std::format("file_table_{}", imageID).c_str(), 3, ImGuiTableFlags_NoBordersInBody)) {
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {0, 0});
			ImGui::TableNextRow();
			ImGui::PopStyleVar();

			const float rowWidth = ImGui::GetContentRegionAvail().x;

			ImGui::TableSetColumnIndex(0);
			// TODO: choose texture id based on image status. completed image is the preview texture,
			//	loading image is some loading texture, failed is some error texture
			ImGui::Image(image->previewTextureId, ImVec2(previewImageSize.x, previewImageSize.y), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::TableSetColumnIndex(1);

			bool setTextColorDisabled = false;
			if (image->saved) {
				ImGui::PushStyleColor(ImGuiCol_Text, Colors::green);
			} else if (image->skipped && imageID != selectedImages[0] && imageID != selectedImages[1]) {
				ImGui::PushStyleColor(ImGuiCol_Text, Colors::textDisabled);
				setTextColorDisabled = true;
			}

			ImGui::Text(image->shortFilename.c_str());

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

			if (setTextColorDisabled) {
				ImGui::PopStyleColor();
			}

			if (uiState.viewMode == ViewMode_ManualCompare && (imageID == hoveredChildIndex || imageID == selectedImages[0] || imageID == selectedImages[1])) {
				const float xOffset = rowWidth - compareButtonSize.x * 2 - compareButtonSpacing;
				const float yOffset = controlPadding + (previewImageSize.x / 2.0f) - (compareButtonSize.y / 2.0f);

				ImGui::TableSetColumnIndex(2);

				ImGui::SetCursorPosX(xOffset);
				ImGui::SetCursorPosY(yOffset);

				const bool leftSelected = imageID == selectedImages[0];
				const bool rightSelected = imageID == selectedImages[1];

				if (leftSelected) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
				else ImGui::PushStyleColor(ImGuiCol_Button, Colors::gray4);
				if (ImGui::Button(std::format("##leftCompare").c_str(), ImVec2(compareButtonSize.x, compareButtonSize.y))) {
					selectImage(0, imageID);
				}
				ImGui::PopStyleColor();

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { compareButtonSpacing, 0});
				ImGui::SameLine();
				ImGui::PopStyleVar();

				if (rightSelected) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
				else ImGui::PushStyleColor(ImGuiCol_Button, Colors::gray4);
				if (ImGui::Button(std::format("##rightCompare").c_str(), ImVec2(compareButtonSize.x, compareButtonSize.y))) {
					selectImage(1, imageID);
				}
				ImGui::PopStyleColor();
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();

		// File right click popup
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::gray1);
		if (ImGui::BeginPopupContextItem()) {
			ImGui::BeginChild(std::format("file popup {}", imageID).c_str(), ImVec2(rightClickMenuWidth, 0), ImGuiChildFlags_AutoResizeY);
			ImGui::TextWrapped(image->filename.c_str());
			ImGui::Separator();
			if (ImGui::Selectable(image->saved ? "Undo save" : "Save")) {
				onSaveImage(imageID);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Selectable(image->skipped ? "Undo skip" : "Skip")) {
				onSkipImage(imageID);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndChild();
			ImGui::EndPopup();
		}
		ImGui::PopStyleColor();

		ImGui::Spacing();

		if (uiState.scrollToSelectedFile && imageID == selectedImages[0]) {
			uiState.scrollToSelectedFile = false;
			ImGui::ScrollToItem();
		}

		if (setChildBackgroundColor) {
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			hoveredChildIndex = imageID;
			anyChildHovered = true;
		}

		if (ImGui::IsItemClicked() && uiState.viewMode == ViewMode_Single) {
			selectImage(0, imageID);
		}
	}

	if (!anyChildHovered) {
		hoveredChildIndex = -1;
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
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
		if (ImGui::Button("Save", ImVec2(exportButtonSize.x, exportButtonSize.y))) {
			Export::exportFilenames(directoryPath, imageCache->getImages());
			uiState.exportInProgress = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(exportButtonSize.x, exportButtonSize.y))) {
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

		static bool copyAllMatching = true;
		ImGui::Checkbox("Include matching filenames", &copyAllMatching);
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::Text("?");
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered()) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(tooltipPadding, tooltipPadding));
			if (ImGui::BeginItemTooltip()) {
				ImGui::BeginChild("include_matches_popup", ImVec2(exportTooltipWidth, 0), ImGuiChildFlags_AutoResizeY);
				ImGui::TextWrapped("Along with the saved images, any files with a filename matching a saved image (excluding the extension) will also be copied.");
				ImGui::TextWrapped("This may be useful for copying raw image files along with the selected jpg versions.");
				ImGui::EndChild();
				ImGui::EndTooltip();
			}
			ImGui::PopStyleVar();
		}

		ImGui::Spacing();

		if (ImGui::Button("Save", ImVec2(exportButtonSize.x, exportButtonSize.y))) {
			Export::exportImages(directoryPath, imageCache->getImages(), copyAllMatching);
			uiState.exportInProgress = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(exportButtonSize.x, exportButtonSize.y))) {
			controlPanelState = ControlPanel_ShowGroups;
		}
	}
}

void UI::renderSingleImageView() {
	imageTargetSize.x = (int)ImGui::GetContentRegionAvail().x;
	imageTargetSize.y = (int)ImGui::GetContentRegionAvail().y;
	imageTargetPositions[0].x = (int)ImGui::GetWindowPos().x;
	imageTargetPositions[0].y = (int)ImGui::GetWindowPos().y;

	ImGui::Image(imageViewer[0]->getTextureId(), ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));

	bool showControls =
		controlPanelState == ControlPanel_ShowFiles &&
		selectedImages[0] != -1 &&
		mouseOverlappingImage(0);

	if (showControls) {
		// Renders the image view overlay
		const float topMargin = 10;
		glm::vec2 position{
			imageTargetPositions[0].x + (imageTargetSize.x / 2),
			imageTargetPositions[0].y + topMargin
		};

		renderImageViewOverlay(0, position);
	}
}

void UI::renderCompareImageView() {
	imageTargetSize.x = static_cast<int>(ImGui::GetContentRegionAvail().x / 2.0f);
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
	bool leftImageHovered = mouseOverlappingImage(0);
	bool rightImageHovered = mouseOverlappingImage(1);

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
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::gray5Transparent);

	ImGui::BeginChild("right_controls", controlSize);

	ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, buttonSpacing);

	int id = selectedImages[imageView];
	const Image* image = imageCache->getImage(id);
	bool imageSaved = image->saved;
	bool imageSkipped = image->skipped;

	if (imageSaved) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
	if (ImGui::Button("Save", buttonSize)) {
		onSaveImage(id);
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (imageSaved) ImGui::PopStyleColor();

	ImGui::SameLine();

	if (imageSkipped) ImGui::PushStyleColor(ImGuiCol_Button, Colors::green);
	if (ImGui::Button("Skip", buttonSize)) {
		onSkipImage(id);
	}
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (imageSkipped) ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button(ICON_RESIZE, buttonSize)) {
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

	ImGui::ProgressBar(imageCache->getPreviewLoadProgress());

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void UI::renderStatsWindow() {
	ImGui::SetNextWindowPos(ImVec2(
		ImGui::GetMainViewport()->Size.x / 2.0f - statsWindowSize.x / 2.0f,
		ImGui::GetMainViewport()->Size.y / 2.0f - statsWindowSize.y / 2.0f
	), ImGuiCond_Appearing);

	ImGui::SetNextWindowSize(ImVec2(statsWindowSize.x, statsWindowSize.y), ImGuiCond_Appearing);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_Border, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_SeparatorActive, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Colors::greenDark);

	ImGui::Begin("Infomation", &showStatsWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Cormorant v%s", VERSION);

	if (uiState.glVersionString.empty()) {
		const unsigned char* version = glGetString(GL_VERSION);
		uiState.glVersionString = std::format("OpenGL Version {}", reinterpret_cast<const char*>(version));
	}
	ImGui::Text(uiState.glVersionString.c_str());

	ImGui::Spacing();

	ImageCacheUIData cacheData;
	imageCache->getUIData(cacheData);

	if (ImGui::CollapsingHeader("Frame Rate")) {
		std::vector<float> yData;
		yData.reserve(monitor->frameTimes.size());

		double minFrameTime = FLT_MAX;
		double maxFrameTime = 0.0f;
		int i = 0;
		for (auto x : monitor->frameTimes) {
			if (monitor->frameTimes[i] > maxFrameTime) {
				maxFrameTime = monitor->frameTimes[i];
			}
			if (monitor->frameTimes[i] < minFrameTime) {
				minFrameTime = monitor->frameTimes[i];
			}
			yData.push_back(static_cast<float>(monitor->frameTimes[i++]));
		}

		ImGui::BeginTable("fps_table", 2, 0, ImVec2(250, 0));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Moving average");
		ImGui::TableSetColumnIndex(1);
		double ms = monitor->frameTimeAverage;
		ImGui::Text(std::format("{:.4f} s, {:.0f} FPS", ms, 1 / ms).c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("1k Frame Peak");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text(std::format("{:.4f} s", maxFrameTime).c_str());
		ImGui::EndTable();

		if (ImPlot::BeginPlot("##frametime_plot", ImVec2(-1, 100), ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs | ImPlotFlags_NoChild)) {
			ImPlot::SetupAxes(nullptr, "y", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoLabel);
			ImPlot::SetupAxesLimits(0.0, static_cast<double>(yData.size()), minFrameTime * 0.9, maxFrameTime * 1.1, ImPlotCond_Always);
			ImPlot::PushStyleColor(ImPlotCol_Line, Colors::greenDark);
			ImPlot::PlotLine("average plot", yData.data(), static_cast<int>(yData.size()));
			ImPlot::PopStyleColor();
			ImPlot::EndPlot();
		}
	}

	if (ImGui::CollapsingHeader("Textures")) {
		ImGui::Text("Preview Images");
		ImGui::Indent();
		ImGui::BeginTable("previews_table", 2, 0, ImVec2(250, 0));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Count");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%d", cacheData.previewCount);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Dimensions");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%d x %d pixels", cacheData.previewTextureSize.x, cacheData.previewTextureSize.y);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Estimated size");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text(bytesToSizeString(cacheData.estimatedPreviewBytes).c_str());
		ImGui::EndTable();

		ImGui::Unindent();

		ImGui::Text("Full Resolution Images");
		ImGui::Indent();
		ImGui::BeginTable("images_table", 2, 0, ImVec2(250, 0));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Cache capacity");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%d", cacheData.cacheCapacity);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Cached textures");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%d", cacheData.fullResolutionCount);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Estimated size");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text(bytesToSizeString(cacheData.estimatedFullTextureBytes).c_str());
		ImGui::EndTable();

		ImGui::Unindent();
	}

	if (ImGui::CollapsingHeader("Queues")) {
		ImGui::BeginTable("queues_table", 2, 0, ImVec2(325, 0));

		const char* rowLabels[] = {
			"Available PBO Queue Size",
			"Pending Image Queue Size",
			"Image Queue Threads",
			"Image Queue Size",
			"Texture Queue Size",
			"Pending PBO Map Size"
		};

		const int rowValues[] = {
			cacheData.availablePBOQueueSize,
			cacheData.pendingImageQueueSize,
			cacheData.imageLoadingThreads,
			cacheData.imageQueueSize,
			cacheData.textureQueueSize,
			cacheData.pendingPBOSize
		};

		for (int i = 0; i < 6; i++) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(rowLabels[i]);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", rowValues[i]);
		}

		ImGui::EndTable();
	}

	ImGui::End();
	ImGui::PopStyleColor(5);
}

void UI::renderSettingsWindow() {
	ImGui::SetNextWindowPos(ImVec2(
		ImGui::GetMainViewport()->Size.x / 2.0f - settingsWindowSize.x / 2.0f,
		ImGui::GetMainViewport()->Size.y / 2.0f - settingsWindowSize.y / 2.0f
	), ImGuiCond_Appearing);

	ImGui::SetNextWindowSize(ImVec2(settingsWindowSize.x, settingsWindowSize.y), ImGuiCond_Appearing);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_Border, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_SeparatorActive, Colors::greenDark);
	ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Colors::greenDark);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));
	ImGui::Begin("Settings", &showSettingsWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(5);

	static Config tempConfig;

	if (settingsWindowFirstOpen) {
		settingsWindowFirstOpen = false;

		tempConfig.cacheCapacity = config.cacheCapacity;
		tempConfig.cacheForwardPreload = config.cacheForwardPreload;
		tempConfig.cacheBackwardPreload = config.cacheBackwardPreload;
	}

	if (ImGui::CollapsingHeader("Cache")) {
		const int step = 1;

		ImGui::BeginTable("##settings_cache_table", 2, ImGuiTableFlags_None, ImVec2(0.0f, 0.0f));

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Cache capacity");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::Text("?");
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("Maximum number of images to keep in memory at a time.");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputScalar("##cache capacity", ImGuiDataType_U32, &tempConfig.cacheCapacity, &step, nullptr, "%d", ImGuiInputTextFlags_None);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Forward preload");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::Text("?");
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("When selecting an image, the number of images after that image to load in addition.");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputScalar("##cache_forward_preload", ImGuiDataType_U32, &tempConfig.cacheForwardPreload, &step, nullptr, "%d", ImGuiInputTextFlags_None);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Backward preload");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::textHint);
		ImGui::Text("?");
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("When selecting an image, the number of images before that image to load in addition.");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputScalar("##cache_backward_preload", ImGuiDataType_U32, &tempConfig.cacheBackwardPreload, &step, nullptr, "%d", ImGuiInputTextFlags_None);
	
		ImGui::EndTable();

		if (ImGui::Button("Save changes")) {
			onConfigUpdate(tempConfig);
		}
	}

	if (ImGui::CollapsingHeader("Key Bindings")) {
		ImGui::BeginTable("##settings_key_table", 2, ImGuiTableFlags_SizingFixedFit, ImVec2(0.0f, 0.0f));

		const char* keyActions[7] = {
			"Save image",
			"Skip image",
			"Go to next image",
			"Go to previous image",
			"Reset image zoom",
			"Toggle \"Hide skipped images\"",
			"Toggle \"Lock movement\""
		};
		const char* keyBindings[7][4] = {
			{ "Space", "Enter", "1", nullptr },
			{ "Backspace", "X", "2", nullptr },
			{ "Down arrow", "S", nullptr },
			{ "Up arrow", "W", nullptr },
			{ "R", "3", nullptr },
			{ "H", nullptr },
			{ "L", nullptr }
		};

		for (int i = 0; i < KeyAction_Count; i++) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(keyActions[i]);
			ImGui::TableSetColumnIndex(1);
			for (int j = 0; j < 4; j++) {
				if (keyBindings[i][j] == nullptr) break;
				ImGui::SameLine();
				ImGui::Button(keyBindings[i][j], ImVec2(100.0f, 0.0f));
			}
		}

		ImGui::EndTable();

		ImGui::Text("Note: when viewimg two images, most key bindings apply only to the image under the cursor.");
	}

	ImGui::End();
}

void UI::selectImage(int imageView, int id) {
	selectedImages[imageView] = id;
	imageViewer[imageView]->setImage(id);

	if (id != -1) onImageSelected(id);

	if (uiState.viewMode == ViewMode_Single) {
		imageViewer[imageView]->resetTransform();
	} else if (uiState.viewMode == ViewMode_ManualCompare) {
		// The image in the other view needs to be selected as well to avoid it being
		// evicted from the cache.
		int otherView = imageView == 1 ? 0 : 1;
		if (selectedImages[otherView] != -1) {
			onImageSelected(selectedImages[otherView]);
		}
	}
}

namespace {
std::string bytesToSizeString(unsigned long long bytes) {
	if (bytes < 1024) {
		return std::format("{} bytes", bytes);
	} else if (bytes < 1024 * 1024) {
		return std::format("{} KB", floor(bytes / 1024));
	} else if (bytes < 1024 * 1024 * 1024) {
		return std::format("{} MB", floor(bytes / 1024 / 1024));
	} else {
		return std::format("{} GB", floor(bytes / 1024 / 1024 / 1024));
	}
}
}

int UI::getCurrentGroupIndex() const {
	return selectedGroup;
}

void UI::setControlPanelState(ControlPanelState newState) {
	controlPanelState = newState;
}

void UI::reset() {
	controlPanelState = ControlPanel_NothingLoaded;
	imageViewer[0]->setImage(-1);
	imageViewer[1]->setImage(-1);
	selectedImages.fill(-1);
	directoryPath.clear();
	allowGroupInteraction = false;
	allowExports = false;
}

void UI::goToNextUnskippedImage(int imageView) {
	if (imageView > 0 && uiState.viewMode == ViewMode_Single) return;

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
			selectImage(imageView, newImage);
			uiState.scrollToSelectedFile = true;
			break;
		}
	}
}

void UI::goToPreviousUnskippedImage(int imageView) {
	if (imageView > 0 && uiState.viewMode == ViewMode_Single) return;

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
			selectImage(imageView, newImage);
			uiState.scrollToSelectedFile = true;
			break;
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
				selectImage(imageView, newImage);
				return;
			}
		}

		// Search backward for a non-skipped image
		for (int i = index - 1; i >= 0; i--) {
			int newImage = groups[selectedGroup].ids[i];
			if (!imageCache->getImage(groups[selectedGroup].ids[i])->skipped && newImage != selectedImages[0] && newImage != selectedImages[1]) {
				selectImage(imageView, newImage);
				return;
			}
		}

		// No available images
		int newImage = -1;
		selectImage(imageView, newImage);
	}
}

void UI::startLoadingImages() {
	controlPanelState = ControlPanel_DirectoryLoading;
	allowGroupInteraction = false;
	allowExports = false;
	showPreviewProgress = true;
	previewProgress = 0.0f;
}

void UI::endLoadingImages() {
	allowGroupInteraction = true;
	allowExports = true;
	showPreviewProgress = false;
	previewProgress = 0.0f;
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

void UI::beginControlPanelSection(const char* label) {
	beginSection(label, controlPadding, controlWidth[uiState.viewMode]);
}

void UI::beginSection(const char* label, float padding, float outerWidth) {
	ImGui::SetCursorPos(ImVec2(padding, padding));
	ImGui::BeginChild(label, ImVec2(outerWidth - 2 * padding, 0), ImGuiChildFlags_AutoResizeY, 0);
}

void UI::endSection() {
	ImGui::EndChild();
}
