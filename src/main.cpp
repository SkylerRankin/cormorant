#include <iostream>
#include "application.h"
#include "glCommon.h"

static void errorCallback(int error, const char* description) {
    std::cout << "GLFW error (" << error << "): " << description << std::endl;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onKeyPress(key, scancode, action, mods);
}

static void clickCallback(GLFWwindow* window, int button, int action, int mods) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onClick(button, action, mods);
}

static void mousePositionCallback(GLFWwindow* window, double x, double y) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onMouseMove(x, y);
}

static void mouseEnteredCallback(GLFWwindow* window, int entered) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onMouseEntered(entered);
}

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onScroll((int) yOffset);
}

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow *window = glfwCreateWindow(1000, 600, "cormorant", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwMakeContextCurrent(window);
    glfwSetErrorCallback(errorCallback);
    glfwSetKeyCallback(window, &keyCallback);
    glfwSetMouseButtonCallback(window, &clickCallback);
    glfwSetCursorPosCallback(window, &mousePositionCallback);
    glfwSetCursorEnterCallback(window, &mouseEnteredCallback);
    glfwSetScrollCallback(window, &scrollCallback);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL version: " << version << std::endl;

    Application app{window};
    glfwSetWindowUserPointer(window, &app);

    while (!glfwWindowShouldClose(window)) {
        app.renderFrame();
    }
    
    glfwTerminate();
    return 0;
}