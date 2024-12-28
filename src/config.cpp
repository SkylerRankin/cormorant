#include <filesystem>
#include <fstream>
#include <whereami.h>
#include "config.h"
#include "version.h"

namespace Config {
	namespace {
		const char* configFilename = "cormorant.config";
		char* executablePath = nullptr;
	}

	namespace fs = std::filesystem;

	std::string trim(std::string s) {
		int start = 0;
		while (start < s.size() - 1 && std::isspace(s.at(start))) {
			start++;
		}

		size_t end = s.size() - 1;
		while (end >= start && std::isspace(s.at(end))) {
			end--;
		}

		return s.substr(start, end - start + 1);
	}

	void setExecutablePath() {
		int pathLength = wai_getExecutablePath(nullptr, 0, nullptr);
		executablePath = new char[pathLength + 1];
		wai_getExecutablePath(executablePath, pathLength, nullptr);
		executablePath[pathLength] = '\0';
	}

	void loadConfig(Config& config) {
		if (executablePath == nullptr) {
			setExecutablePath();
		}

		fs::path configPath = fs::path(executablePath).parent_path().append(configFilename);
		if (fs::exists(configPath)) {
			std::ifstream stream(configPath);
			std::string line;
			while (std::getline(stream, line)) {
				size_t equalsIndex = line.find("=");
				if (equalsIndex != std::string::npos) {
					std::string key = trim(line.substr(0, equalsIndex));
					std::string value = trim(line.substr(equalsIndex + 1));
					std::optional<int> intValue = std::nullopt;
					try {
						intValue.emplace(std::stoi(value));
					} catch (...) {}

					if (key == "cacheCapacity" && intValue.has_value()) {
						config.cacheCapacity = intValue.value();
					} else if (key == "cacheBackwardPreload" && intValue.has_value()) {
						config.cacheBackwardPreload = intValue.value();
					} else if (key == "cacheForwardPreload" && intValue.has_value()) {
						config.cacheForwardPreload = intValue.value();
					}
				}
			}

			stream.close();
		}
	}

	void saveConfig(const Config& config) {
		if (executablePath == nullptr) {
			setExecutablePath();
		}

		fs::path configPath = fs::path(executablePath).parent_path().append(configFilename);
		std::ofstream stream(configPath);
		if (!stream.is_open()) return;

		stream << "version = " << VERSION << std::endl;
		stream << "cacheCapacity = " << config.cacheCapacity << std::endl;
		stream << "cacheBackwardPreload = " << config.cacheBackwardPreload << std::endl;
		stream << "cacheForwardPreload = " << config.cacheForwardPreload << std::endl;

		stream.close();
	}
}
