#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include "cache.h"
#include "config.h"
#include "glCommon.h"
#include "group.h"
#include "imageView.h"
#include "stats.h"

enum ControlPanelState {
    ControlPanel_NothingLoaded = 0,
    ControlPanel_DirectoryLoading = 1,
    ControlPanel_ShowGroups = 2,
    ControlPanel_ShowFiles = 3,
    ControlPanel_SaveFilenames = 4,
    ControlPanel_SaveImages = 5,
};

enum ViewMode {
    ViewMode_Single = 0,
    ViewMode_ManualCompare = 1,
};

class UI {
public:
    UI(GLFWwindow* window, const Config::Config& config, const std::vector<Group::ImageGroup>& groups, ImageCache* imageCache, Group::GroupParameters& groupParameters, Monitor* monitor);
    ~UI();
    void renderFrame(double elapsed);
    int getCurrentGroupIndex() const;
    void setControlPanelState(ControlPanelState newState);
    void reset();

    // Called when the skipped flag on an image is changed. This is required because if the selected image is
    // now skipped, the UI should automatically move to the next unskipped image.
    void skippedImage();
    void setShowPreviewProgress(bool enabled);
    // Next two functions calls when a new image directory is opened, and then when all previews for that directory
    // have been processed. In between the two calls, the UI will prevent groups from being opened since the images
    // likely won't be ready yet.
    void startLoadingImages();
    void endLoadingImages();

    // Input handling
    void inputKey(int key);
    void inputClick(int button, int action, int mods);
    void inputMouseMove(double x, double y);
    void inputScroll(int offset);

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
    std::function<void()> onDirectoryClosed;
    std::function<void(int)> onGroupSelected;
    std::function<void(int)> onImageSelected;
    std::function<void()> onRegenerateGroups;
    std::function<void(int)> onSkipImage;
    std::function<void(int)> onSaveImage;
    std::function<void(int)> onSkipGroup;
    std::function<void(int)> onSaveGroup;
    std::function<void(int)> onResetGroup;
    std::function<void(Config::Config&)> onConfigUpdate;

private:
    const glm::vec2 previewImageSize{75, 75};
    const float controlPadding = 8.0f;
    const float tooltipPadding = 4.0f;
    const float controlWidth[3] = { 250.0f, 325.0f, 325.0f };
    const glm::vec2 compareButtonSize{ 30.0f, 30.0f };
    const glm::vec2 exportButtonSize{100, 25};
    const float compareButtonSpacing = 5;
    const float viewModeComboWidth = 150.0f;
    const float rightClickMenuWidth = 150.0f;
    const float exportTooltipWidth = 250.0f;
    const glm::vec2 statsWindowSize{400.0f, 300.0f};
    const glm::vec2 settingsWindowSize{ 600.0f, 300.0f };
    const glm::vec2 aboutWindowSize{ 300.0f, 100.0f };
    const float doubleClickSeconds = 0.25f;

    const std::vector<Group::ImageGroup>& groups;
    const Config::Config& config;

    GLFWwindow* window;
    ImageViewer* imageViewer[2];
    Group::GroupParameters& groupParameters;
    ImageCache* imageCache;
    Monitor* monitor;
    glm::ivec2 imageTargetSize;
    std::array<glm::ivec2, 2> imageTargetPositions;
    bool allowGroupInteraction = false;
    bool allowExports = false;
    bool showPreviewProgress = false;
    bool showStatsWindow = false;
    bool showSettingsWindow = false;
    bool settingsWindowFirstOpen = false;
    float previewProgress = 0.0f;
    bool showAboutWindow = false;

    ControlPanelState controlPanelState = ControlPanel_NothingLoaded;
    ControlPanelState prevControlPanelState = ControlPanel_NothingLoaded;
    int selectedGroup;
    // IDs of the images, index 0 for the left and single image, and index 1 for the right image.
    std::array<int, 2> selectedImages = {-1, -1};
    std::filesystem::path directoryPath;

    void renderControlPanelGroupOptions();
    void renderControlPanelGroupsList();
    void renderControlPanelFiles();
    void renderControlPanelSaveFilenames();
    void renderControlPanelSaveImages();
    void renderSingleImageView();
    void renderCompareImageView();
    void renderImageViewOverlay(int imageView, glm::vec2 position);
    void renderPreviewProgress();
    void renderStatsWindow();
    void renderSettingsWindow();
    void renderAboutWindow();

    void selectImage(int imageView, int id);
    bool mouseOverlappingImage(int imageView);

    // For all relevant image views, update the selected image to the next unskipped image that is not
    // also used by another image view. If there are no more unskipped images, the selected image is
    // set to -1.
    void goToNextUnskippedImage(int imageView);
    void goToPreviousUnskippedImage(int imageView);

    void beginControlPanelSection(const char* label);
    void beginSection(const char* label, float padding, float outerWidth);
    void endSection();
};
