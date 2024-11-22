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

void Application::onMouseMove(double x, double y) {}

void Application::onMouseEntered(int entered) {}

void Application::onWindowResized(int width, int height) {
    glViewport(0, 0, width, height);
}
