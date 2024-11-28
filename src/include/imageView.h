#pragma once
#include <glm/glm.hpp>
#include "cache.h"
#include "glCommon.h"
#include "frame_buffer.h"
#include "cache.h"

#define u8 unsigned char

struct RectObject {
	GLuint vao;
	GLuint vbo;
};

class ImageRenderer {
public:
	ImageRenderer(const std::map<int, Image>&);
	void renderFrame();
	void updateTargetSize(glm::ivec2 newSize);
	void zoom(int amount);
	void pan(glm::ivec2 offset);
	void setImage(int id);
	GLint getTextureId();

private:
	const std::map<int, Image>& images;
	glm::ivec2 imageTargetSize{ 1, 1 };
	glm::ivec2 imageSize;
	FrameBuffer frameBuffer;

	const float zoomSpeed = 0.5f;
	float currentZoom = 1.0f;
	glm::vec2 panOffset = glm::vec2(0, 0);

	GLuint shaderProgram;
	GLuint imageTextureUnit = 1;
	int imageID = -1;
	RectObject singleImageRect;

	void buildShaders();
	void renderSingleImage();
	void updateBaseImageTransform();
	void updatePanZoomTransform();
};