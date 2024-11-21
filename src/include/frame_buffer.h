#pragma once
#include "glCommon.h"

class FrameBuffer {
public:
	FrameBuffer();
	FrameBuffer(int width, int height);
	~FrameBuffer();
	GLuint getTextureId();
	void update(int width, int height);
	void bind();
	void unbind();

private:
	GLuint fbo;
	GLuint texture;
	GLuint rbo;
};
