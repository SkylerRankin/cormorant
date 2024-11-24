#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Image {
	std::string filename;
	glm::ivec2 size;
	glm::ivec2 previewSize;
	unsigned int filesize;
	unsigned int timestamp;
	unsigned int previewTextureId;
	unsigned int fullTextureId;
	bool fullTextureLoaded = false;
};

class ImageCache {
public:
	ImageCache();
	~ImageCache();
	void loadDirectory(std::string path);

	const Image& getImage(int index) const;
	const std::vector<Image>& getImages() const;

private:
	const glm::ivec2 previewTextureSize{75, 75};
	const glm::ivec3 previewBackgroundColor{50, 50, 50};
	unsigned char* previewTextureBackground;

	std::vector<Image> images;
	void loadImage(std::string path, Image& image);

};