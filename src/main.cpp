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

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->onScroll((int) yOffset);
}

static void debugCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam) {

    if (id == 131185) return;
    if (id == 131154) return; // Pixel-path performance warning: Pixel transfer is synchronized with 3D rendering.
    if (id == 131218) return; // Program/shader state performance warning: Vertex shader in program 3 is being recompiled based on GL state.

    std::cout << std::format("Debug message: source={} type={} id={} sev={} len={} msg={}", source, type, id, severity, length, message) << std::endl;
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
    glfwSetScrollCallback(window, &scrollCallback);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallback, nullptr);

    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL version: " << version << std::endl;

    Application app{window};
    glfwSetWindowUserPointer(window, &app);

    while (!glfwWindowShouldClose(window)) {
        app.frameUpdate();
    }
    
    glfwTerminate();
    return 0;
}