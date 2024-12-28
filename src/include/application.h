#pragma once
#include <atomic>
#include <vector>
#include "config.h"
#include "glCommon.h"
#include "group.h"
#include "stats.h"

enum ProcessingState {
    ProcessingState_None = 0,
    ProcessingState_LoadingDirectory = 1,
    ProcessingState_LoadingPreviews = 2,
};

class ImageCache;
class UI;

class Application {
public:
    Application(GLFWwindow* window);
    ~Application();

    void frameUpdate();

    void onClick(int button, int action, int mods);
    void onKeyPress(int key, int scancode, int action, int mods);
    void onMouseMove(double x, double y);
    void onScroll(int offset);
    void onWindowResized(int width, int height);

private:
    GLFWwindow* window;
    Config::Config config;
    UI* ui;
    ImageCache* cache;
    Monitor* monitor;
    std::vector<Group::ImageGroup> groups;
    Group::GroupParameters groupParameters;
    double previousFrameTime;
    bool directoryOpen = false;

    // Data processing
    ProcessingState processingState = ProcessingState_None;
    std::atomic_bool directoryLoaded;

    // When an image is selected using onImageSelected, the next n and
    // previous m images are also loaded.
    int preloadNextImageCount;
    int preloadPreviousImageCount;

    void loadImageWithPreload(int id);
    void toggleSaveImage(int id);
    void toggleSkipImage(int id);
};
