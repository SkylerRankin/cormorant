#include <filesystem>
#include <iostream>
#include <thread>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include "glCommon.h"
#include "cache.h"

namespace fs = std::filesystem;

int ImageCache::nextImageID = 0;

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

	unsigned int hardwareThreads = std::thread::hardware_concurrency();
	if (hardwareThreads > 0) {
		imageLoadThreads = hardwareThreads;
	}

	runImageLoadThreads.store(true);
	startImageLoadingThreads();
}

ImageCache::~ImageCache() {
	runImageLoadThreads.store(false);
	delete[] previewTextureBackground;
}

void ImageCache::frameUpdate() {
	processTextureQueue();
}

void ImageCache::initCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded) {
	std::thread thread(&ImageCache::threadInitCacheFromDirectory, this, path, std::ref(directoryLoaded));
	thread.detach();
}

void ImageCache::startImageLoadingThreads() {
	for (int i = 0; i < imageLoadThreads; i++) {
		std::thread thread(&ImageCache::runImageLoadThread, this, i);
		thread.detach();
	}
}

const std::map<int, Image>& ImageCache::getImages() const {
	return images;
}

void ImageCache::threadInitCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded) {
	// TODO: clear all previous stored images from memory
	images.clear();
	for (const auto& entry : fs::directory_iterator{ fs::path(path) }) {
		std::string entryPathString{ reinterpret_cast<const char*>(entry.path().u8string().c_str()) };
		fs::path entryPath{ entryPathString };

		if (fs::is_regular_file(entryPath) && entryPath.extension().compare(".JPG") == 0) {
			Image image;
			image.id = nextImageID++;
			image.path = entryPathString;
			image.filename = entryPath.filename().string();
			image.filesize = fs::file_size(entryPath);
			images.emplace(image.id, image);
		}
	}
	directoryLoaded.store(true);

	// Add first n images to queue for full processing, and the remaining images
	// for just preview processing.
	std::lock_guard<std::mutex> guard(imageQueueMutex);
	int i = 0;
	while (i < cacheCapacity && i < images.size()) {
		ImageQueueEntry entry{ i, true, true };
		imageQueue.push_back(entry);
		i++;
	}
	while (i < images.size()) {
		ImageQueueEntry entry{ i, false, true };
		imageQueue.push_back(entry);
		i++;
	}
	std::cout << "notifing condition var, image queue size = " << imageQueue.size() << std::endl;
	imageQueueConditionVariable.notify_all();
}

void ImageCache::runImageLoadThread(int id) {
	while (runImageLoadThreads.load()) {
		// Wait for condition variable signal
		std::unique_lock<std::mutex> lock(imageQueueMutex);
		std::cout << "thread " << id << " waiting" << std::endl;
		imageQueueConditionVariable.wait(lock);
		std::cout << "thread " << id << " notified" << std::endl;
		lock.unlock();

		while (true) {
			ImageQueueEntry entry;
			{
				std::lock_guard<std::mutex> guard(imageQueueMutex);
				if (imageQueue.size() == 0) {
					std::cout << "thread " << id << " nothing left in queue, exiting" << std::endl;
					break;
				} else {
					entry = imageQueue.front();
					imageQueue.pop_front();
					std::cout << "thread " << id << " processing image " << entry.imageID << " new queue size = " << imageQueue.size() << std::endl;
				}
			}

			unsigned char* imageData;
			unsigned char* previewData;

			// TODO: is this thread safe? image with that id might have been deleted from cache while loading from file
			images.at(entry.imageID).status = ImageStatus_Loading;
			if (loadImageFromFile(entry.imageID, imageData, previewData, !entry.loadFullTexture, !entry.loadPreviewTexture)) {
				// TODO: the image data is loaded, but its not yet available on the GPU. this status shouldnt be called loaded
				images.at(entry.imageID).status = ImageStatus_Loaded;

				TextureQueueEntry textureQueueEntry{ entry.imageID, imageData, previewData };
				{
					std::lock_guard<std::mutex> guard(textureQueueMutex);
					textureQueue.push_back(textureQueueEntry);
				}
			} else {
				images.at(entry.imageID).status = ImageStatus_Failed;
			}
		}
	}
}

