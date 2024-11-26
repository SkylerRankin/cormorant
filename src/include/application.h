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
	ProcessingState_LoadingImages = 2,
	ProcessingState_GeneratingGroups = 3,
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
	ImageRenderer* imageRenderer;
	UI* ui;
	ImageCache* cache;
	std::vector<Group> groups;

	// Input state
	bool leftClickDown = false;
	bool panningImage = false;
	glm::ivec2 prevMousePosition;
	glm::ivec2 leftClickStart;

	// Data processing
	ProcessingState processingState = ProcessingState_None;
	std::atomic_bool directoryLoaded;

	bool mouseOverlappingImage();

};
