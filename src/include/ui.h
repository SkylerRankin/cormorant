#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include "imageView.h"
#include "glCommon.h"
#include "group.h"
#include "cache.h"

enum ControlPanelState {
    ControlPanel_NothingLoaded = 0,
    ControlPanel_DirectoryLoading = 1,
    ControlPanel_ShowGroups = 2,
    ControlPanel_ShowFiles = 3,
    ControlPanel_SaveFilenames = 4,
    ControlPanel_SaveImages = 5,
};

class UI {
public:
    UI(GLFWwindow* window, const std::vector<ImageGroup>& groups, ImageCache* imageCache, GroupParameters& groupParameters);
    ~UI();
    void renderFrame();
    int getCurrentGroupIndex() const;
    void setControlPanelState(ControlPanelState newState);

    // For all relevant image views, update the selected image to the next unskipped image that is not
    // also used by another image view. If there are no more unskipped images, the selected image is
    // set to -1.
    void goToNextUnskippedImage();
    void goToPreviousUnskippedImage();
    // Called when the skipped flag on an image is changed. This is required because if the selected image is
    // now skipped, the UI should automatically move to the next unskipped image.
    void skippedImage();
    void setShowPreviewProgress(bool enabled);

    // Input handling
    void inputClick(int button, int action, int mods);
    void inputMouseMove(double x, double y);
    void inputScroll(int offset);

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
    std::function<void(int)> onGroupSelected;
    std::function<void(int)> onImageSelected;
    std::function<void()> onRegenerateGroups;
    std::function<void(int)> onSkipImage;
    std::function<void(int)> onSaveImage;

private:
    const glm::ivec2 previewImageSize{75, 75};
    const std::vector<ImageGroup>& groups;

    GLFWwindow* window;
    ImageViewer* imageViewer[2];
    GroupParameters& groupParameters;
    ImageCache* imageCache;
    glm::ivec2 imageTargetSize;
    std::array<glm::ivec2, 2> imageTargetPositions;
    bool showPreviewProgress = false;
    float previewProgress = 0.0f;

    ControlPanelState controlPanelState = ControlPanel_NothingLoaded;
    ControlPanelState prevControlPanelState = ControlPanel_NothingLoaded;
    int selectedGroup;
    // IDs of the images, index 0 for the left and single image, and index 1 for the right image.
    std::array<int, 2> selectedImages = {-1, -1};
    std::filesystem::path directoryPath;

    void renderControlPanelGroups();
    void renderControlPanelFiles();
    void renderControlPanelSaveFilenames();
    void renderControlPanelSaveImages();
    void renderSingleImageView();
    void renderCompareImageView();
    void renderImageViewOverlay(int imageView, glm::vec2 position);
    void renderPreviewProgress();
    void selectImage(int imageView, int id);
    std::string bytesToSizeString(int bytes);
    bool mouseOverlappingImage(int imageView);
};
