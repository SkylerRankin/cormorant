#include <filesystem>
#include <fstream>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include <thread>
#include <TinyEXIF.h>
#include "cache.h"
#include "glCommon.h"

namespace fs = std::filesystem;

ImageCache::ImageCache(int capacity) : cacheCapacity(capacity) {
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

	// Create the PBOs usable for texture uploads
	// TODO: is this an appropriate number of PBOs given the number of threads?
	int pboCount = imageLoadThreads;
	for (int i = 0; i < pboCount; i++) {
		PBOQueueEntry entry{
			.pbo = 0,
			.mappedBuffer = nullptr
		};
		glGenBuffers(1, &entry.pbo);
		availablePBOQueue.push_back(entry);
	}

	runImageLoadThreads.store(true);
	startImageLoadingThreads();
}

ImageCache::~ImageCache() {
	runImageLoadThreads.store(false);
	imageQueueConditionVariable.notify_all();
	delete[] previewTextureBackground;
	clear();
}

void ImageCache::clear() {
	// Clear the image queue, returning PBOs back to the available pool.
	{
		std::lock_guard<std::mutex> guard(imageQueueMutex);
		for (const auto& entry : imageQueue) {
			PBOQueueEntry pboEntry{
				.pbo = entry.pbo,
				.mappedBuffer = nullptr
			};
			availablePBOQueue.push_back(pboEntry);
		}
		imageQueue.clear();
	}

	// Clear the texture queue, returning PBOs back to the available pool.
	{
		std::lock_guard<std::mutex> guard(textureQueueMutex);
		for (const auto& entry : textureQueue) {
			PBOQueueEntry pboEntry{
				.pbo = entry.pbo,
				.mappedBuffer = nullptr
			};
			availablePBOQueue.push_back(pboEntry);
		}
		textureQueue.clear();
	}

	for (const auto& entry : images) {
		glDeleteTextures(1, &entry.second.previewTextureId);
		glDeleteTextures(1, &entry.second.fullTextureId);
	}
	images.clear();

	pendingImageQueue.clear();
	pendingImageQueueIds.clear();
	pboToFence.clear();
	textureIds.clear();
	previewsLoaded.clear();

	// Increment current directory id so that any previous queue entries
	// will be ignore by the current cache state.
	currentDirectoryID++;

	previewTexturesTotalBytes = 0;
	fullResolutionTexturesTotalBytes = 0;

	LRUNode* current = lruHead;
	while (current) {
		LRUNode* next = current->next;
		delete current;
		current = next;
	}
	lruHead = nullptr;
	lruEnd = nullptr;
	imageToLRUNode.clear();
}

void ImageCache::frameUpdate() {
	processPendingPBOQueue();
	processPendingImageQueue();
	processTextureQueue();
	assert(textureIds.size() <= cacheCapacity && "Number of in-use textures has exceeded cache capacity.");
}

void ImageCache::updateCapacity(int capacity) {
	cacheCapacity = capacity;
}

void ImageCache::initCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded) {
	currentDirectoryID++;
	previewsLoaded.clear();
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

bool ImageCache::previewLoadingComplete() const {
	return previewsLoaded.size() == images.size();
}

float ImageCache::getPreviewLoadProgress() const {
	return previewsLoaded.size() / (float)images.size();
}

void ImageCache::getUIData(ImageCacheUIData& data) {
	// Textures
	data.cacheCapacity = cacheCapacity;
	data.previewTextureSize = previewTextureSize;
	data.previewCount = static_cast<int>(previewsLoaded.size());
	data.fullResolutionCount = static_cast<int>(textureIds.size());
	data.estimatedPreviewBytes = previewTexturesTotalBytes;
	data.estimatedFullTextureBytes = fullResolutionTexturesTotalBytes;

	// Queues
	data.imageLoadingThreads = imageLoadThreads;
	data.imageQueueSize = static_cast<int>(imageQueue.size());
	data.availablePBOQueueSize = static_cast<int>(availablePBOQueue.size());
	data.pendingImageQueueSize = static_cast<int>(pendingImageQueue.size());
	data.textureQueueSize = static_cast<int>(textureQueue.size());
	data.pendingPBOSize = static_cast<int>(pboToFence.size());
}

void ImageCache::startInitialTextureLoads() {
	// Add all images for preview texture processing
	for (auto const& e : images) {
		ImageQueueEntry entry{
			.imageID = e.first,
			.directoryID = currentDirectoryID,
			.pbo = 0,
			.pboMapping = nullptr,
			.isPreview = true
		};
		pendingImageQueue.push_back(entry);
	}

	// Add enough images for full texture processing to fill cache. The first
	// n images are arbitrarily chosen.
	std::vector<int> fullTextureIds;
	for (const auto& e : images) {
		fullTextureIds.push_back(e.first);
		if (fullTextureIds.size() == cacheCapacity) break;
	}
	useImagesFullTextures(fullTextureIds);
}

void ImageCache::threadInitCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded) {
	for (const auto& entry : fs::directory_iterator{ fs::path(path) }) {
		std::string entryPathString{ reinterpret_cast<const char*>(entry.path().u8string().c_str()) };
		fs::path entryPath{ entryPathString };

		if (fs::is_regular_file(entryPath) && entryPath.extension().compare(".JPG") == 0) {
			Image image;
			image.id = nextImageID++;
			image.path = entryPathString;
			image.filename = entryPath.filename().string();
			image.filesize = static_cast<unsigned int>(fs::file_size(entryPath));
			image.previewSize = previewTextureSize;

			if (image.filename.size() <= shortFilenameLength) {
				image.shortFilename = image.filename;
			} else {
				image.shortFilename = "..." + image.filename.substr(image.filename.size() - shortFilenameLength + 3);
			}

			int width, height, components;
			if (stbi_info(image.path.c_str(), &width, &height, &components)) {
				image.size = glm::ivec2(width, height);
			} else {
				// TODO: mark this image with some error flag, cannot be rendered later
				image.size = glm::ivec2(0, 0);
				std::cout << "Failed to get info from stbi_info for file " << image.path << std::endl;
			}

			images.emplace(image.id, image);
		}
	}
	directoryLoaded.store(true);
}

