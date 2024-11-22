#pragma once
#include <glm/glm.hpp>
#include "glCommon.h"
#include "frame_buffer.h"

#define u8 unsigned char

struct RectObject {
	GLuint vao;
	GLuint vbo;
};

class ImageRenderer {
public:
	ImageRenderer();
	void renderFrame();
	void updateTargetSize(glm::ivec2 newSize);
	void loadImage(std::string path);
	GLint getTextureId();

private:
	glm::ivec2 imageTargetSize{ 1, 1 };
	glm::ivec2 imageSize;
	FrameBuffer frameBuffer;

	GLuint shaderProgram;
	GLuint imageTextureUnit = 1;
	GLuint imageTexture;
	RectObject singleImageRect;

	void buildPassthroughShaders();
	void renderSingleImage();
	void updateBaseImageTransform();
};