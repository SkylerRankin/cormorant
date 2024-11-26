#include <filesystem>
#include <iostream>
#include <thread>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include "glCommon.h"
#include "cache.h"
#include <TinyEXIF.h>

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

	// Add enough images for full processing to saturate cache, and then add the remaining images
	// for just preview processing.
	int id = 0;
	while (id < cacheCapacity && id < images.size()) {
		getImageWithCacheUpdate(id);
		id++;
	}

	std::lock_guard<std::mutex> guard(imageQueueMutex);
	while (id < images.size()) {
		ImageQueueEntry entry{ id, false, true };
		imageQueue.push_back(entry);
		id++;
	}

	imageQueueConditionVariable.notify_all();
}

void ImageCache::runImageLoadThread(int id) {
	while (runImageLoadThreads.load()) {
		// Wait for condition variable signal
		std::unique_lock<std::mutex> lock(imageQueueMutex);
		imageQueueConditionVariable.wait(lock);
		lock.unlock();

		// Pull items from image queue as long as there are more entries
		while (true) {
			ImageQueueEntry entry;
			{
				std::lock_guard<std::mutex> guard(imageQueueMutex);
				if (imageQueue.size() == 0) {
					// No more entries in image queue, break and go back to waiting on condition
					break;
				} else {
					entry = imageQueue.front();
					imageQueue.pop_front();
				}
			}

			unsigned char* imageData;
			unsigned char* previewData;

			if (loadImageFromFile(entry.imageID, imageData, previewData, !entry.loadFullTexture, !entry.loadPreviewTexture)) {
				TextureQueueEntry textureQueueEntry{ entry.imageID, imageData, previewData };
				{
					std::lock_guard<std::mutex> guard(textureQueueMutex);
					textureQueue.push_back(textureQueueEntry);
				}
			} else {
				// TODO: is this thread safe?
				images.at(entry.imageID).imageLoaded = false;
				images.at(entry.imageID).previewLoaded = false;
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
	
	if (!image->fileInfoLoaded) {
		std::ifstream stream(image->path, std::ios::binary);
		if (!stream) {
			std::cout << "Failed to open file stream for image " << image->path << std::endl;
		} else {
			TinyEXIF::EXIFInfo exif{ stream };
			if (exif.Fields) {
				image->metadata.cameraMake = exif.Make;
				image->metadata.cameraModel = exif.Model;
				image->metadata.aperture = exif.FNumber;
				image->metadata.shutterSpeed = exif.ExposureTime;
				image->metadata.iso = exif.ISOSpeedRatings;
				image->metadata.focalLength = exif.FocalLength;
				image->metadata.bitsPerSample = exif.BitsPerSample;
				image->metadata.resolution = glm::vec2(exif.XResolution, exif.YResolution);

				// Preference order for timestamps: original > digitized > last modified
				if (!exif.DateTimeOriginal.empty()) {
					image->metadata.timestamp = parseEXIFTimestamp(exif.DateTimeOriginal);
				} else if (!exif.DateTimeDigitized.empty()) {
					image->metadata.timestamp = parseEXIFTimestamp(exif.DateTimeDigitized);
				} else {
					image->metadata.timestamp = parseEXIFTimestamp(exif.DateTime);
				}
			}
		}
		stream.close();
	}

	image->fileInfoLoaded = true;

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
		if (entry.imageData != nullptr) {
			glGenTextures(1, &image.fullTextureId);
			glBindTexture(GL_TEXTURE_2D, image.fullTextureId);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.size.x, image.size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, entry.imageData);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(entry.imageData);
			image.imageLoaded = true;
		}

		// Create texture for preview image
		if (entry.previewData != nullptr) {
			glGenTextures(1, &image.previewTextureId);
			glBindTexture(GL_TEXTURE_2D, image.previewTextureId);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.previewSize.x, image.previewSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, entry.previewData);
			glBindTexture(GL_TEXTURE_2D, 0);
			delete[] entry.previewData;
			image.previewLoaded = true;
		}
	}
}

const Image* ImageCache::getImage(int id) {
	return &images.at(id);
}

const Image* ImageCache::getImageWithCacheUpdate(int id) {
	assert(images.contains(id) && std::format("Image cache has no entry for id {}", id).c_str());

	const Image& image = images.at(id);

	if (!image.imageLoaded) {
		// This image does not have a full resolution texture loaded. Add it
		// to the image loading queue. Only one thread needs to be notified to
		// process this entry.
		std::lock_guard<std::mutex> guard(imageQueueMutex);
		ImageQueueEntry entry{ id, true, !image.previewLoaded };
		imageQueue.push_back(entry);
		imageQueueConditionVariable.notify_one();
	}

	if (imageToLRUNode.contains(id)) {
		// This image is already somewhere in the LRU list. Move it to the end, making it
		// the new most recently used image.
		useLRUNode(imageToLRUNode.at(id));
	} else {
		// This image is not in the LRU list. Add it to the end and evict the oldest image
		// if necessary.
		if (imageToLRUNode.size() == cacheCapacity) {
			evictOldestImage();
		}
		addLRUNode(id);
	}

	return &image;
}

void ImageCache::addLRUNode(int id) {
	LRUNode* node = new LRUNode();
	node->imageID = id;
	node->next = nullptr;
	node->prev = lruEnd;

	if (lruEnd == nullptr) {
		lruEnd = node;
		lruHead = node;
	} else {
		lruEnd->next = node;
		lruEnd = node;
	}

	imageToLRUNode.emplace(id, node);
}

void ImageCache::useLRUNode(LRUNode* node) {
	if (node == lruEnd) {
		// Node is already at the end of the list, nothing to do.
		return;
	}

	// Remove node from its position
	if (node == lruHead) {
		// Node is first in list
		lruHead = node->next;
		lruHead->prev = nullptr;
	} else {
		// Node has other nodes before and after
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	node->next = nullptr;
	node->prev = nullptr;

	// Add node to the end of LRU list
	lruEnd->next = node;
	node->prev = lruEnd;
	lruEnd = node;
}

void ImageCache::evictOldestImage() {
	if (lruHead == nullptr) {
		return;
	}

	int removedID = lruHead->imageID;
	// TODO: probably not thread safe, should lock when modifying images map
	Image& image = images.at(removedID);
	image.imageLoaded = false;
	glDeleteTextures(1, &image.fullTextureId);

	imageToLRUNode.erase(removedID);

	LRUNode* originalHead = lruHead;
	if (lruHead == lruEnd) {
		lruHead = nullptr;
		lruEnd = nullptr;
	} else {
		lruHead = lruHead->next;
		lruHead->prev = nullptr;
	}

	delete originalHead;
}

ImageTimestamp ImageCache::parseEXIFTimestamp(std::string text) {
	ImageTimestamp timestamp{};

	int start = 0;
	int end = text.find(':', start);
	timestamp.year = std::stoi(text.substr(start, end));
	start = end + 1;

	end = text.find(':', start);
	timestamp.month = std::stoi(text.substr(start, end));
	start = end + 1;

	end = text.find(' ', start);
	timestamp.day = std::stoi(text.substr(start, end));
	start = end + 1;

	end = text.find(':', start);
	timestamp.hour = std::stoi(text.substr(start, end));
	start = end + 1;

	end = text.find(':', start);
	timestamp.minute = std::stoi(text.substr(start, end));
	start = end + 1;

	timestamp.second = std::stoi(text.substr(start));

	return timestamp;
}
