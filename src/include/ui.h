#pragma once
#include <glm/glm.hpp>
#include "glCommon.h"

class UI {
public:
    UI(GLFWwindow* window, GLuint imageTexture);
    ~UI();
    void renderFrame();
    glm::ivec2 getImageTargetSize() const;
private:
    GLuint imageTexture;
    glm::ivec2 imageTargetSize;
};
