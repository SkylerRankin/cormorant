#include <iostream>
#include "application.h"

Application::Application(GLFWwindow* window) : window(window) {
    cache = new ImageCache();
    imageViewer[0] = new ImageViewer(cache->getImages());
    imageViewer[1] = new ImageViewer(cache->getImages());
    std::array<GLuint, 2> imageViewerTextures = { imageViewer[0]->getTextureId(), imageViewer[1]->getTextureId() };
    ui = new UI(window, imageViewerTextures, groups, cache, groupParameters);

    directoryLoaded.store(false);

    // Setup UI callbacks
    ui->onDirectoryOpened = [this](std::string path) -> void {
        processingState = ProcessingState_LoadingDirectory;
        ui->setControlPanelState(ControlPanel_DirectoryLoading);
        cache->initCacheFromDirectory(path, directoryLoaded);
    };

    ui->onGroupSelected = [this](int group) -> void {
        // Preload images in the selected group
        cache->useImagesFullTextures(groups.at(group).ids);
    };

    ui->onImageSelected = [this](int imageView, int id) -> void {
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

        imageViewer[imageView]->setImage(id);
        if (viewMode == ViewMode_Single) {
            imageViewer[imageView]->resetTransform();
        }
    };

    ui->onRegenerateGroups = [this]() -> void {
        generateGroups(groups, groupParameters, cache->getImages());
    };

    ui->onViewModeUpdated = [this](int i) -> void {
        if (static_cast<ViewMode>(i) != viewMode) {
            viewMode = static_cast<ViewMode>(i);
            imageViewer[0]->setImage(-1);
            imageViewer[1]->setImage(-1);
            imageViewer[0]->resetTransform();
            imageViewer[1]->resetTransform();
        }
    };

    ui->onSaveImage = [this](int id) -> void {
        toggleSaveImage(id);
    };

    ui->onSkipImage = [this](int id) -> void {
        toggleSkipImage(id);
    };

    ui->onMovementLock = [this](bool locked) -> void {
        imageViewMovementLocked = locked;
        imageViewer[0]->resetTransform();
        imageViewer[1]->resetTransform();
    };

    ui->onResetImageTransform = [this](int imageView) -> void {
        if (imageViewMovementLocked) {
            imageViewer[0]->resetTransform();
            imageViewer[1]->resetTransform();
        } else {
            imageViewer[imageView]->resetTransform();
        }
    };
}

Application::~Application() {
    delete imageViewer[0];
    delete imageViewer[1];
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

    imageViewer[0]->renderFrame();
    if (viewMode != ViewMode_Single) {
        imageViewer[1]->renderFrame();
    }

    ui->renderFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();

    imageViewer[0]->updateTargetSize(ui->getImageTargetSize());
    imageViewer[1]->updateTargetSize(ui->getImageTargetSize());

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
            ui->setShowPreviewProgress(false);
        } else {
            ui->setPreviewProgress(cache->getPreviewLoadProgress());
        }
        break;
    }
}

void Application::onKeyPress(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_UP || key == GLFW_KEY_LEFT || key == GLFW_KEY_W) {
            ui->goToPreviousUnskippedImage();
        } else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_RIGHT || key == GLFW_KEY_S) {
            ui->goToNextUnskippedImage();
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

            bool overlaps[2] = {
                mouseOverlappingImage(0),
                mouseOverlappingImage(1)
            };

            panningImage = overlaps[0] || overlaps[1];
            if (overlaps[0]) {
                mouseActiveImage = 0;
            } else if (overlaps[1]) {
                mouseActiveImage = 1;
            } else {
                mouseActiveImage = -1;
            }
        } else if (action == GLFW_RELEASE) {
            leftClickDown = false;
            panningImage = false;
            mouseActiveImage = -1;
        }
    }
}

void Application::onMouseMove(double x, double y) {
    if (leftClickDown) {
        glm::ivec2 dragOffset = glm::ivec2(x, y) - prevMousePosition;
        if (panningImage) {
            if (imageViewMovementLocked) {
                imageViewer[0]->pan(dragOffset);
                imageViewer[1]->pan(dragOffset);
            } else {
                imageViewer[mouseActiveImage]->pan(dragOffset);
            }
        }
    }
    prevMousePosition = glm::ivec2(x, y);
}

void Application::onMouseEntered(int entered) {}

void Application::onWindowResized(int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::onScroll(int offset) {
    if (mouseOverlappingImage(0)) {
        imageViewer[0]->zoom(offset, prevMousePosition - ui->getImageTargetPosition(0));
        if (imageViewMovementLocked) {
            imageViewer[1]->zoom(offset, prevMousePosition - ui->getImageTargetPosition(0));
        }
    } else if (mouseOverlappingImage(1)) {
        imageViewer[1]->zoom(offset, prevMousePosition - ui->getImageTargetPosition(1));
        if (imageViewMovementLocked) {
            imageViewer[0]->zoom(offset, prevMousePosition - ui->getImageTargetPosition(1));
        }
    }
}

bool Application::mouseOverlappingImage(int imageView) {
    // Prevent second image viewer from being interactable in single image mode.
    if (imageView == 1 && viewMode == ViewMode_Single) {
        return false;
    }

    glm::ivec2 imagePosition = ui->getImageTargetPosition();
    glm::ivec2 imageSize = ui->getImageTargetSize();

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    switch (viewMode) {
    case ViewMode_Single:
        return imagePosition.x <= mouseX && mouseX <= imagePosition.x + imageSize.x &&
               imagePosition.y <= mouseY && mouseY <= imagePosition.y + imageSize.y;
    case ViewMode_ManualCompare:
    case ViewMode_AutoCompare:
        if (imageView == 0) {
            return imagePosition.x <= mouseX && mouseX <= imagePosition.x + imageSize.x &&
                   imagePosition.y <= mouseY && mouseY <= imagePosition.y + imageSize.y;
        } else if (imageView == 1) {
            return imagePosition.x + imageSize.x <= mouseX && mouseX <= imagePosition.x + 2 * imageSize.x &&
                   imagePosition.y <= mouseY && mouseY <= imagePosition.y + imageSize.y;
        }
    }

    return false;
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
    ui->skippedImage();
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
