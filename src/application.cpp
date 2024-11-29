#include <iostream>
#include "application.h"

Application::Application(GLFWwindow* window) : window(window) {
    cache = new ImageCache();
    imageRenderer = new ImageRenderer(cache->getImages());
    ui = new UI(window, imageRenderer->getTextureId(), groups, cache, groupParameters);

    directoryLoaded.store(false);

    // Setup UI callbacks
    ui->setDirectoryOpenedCallback([this](std::string path) -> void {
        processingState = ProcessingState_LoadingDirectory;
        cache->initCacheFromDirectory(path, directoryLoaded);
    });

    ui->setGroupSelectedCallback([this](int group) -> void {
        // Preload images in the selected group
        cache->useImagesFullTextures(groups.at(group));
    });

    ui->setImageSelectedCallback([this](int id) -> void {
        cache->useImageFullTexture(id);
        imageRenderer->setImage(id);
    });

    ui->setRegenerateGroupsCallback([this]() -> void {
        generateGroups(groups, groupParameters, cache->getImages());
    });
}

Application::~Application() {
    delete imageRenderer;
    delete cache;
    delete ui;
}

void Application::frameUpdate() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cache->frameUpdate();

    imageRenderer->renderFrame();
    ui->renderFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();

    imageRenderer->updateTargetSize(ui->getImageTargetSize());

    switch (processingState) {
    case ProcessingState_LoadingDirectory:
        if (directoryLoaded.load()) {
            directoryLoaded.store(false);
            processingState = ProcessingState_None;
            generateInitialGroup(groups, cache->getImages());
        }
        break;
    case ProcessingState_LoadingImages:
        break;
    case ProcessingState_GeneratingGroups:
        break;
    }
}

void Application::onKeyPress(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_UP || key == GLFW_KEY_LEFT || key == GLFW_KEY_W) {
            ui->previousImage();
        } else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_RIGHT || key == GLFW_KEY_S) {
            ui->nextImage();
        } else if (key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) {
            // Toggle marking image as skipped
            Image* image = cache->getImage(ui->getCurrentImageID());
            image->skipped = !image->skipped;
            image->saved = false;
            ui->skippedImage();
        } else if (key == GLFW_KEY_ENTER) {
            // Toggle marking image as saved
            Image* image = cache->getImage(ui->getCurrentImageID());
            image->saved = !image->saved;
            image->skipped = false;
        }
    }
}

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

