#pragma once
#include <array>
#include <functional>
#include <glm/glm.hpp>
#include "glCommon.h"
#include "group.h"
#include "cache.h"

enum ControlPanelState {
    ControlPanel_NothingLoaded = 0,
    ControlPanel_ShowGroups = 1,
    ControlPanel_ShowFiles = 2
};

enum ViewMode {
    ViewMode_Single = 0,
    ViewMode_ManualCompare = 1,
    ViewMode_AutoCompare = 2
};

class UI {
public:
    UI(GLFWwindow* window, std::array<GLuint, 2> textureIDs, const std::vector<ImageGroup>& groups, ImageCache* imageCache, GroupParameters& groupParameters);
    ~UI();
    void renderFrame();
    int getCurrentGroupIndex() const;
    glm::ivec2 getImageTargetSize() const;
    glm::ivec2 getImageTargetPosition(int imageView = 0) const;

    // For all relevant image views, update the selected image to the next unskipped image that is not
    // also used by another image view. If there are no more unskipped images, the selected image is
    // set to -1.
    void goToNextUnskippedImage();
    void goToPreviousUnskippedImage();
    // Called when the skipped flag on an image is changed. This is required because if the selected image is
    // now skipped, the UI should automatically move to the next unskipped image.
    void skippedImage();
    // Functions for handling the preview loading progress UI
    void setShowPreviewProgress(bool enabled);
    void setPreviewProgress(float progress);

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
    std::function<void(int)> onGroupSelected;
    std::function<void(int, int)> onImageSelected;
    std::function<void(int, int)> onCompareImagesSelected;
    std::function<void()> onRegenerateGroups;
    std::function<void(int)> onViewModeUpdated;
    std::function<void(int)> onSkipImage;
    std::function<void(int)> onSaveImage;
    std::function<void(bool)> onMovementLock;
    std::function<void(int)> onResetImageTransform;

private:
    const glm::ivec2 previewImageSize{75, 75};

    std::array<GLuint, 2> imageViewTextures;
    const std::vector<ImageGroup>& groups;
    GroupParameters& groupParameters;
    ImageCache* imageCache;
    glm::ivec2 imageTargetSize;
    std::array<glm::ivec2, 2> imageTargetPositions;
    ViewMode viewMode = ViewMode_Single;
    bool showPreviewProgress = false;
    float previewProgress = 0.0f;

    ControlPanelState controlPanelState = ControlPanel_NothingLoaded;
    int selectedGroup;
    // IDs of the images, index 0 for the left and single image, and index 1 for the right image.
    std::array<int, 2> selectedImages = {-1, -1};
    bool hideSkippedImages = true;

    // Variables for deferred updates that must happena after frame is rendered
    // TODO: should these be moved into the cpp file
    bool openDirectoryPicker = false;
    bool updateViewMode = false;
    int newViewMode = -1;

    void renderControlPanelGroups();
    void renderControlPanelFiles();
    void renderSingleImageView();
    void renderCompareImageView();
    void renderImageViewOverlay(int imageView, glm::vec2 position);
    void renderPreviewProgress();
    std::string bytesToSizeString(int bytes);
};
