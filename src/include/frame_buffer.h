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
	const GLuint textureUnit = GL_TEXTURE0;
	GLuint fbo;
	GLuint texture;
	GLuint rbo;
};
