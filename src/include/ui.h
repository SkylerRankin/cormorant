#pragma once
#include <functional>
#include <glm/glm.hpp>
#include "glCommon.h"

typedef void(*ImageSelectedCallback)(std::string);

class UI {
public:
    UI(GLFWwindow* window, GLuint imageTexture);
    ~UI();
    void renderFrame();
    glm::ivec2 getImageTargetSize() const;
    glm::ivec2 getImageTargetPosition() const;

    void setImageSelectCallback(std::function<void(std::string)> callback);

private:
    GLuint imageTexture;
    glm::ivec2 imageTargetSize;
    glm::ivec2 imageTargetPosition;

    // Callbacks
    std::function<void(std::string)> onImageSelected;
};
