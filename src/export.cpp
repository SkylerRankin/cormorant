#include <fstream>
#include "application.h"
#include "export.h"

namespace fs = std::filesystem;

namespace Export {
    namespace {
        const std::string baseFilename = "saved_images";
        const std::string extension = ".txt";
        const std::string savedDirectoryPostfix = " (saved)";

        std::atomic_bool inProgress(false);
        float progress = 0.0f;
    }

    // Forward declarations
    void threadExportFilenames(fs::path directoryPath, const std::map<int, Image>& images);
    void threadExportImages(fs::path directoryPath, const std::map<int, Image>& images);

    bool exportInProgress() { return inProgress.load(); }
    float exportProgress() { return progress; }

    fs::path filenameOutputPath(fs::path directoryPath) {
        // TODO: not ideal to do this file system interaction every frame while the save filenames UI is showing.
        fs::path outputPath;

        // Files with same name may already exist, so find the largest number currently used
        if (fs::exists(fs::path(directoryPath).append(baseFilename + extension))) {
            int largest = 0;
            std::string prefix = std::format("{} (", baseFilename);
            std::string postfix = std::format("){}", extension);

            for (const auto& item : fs::directory_iterator{ directoryPath }) {
                if (!item.is_regular_file()) continue;
                std::string filename = item.path().filename().string();
                if (filename.starts_with(prefix) && filename.ends_with(postfix)) {
                    try {
                        int value = std::stoi(filename.substr(prefix.length(), filename.length() - prefix.length() - postfix.length()));
                        if (value > largest) largest = value;
                    } catch (...) {}
                }
            }
            outputPath = fs::path(directoryPath).append(baseFilename + std::format(" ({})", largest + 1) + extension);
        } else {
            outputPath = fs::path(directoryPath).append(baseFilename + extension);
        }

        return outputPath;
    }

    fs::path imageOutputPath(fs::path directoryPath) {
        return fs::path(directoryPath).parent_path().append(directoryPath.filename().string() + savedDirectoryPostfix);
    }

    void exportFilenames(fs::path directory, const std::map<int, Image>& images) {
        inProgress.store(true);
        progress = 0.0f;
        std::thread thread(threadExportFilenames, directory, images);
        thread.detach();
    }

    void exportImages(fs::path directory, const std::map<int, Image>& images) {
        inProgress.store(true);
        progress = 0.0f;
        std::thread thread(threadExportImages, directory, images);
        thread.detach();
    }

    void threadExportFilenames(fs::path directoryPath, const std::map<int, Image>& images) {
        fs::path outputPath = filenameOutputPath(directoryPath);
        std::ofstream outputFile;
        outputFile.open(outputPath);

        outputFile << "-- Cormorant --" << std::endl;
        outputFile << "Saved images for directory \"" << directoryPath.string() << "\"" << std::endl;

        auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        outputFile << "This file was generated on " << std::ctime(&time) << std::endl;

        float increment = 1.0f / images.size();
        for (auto& e : images) {
            if (e.second.saved) {
                outputFile << e.second.filename << std::endl;
            }
            progress += increment;
        }

        outputFile.close();
        inProgress.store(false);
        progress = 1.0f;
    }

    void threadExportImages(fs::path directoryPath, const std::map<int, Image>& images) {
        fs::path outputPath = imageOutputPath(directoryPath);

        if (!fs::exists(outputPath)) {
            fs::create_directory(outputPath);
        }

        int totalSaved = 0;
        for (auto& e : images) {
            if (e.second.saved) totalSaved++;
        }

        float increment = 1.0f / totalSaved;
        for (auto& e : images) {
            if (e.second.saved) {
                fs::copy(e.second.path, fs::path(outputPath).append(e.second.filename), fs::copy_options::skip_existing);
            }
            progress += increment;
        }

        inProgress.store(false);
        progress = 1.0f;
    }
}