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
    UI(GLFWwindow* window, GLuint imageTexture, const std::vector<Group>& groups, ImageCache* imageCache);
    ~UI();
    void renderFrame();
    glm::ivec2 getImageTargetSize() const;
    glm::ivec2 getImageTargetPosition() const;

    void setDirectoryOpenedCallback(std::function<void(std::string)> callback);
    void setImageSelectedCallback(std::function<void(int)> callback);

private:
    const glm::ivec2 previewImageSize{75, 75};

    GLuint imageTexture;
    const std::vector<Group>& groups;
    ImageCache* imageCache;
    glm::ivec2 imageTargetSize;
    glm::ivec2 imageTargetPosition;
    bool openDirectoryPicker = false;

    ControlPanelState controlPanelState = ControlPanel_NothingLoaded;
    int selectedGroup;
    int selectedImage = -1;

    void renderControlPanelGroups();
    void renderControlPanelFiles();
    std::string bytesToSizeString(int bytes);

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
    std::function<void(int)> onImageSelected;
};
