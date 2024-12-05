#pragma once
#include <atomic>
#include <vector>
#include "cache.h"
#include "glCommon.h"
#include "imageView.h"
#include "ui.h"
#include "group.h"

enum ProcessingState {
	ProcessingState_None = 0,
	ProcessingState_LoadingDirectory = 1,
	ProcessingState_LoadingPreviews = 2,
};

class Application {
public:
	Application(GLFWwindow* window);
	~Application();
	void frameUpdate();

	void onKeyPress(int key, int scancode, int action, int mods);
	void onMouseMove(double x, double y);
	void onMouseEntered(int entered);
	void onScroll(int offset);
	void onClick(int button, int action, int mods);
	void onWindowResized(int width, int height);

private:
	GLFWwindow* window;
	ImageViewer* imageViewer[2];
	UI* ui;
	ImageCache* cache;
	std::vector<ImageGroup> groups;
	GroupParameters groupParameters;
	ViewMode viewMode = ViewMode_Single;
	bool imageViewMovementLocked = true;

	// Input state
	bool leftClickDown = false;
	bool panningImage = false;
	int mouseActiveImage = -1;
	glm::ivec2 prevMousePosition;
	glm::ivec2 leftClickStart;

	// Data processing
	ProcessingState processingState = ProcessingState_None;
	std::atomic_bool directoryLoaded;

	// Image pre-loading
	// When an image is selected using onImageSelected, the next n and
	// previous m images are also loaded.
	int preloadNextImageCount = 2;
	int preloadPreviousImageCount = 1;

	bool mouseOverlappingImage(int imageView = 0);
	void toggleSkipImage(int id);
	void toggleSaveImage(int id);

};