void ImageCache::runImageLoadThread(int threadID) {
	while (runImageLoadThreads.load()) {
		// Wait for condition variable signal, signaled when main threads adds entries to queue
		std::unique_lock<std::mutex> lock(imageQueueMutex);
		imageQueueConditionVariable.wait(lock);
		lock.unlock();

		while (true) {
			ImageQueueEntry imageEntry;
			{
				std::lock_guard<std::mutex> imageQueueGuard(imageQueueMutex);
				if (imageQueue.size() == 0) {
					// No more entries in image queue, break and go back to waiting on condition.
					break;
				} else {
					imageEntry = imageQueue.front();
					imageQueue.pop_front();
				}
			}

			const Image* image = &images.at(imageEntry.imageID);
			unsigned char* imageData;
			if (loadImageFromFile(imageEntry.imageID, imageEntry.isPreview, imageData)) {
				// Copy the image data from memory into the PBO mapped memory location
				unsigned int imageDataSize = imageEntry.isPreview ?
					image->previewSize.x * image->previewSize.y * 3 :
					image->size.x * image->size.y * 3;
				// TODO: for full resolution images, load directly into the PBO memory to avoid this second copy
				memcpy(imageEntry.pboMapping, imageData, imageDataSize);
				if (imageEntry.isPreview) {
					delete[] imageData;
				} else {
					stbi_image_free(imageData);
				}

				// Push a texture queue entry back to the main thread for uploading
				TextureQueueEntry textureQueueEntry{
					.imageID = imageEntry.imageID,
					.directoryID = imageEntry.directoryID,
					.pbo = imageEntry.pbo,
					.isPreview = imageEntry.isPreview
				};
				{
					std::lock_guard<std::mutex> guard(textureQueueMutex);
					textureQueue.push_back(textureQueueEntry);
				}
			} else {
				// TODO: is this thread safe?
				images.at(imageEntry.imageID).imageLoaded = false;
				images.at(imageEntry.imageID).previewLoaded = false;
			}
		}
	}
}

bool ImageCache::loadImageFromFile(int id, bool loadPreview, unsigned char*& imageData) {
	Image* image = &images.at(id);
	stbi_set_flip_vertically_on_load(true);
	int width = 0, height = 0, channels = 0;
	imageData = stbi_load(image->path.c_str(), &width, &height, &channels, 3);
	
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

	if (loadPreview) {
		// Determine the preview image size that matches the original image aspect ratio, but maximally fits
		// within the preview image size, which may have a different aspect ratio.
		const float aspectRatio = width / (float)height;
		const float previewAspectRatio = previewTextureSize.x / (float)previewTextureSize.y;
		glm::ivec2 resizeSize = previewTextureSize;
		if (aspectRatio > previewAspectRatio) {
			resizeSize.y = static_cast<int>(floor(resizeSize.y * (previewAspectRatio / aspectRatio)));
		} else if (aspectRatio < previewAspectRatio) {
			resizeSize.x = static_cast<int>(floor(resizeSize.x * (aspectRatio / previewAspectRatio)));
		}

		unsigned char* previewData;

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

		stbi_image_free(imageData);
		imageData = previewData;
	}

	return true;
}