bool ImageCache::loadImageFromFile(int id, unsigned char*& imageData, unsigned char*& previewData, bool skipFullResolution, bool skipPreview) {
	Image* image = &images.at(id);
	stbi_set_flip_vertically_on_load(true);
	int width = 0, height = 0, channels = 0;
	imageData = stbi_load(image->path.c_str(), &width, &height, &channels, 3);
	image->size = glm::ivec2(width, height);
	image->previewSize = previewTextureSize;

	if (imageData == nullptr) {
		std::cout << "Failed to load image " << image->path << std::endl;
		return false;
	}

	if (channels != 3) {
		std::cout << "Unexpected channel count (" << channels << ") for image at " << image->path << std::endl;
		return false;
	}

	// Determine the preview image size that matches the original image aspect ratio, but maximally fits
	// within the preview image size, which may have a different aspect ratio.
	const float aspectRatio = width / (float)height;
	const float previewAspectRatio = previewTextureSize.x / (float)previewTextureSize.y;
	glm::ivec2 resizeSize = previewTextureSize;
	if (aspectRatio > previewAspectRatio) {
		resizeSize.y *= (previewAspectRatio / aspectRatio);
	} else if (aspectRatio < previewAspectRatio) {
		resizeSize.x *= (aspectRatio / previewAspectRatio);
	}

	if (skipPreview) {
		previewData = nullptr;
	} else {
		// Resize original image into preview image size.
		const int resizedImageBytes = resizeSize.x * resizeSize.y * channels;
		unsigned char* resizedImageData = new unsigned char[resizedImageBytes];
		stbir_resize_uint8_srgb(imageData, width, height, 0, resizedImageData, resizeSize.x, resizeSize.y, 0, STBIR_RGB);

		// Place resized image into a larger image that covered the full preview texture size.
		const int previewImageBytes = previewTextureSize.x * previewTextureSize.y * channels;
		if (aspectRatio == previewAspectRatio) {
			// Aspect ratios were equal, so the resized image covers the entire preview texture.
			previewData = resizedImageData;
		} else if (aspectRatio > previewAspectRatio) {
			// Image has a larger w/h ratio, so bars on top/bottom are required.
			previewData = new unsigned char[previewImageBytes];
			memcpy(previewData, previewTextureBackground, previewImageBytes);

			int barHeight = (previewTextureSize.y - resizeSize.y) / 2;
			int middleOffset = barHeight * previewTextureSize.x * channels;
			memcpy(previewData + middleOffset, resizedImageData, resizedImageBytes);
			delete[] resizedImageData;
		} else {
			// Image has a smaller w/h ratio, so bars on sides are required.
			previewData = new unsigned char[previewImageBytes];
			memcpy(previewData, previewTextureBackground, previewImageBytes);

			int barWidth = (previewTextureSize.x - resizeSize.x) / 2;
			for (int y = 0; y < previewTextureSize.y; y++) {
				memcpy(previewData + (y * previewTextureSize.x * channels) + (barWidth * channels), resizedImageData + (y * resizeSize.x * channels), resizeSize.x * channels);
			}
			delete[] resizedImageData;
		}
	}

	if (skipFullResolution) {
		stbi_image_free(imageData);
		imageData = nullptr;
	}

	return true;
}

const Image& ImageCache::getImage(int index) const {
	return images.at(index);
}

void ImageCache::processTextureQueue() {
	for (int i = 0; i < textureQueue.size() && i < textureQueueEntriesPerFrame; i++) {
		TextureQueueEntry entry;
		{
			std::lock_guard<std::mutex> guard(textureQueueMutex);
			entry = textureQueue.front();
			textureQueue.pop_front();
		}

		Image& image = images.at(entry.imageID);

		// Create texture for full resolution image
		glGenTextures(1, &image.fullTextureId);
		glBindTexture(GL_TEXTURE_2D, image.fullTextureId);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.size.x, image.size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, entry.imageData);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Create texture for previous image
		glGenTextures(1, &image.previewTextureId);
		glBindTexture(GL_TEXTURE_2D, image.previewTextureId);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.previewSize.x, image.previewSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, entry.previewData);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (entry.imageData != nullptr) {
			stbi_image_free(entry.imageData);
		}
		if (entry.previewData != nullptr) {
			delete[] entry.previewData;
		}
		image.status = ImageStatus_Loaded;
	}
}
