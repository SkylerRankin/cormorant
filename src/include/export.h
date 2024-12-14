#pragma once
#include <atomic>
#include <filesystem>
#include <map>
#include "cache.h"

namespace Export {
	bool exportInProgress();
	float exportProgress();

	std::filesystem::path filenameOutputPath(std::filesystem::path directoryPath);
	std::filesystem::path imageOutputPath(std::filesystem::path directoryPath);

	void exportFilenames(std::filesystem::path directory, const std::map<int, Image>& images);
	/*
	Copies all saved images in the given map into a new directory located within the
	the same parent directory as the provided directory.
	If `copyAllMatchingFiles` is true, all files matching the filename, but not necessarily
	the extension, of the saved images will be copied. This is useful for copying raw files
	with a different extension along with the jpg files.
	*/
	void exportImages(std::filesystem::path directory, const std::map<int, Image>& images, bool copyAllMatchingFiles);
};
