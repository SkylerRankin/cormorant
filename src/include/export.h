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
	void exportImages(std::filesystem::path directory, const std::map<int, Image>& images);
};
