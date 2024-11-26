#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imageView.h"

ImageRenderer::ImageRenderer() {
	// Setup quad for single images
	float singleQuad[] = {
		// First triangle
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,

		// Second triangle
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	};

	glGenVertexArrays(1, &singleImageRect.vao);
	glGenBuffers(1, &singleImageRect.vbo);

	glBindVertexArray(singleImageRect.vao);
	glBindBuffer(GL_ARRAY_BUFFER, singleImageRect.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(singleQuad), singleQuad, GL_STATIC_DRAW);

	int stride = 5;
	int positionIndex = 0;
	glVertexAttribPointer(positionIndex, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), 0);
	glEnableVertexAttribArray(positionIndex);

	int uvIndex = 1;
	glVertexAttribPointer(uvIndex, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void *) (sizeof(float) * 3));
	glEnableVertexAttribArray(uvIndex);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	buildShaders();
}

void ImageRenderer::renderFrame() {
	frameBuffer.bind();

	glViewport(0, 0, imageTargetSize.x, imageTargetSize.y);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

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
	updateBaseImageTransform();
}

void ImageRenderer::zoom(int amount) {
	currentZoom += zoomSpeed * amount;
	if (currentZoom < 1.0f) {
		currentZoom = 1.0f;
	}
	updatePanZoomTransform();
}

void ImageRenderer::pan(glm::ivec2 offset) {
	panOffset.x += offset.x / (float)imageTargetSize.x * 2.0f;
	panOffset.y += -1 * offset.y / (float)imageTargetSize.y * 2.0f;
	updatePanZoomTransform();
}

void ImageRenderer::setImage(const Image* image) {
	this->image = image;
	currentZoom = 1.0f;
	panOffset = glm::ivec2(0.0f, 0.0f);
	updatePanZoomTransform();
	updateBaseImageTransform();
}

GLint ImageRenderer::getTextureId() {
	return frameBuffer.getTextureId();
}

void ImageRenderer::buildShaders() {
	const char* vertexShaderSource = R"(
		#version 330 core
		layout (location = 0) in vec3 position;
		layout (location = 1) in vec2 in_uv;
		out vec2 uv;
		uniform mat4 transform;
		uniform mat4 baseScaleTransform;
		void main() {
			uv = in_uv;
			gl_Position = transform * baseScaleTransform * vec4(position, 1.0);
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 330 core
		in vec2 uv;
		out vec4 color;
		uniform sampler2D imageTexture;
		void main() {
			color = texture2D(imageTexture, uv);
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

	// Set the texture uniform to use the correct texture unit
	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "imageTexture"), imageTextureUnit);

	// Default identity matrix for the zoom/pan transform
	glm::mat4 transform = glm::mat4(1.0f);
	GLuint transformLocation = glGetUniformLocation(shaderProgram, "transform");
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void ImageRenderer::renderSingleImage() {
	// TODO: when image is not available, maybe render some other loading image instead of nothing?
	if (image == nullptr || !image->imageLoaded) {
		return;
	}

	glActiveTexture(GL_TEXTURE0 + imageTextureUnit);
	glBindTexture(GL_TEXTURE_2D, image->fullTextureId);

	glBindVertexArray(singleImageRect.vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glActiveTexture(GL_TEXTURE0);
}

void ImageRenderer::updateBaseImageTransform() {
	if (image == nullptr) {
		return;
	}

	float windowAspectRatio = (float) imageTargetSize.x / imageTargetSize.y;
	float imageAspectRatio = (float) image->size.x / image->size.y;

	glm::mat4 transform = glm::mat4(1.0f);
	if (windowAspectRatio > imageAspectRatio) {
		// Shrink quad horizontally
		transform = glm::scale(transform, glm::vec3(imageAspectRatio / windowAspectRatio, 1.0f, 1.0f));
	} else if (windowAspectRatio < imageAspectRatio) {
		// Shrink quad vertically
		transform = glm::scale(transform, glm::vec3(1.0f, windowAspectRatio / imageAspectRatio, 1.0f));
	}

	glUseProgram(shaderProgram);
	GLuint transformLocation = glGetUniformLocation(shaderProgram, "baseScaleTransform");
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
}

void ImageRenderer::updatePanZoomTransform() {
	glUseProgram(shaderProgram);
	glm::mat4 transform = glm::mat4(1.0f);
	transform = glm::translate(transform, glm::vec3(panOffset, 0.0f));
	transform = glm::scale(transform, glm::vec3(currentZoom, currentZoom, 1.0f));
	GLuint transformLocation = glGetUniformLocation(shaderProgram, "transform");
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
}
