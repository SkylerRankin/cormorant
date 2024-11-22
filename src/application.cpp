#include <iostream>
#include "application.h"

Application::Application(GLFWwindow* window) : window(window) {
    imageRenderer = new ImageRenderer();
    ui = new UI(window, imageRenderer->getTextureId());

    // Setup UI callbacks
    ui->setImageSelectCallback([this](std::string path) -> void {
        imageRenderer->loadImage(path);
    });

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

Application::~Application() {
    delete imageRenderer;
    delete ui;
}

void Application::renderFrame() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    imageRenderer->renderFrame();
    ui->renderFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();

    imageRenderer->updateTargetSize(ui->getImageTargetSize());
}

void Application::onKeyPress(int key, int scancode, int action, int mods) {}

void Application::onClick(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftClickDown = true;
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            leftClickStart = glm::ivec2(mouseX, mouseY);
            prevMousePosition = glm::ivec2(mouseX, mouseY);

            panningImage = mouseOverlappingImage();
        } else if (action == GLFW_RELEASE) {
            leftClickDown = false;
            panningImage = false;
        }
    }
}

void Application::onMouseMove(double x, double y) {
    if (leftClickDown) {
        glm::ivec2 dragOffset = glm::ivec2(x, y) - prevMousePosition;
        if (panningImage) {
            imageRenderer->pan(dragOffset);
        }
    }
    prevMousePosition = glm::ivec2(x, y);
}

void Application::onMouseEntered(int entered) {}

void Application::onWindowResized(int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::onScroll(int offset) {
    if (mouseOverlappingImage()) {
        imageRenderer->zoom(offset);
    }
}

bool Application::mouseOverlappingImage() {
    glm::ivec2 imagePosition = ui->getImageTargetPosition();
    glm::ivec2 imageSize = ui->getImageTargetSize();

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    return imagePosition.x <= mouseX && mouseX <= imagePosition.x + imageSize.x &&
           imagePosition.y <= mouseY && mouseY <= imagePosition.y + imageSize.y;
}

