#pragma once
#include <glm/glm.hpp>
#include "cache.h"
#include "glCommon.h"
#include "frame_buffer.h"
#include "cache.h"

class ImageViewer {
public:
	ImageViewer(const std::map<int, Image>&);
	void renderFrame(double elapsed, glm::ivec2 targetSize);
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
	// Unit is %distance per second
	float zoomLag = 8.0f;
	float currentZoom = 0.0f;
	float targetZoom = 0.0f;
	bool animatingZoom = false;
	glm::vec2 targetZoomPosition;
	glm::vec2 panOffset = glm::vec2(0, 0);

	GLuint shaderProgram;
	GLuint imageTextureUnit = 1;
	int imageID = -1;
	GLuint vao;
	GLuint vbo;

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
	/*
	When animating a zoom, this updates the current zoom, slowly approaching the target zoom.
	The pan offset is also updated to align with the updated zoom.
	*/
	void updateZoom(double elapsed);
};