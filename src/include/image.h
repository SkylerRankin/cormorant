#pragma once
#include <glm/glm.hpp>
#include "glCommon.h"
#include "frame_buffer.h"

struct RectObject {
	GLuint vao;
	GLuint vbo;
};

class ImageRenderer {
public:
	ImageRenderer();
	void renderFrame();
	void updateTargetSize(glm::ivec2 newSize);
	GLint getTextureId();

private:
	glm::ivec2 imageTargetSize{ 1, 1 };
	FrameBuffer frameBuffer;

	GLuint shaderProgram;
	RectObject singleImageRect;

	void buildPassthroughShaders();
	void renderSingleImage();
};