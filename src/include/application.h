#pragma once
#include "glCommon.h"
#include "image.h"
#include "ui.h"

class Application {
public:
	Application(GLFWwindow* window);
	~Application();
	void renderFrame();

	void onKeyPress(int key, int scancode, int action, int mods);
	void onMouseMove(double x, double y);
	void onMouseEntered(int entered);
	void onWindowResized(int width, int height);

private:
	GLFWwindow* window;
	ImageRenderer* imageRenderer;
	UI* ui;
};
