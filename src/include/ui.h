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
    UI(GLFWwindow* window, GLuint imageTexture, const std::vector<Group>& groups, const ImageCache* imageCache);
    ~UI();
    void renderFrame();
    glm::ivec2 getImageTargetSize() const;
    glm::ivec2 getImageTargetPosition() const;

    void setDirectoryOpenedCallback(std::function<void(std::string)> callback);

private:
    const glm::ivec2 previewImageSize{75, 75};

    GLuint imageTexture;
    const std::vector<Group>& groups;
    const ImageCache* imageCache;
    glm::ivec2 imageTargetSize;
    glm::ivec2 imageTargetPosition;

    ControlPanelState controlPanelState = ControlPanel_ShowGroups;

    void openImageDirectory();

    // Callbacks
    std::function<void(std::string)> onDirectoryOpened;
};
