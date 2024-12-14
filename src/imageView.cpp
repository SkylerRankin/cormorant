#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imageView.h"

ImageViewer::ImageViewer(const std::map<int, Image>& images) : images(images) {
	// Setup quad
	float quad[] = {
		// First triangle
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,

		// Second triangle
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

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

void ImageViewer::renderFrame(double elapsed, glm::ivec2 targetSize) {
	if (animatingZoom) {
		updateZoom(elapsed);
	}

	// The space available in UI to render image may have changed since last frame.
	// If so, the aspect ratio transform needs to be updated as well as the frame
	// buffer dimensions.
	updateTargetSize(targetSize);

	frameBuffer.bind();

	glViewport(0, 0, imageTargetSize.x, imageTargetSize.y);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shaderProgram);
	renderImage();

	frameBuffer.unbind();
}

void ImageViewer::updateTargetSize(glm::ivec2 newSize) {
	if (newSize == imageTargetSize) {
		return;
	}

	imageTargetSize = newSize;
	frameBuffer.update(newSize.x, newSize.y);
	updateBaseImageTransform();
}

/*
Zooming is relative to the mouse position. This means the pixel under the mouse should not be
translated when zooming. If the zoom position would require additional background space to be
shown, then the pan offset update is modified by a call to `clampPanToEdges`.
*/
void ImageViewer::zoom(int amount, glm::ivec2 position) {
	if (imageID == -1) {
		return;
	}

	targetZoom += zoomSpeed * amount;
	if (targetZoom < 0.0f) targetZoom = 0.0f;
	targetZoomPosition = position;
	animatingZoom = true;
}

/*
Move the image within a 2D plane. Positive offset values move right and down. The
vertical offset is negated so that the `panOffset` vec2 uses positives for upwards
movement.

`panOffset` stores the screen space offset from 0,0 to the current center of the image.
*/
void ImageViewer::pan(glm::ivec2 offset) {
	if (imageID == -1) {
		return;
	}

	animatingZoom = false;
	panOffset.x += offset.x / (float)imageTargetSize.x * 2.0f;
	panOffset.y += -1 * offset.y / (float)imageTargetSize.y * 2.0f;
	clampPanToEdges();
	updatePanZoomTransform();
}

void ImageViewer::setImage(int id) {
	imageID = id;
	updateBaseImageTransform();
}

GLuint ImageViewer::getTextureId() {
	return frameBuffer.getTextureId();
}

void ImageViewer::buildShaders() {
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

void ImageViewer::renderImage() {
	// TODO: when image is not available, maybe render some other loading image instead of nothing?
	if (imageID == -1 || !images.at(imageID).imageLoaded) {
		return;
	}

	const Image& image = images.at(imageID);

	glActiveTexture(GL_TEXTURE0 + imageTextureUnit);
	glBindTexture(GL_TEXTURE_2D, image.fullTextureId);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glActiveTexture(GL_TEXTURE0);
}

void ImageViewer::updateBaseImageTransform() {
	if (imageID == -1) {
		return;
	}

	const Image& image = images.at(imageID);
	if (!image.fileInfoLoaded) {
		return;
	}

	float windowAspectRatio = (float) imageTargetSize.x / imageTargetSize.y;
	float imageAspectRatio = (float) image.size.x / image.size.y;

	glm::mat4 transform = glm::mat4(1.0f);
	if (windowAspectRatio > imageAspectRatio) {
		// Shrink quad horizontally
		imageBaseScale = glm::vec3(imageAspectRatio / windowAspectRatio, 1.0f, 1.0f);
		transform = glm::scale(transform, imageBaseScale);
	} else if (windowAspectRatio < imageAspectRatio) {
		// Shrink quad vertically
		imageBaseScale = glm::vec3(1.0f, windowAspectRatio / imageAspectRatio, 1.0f);
		transform = glm::scale(transform, imageBaseScale);
	}

	glUseProgram(shaderProgram);
	GLuint transformLocation = glGetUniformLocation(shaderProgram, "baseScaleTransform");
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
}

void ImageViewer::updatePanZoomTransform() {
	glUseProgram(shaderProgram);
	glm::mat4 identity = glm::mat4(1.0f);
	float zoomFactor = getZoomFactor(currentZoom);
	glm::mat4 scale = glm::scale(identity, glm::vec3(zoomFactor, zoomFactor, 1.0f));
	glm::mat4 translate = glm::translate(identity, glm::vec3(panOffset, 0.0f));
	glm::mat4 transform = translate * scale;

	GLuint transformLocation = glGetUniformLocation(shaderProgram, "transform");
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
}

void ImageViewer::resetTransform() {
	currentZoom = 0.0f;
	panOffset = glm::vec2(0, 0);
	targetZoom = 0.0f;
	animatingZoom = false;
	updatePanZoomTransform();
}

float ImageViewer::getZoomFactor(float zoom) {
	return powf(1.25f, zoom);
}

float min(float a, float b) {
	return a < b ? a : b;
}

float max(float a, float b) {
	return a > b ? a : b;
}

float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

glm::vec2 lerp(glm::vec2 a, glm::vec2 b, float t) {
	return glm::vec2(
		lerp(a.x, b.x, t),
		lerp(a.y, b.y, t)
	);
}

void ImageViewer::clampPanToEdges() {
	float zoomFactor = getZoomFactor(currentZoom);

	// Check if left edge is too far right
	if (-imageBaseScale.x * zoomFactor + panOffset.x > max(-imageBaseScale.x * zoomFactor, -1.0f)) {
		panOffset.x = max(-imageBaseScale.x * zoomFactor, -1.0f) - (-imageBaseScale.x * zoomFactor);
	}

	// Check if right edge is too far left
	if (imageBaseScale.x * zoomFactor + panOffset.x < min(imageBaseScale.x * zoomFactor, 1.0f)) {
		panOffset.x = min(imageBaseScale.x * zoomFactor, 1.0f) - (imageBaseScale.x * zoomFactor);
	}

	// Check if top edge is too far down
	if (imageBaseScale.y * zoomFactor + panOffset.y < min(imageBaseScale.y * zoomFactor, 1.0f)) {
		panOffset.y = min(imageBaseScale.y * zoomFactor, 1.0f) - (imageBaseScale.y * zoomFactor);
	}

	// Check if bottom egge is too far up
	if (-imageBaseScale.y * zoomFactor + panOffset.y > max(-imageBaseScale.y * zoomFactor, -1.0f)) {
		panOffset.y = max(-imageBaseScale.y * zoomFactor, -1.0f) - (-imageBaseScale.y * zoomFactor);
	}
}

void ImageViewer::updateZoom(double elapsed) {
	if (abs(currentZoom - targetZoom) <= 0.01f) {
		currentZoom = targetZoom;
		animatingZoom = false;
	} else {
		float previousZoom = currentZoom;
		currentZoom += (targetZoom - currentZoom) * zoomLag * static_cast<float>(elapsed);

		// Calculate new pan offset since zoom has changed

		// Current center of image, in screen space coordinates
		glm::vec2 screenSpaceCenter = glm::vec2(panOffset.x, -1.0f * panOffset.y);
		// Position of cursor, in screen space coordinates
		glm::vec2 screenSpacePosition = glm::vec2(targetZoomPosition) / glm::vec2(imageTargetSize) * 2.0f - 1.0f;
		glm::vec2 screenSpaceDiff = screenSpacePosition - screenSpaceCenter;
		// The screen space offset from current pixel position to its position after scaling. Need to use
		// the ratio of current zoom to previous here. The original point was already scaled by the previous
		// zoom factor, so only the additional zoom should be used.
		glm::vec2 offset = screenSpaceDiff * (getZoomFactor(currentZoom) / getZoomFactor(previousZoom)) - screenSpaceDiff;
		offset.y *= -1;
		panOffset -= offset;
	}

	clampPanToEdges();
	updatePanZoomTransform();
}
