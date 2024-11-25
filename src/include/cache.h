#pragma once
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <glm/glm.hpp>

enum ImageStatus {
	ImageStatus_NotLoaded = 0,
	ImageStatus_Loading = 1,
	ImageStatus_Loaded = 2,
	ImageStatus_Failed = 3
};

struct Image {
	ImageStatus status = ImageStatus_NotLoaded;
	int id = -1;
	std::string filename;
	std::string path;
	glm::ivec2 size;
	glm::ivec2 previewSize;
	unsigned int filesize;
	unsigned int timestamp;
	unsigned int previewTextureId;
	unsigned int fullTextureId;
};

struct ImageQueueEntry {
	int imageID;
	bool loadFullTexture;
	bool loadPreviewTexture;
};

struct TextureQueueEntry {
	int imageID;
	unsigned char* imageData;
	unsigned char* previewData;
};

class ImageCache {
public:
	ImageCache();
	~ImageCache();

	void frameUpdate();
	const Image& getImage(int index) const;
	/*
	Initializes the cache to have an entry for every valid image file in the given directory. Full resolution
	image data is loaded for each image up until the cache is full, at which point only preview images are
	stored and the full resolution data is discarded.
	*/
	void initCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded);
	const std::map<int, Image>& getImages() const;

private:
	// Max full resolution images to keep in cache at a time
	const int cacheCapacity = 10;
	const int defaultImageLoadThreads = 1;
	const int textureQueueEntriesPerFrame = 1;
	const glm::ivec2 previewTextureSize{75, 75};
	const glm::ivec3 previewBackgroundColor{50, 50, 50};

	static int nextImageID;
	unsigned char* previewTextureBackground;
	int imageLoadThreads = defaultImageLoadThreads;
	std::atomic_bool runImageLoadThreads;
	std::map<int, Image> images;
	std::deque<ImageQueueEntry> imageQueue;
	std::deque<TextureQueueEntry> textureQueue;
	std::mutex imageQueueMutex;
	std::mutex textureQueueMutex;
	std::mutex imageQueueConditionVariableMutex;
	std::condition_variable imageQueueConditionVariable;

	void startImageLoadingThreads();
	void processTextureQueue();
	void threadInitCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded);
	void runImageLoadThread(int id);
	void loadDirectory(std::string path, std::atomic_bool& directoryLoaded);
	/*
	Loads an image file for the given image id. This file is then decoded and stored in a buffer,
	which `imageData` is updated to point to. The image is also resized to the preview size, and stored in a buffer
	which `previewImageData` is updated to point to.

	If the full resolution image does not need to be saved, pass `skipFullResolution` as true. This will only generate
	the preview image, freeing the full resolution image after the resize is complete. `imageData` will be set to
	a null pointer.

	If the preview image does not need to be saved, pass `skipPreview` as true. This will only allocate space for the
	full resolution. `previewData` will be set to a null pointer.
	*/
	bool loadImageFromFile(int id, unsigned char*& imageData, unsigned char*& previewData, bool skipFullResolution, bool skipPreview);
	void loadImageRange(int start, int end);

};