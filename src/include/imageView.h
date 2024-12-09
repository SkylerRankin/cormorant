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

class ImageViewer {
public:
	ImageViewer(const std::map<int, Image>&);
	void renderFrame(glm::ivec2 targetSize);
	void updateTargetSize(glm::ivec2 newSize);
	void zoom(int amount, glm::ivec2 position);
	void pan(glm::ivec2 offset);
	void setImage(int id);
	GLuint getTextureId();
	void resetTransform();

private:
	const std::map<int, Image>& images;
	glm::ivec2 imageTargetSize{ 1, 1 };
	glm::ivec2 imageSize;
	glm::vec3 imageBaseScale;
	FrameBuffer frameBuffer;

	const float zoomSpeed = 0.75f;
	float currentZoom = 0.0f;
	glm::vec2 panOffset = glm::vec2(0, 0);

	GLuint shaderProgram;
	GLuint imageTextureUnit = 1;
	int imageID = -1;
	RectObject singleImageRect;

	void buildShaders();
	void renderImage();
	void updateBaseImageTransform();
	void updatePanZoomTransform();
	float getZoomFactor(float zoom);
	/*
	Modifies the pan offset so that backgrouund space is never shown unless required by
	the image's aspect ratio.
	*/
	void clampPanToEdges();
};