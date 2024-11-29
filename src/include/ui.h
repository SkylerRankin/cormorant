#pragma once
#include <functional>
#include <glm/glm.hpp>
#include "glCommon.h"
#include "group.h"
#include "cache.h"

typedef void(*ImageSelectedCallback)(std::string);

enum ControlPanelState {
    ControlPanel_NothingLoaded = 0,
    ControlPanel_ShowGroups = 1,
    ControlPanel_ShowFiles = 2
};

class UI {
public:
    UI(GLFWwindow* window, GLuint imageTexture, const std::vector<std::vector<int>>& groups, ImageCache* imageCache, GroupParameters& groupParameters);
    ~UI();
    void renderFrame();
    int getCurrentGroupIndex() const;
    int getCurrentImageID() const;
    glm::ivec2 getImageTargetSize() const;
    glm::ivec2 getImageTargetPosition() const;

    void setDirectoryOpenedCallback(std::function<void(std::string)> callback);
    void setGroupSelectedCallback(std::function<void(int)> callback);
    void setImageSelectedCallback(std::function<void(int)> callback);
    void setRegenerateGroupsCallback(std::function<void()> callback);

    void nextImage();
    void previousImage();
    // Called when the skipped flag on an image is changed. This is required because if the selected image is
    // now skipped, the UI should automatically move to the next unskipped image.
    void skippedImage();

private:
    const glm::ivec2 previewImageSize{75, 75};

    GLuint imageTexture;
    const std::vector<std::vector<int>>& groups;
    GroupParameters& groupParameters;
    ImageCache* imageCache;
    glm::ivec2 imageTargetSize;
    glm::ivec2 imageTargetPosition;
    bool openDirectoryPicker = false;

    ControlPanelState controlPanelState = ControlPanel_NothingLoaded;
    int selectedGroup;
    int selectedImage = -1;
    bool hideSkippedImages = true;

    void renderControlPanelGroups();
    void renderControlPanelFiles();
    std::string bytesToSizeString(int bytes);

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
    std::function<void(int)> onGroupSelected;
    std::function<void(int)> onImageSelected;
    std::function<void()> onRegenerateGroups;
};
