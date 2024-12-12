#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "application.h"
#include "cache.h"
#include "ui.h"

Application::Application(GLFWwindow* window) : window(window) {
    cache = new ImageCache();
    ui = new UI(window, config, groups, cache, groupParameters);

    directoryLoaded.store(false);

    // Setup UI callbacks
    ui->onDirectoryOpened = [this](std::string path) -> void {
        processingState = ProcessingState_LoadingDirectory;
        ui->startLoadingImages();
        cache->initCacheFromDirectory(path, directoryLoaded);
        directoryPath = std::filesystem::path{ path };
    };

    ui->onGroupSelected = [this](int group) -> void {
        // Preload images in the selected group
        cache->useImagesFullTextures(groups.at(group).ids);
    };

    ui->onImageSelected = [this](int id) -> void {
        loadImageWithPreload(id);
    };

    ui->onRegenerateGroups = [this]() -> void {
        generateGroups(groups, groupParameters, cache->getImages());
    };

    ui->onSaveImage = [this](int id) -> void {
        toggleSaveImage(id);
    };

    ui->onSkipImage = [this](int id) -> void {
        toggleSkipImage(id);
        ui->skippedImage();
    };
}

Application::~Application() {
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
    ui->renderFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();

    switch (processingState) {
    case ProcessingState_LoadingDirectory:
        if (directoryLoaded.load()) {
            directoryLoaded.store(false);
            processingState = ProcessingState_LoadingPreviews;
            ui->setShowPreviewProgress(true);
            cache->startInitialTextureLoads();
            generateInitialGroup(groups, cache->getImages());
            ui->setControlPanelState(ControlPanel_ShowGroups);
        }
        break;
    case ProcessingState_LoadingPreviews:
        if (cache->previewLoadingComplete()) {
            processingState = ProcessingState_None;
            ui->endLoadingImages();
        }
        break;
    }
}

void Application::onKeyPress(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        ui->inputKey(key);
    }
}

void Application::onClick(int button, int action, int mods) {
    ui->inputClick(button, action, mods);
}

void Application::onMouseMove(double x, double y) {
    ui->inputMouseMove(x, y);
}

void Application::onWindowResized(int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::onScroll(int offset) {
    ui->inputScroll(offset);
}

void Application::loadImageWithPreload(int id) {
    int indexInGroup = -1;
    std::vector<int>& group = groups.at(ui->getCurrentGroupIndex()).ids;
    for (int i = 0; i < group.size(); i++) {
        if (group[i] == id) {
            indexInGroup = i;
            break;
        }
    }

    if (indexInGroup == -1) {
        cache->useImageFullTexture(id);
    } else {
        // Also load a few images ahead and behind when loading the selected image.
        const auto& images = cache->getImages();
        std::vector<int> imageIDs;
        imageIDs.push_back(id);

        for (int i = 1; i <= preloadNextImageCount; i++) {
            if (indexInGroup + i >= group.size()) break;
            imageIDs.push_back(group.at(indexInGroup + i));
        }

        for (int i = 1; i <= preloadPreviousImageCount; i++) {
            if (indexInGroup - i < 0) break;
            imageIDs.push_back(group.at(indexInGroup - i));
        }

        cache->useImagesFullTextures(imageIDs);
    }
}

void Application::toggleSkipImage(int id) {
    Image* image = cache->getImage(id);

    // TODO: have a mapping from image id to group index to avoid this linear search
    for (ImageGroup& group : groups) {
        if (std::find(group.ids.begin(), group.ids.end(), id) != group.ids.end()) {
            if (image->skipped) {
                group.skippedCount--;
            } else {
                group.skippedCount++;
                if (image->saved) {
                    group.savedCount--;
                }
            }
            break;
        }
    }

    image->skipped = !image->skipped;
    image->saved = false;
}

void Application::toggleSaveImage(int id) {
    Image* image = cache->getImage(id);

    for (ImageGroup& group : groups) {
        if (std::find(group.ids.begin(), group.ids.end(), id) != group.ids.end()) {
            if (image->saved) {
                group.savedCount--;
            } else {
                group.savedCount++;
                if (image->skipped) {
                    group.skippedCount--;
                }
            }
            break;
        }
    }

    image->saved = !image->saved;
    image->skipped = false;
}
