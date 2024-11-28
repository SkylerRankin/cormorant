#pragma once
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct ImageTimestamp {
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	long long secondsSinceEpoch;
};

struct ImageMetadata {
	std::optional<std::string> cameraMake = std::nullopt;
	std::optional<std::string> cameraModel = std::nullopt;
	std::optional<unsigned short> bitsPerSample = std::nullopt;
	std::optional<double> shutterSpeed = std::nullopt;
	std::optional<double> aperture = std::nullopt;
	std::optional<unsigned short> iso = std::nullopt;
	std::optional<double> focalLength = std::nullopt;
	std::optional<glm::vec2> resolution = std::nullopt;
	std::optional<ImageTimestamp> timestamp = std::nullopt;
};

struct Image {
	int id = -1;
	std::string filename;
	std::string path;
	glm::ivec2 size;
	glm::ivec2 previewSize;
	ImageMetadata metadata;
	unsigned int filesize;
	unsigned int timestamp;
	unsigned int previewTextureId;
	unsigned int fullTextureId;
	// Flags are true if the texture is available on GPU
	bool previewLoaded = false;
	bool imageLoaded = false;
	// True if the image metadata is loaded
	bool fileInfoLoaded = false;
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

struct LRUNode {
	int imageID;
	LRUNode* next;
	LRUNode* prev;
};

class ImageCache {
public:
	ImageCache();
	~ImageCache();

	void frameUpdate();
	// Returns an image, but this image is not updated in the context of the LRU cache.
	const Image* getImage(int id);

	/*
	For each provided id, move that image to front of LRU list and add it to the image queue
	to load the full resolution texture, if its not already loaded.
	*/
	void useImageFullTexture(int id);
	void useImagesFullTextures(std::vector<int>& ids);

	/*
	Initializes the cache to have an entry for every valid image file in the given directory. Full resolution
	image data is loaded for each image up until the cache is full, at which point only preview images are
	stored and the full resolution data is discarded.
	*/
	void initCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded);
	const std::map<int, Image>& getImages() const;

private:
	// Max full resolution images to keep stored in GPU at a time
	const int cacheCapacity = 20;
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
	LRUNode* lruHead = nullptr;
	LRUNode* lruEnd = nullptr;
	std::map<int, LRUNode*> imageToLRUNode;

	// LRU interaction functions
	void useLRUNode(LRUNode* node);
	void addLRUNode(int id);
	void evictOldestImage();

	void startImageLoadingThreads();
	void processTextureQueue();
	void threadInitCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded);
	void runImageLoadThread(int id);
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
	
	/*
	Parses strings in format YYYY:MM:DD HH:MM:SS into a struct.
	*/
	ImageTimestamp parseEXIFTimestamp(std::string text);

};
