#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <windows.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "colors.h"

std::string executableDirectory;

static void errorCallback(int error, const char* description) {
    std::cout << "GLFW error (" << error << "): " << description << std::endl;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {}

static void mousePositionCallback(GLFWwindow* window, double x, double y) {}

static void mouseEnteredCallback(GLFWwindow* window, int entered) {}

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow *window = glfwCreateWindow(1400, 800, "cormorant", NULL, NULL);
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
    glfwSetCursorPosCallback(window, &mousePositionCallback);
    glfwSetCursorEnterCallback(window, &mouseEnteredCallback);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL version: " << version << std::endl;

    WCHAR wideExecutableDirectory[MAX_PATH];
    GetModuleFileNameW(NULL, wideExecutableDirectory, MAX_PATH);
    unsigned int endIndex = MAX_PATH - 1;
    while (wideExecutableDirectory[endIndex] != '\\') {
        wideExecutableDirectory[endIndex--] = 0;
    }

    std::wstring wideString{ wideExecutableDirectory, endIndex };
    executableDirectory = std::string(wideString.begin(), wideString.end());
    std::cout << "Executable directory: " << executableDirectory << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}