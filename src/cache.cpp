#include <filesystem>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include "glCommon.h"
#include "cache.h"

namespace fs = std::filesystem;

ImageCache::ImageCache() {
	const int channels = 3;
	previewTextureBackground = new unsigned char[previewTextureSize.x * previewTextureSize.y * channels];
	for (int x = 0; x < previewTextureSize.x; x++) {
		for (int y = 0; y < previewTextureSize.y; y++) {
			int i = y * previewTextureSize.x * channels + x * channels;
			previewTextureBackground[i + 0] = previewBackgroundColor.r;
			previewTextureBackground[i + 1] = previewBackgroundColor.g;
			previewTextureBackground[i + 2] = previewBackgroundColor.b;
		}
	}
}

ImageCache::~ImageCache() {
	delete[] previewTextureBackground;
}

void ImageCache::loadDirectory(std::string path) {
	for (const auto& entry : fs::directory_iterator{ fs::path(path) }) {
		std::string entryPathString{ reinterpret_cast<const char*>(entry.path().u8string().c_str()) };
		fs::path entryPath{ entryPathString };

		if (fs::is_regular_file(entryPath) && entryPath.extension().compare(".JPG") == 0) {
			Image image;
			image.filename = entryPath.filename().string();
			image.filesize = fs::file_size(entryPath);
			loadImage(entryPath.string(), image);
			images.push_back(image);
		}
	}
}

const Image& ImageCache::getImage(int index) const {
	return images.at(index);
}

const std::vector<Image>& ImageCache::getImages() const {
	return images;
}

void ImageCache::loadImage(std::string path, Image& image) {
	stbi_set_flip_vertically_on_load(true);
	int width = 0, height = 0, channels = 0;
	unsigned char* imageData = stbi_load(path.c_str(), &width, &height, &channels, 3);
	image.size = glm::ivec2(width, height);
	image.previewSize = previewTextureSize;

	if (imageData == nullptr) {
		std::cout << "Failed to load image " << path << std::endl;
		return;
	}

	if (channels != 3) {
		std::cout << "Unexpected channel count (" << channels << ") for image at " << path << std::endl;
		return;
	}

	// Determine the preview image size that matches the original image aspect ratio, but maximally fits
	// within the preview image size, which may have a different aspect ratio.
	const float aspectRatio = width / (float)height;
	const float previewAspectRatio = previewTextureSize.x / (float) previewTextureSize.y;
	glm::ivec2 resizeSize = previewTextureSize;
	if (aspectRatio > previewAspectRatio) {
		resizeSize.y *= (previewAspectRatio / aspectRatio);
	} else if (aspectRatio < previewAspectRatio) {
		resizeSize.x *= (aspectRatio / previewAspectRatio);
	}

	// Resize original image into preview image size.
	const int resizedImageBytes = resizeSize.x * resizeSize.y * channels;
	unsigned char* resizedImageData = new unsigned char[resizedImageBytes];
	stbir_resize_uint8_srgb(imageData, width, height, 0, resizedImageData, resizeSize.x, resizeSize.y, 0, STBIR_RGB);

	// Place resized image into a larger image that covered the full preview texture size.
	unsigned char* previewImageData;
	const int previewImageBytes = previewTextureSize.x * previewTextureSize.y * channels;
	if (aspectRatio == previewAspectRatio) {
		// Aspect ratios were equal, to the resized image covers the entire preview texture.
		previewImageData = resizedImageData;
	} else if (aspectRatio > previewAspectRatio) {
		// Image has a larger w/h ratio, so bars on top/bottom are required.
		previewImageData = new unsigned char[previewImageBytes];
		memcpy(previewImageData, previewTextureBackground, previewImageBytes);

		int barHeight = (previewTextureSize.y - resizeSize.y) / 2;
		int middleOffset = barHeight * previewTextureSize.x * channels;
		memcpy(previewImageData + middleOffset, resizedImageData, resizedImageBytes);
	} else {
		// Image has a smaller w/h ratio, so bars on sides are required.
		previewImageData = new unsigned char[previewImageBytes];
		memcpy(previewImageData, previewTextureBackground, previewImageBytes);

		int barWidth = (previewTextureSize.x - resizeSize.x) / 2;
		for (int y = 0; y < previewTextureSize.y; y++) {
			memcpy(previewImageData + (y * previewTextureSize.x * channels) + (barWidth * channels), resizedImageData + (y * resizeSize.x * channels), resizeSize.x * channels);
		}
	}

	// Create texture for full resolution image
	glGenTextures(1, &image.fullTextureId);
	glBindTexture(GL_TEXTURE_2D, image.fullTextureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Create texture for previous image
	glGenTextures(1, &image.previewTextureId);
	glBindTexture(GL_TEXTURE_2D, image.previewTextureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, previewTextureSize.x, previewTextureSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, previewImageData);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(imageData);
	if (previewImageData == resizedImageData) {
		delete[] previewImageData;
	} else {
		delete[] resizedImageData;
		delete[] previewImageData;
	}

	image.fullTextureLoaded = true;
}