void ImageCache::processPendingImageQueue() {
	int maxIterations = 5;
	int iterations = 0;
	bool pushedToQueue = false;
	while (iterations++ < maxIterations) {
		if (pendingImageQueue.size() == 0 || availablePBOQueue.size() == 0) break;

		ImageQueueEntry imageEntry = pendingImageQueue.front();
		pendingImageQueue.pop_front();
		pendingImageQueueIds.erase(imageEntry.imageID);
		if (
			(imageEntry.isPreview && images.at(imageEntry.imageID).previewLoaded) ||
			(!imageEntry.isPreview && images.at(imageEntry.imageID).imageLoaded)) {
			continue;
		}

		PBOQueueEntry pboEntry = availablePBOQueue.front();
		availablePBOQueue.pop_front();
		imageEntry.pbo = pboEntry.pbo;

		const Image& image = images.at(imageEntry.imageID);
		const unsigned int pboBytes = imageEntry.isPreview ?
			previewTextureSize.x * previewTextureSize.y * 3 :
			image.size.x * image.size.y * 3;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, imageEntry.pbo);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, pboBytes, 0, GL_STATIC_DRAW);
		imageEntry.pboMapping = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		{
			std::lock_guard<std::mutex> guard(imageQueueMutex);
			imageQueue.push_back(imageEntry);
			pushedToQueue = true;
		}
	}

	if (pushedToQueue) {
		imageQueueConditionVariable.notify_all();
	}
}

void ImageCache::processTextureQueue() {
	int iterations = 0;
	while (iterations < textureQueueEntriesPerFrame) {
		TextureQueueEntry entry;
		{
			std::lock_guard<std::mutex> guard(textureQueueMutex);
			if (textureQueue.size() == 0) return;
			entry = textureQueue.front();
			textureQueue.pop_front();
		}

		bool skipEntry = false;
		
		if (entry.directoryID != currentDirectoryID) {
			skipEntry = true;
		}

		Image& image = images.at(entry.imageID);
		if (!entry.isPreview && (!imageToLRUNode.contains(image.id) || image.imageLoaded)) {
			if (!imageToLRUNode.contains(image.id)) {
				image.imageLoaded = false;
			}

			skipEntry = true;
		}

		/*
		There are a few conditions in which the texture entry should be ignored.
		- If image id is no longer in the LRU list, it must have been evicted in the time that
		  the image data was being read/decoded (does not apply to previews).
		- If the image is already loaded, there is no need to do an additional upload.
		- If the directory ID doesn't match, the entry is leftover from a previous directory.
		*/
		if (skipEntry) {
			// Unbind the PBO
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, entry.pbo);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			// Make this PBO available immediately
			PBOQueueEntry pboEntry{
				.pbo = entry.pbo,
				.mappedBuffer = nullptr
			};
			availablePBOQueue.push_back(pboEntry);
			continue;
		}

		iterations++;

		GLuint* textureID = entry.isPreview ? &image.previewTextureId : &image.fullTextureId;
		glm::ivec2& textureSize = entry.isPreview ? image.previewSize : image.size;

		glGenTextures(1, textureID);
		glBindTexture(GL_TEXTURE_2D, *textureID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, entry.pbo);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize.x, textureSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		pboToFence.emplace(entry.pbo, glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		u64 totalBytes = static_cast<u64>(textureSize.x) * static_cast<u64>(textureSize.y) * 3ULL;

		if (entry.isPreview) {
			image.previewLoaded = true;
			if (previewsLoaded.size() < images.size()) {
				previewsLoaded.insert(image.id);
			}
			previewTexturesTotalBytes += totalBytes;
		} else {
			image.imageLoaded = true;
			textureIds.insert(*textureID);
			fullResolutionTexturesTotalBytes += totalBytes;
		}
	}
}

void ImageCache::processPendingPBOQueue() {
	// Use sync and fences to move completed pbo transfers back to the available pool
	for (auto i = pboToFence.cbegin(); i != pboToFence.cend();) {
		if (glClientWaitSync(i->second, 0, 0)) {
			// The sync fence is signaled, so PBO is available for reuse
			PBOQueueEntry entry{
				.pbo = i->first,
				.mappedBuffer = nullptr
			};

			// Free the memory used by the PBO
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, entry.pbo);
			glBufferData(GL_PIXEL_UNPACK_BUFFER, 0, 0, GL_STATIC_DRAW);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			availablePBOQueue.push_back(entry);
			pboToFence.erase(i++);
		} else {
			i++;
		}
	}
}

Image* ImageCache::getImage(int id) {
	return &images.at(id);
}

void ImageCache::useImageFullTexture(int id) {
	if (id == -1) return;

	std::vector<int> ids;
	ids.push_back(id);
	useImagesFullTextures(ids);
}

