#pragma once
#include <vector>
#include "cache.h"
#include "glCommon.h"
#include "image.h"
#include "ui.h"
#include "group.h"

class Application {
public:
	Application(GLFWwindow* window);
	~Application();
	void renderFrame();

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

	bool mouseOverlappingImage();

};
