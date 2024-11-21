#include <iostream>
#include "image.h"

ImageRenderer::ImageRenderer() {
	// Setup quad for single images
	float singleQuad[] = {
		// First triangle
		0.5f, 0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		-0.5f, 0.5f, 0.0f,

		// Second triangle
		0.5f, -0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,
		-0.5f, 0.5f, 0.0f,
	};

	glGenVertexArrays(1, &singleImageRect.vao);
	glGenBuffers(1, &singleImageRect.vbo);

	glBindVertexArray(singleImageRect.vao);
	glBindBuffer(GL_ARRAY_BUFFER, singleImageRect.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(singleQuad), singleQuad, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	buildPassthroughShaders();
}

void ImageRenderer::renderFrame() {
	frameBuffer.bind();

	glViewport(0, 0, imageTargetSize.x, imageTargetSize.y);
	glUseProgram(shaderProgram);
	renderSingleImage();

	frameBuffer.unbind();
}

void ImageRenderer::updateTargetSize(glm::ivec2 newSize) {
	if (newSize == imageTargetSize) {
		return;
	}

	imageTargetSize = newSize;
	frameBuffer.update(newSize.x, newSize.y);
	std::cout << "Updating framebuffer size to (" << newSize.x << ", " << newSize.y << ")" << std::endl;
}

GLint ImageRenderer::getTextureId() {
	return frameBuffer.getTextureId();
}

void ImageRenderer::buildPassthroughShaders() {
	const char* vertexShaderSource = R"(
		#version 330 core
		layout (location = 0) in vec3 position;
		void main() {
			gl_Position = vec4(position, 1.0);
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 330 core
		out vec4 color;
		void main() {
			color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
		}
	)";

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);

	GLint compilationSuccess;
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compilationSuccess);
	if (!compilationSuccess) {
		GLint maxLength;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
		std::string errorLog;
		errorLog.resize(maxLength);
		glGetShaderInfoLog(vertexShader, maxLength, &maxLength, errorLog.data());
		std::cout << "Failed to compile vertex shader:\n" << errorLog << std::endl;
	}

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);

	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compilationSuccess);
	if (!compilationSuccess) {
		GLint maxLength;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);
		std::string errorLog;
		errorLog.resize(maxLength);
		glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, errorLog.data());
		std::cout << "Failed to compile fragment shader:\n" << errorLog << std::endl;
	}

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	GLint linkSuccess;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSuccess);
	if (!linkSuccess) {
		GLint maxLength;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
		std::string errorLog;
		errorLog.resize(maxLength);
		glGetShaderInfoLog(shaderProgram, maxLength, &maxLength, errorLog.data());
		std::cout << "Failed to link shaders to program:\n" << errorLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void ImageRenderer::renderSingleImage() {
	glBindVertexArray(singleImageRect.vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}