void ImageCache::useImagesFullTextures(std::vector<int>& allIds) {
	// Reduce the number of image ids to add to a set that is <= the cache capacity. Any more
	// will result in the last images in the list evicting the first images in the list.
	std::set<int> ids;
	for (int i = 0; i < allIds.size() && ids.size() < cacheCapacity; i++) {
		ids.insert(allIds.at(i));
	}

	for (int id : ids) {
		// Skip pushing IDs that are already in the queue.
		if (pendingImageQueueIds.contains(id)) {
			continue;
		}

		ImageQueueEntry entry{
			.imageID = id,
			.directoryID = currentDirectoryID,
			.pbo = 0,
			.pboMapping = nullptr,
			.isPreview = false
		};
		pendingImageQueue.push_back(entry);
		pendingImageQueueIds.insert(entry.imageID);
	}

	// If the pending image queue is larger than the cache capacity, images early in the queue will
	// always be evicted as images later in the queue eventually get processed. To prevent that wasted
	// time, enough images early in the queue are deleted such that all images in the queue will actually
	// be kept in the cache.
	if (previewLoadingComplete() && pendingImageQueue.size() > cacheCapacity) {
		int entriesToDelete = static_cast<int>(pendingImageQueue.size()) - cacheCapacity;
		int entriesDeleted = 0;

		int i = 0;
		while (i < pendingImageQueue.size() && entriesDeleted < entriesToDelete) {
			ImageQueueEntry entry = pendingImageQueue.at(i);
			if (ids.contains(entry.imageID)) {
				// Don't delete images that are being requested by this function call. They may already
				// exist at early positions in the queue.
				i++;
			} else {
				pendingImageQueue.erase(pendingImageQueue.cbegin() + i);
				pendingImageQueueIds.erase(entry.imageID);
				deleteImage(entry.imageID);
				entriesDeleted++;
			}
		}
	}

	// First, find any ids that are already loaded and in the LRU list. Mark these
	// images as used so they move up in the list and won't be considered for eviction
	// unnecessarily.
	for (int id : ids) {
		if (imageToLRUNode.contains(id)) {
			useLRUNode(imageToLRUNode.at(id));
		}
	}

	// Second, add any new ids to the list, evicting the oldest when space is needed.
	for (int id : ids) {
		if (!imageToLRUNode.contains(id)) {
			if (imageToLRUNode.size() == cacheCapacity) {
				evictOldestImage();
			}
			addLRUNode(id);
		}
	}
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
	deleteImage(removedID);
}

void ImageCache::deleteImage(int id) {
	// TODO: probably not thread safe, should lock when modifying images map
	Image& image = images.at(id);

	if (image.imageLoaded) {
		image.imageLoaded = false;
		glDeleteTextures(1, &image.fullTextureId);
		textureIds.erase(image.fullTextureId);
		fullResolutionTexturesTotalBytes -= static_cast<u64>(image.size.x) * static_cast<u64>(image.size.y) * 3ULL;
	}

	imageToLRUNode.erase(id);

	assert(lruHead && "Attempted to delete from empty LRU list.");

	if (lruHead->imageID == id) {
		// Deleting head of list
		LRUNode* originalHead = lruHead;
		if (lruHead == lruEnd) {
			lruHead = nullptr;
			lruEnd = nullptr;
		} else {
			lruHead = lruHead->next;
			lruHead->prev = nullptr;
		}
		delete originalHead;
	} else if (lruEnd->imageID == id) {
		// Deleting end of list
		LRUNode* node = lruEnd;
		lruEnd = lruEnd->prev;
		lruEnd->next = nullptr;
		delete node;
	} else {
		LRUNode* current = lruHead;
		while (current) {
			if (current->imageID == id) break;
			current = current->next;
		}

		assert(current && current->imageID == id && "Attempted to delete image, but not found in LRU list.");
		current->prev->next = current->next;
		current->next->prev = current->prev;
		delete current;
	}
}

ImageTimestamp ImageCache::parseEXIFTimestamp(std::string text) {
	ImageTimestamp timestamp{};

	size_t start = 0;
	size_t end = text.find(':', start);
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

	tm timeComponents{};
	timeComponents.tm_year = timestamp.year - 1900;
	timeComponents.tm_mon = timestamp.month - 1;
	timeComponents.tm_mday = timestamp.day;
	timeComponents.tm_hour = timestamp.hour;
	timeComponents.tm_min = timestamp.minute;
	timeComponents.tm_sec = timestamp.second;
	timeComponents.tm_isdst = -1;
	timestamp.secondsSinceEpoch = mktime(&timeComponents);

	return timestamp;
}
