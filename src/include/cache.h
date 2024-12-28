#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <set>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "glCommon.h"

using u64 = unsigned long long;

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
	std::string shortFilename;
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
	// True if image was marked as skipped in UI
	bool skipped = false;
	// True if image was marked as saved in UI
	bool saved = false;
};

/*
An item in the image loading queue. These entries are consumed by one of the image
loading threads. The image represented by `imageID` is decoded from disk and copied
into the memory address used by the PBO `pbo`.
*/
struct ImageQueueEntry {
	int imageID;
	int directoryID;
	unsigned int pbo;
	void* pboMapping;
	// True if the image should be loaded in the preview format instead of its full resolution and aspect ratio.
	bool isPreview;
};

struct TextureQueueEntry {
	int imageID;
	int directoryID;
	unsigned int pbo;
	bool isPreview;
};

struct PBOQueueEntry {
	unsigned int pbo;
	void* mappedBuffer;
};

struct LRUNode {
	int imageID;
	LRUNode* next;
	LRUNode* prev;
};

struct ImageCacheUIData {
	glm::ivec2 previewTextureSize{};
	int cacheCapacity;
	int imageLoadingThreads;
	int previewCount;
	int fullResolutionCount;
	u64 estimatedPreviewBytes;
	u64 estimatedFullTextureBytes;

	int imageQueueSize;
	int textureQueueSize;
	int pendingImageQueueSize;
	int availablePBOQueueSize;
	int pendingPBOSize;
};

class ImageCache {
public:
	ImageCache(int capacity);
	~ImageCache();

	/*
	Removes all images stored in the cache. Images currently within a queue or on a separate thread won't be
	deleted, but will be discarded once completed.
	*/
	void clear();

	void frameUpdate();
	// Returns an image, but this image is not updated in the context of the LRU cache.
	Image* getImage(int id);
	
	void updateCapacity(int capacity);

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
	/*
	Called right after the image directory is traversed and all files and their sizes are known. Pushes entires to
	load preview textures for every image, as well as entires to load full resolution textures for the first n
	images, where n is the cache capacity.
	*/
	void startInitialTextureLoads();

	bool previewLoadingComplete() const;
	float getPreviewLoadProgress() const;

	void getUIData(ImageCacheUIData& data);

private:
	// Max full resolution images to keep stored in GPU at a time
	int cacheCapacity;
	const int defaultImageLoadThreads = 1;
	const int textureQueueEntriesPerFrame = 1;
	const glm::ivec2 previewTextureSize{75, 75};
	const glm::ivec3 previewBackgroundColor{50, 50, 50};
	const int shortFilenameLength = 16;

	int nextImageID = 0;
	int currentDirectoryID = 0;
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

	// For debugging
	std::set<GLuint> textureIds;
	u64 previewTexturesTotalBytes = 0;
	u64 fullResolutionTexturesTotalBytes = 0;
	
	// Contains ids of all images that have had their image data and preview texture loaded.
	// Used to track progress of the initial loads before full resolution textures can be
	// used normally.
	std::set<int> previewsLoaded;

	// Pending image queue: queue containing requested images on the main thread.
	// Entries are only popped from this queue once they can be paired with an available
	// PBO, and moved into the imageQueue accessible by the image loading threads.
	std::deque<ImageQueueEntry> pendingImageQueue;
	std::set<int> pendingImageQueueIds;

	// PBO queues
	std::deque<PBOQueueEntry> availablePBOQueue;
	std::map<unsigned int, GLsync> pboToFence;

	// LRU linked-list components
	LRUNode* lruHead = nullptr;
	LRUNode* lruEnd = nullptr;
	std::map<int, LRUNode*> imageToLRUNode;

	// LRU interaction functions
	void useLRUNode(LRUNode* node);
	void addLRUNode(int id);
	// Removes the full resolution data for an image and removes it from the LRU list. The
	// remaining image data is not deleted.
	void deleteImage(int id);
	void evictOldestImage();

	void startImageLoadingThreads();
	/*
	Pulls entries from the pending image queue, pairs them with an available PBO, and moves
	the entry into the image queue.
	*/
	void processPendingImageQueue();
	void processTextureQueue();
	void processPendingPBOQueue();
	void threadInitCacheFromDirectory(std::string path, std::atomic_bool& directoryLoaded);
	void runImageLoadThread(int threadID);
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
	bool loadImageFromFile(int id, bool loadPreview, unsigned char*& imageData);
	
	/*
	Parses strings in format YYYY:MM:DD HH:MM:SS into a struct.
	*/
	ImageTimestamp parseEXIFTimestamp(std::string text);

};
